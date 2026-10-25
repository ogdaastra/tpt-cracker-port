// TPT Ultimata's tools and switches, driven through the game's own UI: TEXT, RULR, TRFM, CNFG, PRP2,
// hollow brushes and the field quick options in the second side-bar column.
import { test, expect } from '@playwright/test';
import * as h from './helpers.mjs';

const results = {};
test.afterAll(() => h.writeArtifact('ultimata-tools.json', results));

async function selectTool(page, id, button = 0) {
	await page.evaluate(([i, b]) => window.tptPage.module.ccall('tpt_select_tool', null, ['string', 'number'], [i, b]), [id, button]);
	await h.waitFrames(page, 6);
	// not through the Lua console: tools like PRP2 open a window that has the keyboard
	const active = await page.evaluate(() => window.tptPage.module.ccall('tpt_active_tools', 'string', [], []).split('\n'));
	expect(active[button]).toBe(id);
}

// Count particles of a type inside a rectangle; also return their bounding box.
async function region(page, type, x0, y0, x1, y1) {
	const r = await h.luaFile(page, `
		local n, minx, miny, maxx, maxy = 0, 9999, 9999, -1, -1
		for i in sim.parts() do
			local x, y = sim.partPosition(i)
			x, y = math.floor(x + 0.5), math.floor(y + 0.5)
			if sim.partProperty(i, "type") == ${type} and x >= ${x0} and x <= ${x1} and y >= ${y0} and y <= ${y1} then
				n = n + 1
				minx, miny, maxx, maxy = math.min(minx, x), math.min(miny, y), math.max(maxx, x), math.max(maxy, y)
			end
		end
		return n .. "," .. minx .. "," .. miny .. "," .. maxx .. "," .. maxy`);
	const [n, minx, miny, maxx, maxy] = r.split(',').map(Number);
	return { n, w: maxx - minx + 1, h: maxy - miny + 1, minx, miny };
}

// Pixels of the electric-field colours (green 73,216,151 or magenta 182,39,104 blended) in a rectangle.
async function colouredIn(page, x0, y0, x1, y1) {
	return pixelsIn(page, x0, y0, x1, y1, (r, g, b) => (g > r + 40 && g > b) || (r > g + 60 && b > g + 20));
}

async function litIn(page, x0, y0, x1, y1) {
	return pixelsIn(page, x0, y0, x1, y1, (r, g, b) => r + g + b > 150);
}

// Pixels of the screen matching a test, inside a rectangle given in game coordinates.
async function pixelsIn(page, x0, y0, x1, y1, test) {
	const a = await h.gamePoint(page, x0, y0);
	const b = await h.gamePoint(page, x1, y1);
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
		return n;
	}, [png.toString('base64'), test.toString()]);
}

test.beforeEach(async ({ page }) => {
	await h.openGame(page);
	await h.lua(page, 'tpt.set_pause(1) or sim.clearSim() or 0');
});

test('tools are in the Tools menu', async ({ page }) => {
	const menus = await page.evaluate(() => JSON.parse(window.tptPage.module.ccall('tpt_element_list', 'string', [], [])).menus);
	const tools = menus.find((m) => m.id === 13).tools.map((t) => t.id);
	for (const id of ['DEFAULT_UI_TRANSFORM', 'DEFAULT_UI_CONFIG', 'DEFAULT_UI_TEXT', 'DEFAULT_UI_RULER', 'DEFAULT_UI_PROPERTY2'])
		expect(tools, id).toContain(id);
});

test('TEXT writes text as particles', async ({ page }) => {
	const log = h.collect(page);
	const dmnd = Number(await h.lua(page, 'elem.DEFAULT_PT_DMND'));
	await selectTool(page, 'DEFAULT_UI_TEXT');
	await h.click(page, 100, 300); // the text goes here; the window opens at (306, 50)
	await h.saveScreenshot(page, '30-text-window');
	await h.click(page, 330, 85); // text field
	await page.keyboard.type('HI');
	await h.waitFrames(page, 4);
	await page.keyboard.press('Control+Enter');
	await h.waitFrames(page, 10);
	const r = await region(page, dmnd, 0, 0, 611, 383);
	results.text = r;
	expect(r.n, 'DMND pixels of "HI"').toBeGreaterThan(15);
	expect(r.minx).toBeGreaterThanOrEqual(95);
	expect(r.miny).toBeGreaterThanOrEqual(280);
	await h.expectNoUnexplainedErrors(log);
});

