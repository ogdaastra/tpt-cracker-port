// Phone and tablet: one finger draws, two fingers zoom and move the view without drawing, a touch still lands
// where the finger is when zoomed, and the touch bar's buttons work (pause, eraser, brush size, undo, elements).
import { test, expect, devices } from '@playwright/test';
import * as h from './helpers.mjs';

test.use({ ...devices['Pixel 7'] });

const results = {};
test.afterAll(() => h.writeArtifact('touch.json', results));

const count = (page) => h.loopLua(page, '(function() local n = 0 for i in sim.parts() do n = n + 1 end return n end)()').then(Number);

// CDP touch events: several fingers at once, which page.touchscreen can't do
async function touches(page, cdp, frames) {
	const box = await page.locator('#canvas').boundingBox();
	const at = ([fx, fy], id) => ({ x: box.x + box.width * fx, y: box.y + box.height * fy, id });
	for (const [type, points] of frames) {
		await cdp.send('Input.dispatchTouchEvent', { type, touchPoints: points.map((p, i) => at(p, i)) });
		await page.waitForTimeout(16);
	}
}

// two fingers pressed together at a and b, moved to c and d in steps, lifted
function twoFingerMove(a, b, c, d, steps = 12) {
	const frames = [['touchStart', [a, b]]];
	for (let k = 1; k <= steps; k++) {
		const t = k / steps;
		const lerp = (p, q) => [p[0] + (q[0] - p[0]) * t, p[1] + (q[1] - p[1]) * t];
		frames.push(['touchMove', [lerp(a, c), lerp(b, d)]]);
	}
	frames.push(['touchEnd', []]);
	return frames;
}

test('touch: draw with one finger, zoom and pan with two (no drawing), taps land where the finger is, touch bar', async ({ page, context }) => {
	test.setTimeout(120_000);
	const log = h.collect(page);
	await h.openGame(page);
	const cdp = await context.newCDPSession(page);
	await expect(page.locator('#touch-bar')).toBeVisible();
	await h.loopLua(page, '(function() sim.clearSim() tpt.set_pause(1) return 0 end)()');

	// one finger draws
	await touches(page, cdp, [['touchStart', [[0.2, 0.5]]], ...Array.from({ length: 10 }, (_, k) => ['touchMove', [[0.2 + 0.02 * (k + 1), 0.5]]]), ['touchEnd', []]]);
	await page.waitForTimeout(300);
	const drawn = await count(page);
	expect(drawn, 'particles drawn with one finger').toBeGreaterThan(50);

	// two fingers apart: zoom in, nothing drawn
	await touches(page, cdp, twoFingerMove([0.45, 0.5], [0.55, 0.5], [0.3, 0.5], [0.7, 0.5]));
	await page.waitForTimeout(300);
	let view = await page.evaluate(() => ({ ...window.tptPage.touch.view }));
	results.afterPinch = view;
	expect(view.scale, 'zoomed in').toBeGreaterThan(2.5);
	expect(await count(page), 'a pinch draws nothing').toBe(drawn);
	await expect(page.locator('#touch-bar [data-touch=zoom-reset]')).toBeVisible();

	// two fingers together: the view moves, nothing drawn
	const before = view.x;
	await touches(page, cdp, twoFingerMove([0.4, 0.5], [0.5, 0.5], [0.5, 0.5], [0.6, 0.5]));
	await page.waitForTimeout(300);
	view = await page.evaluate(() => ({ ...window.tptPage.touch.view }));
	expect(view.x, 'panned').not.toBe(before);
	expect(await count(page), 'a pan draws nothing').toBe(drawn);
	await h.saveScreenshot(page, '11-touch-zoomed');

	// zoomed in, a one-pixel tap creates the particle under the finger
	await h.loopLua(page, '(function() sim.clearSim() tpt.brushx = 0 tpt.brushy = 0 return 0 end)()');
	const box = await page.locator('#canvas').boundingBox(); // the transformed rectangle
	const tap = { x: box.x + box.width * 0.4, y: box.y + box.height * 0.45 };
	await page.touchscreen.tap(tap.x, tap.y);
	await page.waitForTimeout(400);
	const where = await h.loopLua(page, '(function() for i in sim.parts() do local x, y = sim.partPosition(i) return math.floor(x + 0.5) .. " " .. math.floor(y + 0.5) end return "none" end)()');
	const expected = [Math.floor((tap.x - box.x) / box.width * h.GAME_W), Math.floor((tap.y - box.y) / box.height * h.GAME_H)];
	results.tap = { where, expected };
	expect(where).not.toBe('none');
	const [px, py] = where.split(' ').map(Number);
	expect(Math.abs(px - expected[0]), `x of the particle (${where}) vs under the finger (${expected})`).toBeLessThanOrEqual(2);
	expect(Math.abs(py - expected[1]), `y of the particle (${where}) vs under the finger (${expected})`).toBeLessThanOrEqual(2);

	// Reset zoom
	await page.click('#touch-bar [data-touch=zoom-reset]');
	expect(await page.evaluate(() => window.tptPage.touch.view.scale)).toBe(1);
	expect(await page.locator('#canvas').evaluate((c) => c.style.transform)).toBe('');

	// the bar: brush size, pause, undo, eraser, elements
	await h.loopLua(page, '(function() tpt.brushx = 8 tpt.brushy = 8 return 0 end)()');
	await page.click('#touch-bar [data-touch=brush-up]');
	await expect.poll(() => h.loopLua(page, 'tpt.brushx').then(Number)).toBeGreaterThan(8);
	await page.click('#touch-bar [data-touch=brush-down]');
	await page.click('#touch-bar [data-touch=brush-down]');
	await expect.poll(() => h.loopLua(page, 'tpt.brushx').then(Number)).toBeLessThan(8);

	const paused = await h.loopLua(page, 'tpt.set_pause()');
	await page.click('#touch-bar [data-touch=pause]');
	await expect.poll(() => h.loopLua(page, 'tpt.set_pause()')).toBe(paused === '1' ? '0' : '1');
	await page.click('#touch-bar [data-touch=pause]');
	await expect.poll(() => h.loopLua(page, 'tpt.set_pause()')).toBe(paused);

	await h.loopLua(page, '(function() sim.clearSim() tpt.set_pause(1) return 0 end)()');
	await touches(page, cdp, [['touchStart', [[0.3, 0.4]]], ...Array.from({ length: 6 }, (_, k) => ['touchMove', [[0.3 + 0.03 * (k + 1), 0.4]]]), ['touchEnd', []]]);
	await page.waitForTimeout(300);
	const stroke = await count(page);
	expect(stroke).toBeGreaterThan(0);
	await page.click('#touch-bar [data-touch=undo]');
	await expect.poll(() => count(page), { message: 'undo removes the stroke' }).toBeLessThan(stroke);

	const leftTool = () => page.evaluate(() => window.tptPage.module.ccall('tpt_active_tools', 'string', [], []).split('\n')[0]);
	const toolBefore = await leftTool();
	await page.click('#touch-bar [data-touch=eraser]');
	await expect(page.locator('#touch-bar [data-touch=eraser]')).toHaveAttribute('aria-pressed', 'true');
	await expect.poll(leftTool).toBe('DEFAULT_PT_NONE');
	await page.click('#touch-bar [data-touch=eraser]');
	await expect.poll(leftTool).toBe(toolBefore);

	await page.click('#touch-bar [data-touch=elements]');
	await expect(page.locator('#elements')).toBeVisible();
	await h.saveScreenshot(page, '11-touch-elements');
	await h.expectNoUnexplainedErrors(log);
});
