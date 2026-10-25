// Simulation speed benchmark: fixed scenes built from Lua, frame cap removed, ticks counted over a
// wall-clock window. Slow and machine-dependent, so it only runs with PERF=1; results go to
// perf.json next to the other artifacts. Use it to compare builds on the same machine.
import { test } from '@playwright/test';
import * as h from './helpers.mjs';

test.skip(!process.env.PERF, 'benchmark only runs with PERF=1');

const results = {};
test.afterAll(() => h.writeArtifact(process.env.PERF_OUT || 'perf.json', results));

// Each scene clears the simulation and fills it; the chunk returns the particle count.
const SCENES = {
	// bands of falling powder, like the "busy" scene of tpt.spec.mjs (~28k particles)
	powderBands: `
		sim.clearSim()
		for y = 30, 190, 8 do for x = 20, 590 do for d = 0, 1 do sim.partCreate(-1, x, y + d, elem.DEFAULT_PT_DUST) end end end`,
	// a deep pool of water under a layer of sand, with fire on top (~100k particles)
	liquidsAndFire: `
		sim.clearSim()
		for y = 250, 379 do for x = 10, 600 do sim.partCreate(-1, x, y, elem.DEFAULT_PT_WATR) end end
		for y = 150, 189 do for x = 10, 600 do sim.partCreate(-1, x, y, elem.DEFAULT_PT_SAND) end end
		for x = 10, 600, 3 do sim.partCreate(-1, x, 140, elem.DEFAULT_PT_FIRE) end`,
	// the powder bands again with Newtonian gravity on and black holes (section 5 of docs/NEXT-PLANS.md: what a
	// gravity thread could save); the gravity field is computed on the main thread in this build
	powderBandsGravity: `
		sim.clearSim()
		tpt.newtonian_gravity(1)
		for y = 30, 190, 8 do for x = 20, 590 do for d = 0, 1 do sim.partCreate(-1, x, y + d, elem.DEFAULT_PT_DUST) end end end
		-- only some elements make gravity; black holes that keep eating change the field every frame
		for x = 40, 580, 60 do for y = 240, 340, 50 do sim.partCreate(-1, x, y, elem.DEFAULT_PT_NBHL) end end`,
	// powered electronics: metal wires fed by batteries, with switches and copper (~25k particles)
	electronics: `
		sim.clearSim()
		for row = 0, 19 do
			local y = 20 + row * 18
			for x = 20, 590 do sim.partCreate(-1, x, y, elem.DEFAULT_PT_METL) end
			for x = 20, 590 do sim.partCreate(-1, x, y + 1, (x % 2 == 0) and elem.DEFAULT_PT_PSCN or elem.DEFAULT_PT_NSCN) end
			for x = 20, 590, 40 do sim.partCreate(-1, x, y - 1, elem.DEFAULT_PT_BTRY) end
			for x = 20, 590 do sim.partCreate(-1, x, y + 2, elem.DEFAULT_PT_COPR or elem.DEFAULT_PT_METL) end
		end`,
};

// Milliseconds per frame from the game's own benchmark export (wasm, no rendering or browser
// scheduling in the measurement): the median of several rounds, so one slow round doesn't count.
async function frameCost(page, what, rounds, frames) {
	const times = [];
	for (let r = 0; r < rounds; r++) {
		times.push(await page.evaluate(([f, w]) => window.tptPage.module.ccall('tpt_benchmark', 'number', ['number', 'number'], [f, w]), [frames, what]) / frames);
	}
	times.sort((a, b) => a - b);
	return Math.round(times[Math.floor(times.length / 2)] * 1000) / 1000;
}

const round3 = (v) => Math.round(v * 1000) / 1000;

