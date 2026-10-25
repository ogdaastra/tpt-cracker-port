// Fixes ported from newer official TPT (section 4 of docs/NEXT-PLANS.md): each test builds the situation that
// used to hang, crash, corrupt the particle map or drift, and checks that the game now handles it.
import { test, expect } from '@playwright/test';
import * as h from './helpers.mjs';

const results = {};
test.afterAll(() => h.writeArtifact('upstream-fixes.json', results));

// Steps the simulation without drawing (tpt_benchmark runs whole frames, paused or not).
async function step(page, frames) {
	return page.evaluate((n) => window.tptPage.module.ccall('tpt_benchmark', 'number', ['number', 'number'], [n, 0]), frames);
}

// Every pmap/photons entry in the area points at a live particle at that spot (a stale entry is the corruption).
const PMAP_CHECK = `
	local bad = 0
	for y = 80, 320 do for x = 80, 420 do
		for _, id in ipairs({ sim.partID(x, y) or -1, sim.photons(x, y) or -1 }) do
			if id >= 0 then
				local px, py = sim.partPosition(id)
				if sim.partProperty(id, "type") == 0 or math.floor(px + 0.5) ~= x or math.floor(py + 0.5) ~= y then bad = bad + 1 end
			end
		end
	end end
	return bad`;

test('NaN velocity and sub-zero MERC no longer hang or crash the game', async ({ page }) => {
	const log = h.collect(page);
	await h.openGame(page);
	// upstream 51508113: a NaN velocity made the movement loop run forever
	await h.luaFile(page, `
		sim.clearSim()
		local i = sim.partCreate(-1, 200, 200, elem.DEFAULT_PT_DUST)
		sim.partProperty(i, "vx", 0/0)
		sim.partProperty(i, "vy", 0/0)
		return i`);
	const ms = await step(page, 60);
	results.nanFrames60ms = ms;
	expect(ms, '60 frames with a NaN velocity').toBeLessThan(5000);
	// upstream d6f66f5c: MERC at -2 K < T <= -1 K divided by zero (int(T) + 1 == 0; a WebAssembly trap)
	await h.luaFile(page, `
		sim.clearSim()
		sim.createWallBox(80, 204, 160, 211, 8) -- a floor
		for x = 100, 140 do sim.partCreate(-1, x, 200, elem.DEFAULT_PT_MERC) end
		for i in sim.parts() do sim.partProperty(i, "temp", -1.5) end
		return 0`);
	await step(page, 120);
	expect(Number(await h.luaFile(page, 'local n = 0 for i in sim.parts() do n = n + 1 end return n'))).toBeGreaterThan(0);
	expect(await page.evaluate(() => window.tptPage.state)).toBe('running');
	await h.expectNoUnexplainedErrors(log);
});

test('a neutron entering a portal through DMND leaves the particle maps consistent', async ({ page }) => {
	const log = h.collect(page);
	await h.openGame(page);
	// the reproduction from upstream 82ec6b27: the portal only cleared .type, leaving a stale photons entry
	await h.luaFile(page, `
		sim.clearSim()
		for k = 0, 30 do
			local i = sim.partCreate(-1, 100 + k * 10, 100, elem.DEFAULT_PT_NEUT)
			sim.partProperty(i, "vx", 0)
			sim.partProperty(i, "vy", 2)
			sim.partCreate(-1, 100 + k * 10, 100, elem.DEFAULT_PT_DMND)
			sim.partCreate(-1, 100 + k * 10, 102, elem.DEFAULT_PT_PRTI)
		end
		sim.partCreate(-1, 300, 300, elem.DEFAULT_PT_PRTO)
		return 0`);
	for (let k = 0; k < 10; k++) {
		await step(page, 6);
		const bad = Number(await h.luaFile(page, PMAP_CHECK));
		expect(bad, `inconsistent particles after ${(k + 1) * 6} frames`).toBe(0);
	}
	await h.expectNoUnexplainedErrors(log);
});

test('gas spreads evenly instead of drifting down and right', async ({ page }) => {
	const log = h.collect(page);
	await h.openGame(page);
	// upstream d21a2293: a 0.25 px bias in the gas movement pushed every cloud towards the lower right
	const r = JSON.parse(await h.luaFile(page, `
		sim.clearSim()
		tpt.set_pause(1)
		-- a closed wall box centred on the cloud, so nothing leaves and the walls pull no side
		for x = 196, 415, 4 do sim.createWallBox(x, 96, x, 99, 8) sim.createWallBox(x, 284, x, 287, 8) end
		for y = 96, 287, 4 do sim.createWallBox(196, y, 199, y, 8) sim.createWallBox(412, y, 415, y, 8) end
		for x = 276, 335 do for y = 162, 221 do sim.partCreate(-1, x, y, elem.DEFAULT_PT_GAS) end end
		local function centre()
			local sx, sy, n = 0, 0, 0
			for i in sim.parts() do local x, y = sim.partPosition(i) sx, sy, n = sx + x, sy + y, n + 1 end
			return sx / n, sy / n, n
		end
		local x0, y0 = centre()
		_G.gasStart = { x0, y0 }
		return string.format('{"x0":%.2f,"y0":%.2f}', x0, y0)`));
	await step(page, 600);
	const end = JSON.parse(await h.luaFile(page, `
		local sx, sy, n = 0, 0, 0
		for i in sim.parts() do local x, y = sim.partPosition(i) sx, sy, n = sx + x, sy + y, n + 1 end
		return string.format('{"x":%.2f,"y":%.2f,"n":%d}', sx / n, sy / n, n)`));
	results.gas = { start: r, end };
	expect(end.n).toBeGreaterThan(3000);
	expect(Math.abs(end.x - r.x0), 'horizontal drift of the gas cloud (px)').toBeLessThan(3);
	expect(Math.abs(end.y - r.y0), 'vertical drift of the gas cloud (px)').toBeLessThan(3);
	await h.expectNoUnexplainedErrors(log);
});
