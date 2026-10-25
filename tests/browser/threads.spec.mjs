// The threaded build (section 5 of docs/NEXT-PLANS.md): the page loads powder-mt.* when it is cross-origin
// isolated (COOP/COEP headers from vercel.json) and powder.* otherwise. The gravity field is then computed on
// its own thread. These tests compare the two builds and check the fallback.
import { test, expect } from '@playwright/test';
import * as h from './helpers.mjs';

const results = {};
test.afterAll(() => h.writeArtifact('threads.json', results));

// Frame cost with a gravity field that changes every frame (moving GRVT), median of 5 rounds of 60 frames.
async function gravityFrameCost(page, gravity) {
	await page.evaluate((g) => window.tptPage.module.ccall('tpt_lua_command', null, ['string'], [`
		sim.clearSim() tpt.set_pause(1) tpt.newtonian_gravity(${g})
		for x = 40, 580, 60 do for y = 60, 340, 40 do sim.partCreate(-1, x, y, elem.DEFAULT_PT_NBHL) end end
		for x = 30, 590, 4 do for y = 20, 60, 4 do
			local i = sim.partCreate(-1, x, y, elem.DEFAULT_PT_GRVT)
			if i >= 0 then sim.partProperty(i, "tmp", 50) sim.partProperty(i, "life", 100000) end
		end end`]), gravity);
	await h.waitFrames(page, 20);
	const bench = (n) => page.evaluate((f) => window.tptPage.module.ccall('tpt_benchmark', 'number', ['number', 'number'], [f, 0]) / f, n);
	await bench(30);
	const t = [];
	for (let k = 0; k < 5; k++) t.push(await bench(60));
	t.sort((a, b) => a - b);
	return Math.round(t[2] * 1000) / 1000;
}

// A dust particle 50 px from a block of black holes is pulled in and eaten within 2 s of game time.
async function gravityPulls(page) {
	await h.luaFile(page, `
		sim.clearSim() tpt.set_pause(1) tpt.newtonian_gravity(1) sim.gravityMode(1)
		for x = 290, 309 do for y = 190, 209 do sim.partCreate(-1, x, y, elem.DEFAULT_PT_NBHL) end end
		_G.probeDust = sim.partCreate(-1, 360, 200, elem.DEFAULT_PT_DUST)
		return 0`);
	await page.evaluate(() => window.tptPage.module.ccall('tpt_benchmark', 'number', ['number', 'number'], [120, 0]));
	const left = await h.luaFile(page, `
		local alive = sim.partProperty(_G.probeDust, "type") == elem.DEFAULT_PT_DUST
		sim.gravityMode(0) tpt.newtonian_gravity(0)
		return alive and "still there" or "eaten"`);
	return left;
}

test('cross-origin isolated page runs the threaded build; gravity moves off the main thread', async ({ page }) => {
	test.skip(!!process.env.TPT_START_PATH, 'file:// pages are never cross-origin isolated');
	const log = h.collect(page);
	await h.openGame(page);
	const info = await page.evaluate(() => ({ isolated: self.crossOriginIsolated, threads: window.tptPage.threads, threaded: window.tptPage.buildInfo.threaded }));
	expect(info.isolated, 'COOP/COEP headers make the page cross-origin isolated').toBe(true);
	expect(info.threaded, 'build-info.json lists the threaded build').toBeTruthy();
	expect(info.threads, 'the page chose the threaded build').toBe(true);
	expect(log.requests.some((r) => /powder-mt\.wasm$/.test(r.url))).toBe(true);
	expect(await gravityPulls(page)).toBe('eaten');
	const threaded = { off: await gravityFrameCost(page, 0), on: await gravityFrameCost(page, 1) };
	results.threaded = threaded;
	await h.expectNoUnexplainedErrors(log);

	// the same page with ?threads=0 runs the single-threaded build: gravity costs the main thread more
	const log2 = h.collect(page);
	await page.goto(h.START + '?threads=0');
	await page.waitForFunction(() => window.tptPage && window.tptPage.state === 'running', null, { timeout: 60_000 });
	await h.waitFrames(page, 60);
	expect(await page.evaluate(() => window.tptPage.threads)).toBe(false);
	expect(log2.requests.some((r) => /powder\.wasm$/.test(r.url))).toBe(true);
	expect(await gravityPulls(page)).toBe('eaten');
	const single = { off: await gravityFrameCost(page, 0), on: await gravityFrameCost(page, 1) };
	results.single = single;
	// gravity's extra cost per frame on the main thread
	const extraThreaded = threaded.on - threaded.off;
	const extraSingle = single.on - single.off;
	results.gravityExtraMs = { threaded: extraThreaded, single: extraSingle };
	expect(extraSingle, 'gravity costs the single-threaded build something').toBeGreaterThan(0.5);
	expect(extraThreaded, 'and much less with its own thread').toBeLessThan(extraSingle / 2);
});

test('without the isolation headers the page falls back to the single-threaded build', async ({ page }) => {
	test.skip(!!process.env.TPT_START_PATH, 'the offline file:// run is this case already');
	const log = h.collect(page);
	// strip COOP/COEP from every response, as a host without them would serve the site
	await page.route('**/*', async (route) => {
		const response = await route.fetch();
		const headers = { ...response.headers() };
		delete headers['cross-origin-opener-policy'];
		delete headers['cross-origin-embedder-policy'];
		await route.fulfill({ response, headers });
	});
	await h.openGame(page);
	const info = await page.evaluate(() => ({ isolated: self.crossOriginIsolated, threads: window.tptPage.threads }));
	results.fallback = info;
	expect(info).toEqual({ isolated: false, threads: false });
	expect(log.requests.some((r) => /powder-mt\./.test(r.url)), 'the threaded build is not even downloaded').toBe(false);
	expect(await gravityPulls(page)).toBe('eaten');
	await h.expectNoUnexplainedErrors(log);
});
