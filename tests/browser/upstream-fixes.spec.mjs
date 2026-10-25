// Fixes ported from newer official TPT (section 4 of docs/NEXT-PLANS.md): each test builds the situation that
// used to hang, crash, corrupt the particle map or drift, and checks that the game now handles it.
import { test, expect } from '@playwright/test';
import * as h from './helpers.mjs';

const results = {};
test.afterAll(() => h.writeArtifact('upstream-fixes.json', results));

// Steps the simulation without drawing (tpt_benchmark runs whole frames, paused or not).
async function step(page, frames) {
	return page.evaluate((n) => window.tptPage.module.ccall('tpt_benchmark', 'number', ['number', 'number'], [n, 0]), frames);
}

// Every pmap/photons entry in the area points at a live particle at that spot (a stale entry is the corruption).
const PMAP_CHECK = `
	local bad = 0
	for y = 80, 320 do for x = 80, 420 do
		for _, id in ipairs({ sim.partID(x, y) or -1, sim.photons(x, y) or -1 }) do
			if id >= 0 then
				local px, py = sim.partPosition(id)
				if sim.partProperty(id, "type") == 0 or math.floor(px + 0.5) ~= x or math.floor(py + 0.5) ~= y then bad = bad + 1 end
			end
		end
	end end
	return bad`;

test('NaN velocity and sub-zero MERC no longer hang or crash the game', async ({ page }) => {
	const log = h.collect(page);
	await h.openGame(page);
	// upstream 51508113: a NaN velocity made the movement loop run forever
	await h.luaFile(page, `
		sim.clearSim()
		local i = sim.partCreate(-1, 200, 200, elem.DEFAULT_PT_DUST)
		sim.partProperty(i, "vx", 0/0)
		sim.partProperty(i, "vy", 0/0)
		return i`);
	const ms = await step(page, 60);
	results.nanFrames60ms = ms;
	expect(ms, '60 frames with a NaN velocity').toBeLessThan(5000);
	// upstream d6f66f5c: MERC at -2 K < T <= -1 K divided by zero (int(T) + 1 == 0; a WebAssembly trap)
	await h.luaFile(page, `
		sim.clearSim()
		sim.createWallBox(80, 204, 160, 211, 8) -- a floor
		for x = 100, 140 do sim.partCreate(-1, x, 200, elem.DEFAULT_PT_MERC) end
		for i in sim.parts() do sim.partProperty(i, "temp", -1.5) end
		return 0`);
	await step(page, 120);
	expect(Number(await h.luaFile(page, 'local n = 0 for i in sim.parts() do n = n + 1 end return n'))).toBeGreaterThan(0);
	expect(await page.evaluate(() => window.tptPage.state)).toBe('running');
	await h.expectNoUnexplainedErrors(log);
});

test('a neutron entering a portal through DMND leaves the particle maps consistent', async ({ page }) => {
	const log = h.collect(page);
	await h.openGame(page);
	// the reproduction from upstream 82ec6b27: the portal only cleared .type, leaving a stale photons entry
	await h.luaFile(page, `
		sim.clearSim()
		for k = 0, 30 do
			local i = sim.partCreate(-1, 100 + k * 10, 100, elem.DEFAULT_PT_NEUT)
			sim.partProperty(i, "vx", 0)
			sim.partProperty(i, "vy", 2)
			sim.partCreate(-1, 100 + k * 10, 100, elem.DEFAULT_PT_DMND)
			sim.partCreate(-1, 100 + k * 10, 102, elem.DEFAULT_PT_PRTI)
		end
		sim.partCreate(-1, 300, 300, elem.DEFAULT_PT_PRTO)
		return 0`);
	for (let k = 0; k < 10; k++) {
		await step(page, 6);
		const bad = Number(await h.luaFile(page, PMAP_CHECK));
		expect(bad, `inconsistent particles after ${(k + 1) * 6} frames`).toBe(0);
	}
	await h.expectNoUnexplainedErrors(log);
});

