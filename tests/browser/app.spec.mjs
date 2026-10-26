// The site as an installable app (manifest, icons, sw.js): it works offline once loaded, keeps the
// cross-origin isolation (and so the threaded build) when served from its cache, and offers a reload when
// a new build is deployed. Other spec files run with service workers blocked.
import { test, expect } from '@playwright/test';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import * as h from './helpers.mjs';
import { startServer } from './serve.mjs';

test.use({ serviceWorkers: 'allow' });
test.skip(!!process.env.TPT_START_PATH, 'file:// pages cannot use service workers');

const results = {};
test.afterAll(() => h.writeArtifact('app.json', results));

const DIST = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../../dist');

async function waitOfflineReady(page) {
	await page.waitForFunction(() => window.tptPage.app && window.tptPage.app.offlineReady, null, { timeout: 60_000 });
	return page.evaluate(() => window.tptPage.app.offlineReady);
}

test('the manifest and icons make the site installable', async ({ page, request }) => {
	await page.goto(h.START);
	const href = await page.getAttribute('link[rel=manifest]', 'href');
	const manifest = await (await request.get(new URL(href, page.url()).href)).json();
	results.manifest = manifest;
	expect(manifest.display).toBe('standalone');
	expect(manifest.start_url).toBeTruthy();
	const sizes = manifest.icons.map((i) => `${i.sizes} ${i.purpose}`);
	expect(sizes).toEqual(expect.arrayContaining(['192x192 any', '512x512 any', '512x512 maskable']));
	for (const icon of manifest.icons) {
		const r = await request.get(new URL(icon.src, new URL(href, page.url())).href);
		expect(r.status(), icon.src).toBe(200);
		expect(r.headers()['content-type']).toContain('image/png');
	}
});

test('once loaded, the game starts offline, still cross-origin isolated and threaded', async ({ page, context }) => {
	test.setTimeout(180_000);
	const log = h.collect(page);
	await h.openGame(page);
	const version = await waitOfflineReady(page);
	const info = await page.evaluate(() => window.tptPage.buildInfo);
	results.version = version;
	expect(version.startsWith(info.commit), 'the cache is named after the running build').toBe(true);
	const threads = await page.evaluate(() => window.tptPage.threads);
	const cached = await page.evaluate(async () => {
		const keys = [];
		for (const name of await caches.keys()) {
			for (const r of await (await caches.open(name)).keys()) keys.push(new URL(r.url).pathname.replace(/^.*\//, ''));
		}
		return keys.sort();
	});
	results.cached = cached;
	expect(cached).toEqual(expect.arrayContaining(['index.html', 'tpt-web.js', 'style.css', threads ? 'powder-mt.wasm' : 'powder.wasm']));
	expect(cached, 'only the game build the page uses is cached').not.toContain(threads ? 'powder.wasm' : 'powder-mt.wasm');
	await h.expectNoUnexplainedErrors(log);

	// no network at all: the page, the game and its build come from the cache
	await context.setOffline(true);
	const log2 = h.collect(page);
	await page.reload();
	await page.waitForFunction(() => window.tptPage && window.tptPage.state === 'running', null, { timeout: 60_000 });
	await h.waitFrames(page, 60);
	const offline = await page.evaluate(() => ({ online: navigator.onLine, isolated: self.crossOriginIsolated, threads: window.tptPage.threads, commit: window.tptPage.buildInfo.commit }));
	results.offline = offline;
	expect(offline).toEqual({ online: false, isolated: threads, threads, commit: info.commit });
	// and the game works: dust falls
	const fell = await h.luaFile(page, `
		sim.clearSim() local i = sim.partCreate(-1, 300, 100, elem.DEFAULT_PT_DUST) tpt.set_pause(0)
		return i`);
	await h.waitFrames(page, 60);
	const y = Number(await h.luaFile(page, `return sim.partProperty(${fell}, "y")`));
	expect(y, 'dust fell while offline').toBeGreaterThan(110);
	await h.saveScreenshot(page, 'app-offline');
	await h.expectNoUnexplainedErrors(log2);
	await context.setOffline(false);
});

test('a new deployment shows "A new version is ready"; reloading switches to it and drops the old cache', async ({ page }) => {
	test.setTimeout(240_000);
	// a private copy of the site, so the test can "deploy" a new build into it
	const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'tpt-app-'));
	fs.cpSync(DIST, dir, { recursive: true, filter: (src) => !src.includes(`${path.sep}source`) });
	const { server, url } = await startServer(dir, 0);
	try {
		const log = h.collect(page);
		await page.goto(url);
		await page.waitForFunction(() => window.tptPage && window.tptPage.state === 'running', null, { timeout: 60_000 });
		const v1 = await waitOfflineReady(page);
		await expect(page.locator('#update-bar')).toBeHidden();

		// deploy: same files, a service worker for a new build name
		const v2 = v1 + '-next';
		const sw = fs.readFileSync(path.join(dir, 'sw.js'), 'utf8');
		fs.writeFileSync(path.join(dir, 'sw.js'), sw.replace(`const VERSION = '${v1}'`, `const VERSION = '${v2}'`));
		await page.evaluate(() => window.tptPage.app.registration.update());
		await expect(page.locator('#update-bar')).toBeVisible({ timeout: 30_000 });
		await h.saveScreenshot(page, 'app-update-bar');
		// the old build keeps running until the player chooses
		expect(await page.evaluate(() => caches.keys())).toEqual(expect.arrayContaining([`tpt-${v1}`, `tpt-${v2}`]));

		await Promise.all([
			page.waitForEvent('load'),
			page.click('#update-reload'),
		]);
		await page.waitForFunction(() => window.tptPage && window.tptPage.state === 'running', null, { timeout: 60_000 });
		const v = await waitOfflineReady(page);
		results.update = { from: v1, to: v, caches: await page.evaluate(() => caches.keys()) };
		expect(v).toBe(v2);
		expect(await page.evaluate(() => caches.keys())).toEqual([`tpt-${v2}`]);
		await expect(page.locator('#update-bar')).toBeHidden();
		await h.expectNoUnexplainedErrors(log);
	} finally {
		server.close();
		fs.rmSync(dir, { recursive: true, force: true });
	}
});
