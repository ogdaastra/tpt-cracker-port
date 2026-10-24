// Helpers for driving the real game through its own UI: mouse and keyboard on the canvas, and the
// in-game Lua console (opened with the ` key) for reading game state back.
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { expect } from '@playwright/test';

export const GAME_W = 629; // WINDOWW = XRES + BARSIZE
export const GAME_H = 424; // WINDOWH = YRES + MENUSIZE
export const DATA_DIR = '/libsdl/The Powder Toy';
// Page to open relative to the base URL; index.html when testing a file:// copy of the site.
export const START = process.env.TPT_START_PATH || './';
export const ARTIFACTS = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../../artifacts/browser');
fs.mkdirSync(ARTIFACTS, { recursive: true });

// Console messages that are understood and harmless; anything else at error level fails the load test.
export const EXPLAINED_MESSAGES = [
	// SDL's WebGL renderer sets the swap interval through emscripten_set_main_loop_timing, which only
	// applies to emscripten_set_main_loop; the game runs its own loop (JSPI), so this is a no-op.
	/emscripten_set_main_loop_timing: Cannot set timing mode for main loop since a main loop does not exist/,
	// Printed by the multiplayer script: no raw TCP in browsers.
	/TPTMP v[\d.]+: Cannot load: no TCP socket support in this build/,
	// Chromium/SwiftShader driver notes when the test reads back the WebGL canvas for screenshots.
	/GL Driver Message .*GPU stall due to ReadPixels/,
];

export function isExplained(text) {
	return EXPLAINED_MESSAGES.some((re) => re.test(text));
}

// Attach collectors before navigation. Returns arrays that keep filling while the page lives.
export function collect(page) {
	const out = { console: [], pageErrors: [], failedRequests: [], badResponses: [], requests: [] };
	page.on('console', (m) => out.console.push({ type: m.type(), text: m.text() }));
	page.on('pageerror', (e) => out.pageErrors.push(String(e && e.stack || e)));
	page.on('requestfailed', (r) => out.failedRequests.push(`${r.url()} ${r.failure() && r.failure().errorText}`));
	page.on('response', (r) => {
		out.requests.push({ url: r.url(), status: r.status(), type: r.headers()['content-type'] || '' });
		if (r.status() >= 400) out.badResponses.push(`${r.status()} ${r.url()}`);
	});
	return out;
}

export async function openGame(page, { dismissIntro = true } = {}) {
	await page.goto(START);
	// 'running' is set when the game has presented its first frame (SDL input listeners exist by then)
	await page.waitForFunction(() => window.tptPage && ((window.tptPage.state === 'running' && window.tptPage.module) || window.tptPage.state === 'failed'), null, { timeout: 60_000 });
	const state = await page.evaluate(() => window.tptPage.state);
	if (state !== 'running') {
		const failure = await page.evaluate(() => window.tptPage.failure);
		throw new Error('game failed to start: ' + JSON.stringify(failure));
	}
	await waitFrames(page, 30);
	if (dismissIntro) {
		await page.keyboard.press('Shift'); // "Press any key" intro of the mod
		await waitFrames(page, 60); // the intro text fades out over 50 frames
	}
}

// The game runs at its 60 fps cap; waiting in wall time is the honest way to let frames pass.
export async function waitFrames(page, n) {
	await page.waitForTimeout(Math.ceil(n * 1000 / 60) + 50);
}

export async function gamePoint(page, x, y) {
	const box = await page.locator('#canvas').boundingBox();
	return { x: box.x + (x + 0.5) * box.width / GAME_W, y: box.y + (y + 0.5) * box.height / GAME_H };
}

export async function click(page, x, y, button = 'left') {
	const p = await gamePoint(page, x, y);
	await page.mouse.move(p.x, p.y);
	await waitFrames(page, 2);
	await page.mouse.down({ button });
	await waitFrames(page, 3);
	await page.mouse.up({ button });
	await waitFrames(page, 3);
}

export async function drag(page, points, button = 'left') {
	let p = await gamePoint(page, points[0][0], points[0][1]);
	await page.mouse.move(p.x, p.y);
	await waitFrames(page, 2);
	await page.mouse.down({ button });
	for (const [x, y] of points.slice(1)) {
		p = await gamePoint(page, x, y);
		await page.mouse.move(p.x, p.y, { steps: 4 });
		await waitFrames(page, 1);
	}
	await page.mouse.up({ button });
	await waitFrames(page, 3);
}

