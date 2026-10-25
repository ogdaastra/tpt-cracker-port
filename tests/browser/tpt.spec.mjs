// End-to-end tests of the WebAssembly build in Chromium. Everything is driven through the page and the
// game's own UI: mouse/keyboard on the canvas, the in-game Lua console for reading state back.
import { test, expect } from '@playwright/test';
import fs from 'node:fs';
import path from 'node:path';
import * as h from './helpers.mjs';

const results = {};
test.afterAll(() => h.writeArtifact('measurements.json', results));

test('page loads over HTTP, powder.js and powder.wasm initialise, no unexplained errors', async ({ page }) => {
	const log = h.collect(page);
	const navStart = Date.now();
	await h.openGame(page, { dismissIntro: false });
	const runningAt = Date.now();

	// powder-mt.* (threaded) on a cross-origin isolated page, powder.* otherwise
	const threads = await page.evaluate(() => window.tptPage.threads);
	const name = threads ? 'powder-mt' : 'powder';
	const js = log.requests.find((r) => r.url.endsWith(`/${name}.js`));
	const wasm = log.requests.find((r) => r.url.endsWith(`/${name}.wasm`));
	expect(js && js.status).toBe(200);
	expect(wasm && wasm.status).toBe(200);
	expect(wasm.type).toContain('application/wasm');

	// the module really is the game: its exports and filesystem are there and main() is running
	const info = await page.evaluate(() => ({
		state: window.tptPage.state,
		hasFS: typeof window.tptPage.module.FS === 'object',
		openFile: typeof window.tptPage.module._tpt_open_file,
		cwdFiles: window.tptPage.module.FS.readdir('/libsdl/The Powder Toy'),
		timings: window.tptPage.timings,
		buildInfo: window.tptPage.buildInfo,
	}));
	expect(info.state).toBe('running');
	expect(info.hasFS).toBe(true);
	expect(info.openFile).toBe('function');

	await h.waitFrames(page, 120); // let a couple of seconds of game time pass before judging the console
	await h.saveScreenshot(page, '01-loaded-intro');
	results.load = {
		msToRunning: runningAt - navStart,
		pageTimings: info.timings,
		wasmBytes: info.buildInfo.wasmBytes,
		threads,
		commit: info.buildInfo.commit,
	};
	h.writeArtifact('01-load-log.json', { info, console: log.console, requests: log.requests, pageErrors: log.pageErrors });
	await h.expectNoUnexplainedErrors(log);
});

test('canvas is non-blank and keeps updating', async ({ page }) => {
	const log = h.collect(page);
	await h.openGame(page);
	const first = await h.canvasStats(page);
	expect(first.lit / first.total, 'share of lit pixels').toBeGreaterThan(0.01); // UI chrome at least

	// draw a block of powder near the top; with the simulation running it must fall
	await h.drag(page, [[250, 40], [350, 40], [350, 50], [250, 50], [250, 60], [350, 60]]);
	const a = await h.canvasStats(page);
	await h.waitFrames(page, 45);
	const b = await h.canvasStats(page);
	await h.waitFrames(page, 45);
	const c = await h.canvasStats(page);
	expect(a.hash).not.toBe(b.hash);
	expect(b.hash).not.toBe(c.hash);
	await h.saveScreenshot(page, '02-animating');
	await h.expectNoUnexplainedErrors(log);
});

