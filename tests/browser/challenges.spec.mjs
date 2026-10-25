// Challenge mode (src/lua/luascripts/challenges.lua): every challenge's reference solution wins in the real
// game and doing nothing does not; a challenge played by hand through the page, the ink budget, locked areas,
// tool restrictions, the in-game challenge window and saved stars.
import { test, expect } from '@playwright/test';
import * as h from './helpers.mjs';

const results = { solutions: {}, idle: {} };
test.afterAll(() => h.writeArtifact('challenges.json', results));

// Picks a tool in the page's Elements panel: searching opens the groups, so the chip can be clicked.
async function pick(page, name) {
	await page.fill('#element-search', name);
	await page.click(`#element-groups .chip[data-id=${name.includes('_') ? name : 'DEFAULT_PT_' + name}]`);
	await page.fill('#element-search', '');
	await page.locator('#canvas').focus();
}

async function catalog(page) {
	await page.evaluate(() => window.tptPage.challenges.command('export'));
	await page.waitForFunction(() => window.tptPage.challenges.catalog, null, { timeout: 10_000 });
	return page.evaluate(() => window.tptPage.challenges.catalog);
}

// Starts (or solves) a challenge with a page command and steps the simulation quickly with tpt_benchmark,
// which runs whole simulation frames (event.aftersim included, so the challenge checks run) without drawing.
async function play(page, id, mode, maxFrames) {
	return page.evaluate(async ({ id, mode, maxFrames }) => {
		const chal = window.tptPage.challenges;
		const m = window.tptPage.module;
		chal.events = []; // the page keeps the last 200 events; start a fresh list for this run
		const since = () => chal.events;
		chal.command(`${mode} ${id}`);
		const t0 = performance.now();
		while (!since().some((e) => e.type === 'started' && e.id === id)) {
			if (performance.now() - t0 > 10_000) {
				return { error: 'the challenge did not start' };
			}
			await new Promise((r) => setTimeout(r, 20));
		}
		let frames = 0;
		const w0 = performance.now();
		while (frames <= maxFrames) {
			m.ccall('tpt_benchmark', 'number', ['number', 'number'], [60, 0]);
			frames += 60;
			const end = since().find((e) => (e.type === 'won' || e.type === 'lost') && e.id === id);
			if (end) {
				// for a failure report: the best progress seen on the way
				const best = Math.max(0, ...since().filter((e) => e.type === 'progress').map((e) => e.progress));
				return { ...end, stepped: frames, ms: Math.round(performance.now() - w0), bestProgress: best };
			}
			await new Promise((r) => setTimeout(r, 0));
		}
		return { error: 'no result', stepped: frames };
	}, { id, mode, maxFrames });
}

test('every challenge: the reference solution wins', async ({ page }) => {
	test.setTimeout(900_000);
	const log = h.collect(page);
	await h.openGame(page);
	const cat = await catalog(page);
	results.count = cat.challenges.length;
	results.categories = cat.categories.map((c) => c.id);
	expect(cat.challenges.length, 'number of challenges').toBeGreaterThanOrEqual(45);
	for (const c of cat.categories) {
		expect(cat.challenges.filter((x) => x.category === c.id).length, `challenges in ${c.id}`).toBeGreaterThanOrEqual(2);
	}
	const failed = [];
	for (const c of cat.challenges) {
		const r = await play(page, c.id, 'solve', c.time * 60 + 120);
		results.solutions[c.id] = r;
		if (r.type !== 'won') {
			failed.push(`${c.id}: ${JSON.stringify(r)}`);
		} else {
			expect(r.solved, `${c.id} is marked as a solution run`).toBe(true);
		}
	}
	expect(failed, 'challenges whose solution did not win').toEqual([]);
	// solution runs earn no stars
	expect((await catalog(page)).progress).toEqual({});
	await page.evaluate(() => window.tptPage.challenges.command('stop'));
	await h.expectNoUnexplainedErrors(log);
});

