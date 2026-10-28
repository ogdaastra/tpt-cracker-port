// Visual effects (web/tpt-web.js, effects; tpt_fx_* in PowderToySDL.cpp): glow, heat haze, shockwaves and night
// lighting drawn with WebGL2 over the game. Each effect changes what it should and nothing else: the menus, the
// game's text and everything away from heat, light and pressure stay exactly as the game drew them, and the
// mouse still reaches the game.
import { test, expect } from '@playwright/test';
import { createHash } from 'node:crypto';
import * as h from './helpers.mjs';

const results = {};
test.afterAll(() => h.writeArtifact('effects.json', results));

// For each rectangle of canvas pixels [x0, y0, x1, y1): what the game drew (its own pixel buffer, tpt_fx_frame)
// and what is shown (the effects canvas when it is on, read right after it is drawn, in the same frame): mean
// brightness (0-255) of each, a hash of what is shown, and how many pixels differ. (The game's canvas itself
// is not read: the browser may already have cleared it once shown.)
async function regions(page, rects) {
	return page.evaluate((rects) => new Promise((resolve) => {
		const m = window.tptPage.module, game = document.querySelector('#canvas'), fx = document.querySelector('#fx-canvas');
		const inner = m.tptOnFrame;
		m.tptOnFrame = () => {
			if (inner) inner();
			m.tptOnFrame = inner;
			if (!inner) m.ccall('tpt_set_frame_callback', null, ['number'], [0]);
			const w = game.width, h = game.height, heap = m.HEAPU8, frame = m.ccall('tpt_fx_frame', 'number', [], []);
			let shown = null;
			if (window.tptPage.effects.active) {
				const o = new OffscreenCanvas(w, h), x = o.getContext('2d');
				x.drawImage(fx, 0, 0);
				shown = x.getImageData(0, 0, w, h).data;
			}
			resolve(rects.map(([x0, y0, x1, y1]) => {
				let a = 0, b = 0, differ = 0, hash = 0, n = 0;
				for (let y = y0; y < y1; y++)
					for (let x = x0; x < x1; x++) {
						const f = frame + (y * w + x) * 4; // B, G, R, A
						const dr = heap[f + 2], dg = heap[f + 1], db = heap[f];
						const i = (y * w + x) * 4;
						const [sr, sg, sb] = shown ? [shown[i], shown[i + 1], shown[i + 2]] : [dr, dg, db];
						a += (dr + dg + db) / 3;
						b += (sr + sg + sb) / 3;
						if (dr !== sr || dg !== sg || db !== sb) differ++;
						hash = (hash * 31 + sr * 7 + sg * 13 + sb) >>> 0;
						n++;
					}
				return { drawn: a / n, shown: b / n, differ, hash };
			}));
		};
		if (!inner) m.ccall('tpt_set_frame_callback', null, ['number'], [1]);
	}), rects);
}

async function setEffects(page, on, values = {}) {
	await page.click('#btn-effects');
	for (const k of ['glow', 'haze', 'shock', 'night']) {
		const box = page.locator(`#effects-form input[name=${k}]`);
		if ((await box.isChecked()) !== on.includes(k)) await box.click();
	}
	for (const [k, v] of Object.entries(values)) await page.locator(`#effects-form input[name=${k}]`).fill(String(v));
	await page.click('#effects-form button[value=close]');
	const frames = await page.evaluate(() => window.tptPage.effects.frames);
	if (on.length) await page.waitForFunction((f) => window.tptPage.effects.frames > f + 5, frames, { timeout: 10_000 });
	await h.waitFrames(page, 5);
}

const lua = (page, code) => h.loopLua(page, `(function() ${code} return 0 end)()`);

test('effects: off by default; glow lights up around hot things and leaves the rest as drawn', async ({ page }) => {
	const log = h.collect(page);
	await h.openGame(page);
	expect(await page.evaluate(() => window.tptPage.effects.supported), 'WebGL2').toBe(true);
	await expect(page.locator('#btn-effects')).toHaveText('Effects…');
	await expect(page.locator('#fx-canvas')).toBeHidden();
	// paused: the same picture every frame
	await lua(page, `tpt.set_pause(1) sim.clearSim()
		for x = 100, 140 do for y = 200, 240 do local p = sim.partCreate(-1, x, y, elem.DEFAULT_PT_METL) sim.partProperty(p, "temp", 2500) end end
		for x = 400, 440 do for y = 200, 240 do sim.partCreate(-1, x, y, elem.DEFAULT_PT_DMND) end end`);
	await setEffects(page, ['glow']);
	await expect(page.locator('#btn-effects')).toHaveText('Effects: on');
	await expect(page.locator('#fx-canvas')).toBeVisible();
	const [ring, cold, menus, bar] = await regions(page, [[142, 200, 152, 240], [400, 200, 440, 240], [612, 0, 646, 424], [0, 384, 612, 424]]);
	results.glow = { ring, cold, menus: menus.differ, bar: bar.differ };
	expect(ring.shown, 'the dark beside hot metal now glows').toBeGreaterThan(ring.drawn + 20);
	expect(cold.differ, 'cold diamond far away: unchanged').toBe(0);
	expect(menus.differ, 'side bar: unchanged').toBe(0);
	expect(bar.differ, 'bottom bar: unchanged').toBe(0);
	await h.saveScreenshot(page, '45-effects-glow');
	await h.expectNoUnexplainedErrors(log);
});