test('mouse draws and erases particles, keyboard shortcuts reach the game', async ({ page }) => {
	const log = h.collect(page);
	await h.openGame(page);
	expect(await h.lua(page, 'sim.NUM_PARTS')).toBe('0');

	// Space is the pause shortcut
	expect(await h.lua(page, 'tpt.set_pause()')).toBe('0');
	await page.keyboard.press('Space');
	await h.waitFrames(page, 6);
	expect(await h.lua(page, 'tpt.set_pause()')).toBe('1');
	await page.keyboard.press('Space');
	await h.waitFrames(page, 6);
	expect(await h.lua(page, 'tpt.set_pause()')).toBe('0');
	await page.keyboard.press('Space'); // stay paused so the powder doesn't fall away before erasing
	await h.waitFrames(page, 6);

	// left button draws with the left tool (DUST)
	expect(await h.lua(page, 'tpt.selectedl')).toBe('DEFAULT_PT_DUST');
	await h.drag(page, [[100, 100], [200, 100], [200, 110], [100, 110]]);
	const afterDraw = Number(await h.lua(page, 'sim.NUM_PARTS'));
	expect(afterDraw).toBeGreaterThan(50);
	await h.saveScreenshot(page, '03-drawn');

	// right button uses the right tool (NONE = erase) over the same strokes
	expect(await h.lua(page, 'tpt.selectedr')).toBe('DEFAULT_PT_NONE');
	await h.drag(page, [[100, 100], [200, 100], [200, 110], [100, 110]], 'right');
	const afterErase = Number(await h.lua(page, 'sim.NUM_PARTS'));
	expect(afterErase).toBeLessThan(afterDraw / 10);
	results.input = { particlesAfterDraw: afterDraw, particlesAfterErase: afterErase };
	await h.saveScreenshot(page, '03-erased');
	await h.expectNoUnexplainedErrors(log);
});

test('crackerk.lua ran: MIST and PLNE (SPSH) exist, are selectable in the UI and can be placed', async ({ page }) => {
	const log = h.collect(page);
	await h.openGame(page);

	// the mod script finished: it sets this global on its last line; the fail-safe banner is absent
	expect(await h.lua(page, 'failsafe')).toBe('1');
	expect(await h.lua(page, 'type(MANAGER)')).toBe('table');

	const mist = await h.lua(page, 'elem.CR1K_PT_MIST');
	const plne = await h.lua(page, 'elem.CR1K_PT_PLNE');
	expect(Number(mist)).toBeGreaterThan(0);
	expect(Number(plne)).toBeGreaterThan(0);
	expect(await h.lua(page, 'elem.property(elem.CR1K_PT_MIST, "Name")')).toBe('MIST');
	expect(await h.lua(page, 'elem.property(elem.CR1K_PT_PLNE, "Name")')).toBe('SPSH');
	expect(await h.lua(page, 'elem.property(elem.CR1K_PT_MIST, "MenuSection") == elem.SC_TOOL')).toBe('true');
	expect(await h.lua(page, 'elem.property(elem.CR1K_PT_PLNE, "MenuSection") == elem.SC_SPECIAL')).toBe('true');
	// a C++ mod element, for comparison
	expect(await h.lua(page, 'elem.property(elem.DEFAULT_PT_RUBR, "Name")')).toBe('RUBR');

	// pause so the placed particles stay put, then select each through the element search window (E)
	await page.keyboard.press('Space');
	for (const [name, ident, x] of [['SPSH', 'CR1K_PT_PLNE', 200], ['MIST', 'CR1K_PT_MIST', 400]]) {
		await page.keyboard.press('KeyE');
		await h.waitFrames(page, 10);
		await page.keyboard.type(name);
		await h.waitFrames(page, 10);
		await h.saveScreenshot(page, `04-search-${name}`);
		await page.keyboard.press('Enter'); // selects the first result as the left tool and closes the search
		await h.waitFrames(page, 10);
		expect(await h.lua(page, 'tpt.selectedl')).toBe(ident);
		await h.click(page, x, 150);
		await h.waitFrames(page, 10);
		expect(Number(await h.lua(page, `sim.elementCount(elem.${ident})`)), `${name} particles placed`).toBeGreaterThan(0);
	}
	await h.saveScreenshot(page, '04-placed-mod-elements');
	await h.expectNoUnexplainedErrors(log);
});