test('every challenge: doing nothing does not win', async ({ page }) => {
	test.setTimeout(900_000);
	const log = h.collect(page);
	await h.openGame(page);
	const cat = await catalog(page);
	const won = [];
	for (const c of cat.challenges) {
		const r = await play(page, c.id, 'start', c.time * 60 + 120);
		results.idle[c.id] = r;
		if (r.type !== 'lost') {
			won.push(`${c.id}: ${JSON.stringify(r)}`);
		}
	}
	expect(won, 'challenges that were won (or never ended) without doing anything').toEqual([]);
	await page.evaluate(() => window.tptPage.challenges.command('stop'));
	await h.expectNoUnexplainedErrors(log);
});

test('play "First light" by hand from the page; the stars are saved', async ({ page }) => {
	test.setTimeout(240_000);
	const log = h.collect(page);
	await h.openGame(page);
	await page.click('#btn-challenges');
	await expect(page.locator('#challenges-dialog')).toBeVisible();
	await expect(page.locator('#challenge-cats button')).toHaveCount(9);
	await page.click('#challenge-cats button[data-category=electronics]');
	await expect(page.locator('#challenge-list li[data-id=elec-first-light]')).toBeVisible();
	await h.saveScreenshot(page, 'challenges-dialog');
	await page.click('#challenge-list li[data-id=elec-first-light] button');
	await expect(page.locator('#challenge-bar')).toBeVisible();
	await expect(page.locator('#challenge-title')).toHaveText('First light');

	// only the allowed tools are offered (plus the eraser and the sample tool)
	await page.waitForTimeout(500);
	const offered = await page.evaluate(() => JSON.parse(window.tptPage.module.ccall('tpt_element_list', 'string', [], []))
		.menus.flatMap((m) => m.tools.map((t) => t.id)).sort());
	expect(offered).toEqual(['DEFAULT_PT_BTRY', 'DEFAULT_PT_METL', 'DEFAULT_PT_NONE', 'DEFAULT_PT_PSCN', 'DEFAULT_UI_SAMPLE']);
	results.firstLightTools = offered;
	await expect(page.locator('#element-groups .chip')).toHaveCount(5);
	// other tools are refused, whether chosen from the page or from Lua
	const before = await page.evaluate(() => window.tptPage.module.ccall('tpt_active_tools', 'string', [], []));
	await page.evaluate(() => window.tptPage.module.ccall('tpt_select_tool', null, ['string', 'number'], ['DEFAULT_PT_WATR', 0]));
	await h.waitFrames(page, 10);
	expect(await page.evaluate(() => window.tptPage.module.ccall('tpt_active_tools', 'string', [], []))).toBe(before);
	expect(await h.luaFile(page, 'tpt.selectedl = "DEFAULT_PT_PLSM" return tpt.selectedl')).not.toBe('DEFAULT_PT_PLSM');

	// the LED is locked: erasing over it does nothing
	await h.luaFile(page, 'tpt.brushx = 3 tpt.brushy = 3 return 0');
	await pick(page, 'NONE');
	await h.drag(page, [[396, 196], [408, 208]]);
	expect(await h.luaFile(page, 'return challenges.C.count("LED")')).toBe('25');

	// build the circuit with the mouse, one-pixel brush: battery, metal wire, PSCN next to the LED
	await h.luaFile(page, 'tpt.brushx = 0 tpt.brushy = 0 return 0');
	await pick(page, 'BTRY');
	await h.click(page, 360, 202);
	await pick(page, 'METL');
	await h.drag(page, [[361, 202], [380, 202], [396, 202]]);
	await pick(page, 'PSCN');
	await h.drag(page, [[397, 202], [399, 202]]);
	const built = await h.luaFile(page, 'return challenges.C.count("BTRY") .. " " .. challenges.C.count("METL") .. " " .. challenges.C.count("PSCN") .. " " .. challenge.ink()');
	results.firstLightBuilt = built;
	const [btry, metl, pscn, ink] = built.split(' ').map(Number);
	expect(btry).toBeGreaterThanOrEqual(1);
	expect(metl).toBeGreaterThanOrEqual(30);
	expect(pscn).toBeGreaterThanOrEqual(1);
	expect(ink, 'ink counts every particle the player placed').toBe(btry + metl + pscn);

	// challenges start paused; unpause and wait for the win
	await page.locator('#canvas').focus();
	await page.keyboard.press('Space');
	await page.waitForFunction(() => window.tptPage.challenges.events.some((e) => e.type === 'won' && e.id === 'elec-first-light'), null, { timeout: 30_000 });
	const won = await page.evaluate(() => window.tptPage.challenges.events.find((e) => e.type === 'won'));
	results.firstLightWon = won;
	expect(won.solved).toBe(false);
	expect(won.stars).toBeGreaterThanOrEqual(1);
	await expect(page.locator('#challenge-status')).toContainText('Complete');
	await h.saveScreenshot(page, 'challenges-won');
	// the tools are free again after the challenge
	await page.waitForTimeout(500);
	const freeTools = await page.evaluate(() => JSON.parse(window.tptPage.module.ccall('tpt_element_list', 'string', [], []))
		.menus.reduce((n, m) => n + m.tools.length, 0));
	expect(freeTools).toBeGreaterThan(400);

	// the stars survive a reload (progress file in the browser's persistent storage)
	await page.waitForTimeout(1500); // IDBFS sync
	await page.reload();
	await h.openGame(page);
	const cat = await catalog(page);
	expect(cat.progress['elec-first-light'] && cat.progress['elec-first-light'].stars).toBe(won.stars);
	await page.click('#btn-challenges');
	await page.click('#challenge-cats button[data-category=electronics]');
	await expect(page.locator('#challenge-list li[data-id=elec-first-light] .stars')).toHaveText('★'.repeat(won.stars));
	await page.click('#challenges-dialog button[value=close]');
	await h.expectNoUnexplainedErrors(log);
});

