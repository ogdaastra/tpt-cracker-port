import { chromium } from '/home/user/tpt-cracker-port/tests/browser/node_modules/playwright/index.mjs';
const browser = await chromium.launch();
const page = await browser.newPage({ viewport: { width: 1280, height: 800 } });
await page.goto('http://127.0.0.1:8822/');
await page.waitForFunction(() => window.tptPage && window.tptPage.state === 'running', null, { timeout: 60000 });
await page.waitForTimeout(1500);
const cdp = await page.context().newCDPSession(page);
await cdp.send('Profiler.enable');
await cdp.send('Profiler.setSamplingInterval', { interval: 100 });
const lua = (c) => page.evaluate((code) => window.tptPage.module.ccall('tpt_lua_command', null, ['string'], [code]), c);
const scenes = {
	powderBands: 'sim.clearSim() tpt.set_pause(1) for y = 30, 190, 8 do for x = 20, 590 do for d = 0, 1 do sim.partCreate(-1, x, y + d, elem.DEFAULT_PT_DUST) end end end',
	smallFire: 'sim.clearSim() tpt.set_pause(1) for x = 280, 330 do for y = 300, 330 do sim.partCreate(-1, x, y, elem.DEFAULT_PT_WOOD) end end for x = 280, 330, 2 do sim.partCreate(-1, x, 299, elem.DEFAULT_PT_FIRE) end',
	liquidsAndFire: 'sim.clearSim() tpt.set_pause(1) for y = 250, 379 do for x = 10, 600 do sim.partCreate(-1, x, y, elem.DEFAULT_PT_WATR) end end for y = 150, 189 do for x = 10, 600 do sim.partCreate(-1, x, y, elem.DEFAULT_PT_SAND) end end for x = 10, 600, 3 do sim.partCreate(-1, x, 140, elem.DEFAULT_PT_FIRE) end',
	electronics: 'sim.clearSim() tpt.set_pause(1) for row = 0, 19 do local y = 20 + row * 18 for x = 20, 590 do sim.partCreate(-1, x, y, elem.DEFAULT_PT_METL) end for x = 20, 590 do sim.partCreate(-1, x, y + 1, (x % 2 == 0) and elem.DEFAULT_PT_PSCN or elem.DEFAULT_PT_NSCN) end for x = 20, 590, 40 do sim.partCreate(-1, x, y - 1, elem.DEFAULT_PT_BTRY) end for x = 20, 590 do sim.partCreate(-1, x, y + 2, elem.DEFAULT_PT_COPR or elem.DEFAULT_PT_METL) end end',
};
for (const [name, code] of Object.entries(scenes)) {
	await lua(code);
	await page.waitForTimeout(500);
	await page.evaluate(() => window.tptPage.module.ccall('tpt_benchmark', 'number', ['number', 'number'], [30, 0]));
	const ms = await page.evaluate(() => window.tptPage.module.ccall('tpt_benchmark', 'number', ['number', 'number'], [60, 0]) / 60);
	await cdp.send('Profiler.start');
	await page.evaluate(() => window.tptPage.module.ccall('tpt_benchmark', 'number', ['number', 'number'], [200, 0]));
	const { profile } = await cdp.send('Profiler.stop');
	const self = new Map();
	const dt = profile.timeDeltas;
	const byId = new Map(profile.nodes.map((n) => [n.id, n]));
	const counts = new Map();
	for (const s of profile.samples) counts.set(s, (counts.get(s) || 0) + 1);
	let total = 0;
	for (const [id, c] of counts) { const n = byId.get(id); const fn = n.callFrame.functionName || '(anon)'; self.set(fn, (self.get(fn) || 0) + c); total += c; }
	const top = [...self.entries()].sort((a, b) => b[1] - a[1]).slice(0, 18).map(([f, c]) => `${(100 * c / total).toFixed(1)}% ${f}`);
	console.log(`== ${name}: ${ms.toFixed(3)} ms per simulated frame\n` + top.join('\n'));
}
await browser.close();
