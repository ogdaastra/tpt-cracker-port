// World size (the page's World size setting, the game's world:WxHxS argument): small worlds are drawn with
// bigger pixels so they fill the screen, big ones have more room; the mouse lands on the pixel under it, the side
// bar, bottom bar and menus follow the edges, and the setting restarts the game bringing the world along.
import { test, expect } from '@playwright/test';
import * as h from './helpers.mjs';

const results = { sizes: {} };
test.afterAll(() => h.writeArtifact('world.json', results));

const lua = (page, expr) => h.loopLua(page, expr);
const canvasSize = (page) => page.evaluate(() => [document.querySelector('#canvas').width, document.querySelector('#canvas').height]);

async function clickAt(page, p) {
	await page.mouse.move(p.x, p.y);
	await h.waitFrames(page, 2);
	await page.mouse.down();
	await h.waitFrames(page, 3);
	await page.mouse.up();
	await h.waitFrames(page, 3);
}

// Share of the screen pixels in a rectangle of canvas pixels that pass a test
async function shareOfPixels(page, x0, y0, x1, y1, test) {
	const a = await h.screenPoint(page, x0, y0);
	const b = await h.screenPoint(page, x1, y1);
	const png = await page.screenshot({ clip: { x: a.x, y: a.y, width: b.x - a.x, height: b.y - a.y } });
	return page.evaluate(async ([b64, testSource]) => {
		const match = new Function('r', 'g', 'b', `return (${testSource})(r, g, b);`);
		const bmp = await createImageBitmap(await (await fetch('data:image/png;base64,' + b64)).blob());
		const c = new OffscreenCanvas(bmp.width, bmp.height);
		const ctx = c.getContext('2d');
		ctx.drawImage(bmp, 0, 0);
		const d = ctx.getImageData(0, 0, bmp.width, bmp.height).data;
		let n = 0;
		for (let i = 0; i < d.length; i += 4) if (match(d[i], d[i + 1], d[i + 2])) n++;
		return n / (d.length / 4);
	}, [png.toString('base64'), test.toString()]);
}
const bright = (r, g, b) => r + g + b > 450; // DMND is (204, 204, 255)

// The checks every world size gets: sizes, clicks, drawing scale, the bars and the menus
async function checkWorld(page, w, hgt, s) {
	const W = w * s, H = hgt * s; // the simulation area on screen
	const dx = W - 612, dy = H - 384; // how far the side bar and the bottom bar moved from the classic layout
	expect(await page.evaluate(() => window.tptPage.world)).toEqual({ w, h: hgt, scale: s });
	expect(await canvasSize(page)).toEqual([W + 34, H + 40]);
	expect(await lua(page, 'sim.XRES .. "x" .. sim.YRES')).toBe(`${w}x${hgt}`);
	const classic = w === 612 && hgt === 384;
	expect(await page.locator('#btn-challenges').isDisabled(), 'challenges need the classic world').toBe(!classic);

	// clicks with a one-pixel brush: the particle is on the simulation pixel under the mouse
	await lua(page, '(function() tpt.set_pause(1) sim.clearSim() tpt.brushx = 0 tpt.brushy = 0 return 0 end)()');
	const landed = [];
	for (const [x, y] of [[8, 8], [w >> 1, hgt >> 1], [w - 10, hgt - 10]]) {
		await clickAt(page, await h.worldPoint(page, x, y));
		const at = await lua(page, '(function() for i in sim.parts() do local x, y = sim.partPosition(i) return math.floor(x + 0.5) .. "," .. math.floor(y + 0.5) end return "none" end)()');
		landed.push(`${x},${y} -> ${at}`);
		expect(at, `click on ${x},${y}`).toBe(`${x},${y}`);
		await lua(page, '(function() sim.clearSim() return 0 end)()');
	}

	// drawn s times bigger: a 10 x 10 block of DMND covers a 10s x 10s square on screen, and not beyond it
	// (below the text in the top left corner, which is the same size in every world)
	await lua(page, '(function() for x = 40, 49 do for y = 60, 69 do sim.partCreate(-1, x, y, elem.DEFAULT_PT_DMND) end end return 0 end)()');
	await h.waitFrames(page, 6);
	const inside = await shareOfPixels(page, 40 * s + 1, 60 * s + 1, 50 * s - 1, 70 * s - 1, bright);
	const beside = await shareOfPixels(page, 50 * s + 2, 60 * s + 1, 50 * s + 2 + 4 * s, 70 * s - 1, bright);
	expect(inside, 'the block fills its square').toBeGreaterThan(0.95);
	expect(beside, 'nothing next to it').toBeLessThan(0.05);

	// the menus moved with the edges (classic positions from categories.spec.mjs)
	const menuAt = async (x, y) => {
		await page.mouse.move(...Object.values(await h.screenPoint(page, x + dx, y + dy)));
		await h.waitFrames(page, 6);
		return Number(await lua(page, 'tpt.active_menu()'));
	};
	expect(await menuAt(620, 143)).toBe(0); // Walls
	expect(await menuAt(620, 175)).toBe(2); // Powered Materials
	expect(await menuAt(637, 143)).toBe(18); // Life and Nature
	// the pause button, last in the bottom bar
	const paused = await lua(page, 'tpt.set_pause()');
	await clickAt(page, await h.screenPoint(page, W + 8, H + 31));
	expect(await lua(page, 'tpt.set_pause()'), 'the pause button').toBe(paused === '1' ? '0' : '1');
	return { landed, inside, beside };
}