test('RULR draws a measured line and creates nothing', async ({ page }) => {
	const log = h.collect(page);
	await selectTool(page, 'DEFAULT_UI_RULER');
	expect(await litIn(page, 110, 99, 290, 101), 'nothing drawn yet').toBe(0);
	await h.click(page, 100, 100); // opens the ruler window, first point
	await h.click(page, 300, 100); // second point: 200 px
	await h.waitFrames(page, 6);
	await h.saveScreenshot(page, '31-ruler');
	const lit = await litIn(page, 110, 99, 290, 101);
	results.ruler = { litOnLine: lit };
	expect(lit, 'the line between the points is drawn').toBeGreaterThan(150);
	await page.keyboard.press('Escape'); // closes the ruler (the console can't open over it)
	await h.waitFrames(page, 6);
	expect(Number(await h.luaFile(page, 'local n = 0 for i in sim.parts() do n = n + 1 end return n'))).toBe(0);
	await h.expectNoUnexplainedErrors(log);
});

test('TRFM rotates a selection by 90 degrees', async ({ page }) => {
	const log = h.collect(page);
	const dmnd = Number(await h.lua(page, 'elem.DEFAULT_PT_DMND'));
	await h.lua(page, 'sim.createBox(200, 150, 219, 159, elem.DEFAULT_PT_DMND) or 0');
	expect((await region(page, dmnd, 0, 0, 611, 383)).n).toBe(200);
	await selectTool(page, 'DEFAULT_UI_TRANSFORM');
	await h.click(page, 199, 149); // first corner, opens the window at (402, 244)
	await h.click(page, 220, 160); // second corner
	await h.click(page, 100, 330); // third click takes the selection
	await h.saveScreenshot(page, '32-transform-selected');
	await h.click(page, 510, 313); // rotation field
	await page.keyboard.press('Control+a');
	await page.keyboard.type('90');
	await h.waitFrames(page, 4);
	await h.click(page, 552, 366); // OK
	await h.waitFrames(page, 10);
	await h.saveScreenshot(page, '32-transform-rotated');
	const r = await region(page, dmnd, 0, 0, 611, 383);
	results.transform = r;
	expect(r.w, 'width after rotation').toBeGreaterThanOrEqual(9);
	expect(r.w).toBeLessThanOrEqual(12);
	expect(r.h, 'height after rotation').toBeGreaterThanOrEqual(19);
	expect(r.h).toBeLessThanOrEqual(22);
	expect(r.n).toBeGreaterThan(170);
	await h.expectNoUnexplainedErrors(log);
});

test('CNFG sets a WIFI channel through its dialog', async ({ page }) => {
	const log = h.collect(page);
	await h.lua(page, 'sim.partCreate(-1, 300, 200, elem.DEFAULT_PT_WIFI) or 0');
	await selectTool(page, 'DEFAULT_UI_CONFIG');
	await h.click(page, 300, 200); // window near the particle, at (205, 132)
	await h.saveScreenshot(page, '33-config-wifi');
	await h.click(page, 335, 159); // channel field
	await page.keyboard.press('Control+a');
	await page.keyboard.type('5');
	await h.click(page, 347, 182); // OK
	await h.waitFrames(page, 6);
	const temp = Number(await h.lua(page, 'sim.partProperty(sim.partID(300, 200), "temp")'));
	results.config = { temp };
	expect(temp).toBeCloseTo(473.15, 1); // channel 5 = (5 - 1) * 100 + 73.15 K
	await h.expectNoUnexplainedErrors(log);
});

