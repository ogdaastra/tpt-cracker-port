// Emscripten --pre-js for the WebAssembly build: runs inside the generated module, before main().
//
// The game keeps powder.pref, Saves/, stamps/, scripts/ etc. in SDL_GetPrefPath(NULL, APPDATA),
// which SDL2's Emscripten backend maps to "/libsdl/The Powder Toy/". Mount IndexedDB there and
// load it before main() runs; autoPersist writes changes back after every file close/unlink/rename.

Module['preRun'] = [].concat(Module['preRun'] || []);
Module['preRun'].push(() => {
	FS.mkdir('/libsdl');
	FS.mount(IDBFS, { autoPersist: true }, '/libsdl');
	addRunDependency('tpt-idbfs-load');
	FS.syncfs(true, (error) => {
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
