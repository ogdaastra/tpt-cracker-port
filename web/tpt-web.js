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
		for (const id of ['btn-import', 'btn-export', 'btn-fullscreen', 'btn-elements', 'btn-record', 'btn-challenges']) {
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

	function showExportDialog() {
		const list = $('export-list');
		list.textContent = '';
		const files = listSaves();
		if (!files.length) {
			const li = document.createElement('li');
			li.textContent = 'No saves yet.';
			list.append(li);
		}
		for (const f of files) {
			const li = document.createElement('li');
			const label = document.createElement('span');
			label.textContent = `${f.name} (${f.kind}, ${(f.size / 1024).toFixed(1)} KiB)`;
			const btn = document.createElement('button');
			btn.type = 'button';
			btn.textContent = 'Download';
			btn.addEventListener('click', () => download(f.path, f.name));
			li.append(label, btn);
			list.append(li);
		}
		$('export-dialog').showModal();
	}

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
	const GAME_W = 646;
	const SIM_W = 612;
	const SIM_H = 384;
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
		const ratio = canvas.width / GAME_W; // the game's own scale option makes the canvas 2x, 3x...
		const sw = (area === 'sim' ? SIM_W : canvas.width / ratio) * ratio;
		const sh = (area === 'sim' ? SIM_H : canvas.height / ratio) * ratio;
		const out = document.createElement('canvas');
		out.width = Math.round(sw / ratio * scale);
		out.height = Math.round(sh / ratio * scale);
		const ctx = out.getContext('2d', { willReadFrequently: false });
		ctx.imageSmoothingEnabled = false; // keep the pixels sharp when scaling up
		return { canvas: out, ctx, copy: () => ctx.drawImage(canvas, 0, 0, sw, sh, 0, 0, out.width, out.height) };
	}

	function setFrameCallback(fn) {
		mod.tptOnFrame = fn;
		mod.ccall('tpt_set_frame_callback', null, ['number'], [fn ? 1 : 0]);
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

	function start() {
		setStatus('Starting…');
		window.createPowderToy({
			canvas,
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
				for (const id of ['btn-import', 'btn-export', 'btn-fullscreen', 'btn-record', 'btn-challenges']) {
					$(id).disabled = false;
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
