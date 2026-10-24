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
		for (const id of ['btn-import', 'btn-export', 'btn-fullscreen', 'btn-elements']) {
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

	function instantiateWithProgress(imports, receiveInstance) {
		const total = buildInfo.wasmBytes || 0;
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
			const response = await fetch('powder.wasm');
			if (!response.ok) {
				throw new Error(`HTTP ${response.status} ${response.statusText} while downloading powder.wasm`);
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
				for (const id of ['btn-import', 'btn-export', 'btn-fullscreen']) {
					$(id).disabled = false;
				}
				initPanel();
				fit();
				canvas.focus();
			},
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
		const script = document.createElement('script');
		script.src = 'powder.js';
		script.onload = start;
		script.onerror = () => fail('Could not download the game (powder.js).', 'Check your connection and reload the page.');
		document.head.append(script);
	});
})();
