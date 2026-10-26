// Renders the game icon (resources/icon_exe.svg) into the PNG sizes the web app manifest uses, with the
// Chromium that Playwright installs: icon-192.png, icon-512.png (round icon, transparent corners) and
// icon-maskable-512.png (the icon inside the 80% safe zone on a dark square, for launchers that crop icons
// into their own shape). Usage, from tests/browser (where Playwright is installed): node ../../wasm/make-icons.mjs
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const { chromium } = await import(path.join(process.cwd(), 'node_modules/playwright/index.mjs'));
const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const svg = fs.readFileSync(path.join(root, 'resources/icon_exe.svg'), 'utf8');
const src = 'data:image/svg+xml;base64,' + Buffer.from(svg).toString('base64');
const out = path.join(root, 'web/icons');
fs.mkdirSync(out, { recursive: true });

const browser = await chromium.launch();
const page = await browser.newPage();
async function render(file, size, inset, background) {
	await page.setViewportSize({ width: size, height: size });
	await page.setContent(`<html><body style="margin:0;background:${background}">
		<img src="${src}" style="position:absolute;left:${inset}px;top:${inset}px;width:${size - 2 * inset}px;height:${size - 2 * inset}px">
		</body></html>`);
	await page.waitForFunction(() => document.images[0].complete);
	await page.screenshot({ path: path.join(out, file), omitBackground: background === 'transparent' });
}
await render('icon-192.png', 192, 0, 'transparent');
await render('icon-512.png', 512, 0, 'transparent');
await render('icon-maskable-512.png', 512, Math.round(512 * 0.1), '#101014');
await browser.close();
console.log('icons written to', out);
