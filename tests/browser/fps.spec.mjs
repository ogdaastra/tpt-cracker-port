// Simulation speed cap and drawing cap in the game's Simulation options: set through the dialog, applied to the
// engine (the values tpt.setfpscap() / tpt.setdrawcap() report), measured, and kept after a reload.
import { test, expect } from '@playwright/test';
import * as h from './helpers.mjs';

const results = {};
test.afterAll(() => h.writeArtifact('fps.json', results));

// Game coordinates in the Simulation options dialog (bottom bar button, the two dropdowns, OK). A dropdown
// opens as a list centred on the current choice, one choice every 16 pixels.
const OPTIONS_BUTTON = [588, 415];
const SPEED = [428, 301];
const DRAWING = [428, 335];
const OK = [322, 373];
const SPEED_CHOICES = [30, 60, 120, 240, 0]; // 0 = Unlimited
const DRAWING_CHOICES = [0, 30, 60, 120, 144]; // 0 = every tick

async function openOptions(page) {
	await h.click(page, ...OPTIONS_BUTTON);
	await h.waitFrames(page, 15);
}

async function choose(page, [x, y], choices, current, wanted) {
	await h.click(page, x, y);
	await h.waitFrames(page, 10);
	await h.click(page, x, y + 16 * (choices.indexOf(wanted) - choices.indexOf(current)));
	await h.waitFrames(page, 10);
}

// Runs Lua in the game loop (tpt_lua_command, no console: while the console window is open the simulation
// doesn't step) and returns what it writes to `out`.
let luaRuns = 0;
async function loopLua(page, code) {
	const out = `/tmp/fps${++luaRuns}.txt`;
	await page.evaluate(([c, p]) => window.tptPage.module.ccall('tpt_lua_command', null, ['string'],
		[`local f = io.open("${p}", "w") f:write(tostring((function() ${c} end)())) f:close()`]), [code, out]);
	for (let k = 0; k < 400; k++) {
		const text = await page.evaluate((p) => { try { return new TextDecoder().decode(window.tptPage.module.FS.readFile(p)); } catch (e) { return null; } }, out);
		if (text !== null) return text;
		await page.waitForTimeout(5);
	}
	throw new Error('Lua command did not run: ' + code);
}

// Simulation frames per second over a wall-clock window (event.aftersim runs once per simulated frame; event.tick
// only runs when a frame is drawn), and drawn frames per second (the page's frame callback).
async function rates(page, seconds = 2) {
	await page.evaluate(() => {
		window.__draws = 0;
		const m = window.tptPage.module;
		m.tptOnFrame = () => { window.__draws++; };
		m.ccall('tpt_set_frame_callback', null, ['number'], [1]);
	});
	await loopLua(page, 'SIMFRAMES = 0 if not SIMCOUNTER then SIMCOUNTER = function() SIMFRAMES = SIMFRAMES + 1 end event.register(event.aftersim, SIMCOUNTER) end return 0');
	await page.waitForTimeout(300);
	const t0 = Date.now();
	const s0 = Number(await loopLua(page, 'return SIMFRAMES')), d0 = await page.evaluate(() => window.__draws);
	await page.waitForTimeout(seconds * 1000);
	const s1 = Number(await loopLua(page, 'return SIMFRAMES')), d1 = await page.evaluate(() => window.__draws);
	const w = (Date.now() - t0) / 1000;
	await page.evaluate(() => window.tptPage.module.ccall('tpt_set_frame_callback', null, ['number'], [0]));
	return { simPerSecond: Math.round((s1 - s0) / w), drawnPerSecond: Math.round((d1 - d0) / w) };
}

test('speed and drawing caps: set in the Simulation options, applied, measured, kept after reload', async ({ page }) => {
	test.setTimeout(180_000);
	const log = h.collect(page);
	await h.openGame(page);
	await h.lua(page, 'sim.clearSim()');

	// defaults: 60 ticks per second, drawn every tick
	expect(await h.lua(page, 'tpt.setfpscap()')).toBe('60');
	expect(await h.lua(page, 'tpt.setdrawcap()')).toBe('0');
	results.default = await rates(page);
	expect(results.default.simPerSecond, 'default speed').toBeGreaterThan(45);
	expect(results.default.simPerSecond, 'default speed').toBeLessThan(70);

	// Unlimited speed, drawing capped at 30
	await openOptions(page);
	await choose(page, SPEED, SPEED_CHOICES, 60, 0);
	await choose(page, DRAWING, DRAWING_CHOICES, 0, 30);
	await h.saveScreenshot(page, '08-fps-options');
	await h.click(page, ...OK);
	await h.waitFrames(page, 10);
	expect(await h.lua(page, 'tpt.setfpscap()'), 'no cap is reported as 2').toBe('2');
	expect(await h.lua(page, 'tpt.setdrawcap()')).toBe('30');
	results.unlimited = await rates(page);
	// at least twice the normal speed (about 450 locally with an empty screen)
	expect(results.unlimited.simPerSecond, 'unlimited speed').toBeGreaterThan(120);
	expect(results.unlimited.drawnPerSecond, 'drawing cap 30').toBeLessThan(36);

	// kept after a reload (saved in the game's preferences)
	await page.waitForTimeout(500);
	await page.reload();
	await h.openGame(page);
	expect(await h.lua(page, 'tpt.setfpscap()')).toBe('2');
	expect(await h.lua(page, 'tpt.setdrawcap()')).toBe('30');
	await openOptions(page);
	await h.saveScreenshot(page, '08-fps-options-after-reload');

	// back to 60 and drawing every tick
	await choose(page, SPEED, SPEED_CHOICES, 0, 60);
	await choose(page, DRAWING, DRAWING_CHOICES, 30, 0);
	await h.click(page, ...OK);
	await h.waitFrames(page, 10);
	expect(await h.lua(page, 'tpt.setfpscap()')).toBe('60');
	expect(await h.lua(page, 'tpt.setdrawcap()')).toBe('0');
	await h.lua(page, 'sim.clearSim()');
	results.backTo60 = await rates(page);
	expect(results.backTo60.simPerSecond).toBeGreaterThan(45);
	expect(results.backTo60.simPerSecond).toBeLessThan(70);

	// a cap set by a script that isn't one of the choices still shows (as an extra choice)
	await h.lua(page, 'tpt.setfpscap(75)');
	await openOptions(page);
	await h.saveScreenshot(page, '08-fps-options-script-value');
	await h.click(page, ...OK);
	await h.waitFrames(page, 10);
	expect(await h.lua(page, 'tpt.setfpscap()')).toBe('75');
	await h.expectNoUnexplainedErrors(log);
});
