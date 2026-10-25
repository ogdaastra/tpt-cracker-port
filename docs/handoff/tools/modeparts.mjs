import { chromium } from '/home/user/tpt-cracker-port/tests/browser/node_modules/playwright/index.mjs';
const port = process.argv[2] || '8822';
const browser = await chromium.launch();
const page = await browser.newPage({ viewport: { width: 1280, height: 800 } });
await page.goto(`http://127.0.0.1:${port}/`);
await page.waitForFunction(() => window.tptPage && window.tptPage.state === 'running', null, { timeout: 60000 });
await page.waitForTimeout(1500);
const lua = (c) => page.evaluate((code) => window.tptPage.module.ccall('tpt_lua_command', null, ['string'], [code]), c);
const luaRet = (c) => page.evaluate((code) => { window.__r = null; window.tptPage.module.ccall('tpt_lua_command', null, ['string'], [code]); }, c);
await lua('sim.clearSim() tpt.set_pause(1) for y = 250, 379 do for x = 10, 600 do sim.partCreate(-1, x, y, elem.DEFAULT_PT_WATR) end end for y = 150, 189 do for x = 10, 600 do sim.partCreate(-1, x, y, elem.DEFAULT_PT_SAND) end end for x = 10, 600, 3 do sim.partCreate(-1, x, 140, elem.DEFAULT_PT_FIRE) end');
const R = { BASC: 'ren.RENDER_BASC', FIRE: 'ren.RENDER_FIRE', SPRK: 'ren.RENDER_SPRK', GLOW: 'ren.RENDER_GLOW', BLUR: 'ren.RENDER_BLUR', EFFE: 'ren.RENDER_EFFE' };
const combos = [['BASC'], ['BASC', 'FIRE'], ['BASC', 'SPRK'], ['BASC', 'GLOW'], ['BASC', 'BLUR'], ['BASC', 'EFFE'], ['FIRE', 'SPRK', 'GLOW', 'BLUR', 'EFFE', 'BASC']];
const out = [];
for (const warp of [false, true]) for (const c of combos) {
	await lua(`ren.renderModes({${c.map((k) => R[k]).join(',')}}) ren.displayModes({${warp ? 'ren.DISPLAY_WARP' : ''}}) ren.colorMode(0)`);
	await page.evaluate(() => window.tptPage.module.ccall('tpt_benchmark', 'number', ['number', 'number'], [30, 1]));
	const t = [];
	for (let k = 0; k < 3; k++) t.push(await page.evaluate(() => window.tptPage.module.ccall('tpt_benchmark', 'number', ['number', 'number'], [100, 1]) / 100));
	t.sort((a, b) => a - b);
	out.push(`${warp ? 'warp ' : '     '}${c.join('+')}: ${t[1].toFixed(3)} ms`);
}
console.log(out.join('\n'));
await browser.close();
