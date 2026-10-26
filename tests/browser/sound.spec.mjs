// Sound effects: off by default (no AudioContext at all), the Sound button turns them on, events from the game
// reach the page and play (sparks, fire, explosions, steam, acid), volume, off again, kept after a reload.
import { test, expect } from '@playwright/test';
import * as h from './helpers.mjs';

const results = {};
test.afterAll(() => h.writeArtifact('sound.json', results));

// one of each: a battery on a metal wire, burning wood, a bomb dropped on a block, water on lava, acid on wood
const SCENE = `
	sim.clearSim()
	sim.createWallBox(0, 370, 611, 373, 8)
	sim.partCreate(-1, 40, 60, elem.DEFAULT_PT_BTRY)
	for x = 41, 200 do sim.partCreate(-1, x, 60, elem.DEFAULT_PT_METL) end
	for x = 250, 300 do for y = 340, 369 do sim.partCreate(-1, x, y, elem.DEFAULT_PT_WOOD) end end
	for x = 250, 300, 3 do sim.partCreate(-1, x, 339, elem.DEFAULT_PT_FIRE) end
	for x = 380, 420 do for y = 350, 369 do sim.partCreate(-1, x, y, elem.DEFAULT_PT_STNE) end end
	sim.partCreate(-1, 400, 300, elem.DEFAULT_PT_BOMB)
	for x = 460, 500 do for y = 355, 369 do local id = sim.partCreate(-1, x, y, elem.DEFAULT_PT_LAVA) sim.partProperty(id, "temp", 2500) end end
	for x = 460, 500 do for y = 330, 340 do sim.partCreate(-1, x, y, elem.DEFAULT_PT_WATR) end end
	for x = 530, 580 do for y = 345, 369 do sim.partCreate(-1, x, y, elem.DEFAULT_PT_WOOD) end end
	for x = 530, 580 do for y = 320, 330 do sim.partCreate(-1, x, y, elem.DEFAULT_PT_ACID) end end
	tpt.set_pause(0)
	return 0`;

const soundState = (page) => page.evaluate(() => {
	const s = window.tptPage.sound;
	return { on: s.on, ctx: s.ctx ? s.ctx.state : null, gain: s.master ? s.master.gain.value : null, played: { ...s.played }, heard: { ...s.heard } };
});

test('sound effects: off by default, on from the Sound button, every kind plays, volume, off, kept after reload', async ({ page }) => {
	test.setTimeout(120_000);
	const log = h.collect(page);
	await h.openGame(page);

	// off by default: no audio at all
	await expect(page.locator('#btn-sound')).toHaveText('Sound: off');
	await expect(page.locator('#sound-volume')).toBeHidden();
	await h.luaFile(page, SCENE);
	await page.waitForTimeout(1500);
	let s = await soundState(page);
	expect(s.ctx, 'no AudioContext while sound is off').toBeNull();
	expect(Object.values(s.played).every((n) => n === 0)).toBe(true);

	// on: the click starts the AudioContext
	await page.click('#btn-sound');
	await expect(page.locator('#btn-sound')).toHaveText('Sound: on');
	await expect(page.locator('#btn-sound')).toHaveAttribute('aria-pressed', 'true');
	await expect(page.locator('#sound-volume')).toBeVisible();
	await h.luaFile(page, SCENE);
	await page.waitForTimeout(4000);
	s = await soundState(page);
	results.on = s;
	expect(s.ctx).toBe('running');
	for (const kind of ['spark', 'fire', 'explosion', 'steam', 'acid']) {
		expect(s.heard[kind], `${kind} events from the game`).toBeGreaterThan(0);
		expect(s.played[kind], `${kind} sounds played`).toBeGreaterThan(0);
	}
	await h.saveScreenshot(page, '09-sound-on');
	// and each kind is an actual sound: rendered offline, its waveform isn't silent
	results.peaks = {};
	for (const kind of ['spark', 'fire', 'explosion', 'steam', 'acid']) {
		results.peaks[kind] = await page.evaluate((k) => window.tptPage.sound.preview(k), kind);
		expect(results.peaks[kind], `${kind} waveform peak`).toBeGreaterThan(0.01);
	}

	// volume
	await page.locator('#sound-volume').fill('25');
	expect((await soundState(page)).gain).toBeCloseTo(0.25, 5);

	// off: the audio stops and nothing more plays
	await page.click('#btn-sound');
	await expect(page.locator('#btn-sound')).toHaveText('Sound: off');
	await page.waitForTimeout(300);
	const before = await soundState(page);
	await page.waitForTimeout(1500);
	const after = await soundState(page);
	expect(after.ctx).toBe('suspended');
	expect(after.played).toEqual(before.played);

	// on again, then reload: the choice and the volume are kept, and the audio waits for a click
	await page.click('#btn-sound');
	await page.waitForTimeout(300);
	await page.reload();
	await h.openGame(page, { dismissIntro: false });
	await expect(page.locator('#btn-sound')).toHaveText('Sound: on');
	await expect(page.locator('#sound-volume')).toHaveValue('25');
	expect((await soundState(page)).ctx, 'no audio before the first click or key press').toBeNull();
	await page.keyboard.press('Shift'); // dismisses the mod's intro, and counts as the first key press
	await h.waitFrames(page, 60);
	await h.luaFile(page, SCENE);
	await page.waitForTimeout(2500);
	s = await soundState(page);
	results.afterReload = s;
	expect(s.ctx).toBe('running');
	expect(s.played.spark + s.played.fire).toBeGreaterThan(0);
	await h.expectNoUnexplainedErrors(log);
});
