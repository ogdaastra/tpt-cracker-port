// The split menus (second side-bar column) and the page's element browser.
import { test, expect } from '@playwright/test';
import * as h from './helpers.mjs';

const results = {};
test.afterAll(() => h.writeArtifact('categories.json', results));

const NEW_MENUS = { 18: 'Life and Nature', 19: 'Space and Gravity', 20: 'Characters and Vehicles', 21: 'Circuits (voltage simulation)', 22: 'Metals' };
// menus that list tools rather than elements; their size is not about browsing elements
const NOT_ELEMENT_MENUS = new Set([0, 12, 13, 15]); // walls, Game of Life, tools, decoration

async function elementList(page) {
	return page.evaluate(() => JSON.parse(window.tptPage.module.ccall('tpt_element_list', 'string', [], [])).menus);
}

async function activeTools(page) {
	return page.evaluate(() => window.tptPage.module.ccall('tpt_active_tools', 'string', [], []).split('\n'));
}

test('menus: new categories exist, every element is in one menu, no element menu is huge', async ({ page }) => {
	const log = h.collect(page);
	await h.openGame(page);
	const menus = await elementList(page);
	const byId = Object.fromEntries(menus.map((m) => [m.id, m]));
	for (const [id, name] of Object.entries(NEW_MENUS)) {
		expect(byId[id], `menu ${id}`).toBeTruthy();
		expect(byId[id].name).toBe(name);
	}
	const seen = new Map();
	for (const m of menus) {
		for (const t of m.tools) {
			expect(seen.get(t.id), `${t.id} is in two menus`).toBeUndefined();
			seen.set(t.id, m.name);
		}
	}
	const sizes = Object.fromEntries(menus.map((m) => [m.name, m.tools.length]));
	results.menuSizes = sizes;
	for (const m of menus) {
		if (!NOT_ELEMENT_MENUS.has(m.id)) {
			expect(m.tools.length, `${m.name} has ${m.tools.length} entries`).toBeLessThanOrEqual(40);
		}
	}
	// a few elements in their new homes
	expect(seen.get('DEFAULT_PT_ANT')).toBe('Life and Nature');
	expect(seen.get('DEFAULT_PT_STAR')).toBe('Space and Gravity');
	expect(seen.get('DEFAULT_PT_TANK')).toBe('Characters and Vehicles');
	expect(seen.get('DEFAULT_PT_VOLT')).toBe('Circuits (voltage simulation)');
	expect(seen.get('DEFAULT_PT_GOLD')).toBe('Metals');
	expect(seen.get('DEFAULT_PT_METL')).toBe('Electronics');
	// raw section numbers used by Lua scripts keep their meaning
	expect(await h.luaFile(page, 'return elem.SC_SPECIAL .. "," .. elem.SC_DECO .. "," .. elem.SC_ORGANIC .. "," .. elem.SC_METALS')).toBe('11,15,18,22');
	await h.expectNoUnexplainedErrors(log);
});

test('side bar: the second column switches to the new menus, the classic column is where it was', async ({ page }) => {
	const log = h.collect(page);
	await h.openGame(page);
	// classic column at x = 613..627; Walls is the topmost classic menu, at y = 136
	const menuAt = async (x, y) => {
		const p = await h.gamePoint(page, x, y);
		await page.mouse.move(p.x, p.y);
		await h.waitFrames(page, 6);
		return Number(await h.lua(page, 'tpt.active_menu()'));
	};
	expect(await menuAt(620, 143)).toBe(0); // Walls
	expect(await menuAt(620, 319)).toBe(11); // Special (Decoration tools below it needs a click)
	expect(await menuAt(637, 143)).toBe(18); // Life and Nature, top of the second column
	expect(await menuAt(637, 207)).toBe(22); // Metals
	expect(await menuAt(620, 175)).toBe(2); // Powered Materials
	await h.saveScreenshot(page, 'categories-sidebar');
	await h.expectNoUnexplainedErrors(log);
});

