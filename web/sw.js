// Service worker: makes the site work offline and installable as an app. Everything is cached per build
// (@COMMIT@ is replaced by wasm/assemble-dist.sh), so the page and the game binary always come from the
// same build. A new deployment brings a new sw.js; it installs next to the old one and the page offers a
// reload ("A new version is ready"), after which the old build's cache is deleted.
const VERSION = '@COMMIT@';
const CACHE = 'tpt-' + VERSION;
// the page itself; the game binary (single-threaded or threaded, whichever the page uses) is added when
// the page asks for it, so a visitor never downloads both
const SHELL = [
	'./', 'index.html', 'style.css', 'tpt-web.js', 'vendor/gifenc.js', 'build-info.json',
	'manifest.webmanifest', 'icons/icon-192.png', 'icons/icon-512.png', 'icons/icon-maskable-512.png',
	'LICENSE.txt', 'SOURCE.html', 'THIRD_PARTY_NOTICES.txt',
];

self.addEventListener('install', (event) => {
	event.waitUntil(caches.open(CACHE).then((cache) => cache.addAll(SHELL.map((u) => new Request(u, { cache: 'reload' })))));
});

self.addEventListener('activate', (event) => {
	event.waitUntil((async () => {
		for (const name of await caches.keys()) {
			if (name.startsWith('tpt-') && name !== CACHE) {
				await caches.delete(name);
			}
		}
		await self.clients.claim();
	})());
});

self.addEventListener('message', (event) => {
	const msg = event.data || {};
	if (msg.type === 'skip-waiting') {
		self.skipWaiting();
	} else if (msg.type === 'cache-game' && Array.isArray(msg.files)) {
		// the page started the game: keep its files for offline use
		event.waitUntil((async () => {
			const cache = await caches.open(CACHE);
			for (const file of msg.files) {
				if (!(await cache.match(file))) {
					try {
						await cache.add(new Request(file, { cache: 'reload' }));
					} catch (e) {
						// offline or a missing file: try again next time
					}
				}
			}
			if (event.source) {
				event.source.postMessage({ type: 'game-cached', version: VERSION, files: msg.files });
			}
		})());
	}
});

self.addEventListener('fetch', (event) => {
	const request = event.request;
	const url = new URL(request.url);
	// only this site's own files; the bundled source archives are large and rarely wanted offline
	if (request.method !== 'GET' || url.origin !== self.location.origin || url.pathname.includes('/source/') ||
		request.headers.has('range')) {
		return;
	}
	event.respondWith((async () => {
		const cache = await caches.open(CACHE);
		// navigations (with or without ?threads=0 and the like) get the cached page
		const key = request.mode === 'navigate' ? 'index.html' : request;
		const cached = await cache.match(key, { ignoreSearch: request.mode === 'navigate' });
		if (cached) {
			return cached;
		}
		const response = await fetch(request);
		// the game's files are cached as they come in (same build only: they live in this build's cache)
		if (response.ok && /\/powder(-mt)?\.(js|wasm)$/.test(url.pathname)) {
			cache.put(request, response.clone());
		}
		return response;
	})());
});