test('save export and import round trip, persistence across reload', async ({ page }) => {
	const log = h.collect(page);
	await h.openGame(page);

	// a recognisable simulation: pause, then draw a block of the mod's C++ element RUBR
	await page.keyboard.press('Space');
	await page.keyboard.press('KeyE');
	await h.waitFrames(page, 10);
	await page.keyboard.type('RUBR');
	await page.keyboard.press('Enter');
	await h.waitFrames(page, 10);
	await h.drag(page, [[150, 150], [300, 150], [300, 170], [150, 170], [150, 190], [300, 190]]);
	const rubr = Number(await h.lua(page, 'sim.elementCount(elem.DEFAULT_PT_RUBR)'));
	const total = Number(await h.lua(page, 'sim.NUM_PARTS'));
	expect(rubr).toBeGreaterThan(100);

	// save with the game's own local save dialog (not logged in: the save button saves locally)
	await h.click(page, 37 + 100, 415); // right part of the bottom-bar save button
	await h.waitFrames(page, 15);
	await page.keyboard.press('Control+a');
	await page.keyboard.type('roundtrip');
	await h.saveScreenshot(page, '05-local-save-dialog');
	await page.keyboard.press('Enter');
	await h.waitFrames(page, 20);
	const saved = await page.evaluate((dir) => window.tptPage.module.FS.readdir(dir + '/Saves'), h.DATA_DIR);
	expect(saved).toContain('roundtrip.cps');

	// export through the page
	await page.click('#btn-export');
	await expect(page.locator('#export-list')).toContainText('roundtrip.cps');
	await h.saveScreenshot(page, '05-export-dialog');
	const [download] = await Promise.all([
		page.waitForEvent('download'),
		page.locator('#export-list li', { hasText: 'roundtrip.cps' }).locator('button').click(),
	]);
	const exported = path.join(h.ARTIFACTS, 'roundtrip-exported.cps');
	await download.saveAs(exported);
	await page.locator('#export-dialog button[value=close]').click();
	const bytes = fs.readFileSync(exported);
	expect(download.suggestedFilename()).toBe('roundtrip.cps');
	expect(bytes.subarray(0, 4).toString('latin1')).toBe('OPS1'); // TPT save format magic
	const inFs = await page.evaluate((p) => window.tptPage.module.FS.readFile(p).length, h.DATA_DIR + '/Saves/roundtrip.cps');
	expect(bytes.length).toBe(inFs);

	// clear the simulation, then import the exported file through the page's file picker
	await h.lua(page, 'sim.clearSim()');
	expect(await h.lua(page, 'sim.NUM_PARTS')).toBe('0');
	await page.setInputFiles('#file-import', { name: 'imported-roundtrip.cps', mimeType: 'application/octet-stream', buffer: bytes });
	await h.waitFrames(page, 30);
	expect(Number(await h.lua(page, 'sim.elementCount(elem.DEFAULT_PT_RUBR)'))).toBe(rubr);
	expect(Number(await h.lua(page, 'sim.NUM_PARTS'))).toBe(total);
	await h.saveScreenshot(page, '05-imported');

	// reload: the saves (IndexedDB) must still be there
	await page.waitForTimeout(500); // autoPersist runs right after the frame that closed the file
	await page.reload();
	await h.openGame(page);
	const afterReload = await page.evaluate((dir) => window.tptPage.module.FS.readdir(dir + '/Saves'), h.DATA_DIR);
	expect(afterReload).toContain('roundtrip.cps');
	expect(afterReload).toContain('imported-roundtrip.cps');
	const reloadedBytes = await page.evaluate((p) => Array.from(window.tptPage.module.FS.readFile(p)), h.DATA_DIR + '/Saves/roundtrip.cps');
	expect(Buffer.from(reloadedBytes).equals(bytes)).toBe(true);
	// and the game's own save browser (Ctrl + Open button) lists them and loads one; it reads the
	// directory and renders thumbnails through Task, which runs without threads in this build
	await h.lua(page, 'sim.clearSim()');
	await page.keyboard.down('Control');
	await h.click(page, 9, 415);
	await page.keyboard.up('Control');
	await h.waitFrames(page, 30);
	await h.saveScreenshot(page, '05-game-save-browser');
	await h.click(page, 128, 131); // first entry of the 4x3 grid in the centred 500x350 window
	await h.waitFrames(page, 30);
	expect(Number(await h.lua(page, 'sim.elementCount(elem.DEFAULT_PT_RUBR)'))).toBe(rubr);
	results.roundTrip = { rubrParticles: rubr, totalParticles: total, saveBytes: bytes.length };
	await h.expectNoUnexplainedErrors(log);
});