test('element browser: search, select for both mouse buttons, keys stay out of the game', async ({ page }) => {
	const log = h.collect(page);
	await h.openGame(page);
	const panel = page.locator('#elements');
	if (!(await panel.isVisible())) {
		await page.click('#btn-elements');
	}
	await expect(panel).toBeVisible();
	const listed = await elementList(page);
	const total = listed.reduce((n, m) => n + m.tools.length, 0);
	expect(await page.locator('#element-groups .chip').count()).toBe(total);

	// typing in the search box must not reach the game (Space would toggle pause)
	const paused0 = await h.lua(page, 'tpt.set_pause()');
	await page.click('#element-search');
	await page.keyboard.type('foam bubble', { delay: 20 });
	await page.keyboard.press('Control+a');
	await page.keyboard.type('foam');
	await page.locator('#canvas').focus(); // the console helper types into the game
	expect(await h.lua(page, 'tpt.set_pause()')).toBe(paused0);
	const dfom = page.locator('#element-groups .chip[data-id="DEFAULT_PT_DFOM"]');
	await expect(dfom).toBeVisible();

	await dfom.click();
	await expect.poll(async () => (await activeTools(page))[0]).toBe('DEFAULT_PT_DFOM');
	expect(await h.lua(page, 'tpt.selectedl')).toBe('DEFAULT_PT_DFOM');
	expect(Number(await h.lua(page, 'tpt.active_menu()'))).toBe(6); // Gases, DFOM's menu
	await expect(dfom).toHaveClass(/sel-left/);

	await page.fill('#element-search', 'gold');
	const gold = page.locator('#element-groups .chip[data-id="DEFAULT_PT_GOLD"]');
	await gold.click({ button: 'right' });
	await expect.poll(async () => (await activeTools(page))[1]).toBe('DEFAULT_PT_GOLD');
	expect(await h.lua(page, 'tpt.selectedr')).toBe('DEFAULT_PT_GOLD');

	// Enter picks the first match
	await page.fill('#element-search', 'tank');
	await page.press('#element-search', 'Enter');
	await expect.poll(async () => (await activeTools(page))[0]).toBe('DEFAULT_PT_TANK');
	await page.locator('#canvas').focus();

	// the selected element really draws
	await page.fill('#element-search', 'sand');
	await page.locator('#element-groups .chip[data-id="DEFAULT_PT_SAND"]').click();
	await expect.poll(async () => (await activeTools(page))[0]).toBe('DEFAULT_PT_SAND');
	await h.lua(page, 'tpt.set_pause(1) or sim.clearSim() or 0');
	await h.click(page, 300, 100);
	expect(Number(await h.lua(page, 'sim.partID(300,100) and sim.partProperty(sim.partID(300,100),"type") or -1'))).toBe(Number(await h.lua(page, 'elem.DEFAULT_PT_SAND')));

	// elements made by Lua scripts show up when the list is refreshed
	await h.luaFile(page, `local id = elem.allocate("PORTTEST", "ZZTST")
		elem.property(id, "Colour", 0xFF00FF)
		elem.property(id, "MenuSection", elem.SC_SPECIAL)
		elem.property(id, "MenuVisible", 1)
		return id`);
	await page.evaluate(() => window.tptPage.elementPanel.refresh());
	await page.fill('#element-search', 'zztst');
	await expect(page.locator('#element-groups .chip[data-id="PORTTEST_PT_ZZTST"]')).toBeVisible();

	await page.fill('#element-search', 'no such element anywhere');
	await expect(page.locator('#element-groups .no-match')).toBeVisible();
	await page.fill('#element-search', '');
	results.panel = { chips: total };
	await h.saveScreenshot(page, 'categories-panel');

	// the panel can be hidden again, and the choice is remembered
	await page.click('#btn-elements');
	await expect(panel).toBeHidden();
	await page.reload();
	await page.waitForFunction(() => window.tptPage && window.tptPage.state === 'running', null, { timeout: 60_000 });
	await expect(panel).toBeHidden();
	await h.expectNoUnexplainedErrors(log);
});

test('phone-width layout: no horizontal scrolling, panel below the game', async ({ page }) => {
	await page.setViewportSize({ width: 375, height: 760 });
	const log = h.collect(page);
	await h.openGame(page);
	if (!(await page.locator('#elements').isVisible())) {
		await page.click('#btn-elements');
	}
	const layout = await page.evaluate(() => ({
		scrollWidth: document.documentElement.scrollWidth,
		clientWidth: document.documentElement.clientWidth,
		canvas: document.getElementById('canvas').getBoundingClientRect().toJSON(),
		panel: document.getElementById('elements').getBoundingClientRect().toJSON(),
	}));
	results.phone = layout;
	expect(layout.scrollWidth).toBeLessThanOrEqual(layout.clientWidth);
	expect(layout.panel.top).toBeGreaterThanOrEqual(layout.canvas.bottom - 1);
	expect(layout.canvas.width).toBeGreaterThan(300);
	await h.saveScreenshot(page, 'categories-phone');
	await h.expectNoUnexplainedErrors(log);
});
