// Helpers for driving the real game through its own UI: mouse and keyboard on the canvas, and the
// in-game Lua console (opened with the ` key) for reading game state back.
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { expect } from '@playwright/test';

export const GAME_W = 646; // WINDOWW = XRES + BARSIZE + EXTRABARSIZE
export const GAME_H = 424; // WINDOWH = YRES + MENUSIZE
export const DATA_DIR = '/libsdl/The Powder Toy';
// Page to open relative to the base URL; index.html when testing a file:// copy of the site.
export const START = process.env.TPT_START_PATH || './';
export const ARTIFACTS = path.resolve(path.dirname(fileURLToPath(import.meta.url)), process.env.TPT_ARTIFACT_DIR || '../../artifacts/browser');
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

export async function openGame(page, { dismissIntro = true, query = '' } = {}) {
	// query: extra address parameters, e.g. 'world=308x192x2' (World size)
	await page.goto(query ? `${START}${START.includes('?') ? '&' : '?'}${query}` : START);
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

// Emscripten measures the mouse in whole CSS pixels from the canvas edge rounded down
// (targetX = clientX - (rect.left | 0)), then SDL scales that by game size / CSS size. Aim at a whole CSS
// pixel measured the same way, checked to land in the wanted game pixel: aiming at the pixel's centre on a
// canvas at a fractional position (73.797 px with CI's fonts) tipped clicks into the next pixel.
function aim(n, edge, size, gameSize) {
	const toGame = (t) => Math.trunc(t * gameSize / size);
	const want = Math.floor(n);
	let t = Math.round((want + 0.5) * size / gameSize);
	if (toGame(t) > want) t--;
	else if (toGame(t) < want) t++;
	return Math.trunc(edge) + t;
}

// The centre of simulation pixel (x, y) on the page in any world size: the world is drawn at its pixel size
// (window.tptPage.world.scale) from the canvas's top left, so it is screen pixel x * scale + scale / 2.
export async function worldPoint(page, x, y) {
	const box = await page.locator('#canvas').boundingBox();
	const { scale, cw, ch } = await page.evaluate(() => ({ scale: window.tptPage.world.scale, cw: document.querySelector('#canvas').width, ch: document.querySelector('#canvas').height }));
	const half = Math.floor(scale / 2);
	return { x: aim(x * scale + half, box.x, box.width, cw), y: aim(y * scale + half, box.y, box.height, ch) };
}

// Canvas pixel (x, y) on the page, for any canvas size (gamePoint assumes the classic 646x424)
export async function screenPoint(page, x, y) {
	const box = await page.locator('#canvas').boundingBox();
	const [cw, ch] = await page.evaluate(() => [document.querySelector('#canvas').width, document.querySelector('#canvas').height]);
	return { x: aim(x, box.x, box.width, cw), y: aim(y, box.y, box.height, ch) };
}

export async function gamePoint(page, x, y) {
	const box = await page.locator('#canvas').boundingBox();
	return { x: aim(x, box.x, box.width, GAME_W), y: aim(y, box.y, box.height, GAME_H) };
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
	// Chromium can't capture a page it hasn't drawn yet ("Unable to capture screenshot", seen once in CI right
	// after a page showed its failure message): wait for two drawn frames, and give it one more try. The
	// screenshot is a record for people, not something a test checks.
	await page.evaluate(() => new Promise((resolve) => requestAnimationFrame(() => requestAnimationFrame(resolve))));
	try {
		await page.screenshot({ path: file });
	} catch (e) {
		if (!String(e).includes('Unable to capture screenshot')) throw e;
		await page.waitForTimeout(250);
		await page.screenshot({ path: file });
	}
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

let luaFileCounter = 0;

// Runs a whole Lua chunk: the code is written to the virtual filesystem and run with dofile from the
// in-game console (the console's line limit makes typing long code impossible). The chunk's return
// value comes back as a string; Lua errors are thrown with their message.
export async function luaFile(page, code, timeoutMs = 20_000) {
	const n = ++luaFileCounter;
	const src = `/tmp/lf${n}.lua`, out = `/tmp/lf${n}.out`;
	const wrapped = `local ok, r = pcall(function()\n${code}\nend)\n` +
		`local f = io.open("${out}", "w") f:write((ok and "OK " or "ERR ") .. tostring(r)) f:close()\n`;
	await page.evaluate(([p, c]) => window.tptPage.module.FS.writeFile(p, c), [src, wrapped]);
	await consoleRun(page, `dofile("${src}")`);
	let result = null;
	const deadline = Date.now() + timeoutMs;
	while (Date.now() < deadline) {
		result = await readFsText(page, out);
		if (result !== null) break;
		await page.waitForTimeout(50);
	}
	if (result === null) throw new Error(`Lua chunk ${src} produced no result`);
	if (!result.startsWith('OK ')) throw new Error(`Lua error in ${src}: ${result}`);
	return result.slice(3);
}

// Lua that writes every particle's properties (floats with 9 significant digits, enough for an exact float32) and
// the air grids to path, one line each: comparing two of these tells whether two simulation states are the same
// (same as docs/handoff/tools/samecheck.mjs).
export function particleStateLua(path) {
	return `
	local props = { "type", "ctype", "life", "tmp", "tmp2", "x", "y", "vx", "vy", "temp", "flags", "dcolour" }
	for _, extra in ipairs({ "tmp3", "tmp4", "pavg0", "pavg1" }) do
		if pcall(sim.partProperty, 0, extra) then props[#props + 1] = extra end
	end
	local out = {}
	for i in sim.parts() do
		local row = { i }
		for _, p in ipairs(props) do
			local ok, v = pcall(sim.partProperty, i, p)
			row[#row + 1] = ok and (math.floor(v) == v and string.format("%d", v) or string.format("%.9g", v)) or "-"
		end
		out[#out + 1] = table.concat(row, " ")
	end
	for y = 0, 95 do
		local row = { "air", y }
		for x = 0, 152 do
			row[#row + 1] = string.format("%.9g %.9g %.9g", sim.pressure(x, y), sim.velocityX(x, y), sim.velocityY(x, y))
		end
		out[#out + 1] = table.concat(row, " ")
	end
	local f = io.open("${path}", "w") f:write(table.concat(out, "\\n")) f:close()`;
}

// Runs a Lua expression in the game loop (tpt_lua_command; the console's window would stop the simulation while
// it is open) and returns its value as a string.
let loopLuaRuns = 0;
export async function loopLua(page, expression) {
	const out = `/tmp/looplua${++loopLuaRuns}.txt`;
	await page.evaluate(([e, p]) => window.tptPage.module.ccall('tpt_lua_command', null, ['string'],
		[`local f = io.open("${p}", "w") f:write(tostring((function() return ${e} end)())) f:close()`]), [expression, out]);
	for (let k = 0; k < 600; k++) {
		const text = await page.evaluate((p) => { try { return new TextDecoder().decode(window.tptPage.module.FS.readFile(p)); } catch (e) { return null; } }, out);
		if (text !== null) return text;
		await page.waitForTimeout(5);
	}
	throw new Error('Lua did not run: ' + expression);
}