test('ink budget, hints, restart and the in-game challenge window', async ({ page }) => {
	test.setTimeout(240_000);
	const log = h.collect(page);
	await h.openGame(page);
	await catalog(page);

	// "Reach for the sky" allows 300 bricks: scribbling with a big brush stops at exactly 300
	await page.evaluate(() => window.tptPage.challenges.command('start puzzle-tower'));
	await expect(page.locator('#challenge-bar')).toBeVisible();
	await h.luaFile(page, 'tpt.brushx = 8 tpt.brushy = 8 return 0');
	await pick(page, 'BRCK');
	await h.drag(page, [[100, 150], [200, 160], [100, 200], [200, 210], [100, 250]]);
	const inkUsed = await h.luaFile(page, 'return challenges.C.count("BRCK") .. " " .. challenge.ink()');
	results.inkBudget = inkUsed;
	expect(inkUsed).toBe('300 300');
	// the flag is locked
	await pick(page, 'NONE');
	await h.drag(page, [[395, 90], [420, 100]]);
	expect(await h.luaFile(page, 'return challenges.C.count("GOLD")')).toBe('80');

	// hints show in the bar; each costs a star
	await page.click('#challenge-hint');
	await expect(page.locator('#challenge-text')).toContainText('Hint 1:');
	expect(await h.luaFile(page, 'return challenges.state() and 1')).toBe('1');

	// restart rebuilds the scene and clears the ink
	await page.click('#challenge-restart');
	await page.waitForTimeout(500);
	expect(await h.luaFile(page, 'return challenges.C.count("BRCK") .. " " .. challenge.ink()')).toBe('0 0');

	// the in-game window: the star button at the bottom of the second side-bar column
	await page.click('#challenge-stop');
	await expect(page.locator('#challenge-bar')).toBeHidden();
	await h.click(page, 637, 399);
	await h.waitFrames(page, 10);
	await h.saveScreenshot(page, 'challenges-ingame-window');
	// window 420x290 centred in 646x424: categories at x 123-243 (Electronics is the second), challenges at x 253-523
	await h.click(page, 183, 119);
	await h.click(page, 388, 101);
	await page.waitForFunction(() => window.tptPage.challenges.events.some((e) => e.type === 'started' && e.id === 'elec-first-light'), null, { timeout: 10_000 });
	await page.evaluate(() => window.tptPage.challenges.command('stop'));
	await h.expectNoUnexplainedErrors(log);
});
