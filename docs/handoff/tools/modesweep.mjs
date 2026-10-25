import { chromium } from '/home/user/tpt-cracker-port/tests/browser/node_modules/playwright/index.mjs';
const port = process.argv[2] || '8822';
const browser = await chromium.launch();
const page = await browser.newPage({ viewport: { width: 1280, height: 800 } });
await page.goto(`http://127.0.0.1:${port}/`);
await page.waitForFunction(() => window.tptPage && window.tptPage.state === 'running', null, { timeout: 60000 });
await page.waitForTimeout(1500);
const lua = (c) => page.evaluate((code) => window.tptPage.module.ccall('tpt_lua_command', null, ['string'], [code]), c);
await lua('sim.clearSim() tpt.set_pause(1) for y = 250, 379 do for x = 10, 600 do sim.partCreate(-1, x, y, elem.DEFAULT_PT_WATR) end end for y = 150, 189 do for x = 10, 600 do sim.partCreate(-1, x, y, elem.DEFAULT_PT_SAND) end end for x = 10, 600, 3 do sim.partCreate(-1, x, 140, elem.DEFAULT_PT_FIRE) end for x = 100, 500, 50 do sim.partCreate(-1, x, 100, elem.DEFAULT_PT_GRVT) end tpt.newtonian_gravity(1)');
await page.evaluate(() => window.tptPage.module.ccall('tpt_benchmark', 'number', ['number', 'number'], [60, 0]));
const names = ['velocity', 'pressure', 'persistent', 'fire', 'blob', 'heat', 'fancy', 'nothing', 'heat gradient', 'life gradient', 'alt velocity'];
const out = [];
for (let m = 0; m <= 10; m++) {
	await lua(`tpt.display_mode(${m})`);
	await page.evaluate(() => window.tptPage.module.ccall('tpt_benchmark', 'number', ['number', 'number'], [30, 1]));
	const t = [];
	for (let k = 0; k < 3; k++) t.push(await page.evaluate(() => window.tptPage.module.ccall('tpt_benchmark', 'number', ['number', 'number'], [100, 1]) / 100));
	t.sort((a, b) => a - b);
	out.push(`${m} ${names[m]}: ${t[1].toFixed(3)} ms`);
}
console.log(out.join('\n'));
await browser.close();
