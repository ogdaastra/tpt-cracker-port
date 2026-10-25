// Recording the game from the page: video (MediaRecorder), GIF (gifenc worker) and PNG screenshots.
import fs from 'node:fs';
import { test, expect } from '@playwright/test';
import * as h from './helpers.mjs';

const results = {};
test.afterAll(() => h.writeArtifact('record.json', results));

// Count the image frames in a GIF: walk the blocks after the header and the global colour table.
function gifInfo(buf) {
	const sig = buf.subarray(0, 6).toString('latin1');
	const width = buf.readUInt16LE(6);
	const height = buf.readUInt16LE(8);
	let p = 13;
	if (buf[10] & 0x80) p += 3 * (1 << ((buf[10] & 7) + 1));
	let frames = 0;
	while (p < buf.length) {
		const b = buf[p];
		if (b === 0x3b) break; // trailer
		if (b === 0x21) { // extension: label, then sub-blocks
			p += 2;
			while (buf[p]) p += buf[p] + 1;
			p++;
		} else if (b === 0x2c) { // image descriptor, optional local colour table, LZW data sub-blocks
			frames++;
			const packed = buf[p + 9];
			p += 10;
			if (packed & 0x80) p += 3 * (1 << ((packed & 7) + 1));
			p++; // LZW minimum code size
			while (buf[p]) p += buf[p] + 1;
			p++;
		} else {
			throw new Error(`bad GIF block 0x${b.toString(16)} at ${p}`);
		}
	}
	return { sig, width, height, frames, complete: buf[p] === 0x3b };
}

async function busyScene(page) {
	await h.luaFile(page, `
		sim.clearSim()
		for x = 100, 500, 4 do sim.partCreate(-1, x, 60, elem.DEFAULT_PT_DUST) end
		for x = 150, 450, 30 do sim.partCreate(-1, x, 300, elem.DEFAULT_PT_FIRE) end
		return 0`);
}

async function recordVia(page, options, label) {
	const download = page.waitForEvent('download', { timeout: 120_000 });
	await page.click('#btn-record');
	await expect(page.locator('#record-dialog')).toBeVisible();
	await page.check(`#record-dialog input[name=format][value=${options.format}]`);
	await page.check(`#record-dialog input[name=area][value=${options.area}]`);
	await page.selectOption('#record-dialog select[name=scale]', String(options.scale));
	await page.selectOption('#record-dialog select[name=seconds]', String(options.seconds));
	await page.click(options.screenshot ? '#record-screenshot' : '#record-start');
	if (!options.screenshot) {
		await expect(page.locator('#btn-record')).toHaveClass(/recording/);
	}
	const d = await download;
	const file = `${h.ARTIFACTS}/${label}-${d.suggestedFilename()}`;
	await d.saveAs(file);
	return { name: d.suggestedFilename(), buf: fs.readFileSync(file) };
}

test('video recording: a file of the right kind downloads and plays', async ({ page }) => {
	test.setTimeout(180_000);
	const log = h.collect(page);
	await h.openGame(page);
	const videoType = await page.evaluate(() => window.tptPage.recorder.videoType);
	expect(videoType, 'MediaRecorder supports a video type').toBeTruthy();
	await busyScene(page);
	const { name, buf } = await recordVia(page, { format: 'video', area: 'sim', scale: 2, seconds: 5 }, 'video');
	const isMp4 = buf.subarray(4, 8).toString('latin1') === 'ftyp';
	const isWebm = buf.readUInt32BE(0) === 0x1a45dfa3;
	expect(isMp4 || isWebm, `container of ${name}`).toBe(true);
	expect(name).toMatch(isMp4 ? /\.mp4$/ : /\.webm$/);
	expect(buf.length).toBeGreaterThan(20_000);
	// the browser can decode it: metadata gives the recorded size (612x384 at 2x)
	const meta = await page.evaluate(async ({ b64, type }) => {
		const v = document.createElement('video');
		v.muted = true;
		v.src = URL.createObjectURL(new Blob([Uint8Array.from(atob(b64), (c) => c.charCodeAt(0))], { type }));
		await new Promise((res, rej) => { v.onloadeddata = res; v.onerror = () => rej(new Error('video did not load')); });
		return { w: v.videoWidth, h: v.videoHeight };
	}, { b64: buf.toString('base64'), type: isMp4 ? 'video/mp4' : 'video/webm' });
	expect(meta).toEqual({ w: 1224, h: 768 });
	const rec = await page.evaluate(() => window.tptPage.recording);
	expect(rec.state).toBe('done');
	expect(rec.frames, 'frames copied in 5 s').toBeGreaterThan(100);
	results.video = { name, bytes: buf.length, ...meta, frames: rec.frames, videoType };
	await expect(page.locator('#btn-record')).not.toHaveClass(/recording/);
	await h.expectNoUnexplainedErrors(log);
});

test('GIF recording: a valid animated GIF of the simulation area', async ({ page }) => {
	test.setTimeout(180_000);
	const log = h.collect(page);
	await h.openGame(page);
	await busyScene(page);
	const { name, buf } = await recordVia(page, { format: 'gif', area: 'sim', scale: 1, seconds: 5 }, 'gif');
	expect(name).toMatch(/\.gif$/);
	const info = gifInfo(buf);
	results.gif = { name, bytes: buf.length, ...info };
	expect(info.sig).toBe('GIF89a');
	expect(info.complete).toBe(true);
	expect([info.width, info.height]).toEqual([612, 384]);
	expect(info.frames, '20 fps for 5 s').toBeGreaterThanOrEqual(60);
	expect(info.frames).toBeLessThanOrEqual(110);
	// the game kept running while it recorded
	expect(await page.evaluate(() => window.tptPage.state)).toBe('running');
	await h.expectNoUnexplainedErrors(log);
});

test('screenshot of the whole window, stopping a recording early', async ({ page }) => {
	test.setTimeout(180_000);
	const log = h.collect(page);
	await h.openGame(page);
	const shot = await recordVia(page, { format: 'video', area: 'window', scale: 1, screenshot: true, seconds: 5 }, 'shot');
	expect(shot.name).toMatch(/\.png$/);
	expect(shot.buf.subarray(1, 4).toString('latin1')).toBe('PNG');
	expect([shot.buf.readUInt32BE(16), shot.buf.readUInt32BE(20)]).toEqual([h.GAME_W, h.GAME_H]);
	results.screenshot = { name: shot.name, bytes: shot.buf.length };

	// "until I stop it", stopped with the header button after ~2 s
	const download = page.waitForEvent('download', { timeout: 120_000 });
	await page.click('#btn-record');
	await page.check('#record-dialog input[name=format][value=gif]');
	await page.selectOption('#record-dialog select[name=seconds]', '0');
	await page.selectOption('#record-dialog select[name=scale]', '1');
	await page.click('#record-start');
	await page.waitForTimeout(2000);
	await page.click('#btn-record'); // Stop
	const d = await download;
	const file = `${h.ARTIFACTS}/stopped-${d.suggestedFilename()}`;
	await d.saveAs(file);
	const info = gifInfo(fs.readFileSync(file));
	results.stopped = info;
	expect(info.frames).toBeGreaterThanOrEqual(20);
	expect(info.frames).toBeLessThanOrEqual(80);
	// recording no longer copies frames
	expect(await page.evaluate(() => !!window.tptPage.module.tptOnFrame)).toBe(false);
	await h.expectNoUnexplainedErrors(log);
});
