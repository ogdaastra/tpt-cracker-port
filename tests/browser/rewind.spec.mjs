// Rewind: the page keeps the last 30 seconds (a snapshot every 30 frames, memory capped); the slider shows any of
// them exactly as it was (every particle and the air compared), Resume carries on from there, Back to now returns,
// Shift+Backspace goes back a second, and the on/off choice is kept after a reload.
import { createHash } from 'node:crypto';
import { test, expect } from '@playwright/test';
import * as h from './helpers.mjs';

const results = {};
test.afterAll(() => h.writeArtifact('rewind.json', results));

// water, sand, wood, dust, fire and plasma in a walled box, and powered wires: things move and change every frame
const SCENE = `
	sim.clearSim()
	for x = 20, 590 do for y = 300, 379 do sim.partCreate(-1, x, y, (x % 7 == 0) and elem.DEFAULT_PT_SAND or elem.DEFAULT_PT_WATR) end end
	for x = 20, 590 do for y = 200, 230 do sim.partCreate(-1, x, y, (x % 5 == 0) and elem.DEFAULT_PT_WOOD or elem.DEFAULT_PT_DUST) end end
	for x = 20, 590, 4 do sim.partCreate(-1, x, 195, elem.DEFAULT_PT_FIRE) end
	for x = 100, 140 do for y = 100, 140 do sim.partCreate(-1, x, y, elem.DEFAULT_PT_PLSM) end end
	for row = 0, 3 do local y = 30 + row * 12 sim.partCreate(-1, 300, y, elem.DEFAULT_PT_BTRY) for x = 301, 580 do sim.partCreate(-1, x, y, elem.DEFAULT_PT_METL) end end
	sim.createWallBox(0, 0, 611, 3, 8) sim.createWallBox(0, 380, 611, 383, 8) sim.createWallBox(0, 0, 3, 383, 8) sim.createWallBox(608, 0, 611, 383, 8)`;

// Lua run in the game loop (not the console, whose window stops the simulation); returns what it writes to out
let runs = 0;
async function loopLua(page, code, out = `/tmp/rewind${++runs}.txt`) {
	await page.evaluate(([c, p]) => window.tptPage.module.ccall('tpt_lua_command', null, ['string'],
		[`${c.replace(/\s*\n\s*/g, ' ')} local f = io.open("${p}.done", "w") f:write("1") f:close()`]), [code, out]);
	for (let k = 0; k < 3000; k++) {
		const done = await page.evaluate((p) => { try { window.tptPage.module.FS.readFile(p + '.done'); return true; } catch (e) { return false; } }, out);
		if (done) return out;
		await page.waitForTimeout(10);
	}
	throw new Error('Lua command did not run');
}

async function stateHash(page) {
	const file = await loopLua(page, h.particleStateLua(`/tmp/state${runs + 1}.txt`), `/tmp/state${runs + 1}`);
	const text = await page.evaluate((p) => new TextDecoder().decode(window.tptPage.module.FS.readFile(p + '.txt')), file);
	return createHash('sha256').update(text).digest('hex');
}

const step = (page, frames) => page.evaluate((f) => window.tptPage.module.ccall('tpt_benchmark', 'number', ['number', 'number'], [f, 0]), frames);
const info = (page) => page.evaluate(() => window.tptPage.rewind && { ...window.tptPage.rewind, timer: undefined });
const paused = async (page) => (await loopLua(page, 'local f = io.open("/tmp/paused.txt", "w") f:write(tostring(tpt.set_pause())) f:close()'),
	page.evaluate(() => new TextDecoder().decode(window.tptPage.module.FS.readFile('/tmp/paused.txt'))));

async function setSlider(page, value) {
	await page.locator('#rewind-slider').evaluate((el, v) => {
		el.value = String(v);
		el.dispatchEvent(new Event('input', { bubbles: true }));
	}, value);
}

