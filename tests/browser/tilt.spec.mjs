// Tilt to pour (phones): the Tilt button turns the device's tilt into the game's gravity, sand pours towards the
// lower side, a shake pushes loose particles, and turning it off puts the old gravity back.
import { test, expect, devices } from '@playwright/test';
import * as h from './helpers.mjs';

// the player allowed the motion sensors (Chromium, like iOS, asks first)
test.use({ ...devices['Pixel 7'], permissions: ['accelerometer', 'gyroscope', 'magnetometer'] });

const results = {};
test.afterAll(() => h.writeArtifact('tilt.json', results));

const step = (page, frames) => page.evaluate((f) => window.tptPage.module.ccall('tpt_benchmark', 'number', ['number', 'number'], [f, 0]), frames);
const orient = (page, beta, gamma) => page.evaluate(([b, g]) => {
	window.dispatchEvent(new DeviceOrientationEvent('deviceorientation', { alpha: 0, beta: b, gamma: g }));
}, [beta, gamma]);
// a few readings, so the smoothing settles on the new direction
async function holdAt(page, beta, gamma) {
	for (let k = 0; k < 20; k++) await orient(page, beta, gamma);
}
const gravity = (page) => h.loopLua(page, '(function() local x, y = sim.customGravity() return sim.gravityMode() .. " " .. x .. " " .. y end)()')
	.then((t) => { const [mode, x, y] = t.split(' ').map(Number); return { mode, x, y }; });
// mean x of the sand
const sandX = (page) => h.loopLua(page, '(function() local n, sx = 0, 0 for i in sim.parts() do if sim.partProperty(i, "type") == elem.DEFAULT_PT_SAND then n = n + 1 sx = sx + sim.partProperty(i, "x") end end return n > 0 and sx / n or -1 end)()').then(Number);

test('tilt: the direction from the sensor angles, including a turned screen', async ({ page }) => {
	await h.openGame(page);
	const toGame = (b, g, a) => page.evaluate(([b, g, a]) => window.tptPage.tilt.toGame(b, g, a), [b, g, a]);
	const close = (v, x, y) => Math.abs(v.x - x) < 1e-6 && Math.abs(v.y - y) < 1e-6;
	// upright: straight down; lying flat: nothing
	expect(close(await toGame(90, 0, 0), 0, 1)).toBe(true);
	expect(close(await toGame(0, 0, 0), 0, 0)).toBe(true);
	// leaning back 45° and right edge down 45°: down and to the right
	const right = await toGame(45, 45, 0);
	expect(right.x).toBeGreaterThan(0.4);
	expect(right.y).toBeGreaterThan(0.6);
	// held sideways with its top to the left (screen turned 90°) and standing up: straight down on screen
	expect(close(await toGame(0, -90, 90), 0, 1)).toBe(true);
	// the same hold with the screen not turned: things fall to the left
	expect(close(await toGame(0, -90, 0), -1, 0)).toBe(true);
});

test('tilt: sand pours to the lower side, a shake pushes it, off restores gravity', async ({ page }) => {
	test.setTimeout(120_000);
	const log = h.collect(page);
	await h.openGame(page);
	const button = page.locator('#touch-bar [data-touch=tilt]');
	await expect(button).toBeVisible();
	// a closed box with a heap of sand in the middle
	await h.loopLua(page, `(function() tpt.set_pause(1) sim.clearSim() sim.gravityMode(0)
		for x = 100, 500 do sim.partCreate(-1, x, 120, elem.DEFAULT_PT_GLAS) sim.partCreate(-1, x, 360, elem.DEFAULT_PT_GLAS) end
		for y = 120, 360 do sim.partCreate(-1, 100, y, elem.DEFAULT_PT_GLAS) sim.partCreate(-1, 500, y, elem.DEFAULT_PT_GLAS) end
		for x = 270, 330 do for y = 200, 280 do sim.partCreate(-1, x, y, elem.DEFAULT_PT_SAND) end end return 0 end)()`);
	const start = await sandX(page);

	await button.click();
	await expect(button).toHaveAttribute('aria-pressed', 'true');
	// tipped well over onto the right side (a gentler tilt leaves sand resting in a corner, like real sand on a
	// slope less steep than its heap)
	await holdAt(page, 20, 70);
	const g1 = await gravity(page);
	expect(g1.mode, 'custom gravity while tilting').toBe(3);
	expect(g1.x).toBeGreaterThan(0.5);
	await step(page, 300);
	const afterRight = await sandX(page);
	// over onto the left side
	await holdAt(page, 20, -70);
	expect((await gravity(page)).x).toBeLessThan(-0.5);
	await step(page, 400);
	const afterLeft = await sandX(page);
	results.sandX = { start, afterRight, afterLeft };
	expect(afterRight - start, 'sand moved right').toBeGreaterThan(60);
	expect(afterLeft - afterRight, 'then left').toBeLessThan(-100);
	await h.saveScreenshot(page, '12-tilt-poured-left');

	// lying flat: no pull at all, the sand stays where it is
	await holdAt(page, 0, 0);
	const flat = await gravity(page);
	expect(Math.hypot(flat.x, flat.y)).toBeLessThan(0.05);

	// a shake pushes loose particles, a gentle movement doesn't
	const speed = () => h.loopLua(page, '(function() local s = 0 for i in sim.parts() do s = s + math.abs(sim.partProperty(i, "vx")) + math.abs(sim.partProperty(i, "vy")) end return s end)()').then(Number);
	await step(page, 120);
	const before = await speed();
	await page.evaluate(() => window.dispatchEvent(new DeviceMotionEvent('devicemotion', { acceleration: { x: 4, y: 3, z: 0 }, interval: 16 })));
	expect(await page.evaluate(() => window.tptPage.tilt.shakes), 'a gentle movement is not a shake').toBe(0);
	await page.evaluate(() => window.dispatchEvent(new DeviceMotionEvent('devicemotion', { acceleration: { x: 22, y: 9, z: 4 }, interval: 16 })));
	expect(await page.evaluate(() => window.tptPage.tilt.shakes)).toBe(1);
	const after = await speed();
	results.shake = { before, after };
	expect(after, 'the shake set the sand moving').toBeGreaterThan(before + 1000);

	// off: the gravity it had before
	await button.click();
	await expect(button).toHaveAttribute('aria-pressed', 'false');
	const g2 = await gravity(page);
	expect(g2).toEqual({ mode: 0, x: 0, y: 0 });
	await h.expectNoUnexplainedErrors(log);
});