let luaCounter = 0;

// The console's input box only accepts characters while the line still fits its pixel width
// (src/gui/interface/Textbox.cpp), so every typed line must stay short.
const MAX_CONSOLE_LINE = 90;

export async function consoleRun(page, line) {
	if (line.length > MAX_CONSOLE_LINE) throw new Error(`console line too long (${line.length}): ${line}`);
	await page.keyboard.press('Backquote');
	await waitFrames(page, 8);
	await page.keyboard.type(line, { delay: 0 });
	if (process.env.TPT_DEBUG_LUA) await page.screenshot({ path: `/root/tools/probe/lua-typed-${luaCounter}.png` });
	await page.keyboard.press('Enter');
	await waitFrames(page, 4);
	await page.keyboard.press('Escape'); // close the console
	await waitFrames(page, 6);
}

async function readFsText(page, file) {
	return page.evaluate((f) => {
		try {
			return new TextDecoder().decode(window.tptPage.module.FS.readFile(f));
		} catch (e) {
			return null;
		}
	}, file);
}

// Runs a Lua expression in the game's own console and returns its value as a string. Two tiny helper
// functions are defined first (once per page load); they write the result to a file in the virtual
// filesystem, which the test then reads.
export async function lua(page, expression) {
	const installed = await page.evaluate(() => !!window.tptPage.luaHelpers);
	if (!installed) {
		await consoleRun(page, 'function T(p,v) local f=io.open(p,"w") f:write(v) f:close() end');
		await consoleRun(page, 'function W(n,o,r) T("/tmp/q"..n,(o and "OK " or "ERR ")..tostring(r)) end');
		await consoleRun(page, 'function Q(n,s) W(n,pcall(loadstring("return "..s))) end');
		await page.evaluate(() => { window.tptPage.luaHelpers = true; });
	}
	const n = ++luaCounter;
	await consoleRun(page, `Q(${n},[[${expression}]])`);
	let result = null;
	const deadline = Date.now() + 10_000;
	while (Date.now() < deadline) {
		result = await readFsText(page, `/tmp/q${n}`);
		if (result !== null) break;
		await page.waitForTimeout(50);
	}
	if (result === null) throw new Error(`Lua console produced no result for: ${expression}`);
	if (!result.startsWith('OK ')) throw new Error(`Lua error for ${expression}: ${result}`);
	return result.slice(3);
}

// Pixel statistics of the canvas as composited on screen (works for the WebGL canvas).
export async function canvasStats(page) {
	const png = await page.locator('#canvas').screenshot();
	return page.evaluate(async (b64) => {
		const blob = await (await fetch('data:image/png;base64,' + b64)).blob();
		const bmp = await createImageBitmap(blob);
		const c = new OffscreenCanvas(bmp.width, bmp.height);
		const ctx = c.getContext('2d');
		ctx.drawImage(bmp, 0, 0);
		const d = ctx.getImageData(0, 0, bmp.width, bmp.height).data;
		let lit = 0;
		let hash = 0;
		for (let i = 0; i < d.length; i += 4) {
			const v = d[i] + d[i + 1] + d[i + 2];
			if (v > 30) lit++;
			hash = (hash * 31 + v) | 0;
		}
		return { width: bmp.width, height: bmp.height, lit, total: d.length / 4, hash };
	}, png.toString('base64'));
}

export async function saveScreenshot(page, name) {
	const file = path.join(ARTIFACTS, `${name}.png`);
	await page.screenshot({ path: file });
	return file;
}

export function writeArtifact(name, data) {
	fs.writeFileSync(path.join(ARTIFACTS, name), typeof data === 'string' ? data : JSON.stringify(data, null, 2));
}

export async function expectNoUnexplainedErrors(log) {
	const errors = log.console.filter((m) => m.type === 'error' && !isExplained(m.text)).map((m) => m.text);
	expect.soft(log.pageErrors, 'uncaught page errors').toEqual([]);
	expect.soft(errors, 'unexplained console errors').toEqual([]);
	expect.soft(log.failedRequests, 'failed requests').toEqual([]);
	expect.soft(log.badResponses, 'HTTP error responses').toEqual([]);
}
