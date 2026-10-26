// Deep undo: Ctrl+Z goes back through every stroke to before the first pixel and Ctrl+Y forward again to exactly
// the same state; the history is capped in memory, dropping the oldest steps first.
import { test, expect } from '@playwright/test';
import { createHash } from 'node:crypto';
import * as h from './helpers.mjs';

const results = {};
test.afterAll(() => h.writeArtifact('undo.json', results));

const info = async (page) => {
	const [count, position, bytes, cap] = (await page.evaluate(() => window.tptPage.module.ccall('tpt_undo_info', 'string', [], []))).split(' ').map(Number);
	return { count, position, bytes, cap };
};
const count = (page) => h.loopLua(page, '(function() local n = 0 for i in sim.parts() do n = n + 1 end return n end)()').then(Number);

let states = 0;
async function stateHash(page) {
	const out = `/tmp/undostate${++states}`;
	await h.loopLua(page, `(function() ${h.particleStateLua(out + '.txt').replace(/\s*\n\s*/g, ' ')} return 0 end)()`);
	const text = await page.evaluate((p) => new TextDecoder().decode(window.tptPage.module.FS.readFile(p)), out + '.txt');
	return createHash('sha256').update(text).digest('hex');
}

async function keys(page, key, times) {
	for (let k = 0; k < times; k++) {
		await page.keyboard.press(key);
		await page.waitForTimeout(20);
	}
	await h.waitFrames(page, 4);
}

test('undo: 60 strokes back to an empty screen, redo forward to exactly the same state', async ({ page }) => {
	test.setTimeout(180_000);
	const log = h.collect(page);
	await h.openGame(page);
	const start = await info(page);
	results.start = start;
	expect(start.cap, 'the page set a memory cap').toBeGreaterThanOrEqual(40 << 20);
	await h.loopLua(page, '(function() sim.clearSim() tpt.set_pause(1) tpt.brushx = 2 tpt.brushy = 2 return 0 end)()');
	await page.locator('#canvas').focus();

	// 60 separate clicks of dust: each is one undo step
	const STROKES = 60;
	for (let k = 0; k < STROKES; k++) {
		await h.click(page, 40 + (k % 12) * 45, 60 + Math.floor(k / 12) * 60);
	}
	const drawn = await count(page);
	const drawnHash = await stateHash(page);
	const full = await info(page);
	results.afterStrokes = { particles: drawn, ...full };
	expect(drawn, 'particles drawn').toBeGreaterThan(STROKES * 5);
	expect(full.position, 'undo steps available').toBeGreaterThanOrEqual(STROKES);

	// Ctrl+Z all the way back: nothing left
	await keys(page, 'Control+z', STROKES);
	expect(await count(page), 'everything undone').toBe(0);
	// a few strokes back forward, then the rest: the same particles in the same places
	await keys(page, 'Control+y', 10);
	const partway = await count(page);
	expect(partway).toBeGreaterThan(0);
	expect(partway).toBeLessThan(drawn);
	await keys(page, 'Control+y', STROKES - 10);
	expect(await count(page), 'everything redone').toBe(drawn);
	expect(await stateHash(page), 'redo restores exactly the drawn state').toBe(drawnHash);
	results.undoRedo = { partway, redone: drawn };
	await h.saveScreenshot(page, '42-undo-redone');
	await h.expectNoUnexplainedErrors(log);
});

test('undo: the history keeps to its memory cap by dropping the oldest steps, and still undoes', async ({ page }) => {
	test.setTimeout(120_000);
	const log = h.collect(page);
	await h.openGame(page);
	await page.evaluate(() => window.tptPage.module.ccall('tpt_undo_setup', null, ['number', 'number'], [4, 0]));
	// a busy scene that changes everywhere every frame, so every step stores a lot
	await h.loopLua(page, '(function() sim.clearSim() for x = 20, 590, 2 do for y = 100, 370, 2 do sim.partCreate(-1, x, y, elem.DEFAULT_PT_WATR) end end tpt.set_pause(0) return 0 end)()');
	await page.locator('#canvas').focus();
	const steps = [];
	for (let k = 0; k < 20; k++) {
		await h.click(page, 50 + k * 25, 40);
		steps.push(await info(page));
	}
	results.capped = steps;
	const last = steps.at(-1);
	expect(last.cap).toBe(4 << 20);
	expect(last.count, 'the oldest steps were dropped').toBeLessThan(20);
	expect(last.bytes, 'the history keeps to its cap').toBeLessThanOrEqual(last.cap);
	await h.loopLua(page, '(function() tpt.set_pause(1) return 0 end)()');
	const before = await count(page);
	await keys(page, 'Control+z', 1);
	expect((await info(page)).position, 'one step back').toBe(last.position - 1);
	expect(await count(page), 'Ctrl+Z still goes back').not.toBe(before);
	await h.expectNoUnexplainedErrors(log);
});
