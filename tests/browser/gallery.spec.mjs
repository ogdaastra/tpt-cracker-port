// "My saves" gallery (the page's saves dialog): thumbnails drawn by the game, search, sort, rename, delete,
// open, the stamps tab, and that all of it survives a reload (IndexedDB).
import { test, expect } from '@playwright/test';
import * as h from './helpers.mjs';

const results = {};
test.afterAll(() => h.writeArtifact('gallery.json', results));

const SAVES = h.DATA_DIR + '/Saves';
const STAMPS = h.DATA_DIR + '/stamps';

// Draws a block of one element, stamps the whole screen with the game (sim.saveStamp writes stamps/<id>.stm
// and adds it to the game's stamp list) and returns the stamp id and the particle count.
async function stampScene(page, element, w, hgt) {
	const [id, count] = (await h.luaFile(page, `
		sim.clearSim()
		for x = 100, ${100 + w - 1} do for y = 150, ${150 + hgt - 1} do sim.partCreate(-1, x, y, elem.DEFAULT_PT_${element}) end end
		local n = 0
		for i in sim.parts() do n = n + 1 end
		return sim.saveStamp(0, 0) .. " " .. n`)).split(' ');
	return { id, count: Number(count) };
}

// Saves and stamps share the game's save format, so a stamp file copied into Saves/ is a local save.
function copyFile(page, from, to, mtimeMs) {
	return page.evaluate(([a, b, t]) => {
		const FS = window.tptPage.module.FS;
		FS.writeFile(b, FS.readFile(a));
		FS.utime(b, t, t);
	}, [from, to, mtimeMs]);
}

const cards = (page) => page.locator('#export-list li[data-name]');
const cardNames = (page) => cards(page).evaluateAll((lis) => lis.map((li) => li.dataset.name));

// Waits until every thumbnail in the list has either loaded or been marked broken; returns the counts.
async function thumbnailsSettled(page) {
	await page.waitForFunction(() => [...document.querySelectorAll('#export-list img.save-thumb')]
		.every((img) => img.classList.contains('broken') || (img.complete && img.naturalWidth > 0)), null, { timeout: 30_000 });
	return page.locator('#export-list img.save-thumb').evaluateAll((imgs) => ({
		loaded: imgs.filter((i) => i.naturalWidth > 0).length,
		broken: imgs.filter((i) => i.classList.contains('broken')).length,
		size: imgs[0] ? [imgs[0].naturalWidth, imgs[0].naturalHeight] : null,
	}));
}

async function openGallery(page) {
	await page.click('#btn-export');
	await expect(page.locator('#export-dialog')).toBeVisible();
}

