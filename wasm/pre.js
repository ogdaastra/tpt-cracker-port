// Emscripten --pre-js for the WebAssembly build: runs inside the generated module, before main().
//
// The game keeps powder.pref, Saves/, stamps/, scripts/ etc. in SDL_GetPrefPath(NULL, APPDATA),
// which SDL2's Emscripten backend maps to "/libsdl/The Powder Toy/". Mount IndexedDB there and
// load it before main() runs; autoPersist writes changes back after every file close/unlink/rename.

Module['preRun'] = [].concat(Module['preRun'] || []);
Module['preRun'].push(() => {
	FS.mkdir('/libsdl');
	const root = FS.mount(IDBFS, { autoPersist: true }, '/libsdl');
	const mount = root.mount;
	// Loading from IndexedDB creates every file locally, and with autoPersist each creation queues a
	// persist. That persist snapshots the local tree immediately, while the load is still filling it,
	// and then removes whatever it did not see from IndexedDB; if the page is closed or reloaded before
	// the follow-up persist re-adds them, those saves are gone for good (reproduced by reloading while
	// the game starts). Writes made by the load are already in IndexedDB, so ignore them: while
	// idbPersistState is neither 0 (idle) nor 'idb' (persisting), IDBFS.queuePersist does nothing.
	// (Emscripten 4.0.23 src/lib/libidbfs.js; re-check when updating emsdk.)
	mount.idbPersistState = 'loading';
	addRunDependency('tpt-idbfs-load');
	FS.syncfs(true, (error) => {
		mount.idbPersistState = 0;
		if (error) {
			// Not fatal: the game still runs, it just starts with an empty data directory.
			err('Could not load saved data from IndexedDB: ' + error);
			Module['tptPersistenceError'] = String(error);
		}
		removeRunDependency('tpt-idbfs-load');
	});
});

// Explicit flush, used by the page before reloading (e.g. platform.restart()).
Module['tptSyncNow'] = () => new Promise((resolve, reject) => {
	FS.syncfs(false, (error) => error ? reject(error) : resolve());
});