test('gas spreads evenly instead of drifting down and right', async ({ page }) => {
	const log = h.collect(page);
	await h.openGame(page);
	// upstream d21a2293: a 0.25 px bias in the gas movement pushed every cloud towards the lower right
	const r = JSON.parse(await h.luaFile(page, `
		sim.clearSim()
		tpt.set_pause(1)
		-- a closed wall box centred on the cloud, so nothing leaves and the walls pull no side
		for x = 196, 415, 4 do sim.createWallBox(x, 96, x, 99, 8) sim.createWallBox(x, 284, x, 287, 8) end
		for y = 96, 287, 4 do sim.createWallBox(196, y, 199, y, 8) sim.createWallBox(412, y, 415, y, 8) end
		for x = 276, 335 do for y = 162, 221 do sim.partCreate(-1, x, y, elem.DEFAULT_PT_GAS) end end
		local function centre()
			local sx, sy, n = 0, 0, 0
			for i in sim.parts() do local x, y = sim.partPosition(i) sx, sy, n = sx + x, sy + y, n + 1 end
			return sx / n, sy / n, n
		end
		local x0, y0 = centre()
		_G.gasStart = { x0, y0 }
		return string.format('{"x0":%.2f,"y0":%.2f}', x0, y0)`));
	await step(page, 600);
	const end = JSON.parse(await h.luaFile(page, `
		local sx, sy, n = 0, 0, 0
		for i in sim.parts() do local x, y = sim.partPosition(i) sx, sy, n = sx + x, sy + y, n + 1 end
		return string.format('{"x":%.2f,"y":%.2f,"n":%d}', sx / n, sy / n, n)`));
	results.gas = { start: r, end };
	expect(end.n).toBeGreaterThan(3000);
	expect(Math.abs(end.x - r.x0), 'horizontal drift of the gas cloud (px)').toBeLessThan(3);
	expect(Math.abs(end.y - r.y0), 'vertical drift of the gas cloud (px)').toBeLessThan(3);
	await h.expectNoUnexplainedErrors(log);
});

// ---- batch B: physics

test('heat capacity: a block with three times the capacity pulls the shared temperature towards its own', async ({ page }) => {
	const log = h.collect(page);
	await h.openGame(page);
	expect(await h.luaFile(page, 'return elem.property(elem.DEFAULT_PT_BASE, "HeatCapacity")')).toBe('1.5');
	// a hot METL block touching a cold TTAN block, nothing else; returns the two average temperatures
	const run = (ttanCapacity) => h.luaFile(page, `
		sim.clearSim()
		elem.property(elem.DEFAULT_PT_TTAN, "HeatCapacity", ${ttanCapacity})
		for x = 280, 299 do for y = 180, 219 do sim.partCreate(-1, x, y, elem.DEFAULT_PT_METL) end end
		for x = 300, 319 do for y = 180, 219 do sim.partCreate(-1, x, y, elem.DEFAULT_PT_TTAN) end end
		for i in sim.parts() do
			sim.partProperty(i, "temp", sim.partProperty(i, "type") == elem.DEFAULT_PT_METL and 1000 or 300)
		end
		return 0`);
	const measure = async () => JSON.parse(await h.luaFile(page, `
		local s = { [elem.DEFAULT_PT_METL] = 0, [elem.DEFAULT_PT_TTAN] = 0 }
		for i in sim.parts() do local t = sim.partProperty(i, "type") s[t] = s[t] + sim.partProperty(i, "temp") end
		return string.format('{"metl":%.1f,"ttan":%.1f}', s[elem.DEFAULT_PT_METL] / 800, s[elem.DEFAULT_PT_TTAN] / 800)`));
	await run(1);
	await step(page, 3000);
	const equal = await measure();
	await run(3);
	await step(page, 3000);
	const heavy = await measure();
	await h.luaFile(page, 'elem.property(elem.DEFAULT_PT_TTAN, "HeatCapacity", 1) return 0');
	results.heatCapacity = { equal, ttanCapacity3: heavy };
	// equal capacities meet near 650 K; with TTAN 3x, energy balance gives (1000 + 3 * 300) / 4 = 475 K
	expect(Math.abs(equal.metl - equal.ttan), 'equal capacities reach one temperature').toBeLessThan(25);
	expect(Math.abs((equal.metl + equal.ttan) / 2 - 650)).toBeLessThan(15);
	expect(Math.abs(heavy.metl - heavy.ttan)).toBeLessThan(25);
	expect(Math.abs((heavy.metl + 3 * heavy.ttan) / 4 - 475), 'heat conserved with capacities').toBeLessThan(15);
	expect((heavy.metl + heavy.ttan) / 2).toBeLessThan(520);
	await h.expectNoUnexplainedErrors(log);
});