test('hollow brushes: Shift+Tab switches to a hollow circle', async ({ page }) => {
	const log = h.collect(page);
	const dust = Number(await h.lua(page, 'elem.DEFAULT_PT_DUST'));
	await selectTool(page, 'DEFAULT_PT_DUST');
	await h.luaFile(page, 'tpt.brushx = 10 tpt.brushy = 10 return 0');
	const p = await h.gamePoint(page, 300, 200);
	await page.mouse.move(p.x, p.y);
	await page.keyboard.press('Shift+Tab');
	await h.waitFrames(page, 4);
	await h.click(page, 300, 200);
	const ring = await region(page, dust, 285, 185, 315, 215);
	const centre = await region(page, dust, 297, 197, 303, 203);
	results.hollow = { ring: ring.n, centre: centre.n };
	expect(ring.n, 'outline drawn').toBeGreaterThan(30);
	expect(centre.n, 'centre left empty').toBe(0);
	await page.keyboard.press('Shift+Tab'); // back to solid brushes
	await h.lua(page, 'sim.clearSim() or 0');
	await h.click(page, 300, 200);
	expect((await region(page, dust, 297, 197, 303, 203)).n, 'solid again').toBeGreaterThan(20);
	await h.expectNoUnexplainedErrors(log);
});

test('field switches in the second column turn the simulations and views on (time dilation view over a SLOW field)', async ({ page }) => {
	const log = h.collect(page);
	expect(await h.lua(page, 'tostring(sim.emField())')).toBe('false');
	expect(await h.lua(page, 'tostring(sim.stressField())')).toBe('false');
	await h.click(page, 637, 8); // EM simulation
	expect(await h.lua(page, 'tostring(sim.emField())')).toBe('true');
	await h.click(page, 637, 56); // stress simulation
	expect(await h.lua(page, 'tostring(sim.stressField())')).toBe('true');
	// a SLOW field (time dilation) shows in green where there are no particles once the T view is on
	await h.luaFile(page, 'for k = 1, 20 do sim.toolBox(250, 150, 350, 250, sim.TOOL_SLOW) end return 0');
	const greenish = (r, g, b) => g > r + 2 && g > b;
	const before = await pixelsIn(page, 260, 160, 340, 240, greenish);
	await h.click(page, 637, 88); // time dilation view
	await h.waitFrames(page, 6);
	await h.saveScreenshot(page, '34-time-dilation-view');
	const after = await pixelsIn(page, 260, 160, 340, 240, greenish);
	results.fields = { greenBefore: before, greenAfter: after };
	expect(after, 'the slowed area is tinted green').toBeGreaterThan(before + 1000);
	await h.click(page, 637, 8); // EM off again
	expect(await h.lua(page, 'tostring(sim.emField())')).toBe('false');
	await h.expectNoUnexplainedErrors(log);
});

test('PRP2 opens its window and sets two properties at once', async ({ page }) => {
	const log = h.collect(page);
	await h.lua(page, 'sim.createBox(290, 190, 310, 210, elem.DEFAULT_PT_DMND) or 0');
	await selectTool(page, 'DEFAULT_UI_PROPERTY2');
	await h.waitFrames(page, 10);
	await h.saveScreenshot(page, '35-prp2-window');
	// one "property:value" per line in the text field (window centred at 223, 89), Ctrl+Enter confirms
	await h.click(page, 260, 215);
	await page.keyboard.type('temp:500');
	await page.keyboard.press('Enter');
	await page.keyboard.type('life:7');
	await page.keyboard.press('Control+Enter');
	await h.waitFrames(page, 10);
	await h.click(page, 300, 200);
	const props = await h.luaFile(page, 'local i = sim.partID(300, 200) return sim.partProperty(i, "temp") .. "," .. sim.partProperty(i, "life")');
	results.prp2 = props;
	const [temp, life] = props.split(',').map(Number);
	expect(temp).toBeCloseTo(500, 0);
	expect(life).toBe(7);
	await h.expectNoUnexplainedErrors(log);
});