for (const [w, hgt, s] of [[308, 192, 2], [204, 128, 3], [156, 96, 4]]) {
	test(`world ${w} x ${hgt} with ${s}x pixels: fills the screen, the mouse lands on its pixel, the bars and menus follow`, async ({ page }) => {
		const log = h.collect(page);
		await h.openGame(page, { query: `world=${w}x${hgt}x${s}` });
		results.sizes[`${w}x${hgt}x${s}`] = await checkWorld(page, w, hgt, s);
		await lua(page, '(function() tpt.set_pause(0) for x = 10, ' + (w - 10) + ', 3 do sim.partCreate(-1, x, 12, elem.DEFAULT_PT_SAND) end return 0 end)()');
		await h.waitFrames(page, 30);
		await h.saveScreenshot(page, `43-world-${w}x${hgt}x${s}`);
		await h.expectNoUnexplainedErrors(log);
	});
}

// a window big enough to show the biggest worlds pixel for pixel
test.describe('big worlds', () => {
test.use({ viewport: { width: 1700, height: 1100 } });
for (const [w, hgt] of [[916, 576], [1224, 768]]) {
	test(`world ${w} x ${hgt}: more room, the mouse lands on its pixel, the bars and menus follow, sand falls to the new floor`, async ({ page }) => {
		test.setTimeout(120_000);
		const log = h.collect(page);
		await h.openGame(page, { query: `world=${w}x${hgt}x1` });
		const r = await checkWorld(page, w, hgt, 1);
		// a column of sand dropped near the far corner settles on the bottom edge (solid edges: with the default
		// void edges sand falls out of the world)
		await lua(page, `(function() sim.clearSim() sim.edgeMode(1) for y = 20, 60 do sim.partCreate(-1, ${w - 40}, y, elem.DEFAULT_PT_SAND) end tpt.set_pause(0) return 0 end)()`);
		await page.evaluate(() => window.tptPage.module.ccall('tpt_benchmark', 'number', ['number', 'number'], [600, 0]));
		const lowest = Number(await lua(page, '(function() local m = 0 for i in sim.parts() do local x, y = sim.partPosition(i) m = math.max(m, y) end return math.floor(m + 0.5) end)()'));
		expect(lowest, 'sand reached the bottom of the bigger world').toBeGreaterThan(hgt - 10);
		// cost of a frame, a whole screen of particles: for the docs
		await lua(page, `(function() tpt.set_pause(1) sim.clearSim() for x = 4, ${w - 5} do for y = ${hgt >> 1}, ${hgt - 5} do sim.partCreate(-1, x, y, elem.DEFAULT_PT_WATR) end end return 0 end)()`);
		const ms = await page.evaluate(() => window.tptPage.module.ccall('tpt_benchmark', 'number', ['number', 'number'], [60, 0]) / 60);
		results.sizes[`${w}x${hgt}x1`] = { ...r, lowest, halfWaterMsPerFrame: Math.round(ms * 10) / 10 };
		await h.saveScreenshot(page, `43-world-${w}x${hgt}x1`);
		await h.expectNoUnexplainedErrors(log);
	});
}
});

