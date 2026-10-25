// runs each challenge's solution N times (and idle M times) in the browser build; prints failures
import { chromium } from '/home/user/tpt-cracker-port/tests/browser/node_modules/playwright/index.mjs';
const N = Number(process.env.N || 5), M = Number(process.env.M || 0);
const only = process.env.IDS ? process.env.IDS.split(',') : null;
const browser = await chromium.launch();
const page = await browser.newPage({ viewport: { width: 1280, height: 800 } });
await page.goto('http://127.0.0.1:8811/');
await page.waitForFunction(() => window.tptPage && window.tptPage.state === 'running', null, { timeout: 60000 });
await page.waitForTimeout(1500);
await page.evaluate(() => window.tptPage.challenges.command('export'));
await page.waitForFunction(() => window.tptPage.challenges.catalog);
const cat = await page.evaluate(() => window.tptPage.challenges.catalog.challenges.map((c) => ({ id: c.id, time: c.time })));
async function play(id, mode, max) {
	return page.evaluate(async ({ id, mode, max }) => {
		const chal = window.tptPage.challenges, m = window.tptPage.module;
		chal.events = [];
		chal.command(mode + ' ' + id);
		const t0 = performance.now();
		while (!chal.events.some((e) => e.type === 'started')) { if (performance.now() - t0 > 10000) return { type: 'nostart' }; await new Promise((r) => setTimeout(r, 20)); }
		let frames = 0;
		while (frames <= max) {
			m.ccall('tpt_benchmark', 'number', ['number', 'number'], [60, 0]); frames += 60;
			const end = chal.events.find((e) => e.type === 'won' || e.type === 'lost');
			if (end) return { type: end.type, reason: end.reason, frames, best: Math.max(0, ...chal.events.filter((e) => e.type === 'progress').map((e) => e.progress)) };
			await new Promise((r) => setTimeout(r, 0));
		}
		return { type: 'none' };
	}, { id, mode, max });
}
for (const c of cat) {
	if (only && !only.includes(c.id)) continue;
	const out = [];
	for (let k = 0; k < N; k++) { const r = await play(c.id, 'solve', c.time * 60 + 120); out.push(r.type === 'won' ? `W${r.frames}` : `L(${r.reason},${r.best.toFixed(2)})`); }
	for (let k = 0; k < M; k++) { const r = await play(c.id, 'start', c.time * 60 + 120); out.push(r.type === 'lost' ? `i-ok` : `IDLE-${r.type}`); }
	const bad = out.some((o) => o.startsWith('L') || o.startsWith('IDLE'));
	console.log((bad ? 'BAD ' : 'ok  ') + c.id + ' ' + out.join(' '));
}
await browser.close();
