# WebAssembly build of Cracker1000's Mod

The real C++ game (Cracker1000's Mod v55.1, based on The Powder Toy of January 2023) compiled with
Emscripten and running in the browser. Same simulation, UI, fonts and Lua scripts as the desktop
game; no rewrite, no streaming. `PORT_LOG.md` records how and why every change was made.

On top of the mod: 143 more elements from TPT Ultimata, a newer Cracker1000's Mod branch (DFOM, DIGS, ALUM,
ALMP, RSST, RSSS) and Jacob1's Mod, plus Ultimata's walls, Life rules, tools and the engine code they need
(circuits, vehicles, time dilation, ...). List: `ADDED_ELEMENTS.md`; how they were merged: `PORT_LOG.md` 13.

**Browser:** Microsoft Edge or Google Chrome **137 or newer** (needs WebAssembly JSPI).
Other browsers get a readable message instead of a broken page.

## Build

Linux or WSL2 with git, Python 3, Meson >= 0.61, Ninja, pkg-config, curl, zip and a host C++ compiler
(for the `toarray` resource generator):

```sh
./wasm/build-deps.sh     # installs emsdk 4.0.23 (wasm/emsdk-version.txt) and builds Lua, JsonCpp, FFTW
./wasm/build.sh          # configures build-wasm/, builds powder.js + powder.wasm, assembles dist/
```

**Threaded variant** (optional, used by the page when it is cross-origin isolated):

```sh
WASM_THREADS=1 ./wasm/build-deps.sh   # second sysroot, everything compiled with -pthread (prefix-mt/)
WASM_THREADS=1 ./wasm/build.sh        # build-wasm-mt/, then dist/ gets powder-mt.js + powder-mt.wasm too
```

The site sends `Cross-Origin-Opener-Policy: same-origin` and `Cross-Origin-Embedder-Policy: require-corp`
(`wasm/vercel.json`), which makes the page cross-origin isolated; the page then loads `powder-mt.*`, where
Newtonian gravity runs on its own thread. Without those headers, opened from disk, or with `?threads=0`,
it loads the single-threaded `powder.*`. The offline ZIP only carries the single-threaded build.

`EMSDK_DIR` picks where emsdk lives (default `.cache/emsdk`). Where `github.com/<repo>/archive/...`
downloads are blocked, `TPT_SEED_PORTS=1 ./wasm/build-deps.sh` seeds Emscripten's port cache from the
same versions via `git clone` / checksummed tarballs. The native desktop build is unchanged
(`meson setup build && ninja -C build`).

## Test

```sh
cd tests/browser && npm ci && npx playwright install chromium
npx playwright test                                   # serves dist/ on 127.0.0.1:8765
TPT_BASE_URL=https://<deployment>/ npx playwright test   # against a deployment
```

The suite drives the game through its own UI (mouse, keyboard, the in-game Lua console) and saves
screenshots and measurements to `artifacts/browser/`. `added-elements.spec.mjs` covers the added elements,
`categories.spec.mjs` the split menus and the page's element browser, `heat-pipe.spec.mjs` heat pipes, `record.spec.mjs` video/GIF recording and screenshots, `ultimata-tools.spec.mjs` Ultimata's tools and field switches, `threads.spec.mjs` the threaded build and its fallback, `upstream-fixes.spec.mjs` fixes and physics from newer TPT, `challenges.spec.mjs` challenge mode (every reference solution wins, doing nothing never does, one challenge played by hand, ink budget, locks, saved stars), `render.spec.mjs` that the display modes draw the same pixels as before the renderer was sped up, `app.spec.mjs` the installable app (manifest, offline start, update banner), `gallery.spec.mjs` the "My saves" gallery (thumbnails, search, sort, rename, delete, open, stamps, reload). `fps.spec.mjs` the speed and drawing caps in the Simulation options (set, measured, kept after reload), `sound.spec.mjs` the sound effects (off by default, every kind plays and isn't silent, volume, kept after reload), `rewind.spec.mjs` rewinding (a moment 10 s back comes back exactly, resume, back to now, Shift+Backspace, off), `touch.spec.mjs` phones and tablets (one finger draws, pinch and pan don't, taps land under the finger when zoomed, the touch bar), `undo.spec.mjs` deep undo (60 strokes undone to an empty screen and redone exactly, the memory cap), `tilt.spec.mjs` tilt to pour (direction from the sensor angles, sand pours both ways, shake, gravity restored). `TPT_BROWSER_CHANNEL=msedge` uses an installed Edge.

`PERF=1 npx playwright test perf.spec.mjs` is a benchmark (skipped otherwise): it builds fixed scenes and
times simulation and render frames inside the game (`tpt_benchmark`), and render frames in every display
mode, writing `artifacts/browser/perf.json`.
`PERF_THROTTLES=1,4` adds the whole-game tick rate with Chromium CPU throttling (much noisier).