test('vorticity confinement keeps air swirling, and the setting is saved with the simulation', async ({ page }) => {
	const log = h.collect(page);
	await h.openGame(page);
	// the same air disturbance past a wall block, with confinement off and on; total |curl| afterwards
	const swirl = async (coeff) => {
		await h.luaFile(page, `
			sim.clearSim()
			sim.vorticityCoeff(${coeff})
			sim.createWallBox(296, 176, 315, 207, 8)
			for cx = 60, 70 do for cy = 44, 52 do sim.velocityX(cx, cy, 20) end end
			return 0`);
		await step(page, 90);
		return Number(await h.luaFile(page, `
			local s = 0
			for cy = 2, 93 do for cx = 2, 150 do
				local w = (sim.velocityY(cx + 1, cy) - sim.velocityY(cx - 1, cy)) - (sim.velocityX(cx, cy + 1) - sim.velocityX(cx, cy - 1))
				s = s + math.abs(w) * 0.5
			end end
			return s`));
	};
	const off = await swirl(0);
	const on = await swirl(0.5);
	results.vorticity = { off, on };
	expect(on, 'more swirl with confinement on').toBeGreaterThan(off * 1.2);
	// saved with the simulation: a stamp of the whole screen, opened again as a save, brings the setting back
	const stamp = await h.luaFile(page, `
		sim.vorticityCoeff(0.3)
		return sim.saveStamp(0, 0, 611, 383)`);
	await h.luaFile(page, 'sim.vorticityCoeff(0) return 0');
	await page.evaluate(([dir, name]) => {
		const fs = window.tptPage.module.FS;
		fs.writeFile('/tmp/vorticity.cps', fs.readFile(`${dir}/stamps/${name}.stm`));
		window.tptPage.module.ccall('tpt_open_file', null, ['string'], ['/tmp/vorticity.cps']);
	}, [h.DATA_DIR, stamp]);
	await h.waitFrames(page, 30);
	const loaded = Number(await h.luaFile(page, 'return sim.vorticityCoeff()'));
	results.vorticity.afterReload = loaded;
	expect(loaded).toBeCloseTo(0.3, 5);
	await h.expectNoUnexplainedErrors(log);
});

// ---- batch C: reactions

async function countWhere(page, lua) {
	return Number(await h.luaFile(page, `local n = 0 for i in sim.parts() do if ${lua} then n = n + 1 end end return n`));
}

