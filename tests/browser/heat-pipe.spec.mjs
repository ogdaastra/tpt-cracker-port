// Heat pipes (ported from The Powder Toy 100): a PIPE that forms with HEAC touching it conducts heat
// and mixes the temperature of what it carries; an ordinary PIPE stays a heat insulator.
import { test, expect } from '@playwright/test';
import * as h from './helpers.mjs';

const results = {};
test.afterAll(() => h.writeArtifact('heat-pipe.json', results));

// Two 41-pixel pipes, the upper one with HEAC in place of one of its border bricks. Erasing the bricks at
// the left ends starts the pattern generation (the in-game recipe: draw PIPE, wait for BRCK, open an end).
const BUILD = `
	sim.clearSim()
	for x = 100, 140 do
		sim.partCreate(-1, x, 100, elem.DEFAULT_PT_PIPE)
		sim.partCreate(-1, x, 200, elem.DEFAULT_PT_PIPE)
	end
	return 0`;
const OPEN = `
	local function kill(x, y) local id = sim.partID(x, y) if id then sim.partKill(id) end end
	for _, py in ipairs({100, 200}) do
		for y = py - 2, py + 2 do for x = 98, 99 do kill(x, y) end end
	end
	kill(120, 99)
	return sim.partCreate(-1, 120, 99, elem.DEFAULT_PT_HEAC)`;
// pipe pixels of one row: how many, how many with the heat pipe flag, mean temperature of the last 10
const ROW = (y) => `
	local n, flagged, t, tn = 0, 0, 0, 0
	for x = 100, 140 do
		local id = sim.partID(x, ${y})
		if id and sim.partProperty(id, "type") == elem.DEFAULT_PT_PIPE then
			n = n + 1
			if sim.partProperty(id, "tmp") % 2 == 1 then flagged = flagged + 1 end
			if x >= 131 then t = t + sim.partProperty(id, "temp") tn = tn + 1 end
		end
	end
	return n .. "," .. flagged .. "," .. (tn > 0 and t / tn or -1)`;

async function row(page, y) {
	const [n, flagged, farTemp] = (await h.luaFile(page, ROW(y))).split(',').map(Number);
	return { n, flagged, farTemp };
}

test('PIPE formed next to HEAC becomes a heat pipe; ordinary PIPE stays an insulator; survives a stamp', async ({ page }) => {
	test.setTimeout(240_000);
	const log = h.collect(page);
	await h.openGame(page);
	await h.lua(page, 'tpt.set_pause(0) or 0');
	await h.luaFile(page, BUILD);
	await h.waitFrames(page, 90); // BRCK border appears at life 10
	expect(Number(await h.luaFile(page, OPEN))).toBeGreaterThanOrEqual(0);
	await h.waitFrames(page, 360); // pattern generation runs along the pipe

	const heat = await row(page, 100);
	const plain = await row(page, 200);
	results.afterGeneration = { heat, plain };
	expect(heat.n).toBe(41);
	expect(plain.n).toBe(41);
	expect(heat.flagged, 'every pixel of the HEAC pipe is a heat pipe').toBe(41);
	expect(plain.flagged, 'the ordinary pipe has no heat pipe pixels').toBe(0);

	// heat the left end of both pipes for 3 s; only the heat pipe carries it along
	await h.luaFile(page, `
		HOT = event.register(event.tick, function()
			for _, py in ipairs({100, 200}) do
				for x = 100, 104 do
					local id = sim.partID(x, py)
					if id then sim.partProperty(id, "temp", 1273.15) end
				end
			end
		end)
		return 0`);
	await h.waitFrames(page, 180);
	await h.luaFile(page, 'event.unregister(event.tick, HOT) return 0');
	const heatHot = await row(page, 100);
	const plainHot = await row(page, 200);
	results.afterHeating = { heat: heatHot, plain: plainHot };
	expect(heatHot.farTemp, 'far end of the heat pipe warmed up').toBeGreaterThan(heat.farTemp + 100);
	expect(Math.abs(plainHot.farTemp - plain.farTemp), 'far end of the ordinary pipe kept its temperature').toBeLessThan(0.01);

	// a stamp round trip keeps the flag on the heat pipe and off the ordinary one
	await h.lua(page, 'tpt.set_pause(1) or 0');
	await h.luaFile(page, `
		local name = sim.saveStamp(0, 0, 611, 383)
		sim.clearSim()
		sim.loadStamp(name, 0, 0)
		return name`);
	const heatLoaded = await row(page, 100);
	const plainLoaded = await row(page, 200);
	results.afterStamp = { heat: heatLoaded, plain: plainLoaded };
	expect(heatLoaded.flagged).toBe(41);
	expect(plainLoaded.flagged).toBe(0);
	await h.saveScreenshot(page, '23-heat-pipe');
	await h.expectNoUnexplainedErrors(log);
});
