// Browser tests for the WebAssembly build. Runs against dist/ served over HTTP by serve.mjs, or against a
// deployment when TPT_BASE_URL is set (e.g. a Vercel preview URL).
import { defineConfig, devices } from '@playwright/test';

const baseURL = process.env.TPT_BASE_URL || 'http://127.0.0.1:8765/';
const local = !process.env.TPT_BASE_URL;

export default defineConfig({
	testDir: '.',
	testMatch: /.*\.spec\.mjs/,
	timeout: 120_000,
	expect: { timeout: 15_000 },
	workers: 1, // one game instance at a time keeps timings meaningful
	retries: 0,
	outputDir: '../../artifacts/playwright/output',
	reporter: [
		['list'],
		['json', { outputFile: '../../artifacts/playwright/results.json' }],
		['html', { outputFolder: '../../artifacts/playwright/report', open: 'never' }],
	],
	use: {
		baseURL,
		viewport: { width: 1280, height: 800 },
		acceptDownloads: true,
		trace: 'retain-on-failure',
		screenshot: 'only-on-failure',
	},
	projects: [
		// Target is Microsoft Edge (Chromium). Set TPT_BROWSER_CHANNEL=msedge where Edge is installed.
		{ name: 'chromium', use: { ...devices['Desktop Chrome'], channel: process.env.TPT_BROWSER_CHANNEL || undefined, viewport: { width: 1280, height: 800 } } },
	],
	webServer: local ? {
		command: 'node serve.mjs ../../dist 8765',
		url: baseURL,
		reuseExistingServer: false,
		timeout: 30_000,
	} : undefined,
});