test('My saves: thumbnails, search, sort, rename, delete and open, kept across reload', async ({ page }) => {
	test.setTimeout(180_000);
	const log = h.collect(page);
	await h.openGame(page);
	await page.keyboard.press('Space'); // pause, so the scenes stay as drawn

	const rubr = await stampScene(page, 'RUBR', 120, 30);
	const watr = await stampScene(page, 'WATR', 40, 20);
	const now = Date.now();
	await copyFile(page, `${STAMPS}/${rubr.id}.stm`, `${SAVES}/alpha.cps`, now - 3 * 3600_000);
	await copyFile(page, `${STAMPS}/${watr.id}.stm`, `${SAVES}/beta.cps`, now - 1 * 3600_000);
	await copyFile(page, `${STAMPS}/${rubr.id}.stm`, `${SAVES}/save 10.cps`, now - 2 * 3600_000);
	await copyFile(page, `${STAMPS}/${rubr.id}.stm`, `${SAVES}/save 9.cps`, now - 4 * 3600_000);
	await h.lua(page, 'sim.clearSim()');

	await openGallery(page);
	await expect(cards(page)).toHaveCount(4);
	await expect(page.locator('#saves-kinds button[data-kind=save] span')).toHaveText('(4)');
	const thumbs = await thumbnailsSettled(page);
	expect(thumbs.broken, 'thumbnails that failed to render').toBe(0);
	expect(thumbs.loaded).toBe(4);
	// names are shown without the extension, full name in the tooltip
	await expect(page.locator('#export-list li[data-name="alpha.cps"] .save-name')).toHaveText('alpha');
	await expect(page.locator('#export-list li[data-name="alpha.cps"] .save-name')).toHaveAttribute('title', 'alpha.cps');
	await h.saveScreenshot(page, '07-gallery');

	// the thumbnail is the game's own rendering of the save: the RUBR save is mostly RUBR's colour
	const rubrPixels = await page.locator('#export-list li[data-name="alpha.cps"] img').evaluate(async (img) => {
		const c = document.createElement('canvas');
		c.width = img.naturalWidth;
		c.height = img.naturalHeight;
		const g = c.getContext('2d');
		g.drawImage(img, 0, 0);
		const d = g.getImageData(0, 0, c.width, c.height).data;
		let lit = 0;
		for (let i = 0; i < d.length; i += 4) if (d[i] + d[i + 1] + d[i + 2] > 60) lit++;
		return lit;
	});
	expect(rubrPixels, 'non-black pixels in the RUBR thumbnail').toBeGreaterThan(20);

	// sort orders (the default is newest first)
	expect(await cardNames(page)).toEqual(['beta.cps', 'save 10.cps', 'alpha.cps', 'save 9.cps']);
	await page.selectOption('#saves-sort', 'oldest');
	expect(await cardNames(page)).toEqual(['save 9.cps', 'alpha.cps', 'save 10.cps', 'beta.cps']);
	await page.selectOption('#saves-sort', 'name'); // numbers in names sort as numbers
	expect(await cardNames(page)).toEqual(['alpha.cps', 'beta.cps', 'save 9.cps', 'save 10.cps']);
	await page.selectOption('#saves-sort', 'size');
	expect((await cardNames(page)).at(-1), 'the small WATR save is last by size').toBe('beta.cps');

	// search
	await page.fill('#saves-search', 'SAVE');
	await expect(cards(page)).toHaveCount(2);
	expect((await cardNames(page)).sort()).toEqual(['save 10.cps', 'save 9.cps']);
	await page.fill('#saves-search', 'nothing like this');
	await expect(cards(page)).toHaveCount(0);
	await expect(page.locator('#export-list li.empty')).toHaveText('Nothing matches.');
	await page.fill('#saves-search', '');
	await expect(cards(page)).toHaveCount(4);

	// rename (inline, Enter keeps it; the dialog must stay open)
	await page.locator('#export-list li[data-name="beta.cps"] button[data-action=rename]').click();
	const input = page.locator('#export-list input[type=text]');
	await expect(input).toHaveValue('beta');
	await input.fill('gamma');
	await input.press('Enter');
	await expect(page.locator('#export-list li[data-name="gamma.cps"]')).toBeVisible();
	await expect(page.locator('#export-dialog')).toBeVisible();
	// Escape cancels a rename without closing the dialog
	await page.locator('#export-list li[data-name="gamma.cps"] button[data-action=rename]').click();
	await input.fill('delta');
	await input.press('Escape');
	await expect(page.locator('#export-dialog')).toBeVisible();
	await expect(page.locator('#export-list li[data-name="gamma.cps"]')).toBeVisible();
	// renaming onto an existing name is refused
	await page.locator('#export-list li[data-name="gamma.cps"] button[data-action=rename]').click();
	await input.fill('alpha');
	await input.press('Enter');
	await expect(page.locator('#export-dialog .hint')).toHaveText('There is already a file called alpha.cps.');
	await expect(page.locator('#export-list li[data-name="gamma.cps"]')).toBeVisible();

	// delete asks first; Keep leaves it, Delete removes the file
	await page.locator('#export-list li[data-name="save 9.cps"] button[data-action=delete]').click();
	await page.locator('#export-list li[data-name="save 9.cps"] button[data-action=cancel-delete]').click();
	await expect(cards(page)).toHaveCount(4);
	await page.locator('#export-list li[data-name="save 9.cps"] button[data-action=delete]').click();
	await expect(page.locator('#export-list li[data-name="save 9.cps"]')).toContainText('Delete it?');
	await page.locator('#export-list li[data-name="save 9.cps"] button[data-action=confirm-delete]').click();
	await expect(cards(page)).toHaveCount(3);
	expect(await page.evaluate((d) => window.tptPage.module.FS.readdir(d), SAVES)).not.toContain('save 9.cps');

	// reload: the rename and the delete were stored
	await page.waitForTimeout(500); // autoPersist runs after the change
	await page.locator('#export-dialog button[value=close]').click();
	await page.reload();
	await h.openGame(page);
	const after = await page.evaluate((d) => window.tptPage.module.FS.readdir(d), SAVES);
	expect(after).toEqual(expect.arrayContaining(['alpha.cps', 'gamma.cps', 'save 10.cps']));
	expect(after).not.toContain('beta.cps');
	expect(after).not.toContain('save 9.cps');

	// open loads the save into the game and closes the dialog
	await openGallery(page);
	await page.locator('#export-list li[data-name="alpha.cps"] button[data-action=open]').click();
	await expect(page.locator('#export-dialog')).toBeHidden();
	await h.waitFrames(page, 30);
	expect(Number(await h.lua(page, 'sim.elementCount(elem.DEFAULT_PT_RUBR)'))).toBe(rubr.count);

	results.saves = { thumbs, rubrParticles: rubr.count, watrParticles: watr.count, afterReload: after };
	await h.expectNoUnexplainedErrors(log);
});