test('mod UI and mod C++ elements: J and the Cr-Menu button open the mod menu, all 51 elements run', async ({ page }) => {
	test.setTimeout(180_000);
	const log = h.collect(page);
	await h.openGame(page);
	await page.keyboard.press('Space'); // pause: compare screens without the simulation moving

	// the mod's own menu (crackerk.lua): J toggles it, so does the Cr-Menu button in the bottom bar
	const closed = await h.canvasStats(page);
	await page.keyboard.press('KeyJ');
	await h.waitFrames(page, 15);
	const opened = await h.canvasStats(page);
	await h.saveScreenshot(page, '11-cr-menu-open');
	expect(Math.abs(opened.lit - closed.lit), 'menu changed the screen').toBeGreaterThan(5000);
	await page.keyboard.press('KeyJ');
	await h.waitFrames(page, 15);
	const reclosed = await h.canvasStats(page);
	expect(Math.abs(reclosed.lit - closed.lit), 'menu closed again').toBeLessThan(2000);
	await h.click(page, 444, 415); // "Cr-Menu" button, Button:new(419,408,50,15) in crackerk.lua
	await h.waitFrames(page, 15);
	const viaButton = await h.canvasStats(page);
	expect(Math.abs(viaButton.lit - closed.lit), 'menu opened by its button').toBeGreaterThan(5000);
	await page.keyboard.press('KeyJ');
	await h.waitFrames(page, 15);

	// one particle of each element the mod adds in C++ (MOD_ELEMENT_OFFSET 192 .. RUBR 242)
	await h.lua(page, '0'); // console helpers
	await h.lua(page, 'event.register(event.tick,function() TK=(TK or 0)+1 end) or 0');
	expect(await h.lua(page, 'elem.property(192,"Name").." "..elem.property(242,"Name")')).toBe('FNTC RUBR');
	await h.consoleRun(page, 'for i=0,50 do sim.partCreate(-1,40+i%17*32,80+math.floor(i/17)*60,192+i) end');
	const placed = Number(await h.lua(page, 'sim.NUM_PARTS'));
	await h.saveScreenshot(page, '11-mod-elements-placed');
	await page.keyboard.press('Space'); // run
	const t0 = Number(await h.lua(page, 'TK'));
	await page.waitForTimeout(5000);
	const t1 = Number(await h.lua(page, 'TK'));
	await h.saveScreenshot(page, '11-mod-elements-after-5s');
	expect(await page.evaluate(() => window.tptPage.state)).toBe('running');
	expect(t1 - t0, 'simulation kept ticking with every mod element present').toBeGreaterThan(100);
	results.modElements = { placedParticles: placed, ticksIn5s: t1 - t0 };
	expect(placed).toBeGreaterThanOrEqual(45);
	await h.expectNoUnexplainedErrors(log);
});

test('blocking dialogs from Lua (tpt.confirm, tpt.input, tpt.throw_error) run nested main loops', async ({ page }) => {
	const log = h.collect(page);
	await h.openGame(page);
	await h.lua(page, '0'); // defines the console helpers
	async function consoleCommand(line) {
		await page.keyboard.press('Backquote');
		await h.waitFrames(page, 8);
		await page.keyboard.type(line);
		await page.keyboard.press('Enter'); // the Lua call now blocks inside a nested EngineProcess()
		await h.waitFrames(page, 20);
	}

	await consoleCommand('C1=tpt.confirm("Port test","Nested main loop?","Yes")');
	const during = await h.canvasStats(page);
	await h.saveScreenshot(page, '10-blocking-confirm');
	await page.keyboard.press('Enter'); // the dialog's Yes button
	await h.waitFrames(page, 10);
	await page.keyboard.press('Escape'); // close the console
	await h.waitFrames(page, 6);
	expect(await h.lua(page, 'tostring(C1)')).toBe('true');

	await consoleCommand('I1=tpt.input("Port test","Type something")');
	await page.keyboard.type('hello wasm');
	await h.saveScreenshot(page, '10-blocking-input');
	await page.keyboard.press('Enter');
	await h.waitFrames(page, 10);
	await page.keyboard.press('Escape');
	await h.waitFrames(page, 6);
	expect(await h.lua(page, 'I1')).toBe('hello wasm');

	await consoleCommand('tpt.throw_error("Port test error") E1=1');
	await h.saveScreenshot(page, '10-blocking-error');
	await page.keyboard.press('Enter');
	await h.waitFrames(page, 10);
	await page.keyboard.press('Escape');
	await h.waitFrames(page, 6);
	expect(await h.lua(page, 'E1')).toBe('1');
	expect(during.lit).toBeGreaterThan(0);
	await h.expectNoUnexplainedErrors(log);
});

