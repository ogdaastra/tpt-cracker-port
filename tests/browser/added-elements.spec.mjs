// Tests for the elements added on top of Cracker1000's Mod v55.1: TPT Ultimata's elements, walls and
// engine (circuits, time dilation, ...), the newer Cracker elements and the Jacob1's Mod elements.
// Everything runs in the real game through its Lua API (typed into the in-game console).
import { test, expect } from '@playwright/test';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import * as h from './helpers.mjs';

const MOD_ELEMENT_OFFSET = 192;
const results = {};
test.afterAll(() => h.writeArtifact('added-elements.json', results));

// The added elements are the entries after RUBR (MOD_ELEMENT_OFFSET + 50) in the element list.
function addedElements() {
	const meson = fs.readFileSync(path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../../src/simulation/elements/meson.build'), 'utf8');
	const list = [];
	for (const m of meson.matchAll(/\[\s*'([A-Z0-9]+)',\s*MOD_ELEMENT_OFFSET \+ (\d+)\s*\]/g)) {
		if (Number(m[2]) > 50) list.push({ file: m[1], id: MOD_ELEMENT_OFFSET + Number(m[2]) });
	}
	return list;
}

async function startPaused(page) {
	await h.openGame(page);
	await page.keyboard.press('Space'); // pause
	await h.lua(page, '0'); // console helpers
	// sim.NUM_PARTS and sim.elementCount are only refreshed while the simulation runs; count directly
	await h.luaFile(page, `
		event.register(event.tick, function() TK = (TK or 0) + 1 end)
		function CNT(t) local n = 0 for i in sim.parts() do if not t or sim.partProperty(i, "type") == t then n = n + 1 end end return n end
		return 0`);
}

const SHOWN_SECTIONS = new Set([...Array(16).keys(), 18, 19, 20, 21, 22]);

test('every added element exists, is in a menu, and runs without breaking the simulation', async ({ page }) => {
	test.setTimeout(240_000);
	const log = h.collect(page);
	const added = addedElements();
	expect(added.length, 'elements registered after RUBR').toBeGreaterThanOrEqual(140);
	await startPaused(page);

	const info = JSON.parse(await h.luaFile(page, `
		local out = {}
		for id = ${added[0].id}, ${added[added.length - 1].id} do
			out[#out + 1] = string.format('{"id":%d,"name":"%s","enabled":%d,"visible":%d,"section":%d}', id,
				elem.property(id, "Name"), elem.property(id, "Enabled"), elem.property(id, "MenuVisible"), elem.property(id, "MenuSection"))
		end
		return "[" .. table.concat(out, ",") .. "]"`));
	expect(info.length).toBe(added.length);
	const names = info.map((e) => e.name);
	expect(new Set(names).size, 'element names are unique').toBe(names.length);
	for (const e of info) {
		expect.soft(e.enabled, `${e.name} enabled`).toBe(1);
		// sections 0-15 and the port's 18-22 are shown; 16 and 17 are Cracker's hidden menus
		if (e.visible) expect.soft(SHOWN_SECTIONS.has(e.section), `${e.name} is in a shown menu (section ${e.section})`).toBe(true);
	}
	// names from the element list the user asked for
	for (const n of ['DFOM', 'DIGS', 'ALUM', 'ALMP', 'RSST', 'RSSS', 'CYTK', 'TANK', 'GNSH', 'VOLT', 'NOTE', 'STAR', 'TIME', 'BCTR', 'ROCK2', 'SLCN2', 'EXPL', 'PWHT', 'BUTN', 'RAZR'])
		expect.soft(names, `${n} present`).toContain(n);

	// a small block of every added element, spread over the screen, then let it all run
	const placed = JSON.parse(await h.luaFile(page, `
		sim.clearSim()
		local out = {}
		local n = 0
		for id = ${added[0].id}, ${added[added.length - 1].id} do
			local cx, cy = 20 + (n % 16) * 36, 30 + math.floor(n / 16) * 36
			local made = 0
			for dx = 0, 2 do for dy = 0, 2 do
				if sim.partCreate(-1, cx + dx, cy + dy, id) >= 0 then made = made + 1 end
			end end
			out[#out + 1] = string.format('"%s":%d', elem.property(id, "Name"), made)
			n = n + 1
		end
		return "{" .. table.concat(out, ",") .. "}"`));
	const notPlaced = Object.entries(placed).filter(([, n]) => n === 0).map(([k]) => k);
	results.placement = { elements: Object.keys(placed).length, notPlaceable: notPlaced, particles: Number(await h.lua(page, 'CNT()')) };
	// LQUD, BRKN and the vehicles have creation rules (need a ctype, or one per player); everything else must appear
	expect(notPlaced.length, `elements that could not be created: ${notPlaced.join(' ')}`).toBeLessThanOrEqual(8);
	await h.saveScreenshot(page, '20-added-elements-placed');

	await page.keyboard.press('Space'); // run
	const t0 = Number(await h.lua(page, 'TK'));
	await page.waitForTimeout(8000);
	const t1 = Number(await h.lua(page, 'TK'));
	await h.saveScreenshot(page, '20-added-elements-after-8s');
	expect(await page.evaluate(() => window.tptPage.state)).toBe('running');
	results.placement.ticksIn8s = t1 - t0;
	expect(t1 - t0, 'simulation kept ticking with every added element present').toBeGreaterThan(150);
	await h.expectNoUnexplainedErrors(log);
});

test('walls from TPT Ultimata: I-WALL keeps particles alive, the walls are in the wall menu', async ({ page }) => {
	const log = h.collect(page);
	await startPaused(page);
	const out = await h.luaFile(page, `
		sim.clearSim()
		-- I-WALL is wall 20 in this build (19 is Cracker's ZHOLE)
		tpt.set_wallmap(50, 50, 4, 4, 20)
		local inside = sim.partCreate(-1, 205, 205, elem.DEFAULT_PT_DUST)
		local outside = sim.partCreate(-1, 300, 205, elem.DEFAULT_PT_DUST)
		sim.partKill(inside)
		sim.partKill(outside)
		return tostring(sim.partProperty(inside, "type") == elem.DEFAULT_PT_DUST) .. " " .. tostring(sim.partProperty(outside, "type") == elem.DEFAULT_PT_DUST)`);
	expect(out, 'particle inside I-WALL survives kill_part, the one outside does not').toBe('true false');
	await h.expectNoUnexplainedErrors(log);
});

test('reactions of new elements: RSST solidifies under PHOT, EXPL blows up WOOD, BUTN sparks when clicked', async ({ page }) => {
	test.setTimeout(120_000);
	const log = h.collect(page);
	await startPaused(page);

	// RSST hit by photons turns into RSSS
	await h.luaFile(page, `
		sim.clearSim()
		for x = 100, 140 do for y = 300, 310 do sim.partCreate(-1, x, y, elem.DEFAULT_PT_RSST) end end
		for x = 100, 140 do for y = 300, 310 do sim.partCreate(-3, x, y, elem.DEFAULT_PT_PHOT) end end
		return 0`);
	await h.luaFile(page, 'sim.framerender(20) return 0');
	await h.waitFrames(page, 40);
	const rsss = Number(await h.lua(page, 'CNT(elem.DEFAULT_PT_RSSS)'));
	expect(rsss, 'RSSS made from RSST + PHOT').toBeGreaterThan(50);

	// EXPL makes what it touches explode
	await h.luaFile(page, `
		sim.clearSim()
		for x = 200, 260 do for y = 200, 240 do sim.partCreate(-1, x, y, elem.DEFAULT_PT_WOOD) end end
		for x = 225, 235 do sim.partCreate(-1, x, 199, elem.DEFAULT_PT_EXPL) end
		return 0`);
	const wood0 = Number(await h.lua(page, 'CNT(elem.DEFAULT_PT_WOOD)'));
	await page.keyboard.press('Space'); // run
	await page.waitForTimeout(4000);
	await page.keyboard.press('Space');
	const wood1 = Number(await h.lua(page, 'CNT(elem.DEFAULT_PT_WOOD)'));
	await h.saveScreenshot(page, '21-expl-wood');
	expect(wood1, `WOOD left after EXPL touched it (${wood0} before)`).toBeLessThan(wood0 * 0.9);

	// BUTN: powered with PSCN, then drawing on it sparks the METL next to it
	await h.luaFile(page, `
		sim.clearSim()
		for x = 300, 309 do sim.partCreate(-1, x, 200, elem.DEFAULT_PT_BUTN) end
		for x = 300, 309 do sim.partCreate(-1, x, 201, elem.DEFAULT_PT_METL) end
		for i in sim.parts() do if sim.partProperty(i, "type") == elem.DEFAULT_PT_BUTN then sim.partProperty(i, "life", 10) end end
		return 0`);
	const butn = await h.gamePoint(page, 304, 200);
	await page.mouse.move(butn.x, butn.y);
	await h.waitFrames(page, 3);
	await page.mouse.down(); // draws the selected element (DUST) on the BUTN
	await h.waitFrames(page, 3);
	await page.mouse.up();
	await h.waitFrames(page, 3);
	const sparked = Number(await h.lua(page, 'CNT(elem.DEFAULT_PT_SPRK)'));
	expect(sparked, 'clicking a powered BUTN sparks the METL under it').toBeGreaterThan(0);
	await h.expectNoUnexplainedErrors(log);
});

test('undo/redo and stamps keep the added elements and Ultimata state', async ({ page }) => {
	test.setTimeout(120_000);
	const log = h.collect(page);
	const added = addedElements();
	await startPaused(page);

	// a stamp with one of each added element, reloaded into an empty simulation: the same types come back
	const counts = (await h.luaFile(page, `
		sim.clearSim()
		local n = 0
		for id = ${added[0].id}, ${added[added.length - 1].id} do
			sim.partCreate(-1, 20 + (n % 30) * 19, 40 + math.floor(n / 30) * 19, id)
			n = n + 1
		end
		local before = CNT()
		local name = sim.saveStamp(0, 0, 611, 383)
		sim.clearSim()
		sim.loadStamp(name, 0, 0)
		local types = {}
		for i in sim.parts() do types[sim.partProperty(i, "type")] = true end
		local distinct = 0
		for _ in pairs(types) do distinct = distinct + 1 end
		return before .. " " .. CNT() .. " " .. distinct`)).split(' ').map(Number);
	results.stamp = { before: counts[0], after: counts[1], distinctTypes: counts[2] };
	expect(counts[1], 'particles after loading the stamp').toBe(counts[0]);
	expect(counts[2], 'distinct element types after loading the stamp').toBeGreaterThan(added.length - 10);

	// undo history (SnapshotDelta) with the Ultimata state: time dilation map, one-way wall, VOLT
	const undo = await h.luaFile(page, `
		sim.clearSim()
		for x = 100, 149 do sim.partCreate(-1, x, 100, elem.DEFAULT_PT_VOLT) end
		tpt.set_wallmap(10, 10, 2, 2, 22)
		sim.takeSnapshot()
		for x = 100, 149 do sim.partCreate(-1, x, 150, elem.DEFAULT_PT_DUST) end
		local a = CNT()
		local r1 = sim.historyRestore()
		local b = CNT()
		local r2 = sim.historyForward()
		local c = CNT()
		return a .. " " .. b .. " " .. c .. " " .. tostring(r1) .. " " .. tostring(r2) .. " " .. tpt.get_wallmap(10, 10)`);
	results.undo = undo;
	const [a, b, c] = undo.split(' ').map(Number);
	expect(a).toBe(100);
	expect(b, 'undo removed the DUST').toBe(50);
	expect(c, 'redo brought it back').toBe(100);
	expect(undo.endsWith(' 22'), 'one-way wall restored').toBe(true);
	await h.expectNoUnexplainedErrors(log);
});

test('TPT Ultimata circuits: a VOLT source drives current (RSPK) round a METL loop', async ({ page }) => {
	test.setTimeout(120_000);
	const log = h.collect(page);
	await startPaused(page);
	// + PSCN | VOLT x10 | NSCN -, closed by a METL loop below
	await h.luaFile(page, `
		sim.clearSim()
		local y = 200
		sim.partCreate(-1, 300, y, elem.DEFAULT_PT_PSCN)
		for x = 301, 310 do local i = sim.partCreate(-1, x, y, elem.DEFAULT_PT_VOLT) sim.partProperty(i, "tmp3", 10) end
		sim.partCreate(-1, 311, y, elem.DEFAULT_PT_NSCN)
		for yy = y, y + 20 do sim.partCreate(-1, 299, yy, elem.DEFAULT_PT_METL) sim.partCreate(-1, 312, yy, elem.DEFAULT_PT_METL) end
		for x = 299, 312 do sim.partCreate(-1, x, y + 21, elem.DEFAULT_PT_METL) end
		return 0`);
	await page.keyboard.press('Space');
	await page.waitForTimeout(3000);
	await page.keyboard.press('Space');
	const rspk = Number(await h.lua(page, 'CNT(elem.DEFAULT_PT_RSPK)'));
	await h.saveScreenshot(page, '22-circuit');
	results.circuit = { rspk };
	expect(rspk, 'RSPK (current) on the circuit').toBeGreaterThan(10);
	expect(await page.evaluate(() => window.tptPage.state)).toBe('running');
	await h.expectNoUnexplainedErrors(log);
});

test('a large block of VOLT that is not a circuit does not freeze the game', async ({ page }) => {
	test.setTimeout(120_000);
	const log = h.collect(page);
	await startPaused(page);
	await h.luaFile(page, `
		sim.clearSim()
		for x = 150, 260 do for y = 150, 200 do sim.partCreate(-1, x, y, elem.DEFAULT_PT_VOLT) end end
		for x = 100, 140 do for y = 150, 200 do sim.partCreate(-1, x, y, elem.DEFAULT_PT_METL) end end
		return 0`);
	await page.keyboard.press('Space');
	const t0 = Number(await h.lua(page, 'TK'));
	await page.waitForTimeout(4000);
	const t1 = Number(await h.lua(page, 'TK'));
	await page.keyboard.press('Space');
	results.voltBlock = { ticksIn4s: t1 - t0 };
	expect(t1 - t0, 'frames simulated in 4 s with 5600 VOLT').toBeGreaterThan(60);
	await h.expectNoUnexplainedErrors(log);
});

test('portals still carry particles (portal buffers are now allocated per channel)', async ({ page }) => {
	test.setTimeout(120_000);
	const log = h.collect(page);
	await startPaused(page);
	// A walled cup under PRTO catches the DUST that comes out; without it the DUST falls off the bottom of the
	// screen and the count depends on how many frames ran. A fixed number of frames keeps CPU load out of it.
	await h.luaFile(page, `
		sim.clearSim()
		for x = 100, 110 do sim.partCreate(-1, x, 300, elem.DEFAULT_PT_PRTI) end
		for x = 400, 410 do sim.partCreate(-1, x, 100, elem.DEFAULT_PT_PRTO) end
		for x = 100, 110 do for y = 250, 290 do sim.partCreate(-1, x, y, elem.DEFAULT_PT_DUST) end end
		sim.createWallBox(360, 200, 450, 203, 8)
		sim.createWallBox(360, 120, 363, 203, 8)
		sim.createWallBox(447, 120, 450, 203, 8)
		return 0`);
	await page.evaluate((n) => window.tptPage.module.ccall('tpt_benchmark', 'number', ['number', 'number'], [n, 0]), 240);
	const moved = Number(await h.luaFile(page, `
		local n = 0
		for i in sim.parts() do
			if sim.partProperty(i, "type") == elem.DEFAULT_PT_DUST and sim.partProperty(i, "x") > 300 then n = n + 1 end
		end
		return n`));
	results.portals = { dustThroughPortal: moved };
	expect(moved, 'DUST that came out of PRTO (of 451)').toBeGreaterThan(150); // 235-258 in 6 runs; a broken portal gives about 0
	await h.expectNoUnexplainedErrors(log);
});
