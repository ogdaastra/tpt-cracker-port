// Renders the same still scenes with two builds and compares the canvas pixels.
import { chromium } from '/home/user/tpt-cracker-port/tests/browser/node_modules/playwright/index.mjs';
import fs from 'fs';
const urls = process.argv.slice(2);
const browser = await chromium.launch();
const shots = [];
const setups = {
	gravityOn: `tpt.newtonian_gravity(1) for x = 300, 309 do for y = 200, 209 do sim.partCreate(-1, x, y, elem.DEFAULT_PT_NBHL) end end`,
	gravityOff: `tpt.newtonian_gravity(0)`,
};
const particles = `
	for x = 0, 40 do for y = 0, 12 do sim.partCreate(-1, x, y, elem.DEFAULT_PT_WATR) end end
	for x = 570, 611 do for y = 370, 383 do sim.partCreate(-1, x, y, elem.DEFAULT_PT_OIL) end end
	for x = 0, 8 do for y = 100, 140 do sim.partCreate(-1, x, y, elem.DEFAULT_PT_PLUT) end end
	for x = 200, 230, 3 do for y = 0, 6 do sim.partCreate(-1, x, y, elem.DEFAULT_PT_URAN) end end
	for x = 100, 500 do for y = 250, 300 do sim.partCreate(-1, x, y, (x + y) % 3 == 0 and elem.DEFAULT_PT_LAVA or elem.DEFAULT_PT_WATR) end end
	for x = 600, 611 do for y = 0, 60 do sim.partCreate(-1, x, y, elem.DEFAULT_PT_PLUT) end end
	for x = 50, 560 do sim.partCreate(-1, x, 200, elem.DEFAULT_PT_ACID) end
	for x = 50, 560, 2 do sim.partCreate(-1, x, 380, elem.DEFAULT_PT_ISOZ) end`;
for (const url of urls) {
	const page = await browser.newPage({ viewport: { width: 1280, height: 800 } });
	await page.goto(url + '?threads=0');
	await page.waitForFunction(() => window.tptPage && window.tptPage.state === 'running', null, { timeout: 60000 });
	await page.waitForTimeout(1500);
	const lua = (c) => page.evaluate((code) => window.tptPage.module.ccall('tpt_lua_command', null, ['string'], [code]), c);
	const res = {};
	for (const [sname, setup] of Object.entries(setups)) {
		await lua(`tpt.hud(0) sim.clearSim() tpt.set_pause(1) ${setup}`);
		await page.evaluate(() => window.tptPage.module.ccall('tpt_benchmark', 'number', ['number', 'number'], [20, 0]));
		await lua(`tpt.set_pause(1) ${particles}`);
		for (const mode of [0, 3, 4, 5, 6, 7, 8]) {
			await lua(`tpt.display_mode(${mode})`);
			await page.waitForTimeout(4000);
			const box = await page.locator('#canvas').boundingBox();
			const sc = box.width / 646;
			const buf = await page.screenshot({ clip: { x: box.x, y: box.y, width: Math.floor(612 * sc), height: Math.floor(384 * sc) } });
			res[`${sname}-mode${mode}`] = buf;
		}
	}
	shots.push(res);
	await page.close();
}
const [a, b] = shots;
let same = 0, diff = [];
for (const k of Object.keys(a)) {
	if (Buffer.compare(a[k], b[k]) === 0) same++;
	else { diff.push(k); fs.writeFileSync(`pix-${k}-A.png`, a[k]); fs.writeFileSync(`pix-${k}-B.png`, b[k]); }
}
console.log(`identical: ${same}/${Object.keys(a).length}`, diff.length ? 'different: ' + diff.join(' ') : '');
fs.writeFileSync('pix-sample-fancy.png', b['gravityOn-mode6']);
await browser.close();