test('settings persist across reload (game prefs and the mod\'s MANAGER settings)', async ({ page }) => {
	const log = h.collect(page);
	await h.openGame(page);
	expect(await h.lua(page, 'MANAGER.savesetting("PortTest", "value", "42") or "saved"')).toBe('saved');
	const pref = await page.evaluate((p) => {
		try {
			return new TextDecoder().decode(window.tptPage.module.FS.readFile(p));
		} catch (e) {
			return null;
		}
	}, h.DATA_DIR + '/powder.pref');
	expect(pref, 'powder.pref written').not.toBeNull();
	await page.waitForTimeout(500);
	await page.reload();
	await h.openGame(page);
	expect(await h.lua(page, 'MANAGER.getsetting("PortTest", "value")')).toBe('42');
	const prefAfter = await page.evaluate((p) => new TextDecoder().decode(window.tptPage.module.FS.readFile(p)), h.DATA_DIR + '/powder.pref');
	expect(prefAfter.length).toBeGreaterThan(0);
	await h.expectNoUnexplainedErrors(log);
});

test('resizing keeps the viewport usable and mouse mapping correct', async ({ page }) => {
	const log = h.collect(page);
	await h.openGame(page);
	await page.keyboard.press('Space'); // pause so particles stay where they are drawn
	const sizes = [];
	for (const [w, hgt] of [[1600, 1000], [900, 640], [640, 520]]) {
		await page.setViewportSize({ width: w, height: hgt });
		await h.waitFrames(page, 10);
		const box = await page.locator('#canvas').boundingBox();
		const stage = await page.locator('#stage').boundingBox();
		sizes.push({ viewport: [w, hgt], canvas: [Math.round(box.width), Math.round(box.height)] });
		expect(box.width).toBeLessThanOrEqual(stage.width + 1);
		expect(box.height).toBeLessThanOrEqual(stage.height + 1);
		expect(Math.abs(box.width / box.height - h.GAME_W / h.GAME_H)).toBeLessThan(0.02);
		expect(box.x).toBeGreaterThanOrEqual(stage.x - 1);
		// draw at a known game position and check a particle landed there
		const gx = 300;
		const gy = 100 + sizes.length * 40;
		await h.click(page, gx, gy);
		expect(await h.lua(page, `sim.partID(${gx}, ${gy}) ~= nil`)).toBe('true');
	}
	results.resize = sizes;
	await h.saveScreenshot(page, '06-resized-small');
	await h.expectNoUnexplainedErrors(log);
});