test('My saves: the stamps tab lists stamps and deletes them through the game', async ({ page }) => {
	test.setTimeout(120_000);
	const log = h.collect(page);
	await h.openGame(page);
	await page.keyboard.press('Space');
	const a = await stampScene(page, 'DUST', 30, 30);
	await page.waitForTimeout(1100); // stamp ids carry the time in seconds
	const b = await stampScene(page, 'STNE', 50, 10);

	await openGallery(page);
	await page.locator('#saves-kinds button[data-kind=stamp]').click();
	await expect(page.locator('#saves-kinds button[data-kind=stamp]')).toHaveClass(/active/);
	await expect(cards(page)).toHaveCount(2);
	await expect(page.locator('#saves-kinds button[data-kind=stamp] span')).toHaveText('(2)');
	expect((await thumbnailsSettled(page)).broken).toBe(0);
	// stamps keep the names the game gave them
	await expect(page.locator('#export-list button[data-action=rename]')).toHaveCount(0);
	await h.saveScreenshot(page, '07-gallery-stamps');

	await page.locator(`#export-list li[data-name="${a.id}.stm"] button[data-action=delete]`).click();
	await page.locator(`#export-list li[data-name="${a.id}.stm"] button[data-action=confirm-delete]`).click();
	await expect(cards(page)).toHaveCount(1);
	const state = await page.evaluate((d) => {
		const FS = window.tptPage.module.FS;
		return { files: FS.readdir(d), list: new TextDecoder().decode(FS.readFile(d + '/stamps.def')) };
	}, STAMPS);
	expect(state.files).not.toContain(`${a.id}.stm`);
	expect(state.list, "the game's stamp list").not.toContain(a.id);
	expect(state.list).toContain(b.id);

	results.stamps = { deleted: a.id, kept: b.id };
	await h.expectNoUnexplainedErrors(log);
});

test('My saves: twenty thumbnails render without holding up the game', async ({ page }) => {
	test.setTimeout(120_000);
	const log = h.collect(page);
	await h.openGame(page);
	await page.keyboard.press('Space');
	// a busy, full-screen scene: the slowest kind of save to draw
	const busy = (await h.luaFile(page, `
		sim.clearSim()
		local types = { elem.DEFAULT_PT_WATR, elem.DEFAULT_PT_SAND, elem.DEFAULT_PT_METL, elem.DEFAULT_PT_FIRE, elem.DEFAULT_PT_GLAS }
		for x = 4, 607, 2 do for y = 4, 379, 2 do sim.partCreate(-1, x, y, types[(x + y) % 5 + 1]) end end
		return sim.saveStamp(0, 0)`));
	for (let i = 0; i < 20; i++) {
		await copyFile(page, `${STAMPS}/${busy}.stm`, `${SAVES}/busy ${i}.cps`, Date.now() - i * 1000);
	}
	await h.lua(page, 'sim.clearSim()');

	// watch the gaps between animation frames while the thumbnails are drawn
	await page.evaluate(() => {
		window.__gaps = [];
		let last = performance.now();
		const tick = (t) => {
			window.__gaps.push(t - last);
			last = t;
			if (window.__gaps.length < 100000) window.__rafId = requestAnimationFrame(tick);
		};
		window.__rafId = requestAnimationFrame(tick);
	});
	const t0 = Date.now();
	await openGallery(page);
	const thumbs = await thumbnailsSettled(page);
	const ms = Date.now() - t0;
	const gaps = await page.evaluate(() => {
		cancelAnimationFrame(window.__rafId);
		return window.__gaps.slice(1);
	});
	expect(thumbs.broken).toBe(0);
	expect(thumbs.loaded).toBe(20);
	const maxGap = Math.max(...gaps);
	results.twenty = { totalMs: ms, perThumbMs: Math.round(ms / 20), maxFrameGapMs: Math.round(maxGap), frames: gaps.length };
	console.log('twenty thumbnails:', JSON.stringify(results.twenty));
	// one thumbnail at a time between frames: no single long freeze
	expect(maxGap, 'longest gap between animation frames (ms)').toBeLessThan(500);
	await h.expectNoUnexplainedErrors(log);
});
