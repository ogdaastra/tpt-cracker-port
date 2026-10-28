// Turbo mode (simulation/Turbo.h): the particle update on several threads. The result must not depend on how
// many threads there are or on which thread does what, nothing may be lost or doubled, special particles are
// never changed in parallel, and a big scene runs faster. Plus the page's Turbo button.
import { test, expect } from '@playwright/test';
import { createHash } from 'node:crypto';
import * as h from './helpers.mjs';

const results = { scenes: {} };
test.afterAll(() => h.writeArtifact('turbo.json', results));

const call = (page, name, ret, types, args) =>
	page.evaluate(([n, r, t, a]) => window.tptPage.module.ccall(n, r, t, a), [name, ret, types, args]);
const turboInfo = async (page) => {
	const v = (await call(page, 'tpt_turbo_info', 'string', [], [])).split(' ').map(Number);
	return { frames: v[0], tiles: v[1], parallelTiles: v[2], parallel: v[3], serial: v[4], deferred: v[5], specialTouched: v[6] };
};

// Runs Lua in the game loop and waits until it has run
let runs = 0;
async function run(page, code) {
	const done = `/tmp/turbo-run${++runs}.done`;
	await call(page, 'tpt_lua_command', null, ['string'], [`${code.replace(/\s*\n\s*/g, ' ')} local f = io.open("${done}", "w") f:write("1") f:close()`]);
	for (let k = 0; k < 6000; k++) {
		if (await page.evaluate((p) => { try { window.tptPage.module.FS.readFile(p); return true; } catch (e) { return false; } }, done)) return;
		await page.waitForTimeout(10);
	}
	throw new Error('Lua did not run: ' + code.slice(0, 80));
}

async function stateHash(page) {
	const path = `/tmp/turbo-state${++runs}.txt`;
	await run(page, h.particleStateLua(path));
	const text = await page.evaluate((p) => new TextDecoder().decode(window.tptPage.module.FS.readFile(p)), path);
	return { hash: createHash('sha256').update(text).digest('hex').slice(0, 16), particles: text.split('\n').filter((l) => !l.startsWith('air')).length };
}

const box = 'sim.createWallBox(0, 0, 611, 3, 8) sim.createWallBox(0, 380, 611, 383, 8) sim.createWallBox(0, 0, 3, 383, 8) sim.createWallBox(608, 0, 611, 383, 8)';
const fill = (x0, y0, x1, y1, pick) => `for x = ${x0}, ${x1} do for y = ${y0}, ${y1} do sim.partCreate(-1, x, y, ${pick}) end end`;
const SCENES = {
	// liquids and powders settling in a closed box
	pond: `${box} ${fill(10, 260, 600, 370, '(x + y) % 5 == 0 and elem.DEFAULT_PT_SAND or ((x % 9 == 0) and elem.DEFAULT_PT_SALT or elem.DEFAULT_PT_WATR)')}
		${fill(10, 220, 600, 240, 'elem.DEFAULT_PT_OIL')}`,
	// burning: wood, coal, plants, gas and oil with fire, lava and plasma; ice and snow melting; CO2
	fire: `${box} ${fill(20, 300, 590, 370, '(x % 3 == 0) and elem.DEFAULT_PT_WOOD or ((x % 3 == 1) and elem.DEFAULT_PT_COAL or elem.DEFAULT_PT_PLNT)')}
		${fill(20, 150, 590, 200, '(x % 2 == 0) and elem.DEFAULT_PT_GAS or elem.DEFAULT_PT_OIL')}
		${fill(100, 210, 200, 240, 'elem.DEFAULT_PT_ICEI')} ${fill(300, 210, 400, 240, 'elem.DEFAULT_PT_SNOW')}
		${fill(450, 210, 550, 240, 'elem.DEFAULT_PT_CO2')}
		${fill(20, 295, 590, 297, 'elem.DEFAULT_PT_FIRE')} ${fill(250, 100, 280, 130, 'elem.DEFAULT_PT_PLSM')}
		${fill(500, 60, 560, 90, 'elem.DEFAULT_PT_LAVA')}`,
	// parallel and one-thread particles side by side: SOAP, sparks in metal, a detector wall, a faster time field,
	// photons, a stick figure
	mixed: `${box} ${fill(10, 280, 600, 370, '(x % 4 == 0) and elem.DEFAULT_PT_DUST or elem.DEFAULT_PT_WATR')}
		${fill(100, 150, 140, 190, 'elem.DEFAULT_PT_SOAP')} ${fill(200, 150, 260, 152, 'elem.DEFAULT_PT_METL')}
		sim.partCreate(-1, 200, 149, elem.DEFAULT_PT_BTRY) sim.createWalls(330, 160, 1, 1, 3)
		local t = sim.partCreate(-1, 450, 120, elem.DEFAULT_PT_TIME) sim.partProperty(t, "temp", 273.15 + 48) sim.partProperty(t, "tmp2", 1000000)
		${fill(420, 90, 480, 110, 'elem.DEFAULT_PT_FIRE')}
		for x = 20, 590, 10 do local p = sim.partCreate(-1, x, 60, elem.DEFAULT_PT_PHOT) sim.partProperty(p, "vy", 3) end
		sim.partCreate(-1, 560, 150, elem.DEFAULT_PT_STKM)`,
	// a uniform custom pull (as Tilt to pour uses), and no gravity
	tilted: `sim.gravityMode(3) sim.customGravity(0.4, 0.5) ${box} ${fill(10, 150, 300, 300, '(x % 3 == 0) and elem.DEFAULT_PT_SAND or elem.DEFAULT_PT_WATR')}`,
	floating: `sim.gravityMode(1) ${box} ${fill(10, 150, 600, 250, '(y % 2 == 0) and elem.DEFAULT_PT_DUST or elem.DEFAULT_PT_GAS')}`,
	// looping edges: particles leaving one side come back on the other
	loop: `sim.edgeMode(2) ${fill(4, 4, 607, 80, '(x % 2 == 0) and elem.DEFAULT_PT_WATR or elem.DEFAULT_PT_SAND')} ${fill(4, 330, 607, 379, 'elem.DEFAULT_PT_GAS')}`,
};
const FRAMES = 200;

