import { chromium } from '/home/user/tpt-cracker-port/tests/browser/node_modules/playwright/index.mjs';
const url = process.argv[2];
const browser = await chromium.launch();
const page = await browser.newPage({ viewport: { width: 1280, height: 800 } });
await page.goto(url);
await page.waitForFunction(() => window.tptPage && window.tptPage.state === 'running', null, { timeout: 60000 });
await page.waitForTimeout(1500);
const lua = (c) => page.evaluate((code) => window.tptPage.module.ccall('tpt_lua_command', null, ['string'], [code]), c);
await lua(`tpt.hud(0) sim.clearSim() tpt.set_pause(1) tpt.newtonian_gravity(0)
	for x = 0, 40 do for y = 0, 12 do sim.partCreate(-1, x, y, elem.DEFAULT_PT_WATR) end end
	for x = 570, 611 do for y = 370, 383 do sim.partCreate(-1, x, y, elem.DEFAULT_PT_OIL) end end
	for x = 0, 8 do for y = 100, 140 do sim.partCreate(-1, x, y, elem.DEFAULT_PT_PLUT) end end
	for x = 100, 500 do for y = 250, 300 do sim.partCreate(-1, x, y, (x + y) % 7 == 0 and elem.DEFAULT_PT_LAVA or elem.DEFAULT_PT_WATR) end end
	for x = 50, 560 do sim.partCreate(-1, x, 200, elem.DEFAULT_PT_ACID) end
	for x = 0, 611, 2 do sim.partCreate(-1, x, 383, elem.DEFAULT_PT_ISOZ) end`);
const out = {};
for (const m of [0, 3, 4, 5, 6, 8]) {
	await lua(`tpt.display_mode(${m}) SHOT = nil`); await page.waitForTimeout(3500);
	let prev = null, hash = null;
	for (let k = 0; k < 20; k++) {
		await page.waitForTimeout(300);
		hash = await page.evaluate(async () => {
			const M = window.tptPage.module;
			M.ccall('tpt_lua_command', null, ['string'], ['SHOT = tpt.screenshot(1, 2)']);
			await new Promise((r) => setTimeout(r, 50));
			const files = M.FS.readdir('.').filter((f) => f.startsWith('screenshot') && f.endsWith('.ppm'));
			const data = M.FS.readFile(files[0]);
			for (const f of files) M.FS.unlink(f);
			const d = await crypto.subtle.digest('SHA-256', data);
			return [...new Uint8Array(d)].map((b) => b.toString(16).padStart(2, '0')).join('').slice(0, 16) + ' ' + data.length;
		});
		if (hash === prev) break;
		prev = hash;
	}
	out[m] = hash;
}
console.log(JSON.stringify(out));
await browser.close();