test('world size dialog: sizes the game would refuse are refused; a change restarts with the world brought along, and back', async ({ page }) => {
	test.setTimeout(180_000);
	const log = h.collect(page);
	await h.openGame(page);
	// a recognisable block in the middle of the classic world
	await lua(page, '(function() tpt.set_pause(1) sim.clearSim() for x = 300, 309 do for y = 180, 189 do sim.partCreate(-1, x, y, elem.DEFAULT_PT_DMND) end end return 0 end)()');

	await page.click('#btn-world');
	await expect(page.locator('#world-dialog')).toBeVisible();
	await expect(page.locator('#world-apply')).toBeDisabled(); // the size it has
	// custom: 100 x 100 at 2x is 200 x 200 on screen, too small for the menus
	await page.locator('#world-presets input[value=custom]').check();
	await page.fill('#world-custom input[name=w]', '100');
	await page.fill('#world-custom input[name=h]', '100');
	await page.selectOption('#world-custom select[name=scale]', '2');
	await expect(page.locator('#world-summary')).toContainText('needs at least 612');
	await expect(page.locator('#world-apply')).toBeDisabled();
	await h.saveScreenshot(page, '43-world-dialog-refused');
	// Medium: 308 x 192 with 2x pixels
	await page.locator('#world-presets input[value="2"]').check();
	await expect(page.locator('#world-summary')).toContainText('616 × 384 on screen');
	await h.saveScreenshot(page, '43-world-dialog');
	await Promise.all([page.waitForEvent('load'), page.click('#world-apply')]);
	await page.waitForFunction(() => window.tptPage && window.tptPage.state === 'running', null, { timeout: 60_000 });
	await page.waitForFunction(() => window.tptPage.worldCarried, null, { timeout: 20_000 });
	expect(await page.evaluate(() => window.tptPage.world)).toEqual({ w: 308, h: 192, scale: 2 });
	const carried = await page.evaluate(() => window.tptPage.worldCarried);
	results.carried = carried;
	// centred: (612 - 308) / 2 = 152 to the left, (384 - 192) / 2 = 96 up
	const block = '(function() local n, x0, y0 = 0, 9999, 9999 for i in sim.parts() do if sim.partProperty(i, "type") == elem.DEFAULT_PT_DMND then local x, y = sim.partPosition(i) n = n + 1 x0 = math.min(x0, math.floor(x + 0.5)) y0 = math.min(y0, math.floor(y + 0.5)) end end return n .. "," .. x0 .. "," .. y0 end)()';
	expect(await lua(page, block)).toBe('100,148,84');
	// the setting is kept: a reload keeps the size
	await page.reload();
	await page.waitForFunction(() => window.tptPage && window.tptPage.state === 'running', null, { timeout: 60_000 });
	expect(await page.evaluate(() => window.tptPage.world)).toEqual({ w: 308, h: 192, scale: 2 });

	// and back to Classic, the block brought along again
	await h.waitFrames(page, 60);
	await lua(page, '(function() tpt.set_pause(1) sim.clearSim() for x = 148, 157 do for y = 84, 93 do sim.partCreate(-1, x, y, elem.DEFAULT_PT_DMND) end end return 0 end)()');
	await page.click('#btn-world');
	await page.locator('#world-presets input[value="3"]').check();
	await Promise.all([page.waitForEvent('load'), page.click('#world-apply')]);
	await page.waitForFunction(() => window.tptPage && window.tptPage.state === 'running', null, { timeout: 60_000 });
	await page.waitForFunction(() => window.tptPage.worldCarried, null, { timeout: 20_000 });
	expect(await page.evaluate(() => window.tptPage.world)).toEqual({ w: 612, h: 384, scale: 1 });
	expect(await lua(page, block)).toBe('100,300,180');
	await expect(page.locator('#btn-challenges')).toBeEnabled();
	await h.expectNoUnexplainedErrors(log);
});

test('world size: an address asking for a size the game refuses gives the classic world on both sides', async ({ page }) => {
	await h.openGame(page, { query: 'world=100x100x1' });
	expect(await page.evaluate(() => window.tptPage.world)).toEqual({ w: 612, h: 384, scale: 1 });
	expect(await canvasSize(page)).toEqual([646, 424]);
	expect(await lua(page, 'sim.XRES .. "x" .. sim.YRES')).toBe('612x384');
});