test('new reactions: LITH from molten SLCN + SALT, PTNM from molten gold in a vacuum, acid from gassy RIME, STOR holds a photon', async ({ page }) => {
	test.setTimeout(180_000);
	const log = h.collect(page);
	await h.openGame(page);
	// LAVA(SLCN) next to LAVA(SALT) -> LAVA(LITH) (upstream 2d7f5fa2)
	await h.luaFile(page, `
		sim.clearSim()
		sim.createWallBox(200, 300, 420, 307, 8)
		for x = 220, 400 do for y = 280, 299 do
			local i = sim.partCreate(-1, x, y, elem.DEFAULT_PT_LAVA)
			sim.partProperty(i, "ctype", (x + y) % 2 == 0 and elem.DEFAULT_PT_SLCN or elem.DEFAULT_PT_SALT)
			sim.partProperty(i, "temp", 2500)
		end end
		return 0`);
	await step(page, 300);
	const lith = await countWhere(page, 'sim.partProperty(i, "type") == elem.DEFAULT_PT_LAVA and sim.partProperty(i, "ctype") == elem.DEFAULT_PT_LITH');
	// LAVA(GOLD) at -256 pressure -> LAVA(PTNM)
	await h.luaFile(page, `
		sim.clearSim()
		-- a sealed wall box keeps the vacuum in
		sim.createWallBox(240, 140, 359, 143, 8) sim.createWallBox(240, 196, 359, 199, 8)
		sim.createWallBox(240, 140, 243, 199, 8) sim.createWallBox(356, 140, 359, 199, 8)
		for x = 250, 349 do for y = 150, 189 do
			local i = sim.partCreate(-1, x, y, elem.DEFAULT_PT_LAVA)
			sim.partProperty(i, "ctype", elem.DEFAULT_PT_GOLD)
			sim.partProperty(i, "temp", 2500)
			sim.partProperty(i, "vx", 0) sim.partProperty(i, "vy", 0)
		end end
		return 0`);
	for (let k = 0; k < 20; k++) {
		await h.luaFile(page, 'for cx = 61, 88 do for cy = 36, 48 do sim.pressure(cx, cy, -256) end end return 0');
		await step(page, 30);
	}
	const ptnm = await countWhere(page, 'sim.partProperty(i, "ctype") == elem.DEFAULT_PT_PTNM');
	// RIME surrounded by GAS soaks it up; melted, it turns into ACID (upstream 56ede6ca)
	await h.luaFile(page, `
		sim.clearSim()
		for x = 280, 319 do for y = 180, 199 do sim.partCreate(-1, x, y, elem.DEFAULT_PT_RIME) end end
		return 0`);
	// cold GAS (room-temperature gas would melt the RIME first)
	const gasRing = `
		for x = 278, 321 do
			sim.partProperty(sim.partCreate(-1, x, 178, elem.DEFAULT_PT_GAS), "temp", 200)
			sim.partProperty(sim.partCreate(-1, x, 201, elem.DEFAULT_PT_GAS), "temp", 200)
		end
		for y = 179, 200 do
			sim.partProperty(sim.partCreate(-1, 278, y, elem.DEFAULT_PT_GAS), "temp", 200)
			sim.partProperty(sim.partCreate(-1, 321, y, elem.DEFAULT_PT_GAS), "temp", 200)
		end
		return 0`;
	for (let k = 0; k < 12; k++) {
		await h.luaFile(page, gasRing);
		await step(page, 10);
	}
	const acidicRime = await countWhere(page, 'sim.partProperty(i, "type") == elem.DEFAULT_PT_RIME and sim.partProperty(i, "tmp") > 5');
	await h.luaFile(page, 'for i in sim.parts() do if sim.partProperty(i, "type") == elem.DEFAULT_PT_RIME then sim.partProperty(i, "temp", 300) end end return 0');
	await step(page, 10);
	const acid = await countWhere(page, 'sim.partProperty(i, "type") == elem.DEFAULT_PT_ACID');
	// STOR takes a photon (upstream 68a8b055)
	await h.luaFile(page, `
		sim.clearSim()
		sim.partCreate(-1, 300, 200, elem.DEFAULT_PT_STOR)
		local p = sim.partCreate(-1, 301, 200, elem.DEFAULT_PT_PHOT)
		sim.partProperty(p, "vx", 0) sim.partProperty(p, "vy", 0)
		return 0`);
	await step(page, 5);
	const stor = await h.luaFile(page, 'return sim.partProperty(sim.partID(300, 200), "tmp")');
	const photId = await h.luaFile(page, 'return elem.DEFAULT_PT_PHOT');
	results.reactions = { lith, ptnm, acidicRime, acid, storHolds: stor };
	expect(lith, 'LAVA(LITH) particles').toBeGreaterThan(5);
	expect(ptnm, 'LAVA(PTNM) particles').toBeGreaterThan(0);
	expect(acidicRime, 'RIME with more than 5 GAS soaked up').toBeGreaterThan(20);
	expect(acid, 'ACID from melted acidic RIME').toBeGreaterThan(20);
	expect(stor, 'STOR holds a PHOT').toBe(photId);
	await h.expectNoUnexplainedErrors(log);
});

// ---- batch D: tools

test('PROP values can be written in binary (0b...)', async ({ page }) => {
	const log = h.collect(page);
	await h.openGame(page);
	await h.lua(page, 'sim.createBox(290, 190, 310, 210, elem.DEFAULT_PT_DMND) or 0');
	// the multi-property tool PRP2 uses the same value syntax as PROP (upstream ccb671bd)
	await page.evaluate(() => window.tptPage.module.ccall('tpt_select_tool', null, ['string', 'number'], ['DEFAULT_UI_PROPERTY2', 0]));
	await h.waitFrames(page, 16);
	await h.click(page, 260, 215);
	await page.keyboard.type('life:0b1011');
	await page.keyboard.press('Enter');
	await page.keyboard.type('tmp:0b11111111');
	await page.keyboard.press('Control+Enter');
	await h.waitFrames(page, 10);
	await h.click(page, 300, 200);
	const props = await h.luaFile(page, 'local i = sim.partID(300, 200) return sim.partProperty(i, "life") .. "," .. sim.partProperty(i, "tmp")');
	results.binaryProp = props;
	expect(props).toBe('11,255');
	await h.expectNoUnexplainedErrors(log);
});
