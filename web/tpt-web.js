// Page glue for the WebAssembly build of Cracker1000's Mod.
// Loads powder.js / powder.wasm (Emscripten, MODULARIZE=1), fits the canvas to the window, and
// provides save import/export. The game itself (simulation, UI, Lua) is all inside powder.wasm.
'use strict';

(() => {
	const DATA_DIR = '/libsdl/The Powder Toy'; // SDL_GetPrefPath(NULL, "The Powder Toy") on Emscripten
	const $ = (id) => document.getElementById(id);
	const canvas = $('canvas');
	const game = $('game');
	const statusBox = $('status');
	const statusText = $('status-text');
	const statusProgress = $('status-progress');
	const recentErrors = [];
	let mod = null;
	let failed = false;
	let buildInfo = {};

	window.tptPage = { state: 'loading', timings: { start: performance.now() } };

	function setStatus(text, value, max) {
		statusText.textContent = text;
		if (max) {
			statusProgress.max = max;
			statusProgress.value = Math.min(value, max);
		} else {
			statusProgress.removeAttribute('value');
		}
	}

	function fail(message, details) {
		if (failed) {
			return;
		}
		failed = true;
		window.tptPage.state = 'failed';
		window.tptPage.failure = { message, details: String(details || '') };
		statusBox.hidden = true;
		$('failure-message').textContent = message;
		const tail = recentErrors.length ? '\n\nRecent messages:\n' + recentErrors.join('\n') : '';
		$('failure-details').textContent = String(details || '(none)') + tail;
		$('failure').hidden = false;
		for (const id of ['btn-import', 'btn-export', 'btn-fullscreen', 'btn-elements', 'btn-record', 'btn-challenges', 'btn-sound', 'btn-rewind', 'btn-world', 'btn-turbo', 'btn-effects']) {
			$(id).disabled = true;
		}
	}

	function rememberError(text) {
		recentErrors.push(text);
		if (recentErrors.length > 30) {
			recentErrors.shift();
		}
	}

	// ------------------------------------------------------------------ requirements
	function checkSupport() {
		if (typeof WebAssembly !== 'object') {
			return 'This browser does not support WebAssembly.';
		}
		// The game's main loop and its modal dialogs block; JSPI lets them yield to the browser.
		if (typeof WebAssembly.Suspending !== 'function' || typeof WebAssembly.promising !== 'function') {
			return 'This build needs WebAssembly JavaScript Promise Integration (JSPI), which this browser ' +
				'does not provide. Use a current Microsoft Edge or Google Chrome (version 137 or newer).';
		}
		return null;
	}

	// ------------------------------------------------------------------ canvas sizing
	function fit() {
		const w = canvas.width || 646;
		const h = canvas.height || 424;
		const box = game.getBoundingClientRect();
		if (!box.width || !box.height) {
			return;
		}
		let s = Math.min(box.width / w, box.height / h);
		if (s >= 2) {
			s = Math.floor(s); // whole-pixel scaling when there is room for it
		}
		canvas.style.width = Math.floor(w * s) + 'px';
		canvas.style.height = Math.floor(h * s) + 'px';
		canvas.style.imageRendering = s < 1 ? 'auto' : '';
	}
	new ResizeObserver(fit).observe(game);
	// the game resizes the canvas itself when its scale option changes
	new MutationObserver(fit).observe(canvas, { attributes: true, attributeFilter: ['width', 'height'] });
	window.addEventListener('resize', fit);

	canvas.addEventListener('contextmenu', (e) => e.preventDefault()); // right mouse button is a game tool
	canvas.addEventListener('wheel', (e) => e.preventDefault(), { passive: false });
	canvas.addEventListener('mousedown', () => canvas.focus());

	// Page buttons must not keep keyboard focus, or Space/Enter meant for the game re-trigger them.
	function refocus() {
		if (document.activeElement && document.activeElement !== document.body) {
			document.activeElement.blur();
		}
		canvas.focus();
	}

	// ------------------------------------------------------------------ files
	function mkdirp(path) {
		let cur = '';
		for (const part of path.split('/').filter(Boolean)) {
			cur += '/' + part;
			try {
				mod.FS.mkdir(cur);
			} catch (e) {
				// exists
			}
		}
	}

	function exists(path) {
		try {
			mod.FS.stat(path);
			return true;
		} catch (e) {
			return false;
		}
	}

	function uniquePath(dir, name) {
		const dot = name.lastIndexOf('.');
		const base = dot > 0 ? name.slice(0, dot) : name;
		const ext = dot > 0 ? name.slice(dot) : '';
		let candidate = `${dir}/${name}`;
		for (let i = 2; exists(candidate); i++) {
			candidate = `${dir}/${base} (${i})${ext}`;
		}
		return candidate;
	}

	async function importFile(file) {
		const bytes = new Uint8Array(await file.arrayBuffer());
		// keep the file name readable in the game's own file browser; strip path separators
		const name = file.name.replace(/[\\/]/g, '_') || 'imported.cps';
		let path;
		if (/\.(cps|stm)$/i.test(name)) {
			mkdirp(`${DATA_DIR}/Saves`);
			path = uniquePath(`${DATA_DIR}/Saves`, name);
		} else {
			// not a save: let the game show its own "not a TPT save file" error, keep nothing
			mkdirp('/tmp/import');
			path = `/tmp/import/${name}`;
		}
		mod.FS.writeFile(path, bytes);
		mod.ccall('tpt_open_file', null, ['string'], [path]);
		window.tptPage.lastImport = path;
		return path;
	}

	function listSaves() {
		const files = [];
		for (const sub of ['Saves', 'stamps']) {
			const dir = `${DATA_DIR}/${sub}`;
			let names = [];
			try {
				names = mod.FS.readdir(dir);
			} catch (e) {
				continue;
			}
			for (const name of names) {
				if (!/\.(cps|stm)$/i.test(name)) {
					continue;
				}
				const st = mod.FS.stat(`${dir}/${name}`);
				if (mod.FS.isFile(st.mode)) {
					files.push({ kind: sub === 'Saves' ? 'save' : 'stamp', name, path: `${dir}/${name}`, size: st.size, mtime: st.mtime });
				}
			}
		}
		return files;
	}

	function download(path, name) {
		const data = mod.FS.readFile(path);
		const url = URL.createObjectURL(new Blob([data], { type: 'application/octet-stream' }));
		const a = document.createElement('a');
		a.href = url;
		a.download = name;
		document.body.append(a);
		a.click();
		a.remove();
		setTimeout(() => URL.revokeObjectURL(url), 10000);
	}

	// "My saves": every save and stamp in the browser's storage, with thumbnails drawn by the game itself
	// (tpt_save_thumbnail renders a save like the game's own save browser does).
	const gallery = { kind: 'save', thumbs: new Map(), queue: [], rendering: false };
	window.tptPage.gallery = gallery;
	const THUMB_W = 204, THUMB_H = 128;
	const GALLERY_HINT = $('export-dialog').querySelector('.hint').textContent;

	function formatDate(d) {
		return d.toLocaleString(undefined, { year: 'numeric', month: 'short', day: 'numeric', hour: '2-digit', minute: '2-digit' });
	}

	// thumbnails are drawn one at a time between game frames, so a long list never freezes the game
	function pumpThumbnails() {
		if (gallery.rendering) {
			return;
		}
		const job = gallery.queue.shift();
		if (!job) {
			return;
		}
		gallery.rendering = true;
		setTimeout(() => {
			let url = null;
			try {
				const out = '/tmp/tpt-thumb.png';
				if (mod.ccall('tpt_save_thumbnail', 'number', ['string', 'string', 'number', 'number'], [job.file.path, out, THUMB_W, THUMB_H])) {
					url = URL.createObjectURL(new Blob([mod.FS.readFile(out)], { type: 'image/png' }));
					mod.FS.unlink(out);
				}
			} catch (e) {
				rememberError('thumbnail: ' + e);
			}
			gallery.thumbs.set(job.key, url);
			job.waiting.forEach((done) => done(url));
			gallery.rendering = false;
			pumpThumbnails();
		}, 0);
	}

	function thumbnailFor(file, img) {
		const key = `${file.path}|${+file.mtime}|${file.size}`;
		if (gallery.thumbs.has(key)) {
			const url = gallery.thumbs.get(key);
			url ? (img.src = url) : img.classList.add('broken');
			return;
		}
		const done = (url) => (url ? (img.src = url) : img.classList.add('broken'));
		const queued = gallery.queue.find((job) => job.key === key);
		if (queued) {
			queued.waiting.push(done); // the list was redrawn before this one was drawn
			return;
		}
		gallery.queue.push({ key, file, waiting: [done] });
		pumpThumbnails();
	}

	function sortedFiles() {
		const query = $('saves-search').value.trim().toLowerCase();
		const files = listSaves().filter((f) => f.kind === gallery.kind && (!query || f.name.toLowerCase().includes(query)));
		const by = $('saves-sort').value;
		const cmp = {
			newest: (a, b) => b.mtime - a.mtime,
			oldest: (a, b) => a.mtime - b.mtime,
			name: (a, b) => a.name.localeCompare(b.name, undefined, { numeric: true, sensitivity: 'base' }),
			size: (a, b) => b.size - a.size,
		}[by];
		return files.sort(cmp);
	}

	function cardButton(text, action, handler) {
		const b = document.createElement('button');
		b.type = 'button';
		b.textContent = text;
		b.dataset.action = action;
		b.addEventListener('click', handler);
		return b;
	}

	function renameFile(file, li) {
		const dot = file.name.lastIndexOf('.');
		const ext = file.name.slice(dot);
		const input = document.createElement('input');
		input.type = 'text';
		input.value = file.name.slice(0, dot);
		input.setAttribute('aria-label', 'New name');
		const name = li.querySelector('.save-name');
		name.replaceWith(input);
		input.focus();
		input.select();
		let finished = false;
		const finish = (save) => {
			if (finished) {
				return;
			}
			finished = true;
			const wanted = input.value.trim().replace(/[\\/:*?"<>|]/g, '_');
			if (save && wanted && wanted + ext !== file.name) {
				const dir = file.path.slice(0, file.path.lastIndexOf('/'));
				const target = `${dir}/${wanted}${ext}`;
				if (exists(target)) {
					setGalleryMessage(`There is already a file called ${wanted}${ext}.`);
				} else {
					mod.FS.rename(file.path, target);
				}
			}
			renderGallery();
		};
		input.addEventListener('keydown', (e) => {
			e.stopPropagation();
			if (e.key === 'Enter') {
				e.preventDefault();
				finish(true);
			} else if (e.key === 'Escape') {
				e.preventDefault();
				finish(false);
			}
		});
		input.addEventListener('blur', () => finish(true));
	}

	function confirmDelete(file, li) {
		const actions = li.querySelector('.save-actions');
		actions.textContent = '';
		const label = document.createElement('span');
		label.textContent = 'Delete it?';
		actions.append(label,
			cardButton('Delete', 'confirm-delete', () => {
				if (file.kind === 'stamp') {
					// through the game's stamp list, so the in-game stamp browser doesn't keep a missing stamp
					mod.ccall('tpt_delete_stamp', null, ['string'], [file.name.replace(/\.stm$/i, '')]);
				} else {
					mod.FS.unlink(file.path);
				}
				renderGallery();
			}),
			cardButton('Keep', 'cancel-delete', () => renderGallery()));
	}

	function setGalleryMessage(text) {
		const hint = $('export-dialog').querySelector('.hint');
		hint.textContent = text;
		hint.classList.add('message');
	}

	function renderGallery() {
		const list = $('export-list');
		list.textContent = '';
		const all = listSaves();
		for (const b of $('saves-kinds').querySelectorAll('button')) {
			b.classList.toggle('active', b.dataset.kind === gallery.kind);
			b.querySelector('span').textContent = `(${all.filter((f) => f.kind === b.dataset.kind).length})`;
		}
		const files = sortedFiles();
		if (!files.length) {
			const li = document.createElement('li');
			li.className = 'empty';
			li.textContent = $('saves-search').value.trim() ? 'Nothing matches.' :
				gallery.kind === 'save' ? 'No saves yet.' : 'No stamps yet.';
			list.append(li);
		}
		for (const f of files) {
			const li = document.createElement('li');
			li.dataset.name = f.name;
			const img = document.createElement('img');
			img.className = 'save-thumb';
			img.alt = '';
			img.width = THUMB_W;
			img.height = THUMB_H;
			thumbnailFor(f, img);
			const name = document.createElement('span');
			name.className = 'save-name';
			name.textContent = f.name.replace(/\.(cps|stm)$/i, '');
			name.title = f.name;
			const meta = document.createElement('span');
			meta.className = 'save-meta';
			meta.textContent = `${formatDate(f.mtime)} · ${(f.size / 1024).toFixed(1)} KiB`;
			const actions = document.createElement('span');
			actions.className = 'save-actions';
			actions.append(
				cardButton('Open', 'open', () => {
					$('export-dialog').close();
					mod.ccall('tpt_open_file', null, ['string'], [f.path]);
				}),
				cardButton('Download', 'download', () => download(f.path, f.name)),
				cardButton('Delete', 'delete', () => confirmDelete(f, li)));
			if (f.kind === 'save') {
				// stamps keep the names the game gave them: its stamp list refers to them by name
				actions.insertBefore(cardButton('Rename', 'rename', () => renameFile(f, li)), actions.children[1]);
			}
			li.append(img, name, meta, actions);
			list.append(li);
		}
	}

	function showExportDialog() {
		const hint = $('export-dialog').querySelector('.hint');
		hint.textContent = GALLERY_HINT;
		hint.classList.remove('message');
		renderGallery();
		$('export-dialog').showModal();
	}

	for (const b of $('saves-kinds').querySelectorAll('button')) {
		b.addEventListener('click', () => {
			gallery.kind = b.dataset.kind;
			renderGallery();
		});
	}
	$('saves-search').addEventListener('input', renderGallery);
	$('saves-search').addEventListener('keydown', (e) => e.stopPropagation());
	$('saves-sort').addEventListener('change', renderGallery);

	// ------------------------------------------------------------------ controls
	$('btn-import').addEventListener('click', () => $('file-import').click());
	$('file-import').addEventListener('change', async (e) => {
		const file = e.target.files[0];
		e.target.value = '';
		refocus();
		if (file) {
			await importFile(file);
		}
	});
	$('btn-export').addEventListener('click', showExportDialog);
	$('export-dialog').addEventListener('close', refocus);
	$('btn-fullscreen').addEventListener('click', () => {
		refocus();
		if (document.fullscreenElement) {
			document.exitFullscreen();
		} else {
			game.requestFullscreen().catch((e) => rememberError('fullscreen: ' + e));
		}
	});
	document.addEventListener('fullscreenchange', () => {
		$('btn-fullscreen').textContent = document.fullscreenElement ? 'Exit fullscreen' : 'Fullscreen';
		fit();
	});

	// dropping a save anywhere on the page opens it, like dropping it on the desktop game's window
	document.addEventListener('dragover', (e) => {
		if (mod && e.dataTransfer && [...e.dataTransfer.types].includes('Files')) {
			e.preventDefault();
		}
	});
	document.addEventListener('drop', (e) => {
		if (!mod || !e.dataTransfer || !e.dataTransfer.files.length) {
			return;
		}
		e.preventDefault();
		importFile(e.dataTransfer.files[0]);
	});

	// ------------------------------------------------------------------ element browser
	// Every menu of the game (elements, walls, tools…) as a searchable list beside the canvas. The list
	// comes from the running game (tpt_element_list), so elements added by Lua scripts appear too;
	// clicking one selects it in the game (tpt_select_tool, queued and handled by the game loop).
	const panel = $('elements');
	const search = $('element-search');
	const groupsBox = $('element-groups');
	const PANEL_KEY = 'tptElementPanel';
	const GROUPS_KEY = 'tptElementPanelOpenGroups';
	let menus = [];
	let openGroups = null;

	function storageGet(key) {
		try {
			return localStorage.getItem(key);
		} catch (e) {
			return null;
		}
	}

	function storageSet(key, value) {
		try {
			localStorage.setItem(key, value);
		} catch (e) {
			// private window or storage blocked: the panel just forgets its state
		}
	}

	function loadOpenGroups() {
		try {
			const v = JSON.parse(storageGet(GROUPS_KEY));
			return Array.isArray(v) ? new Set(v) : new Set();
		} catch (e) {
			return new Set();
		}
	}

	function readMenus() {
		try {
			return JSON.parse(mod.ccall('tpt_element_list', 'string', [], [])).menus;
		} catch (e) {
			rememberError('element list: ' + e);
			return [];
		}
	}

	function markSelection() {
		let active = ['', ''];
		try {
			active = mod.ccall('tpt_active_tools', 'string', [], []).split('\n');
		} catch (e) {
			return;
		}
		for (const chip of groupsBox.querySelectorAll('.chip')) {
			chip.classList.toggle('sel-left', chip.dataset.id === active[0]);
			chip.classList.toggle('sel-right', chip.dataset.id === active[1]);
		}
	}

	function selectTool(id, button) {
		mod.ccall('tpt_select_tool', null, ['string', 'number'], [id, button]);
		window.tptPage.lastPanelSelection = { id, button };
		setTimeout(markSelection, 100); // after the game loop has handled it
	}

	function chipFor(tool) {
		const chip = document.createElement('button');
		chip.type = 'button';
		chip.className = 'chip';
		chip.textContent = tool.name || tool.id;
		chip.title = tool.desc ? `${tool.name}: ${tool.desc}` : tool.name;
		chip.dataset.id = tool.id;
		chip.style.setProperty('--c', tool.color);
		chip.addEventListener('click', () => selectTool(tool.id, 0));
		chip.addEventListener('contextmenu', (e) => {
			e.preventDefault();
			selectTool(tool.id, 1);
			canvas.focus();
		});
		return chip;
	}

	function render() {
		const q = search.value.trim().toLowerCase();
		groupsBox.textContent = '';
		let shown = 0;
		for (const menu of menus) {
			let tools = menu.tools;
			if (q) {
				const byName = tools.filter((t) => t.name.toLowerCase().includes(q) || t.id.toLowerCase().includes(q));
				const byDesc = tools.filter((t) => !byName.includes(t) && t.desc.toLowerCase().includes(q));
				tools = byName.concat(byDesc);
			}
			if (!tools.length) {
				continue;
			}
			shown += tools.length;
			const details = document.createElement('details');
			details.dataset.menu = menu.name;
			details.open = q ? true : openGroups.has(menu.name);
			const summary = document.createElement('summary');
			summary.textContent = menu.name;
			const count = document.createElement('span');
			count.className = 'count';
			count.textContent = tools.length;
			summary.append(count);
			const chips = document.createElement('div');
			chips.className = 'chips';
			chips.append(...tools.map(chipFor));
			details.append(summary, chips);
			details.addEventListener('toggle', () => {
				if (search.value.trim()) {
					return; // opened by a search, not by the user
				}
				if (details.open) {
					openGroups.add(menu.name);
				} else {
					openGroups.delete(menu.name);
				}
				storageSet(GROUPS_KEY, JSON.stringify([...openGroups]));
			});
			groupsBox.append(details);
		}
		if (!shown) {
			const p = document.createElement('p');
			p.className = 'no-match';
			p.textContent = q ? `Nothing matches "${search.value.trim()}".` : 'No elements.';
			groupsBox.append(p);
		}
		markSelection();
	}

	function setPanel(open) {
		if (open) {
			menus = readMenus();
			if (!openGroups) {
				openGroups = loadOpenGroups();
			}
			render();
		}
		panel.hidden = !open;
		$('btn-elements').setAttribute('aria-expanded', String(open));
		$('btn-elements').classList.toggle('on', open);
		storageSet(PANEL_KEY, open ? '1' : '0');
		fit();
	}

	$('btn-elements').addEventListener('click', () => {
		setPanel(panel.hidden);
		refocus();
	});
	search.addEventListener('input', render);
	// typing in the search box must not reach the game, whose SDL key listeners are on the window
	for (const type of ['keydown', 'keyup', 'keypress']) {
		search.addEventListener(type, (e) => e.stopPropagation());
	}
	search.addEventListener('keydown', (e) => {
		if (e.key === 'Enter') {
			const first = groupsBox.querySelector('.chip');
			if (first) {
				selectTool(first.dataset.id, 0);
			}
		} else if (e.key === 'Escape') {
			search.value = '';
			render();
			refocus();
		}
	});
	// chips are buttons: give the keyboard back to the game after a click
	groupsBox.addEventListener('click', (e) => {
		if (e.target.closest('.chip')) {
			canvas.focus();
		}
	});
	window.tptPage.elementPanel = { open: setPanel, refresh: () => setPanel(!panel.hidden) };

	function initPanel() {
		$('btn-elements').disabled = false;
		const saved = storageGet(PANEL_KEY);
		setPanel(saved === null ? window.innerWidth >= 1000 : saved === '1');
	}

	// ------------------------------------------------------------------ challenges
	// The challenges are part of the game (src/lua/luascripts/challenges.lua): it builds the scenes, limits the
	// tools, checks the goals and saves the stars. The page sends it commands (tpt_lua_command, run by the game
	// loop) and gets every event back through Module.tptOnChallenge: the catalog, started, progress, hint,
	// won, lost and stopped.
	const chal = { catalog: null, current: null, events: [], category: null };
	window.tptPage.challenges = chal;
	const chalDialog = $('challenges-dialog');
	const chalBar = $('challenge-bar');
	const STAR = '\u2605';
	const NO_STAR = '\u2606';

	function luaString(text) {
		return '"' + String(text).replace(/[\\"]/g, '\\$&').replace(/[\r\n]/g, ' ') + '"';
	}

	function challengeCommand(command) {
		mod.ccall('tpt_lua_command', null, ['string'], ['challenges.command(' + luaString(command) + ')']);
	}
	chal.command = challengeCommand;

	function starText(n) {
		return STAR.repeat(n) + NO_STAR.repeat(3 - n);
	}

	function starNodes(n) {
		const got = document.createElement('span');
		got.className = 'stars';
		got.textContent = STAR.repeat(n);
		const missing = document.createElement('span');
		missing.className = 'stars-missing';
		missing.textContent = STAR.repeat(3 - n);
		const both = document.createElement('span');
		both.title = `${n} of 3 stars`;
		both.append(got, missing);
		return both;
	}

	function toolName(name) {
		const [kind, rest] = name.includes(':') ? name.split(':') : [null, name];
		return kind === 'WALL' ? rest + ' wall' : kind === 'TOOL' ? rest + ' tool' : kind === 'LIFE' ? rest + ' (Life)' : name;
	}

	function byId(id) {
		return chal.catalog && chal.catalog.challenges.find((c) => c.id === id);
	}

	function bestStars(id) {
		const p = chal.catalog && chal.catalog.progress[id];
		return p ? p.stars : 0;
	}

	function nextInCategory(id) {
		const list = chal.catalog.challenges;
		const i = list.findIndex((c) => c.id === id);
		const next = list.slice(i + 1).find((c) => c.category === list[i].category);
		return next || null;
	}

	function renderChallenges() {
		const cats = $('challenge-cats');
		const list = $('challenge-list');
		cats.textContent = '';
		list.textContent = '';
		if (!chal.catalog) {
			list.textContent = 'Loading…';
			return;
		}
		const all = chal.catalog.challenges;
		const total = all.reduce((n, c) => n + bestStars(c.id), 0);
		$('challenges-score').textContent = `${total} / ${all.length * 3} ${STAR}`;
		if (!chal.category) {
			chal.category = chal.catalog.categories[0].id;
		}
		for (const cat of chal.catalog.categories) {
			const mine = all.filter((c) => c.category === cat.id);
			if (!mine.length) {
				continue;
			}
			const b = document.createElement('button');
			b.type = 'button';
			b.dataset.category = cat.id;
			b.classList.toggle('on', cat.id === chal.category);
			const stars = document.createElement('span');
			stars.className = 'stars';
			stars.textContent = `${mine.reduce((n, c) => n + bestStars(c.id), 0)}/${mine.length * 3}`;
			b.append(cat.name, stars);
			b.addEventListener('click', () => {
				chal.category = cat.id;
				renderChallenges();
			});
			cats.append(b);
		}
		for (const c of all.filter((c) => c.category === chal.category)) {
			const li = document.createElement('li');
			li.dataset.id = c.id;
			const h = document.createElement('h3');
			const stars = starNodes(bestStars(c.id));
			const diff = document.createElement('span');
			diff.className = 'diff';
			diff.textContent = 'difficulty ' + '\u25cf'.repeat(c.difficulty) + '\u25cb'.repeat(5 - c.difficulty);
			h.append(c.title, stars, diff);
			const play = document.createElement('button');
			play.type = 'button';
			play.className = 'primary';
			play.textContent = 'Play';
			play.addEventListener('click', () => {
				chalDialog.close();
				challengeCommand('start ' + c.id);
				refocus();
			});
			const text = document.createElement('p');
			text.textContent = c.text;
			const meta = document.createElement('p');
			meta.className = 'meta';
			meta.textContent = `Tools: ${c.tools.map(toolName).join(', ')}` +
				(c.ink >= 0 ? ` · ink ${c.ink}` : '') + ` · ${c.time} s`;
			li.append(play, h, text, meta);
			list.append(li);
		}
	}

	function showBar(c) {
		chalBar.hidden = !c;
		if (c) {
			$('challenge-title').textContent = c.title;
			$('challenge-text').textContent = c.text;
		}
		fit();
	}

	function setStatus2(text, kind) {
		const st = $('challenge-status');
		st.textContent = text;
		st.className = kind || '';
	}

	// the game's menus change when a challenge starts or ends; the element panel shows them
	function refreshPanelSoon() {
		setTimeout(() => {
			if (!panel.hidden) {
				setPanel(true);
			}
		}, 150);
	}

	function onChallenge(text) {
		let ev;
		try {
			ev = JSON.parse(text);
		} catch (e) {
			rememberError('challenge event: ' + e);
			return;
		}
		chal.events.push(ev);
		if (chal.events.length > 200) {
			chal.events.shift();
		}
		if (ev.type === 'catalog') {
			chal.catalog = ev;
			if (Array.isArray(ev.progress)) {
				ev.progress = {}; // the Lua side sends an empty table as []
			}
			renderChallenges();
			return;
		}
		const c = ev.id ? byId(ev.id) : null;
		if (['started', 'won', 'lost', 'stopped'].includes(ev.type)) {
			refreshPanelSoon();
		}
		if (ev.type === 'started') {
			chal.current = { id: ev.id, state: 'playing' };
			showBar(c || { title: ev.id, text: '' });
			setStatus2('Playing');
			$('challenge-progress').value = 0;
			$('challenge-hint').disabled = !(c && c.hints.length);
			$('challenge-next').hidden = true;
		} else if (ev.type === 'progress') {
			$('challenge-progress').value = ev.progress;
			const secs = Math.floor(ev.frames / 60);
			setStatus2(`${Math.round(ev.progress * 100)}% · ${secs} s` + (c && c.ink >= 0 ? ` · ink ${ev.ink}/${c.ink}` : ''));
		} else if (ev.type === 'hint') {
			if (ev.hint) {
				$('challenge-text').textContent = `Hint ${ev.used}: ${ev.hint}`;
			}
			$('challenge-hint').disabled = !c || ev.used >= c.hints.length;
		} else if (ev.type === 'won') {
			chal.current = { id: ev.id, state: 'won', stars: ev.stars };
			$('challenge-progress').value = 1;
			setStatus2(`${starText(ev.stars)} Complete in ${(ev.frames / 60).toFixed(1)} s with ${ev.ink} particles`, 'won');
			if (chal.catalog && !ev.solved) {
				const best = chal.catalog.progress[ev.id];
				if (!best || ev.stars > best.stars) {
					chal.catalog.progress[ev.id] = { stars: ev.stars, frames: ev.frames, ink: ev.ink };
				}
			}
			$('challenge-hint').disabled = true;
			$('challenge-next').hidden = !(chal.catalog && nextInCategory(ev.id));
		} else if (ev.type === 'lost') {
			chal.current = { id: ev.id, state: 'lost' };
			setStatus2(ev.reason === 'time' ? 'Out of time. Try again!' : `Failed: ${ev.reason}`, 'lost');
			$('challenge-hint').disabled = true;
		} else if (ev.type === 'stopped') {
			chal.current = null;
			showBar(null);
		}
	}

	$('btn-challenges').addEventListener('click', () => {
		renderChallenges();
		challengeCommand('export'); // fresh list and stars; renders again when it arrives
		chalDialog.showModal();
	});
	chalDialog.addEventListener('close', () => refocus());
	for (const type of ['keydown', 'keyup', 'keypress']) {
		chalDialog.addEventListener(type, (e) => e.stopPropagation());
	}
	$('challenge-hint').addEventListener('click', () => {
		challengeCommand('hint');
		refocus();
	});
	$('challenge-restart').addEventListener('click', () => {
		challengeCommand('restart');
		refocus();
	});
	$('challenge-stop').addEventListener('click', () => {
		challengeCommand('stop');
		refocus();
	});
	$('challenge-next').addEventListener('click', () => {
		const next = chal.current && nextInCategory(chal.current.id);
		if (next) {
			challengeCommand('start ' + next.id);
		}
		refocus();
	});

	// ------------------------------------------------------------------ recording
	// Video (MediaRecorder) or GIF (gifenc in a worker) of the game canvas, and PNG screenshots. The game's
	// canvas is WebGL, so frames are copied from Module.tptOnFrame, which the game calls right after it
	// draws (tpt_set_frame_callback), into a 2D canvas that holds the chosen area and size.
	const SIDE_BARS = 34; // the game's side bar columns (BARSIZE + EXTRABARSIZE), right of the simulation
	const recordButton = $('btn-record');
	const recordDialog = $('record-dialog');
	const VIDEO_TYPES = ['video/mp4;codecs=avc1', 'video/webm;codecs=vp9', 'video/webm;codecs=vp8', 'video/webm'];
	const videoType = typeof MediaRecorder === 'function' ? VIDEO_TYPES.find((t) => MediaRecorder.isTypeSupported(t)) : undefined;
	let recording = null;

	$('record-video-type').textContent = videoType ? `(${videoType.startsWith('video/mp4') ? 'MP4' : 'WebM'})` : '(not supported by this browser)';

	function timestamp() {
		const d = new Date();
		const p = (n) => String(n).padStart(2, '0');
		return `${d.getFullYear()}${p(d.getMonth() + 1)}${p(d.getDate())}-${p(d.getHours())}${p(d.getMinutes())}${p(d.getSeconds())}`;
	}

	function saveBlob(blob, name) {
		const url = URL.createObjectURL(blob);
		const a = document.createElement('a');
		a.href = url;
		a.download = name;
		document.body.append(a);
		a.click();
		a.remove();
		setTimeout(() => URL.revokeObjectURL(url), 60000);
		window.tptPage.lastDownload = { name, size: blob.size, type: blob.type };
	}

	// A 2D canvas the size of the chosen area, and a function that copies the current game frame into it.
	function frameCopier(area, scale) {
		// the simulation area on screen: the world's size times its pixel size (World size setting)
		const world = window.tptPage.world;
		const simW = world.w * world.scale;
		const simH = world.h * world.scale;
		const ratio = canvas.width / (simW + SIDE_BARS); // the game's own scale option makes the canvas 2x, 3x...
		const sw = (area === 'sim' ? simW : canvas.width / ratio) * ratio;
		const sh = (area === 'sim' ? simH : canvas.height / ratio) * ratio;
		const out = document.createElement('canvas');
		out.width = Math.round(sw / ratio * scale);
		out.height = Math.round(sh / ratio * scale);
		const ctx = out.getContext('2d', { willReadFrequently: false });
		ctx.imageSmoothingEnabled = false; // keep the pixels sharp when scaling up
		// with effects on, record what is shown: the effects canvas (drawn just before, in the same frame)
		return { canvas: out, ctx, copy: () => ctx.drawImage(effects.active ? fxCanvas : canvas, 0, 0, sw, sh, 0, 0, out.width, out.height) };
	}

	// The game calls Module.tptOnFrame right after it draws a frame while tpt_set_frame_callback is on. The
	// effects draw first, so a recording copies the frame with them.
	const frameListeners = { effects: null, recorder: null };
	function updateFrameHook() {
		const on = Boolean(frameListeners.effects || frameListeners.recorder);
		mod.tptOnFrame = on ? () => {
			if (frameListeners.effects) {
				frameListeners.effects();
			}
			if (frameListeners.recorder) {
				frameListeners.recorder();
			}
		} : null;
		mod.ccall('tpt_set_frame_callback', null, ['number'], [on ? 1 : 0]);
	}

	function setFrameCallback(fn) {
		frameListeners.recorder = fn;
		updateFrameHook();
	}

	function takeScreenshot(area, scale) {
		const copier = frameCopier(area, scale);
		setFrameCallback(() => {
			setFrameCallback(null);
			copier.copy();
			copier.canvas.toBlob((blob) => saveBlob(blob, `tpt-screenshot-${timestamp()}.png`), 'image/png');
		});
	}

	// The GIF worker is built from function source so it also works on file:// pages, where module and
	// URL workers are refused; tptGifencFactory comes from vendor/gifenc.js.
	function gifWorkerMain() {
		const enc = tptGifencFactory();
		const gif = enc.GIFEncoder();
		let frames = 0;
		self.onmessage = (e) => {
			const m = e.data;
			if (m.type === 'frame') {
				const data = new Uint8ClampedArray(m.buffer);
				const palette = enc.quantize(data, 256);
				gif.writeFrame(enc.applyPalette(data, palette), m.width, m.height, { palette, delay: m.delay });
				frames++;
				self.postMessage({ type: 'progress', frames });
			} else if (m.type === 'finish') {
				gif.finish();
				const bytes = gif.bytes();
				self.postMessage({ type: 'done', bytes, frames }, [bytes.buffer]);
			}
		};
	}

	function makeGifWorker() {
		const source = `${tptGifencFactory.toString()}\n(${gifWorkerMain.toString()})();`;
		const url = URL.createObjectURL(new Blob([source], { type: 'text/javascript' }));
		const worker = new Worker(url);
		URL.revokeObjectURL(url);
		return worker;
	}

	function updateRecordButton() {
		if (!recording) {
			recordButton.textContent = 'Record…';
			recordButton.classList.remove('recording');
			return;
		}
		const secs = Math.floor((performance.now() - recording.started) / 1000);
		const label = recording.encoding ? `Encoding GIF… ${recording.encoded}/${recording.frames}` :
			`Stop ${Math.floor(secs / 60)}:${String(secs % 60).padStart(2, '0')}`;
		recordButton.textContent = label;
		recordButton.classList.add('recording');
	}

	function startRecording({ format, area, scale, seconds }) {
		const copier = frameCopier(area, scale);
		const rec = { format, started: performance.now(), frames: 0, encoded: 0, encoding: false };
		recording = rec;
		window.tptPage.recording = { format, state: 'recording' };
		const finished = (blob, ext) => {
			saveBlob(blob, `tpt-recording-${timestamp()}.${ext}`);
			window.tptPage.recording = { format, state: 'done', frames: rec.frames, size: blob.size, type: blob.type };
			recording = null;
			clearInterval(rec.timer);
			updateRecordButton();
		};
		if (format === 'video') {
			const stream = copier.canvas.captureStream(60);
			const chunks = [];
			const recorder = new MediaRecorder(stream, { mimeType: videoType, videoBitsPerSecond: 8_000_000 });
			recorder.ondataavailable = (e) => e.data.size && chunks.push(e.data);
			recorder.onstop = () => finished(new Blob(chunks, { type: videoType.split(';')[0] }), videoType.startsWith('video/mp4') ? 'mp4' : 'webm');
			copier.copy(); // first frame before the recorder starts, so the video does not begin black
			recorder.start(1000);
			setFrameCallback(() => {
				copier.copy();
				rec.frames++;
			});
			rec.stop = () => {
				setFrameCallback(null);
				recorder.stop();
				stream.getTracks().forEach((t) => t.stop());
			};
		} else {
			const worker = makeGifWorker();
			const delay = 50; // 20 frames per second
			let last = -Infinity;
			worker.onmessage = (e) => {
				if (e.data.type === 'progress') {
					rec.encoded = e.data.frames;
				} else if (e.data.type === 'done') {
					worker.terminate();
					finished(new Blob([e.data.bytes], { type: 'image/gif' }), 'gif');
				}
			};
			setFrameCallback(() => {
				const now = performance.now();
				if (now - last < delay - 2) {
					return;
				}
				last = now;
				copier.copy();
				const img = copier.ctx.getImageData(0, 0, copier.canvas.width, copier.canvas.height);
				worker.postMessage({ type: 'frame', buffer: img.data.buffer, width: img.width, height: img.height, delay }, [img.data.buffer]);
				rec.frames++;
			});
			rec.stop = () => {
				setFrameCallback(null);
				rec.encoding = true;
				worker.postMessage({ type: 'finish' });
			};
		}
		rec.timer = setInterval(() => {
			updateRecordButton();
			if (seconds && !rec.stopping && performance.now() - rec.started >= seconds * 1000) {
				stopRecording();
			}
		}, 250);
		updateRecordButton();
	}

	function stopRecording() {
		if (recording && !recording.stopping) {
			recording.stopping = true;
			recording.stop();
		}
	}

	recordButton.addEventListener('click', () => {
		refocus();
		if (recording) {
			stopRecording();
		} else {
			recordDialog.showModal();
		}
	});
	recordDialog.addEventListener('close', () => {
		const f = new FormData($('record-form'));
		const opts = { format: f.get('format'), area: f.get('area'), scale: Number(f.get('scale')), seconds: Number(f.get('seconds')) };
		if (recordDialog.returnValue === 'start') {
			if (opts.format === 'video' && !videoType) {
				rememberError('recording: this browser cannot record video');
			} else {
				startRecording(opts);
			}
		} else if (recordDialog.returnValue === 'screenshot') {
			takeScreenshot(opts.area, opts.scale);
		}
		refocus();
	});
	window.tptPage.recorder = { start: startRecording, stop: stopRecording, screenshot: takeScreenshot, videoType };

	// ------------------------------------------------------------------ visual effects
	// Glow, heat haze, shockwaves and night lighting, drawn with WebGL2 on a canvas over the game's (the mouse
	// goes through it). Each frame: the game's frame as a texture, and two maps the game fills, one texel per
	// air cell (tpt_fx_fields): the light particles give off, and the air's pressure and heat. The light is
	// blurred into a glow and a wider light; one pass then bends the picture by the heat (haze) and the pressure
	// differences (shockwaves), darkens it for night, and adds the glow. Only the simulation area is changed;
	// distortion stops while a dialog is open and inside the zoom window. The simulation is not affected.
	const EFFECTS_KEY = 'tptEffects';
	const fxCanvas = $('fx-canvas');
	const effects = { glow: false, glowStrength: 0.5, haze: false, shock: false, night: false, darkness: 0.8, supported: false, active: false, frames: 0, error: '' };
	window.tptPage.effects = effects;
	try {
		const saved = JSON.parse(storageGet(EFFECTS_KEY));
		if (saved && typeof saved === 'object') {
			for (const k of ['glow', 'haze', 'shock', 'night']) {
				effects[k] = saved[k] === true;
			}
			for (const k of ['glowStrength', 'darkness']) {
				if (typeof saved[k] === 'number' && saved[k] >= 0 && saved[k] <= 1) {
					effects[k] = saved[k];
				}
			}
		}
	} catch (e) {
		// nothing saved: all off
	}
	const fx = { gl: null, programs: {}, textures: {}, targets: {}, size: [0, 0], fieldSize: [0, 0], start: performance.now() };

	const FX_VERTEX = `#version 300 es
		out vec2 uv;
		void main() {
			vec2 p = vec2(gl_VertexID == 1 ? 3.0 : -1.0, gl_VertexID == 2 ? 3.0 : -1.0);
			uv = p * 0.5 + 0.5;
			gl_Position = vec4(p, 0.0, 1.0);
		}`;
	// separable Gaussian blur, 9 taps folded into 5 with linear filtering
	const FX_BLUR = `#version 300 es
		precision mediump float;
		uniform sampler2D tex;
		uniform vec2 dir;
		in vec2 uv;
		out vec4 colour;
		void main() {
			vec4 s = texture(tex, uv) * 0.2270270270;
			s += (texture(tex, uv + dir * 1.3846153846) + texture(tex, uv - dir * 1.3846153846)) * 0.3162162162;
			s += (texture(tex, uv + dir * 3.2307692308) + texture(tex, uv - dir * 3.2307692308)) * 0.0702702703;
			colour = s;
		}`;
	const FX_COMPOSITE = `#version 300 es
		precision highp float;
		uniform sampler2D frame, sim, glow, wide, air;
		uniform vec2 canvasSize, simSize, fieldSize;
		uniform vec4 zoom;
		uniform float pixel, still, time, glowK, hazeK, shockK, nightK;
		in vec2 uv;
		out vec4 colour;
		// the game's pixels are 0xAARRGGBB: bytes B, G, R, A
		vec4 frameAt(vec2 px) { return vec4(texture(frame, px / canvasSize).bgr, 1.0); }
		vec4 simAt(vec2 px) { return vec4(texture(sim, px / simSize).bgr, 1.0); }
		void main() {
			vec2 px = vec2(uv.x, 1.0 - uv.y) * canvasSize; // pixels from the top left
			if (px.x >= simSize.x || px.y >= simSize.y) {
				colour = frameAt(px);
				return;
			}
			vec4 shown = frameAt(px);
			// text, tools and windows drawn over the simulation are left as they are
			if (shown.rgb != simAt(px).rgb) {
				colour = shown;
				return;
			}
			vec2 suv = px / simSize; // the simulation area, top left 0
			bool inZoom = zoom.z > 0.0 && px.x >= zoom.x && px.y >= zoom.y && px.x < zoom.x + zoom.z && px.y < zoom.y + zoom.w;
			vec2 offset = vec2(0.0);
			if (still < 0.5 && !inZoom) {
				vec2 t = 1.0 / fieldSize;
				// haze rises: the heat a little below makes the air shimmer here
				float heat = texture(air, suv + vec2(0.0, 1.5 * t.y)).g;
				offset += hazeK * heat * pixel * 1.8 * vec2(sin(px.y * 0.35 / pixel + time * 7.0 + sin(px.x * 0.11 / pixel)), 0.6 * cos(px.x * 0.27 / pixel + time * 5.0));
				// shockwaves: the picture bends where the pressure changes
				float pl = texture(air, suv - vec2(t.x, 0.0)).r, pr = texture(air, suv + vec2(t.x, 0.0)).r;
				float pu = texture(air, suv - vec2(0.0, t.y)).r, pd = texture(air, suv + vec2(0.0, t.y)).r;
				offset += shockK * pixel * 40.0 * vec2(pr - pl, pd - pu);
			}
			vec4 c = simAt(clamp(px + offset, vec2(0.5), simSize - 0.5));
			vec3 g = texture(glow, suv).rgb;
			if (nightK > 0.0) {
				vec3 lit = vec3(1.0 - nightK) + texture(wide, suv).rgb * 2.5 + g * 1.5;
				c.rgb *= min(lit, vec3(1.0));
			}
			// glow adds like light: never past white, and less over what already glows (it is a halo)
			float self = max(max(c.r, c.g), c.b);
			c.rgb = 1.0 - (1.0 - c.rgb) * (1.0 - min(g * glowK * (1.0 - 0.6 * self), vec3(1.0)));
			colour = vec4(c.rgb, 1.0);
		}`;

	function fxProgram(gl, fragment) {
		const program = gl.createProgram();
		for (const [type, source] of [[gl.VERTEX_SHADER, FX_VERTEX], [gl.FRAGMENT_SHADER, fragment]]) {
			const shader = gl.createShader(type);
			gl.shaderSource(shader, source);
			gl.compileShader(shader);
			if (!gl.getShaderParameter(shader, gl.COMPILE_STATUS)) {
				throw new Error(gl.getShaderInfoLog(shader));
			}
			gl.attachShader(program, shader);
		}
		gl.linkProgram(program);
		if (!gl.getProgramParameter(program, gl.LINK_STATUS)) {
			throw new Error(gl.getProgramInfoLog(program));
		}
		const uniforms = {};
		for (let k = 0; k < gl.getProgramParameter(program, gl.ACTIVE_UNIFORMS); k++) {
			const name = gl.getActiveUniform(program, k).name;
			uniforms[name] = gl.getUniformLocation(program, name);
		}
		return { program, uniforms };
	}

	function fxTexture(gl, filter) {
		const texture = gl.createTexture();
		gl.bindTexture(gl.TEXTURE_2D, texture);
		gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, filter);
		gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, filter);
		gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
		gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
		return texture;
	}

	function fxSetup() {
		const gl = fxCanvas.getContext('webgl2', { alpha: false, antialias: false, depth: false, premultipliedAlpha: false });
		if (!gl) {
			return false;
		}
		try {
			fx.programs.blur = fxProgram(gl, FX_BLUR);
			fx.programs.composite = fxProgram(gl, FX_COMPOSITE);
		} catch (e) {
			effects.error = String(e && e.message || e);
			return false;
		}
		fx.textures.frame = fxTexture(gl, gl.NEAREST);
		fx.textures.sim = fxTexture(gl, gl.NEAREST);
		for (const name of ['light', 'air', 'glow', 'wide', 'scratch']) {
			fx.textures[name] = fxTexture(gl, gl.LINEAR);
			fx.targets[name] = gl.createFramebuffer();
		}
		fx.gl = gl;
		fxCanvas.addEventListener('webglcontextlost', () => {
			effects.supported = false;
			effects.error = 'The graphics card stopped the effects (WebGL context lost).';
			setEffectsActive(false);
		});
		return true;
	}

	// blur source into target (both field-sized), step in texels
	function fxBlur(source, target, step) {
		const gl = fx.gl, p = fx.programs.blur, [w, h] = fx.fieldSize;
		gl.useProgram(p.program);
		gl.viewport(0, 0, w, h);
		gl.activeTexture(gl.TEXTURE0);
		gl.uniform1i(p.uniforms.tex, 0);
		for (const [from, to, dir] of [[source, 'scratch', [step / w, 0]], ['scratch', target, [0, step / h]]]) {
			gl.bindFramebuffer(gl.FRAMEBUFFER, fx.targets[to]);
			gl.bindTexture(gl.TEXTURE_2D, fx.textures[from]);
			gl.uniform2f(p.uniforms.dir, dir[0], dir[1]);
			gl.drawArrays(gl.TRIANGLES, 0, 3);
		}
	}

	function fxFrame() {
		const gl = fx.gl;
		if (!gl || !effects.active) {
			return;
		}
		const w = canvas.width, h = canvas.height;
		if (fx.size[0] !== w || fx.size[1] !== h) {
			fxCanvas.width = w;
			fxCanvas.height = h;
			fx.size = [w, h];
		}
		// the same size and transform as the game's canvas (it shares its grid cell)
		fxCanvas.style.width = canvas.style.width;
		fxCanvas.style.height = canvas.style.height;
		fxCanvas.style.transform = canvas.style.transform;
		fxCanvas.style.imageRendering = canvas.style.imageRendering;

		const info = mod.ccall('tpt_fx_info', 'string', [], []).split(' ').map(Number);
		const [xres, yres, simScale, cell, dialog, zoomOn, zx, zy, zw, zh] = info;
		const ratio = w / (xres * simScale + SIDE_BARS); // the game's own scale option
		const pixel = simScale * ratio;
		const cw = xres / cell, ch = yres / cell;
		const ptr = mod.ccall('tpt_fx_fields', 'number', [], []);
		const framePtr = mod.ccall('tpt_fx_frame', 'number', [], []);
		const simPtr = mod.ccall('tpt_fx_sim_frame', 'number', [], []);
		if (!simPtr) {
			return; // kept from the next frame on
		}
		const sw = xres * simScale, sh = yres * simScale;
		const fw = xres * simScale + SIDE_BARS, fh = Math.round(h / ratio); // the game's window, unscaled
		const heap = mod.HEAPU8;
		// WebGL takes no views of shared memory (the threaded build): copy those
		const view = (from, bytes) => heap.buffer instanceof ArrayBuffer ? heap.subarray(from, from + bytes) : heap.slice(from, from + bytes);
		const light = view(ptr, cw * ch * 4), air = view(ptr + cw * ch * 4, cw * ch * 4);

		gl.activeTexture(gl.TEXTURE0);
		gl.bindTexture(gl.TEXTURE_2D, fx.textures.frame);
		gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, fw, fh, 0, gl.RGBA, gl.UNSIGNED_BYTE, view(framePtr, fw * fh * 4));
		gl.bindTexture(gl.TEXTURE_2D, fx.textures.sim);
		gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, sw, sh, 0, gl.RGBA, gl.UNSIGNED_BYTE, view(simPtr, sw * sh * 4));
		const sized = fx.fieldSize[0] === cw && fx.fieldSize[1] === ch;
		fx.fieldSize = [cw, ch];
		for (const [name, data] of [['light', light], ['air', air]]) {
			gl.bindTexture(gl.TEXTURE_2D, fx.textures[name]);
			gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, cw, ch, 0, gl.RGBA, gl.UNSIGNED_BYTE, data);
		}
		if (!sized) {
			for (const name of ['glow', 'wide', 'scratch']) {
				gl.bindTexture(gl.TEXTURE_2D, fx.textures[name]);
				gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, cw, ch, 0, gl.RGBA, gl.UNSIGNED_BYTE, null);
				gl.bindFramebuffer(gl.FRAMEBUFFER, fx.targets[name]);
				gl.framebufferTexture2D(gl.FRAMEBUFFER, gl.COLOR_ATTACHMENT0, gl.TEXTURE_2D, fx.textures[name], 0);
			}
		}
		if (effects.glow || effects.night) {
			fxBlur('light', 'glow', 1);
		}
		if (effects.night) {
			fxBlur('glow', 'wide', 4);
			fxBlur('wide', 'wide', 8);
		}

		const p = fx.programs.composite;
		gl.bindFramebuffer(gl.FRAMEBUFFER, null);
		gl.viewport(0, 0, w, h);
		gl.useProgram(p.program);
		[['frame', 'frame'], ['sim', 'sim'], ['glow', 'glow'], ['wide', 'wide'], ['air', 'air']].forEach(([uniform, texture], unit) => {
			gl.activeTexture(gl.TEXTURE0 + unit);
			gl.bindTexture(gl.TEXTURE_2D, fx.textures[texture]);
			gl.uniform1i(p.uniforms[uniform], unit);
		});
		gl.uniform2f(p.uniforms.canvasSize, w, h);
		gl.uniform2f(p.uniforms.simSize, xres * pixel, yres * pixel);
		gl.uniform2f(p.uniforms.fieldSize, cw, ch);
		gl.uniform4f(p.uniforms.zoom, zx * pixel, zy * pixel, zoomOn ? zw * pixel : 0, zh * pixel);
		gl.uniform1f(p.uniforms.pixel, pixel);
		gl.uniform1f(p.uniforms.still, dialog);
		gl.uniform1f(p.uniforms.time, (performance.now() - fx.start) / 1000);
		gl.uniform1f(p.uniforms.glowK, effects.glow ? effects.glowStrength * 1.1 : 0);
		gl.uniform1f(p.uniforms.hazeK, effects.haze ? 1 : 0);
		gl.uniform1f(p.uniforms.shockK, effects.shock ? 1 : 0);
		gl.uniform1f(p.uniforms.nightK, effects.night ? effects.darkness : 0);
		gl.drawArrays(gl.TRIANGLES, 0, 3);
		fxCanvas.hidden = false;
		effects.frames++;
	}

	function setEffectsActive(on) {
		effects.active = on && effects.supported;
		if (!effects.active && mod) {
			mod.ccall('tpt_fx_off', null, [], []);
		}
		if (!effects.active) {
			fxCanvas.hidden = true; // shown again once it has drawn a frame
		}
		frameListeners.effects = effects.active ? fxFrame : null;
		if (mod) {
			updateFrameHook();
		}
	}

	function showEffects() {
		const any = effects.glow || effects.haze || effects.shock || effects.night;
		$('btn-effects').textContent = any && effects.supported ? 'Effects: on' : 'Effects…';
		const form = $('effects-form');
		for (const k of ['glow', 'haze', 'shock', 'night']) {
			form[k].checked = effects[k];
		}
		form.glowStrength.value = String(Math.round(effects.glowStrength * 100));
		form.darkness.value = String(Math.round(effects.darkness * 100));
		$('effects-status').textContent = effects.supported ? '' : 'This browser has no WebGL2, which the effects need. ' + effects.error;
	}

	function applyEffects() {
		storageSet(EFFECTS_KEY, JSON.stringify({ glow: effects.glow, glowStrength: effects.glowStrength, haze: effects.haze, shock: effects.shock, night: effects.night, darkness: effects.darkness }));
		setEffectsActive(effects.glow || effects.haze || effects.shock || effects.night);
		showEffects();
	}

	// first frame
	function effectsSetup() {
		effects.supported = fxSetup();
		$('btn-effects').disabled = false;
		applyEffects();
	}

	$('btn-effects').addEventListener('click', () => {
		showEffects();
		$('effects-dialog').showModal();
	});
	$('effects-form').addEventListener('input', () => {
		const form = $('effects-form');
		for (const k of ['glow', 'haze', 'shock', 'night']) {
			effects[k] = form[k].checked;
		}
		effects.glowStrength = Number(form.glowStrength.value) / 100;
		effects.darkness = Number(form.darkness.value) / 100;
		applyEffects();
	});
	$('effects-dialog').addEventListener('close', () => canvas.focus());

	// ------------------------------------------------------------------ turbo mode
	// Off by default. On, the game updates the particles on every core (tpt_turbo, simulation/Turbo.h): a
	// world with many particles runs faster, and things move a little differently from classic Powder Toy.
	// Only the threaded build has threads.
	const TURBO_KEY = 'tptTurbo';
	const turbo = { on: storageGet(TURBO_KEY) === '1', threads: Math.max(2, Math.min(8, navigator.hardwareConcurrency || 4)), available: false };
	window.tptPage.turbo = turbo;

	function showTurbo() {
		const b = $('btn-turbo');
		b.textContent = !turbo.available ? 'Turbo: unavailable' : turbo.on ? `Turbo: on (${turbo.threads} cores)` : 'Turbo: off';
		b.setAttribute('aria-pressed', String(turbo.on && turbo.available));
	}

	function setTurbo(on) {
		turbo.on = on;
		storageSet(TURBO_KEY, on ? '1' : '0');
		if (turbo.available && mod) {
			mod.ccall('tpt_turbo', 'number', ['number'], [on ? turbo.threads : 0]);
		}
		showTurbo();
	}

	// first frame: threads only in the threaded build
	function turboSetup() {
		turbo.available = wasmFile === 'powder-mt.wasm';
		const b = $('btn-turbo');
		b.disabled = !turbo.available;
		if (!turbo.available) {
			b.title = 'Turbo needs the threaded version of the game, which this browser or address does not run';
		}
		setTurbo(turbo.on);
	}

	$('btn-turbo').addEventListener('click', () => {
		setTurbo(!turbo.on);
		canvas.focus();
	});

	// ------------------------------------------------------------------ sound effects
	// Off by default. The game counts the particles that appear and vanish (tpt_sound_events); every 50 ms the
	// page reads the counts and plays at most one sound of each kind, louder for more, made with WebAudio from
	// filtered noise and tones (no sound files). The AudioContext only starts after a click or key press,
	// as browsers require.
	const SOUND_KEY = 'tptSound';
	const SOUND_KINDS = ['spark', 'fire', 'explosion', 'steam', 'acid'];
	const sound = {
		on: false,
		volume: 0.6,
		ctx: null,
		master: null,
		noise: null,
		timer: 0,
		last: {}, // when each kind last played (AudioContext time)
		played: Object.fromEntries(SOUND_KINDS.map((k) => [k, 0])),
		heard: Object.fromEntries(SOUND_KINDS.map((k) => [k, 0])), // event counts read from the game
	};
	window.tptPage.sound = sound;

	try {
		const saved = JSON.parse(storageGet(SOUND_KEY));
		if (saved && typeof saved === 'object') {
			sound.on = saved.on === true;
			if (typeof saved.volume === 'number' && saved.volume >= 0 && saved.volume <= 1) {
				sound.volume = saved.volume;
			}
		}
	} catch (e) {
		// nothing saved, or not readable: defaults
	}

	function saveSound() {
		storageSet(SOUND_KEY, JSON.stringify({ on: sound.on, volume: sound.volume }));
	}

	// Called from a click or key press; returns false if the browser has no WebAudio
	function startAudio() {
		if (!sound.ctx) {
			const Ctx = window.AudioContext || window.webkitAudioContext;
			if (!Ctx) {
				return false;
			}
			sound.ctx = new Ctx();
			sound.master = sound.ctx.createGain();
			sound.master.gain.value = sound.volume;
			sound.master.connect(sound.ctx.destination);
			// one second of white noise, the raw material of every sound here
			const rate = sound.ctx.sampleRate;
			sound.noise = sound.ctx.createBuffer(1, rate, rate);
			const data = sound.noise.getChannelData(0);
			for (let i = 0; i < rate; i++) {
				data[i] = Math.random() * 2 - 1;
			}
		}
		if (sound.ctx.state === 'suspended') {
			sound.ctx.resume();
		}
		return true;
	}

	// filtered noise with an attack and an exponential decay
	function noiseBurst(at, { type, freq, q = 1, duration, gain, attack = 0.005 }) {
		const ctx = sound.ctx;
		const src = ctx.createBufferSource();
		src.buffer = sound.noise;
		src.loop = true;
		const filter = ctx.createBiquadFilter();
		filter.type = type;
		filter.frequency.value = freq;
		filter.Q.value = q;
		const env = ctx.createGain();
		env.gain.setValueAtTime(0.0001, at);
		env.gain.exponentialRampToValueAtTime(gain, at + attack);
		env.gain.exponentialRampToValueAtTime(0.0001, at + duration);
		src.connect(filter).connect(env).connect(sound.master);
		src.start(at, Math.random() * 0.5);
		src.stop(at + duration + 0.05);
	}

	function tone(at, { wave, freq, endFreq = freq, duration, gain }) {
		const ctx = sound.ctx;
		const osc = ctx.createOscillator();
		osc.type = wave;
		osc.frequency.setValueAtTime(freq, at);
		osc.frequency.exponentialRampToValueAtTime(endFreq, at + duration);
		const env = ctx.createGain();
		env.gain.setValueAtTime(gain, at);
		env.gain.exponentialRampToValueAtTime(0.0001, at + duration);
		osc.connect(env).connect(sound.master);
		osc.start(at);
		osc.stop(at + duration + 0.05);
	}

	// at least this long between two sounds of a kind (seconds), and how each one sounds; level is 0..1
	const SOUNDS = {
		spark: [0.06, (at, level) => {
			noiseBurst(at, { type: 'highpass', freq: 3500, duration: 0.04, gain: 0.08 + 0.12 * level });
			tone(at, { wave: 'square', freq: 1800 + Math.random() * 600, duration: 0.025, gain: 0.03 + 0.04 * level });
		}],
		fire: [0.15, (at, level) => {
			noiseBurst(at, { type: 'lowpass', freq: 500 + 400 * level, duration: 0.25, gain: 0.05 + 0.15 * level, attack: 0.04 });
		}],
		explosion: [0.25, (at, level) => {
			noiseBurst(at, { type: 'lowpass', freq: 200 + 500 * level, duration: 0.4 + 0.6 * level, gain: 0.25 + 0.5 * level, attack: 0.008 });
			tone(at, { wave: 'sine', freq: 90, endFreq: 35, duration: 0.3 + 0.4 * level, gain: 0.2 + 0.4 * level });
		}],
		steam: [0.2, (at, level) => {
			noiseBurst(at, { type: 'bandpass', freq: 5000, q: 0.8, duration: 0.3, gain: 0.04 + 0.12 * level, attack: 0.03 });
		}],
		acid: [0.12, (at, level) => {
			for (let k = 0; k < 3; k++) {
				const t = at + Math.random() * 0.12;
				tone(t, { wave: 'sine', freq: 600 + Math.random() * 900, endFreq: 1400 + Math.random() * 900, duration: 0.03, gain: 0.03 + 0.06 * level });
			}
		}],
	};

	// Renders one sound at full level without playing it and returns its loudest sample (0..1): for the tests,
	// which can't listen. Uses an OfflineAudioContext in place of the live one while the sound is scheduled.
	sound.preview = async (kind) => {
		const offline = new OfflineAudioContext(1, 44100, 44100);
		const saved = [sound.ctx, sound.master, sound.noise];
		sound.ctx = offline;
		sound.master = offline.createGain();
		sound.master.connect(offline.destination);
		sound.noise = offline.createBuffer(1, 44100, 44100);
		sound.noise.getChannelData(0).forEach((_, i, a) => { a[i] = Math.random() * 2 - 1; });
		try {
			SOUNDS[kind][1](0.01, 1);
		} finally {
			[sound.ctx, sound.master, sound.noise] = saved;
		}
		const data = (await offline.startRendering()).getChannelData(0);
		return data.reduce((peak, v) => Math.max(peak, Math.abs(v)), 0);
	};

	function pollSound() {
		if (!mod || window.tptPage.state !== 'running') {
			return;
		}
		const counts = mod.ccall('tpt_sound_events', 'string', [], []).split(' ').map(Number);
		if (!sound.ctx || sound.ctx.state !== 'running') {
			return; // waiting for a click or key press to start the audio
		}
		const now = sound.ctx.currentTime;
		SOUND_KINDS.forEach((kind, i) => {
			const count = counts[i];
			if (!count) {
				return;
			}
			sound.heard[kind] += count;
			const [gap, play] = SOUNDS[kind];
			if (now - (sound.last[kind] || -1) < gap) {
				return;
			}
			sound.last[kind] = now;
			sound.played[kind]++;
			// 1 particle is quiet, 1000 or more is full volume
			play(now + 0.01, Math.min(1, Math.log10(1 + count) / 3));
		});
	}

	function showSound() {
		const b = $('btn-sound');
		b.textContent = sound.on ? 'Sound: on' : 'Sound: off';
		b.setAttribute('aria-pressed', String(sound.on));
		$('sound-volume').hidden = !sound.on;
		$('sound-volume').value = String(Math.round(sound.volume * 100));
	}

	function setSoundOn(on) {
		sound.on = on;
		clearInterval(sound.timer);
		sound.timer = 0;
		if (on) {
			if (mod) {
				mod.ccall('tpt_sound_events', 'string', [], []); // forget what happened while it was off
			}
			sound.timer = setInterval(pollSound, 50);
		} else if (sound.ctx) {
			sound.ctx.suspend();
		}
		saveSound();
		showSound();
	}

	$('btn-sound').addEventListener('click', () => {
		if (!sound.on && !startAudio()) {
			$('btn-sound').textContent = 'Sound: not supported';
			return;
		}
		setSoundOn(!sound.on);
		canvas.focus();
	});
	$('sound-volume').addEventListener('input', () => {
		sound.volume = Number($('sound-volume').value) / 100;
		if (sound.master) {
			sound.master.gain.value = sound.volume;
		}
		saveSound();
	});
	$('sound-volume').addEventListener('keydown', (e) => e.stopPropagation());
	// sound was left on last time: start the audio at the first click or key press on the page
	const resumeOnGesture = () => {
		if (sound.on) {
			startAudio();
		}
	};
	window.addEventListener('pointerdown', resumeOnGesture, true);
	window.addEventListener('keydown', resumeOnGesture, true);
	showSound();

	// ------------------------------------------------------------------ rewind
	// The game keeps a snapshot every half second (tpt_rewind_setup): the newest whole, older ones as
	// differences, 30 seconds at most and a memory cap. Moving the slider pauses the game and shows that moment;
	// "Resume from here" (or unpausing in the game) carries on from it and forgets everything after it.
	const REWIND_KEY = 'tptRewind';
	const REWIND_INTERVAL = 30; // frames between snapshots
	const REWIND_SECONDS = 30;
	// devices that say they have little memory get a smaller cap (navigator.deviceMemory, in GB, Chromium only)
	const REWIND_CAP_MB = navigator.deviceMemory && navigator.deviceMemory <= 4 ? 60 : 150;
	const UNDO_CAP_MB = navigator.deviceMemory && navigator.deviceMemory <= 4 ? 40 : 100;
	const rewind = { on: storageGet(REWIND_KEY) !== 'off', count: 0, bytes: 0, view: 0, timer: 0 };
	window.tptPage.rewind = rewind;

	function rewindSetup() {
		mod.ccall('tpt_rewind_setup', null, ['number', 'number', 'number', 'number'],
			[rewind.on ? 1 : 0, REWIND_INTERVAL, REWIND_SECONDS * 60 / REWIND_INTERVAL + 1, REWIND_CAP_MB]);
	}

	function rewindSeconds(steps) {
		return steps * REWIND_INTERVAL / 60;
	}

	function rewindRefresh() {
		const [count, bytes, view] = mod.ccall('tpt_rewind_info', 'string', [], []).split(' ').map(Number);
		Object.assign(rewind, { count, bytes, view });
		const slider = $('rewind-slider');
		slider.min = String(-Math.max(0, count - 1));
		slider.value = String(-view);
		slider.disabled = count < 2;
		$('rewind-time').textContent = view ? `−${rewindSeconds(view).toFixed(1)} s` : 'now';
		$('rewind-resume').disabled = !view;
		$('rewind-now').disabled = !view;
		$('rewind-on').checked = rewind.on;
		$('rewind-info').textContent = !rewind.on ? 'Off.' : count < 2 ? 'Nothing kept yet: let the game run.' :
			`${rewindSeconds(count - 1).toFixed(1)} s kept, ${(bytes / 1048576).toFixed(0)} MB.`;
	}

	function rewindShow(steps) {
		steps = Math.max(0, Math.min(steps, rewind.count - 1));
		if (mod.ccall('tpt_rewind_show', 'number', ['number'], [steps])) {
			rewind.view = steps;
		}
		rewindRefresh();
	}

	function rewindOpen(open) {
		$('rewind-bar').hidden = !open;
		$('btn-rewind').setAttribute('aria-expanded', String(open));
		clearInterval(rewind.timer);
		rewind.timer = 0;
		if (open) {
			rewindRefresh();
			rewind.timer = setInterval(rewindRefresh, 500);
		}
		fit();
	}

	$('btn-rewind').addEventListener('click', () => rewindOpen($('rewind-bar').hidden));
	$('rewind-close').addEventListener('click', () => {
		rewindOpen(false);
		canvas.focus();
	});
	$('rewind-slider').addEventListener('input', () => rewindShow(-Number($('rewind-slider').value)));
	$('rewind-slider').addEventListener('keydown', (e) => e.stopPropagation());
	$('rewind-resume').addEventListener('click', () => {
		mod.ccall('tpt_rewind_resume', null, [], []);
		rewindRefresh();
		canvas.focus();
	});
	$('rewind-now').addEventListener('click', () => {
		rewindShow(0);
		mod.ccall('tpt_rewind_resume', null, [], []);
		rewindRefresh();
		canvas.focus();
	});
	$('rewind-on').addEventListener('change', () => {
		rewind.on = $('rewind-on').checked;
		storageSet(REWIND_KEY, rewind.on ? 'on' : 'off');
		rewindSetup();
		rewindRefresh();
	});
	// Shift+Backspace: back about a second (Backspace alone is for the game's text boxes)
	window.addEventListener('keydown', (e) => {
		if (e.key !== 'Backspace' || !e.shiftKey || !mod || window.tptPage.state !== 'running') {
			return;
		}
		e.preventDefault();
		e.stopPropagation();
		if ($('rewind-bar').hidden) {
			rewindOpen(true);
		}
		rewindRefresh();
		rewindShow(rewind.view + Math.round(60 / REWIND_INTERVAL));
	}, true);

	// ------------------------------------------------------------------ touch: bar, pinch zoom, two-finger pan
	// One finger draws (SDL turns touches into mouse input). Two fingers zoom and move the view, a CSS transform
	// on the canvas; SDL maps touches through the canvas's on-screen rectangle, so a touch still lands where the
	// finger is. To keep a two-finger gesture from drawing, the first touch is held back for a moment: if a
	// second finger comes within GESTURE_WAIT ms it is a gesture and the game never sees it, else the held
	// touch is passed on unchanged.
	const GESTURE_WAIT = 70;
	const MAX_ZOOM = 4;
	const view = { scale: 1, x: 0, y: 0 };
	const gesture = { held: null, timer: 0, active: false, start: null };
	window.tptPage.touch = { view, gesture };
	const params = new URLSearchParams(location.search);
	const touchScreen = params.get('touch') === '1' || (params.get('touch') !== '0' && matchMedia('(pointer: coarse)').matches);

	function applyView() {
		canvas.style.transform = view.scale === 1 && !view.x && !view.y ? '' : `translate(${view.x}px, ${view.y}px) scale(${view.scale})`;
		document.querySelector('#touch-bar [data-touch=zoom-reset]').hidden = view.scale === 1;
	}

	function resetView() {
		Object.assign(view, { scale: 1, x: 0, y: 0 });
		applyView();
	}

	function forward(type, e) {
		const copy = new TouchEvent(type, {
			bubbles: true, cancelable: true, composed: true,
			touches: Array.from(e.touches), targetTouches: Array.from(e.targetTouches), changedTouches: Array.from(e.changedTouches),
			ctrlKey: e.ctrlKey, shiftKey: e.shiftKey, altKey: e.altKey, metaKey: e.metaKey,
		});
		copy.tptForwarded = true;
		canvas.dispatchEvent(copy);
	}

	function releaseHeld() {
		clearTimeout(gesture.timer);
		const held = gesture.held;
		gesture.held = null;
		if (held) {
			forward('touchstart', held);
		}
	}

	function twoFingers(e) {
		const [a, b] = e.touches;
		return { d: Math.hypot(a.clientX - b.clientX, a.clientY - b.clientY) || 1, x: (a.clientX + b.clientX) / 2, y: (a.clientY + b.clientY) / 2 };
	}

	// remembers the fingers and the view when two fingers are down (again)
	function startGesture(e) {
		const f = twoFingers(e);
		gesture.active = true;
		gesture.start = { d: f.d, mx: f.x, my: f.y, scale: view.scale, x0: view.x, y0: view.y };
	}

	function moveGesture(e) {
		const now = twoFingers(e), s0 = gesture.start;
		const box = game.getBoundingClientRect();
		const cx = box.left + box.width / 2, cy = box.top + box.height / 2; // the canvas is centred in #game
		const scale = Math.min(MAX_ZOOM, Math.max(1, s0.scale * now.d / s0.d));
		// the canvas point under the fingers at the start stays under them
		const px = (s0.mx - cx - s0.x0) / s0.scale, py = (s0.my - cy - s0.y0) / s0.scale;
		let x = now.x - cx - px * scale, y = now.y - cy - py * scale;
		// don't let the game move out of view
		const w = canvas.offsetWidth, hgt = canvas.offsetHeight;
		const mx = Math.max(0, (w * scale - w) / 2), my = Math.max(0, (hgt * scale - hgt) / 2);
		x = Math.min(mx, Math.max(-mx, x));
		y = Math.min(my, Math.max(-my, y));
		Object.assign(view, { scale, x, y });
		applyView();
	}

	function swallow(e) {
		e.stopImmediatePropagation();
		e.preventDefault();
	}

	game.addEventListener('touchstart', (e) => {
		if (e.tptForwarded) {
			return;
		}
		if (e.touches.length === 1 && !gesture.active) {
			swallow(e);
			gesture.held = e;
			gesture.timer = setTimeout(releaseHeld, GESTURE_WAIT);
			return;
		}
		swallow(e);
		if (gesture.held) {
			// the second finger came in time: a gesture, the first touch never reaches the game
			clearTimeout(gesture.timer);
			gesture.held = null;
		} else if (!gesture.active) {
			// the first finger was already drawing: end its stroke where it is
			forward('touchend', e);
		}
		if (e.touches.length >= 2) {
			startGesture(e);
		}
	}, { capture: true, passive: false });

	game.addEventListener('touchmove', (e) => {
		if (e.tptForwarded) {
			return;
		}
		if (gesture.active) {
			swallow(e);
			if (e.touches.length >= 2) {
				moveGesture(e);
			}
			return;
		}
		if (gesture.held) {
			releaseHeld(); // moving before the wait is over: it's a stroke, let it draw
		}
	}, { capture: true, passive: false });

	for (const type of ['touchend', 'touchcancel']) {
		game.addEventListener(type, (e) => {
			if (e.tptForwarded) {
				return;
			}
			if (gesture.active) {
				swallow(e);
				if (e.touches.length === 0) {
					gesture.active = false;
				} else if (e.touches.length >= 2) {
					startGesture(e);
				}
				return;
			}
			if (gesture.held) {
				releaseHeld(); // a quick tap: the game gets the start, then this end
			}
		}, { capture: true, passive: false });
	}

	// ------------------------------------------------------------------ tilt to pour
	// The Tilt button turns the device's tilt into the game's custom gravity (tpt_tilt): "down" is wherever the
	// device's lowest edge is, as strong as normal gravity when the device stands upright and fading to nothing
	// when it lies flat. A hard shake pushes every loose particle (tpt_shake). Off puts the old gravity back.
	const TILT_SMOOTHING = 0.3; // share of the way to the new direction each reading moves
	const SHAKE_ACCELERATION = 15; // m/s², without gravity: a deliberate shake, not a bump
	const SHAKE_PAUSE = 400; // ms between two shakes
	const tilt = { on: false, gx: 0, gy: 1, readings: 0, shakes: 0, lastShake: 0, toGame: tiltToGame };
	window.tptPage.tilt = tilt;

	// beta/gamma (degrees, as deviceorientation gives them) and the screen's rotation → the direction things fall
	// on screen, as game x (right) and y (down). The fall direction in the device's own axes (x right, y towards
	// its top) is (sin γ cos β, −sin β) for the W3C angle order; turning the screen by θ rotates it.
	function tiltToGame(beta, gamma, angle = 0) {
		const r = Math.PI / 180;
		const dx = Math.sin(gamma * r) * Math.cos(beta * r);
		const dy = -Math.sin(beta * r);
		const a = angle * r;
		const sx = dx * Math.cos(a) - dy * Math.sin(a);
		const sy = dx * Math.sin(a) + dy * Math.cos(a);
		return { x: sx, y: -sy };
	}

	function onOrientation(e) {
		if (!tilt.on || e.beta === null || e.gamma === null) {
			return;
		}
		const angle = (screen.orientation && screen.orientation.angle) || 0;
		const want = tiltToGame(e.beta, e.gamma, angle);
		tilt.gx += (want.x - tilt.gx) * TILT_SMOOTHING;
		tilt.gy += (want.y - tilt.gy) * TILT_SMOOTHING;
		tilt.readings++;
		mod.ccall('tpt_tilt', null, ['number', 'number', 'number'], [1, tilt.gx, tilt.gy]);
	}

	function onMotion(e) {
		const a = e.acceleration;
		if (!tilt.on || !a || a.x === null) {
			return;
		}
		const strength = Math.hypot(a.x, a.y, a.z || 0);
		const now = performance.now();
		if (strength >= SHAKE_ACCELERATION && now - tilt.lastShake > SHAKE_PAUSE) {
			tilt.lastShake = now;
			tilt.shakes++;
			mod.ccall('tpt_shake', 'number', ['number'], [Math.min(6, strength / 5)]);
		}
	}

	async function setTilt(b, on) {
		if (on) {
			// iOS asks the player first, and only from a tap
			for (const E of [window.DeviceOrientationEvent, window.DeviceMotionEvent]) {
				if (E && typeof E.requestPermission === 'function' && await E.requestPermission().catch(() => 'denied') !== 'granted') {
					// on a phone a title never shows: say it on the button for a moment
					b.textContent = 'No sensor access';
					setTimeout(() => { b.textContent = 'Tilt'; }, 3000);
					return;
				}
			}
			Object.assign(tilt, { on: true, gx: 0, gy: 1, readings: 0 });
			addEventListener('deviceorientation', onOrientation);
			addEventListener('devicemotion', onMotion);
			// keep the screen from turning while the device is tilted (only where the browser allows it)
			screen.orientation?.lock?.(screen.orientation.type).catch(() => {});
		} else {
			tilt.on = false;
			removeEventListener('deviceorientation', onOrientation);
			removeEventListener('devicemotion', onMotion);
			screen.orientation?.unlock?.();
			mod.ccall('tpt_tilt', null, ['number', 'number', 'number'], [0, 0, 0]);
		}
		b.setAttribute('aria-pressed', String(on));
	}

	// the bar
	const touchActions = {
		pause: () => mod.ccall('tpt_lua_command', null, ['string'], ['tpt.set_pause(1 - tpt.set_pause())']),
		eraser: (b) => {
			const on = b.getAttribute('aria-pressed') !== 'true';
			if (on) {
				touchActions.previousTool = mod.ccall('tpt_active_tools', 'string', [], []).split('\n')[0];
			}
			mod.ccall('tpt_select_tool', null, ['string', 'number'], [on ? 'DEFAULT_PT_NONE' : (touchActions.previousTool || 'DEFAULT_PT_DUST'), 0]);
			b.setAttribute('aria-pressed', String(on));
		},
		'brush-down': () => mod.ccall('tpt_lua_command', null, ['string'], ['local r = math.floor(tpt.brushx * 0.75) tpt.brushx = r tpt.brushy = r']),
		'brush-up': () => mod.ccall('tpt_lua_command', null, ['string'], ['local r = math.min(100, math.max(tpt.brushx + 1, math.ceil(tpt.brushx * 1.33))) tpt.brushx = r tpt.brushy = r']),
		undo: () => mod.ccall('tpt_undo', 'number', [], []),
		elements: () => $('btn-elements').click(),
		'zoom-reset': resetView,
		tilt: (b) => setTilt(b, b.getAttribute('aria-pressed') !== 'true'),
	};
	for (const b of document.querySelectorAll('#touch-bar button')) {
		b.addEventListener('click', () => {
			if (mod && window.tptPage.state === 'running') {
				touchActions[b.dataset.touch](b);
			}
		});
	}
	if (touchScreen) {
		$('touch-bar').hidden = false;
		document.querySelector('#touch-bar [data-touch=tilt]').hidden = !('DeviceOrientationEvent' in window);
	}

	// ------------------------------------------------------------------ loading
	// Opened straight from disk (file://): browsers refuse fetch() there, but still run <script> files,
	// so the offline bundle also carries the binary as powder-wasm.js (base64, made by
	// wasm/make-offline-zip.sh).
	function loadWasmFromScript() {
		return new Promise((resolve, reject) => {
			setStatus('Loading game from disk…');
			const script = document.createElement('script');
			script.src = 'powder-wasm.js';
			script.onload = () => {
				const b64 = window.TPT_WASM_BASE64;
				delete window.TPT_WASM_BASE64;
				if (typeof b64 !== 'string') {
					reject(new Error('powder-wasm.js did not define the game binary'));
					return;
				}
				const bin = atob(b64);
				const bytes = new Uint8Array(bin.length);
				for (let i = 0; i < bin.length; i++) {
					bytes[i] = bin.charCodeAt(i);
				}
				resolve(bytes);
			};
			script.onerror = () => reject(new Error('powder-wasm.js is missing. Opening index.html directly ' +
				'only works with the offline bundle; otherwise serve this folder over HTTP.'));
			document.head.append(script);
		});
	}

	// The threaded build (gravity on its own thread) needs SharedArrayBuffer, which browsers only give to
	// cross-origin isolated pages (the COOP/COEP headers in vercel.json). Otherwise, offline, or with
	// ?threads=0, the single-threaded build runs.
	let wasmFile = 'powder.wasm';

	function instantiateWithProgress(imports, receiveInstance) {
		const total = (wasmFile === 'powder-mt.wasm' ? buildInfo.threaded && buildInfo.threaded.wasmBytes : buildInfo.wasmBytes) || 0;
		if (location.protocol === 'file:') {
			loadWasmFromScript()
				.then((bytes) => WebAssembly.instantiate(bytes, imports))
				.then(({ instance, module }) => {
					window.tptPage.timings.instantiated = performance.now();
					receiveInstance(instance, module);
				})
				.catch((e) => fail('Could not load the game (powder.wasm).', e && e.stack || e));
			return {};
		}
		(async () => {
			const response = await fetch(wasmFile);
			if (!response.ok) {
				throw new Error(`HTTP ${response.status} ${response.statusText} while downloading ${wasmFile}`);
			}
			let source = response;
			if (response.body && typeof ReadableStream === 'function') {
				let loaded = 0;
				const reader = response.body.getReader();
				const counted = new ReadableStream({
					async pull(controller) {
						const { done, value } = await reader.read();
						if (done) {
							controller.close();
							setStatus('Compiling…');
							return;
						}
						loaded += value.length;
						setStatus(`Downloading game… ${(loaded / 1048576).toFixed(1)}` +
							(total ? ` / ${(total / 1048576).toFixed(1)} MB` : ' MB'), loaded, total);
						controller.enqueue(value);
					},
				});
				source = new Response(counted, { headers: { 'Content-Type': 'application/wasm' } });
			}
			const { instance, module } = await WebAssembly.instantiateStreaming(source, imports);
			window.tptPage.timings.instantiated = performance.now();
			receiveInstance(instance, module);
		})().catch((e) => fail('Could not download or compile the game (powder.wasm).', e && e.stack || e));
		return {};
	}

	// ------------------------------------------------------------------ world size
	// The game's world size is fixed when it starts (the "world" argument, WIDTHxHEIGHTxSCALE): from ?world= or
	// the World size setting, else the classic 612x384.
	const WORLD_KEY = 'tptWorld';
	const WORLD_MAX = { w: 1224, h: 768 }; // XRES_MAX x YRES_MAX in Config.h

	// The game's own rules (PowderToySDL.cpp, the world argument): whole cells of 4 pixels, at most 1224x768,
	// and at least the classic 612x384 on screen so the menus fit beside and below it.
	function worldProblem(w, h, scale) {
		if (!(w >= 64 && h >= 64)) {
			return 'Width and height need to be at least 64.';
		}
		if (w > WORLD_MAX.w || h > WORLD_MAX.h) {
			return `The biggest world is ${WORLD_MAX.w} × ${WORLD_MAX.h}.`;
		}
		if (!(scale >= 1 && scale <= 4)) {
			return 'Pixel size is 1× to 4×.';
		}
		if (w * scale < 612 || h * scale < 384) {
			return `On screen that is ${w * scale} × ${h * scale}; it needs at least 612 × 384 for the menus. ` +
				'Choose bigger pixels or a bigger world.';
		}
		return '';
	}

	const world = (() => {
		let text = new URLSearchParams(location.search).get('world');
		try {
			text = text || localStorage.getItem(WORLD_KEY);
		} catch (e) {
			// storage unavailable: the classic size
		}
		const m = /^(\d+)x(\d+)(?:x(\d))?$/.exec(text || '');
		if (m) {
			// as the game does: whole cells, and the classic size if the game would refuse it
			const w = Math.floor(+m[1] / 4) * 4, h = Math.floor(+m[2] / 4) * 4, scale = +(m[3] || 1);
			if (!worldProblem(w, h, scale)) {
				return { w, h, scale };
			}
		}
		return { w: 612, h: 384, scale: 1 };
	})();
	window.tptPage.world = world;

	const WORLD_PRESETS = [
		{ name: 'Tiny', w: 156, h: 96, scale: 4, note: 'pixels 4× bigger' },
		{ name: 'Small', w: 204, h: 128, scale: 3, note: 'pixels 3× bigger' },
		{ name: 'Medium', w: 308, h: 192, scale: 2, note: 'pixels 2× bigger' },
		{ name: 'Classic', w: 612, h: 384, scale: 1, note: 'the usual size' },
		{ name: 'Large', w: 916, h: 576, scale: 1, note: '2¼ times the room' },
		{ name: 'Huge', w: 1224, h: 768, scale: 1, note: '4 times the room; needs a fast computer' },
	];
	const CARRY_FILE = `${DATA_DIR}/world-carry.stm`; // the world brought along to the next start
	const CARRY_KEY = 'tptWorldCarry'; // the size it was made in, to centre it

	// Runs Lua in the game loop (it runs on the next frame) and resolves with what the expression gives, as text
	let pageLuaRuns = 0;
	function luaValue(expression) {
		const out = `/tmp/page-lua-${++pageLuaRuns}.txt`;
		mod.ccall('tpt_lua_command', null, ['string'], [`local f = io.open("${out}", "w") f:write(tostring(${expression})) f:close()`]);
		const started = performance.now();
		return new Promise((resolve, reject) => {
			(function poll() {
				try {
					const text = new TextDecoder().decode(mod.FS.readFile(out));
					mod.FS.unlink(out);
					resolve(text);
				} catch (e) {
					if (performance.now() - started > 10000) {
						reject(new Error('the game did not answer'));
					} else {
						setTimeout(poll, 20);
					}
				}
			})();
		});
	}

	function chosenWorld(form) {
		const preset = form.querySelector('input[name=preset]:checked');
		if (preset && preset.value !== 'custom') {
			return { ...WORLD_PRESETS[Number(preset.value)] };
		}
		const cell = (v) => Math.floor(Number(v) / 4) * 4;
		return { w: cell(form.w.value), h: cell(form.h.value), scale: Number(form.scale.value) };
	}

	function updateWorldSummary() {
		const form = $('world-form');
		const custom = form.querySelector('input[name=preset][value=custom]').checked;
		$('world-custom').disabled = !custom;
		const next = chosenWorld(form);
		const problem = worldProblem(next.w, next.h, next.scale);
		const summary = $('world-summary');
		summary.classList.toggle('message', Boolean(problem));
		const same = next.w === world.w && next.h === world.h && next.scale === world.scale;
		summary.textContent = problem || (same ? 'This is the size the game has now.' :
			`${next.w} × ${next.h} simulation pixels, ${next.w * next.scale} × ${next.h * next.scale} on screen.`);
		$('world-apply').disabled = Boolean(problem) || same;
	}

	function showWorldDialog() {
		const box = $('world-presets');
		if (!box.querySelector('input')) {
			WORLD_PRESETS.forEach((p, i) => {
				const label = document.createElement('label');
				label.innerHTML = `<input type="radio" name="preset" value="${i}"> ${p.name}: ${p.w} × ${p.h} <span class="hint">(${p.note})</span>`;
				box.append(label);
			});
			const label = document.createElement('label');
			label.innerHTML = '<input type="radio" name="preset" value="custom"> Custom';
			box.append(label);
			$('world-form').addEventListener('input', updateWorldSummary);
		}
		const current = WORLD_PRESETS.findIndex((p) => p.w === world.w && p.h === world.h && p.scale === world.scale);
		box.querySelector(`input[value="${current >= 0 ? current : 'custom'}"]`).checked = true;
		const form = $('world-form');
		form.w.value = world.w;
		form.h.value = world.h;
		form.scale.value = String(world.scale);
		updateWorldSummary();
		$('world-dialog').showModal();
	}

	// Saves the setting and restarts; the current world goes along as a stamp, put back by restoreCarriedWorld
	async function applyWorld(next, carry) {
		const summary = $('world-summary');
		try {
			if (carry) {
				// the whole world as a stamp, moved out of the stamps list
				const name = await luaValue('sim.saveStamp(0, 0)');
				const stamp = `${DATA_DIR}/stamps/${name}.stm`;
				mod.FS.writeFile(CARRY_FILE, mod.FS.readFile(stamp));
				mod.ccall('tpt_delete_stamp', null, ['string'], [name]);
				localStorage.setItem(CARRY_KEY, JSON.stringify({ w: world.w, h: world.h }));
			}
			localStorage.setItem(WORLD_KEY, `${next.w}x${next.h}x${next.scale}`);
			if (mod.tptSyncNow) {
				await mod.tptSyncNow();
			}
		} catch (e) {
			summary.textContent = 'Could not change the world size: ' + (e && e.message || e);
			summary.classList.add('message');
			return;
		}
		// without ?world= the new setting applies
		const url = new URL(location.href);
		url.searchParams.delete('world');
		location.replace(url.href);
	}

	// After a restart with a new size: the world brought along, centred (cut off where it doesn't fit)
	async function restoreCarriedWorld() {
		let from = null;
		try {
			from = JSON.parse(localStorage.getItem(CARRY_KEY) || 'null');
			mod.FS.stat(CARRY_FILE);
		} catch (e) {
			return;
		}
		const cell = (v) => Math.round(v / 4) * 4;
		const x = from ? cell((world.w - from.w) / 2) : 0;
		const y = from ? cell((world.h - from.h) / 2) : 0;
		// Simulation::Load places at (v + 2) / 4 whole cells, rounded towards zero: 2 less lands exactly on a
		// negative v
		const at = (v) => v < 0 ? v - 2 : v;
		try {
			const result = await luaValue(`sim.loadStamp("${CARRY_FILE}", ${at(x)}, ${at(y)})`);
			window.tptPage.worldCarried = { x, y, result };
		} finally {
			try {
				mod.FS.unlink(CARRY_FILE);
				localStorage.removeItem(CARRY_KEY);
			} catch (e) {
				// already gone
			}
		}
	}
	window.tptPage.worldSize = { presets: WORLD_PRESETS, problem: worldProblem, apply: applyWorld };

	$('btn-world').addEventListener('click', showWorldDialog);
	$('world-dialog').addEventListener('close', () => {
		if ($('world-dialog').returnValue === 'apply') {
			const form = $('world-form');
			applyWorld(chosenWorld(form), form.carry.checked);
		}
	});

	function start() {
		setStatus('Starting…');
		window.createPowderToy({
			canvas,
			arguments: [`world:${world.w}x${world.h}x${world.scale}`],
			print: (text) => console.log(text),
			printErr: (text) => {
				console.warn(text);
				rememberError(text);
			},
			instantiateWasm: instantiateWithProgress,
			onAbort: (what) => fail('The game stopped because of an internal error.', what),
			// main() has drawn its first frame: the game is visible and SDL listens for input
			tptOnFirstFrame: () => {
				if (failed) {
					return;
				}
				window.tptPage.state = 'running';
				window.tptPage.timings.firstFrame = performance.now();
				statusBox.hidden = true;
				for (const id of ['btn-import', 'btn-export', 'btn-fullscreen', 'btn-record', 'btn-challenges', 'btn-sound', 'btn-rewind', 'btn-world']) {
					$(id).disabled = false;
				}
				if (world.w !== 612 || world.h !== 384) {
					// the challenges are built for the classic world
					$('btn-challenges').disabled = true;
					$('btn-challenges').title = 'Challenges need the Classic world size (World size…)';
				}
				restoreCarriedWorld().catch((e) => rememberError('world size: ' + e));
				turboSetup();
				effectsSetup();
				rewindSetup();
				// Ctrl+Z goes back as far as this much memory of undo steps allows (thousands of steps in most
				// sessions, fewer when the whole screen keeps changing); less on devices with little memory
				mod.ccall('tpt_undo_setup', null, ['number', 'number'], [UNDO_CAP_MB, 0]);
				if (sound.on) {
					setSoundOn(true); // left on last time; the audio itself starts at the first click or key press
				}
				initPanel();
				fit();
				canvas.focus();
				cacheGameFiles([wasmFile === 'powder-mt.wasm' ? 'powder-mt.js' : 'powder.js', wasmFile]);
			},
			// challenge events from the game's Lua (challenge.notify in challenges.lua)
			tptOnChallenge: (text) => onChallenge(text),
			tptOnQuit: () => {
				window.tptPage.state = 'quit';
				statusBox.hidden = false;
				statusProgress.hidden = true;
				setStatus('The game was closed. Reload the page to start it again.');
			},
		}).then((instance) => {
			// runtime ready; main() is starting (Lua scripts load before the first frame)
			mod = instance;
			window.tptPage.module = instance;
			window.tptPage.timings.runtimeReady = performance.now();
			if (instance.tptPersistenceError) {
				rememberError('persistence: ' + instance.tptPersistenceError);
			}
			if (window.tptPage.state === 'loading') {
				window.tptPage.state = 'starting';
				setStatus('Starting the game…');
			}
		}, (e) => fail('The game failed to initialise.', e && e.stack || e));
	}

	// ---------------------------------------------------------------- installable app, offline use
	// sw.js caches this build's files; the page asks it to keep the game binary it actually loaded. Not on
	// file:// pages (browsers allow service workers only over HTTP(S)) or with ?sw=0.
	const appState = { registration: null, installPrompt: null, reloading: false };
	window.tptPage.app = appState;

	function showUpdate() {
		$('update-bar').hidden = false;
	}

	function watchForUpdates(registration) {
		if (registration.waiting && navigator.serviceWorker.controller) {
			showUpdate();
		}
		registration.addEventListener('updatefound', () => {
			const worker = registration.installing;
			worker && worker.addEventListener('statechange', () => {
				// a second build installed next to the one this page runs: offer the reload
				if (worker.state === 'installed' && navigator.serviceWorker.controller) {
					showUpdate();
				}
			});
		});
		// long sessions: look for a new deployment every hour (the browser also checks on each visit)
		setInterval(() => registration.update().catch(() => {}), 3600_000);
	}

	function registerServiceWorker() {
		if (location.protocol === 'file:' || !('serviceWorker' in navigator) ||
			new URLSearchParams(location.search).get('sw') === '0') {
			return;
		}
		navigator.serviceWorker.addEventListener('message', (event) => {
			const msg = event.data || {};
			if (msg.type === 'game-cached') {
				appState.offlineReady = msg.version;
				$('offline-note').hidden = false;
			}
		});
		navigator.serviceWorker.addEventListener('controllerchange', () => {
			// only after "Reload to update" (the very first install also takes control, without a reload)
			if (appState.reloading) {
				location.reload();
			}
		});
		navigator.serviceWorker.register('sw.js').then((registration) => {
			if (!registration) {
				return; // service workers switched off (browser policy, test harness)
			}
			appState.registration = registration;
			watchForUpdates(registration);
		}, (e) => console.warn('service worker not registered:', e));
	}

	// once the game runs: keep its files for offline use
	function cacheGameFiles(files) {
		if (!appState.registration) {
			return;
		}
		navigator.serviceWorker.ready.then((registration) => {
			registration.active && registration.active.postMessage({ type: 'cache-game', files });
		});
	}

	$('update-reload').addEventListener('click', () => {
		const waiting = appState.registration && appState.registration.waiting;
		if (!waiting) {
			location.reload();
			return;
		}
		appState.reloading = true;
		waiting.postMessage({ type: 'skip-waiting' });
	});
	$('update-later').addEventListener('click', () => { $('update-bar').hidden = true; });

	// Chromium browsers offer installing the site; show our own button for it
	window.addEventListener('beforeinstallprompt', (event) => {
		event.preventDefault();
		appState.installPrompt = event;
		$('btn-install').hidden = false;
	});
	window.addEventListener('appinstalled', () => {
		appState.installPrompt = null;
		$('btn-install').hidden = true;
	});
	$('btn-install').addEventListener('click', async () => {
		const prompt = appState.installPrompt;
		if (!prompt) {
			return;
		}
		appState.installPrompt = null;
		$('btn-install').hidden = true;
		prompt.prompt();
		await prompt.userChoice.catch(() => {});
		refocus();
	});

	registerServiceWorker();

	window.addEventListener('error', (e) => {
		if (!mod) {
			fail('A script error stopped the game from loading.', e.message);
		}
	});

	const unsupported = checkSupport();
	if (unsupported) {
		fail(unsupported, `User agent: ${navigator.userAgent}`);
		return;
	}

	// build-info.json is optional (download size for the progress bar); fetch() cannot read file:// URLs
	const buildInfoRequest = location.protocol === 'file:' ? Promise.resolve({})
		: fetch('build-info.json').then((r) => r.ok ? r.json() : {}).catch(() => ({}));
	buildInfoRequest.then((info) => {
		buildInfo = info || {};
		window.tptPage.buildInfo = buildInfo;
		const threads = !!(buildInfo.threaded && self.crossOriginIsolated && typeof SharedArrayBuffer === 'function' &&
			new URLSearchParams(location.search).get('threads') !== '0');
		window.tptPage.threads = threads;
		wasmFile = threads ? 'powder-mt.wasm' : 'powder.wasm';
		const jsFile = threads ? 'powder-mt.js' : 'powder.js';
		const script = document.createElement('script');
		script.src = jsFile;
		script.onload = start;
		script.onerror = () => fail(`Could not download the game (${jsFile}).`, 'Check your connection and reload the page.');
		document.head.append(script);
	});
})();
