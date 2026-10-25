// The renderer's drawing loops were rewritten for speed (section 6 of docs/NEXT-PLANS.md: fire glow,
// blur, glow, blob and gravity lensing). The new code must draw exactly what the old code drew: the hashes
// below are of frames rendered by the build before the rewrite (commit 0cf6b0c), threaded and single-threaded,
// and the rewritten build gives the same ones. A deliberate change to how things look updates them.
import { test, expect } from '@playwright/test';
import * as h from './helpers.mjs';

const results = {};
test.afterAll(() => h.writeArtifact('render.json', results));

// A still scene (paused, no gravity, no random effects): liquids against the top-left and bottom-right edges,
// radioactive glow at the left edge, a block of water with lava in it (blur, glow, fire), an acid line.
const SCENE = `
	tpt.hud(0) sim.clearSim() tpt.set_pause(1) tpt.newtonian_gravity(0)
	for x = 0, 40 do for y = 0, 12 do sim.partCreate(-1, x, y, elem.DEFAULT_PT_WATR) end end
	for x = 570, 611 do for y = 370, 383 do sim.partCreate(-1, x, y, elem.DEFAULT_PT_OIL) end end
	for x = 0, 8 do for y = 100, 140 do sim.partCreate(-1, x, y, elem.DEFAULT_PT_PLUT) end end
	for x = 100, 500 do for y = 250, 300 do sim.partCreate(-1, x, y, (x + y) % 7 == 0 and elem.DEFAULT_PT_LAVA or elem.DEFAULT_PT_WATR) end end
	for x = 50, 560 do sim.partCreate(-1, x, 200, elem.DEFAULT_PT_ACID) end
	for x = 0, 611, 2 do sim.partCreate(-1, x, 383, elem.DEFAULT_PT_ISOZ) end`;

// display mode (tpt.display_mode) -> first 16 hex digits of the SHA-256 of the screenshot (PPM, 612x384)
const EXPECTED = {
	0: '01d58cb62b71f14b', // velocity
	3: '990e711df5cbe8f6', // fire
	4: '280ead29e6188f12', // blob
	5: '96efc5573bc3783c', // heat
	6: '4f5c09d80a31e015', // fancy
	8: '831c7f7e52a848e0', // heat gradient
};

// The game's own screenshot of the simulation area (tpt.screenshot(1, 2): uncompressed PPM), hashed.
async function frameHash(page) {
	return page.evaluate(async () => {
		const M = window.tptPage.module;
		M.ccall('tpt_lua_command', null, ['string'], ['tpt.screenshot(1, 2)']);
		const files = M.FS.readdir('.').filter((f) => f.startsWith('screenshot') && f.endsWith('.ppm'));
		if (files.length !== 1) return `expected one screenshot, found ${files.length}`;
		const data = M.FS.readFile(files[0]);
		M.FS.unlink(files[0]);
		const digest = new Uint8Array(await crypto.subtle.digest('SHA-256', data));
		return [...digest].map((b) => b.toString(16).padStart(2, '0')).join('').slice(0, 16);
	});
}

test('display modes draw exactly what they drew before the renderer was sped up', async ({ page }) => {
	test.setTimeout(180_000);
	const log = h.collect(page);
	await h.openGame(page);
	await page.evaluate((code) => window.tptPage.module.ccall('tpt_lua_command', null, ['string'], [code]), SCENE);
	const got = {};
	for (const mode of Object.keys(EXPECTED)) {
		await page.evaluate((m) => window.tptPage.module.ccall('tpt_lua_command', null, ['string'], [`tpt.display_mode(${m})`]), mode);
		// the mode's name fades out over the picture, and fire takes a moment to settle: wait for a steady frame
		await page.waitForTimeout(3500);
		let previous = null;
		for (let k = 0; k < 20; k++) {
			got[mode] = await frameHash(page);
			if (got[mode] === previous) break;
			previous = got[mode];
			await page.waitForTimeout(300);
		}
	}
	results.hashes = got;
	expect(got).toEqual(EXPECTED);
	await h.expectNoUnexplainedErrors(log);
});