The page talks to the game through a few exported functions (`src/PowderToySDL.cpp`): `tpt_open_file`
(imports), `tpt_element_list` / `tpt_active_tools` / `tpt_select_tool` (the Elements panel; selection is
queued as an SDL event and handled in the game loop), `tpt_set_frame_callback` (recording), `tpt_lua_command` (runs a Lua line in the game loop; the Challenges
panel sends `challenges.command(...)`, and the game answers through `Module.tptOnChallenge`), `tpt_save_thumbnail` /
`tpt_delete_stamp` (the "My saves" gallery), `tpt_sound_events` (what happened, for the sound effects), `tpt_rewind_setup` / `tpt_rewind_info` / `tpt_rewind_show` / `tpt_rewind_resume` (the Rewind bar), `tpt_undo` (the touch bar), `tpt_undo_setup` / `tpt_undo_info` (the undo history's memory cap and depth), `tpt_tilt` / `tpt_shake` (tilt to pour), `tpt_benchmark` and `tpt_rng_seed` (replays a scene exactly, for
`docs/handoff/tools/samecheck.mjs`, which proves a speed-up leaves the simulation unchanged).

## Deploy (Vercel)

`dist/` is a static site; `dist/vercel.json` sets `application/wasm`, caching and the COOP/COEP headers
the threaded build needs.

**Installable app and offline use:** `manifest.webmanifest` and `icons/` (rendered from
`resources/icon_exe.svg` by `wasm/make-icons.mjs`) make the site installable; `sw.js` caches the page and
the game binary the page actually loaded, in a cache named after the build (`assemble-dist.sh` writes the
commit into it), so a cached page never meets a binary from another build. Cached responses keep their
COOP/COEP headers, so the threaded build also runs offline. When a new build is deployed the page shows
"A new version of the game is ready" and reloads into it on request; the old cache is then deleted. Not
used on `file://` pages (the offline ZIP opened from disk) or with `?sw=0`. Tests run with service workers
blocked except `app.spec.mjs`.

```sh
VERCEL_TOKEN=... ./wasm/deploy-vercel.sh          # preview, prints the URL
VERCEL_TOKEN=... ./wasm/deploy-vercel.sh --prod   # production
```

CI (`.github/workflows/wasm.yml`) builds from scratch, validates the binary, runs the browser tests,
and, when the repository has a `VERCEL_TOKEN` secret, deploys a preview and reruns the tests against
it. Production only on a manual run with `production: true`.

## Offline

`./wasm/make-offline-zip.sh` makes `artifacts/offline/cracker1000-mod-wasm-offline.zip`: the site, a
README, launchers that need no administrator rights (`serve.py`, `serve.mjs`, `Start Game
(Windows).cmd`, `start.sh`), and the GPL source archives. Opening `index.html` directly also works.

## What differs from the desktop game

| Area | Browser build |
|---|---|
| Online save browser, login, voting, comments | unavailable (no HTTP client; requests end with "Network Not Available") |
| Multiplayer (TPTMP) | unavailable (no raw TCP in browsers); it reports this in the console |
| Mod updater (URS), game updater | unavailable; the URS check reports "URS updater: offline" |
| Lua | PUC Lua 5.1 instead of LuaJIT: same language, slower scripts, no `ffi` |
| Saves, stamps, settings, scripts | kept in the browser (IndexedDB) per site address; **My saves…** shows them with thumbnails (open, rename, download, delete); **Import save…** loads `.cps`/`.stm` files |
| Threads | the threaded build (cross-origin isolated pages) runs Newtonian gravity on its own thread; background tasks and the single-threaded fallback run on the main thread |
| Window | the page scales the canvas; the page's Fullscreen button replaces the in-game one |
| Menus | 17 px wider window: a second column of menu buttons holds five menus split off the longest ones (see `ADDED_ELEMENTS.md`); the page adds a searchable Elements panel |
| `platform.restart()` | reloads the page; `platform.openLink()` opens a new tab |
| Challenges | 50 puzzles (heat, electronics, explosives, chemistry, machines, gravity, life, characters, records) with limited tools and ink, stars kept in the browser; Challenges… on the page or the ★ button in the game. List: `docs/CHALLENGES.md` |
| Sound | NOTE and `tpt.play_sound()` play through Web Audio (an AudioContext at 44.1 kHz starts in Chromium; volume kept low) |
| World size | **World size…** on the page: small worlds drawn with 2× to 4× pixels (they fill the screen without zooming), the classic 612 × 384, or bigger up to 1224 × 768; the game restarts with the world brought along. The game takes it as the start argument `world:WxHxS`; challenges need the classic size. `PORT_LOG.md` section 35 |
| Turbo | **Turbo** on the page (threaded build): the particle update (and the work before it) on every core, about 2.5× faster in big scenes; things move a little differently from classic Powder Toy, the same on any number of cores. `PORT_LOG.md` sections 36 and 38 |
| Effects | **Effects…** on the page: glow, heat haze, shockwaves and night lighting, drawn by the graphics card (WebGL2) over the simulation; the menus and the simulation itself are unchanged. `PORT_LOG.md` section 37 |

## License

GPL-3.0 (`LICENSE`). The deployed site and the offline ZIP carry the complete corresponding source
(`source/`), the FFTW source, and the notices of all bundled libraries (`THIRD_PARTY_NOTICES.txt`).
The added elements are GPL-3.0 code from TPT Ultimata (Bowserinator), Cracker1000's Mod and Jacob1's Mod;
Eigen 3.3.7 (MPL-2.0, parts BSD/LGPL-2.1) is used by Ultimata's circuit solver (`src/eigen/COPYING.*`).