test('effects: heat haze shimmers above hot things only; shockwaves bend the picture where the pressure changes', async ({ page }) => {
	await h.openGame(page);
	// a striped background, so bending shows, with a hot block in it; paused
	await lua(page, `tpt.set_pause(1) sim.clearSim()
		for x = 20, 590 do for y = 60, 360 do if (x + y) % 6 < 3 then sim.partCreate(-1, x, y, elem.DEFAULT_PT_DMND) end end end
		for x = 100, 140 do for y = 300, 330 do local i = sim.pmap(x, y) if i then sim.partProperty(i, "temp", 2500) end end end`);
	await setEffects(page, ['haze']);
	const [a, far] = await regions(page, [[100, 285, 140, 320], [400, 100, 500, 200]]);
	await page.waitForTimeout(300);
	const [b] = await regions(page, [[100, 285, 140, 320]]);
	results.haze = { a, b, far };
	expect(a.differ, 'the air above and over the hot block shimmers').toBeGreaterThan(50);
	expect(a.hash, 'and keeps moving').not.toBe(b.hash);
	expect(far.differ, 'cold stripes far away: unchanged').toBe(0);

	// shockwaves: a block of high pressure in the cold stripes
	await setEffects(page, ['shock']);
	await lua(page, 'for cx = 100, 120 do for cy = 25, 45 do sim.pressure(cx, cy, 200) end end');
	await h.waitFrames(page, 3);
	const [edge, inside] = await regions(page, [[390, 90, 410, 190], [420, 120, 460, 160]]);
	results.shock = { edge: edge.differ, inside: inside.differ };
	expect(edge.differ, 'the picture bends at the edge of the pressure').toBeGreaterThan(50);
	expect(inside.differ, 'not inside it, where the pressure is even').toBe(0);
	await h.saveScreenshot(page, '45-effects-shock');
});

test('effects: night darkens what is not lit, lava lights its surroundings; text over the simulation stays as drawn; the mouse still reaches the game', async ({ page }) => {
	await h.openGame(page);
	// text the game draws over the simulation every frame (as the mod's interface is drawn)
	await lua(page, `tpt.set_pause(1) sim.clearSim()
		for x = 20, 590 do for y = 340, 370 do sim.partCreate(-1, x, y, elem.DEFAULT_PT_DMND) end end
		for x = 60, 100 do for y = 300, 339 do sim.partCreate(-1, x, y, elem.DEFAULT_PT_LAVA) end end
		tpt.register_step(function() gfx.drawText(300, 100, "TEXT OVER THE SIMULATION", 255, 255, 255, 255) end)`);
	await setEffects(page, ['night'], { darkness: 80 });
	const [lit, dark, text] = await regions(page, [[105, 345, 125, 365], [500, 345, 580, 365], [296, 96, 460, 112]]);
	results.night = { lit, dark, text: text.differ };
	expect(dark.shown, 'diamond far from the lava goes dark').toBeLessThan(dark.drawn * 0.4);
	expect(lit.shown, 'diamond beside the lava stays lit').toBeGreaterThan(dark.shown * 2);
	expect(text.differ, 'text over the simulation is as drawn').toBe(0);
	await h.saveScreenshot(page, '45-effects-night');

	// a screenshot from Record… shows the effects too
	const download = page.waitForEvent('download', { timeout: 60_000 });
	await page.click('#btn-record');
	await page.check('#record-dialog input[name=area][value=sim]');
	await page.selectOption('#record-dialog select[name=scale]', '1');
	await page.click('#record-screenshot');
	const png = await (await download).createReadStream().then((stream) => new Promise((resolve) => {
		const parts = [];
		stream.on('data', (c) => parts.push(c));
		stream.on('end', () => resolve(Buffer.concat(parts)));
	}));
	const saved = await page.evaluate(async (b64) => {
		const bmp = await createImageBitmap(await (await fetch('data:image/png;base64,' + b64)).blob());
		const c = new OffscreenCanvas(bmp.width, bmp.height), x = c.getContext('2d');
		x.drawImage(bmp, 0, 0);
		const d = x.getImageData(500, 345, 80, 20).data;
		let sum = 0;
		for (let i = 0; i < d.length; i += 4) sum += (d[i] + d[i + 1] + d[i + 2]) / 3;
		return { width: bmp.width, height: bmp.height, dark: sum / (d.length / 4) };
	}, png.toString('base64'));
	results.night.screenshot = saved;
	expect([saved.width, saved.height]).toEqual([612, 384]);
	expect(saved.dark, 'the screenshot has the night in it').toBeLessThan(dark.drawn * 0.4);

	// the effects canvas lets the mouse through: a click still places a particle
	await lua(page, 'sim.clearSim() tpt.brushx = 0 tpt.brushy = 0');
	await page.mouse.click(...Object.values(await h.worldPoint(page, 300, 200)));
	await h.waitFrames(page, 3);
	expect(await h.loopLua(page, '(function() for i in sim.parts() do local x, y = sim.partPosition(i) return math.floor(x + 0.5) .. "," .. math.floor(y + 0.5) end return "none" end)()')).toBe('300,200');

	// kept after reload
	await page.reload();
	await page.waitForFunction(() => window.tptPage && window.tptPage.state === 'running', null, { timeout: 60_000 });
	await page.waitForFunction(() => window.tptPage.effects.frames > 0, null, { timeout: 10_000 });
	expect(await page.evaluate(() => ({ night: window.tptPage.effects.night, glow: window.tptPage.effects.glow }))).toEqual({ night: true, glow: false });
	await setEffects(page, []);
	await expect(page.locator('#fx-canvas')).toBeHidden();
	await expect(page.locator('#btn-effects')).toHaveText('Effects…');
});