async function runScene(page, scene, threads) {
	await run(page, 'tpt.set_pause(1) sim.clearSim() sim.gravityMode(0) sim.edgeMode(0) tpt.newtonian_gravity(0) tpt.ambient_heat(0)');
	await call(page, 'tpt_rng_seed', null, ['number'], [4242]);
	await run(page, SCENES[scene]);
	await call(page, 'tpt_turbo', 'number', ['number'], [threads]);
	const before = await turboInfo(page);
	// seeded in the same call as the stepping: frames drawn while waiting for Lua move the sand colour cycle
	await page.evaluate(([f]) => {
		const m = window.tptPage.module;
		m.ccall('tpt_rng_seed', null, ['number'], [777]);
		m.ccall('tpt_benchmark', 'number', ['number', 'number'], [f, 0]);
	}, [FRAMES]);
	const after = await turboInfo(page);
	await call(page, 'tpt_turbo', 'number', ['number'], [0]);
	return { ...(await stateHash(page)), turboFrames: after.frames - before.frames, last: after, specialTouched: after.specialTouched - before.specialTouched };
}

test('turbo: the same result on one thread and on four, run after run; special particles never changed in parallel', async ({ page }) => {
	test.skip(!!process.env.TPT_START_PATH, 'file:// pages are never cross-origin isolated: no threads');
	test.setTimeout(600_000);
	const log = h.collect(page);
	await h.openGame(page);
	expect(await page.evaluate(() => crossOriginIsolated), 'the threaded build').toBe(true);
	for (const scene of Object.keys(SCENES)) {
		const one = await runScene(page, scene, 1);
		const four = await runScene(page, scene, 4);
		const again = await runScene(page, scene, 4);
		results.scenes[scene] = { one, four, again };
		expect(four.hash, `${scene}: four threads give what one gives`).toBe(one.hash);
		expect(again.hash, `${scene}: and again`).toBe(one.hash);
		expect(one.turboFrames, `${scene}: turbo mode ran`).toBeGreaterThan(FRAMES / 2);
		expect(four.last.parallel, `${scene}: particles were updated in parallel`).toBeGreaterThan(1000);
		for (const r of [one, four, again])
			expect(r.specialTouched, `${scene}: no special particle changed in parallel`).toBe(0);
	}
	// the scene with SOAP, sparks, a detector wall, a time field, photons and a stick figure also kept some on one thread
	expect(results.scenes.mixed.four.last.serial).toBeGreaterThan(0);
	await h.expectNoUnexplainedErrors(log);
});