test('cost of one simulation frame and one rendered frame', async ({ page }) => {
	test.setTimeout(600_000);
	await h.openGame(page);
	await h.lua(page, 'tpt.set_pause(1) or 0'); // scenes start from the same state; the benchmark steps them itself
	const out = {};
	for (const [name, code] of Object.entries(SCENES)) {
		await h.luaFile(page, 'tpt.newtonian_gravity(0)\n' + code + '\nreturn 0', 120_000);
		await page.evaluate(() => window.tptPage.module.ccall('tpt_benchmark', 'number', ['number', 'number'], [20, 0])); // warm-up
		const n = Number(await h.luaFile(page, 'local n = 0 for i in sim.parts() do n = n + 1 end return n'));
		const simMs = await frameCost(page, 0, 7, 30);
		const renderMs = await frameCost(page, 1, 7, 20);
		out[name] = { particles: n, simMsPerFrame: simMs, renderMsPerFrame: renderMs, totalMsPerFrame: round3(simMs + renderMs) };
	}
	results.frameCost = out;
	console.log('frame cost', JSON.stringify(out));
});

// Rendering cost of each display mode (tpt.display_mode) in the liquids scene; "fancy" blurs every liquid
// particle, which made it the slowest by far (section 6 of docs/NEXT-PLANS.md).
test('cost of one rendered frame in each display mode', async ({ page }) => {
	test.setTimeout(600_000);
	await h.openGame(page);
	await h.lua(page, 'tpt.set_pause(1) or 0');
	await h.luaFile(page, 'tpt.newtonian_gravity(0)\n' + SCENES.liquidsAndFire + '\nreturn 0', 120_000);
	const names = ['velocity', 'pressure', 'persistent', 'fire', 'blob', 'heat', 'fancy', 'nothing', 'heatGradient', 'lifeGradient', 'altVelocity'];
	const out = {};
	for (let mode = 0; mode < names.length; mode++) {
		await h.luaFile(page, `tpt.display_mode(${mode}) return 0`);
		await page.evaluate(() => window.tptPage.module.ccall('tpt_benchmark', 'number', ['number', 'number'], [20, 1])); // warm-up
		out[names[mode]] = await frameCost(page, 1, 5, 30);
	}
	await h.luaFile(page, 'tpt.display_mode(3) return 0');
	results.renderMsPerFrameByDisplayMode = out;
	console.log('render cost by display mode', JSON.stringify(out));
});

async function ticksPerSecond(page, ms) {
	const a = Number(await h.lua(page, 'TK'));
	const ta = Date.now();
	await page.waitForTimeout(ms);
	const b = Number(await h.lua(page, 'TK'));
	return (b - a) / ((Date.now() - ta) / 1000);
}

// Whole-game tick rate with the frame cap removed, optionally with CDP CPU throttling to mimic a slower
// device (PERF_THROTTLES=1,4). Much noisier than the frame cost above.
for (const throttle of (process.env.PERF_THROTTLES || '').split(',').filter(Boolean).map(Number)) {
	test(`simulation speed, CPU throttle ${throttle}x`, async ({ page }) => {
		test.setTimeout(600_000);
		await h.openGame(page);
		await h.lua(page, 'event.register(event.tick,function() TK=(TK or 0)+1 end) or 0');
		await h.lua(page, 'tpt.setfpscap(2) or 0'); // 2 = no frame cap
		if (throttle > 1) {
			const cdp = await page.context().newCDPSession(page);
			await cdp.send('Emulation.setCPUThrottlingRate', { rate: throttle });
		}
		const out = { idle: Math.round(await ticksPerSecond(page, 4000) * 10) / 10 };
		for (const [name, code] of Object.entries(SCENES)) {
			await h.luaFile(page, 'tpt.newtonian_gravity(0)\n' + code + '\nreturn 0', 120_000);
			await h.waitFrames(page, 30); // let the scene settle a little
			// sim.NUM_PARTS is only refreshed at the start of a frame, so count the particles directly
			const n = Number(await h.luaFile(page, 'local n = 0 for i in sim.parts() do n = n + 1 end return n'));
			out[name] = { particles: n, ticksPerSecond: Math.round(await ticksPerSecond(page, 10_000) * 10) / 10 };
		}
		results[`throttle${throttle}x`] = out;
		console.log(`throttle ${throttle}x`, JSON.stringify(out));
	});
}
