// TurboRecalc (turbo mode) must give exactly what RecalcFreeParticles gives. Build the threaded build with
// turbo-verify.patch applied (git apply docs/handoff/tools/turbo-verify.patch), serve it, then
//   node turbo-verify.cjs http://127.0.0.1:8834/
// Every frame runs both on the same state and compares particles, maps, counts and the free list; prints, per
// scene, how many frames differed (must be 0). Undo the patch afterwards (git apply -R).
const { chromium } = require('/home/user/tpt-cracker-port/tests/browser/node_modules/playwright');
const SCENES = {
	fire: `for x = 20, 590 do for y = 200, 379 do sim.partCreate(-1, x, y, (x % 3 == 0) and elem.DEFAULT_PT_WOOD or elem.DEFAULT_PT_GAS) end end
		for x = 20, 590, 3 do sim.partCreate(-1, x, 199, elem.DEFAULT_PT_FIRE) end`,
	plasma_photons: `for x = 100, 500 do for y = 100, 300 do sim.partCreate(-1, x, y, (x + y) % 3 == 0 and elem.DEFAULT_PT_PLSM or elem.DEFAULT_PT_PHOT) end end`,
	filt_stack: `for x = 50, 550 do for y = 100, 250 do sim.partCreate(-1, x, y, elem.DEFAULT_PT_FILT) end end
		for x = 50, 550 do for y = 50, 99 do sim.partCreate(-1, x, y, elem.DEFAULT_PT_PHOT) sim.partCreate(-1, x, y, elem.DEFAULT_PT_FIRE) end end
		for x = 50, 550 do for y = 260, 300 do sim.partCreate(-1, x, y, elem.DEFAULT_PT_INVIS) end end
		for x = 50, 550 do for y = 230, 280 do sim.partCreate(-1, x, y, elem.DEFAULT_PT_SMKE) end end`,
	walls: `sim.createWallBox(100, 100, 300, 300, 14) sim.createWallBox(320, 100, 500, 300, 15)
		for x = 100, 500 do for y = 100, 300 do sim.partCreate(-1, x, y, (x % 2 == 0) and elem.DEFAULT_PT_FIRE or elem.DEFAULT_PT_PLSM) end end`,
	sparks: `for row = 0, 15 do local y = 20 + row * 20 for x = 20, 590 do sim.partCreate(-1, x, y, elem.DEFAULT_PT_METL) end
		for x = 20, 590, 40 do sim.partCreate(-1, x, y - 1, elem.DEFAULT_PT_BTRY) end end`,
	mixed: `for x = 20, 590 do for y = 300, 379 do sim.partCreate(-1, x, y, (x % 7 == 0) and elem.DEFAULT_PT_SAND or elem.DEFAULT_PT_WATR) end end
		for x = 20, 590 do for y = 200, 230 do sim.partCreate(-1, x, y, (x % 5 == 0) and elem.DEFAULT_PT_WOOD or elem.DEFAULT_PT_DUST) end end
		for x = 20, 590, 4 do sim.partCreate(-1, x, 195, elem.DEFAULT_PT_FIRE) end`,
	timefield: `for k, spec in ipairs({ { 150, 273.15 + 16 * 3 }, { 450, 273.15 - 16 * 3 } }) do local id = sim.partCreate(-1, spec[1], 190, elem.DEFAULT_PT_TIME)
		sim.partProperty(id, "tmp2", 1) sim.partProperty(id, "temp", spec[2]) end
		for x = 60, 560 do for y = 120, 260 do if (x + y) % 2 == 0 then sim.partCreate(-1, x, y, (x % 3 == 0) and elem.DEFAULT_PT_FIRE or elem.DEFAULT_PT_PLSM) end end end`,
	edges: `sim.edgeMode(0) for x = 0, 611 do for y = 0, 20 do sim.partCreate(-1, x, y, elem.DEFAULT_PT_PHOT) end end
		for x = 0, 611 do for y = 360, 383 do sim.partCreate(-1, x, y, elem.DEFAULT_PT_FIRE) end end`,
};
(async () => {
	const browser = await chromium.launch();
	const page = await browser.newPage({ viewport: { width: 1280, height: 800 } });
	const log = [];
	page.on('console', (m) => { if (m.text().includes('TURBO_VERIFY')) log.push(m.text()); });
	await page.goto(process.argv[2] || 'http://127.0.0.1:8834/');
	await page.waitForFunction(() => window.tptPage && window.tptPage.state === 'running', null, { timeout: 60000 });
	await page.waitForTimeout(1000);
	const lua = (c) => page.evaluate((c) => window.tptPage.module.ccall('tpt_lua_command', null, ['string'], [c]), c.replace(/\s*\n\s*/g, ' ')).then(() => page.waitForTimeout(500));
	const call = (n, r, t, a) => page.evaluate(([n, r, t, a]) => window.tptPage.module.ccall(n, r, t, a), [n, r, t, a]);
	await call('tpt_turbo', 'number', ['number'], [4]);
	for (const [name, scene] of Object.entries(SCENES)) {
		const n0 = log.length;
		await lua('tpt.set_pause(1) sim.clearSim() sim.edgeMode(0) ' + scene);
		await call('tpt_benchmark', 'number', ['number', 'number'], [400, 0]);
		const parts = await page.evaluate(() => window.tptPage.module.ccall('tpt_turbo_info', 'string', [], []));
		console.log(name, 'turbo_info', parts, '|', log.slice(n0).filter((l) => l.includes('mismatch')).length, 'mismatch lines;', log[log.length - 1]);
	}
	console.log('mismatches total', log.filter((l) => l.includes('mismatch')).length, log.filter((l) => l.includes('mismatch')).slice(0, 3));
	await browser.close();
})();