test('turbo: nothing lost or doubled, a big world runs faster, off gives back the classic simulation', async ({ page }) => {
	test.skip(!!process.env.TPT_START_PATH, 'file:// pages are never cross-origin isolated: no threads');
	test.setTimeout(300_000);
	await h.openGame(page, { query: 'world=916x576x1' });
	const water = `sim.edgeMode(1) ${fill(4, 288, 911, 571, '(x % 5 == 0) and elem.DEFAULT_PT_SAND or elem.DEFAULT_PT_WATR')}`;
	const measure = async (threads) => {
		await run(page, 'tpt.set_pause(1) sim.clearSim() ' + water);
		const count = Number(await h.loopLua(page, 'tpt.get_numOfParts()'));
		await call(page, 'tpt_turbo', 'number', ['number'], [threads]);
		await call(page, 'tpt_benchmark', 'number', ['number', 'number'], [10, 0]);
		const ms = [];
		for (let k = 0; k < 5; k++) ms.push(await call(page, 'tpt_benchmark', 'number', ['number', 'number'], [20, 0]) / 20);
		ms.sort((a, b) => a - b);
		const after = Number(await h.loopLua(page, 'tpt.get_numOfParts()'));
		await call(page, 'tpt_turbo', 'number', ['number'], [0]);
		return { count, after, ms: Math.round(ms[2] * 10) / 10 };
	};
	const off = await measure(0);
	const cores = await page.evaluate(() => navigator.hardwareConcurrency);
	const on = await measure(Math.min(8, cores));
	results.speed = { world: '916x576', cores, off, on, faster: Math.round(off.ms / on.ms * 100) / 100 };
	// a closed box: every particle is still there
	expect(on.after, 'no particle lost or doubled in turbo mode').toBe(on.count);
	expect(off.after).toBe(off.count);
	if (cores >= 4)
		expect(on.ms, `turbo on ${cores} cores is faster`).toBeLessThan(off.ms * 0.9);
});

test('turbo: the Turbo button turns it on and off, says how many cores, and is kept after reload', async ({ page }) => {
	test.skip(!!process.env.TPT_START_PATH, 'file:// pages are never cross-origin isolated: no threads');
	await h.openGame(page);
	const button = page.locator('#btn-turbo');
	await expect(button).toBeEnabled();
	await expect(button).toHaveText('Turbo: off');
	expect(await call(page, 'tpt_turbo', 'number', ['number'], [-1])).toBe(0);
	await button.click();
	const threads = await page.evaluate(() => window.tptPage.turbo.threads);
	await expect(button).toHaveText(`Turbo: on (${threads} cores)`);
	await expect(button).toHaveAttribute('aria-pressed', 'true');
	expect(await call(page, 'tpt_turbo', 'number', ['number'], [-1])).toBe(threads);
	// the running game uses it once there is enough to do
	await run(page, `tpt.set_pause(0) sim.clearSim() ${fill(10, 200, 600, 370, 'elem.DEFAULT_PT_WATR')}`);
	const before = (await turboInfo(page)).frames;
	await h.waitFrames(page, 20);
	expect((await turboInfo(page)).frames, 'frames ran in turbo mode').toBeGreaterThan(before);
	await h.saveScreenshot(page, '44-turbo-on');
	await page.reload();
	await page.waitForFunction(() => window.tptPage && window.tptPage.state === 'running', null, { timeout: 60_000 });
	await expect(button).toHaveText(`Turbo: on (${threads} cores)`);
	expect(await call(page, 'tpt_turbo', 'number', ['number'], [-1])).toBe(threads);
	await button.click();
	await expect(button).toHaveText('Turbo: off');
	expect(await call(page, 'tpt_turbo', 'number', ['number'], [-1])).toBe(0);
});

test('turbo: the single-threaded build has no Turbo button to press', async ({ page }) => {
	await h.openGame(page, { query: 'threads=0' });
	await expect(page.locator('#btn-turbo')).toBeDisabled();
	await expect(page.locator('#btn-turbo')).toHaveText('Turbo: unavailable');
});