test('rewind: any moment of the last seconds comes back exactly; resume, back to now, hotkey, off, kept after reload', async ({ page }) => {
	test.setTimeout(240_000);
	const log = h.collect(page);
	await h.openGame(page);
	await loopLua(page, 'tpt.set_pause(1)');
	await loopLua(page, SCENE);
	// start the buffer from here: off and on again (frames are counted from when it was turned on)
	await page.click('#btn-rewind');
	await expect(page.locator('#rewind-bar')).toBeVisible();
	await expect(page.locator('#rewind-on')).toBeChecked();
	await page.locator('#rewind-on').uncheck();
	await expect(page.locator('#rewind-info')).toHaveText('Off.');
	await page.locator('#rewind-on').check();

	// 900 frames (snapshots at 30, 60, ... 900), remember that moment, then 600 more
	await step(page, 900);
	const at900 = await stateHash(page);
	await step(page, 600);
	const at1500 = await stateHash(page);
	expect(at1500).not.toBe(at900);
	await page.waitForTimeout(700); // the bar refreshes twice a second
	let r = await info(page);
	results.after1500 = { snapshots: r.count, megabytes: Math.round(r.bytes / 1048576) };
	expect(r.count, '30 seconds of snapshots at most (61 = now and 60 before)').toBeLessThanOrEqual(61);
	expect(r.bytes, 'within the memory cap').toBeLessThanOrEqual(150 * 1048576);
	await h.saveScreenshot(page, '10-rewind-bar');

	// 600 frames back = 20 snapshots: exactly the state at frame 900
	await setSlider(page, -20);
	await expect(page.locator('#rewind-time')).toHaveText('−10.0 s');
	expect(await paused(page), 'rewinding pauses the game').toBe('1');
	expect(await stateHash(page), 'the state 10 seconds back').toBe(at900);
	// further back and forward again by the slider
	await setSlider(page, -35);
	await setSlider(page, -20);
	expect(await stateHash(page)).toBe(at900);
	await h.saveScreenshot(page, '10-rewind-back');

	// the newest snapshot is exactly frame 1500 (shown while still paused: Back to now also unpauses)
	await page.evaluate(() => window.tptPage.module.ccall('tpt_rewind_show', 'number', ['number'], [0]));
	expect(await stateHash(page), 'back at frame 1500').toBe(at1500);
	// Back to now: the newest moment, and the game runs again
	await setSlider(page, -12);
	await page.click('#rewind-now');
	await expect(page.locator('#rewind-time')).toHaveText('now');
	expect(await paused(page)).toBe('0');

	// Resume from a moment in the past: carries on from there, everything after it is forgotten
	await loopLua(page, 'tpt.set_pause(1)');
	r = await info(page);
	await setSlider(page, -10);
	await page.click('#rewind-resume');
	expect(await paused(page)).toBe('0');
	await page.waitForTimeout(700);
	const afterResume = await info(page);
	expect(afterResume.view).toBe(0);
	expect(afterResume.count, 'the 10 newer snapshots are gone').toBeLessThanOrEqual(r.count - 10 + 3);

	// Shift+Backspace: back about a second (2 snapshots), with the bar open
	await loopLua(page, 'tpt.set_pause(1)');
	await page.click('#rewind-close');
	await expect(page.locator('#rewind-bar')).toBeHidden();
	await step(page, 120);
	await page.keyboard.press('Shift+Backspace');
	await expect(page.locator('#rewind-bar')).toBeVisible();
	await expect(page.locator('#rewind-time')).toHaveText('−1.0 s');

	// off: nothing kept, and the choice survives a reload
	await page.locator('#rewind-on').uncheck();
	await page.waitForTimeout(700);
	expect((await info(page)).count).toBe(0);
	await page.reload();
	await h.openGame(page);
	await page.click('#btn-rewind');
	await expect(page.locator('#rewind-on')).not.toBeChecked();
	await page.locator('#rewind-on').check();
	results.ok = true;
	await h.expectNoUnexplainedErrors(log);
});