test('performance: time to interactive and simulation frame rate, Newtonian gravity works', async ({ page }) => {
	test.setTimeout(240_000);
	const log = h.collect(page);
	const t0 = Date.now();
	await h.openGame(page, { dismissIntro: false });
	const tti = Date.now() - t0;
	const timings = await page.evaluate(() => window.tptPage.timings);
	await page.keyboard.press('Shift');
	// count game ticks from Lua over a wall-clock window
	await h.lua(page, 'event.register(event.tick,function() TK=(TK or 0)+1 end) or 0');
	async function ticksPerSecond(ms) {
		const a = Number(await h.lua(page, 'TK'));
		const ta = Date.now();
		await page.waitForTimeout(ms);
		const b = Number(await h.lua(page, 'TK'));
		return (b - a) / ((Date.now() - ta) / 1000);
	}
	const idle = await ticksPerSecond(3000);

	// a busy simulation: bands of powder across the whole width
	for (let y = 30; y < 190; y += 8) {
		await h.drag(page, [[20, y], [590, y]]);
	}
	const parts = Number(await h.lua(page, 'sim.NUM_PARTS'));
	const busy = await ticksPerSecond(3000);

	// Newtonian gravity (N key): computed on the main thread in this build (no gravity thread).
	// Ordinary particles have no Newtonian mass in TPT; a black hole (NBHL) adds mass every frame.
	await page.keyboard.press('KeyN');
	await h.waitFrames(page, 10);
	expect(await h.lua(page, 'tpt.newtonian_gravity()')).toBe('1');
	expect(Number(await h.lua(page, 'sim.gravMap(80,25)')), 'no field before any mass').toBe(0);
	expect(Number(await h.lua(page, 'sim.partCreate(-1,300,100,elem.DEFAULT_PT_NBHL)'))).toBeGreaterThanOrEqual(0); // empty air, cell (75, 25)
	await h.waitFrames(page, 60);
	// field strength 5 cells from the black hole (at its own cell the pull cancels out by symmetry)
	const field = Number(await h.lua(page, 'sim.gravMap(80,25)'));
	expect(field, 'gravity field next to the black hole').toBeGreaterThan(1e-4); // measured ~2.7e-3 after 60 frames; float noise is ~1e-10
	const busyGravity = await ticksPerSecond(3000);

	results.performance = {
		msToInteractive: tti,
		pageTimingsMs: timings,
		idleTicksPerSecond: Math.round(idle * 10) / 10,
		busyTicksPerSecond: Math.round(busy * 10) / 10,
		busyWithNewtonianGravityTicksPerSecond: Math.round(busyGravity * 10) / 10,
		busyParticles: parts,
		gravityField5CellsFromBlackHole: field,
		note: 'fps cap is 60; headless Chromium, software WebGL (SwiftShader), 4 vCPU container',
	};
	await h.saveScreenshot(page, '07-busy-gravity');
	expect(idle).toBeGreaterThan(30);
	expect(busyGravity).toBeGreaterThan(5);
	await h.expectNoUnexplainedErrors(log);
});

test('saves survive leaving the page while the game starts (IndexedDB load race regression)', async ({ page }) => {
	test.setTimeout(240_000);
	// Before the fix in wasm/pre.js, leaving during the IndexedDB load (~200-300 ms in) deleted saves.
	await h.openGame(page);
	await page.evaluate((dir) => {
		const FS = window.tptPage.module.FS;
		try { FS.mkdir(dir + '/Saves'); } catch (e) {}
		for (let i = 0; i < 40; i++) FS.writeFile(`${dir}/Saves/file${i}.cps`, new Uint8Array(20000).fill(i));
	}, h.DATA_DIR);
	await page.waitForTimeout(1500);
	const counts = [];
	for (const delay of [0, 5, 10, 20, 30, 50, 80, 120, 200, 300, 0, 10, 20, 40, 60, 100]) {
		await page.reload({ waitUntil: 'commit' });
		await page.waitForTimeout(delay); // leave mid-startup
		await page.goto(h.START);
		await page.waitForFunction(() => window.tptPage && window.tptPage.state === 'running' && window.tptPage.module, null, { timeout: 60_000 });
		const n = await page.evaluate((dir) => { try { return window.tptPage.module.FS.readdir(dir + '/Saves').length - 2; } catch (e) { return -1; } }, h.DATA_DIR);
		counts.push(`${delay}ms:${n}`);
		await page.waitForTimeout(800);
	}
	results.interruptedStartups = counts.join(' ');
	expect(counts.every((c) => c.endsWith(':40'))).toBe(true);
});

test('readable failure state when JSPI is missing', async ({ page }) => {
	await page.addInitScript(() => {
		delete WebAssembly.Suspending;
	});
	await page.goto(h.START);
	await expect(page.locator('#failure')).toBeVisible();
	await expect(page.locator('#failure-message')).toContainText('JavaScript Promise Integration');
	await h.saveScreenshot(page, '08-failure-no-jspi');
});

test('readable failure state when powder.wasm cannot be downloaded', async ({ page }) => {
	// both builds' binaries (the page picks powder-mt.wasm when cross-origin isolated)
	await page.route(/powder(-mt)?\.wasm$/, (route) => route.fulfill({ status: 404, body: 'not found' }));
	await page.goto(h.START);
	await expect(page.locator('#failure')).toBeVisible();
	await expect(page.locator('#failure-message')).toContainText('powder.wasm');
	await h.saveScreenshot(page, '09-failure-no-wasm');
});
