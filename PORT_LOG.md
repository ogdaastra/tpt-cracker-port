# WebAssembly port log — Cracker1000's Mod v55.1

Running log of decisions, commands, errors/root causes, verification state and next steps.
If context resets: re-read this file first.

## 0. Inputs and layout

- Inputs (never modified, never shipped):
  - `The-Powder-Toy-master.zip` sha256 `283e3ed201d64cdb32c6cf4f8c1c5913944b4bc80243e80ddbb36a0e546c59dd`
  - `powder.exe` sha256 `aed14755af9edc72a20153a39e3a0b160cd275e39cc5021295fb32aa8ee3913b` (behavioural reference only)
  - `wasm-port-status.md` was **not supplied** in this session (only the zip and the exe were uploaded).
    Every "established fact" from the prompt is therefore re-verified against the source below.
- Layout deviation from the prompt: this session runs in a pre-created, empty git repo
  (`ogdaastra/tpt-cracker-port`) whose working branch is fixed by the session to
  `claude/festive-einstein-70qgap`. The repo root plays the role of `tpt-mod/` and that branch plays the
  role of `wasm-port`. The first commit (`3cb0ce9`) is the pristine, unmodified extraction of the zip.

## 1. Environment (section 3)

| Item | Value |
|---|---|
| OS | Ubuntu 24.04.5 LTS, x86_64, Linux 6.18 (cloud container, not Windows) |
| CPU / RAM / disk | 4 cores, 15 GiB RAM, ~30 GiB free |
| git / python3 / node / npm | 2.43.0 / 3.13.17 / 22.22.0 / 10.9.4 |
| gcc / g++ / clang / cmake / ninja / make | 13.3.0 / 13.3.0 / 18.1.3 / 3.28.3 / 1.11.1 / 4.3 |
| meson | not installed -> venv `/root/tools/venv`, **meson 1.6.1** |
| Emscripten | not installed -> emsdk at `/root/tools/emsdk`, **pinned 4.0.23** (`7a5d93b5`, clang 22) |
| wasm-validate | wabt 1.0.34 from apt |
| vercel CLI | not installed; Vercel **MCP connector is authenticated** (hobby plan, personal scope, no teams) |
| GitHub | GitHub MCP authenticated as `ogdaastra`; `gh` CLI present but GitHub access goes via MCP |
| Playwright | Playwright MCP present; Chromium + `playwright` npm preinstalled (`/opt/pw-browsers`) |

Why emsdk 4.0.23 and not the newest 6.0.13: it is the final release of the 4.0 line (2026-01-09), mature,
and its SDL2 port / IDBFS / Asyncify behaviour is well understood. Pinned in `wasm/emsdk-version.txt`.

Network notes (sandbox-specific): HTTPS goes via an egress proxy. `git clone` of public GitHub repos
works, but HTTP downloads of `github.com/<repo>/archive/...` return 403 for repos not attached to the
session. Emscripten's ports (SDL2, zlib, bzip2) normally download GitHub archives, so
`TPT_SEED_PORTS=1 ./wasm/build-deps.sh` pre-seeds Emscripten's port cache (unpacked source + the
`.emscripten_url` marker emcc checks) from the same versions: git tags pinned by commit id, or the
sha256-checked zlib.net tarball. CI does not need this and uses the normal, hash-checked downloads.
lua.org, fftw.org, zlib.net, storage.googleapis.com, registry.npmjs.org are reachable.

## 2. Facts re-verified from the source

- Meson >= 0.61, C99/C++17, RTTI off: confirmed (`meson.build`). No Emscripten support upstream:
  confirmed (cross files: android, macaa64, mingw only).
- Lua: default `auto` -> LuaJIT. `-Dlua=lua5.1` looks for pkg-config `lua5.1-c++` (Lua compiled as C++).
  Headers are included inside `extern "C"` (`src/lua/LuaCompat.h`), `LuaCompat.c` is C.
- Built-in scripts (`src/lua/LuaScriptInterface.cpp` ~L445): eventcompat (fatal on error), then unless
  `deleteme.txt`: manager.lua, tptmp.lua (unless a downloaded TPTMP exists), then unless `debugmode.txt`:
  crackerk.lua and failsafe.lua. **Load errors of the last four are silently ignored.**
  failsafe.lua shows "Fail safe: Uh oh something went wrong" if crackerk.lua did not reach its last line.
- `MIST` and `PLNE`: `elem.allocate("CR1K", ...)` at crackerk.lua L10 / L150. `RUBR` is C++
  (`src/simulation/elements/RUBR.cpp`). `SPSH`/`MOVS`: see section 6.
- Threads: `std::thread` in Task.cpp, SearchModel.cpp, RequestManager.cpp **and Gravity.cpp** (Newtonian
  gravity worker, not mentioned in the prompt).
- `NOHTTP` is a first-class upstream config (Android). With `-Dhttp=false` requests finish immediately
  with status 604 "Network Not Available"; `http.get` in Lua returns such a request.
- Reference exe: embeds `crackversion = 61.0`; the supplied source has `crackversion = 55.0`
  ("Next version: 55.1") and its changelog says v55.1 is current. **The exe is a later release than the
  source**, so it is only a loose behavioural reference.

## 3. Key decisions

### 3.1 Async model: JSPI (`-sJSPI`) + native Wasm exceptions — not Asyncify, not a refactor
- The game blocks in `EngineProcess()` and re-enters it from `ConfirmPrompt::Blocking`,
  `ErrorMessage::Blocking`, `TextPrompt::Blocking` (12 call sites, incl. Lua `tpt.input`/`tpt.confirm`/
  `tpt.throw_error` and the "script not responding" prompt). Refactoring to callbacks would change the
  synchronous Lua API the mod uses (e.g. crackerk.lua calls `tpt.confirm`). So keep the blocking code and
  suspend the C++ stack instead.
- Probe (`em++` 4.0.23, Chromium 153): JSPI+`-fwasm-exceptions`, ASYNCIFY+`-fwasm-exceptions`,
  ASYNCIFY+`-fexceptions` all passed a nested-loop + throw/catch-across-sleep probe, but emcc warns
  "ASYNCIFY=1 is not compatible with -fwasm-exceptions", and the game sleeps inside try blocks
  (`wrapWithBluescreen`). ASYNCIFY+JS exceptions routes every call in a try region through JS `invoke_*`
  trampolines and instruments the Lua VM and simulation (indirect calls), i.e. slow and large.
- JSPI: no code instrumentation, smallest binary, native EH, nested loops work unchanged.
  Cost: needs JSPI (Chromium/Edge >= 137; target is Edge). The page detects missing JSPI and shows a
  readable message. Emscripten still labels JSPI "experimental" (link warning).
- SDL2's `SDL_Delay` already calls `emscripten_sleep` when Asyncify/JSPI is on; `EngineProcess` adds
  `SDL_Delay(0)` when there is no fps cap so it always yields.

### 3.2 Threads: none (no `-pthread`, no SharedArrayBuffer, no COOP/COEP)
- `meson.build`: `threads_dep = []` on emscripten (Meson would add `-pthread`).
- Task::Start runs the work synchronously (AbandonableTask's wait predicate is already true).
- Gravity: `start_grav_async` does the worker's start-up work, `gravity_update_async` runs one
  `update_grav()` per hand-off then swaps buffers exactly like the threaded path (same 1-frame latency).
- SearchModel: online search runs synchronously (returns 604 immediately without HTTP).
- Benefit: plain static hosting, no cross-origin isolation headers.

### 3.3 Networking / updater
- `-Dhttp=false`: libcurl not linked; `http.get` returns status 604 immediately (no hang).
- crackerk.lua's URS updater sees 604. Originally that showed "URS error code: 604" and the Mod Settings
  status "Report the error!" (it only knew 602 = offline); 604 is now treated like 602, so it reports
  "URS updater: offline" / status "Offline". It never downloads executables.
- `-Dignore_updates=true`: no C++ update check.
- Raw TCP (tptmp.lua multiplayer): unavailable; TPTMP now prints
  "Cannot load: no TCP socket support in this build (e.g. the browser version)".

### 3.4 Dependencies
- Emscripten ports: SDL2 2.32.8, zlib 1.3.1, libpng 1.6.39 (wasm sjlj variant), bzip2 1.0.6.
- From source: Lua 5.1.5 compiled **as C++** (errors are C++ exceptions, as `lua5.1-c++` intends) with the
  public API patched to `extern "C"` under C++ (one `luaconf.h` edit in build-deps.sh); jsoncpp 1.9.5;
  fftw 3.3.10 `--enable-float` (gravity FFT stays enabled).

## 4. Errors and root causes (chronological)

| # | Symptom | Root cause | Fix |
|---|---|---|---|
| 1 | `embuilder build sdl2` -> HTTP 403 | sandbox egress blocks `github.com/.../archive/*` | `TPT_SEED_PORTS=1`: seed port cache from pinned git tags / sha256-checked tarballs |
| 2 | `EMSDK_VERSION: unbound variable` | `emsdk_env.sh` unsets a variable with that name | renamed to `TPT_EMSDK_VERSION` |
| 3 | `tar: Cannot open: [build-deps] downloading ...` | log() wrote to stdout inside `$(...)` | log to stderr |
| 4 | fftw configure: `system emscripten not recognized` | 2021 config.sub | drop `--host`, emconfigure handles it |
| 5 | Game ran but red "Fail safe" banner | manager.lua: `if not socket then error("TPT version not supported")`; socket table only registered with HTTP | compile LuaTCPSocket.cpp (already NOHTTP-aware) on emscripten, guard its unguarded `CURL*` prototype |
| 6 | `crackerk.lua:277: MANAGER.getsetting nil` | manager.lua then failed on `platform.exeName()` == "" (`realpath(/proc/self/exe)`) | ExecutableName returns `/powder` on emscripten |
| 7 | (latent) `J` shortcut would error | crackerk.lua indexes `TPTMP.chatHidden`, TPTMP absent | `(not TPTMP or TPTMP.chatHidden == true)` |
| 8 | JS full of `createExportWrapper`, wasm 4.9 MB | Meson passes `-O3` only when compiling; emcc linked at `-O0` (no wasm-opt, assertions on) | pass `-O<level>` (and `-g` for debug) to the emscripten link (`meson.build`); 4.19 MB wasm, 192 KB JS |
| 9 | Test keys lost right after "running" | the module's ready promise resolves before `main()` creates the SDL window and its input listeners | main loop reports its first presented frame (`Module.tptOnFirstFrame`); page/tests use that |
| 10 | Long console commands silently did nothing | `Textbox.cpp:495` drops typed characters once the line is wider than the box -> truncated Lua | tests keep console lines < 90 chars via two tiny helper functions |
| 11 | Page froze for minutes when Newtonian gravity was switched on | `FFTW_MEASURE` without a cycle counter: 10 ms timing batches x hundreds of candidates (plan > 300 s, measured in node) | `FFTW_ESTIMATE` on emscripten (plan 6 ms, ~1 ms per transform) |
| 12 | Native build fails: `uint32_t does not name a type` | pre-existing: `FontReader.cpp` lacks `<cstdint>` (GCC 13 libstdc++) | add the include; native Linux build (LuaJIT, curl, fftw, threads) then compiles 422/422 and runs headless |
| 13 | Vercel deployment `git_info_fail` | Vercel account has no GitHub connection; MCP `upload_file` needs file content in the call (5.6 MB base64 for the wasm) | blocked on credentials, see section 7 |
| 14 | Saves gone after reload (intermittent, file:// test) | IDBFS `autoPersist` + startup load: each node created by `syncfs(true)` queues a persist that snapshots the half-loaded tree and deletes the rest from IndexedDB; leaving during startup made it permanent | `wasm/pre.js` holds `idbPersistState` non-idle during the load; A/B: old 3/3 lost saves, fixed 0/5; regression test |

First full compile of the tree (before any source change) had **0 errors**; the 11 warnings are
pre-existing code-quality warnings in the mod sources (unused variables etc.) and were left alone.

If Emscripten's cache gets cleared in a sandbox that needs seeding (happened once here, cause not
identified; emcc re-ran its sanity check and started from an empty cache), rerun
`TPT_SEED_PORTS=1 ./wasm/build-deps.sh`.

## 5. Verification

### 5.1 Build
- Clean build from scratch, local (Emscripten cache cleared, build dirs removed):
  `TPT_SEED_PORTS=1 ./wasm/build-deps.sh` 2m34s, `./wasm/build.sh` 2m03s, exit 0
  (`artifacts/logs/clean-build-*.log`).
- Clean build from scratch on GitHub Actions (ubuntu-24.04, normal port downloads, no caches):
  run https://github.com/ogdaastra/tpt-cracker-port/actions/runs/40093530610 - success
  (deps 2m20s, game 3m00s, wasm-validate ok, 11/11 browser tests at that commit).
  Final code commit 3b0b2be: run https://github.com/ogdaastra/tpt-cracker-port/actions/runs/40098015865 -
  success: build, wasm-validate, 13/13 browser tests over HTTP, offline ZIP built and 11/11 tests on
  file://; artifacts `dist`, `offline-zip`, `test-artifacts` uploaded; `deploy` job skipped (no token).
- Native Linux build (LuaJIT, libcurl, fftw, threads) of the same tree: 422/422 targets, runs headless
  (SDL dummy driver), writes powder.pref.
- `wasm-validate --enable-all` passes; exports, sizes, sha256 and embedded game/mod strings recorded in
  `artifacts/validation/wasm-validation.txt`. powder.wasm 4,190,185 bytes, powder.js 191,929 bytes.

### 5.2 Browser tests (`tests/browser/tpt.spec.mjs`, Playwright 1.63.0, Chromium 153 headless)
All driven through the real UI: mouse/keyboard on the canvas, the game's own Lua console for reading state.

| # | Test | Local HTTP | Offline ZIP via serve.py | Offline ZIP via file:// |
|---|---|---|---|---|
| 1 | page loads over HTTP; powder.js/powder.wasm 200 + application/wasm; no unexplained console errors / failed requests | pass | pass | n/a (HTTP-specific) |
| 2 | canvas non-blank, keeps changing (falling powder) | pass | pass | pass |
| 3 | left mouse draws DUST, right mouse erases (NONE), Space toggles pause | pass | pass | pass |
| 4 | crackerk.lua finished (`failsafe == 1`, MANAGER table); MIST and PLNE ("SPSH") exist with the right names/menus; both selected via element search (E) and placed by clicking | pass | pass | pass |
| 5 | local save via the game's save dialog -> page Export (download, `OPS1` magic, same bytes) -> clear -> page Import -> same particle count; reload -> files still there; game's own save browser (Ctrl+Open) lists and loads them | pass | pass | pass |
| 6 | mod UI: J and the Cr-Menu button open/close Mod Settings; one particle of each of the 51 mod C++ elements runs 5 s | pass | pass | pass |
| 7 | nested modal loops: Lua `tpt.confirm`, `tpt.input`, `tpt.throw_error` show dialogs and return | pass | pass | pass |
| 8 | persistence: powder.pref written; MANAGER setting survives reload | pass | pass | pass |
| 9 | resize to 1600x1000, 900x640, 640x520: canvas fits, keeps aspect, clicks land on the right cell | pass | pass | pass |
| 10 | performance + Newtonian gravity field next to a black hole | pass | pass | pass |
| 11 | 16 navigations during startup keep all 40 saves (regression for root cause 14) | pass | pass | pass |
| 12 | readable failure page without JSPI | pass | pass | pass |
| 13 | readable failure page when powder.wasm 404s | pass | pass | n/a |

Measurements (headless Chromium, SwiftShader WebGL, 4 vCPU; `artifacts/browser/measurements.json`):
navigation to first game frame ~0.33 s page time / ~0.9 s wall incl. test harness; idle 53.8 ticks/s,
29,886 particles 54.8 ticks/s, same with Newtonian gravity 54.2 ticks/s (fps cap 60); gravity field
2.7e-3 five cells from a black hole after 60 frames (0 before).
Screenshots: `artifacts/browser/*.png` (HTTP) and `artifacts/browser-offline-file/` (file://).
Not tested: real Microsoft Edge on Windows (same Chromium engine, but not run here), touch input,
HiDPI, audio (the game has none), low-end hardware.

## 6. Mod features (source -> status)

| Feature | Where | Status |
|---|---|---|
| MIST (missile target tool, CR1K_PT_MIST, id 255) | crackerk.lua L10 | runtime-tested: exists, selectable, placed |
| PLNE, shown as "SPSH" (space ship) | crackerk.lua L150 | runtime-tested: exists, selectable, placed |
| 51 C++ elements FNTC..RUBR (ids 192-242) | src/simulation/elements/*.cpp | all runtime-tested together (placed, 5 s of simulation, no crash); RUBR also saved/loaded |
| Mod Settings menu (Cr-Menu button, J shortcut) | crackerk.lua L325, L2760 | runtime-tested open/close; individual options present but not exercised |
| Mod UI theme, HUD, intro text | crackerk.lua, GameView.cpp | visible in screenshots; not asserted beyond that |
| Settings storage via MANAGER (script manager) | manager.lua | runtime-tested (persists across reload) |
| URS updater / MOTD | crackerk.lua L432-660 | unavailable (no network); reports "URS updater: offline" |
| Notifications script (Maticzpl) | crackerk.lua end | loads; needs network, shows its own messages |
| TPTMP multiplayer | tptmp.lua | unavailable (no TCP sockets); prints why |
| MOVS | changelog only | not found in the source by name; the changelog describes it as the earlier name of the space ship (now PLNE/"SPSH") - inferred, not traced |

## 7. Deployment status: BLOCKED on credentials
- Vercel MCP is authenticated (personal hobby account, team `team_Hd1pTA8fuMgOK5kVYAJlPhbd`), but:
  - its `upload_file` takes the file content inside the tool call: the 4.2 MB wasm would be 5.6 MB of
    base64 that has to pass through the agent's own output - not feasible;
  - `create_deployment` from the public GitHub repo fails with `git_info_fail` (no GitHub connection on
    the Vercel account); a Vercel Sandbox has no deployment credentials either.
- Created by the probe: Vercel project `tpt-cracker-port` (`prj_HfIKw7K397cl1CqEp6UKmk8AkOsf`), one
  failed deployment, nothing served. Build settings reset to static defaults afterwards.
- The project uses the team default protection: Vercel Authentication on `all_except_custom_domains`,
  i.e. even production `*.vercel.app` URLs require a Vercel login until that is changed.
- Ready to go: `wasm/deploy-vercel.sh` (preview by default, `--prod` explicit) and the CI `deploy` job
  (preview -> browser tests against it -> production only on manual run with `production: true`).
  Needs a `VERCEL_TOKEN` (repository secret for CI, or a secret in this environment for a session).

## 8. Offline bundle
`./wasm/make-offline-zip.sh` -> `cracker1000-mod-wasm-offline.zip` (20 MB): site, `powder-wasm.js`
(base64 binary for file://), `serve.py`, `serve.mjs`, `Start Game (Windows).cmd`, `start.sh`,
README-OFFLINE.txt, LICENSE, sources. No .exe (the script refuses to package one).
- Tested here: serve.py (13/13) and double-clicking index.html, i.e. file:// in Chromium (11/11).
- Not tested: the Windows .cmd and Edge/Chrome on Windows (no Windows machine); serve.mjs only
  syntax-equivalent to the tested test server, not run as a launcher.
- Saves are per address: http://127.0.0.1:8123/ and file:// keep separate storage (README says so).

## 9. CI
`.github/workflows/wasm.yml`: pinned emsdk, no caches, build deps + game, wasm-validate, browser tests,
offline ZIP + file:// tests, uploads dist/, offline ZIP, logs/screenshots/report; optional `deploy` job.

## 10. Limitations
- JSPI required: Chromium/Edge >= 137. Firefox/Safari get the failure page. A second build variant with
  ASYNCIFY=1 + JS exceptions would run everywhere but slower and larger; not done.
- No online features (save browser, login, votes, comments), no multiplayer, no updaters. A realistic
  path for online saves: powdertoy.co.uk would have to send CORS headers or a proxy would be needed
  (a proxy has cost/abuse implications - not added, not decided).
- PUC Lua 5.1 instead of LuaJIT: heavy Lua scripts run slower; LuaJIT-only `ffi` scripts do not work
  (the built-in scripts do not use it).
- Single-threaded: gravity and thumbnail rendering share the main thread (measured cost small).
- Saves live in browser storage; clearing site data deletes them (Export them as .cps files).
- In-game "Scale"/"Fullscreen" options: the page scales the canvas; use the page's Fullscreen button.

## 11. Next steps
1. Add `VERCEL_TOKEN` (and decide on Deployment Protection), then deploy a preview, run the suite against
   it (CI does this automatically), check it in Edge, promote to production when happy.
2. Optional: run the suite with `TPT_BROWSER_CHANNEL=msedge` on a Windows machine with Edge.

## 12. Deployed
- Production: https://tpt-cracker-port-96z6vdz5n7-9854.vercel.app/ (deployment dpl_FT8RqdNr5X7wTnFjqQz65DkHn7p5,
  commit 983e136), via CI run https://github.com/ogdaastra/tpt-cracker-port/actions/runs/40105591059:
  preview deployed, 13/13 browser tests against the preview, then production.
- Verified from this session: production URL returns 200, no Vercel toolbar script, full suite 13/13.
- Changes made on Vercel at the owner's request: Vercel Authentication disabled (site public); preview and
  production feedback toolbar disabled (it intercepted keys and reloaded the page, breaking the game in tests).

## 13. Added elements: TPT Ultimata, newer Cracker1000's Mod, Jacob1's Mod
Requested after the port shipped: add the elements TPT Ultimata has that Cracker1000's Mod v55.1 does not,
the elements of newer Cracker releases (DFOM etc.), and other standout elements from other mods.
Full list with menus and descriptions: `ADDED_ELEMENTS.md` (143 elements).

### 13.1 Sources
| Source | Commit | Taken |
|---|---|---|
| TPT Ultimata (Bowserinator), github.com/Bowserinator/TPT-Ultimata-Mod `development` | b749717 (Oct 2023) | 133 elements, 3 walls, 14 Life rules, 5 tools, simulation subsystems (circuits, vehicles, moving solids, quantum, stress, EM field, stasis, time dilation, Faraday regions, NOTE sound), Eigen 3.3.7 |
| Cracker1000's Mod, branch `Bledge-2024` | 4ee2995 (Oct 2025) | DFOM, DIGS, ALUM, ALMP, RSST, RSSS (written for TPT 98, adapted to 97) |
| Jacob1's Mod, `c++` | 0635c0e (Sep 2026) | EXPL, RAZR, PWHT, BUTN (rewritten: Jacob1's simulation API differs) |
All GPL-3.0 (Eigen: MPL-2.0 with some BSD/LGPL-2.1 files; its licence texts are in `src/eigen/` and in
`THIRD_PARTY_NOTICES.txt`). The bundled Eigen files were checked byte-for-byte against upstream 3.3.7.

### 13.2 Method
Ultimata and Cracker v55.1 are both forks of the official game of January 2023 (merge-base of Ultimata with
the official repo: 225293cc). The Ultimata changes were merged three-way against that commit: new files copied,
core files (`Simulation.*`, `SimulationData.*`, `GameSave.*`, `Air.cpp`, `Sign.cpp`, `Element.*`, ...) merged
with `git merge-file`, conflicts resolved by hand. The official elements Ultimata had edited (45 files) were
reviewed one by one: edits that only matter when a new element is present were kept, edits that would change
how existing saves behave were not (Cracker keeps official behaviour for compatibility). Not taken, for example:
SAWD displaced by powders, CBNW treated as water for PHOT/legacy FIRE, an older PTNM, PTNM+COAL -> SUFR,
THDR ignoring CLUD, FIGH accepting element 0, GLOW+GEL -> RSST (TPT 98), Ultimata's renderer tweaks.

### 13.3 Decisions
- **UI stays Cracker's.** Ultimata's own UI (two-column side bar, config/transform/text/ruler/PROP2 tools,
  extra brushes, mod options, FPS graph, extra quick options) was not ported. The side bar has no free slot
  (Cracker's script-manager and multiplayer buttons use the two above Walls, and its themes draw at fixed
  positions), so Ultimata's menus (Electromagnetic, Organic, Space, Random) were mapped onto the official ones.
  Menu numbering stays official because scripts use raw section numbers (`crackerk.lua` does).
- **Names:** Ultimata's ROCK and SLCN collide with official elements: renamed ROCK2 and SLCN2 (like Cracker's
  LITH2). Ultimata's BEE, CLUD, CMNT, COPR, CSNS, MGNT, PINV, RADN and SEED are different elements with names
  Cracker already uses: Cracker's are kept.
- **IDs:** new elements take IDs 243-385 after Cracker's (saves map elements by identifier, so Ultimata saves
  still load their elements). Walls: Cracker's ZHOLE is 19, so I-WALL/FARADAY/ONE WAY are 20/21/22 here
  (19/20/21 in Ultimata); Ultimata saves using those walls load them as different walls.
- **Undo history** keeps the official delta snapshots (Ultimata had reverted to full copies); `SnapshotDelta`
  gained the one-way, Faraday and time-dilation maps, the timer and the portal contents.
- **Portal buffers:** Faraday regions multiply portal channels (200 x 101). Ultimata declared a fixed array
  for all of them (~700 MB inside `Simulation`); here a channel is allocated on first use (`PortalBuffers.h`).
  Cracker's PPTI/PPTO use the same channel layout.
- **Global state:** circuits, quantum states and moving solids are globals in Ultimata; only the simulation the
  game runs may reset them (`ownsGlobalElementState`), so save thumbnails don't wipe them. Undo clears them and
  they rebuild from the particles.
- **Sound:** Ultimata 97 had commented out its audio output, so NOTE was silent; the device is opened again
  (on first use), mixed at ~18 % of full scale. `tpt.play_sound()` is exposed.
- **RSSS insulates** like INSL through `parts_avg` (TPT 98 added `pavg != PT_RSSS` at each call site).
- **EXPL** uses a new particle flag `FLAG_EXPLODE` (0x10), handled in `UpdateParticles` as in Jacob1's Mod.
- Stress simulation (TRUS) and the EM field stay off by default as in Ultimata; with Ultimata's quick options
  not ported, stress is only switched on by a save that has it (`stress_enable`).

### 13.4 Bugs found and fixed (Ultimata code unless noted)
| Problem | Found by | Fix |
|---|---|---|
| Browser tab crashed in ~3 of 4 runs with all new elements placed | browser test | root cause below (circuit use-after-free) |
| `Circuit` freed by `add_circuit` while `circuit_map` still pointed to it; RSPK's ChangeType then wrote into freed memory | ASan | destructor clears its `circuit_map` entries |
| ...which made every VOLT pixel of a non-circuit blob flood-fill the blob again each frame (game froze) | ASan stress run (hang) | per-pixel "recently failed" marker, retry every 20 frames |
| BALI picked `DEFAULT_CTYPES[N]` (inclusive RNG bound) | ASan | `N - 1` |
| WEB graphics read `parts[-1]` | UBSan | check before reading |
| Vehicle hull points rotated past the screen edge read `pmap` out of bounds | UBSan | treat as wall; `IsWallBlocking` bounds-checks |
| Moving-solid centre off screen indexed gravity/air/stasis maps out of bounds | ASan | clamp |
| Time dilation / density map read with off-screen coordinates | UBSan | read after the off-screen kill / `InBounds` |
| `std::fill(&a[0][0], &a[ROWS][0])` one-past-end row (7 places) | UBSan | explicit element counts |
| `FARADAY_CHANNELS` macro without parentheses | review | parenthesised |
| QLOG wrote WiFi with the old `[channel][1]` layout | review | Faraday-aware index |
| IRDM sparks created at `x + ry` | review | `x + rx` |
| Deco blend modes: `restrict_flt` result ignored, uninitialised targets | compiler warning | assign / initialise |
| HRSE colour `elements[ctype \|\| PT_DUST]` | compiler warning | ternary |
| Header guards `MANGET_H`, `FOMRAT_CIRCUIT_H` | compiler warning | fixed |
| One-way wall save data read past its end; rotation used a stale vector | review | bounds check; rotate per direction |
| CPTR default capacitance 0.1 became 0 when tmp3 became an int; divide by zero | review | default 1; guard |
| CLNE-GOL save hack rewrote old saves' custom rules; new Life rules used GOL IDs as old element IDs | review | removed; old ID -1 |
| Crash for one specific user name (`pip...ick`, an Ultimata joke) | review | removed |
| ALMP wrote `parts[-1]` when EMBR creation failed; ALUM probes off screen (newer Cracker) | review | checks |

### 13.5 Verification
- `wasm/build.sh`: builds (0 errors), powder.wasm 4.94 MB (was 4.19 MB). Native ASan+UBSan build
  (`-Db_sanitize=address,undefined`, LuaJIT) of the same tree, run headless (SDL dummy drivers) with an
  autorun script:
  - all 143 elements placed together, spread out and packed, 600-1500 frames, repeated: no ASan/UBSan reports;
  - each element alone in a 111x51 block next to METL+SPRK, WATR, FIRE, STKM and pressure, 40 frames each:
    no reports (the only UBSan line is a misaligned read in the official `FontReader.cpp`).
- Browser tests, Chromium headless: `tests/browser/added-elements.spec.mjs` (7 tests: every element present,
  enabled, in a shown menu, placeable, 8 s with all of them running; I-WALL; RSST+PHOT->RSSS, EXPL, BUTN click;
  stamp round trip with all 143 types and undo/redo with the new state; a VOLT circuit producing RSPK; a
  5600-pixel VOLT block keeps running (252 ticks in 4 s); portals) plus the 13 existing ones: 20/20 over HTTP, 18/18 from the
  offline ZIP opened as file://. Performance test unchanged: 53-54 ticks/s idle and with 28k particles.
- Crash probe (all elements, 20 s, repeated): 0/6 tab crashes after the fixes (most runs crashed before).

### 13.6 Limitations of the additions
- Ultimata's tools that need its UI (config tool, transform, text, ruler, PROP2, extra brushes and deco tools,
  EM-field / stress / time-dilation view toggles) are not included; the simulation code behind them is.
- Ultimata described itself as buggy; its circuit solver in particular was written for float `tmp3/tmp4`
  and runs on integers since TPT 97 (voltages and currents are whole numbers).
- Ultimata saves were not tested: their elements should load (saves map elements by identifier), but its
  walls 19-21 load as different walls here.

## 14. Speed and menus (after the element additions)

### 14.1 Where the time goes
Chromium CPU profile (wasm with `--profiling-funcs`) of a 72-82k particle water/sand/fire scene at 4x CPU
throttle: `Simulation::UpdateParticles` 37%, `do_move` 9%, `render_parts` 9%, `RecalcFreeParticles` 8%,
`eval_move` 8%, `BeforeSim` 4%. The TPT Ultimata hooks barely register (`StressField::ComputeStress` 0.2%).
The cost is the core particle engine, so the useful levers were the compiler settings; the Ultimata clean-ups
are kept because they remove work, not because they measurably speed things up.

### 14.2 Changes
- `density_map` (Ultimata, kept up to date every frame and on every create/kill) had no reader since its
  renderer fast path was not ported: removed.
- EM-field and stress hooks are checked at the call site; a new `anyTimeDilation` flag (set by every writer of
  `time_dilation`, recomputed by the 4-frame tick-down) skips the per-particle and per-air-cell time dilation
  checks and the tick-down loop while no cell is dilated.
- Build: `-msimd128` (meson.build) and link-time optimisation (`-Db_lto=true` in `wasm/build.sh`).

### 14.3 Measurements
`tests/browser/perf.spec.mjs` (PERF=1) times frames inside the game with the new `tpt_benchmark` export, so
browser scheduling is not in the number. Same machine, builds alternated, 7 rounds each, ms per frame
(simulation + particle render), median / best:

| Scene | before (O3) | + SIMD | + SIMD + LTO (shipped) |
|---|---|---|---|
| powder bands, 24k particles | 4.30 / 3.40 | 3.65 / 3.43 | 3.77 / 3.66 |
| water, sand and fire, 72k | 8.91 / 6.12 | 8.70 / 5.48 | 5.99 / 5.21 |
| powered electronics, 35k | 11.54 / 10.61 | 11.56 / 10.79 | 11.38 / 10.39 |

The container is noisy (single rounds vary by up to 30%); only the water/fire result is clearly outside it
(about a third less time per frame). powder.wasm grows from 4.95 MB to 5.21 MB (+5.4%). The whole-game tick
rate with throttling (`PERF_THROTTLES=4`) varied 18-31 ticks/s between identical runs and was not used to
decide anything.

### 14.4 Menus
Five menus split off the longest ones (Special had 54 entries, Solids 47, Electronics 45): Life and Nature,
Space and Gravity, Characters and Vehicles, Circuits, Metals (sections 18-22, after Cracker's hidden 16/17,
so raw numbers in Lua scripts keep their meaning; constants `elem.SC_ORGANIC` ... `elem.SC_METALS`). They sit
in a second column of menu buttons: `EXTRABARSIZE` makes the window 646 px wide; the classic column, quick
options, search and bottom-row buttons are pinned to their old x positions (`CLASSIC_W` in GameView.cpp),
because crackerk.lua's theme draws frames at fixed coordinates (it now frames the second column too).
`tpt.num_menus()` counts the classic column only, since manager.lua and tptmp.lua use it to place their side
buttons; manager.lua's button is placed at `sim.XRES+1` instead of `gfx.WIDTH-16`. Largest element menu
afterwards: Liquids, 35 (measured from the running game, which includes elements crackerk.lua moves).

The page has an Elements panel (button in the header; open by default on wide screens, remembered in
localStorage): every menu as a collapsible group, a search box over names, identifiers and descriptions,
click for the left tool and right-click for the right tool, Enter picks the first match. It reads the menus
from the running game (`tpt_element_list`), so Lua-made elements appear after a refresh, and selects through
a queued SDL user event (`tpt_select_tool`) handled in the game loop. Key events in the search box are
stopped before SDL's window listeners see them.

### 14.5 Verification
- Browser tests, Chromium headless: 24 tests over HTTP (new `categories.spec.mjs`: menu contents, side-bar
  hover positions, panel search/select/keyboard isolation/Lua elements/remembered state, 375 px layout without
  horizontal scrolling), 22 from the offline ZIP opened as file://; the PERF benchmark is skipped by default.

## 15. Heat pipes (PIPE and PPIP, from The Powder Toy 100)

Upstream TPT added heat pipes after the version this mod is based on; ported here.
- A PIPE or PPIP that has HEAC next to it while its pattern generates gets the flag `0x1` in `tmp`
  (`Simulation::PFLAG_CAN_CONDUCT`), flood-filled over the whole pipe (`Element_PPIP_flood_trigger`, now also
  for PIPE, with HEAC as trigger). Recipe: draw PIPE, wait for the BRCK border, replace a border brick with
  HEAC, open an end as usual.
- PIPE and PPIP have HeatConduct 251, but `Simulation::IsHeatInsulator` (as upstream: HeatConduct 0, HSWC
  while off, pipes without the flag) keeps ordinary pipes insulators. It replaces the separate HeatConduct/HSWC
  checks in the heat transfer code, PHOT conduction, HEAC, the air heat blocking (Air.cpp), C5, C6, VIBR,
  STKM, LIGH, LASR and DEST. LIGH, LASR and DEST thereby also stop heating HSWC that is off, as upstream.
- A heat pipe keeps its own temperature: a particle entering it, or moving on to the next heat pipe pixel,
  mixes with the pipe's temperature. Upstream weights by heat capacity; this version of the game has no
  per-element heat capacity, so the two count equally. Ordinary pipes carry the particle's temperature as before.
- Saves: the flag shares tmp's low byte with leftovers of the pre-93 PIPE format. Upstream clears that byte
  for saves older than version 100 and saves heat pipes as version 100. This game writes version 97, so it
  writes `heatPipes: true` into saves that contain heat pipes and clears the byte only in older saves without
  that marker (and always in the pre-93 conversions).
- Verified: `tests/browser/heat-pipe.spec.mjs` builds a 41-pixel pipe with HEAC and one without. Afterwards all
  41 pixels of the first are flagged and none of the second; heating the left 5 pixels to 1000 C for 3 s brings
  the far end of the heat pipe from 291 K to 951 K while the ordinary pipe stays at 273.15 K; a stamp round
  trip keeps both. Full suite 25/25 over HTTP; ASan+UBSan runs report nothing in changed code (C5's signed
  shifts and the STKM leg read are in unchanged original code).

## 16. Recording (page)

The header's **Record…** button records a video or a GIF of the game, or takes a PNG screenshot; options:
simulation area or whole window, 1x/2x, 5/10/20 s or until stopped (the button turns into a red Stop timer).
- The game canvas is WebGL, so it can only be read in the task that drew it: `tpt_set_frame_callback(1)`
  makes `blit()` call `Module.tptOnFrame()` after presenting, and the page copies the frame into a 2D canvas
  of the chosen area and size there (pixelated scaling). Off while not recording.
- Video: `MediaRecorder` on that canvas's `captureStream(60)`, MP4/H.264 where supported (Chromium has it),
  else WebM. GIF: gifenc 1.0.3 (MIT, `web/vendor/`, wrapped as a factory function) in a Worker built from a
  Blob, so it also works when the page is opened from disk; 20 frames per second, 256-colour palette per frame.
- Tests (`tests/browser/record.spec.mjs`): a 5 s 2x MP4 (365 KB, decodes at 1224x768, 299 frames copied), a
  5 s GIF (97 frames, 612x384, valid structure), a whole-window PNG (646x424), and a GIF stopped by hand after
  2 s (40 frames). Full suite 28/28.

## 17. TPT Ultimata's tools and switches

Ported from Ultimata (same TPT base, so nearly unchanged; files in `src/gui/game/`):
- **Tools menu:** TRFM (transform: select a rectangle, scale/rotate it), CNFG (click a particle for a proper
  settings dialog: WIFI/portal channels, CRAY/DRAY, detectors, FFGN, FRME, PSTN, TRBN, CLUD, LIFE, quantum…),
  TEXT (type text, placed as particles, several fonts), RULR (distance between two points), PRP2 (set several
  properties at once while drawing, `prop:value` per line with + - * / & ^ | operations).
- **Hollow brushes:** Shift+Tab switches between the solid brushes and hollow circle/square/triangle.
- **Field switches** in the second side-bar column, above the new menus: EM simulation, electric view,
  magnetic view, stress simulation, stress view, time-dilation view. Lua: `sim.emField([on])`,
  `sim.stressField([on])`; tool constants `sim.TOOL_SLOW`, `TOOL_FAST`, `TOOL_NORM`, `TOOL_ERODE`,
  `TOOL_DECO`, `TOOL_RSET`, `TOOL_AMBP`, `TOOL_AMBM` were missing and are added.
- Small changes: `Textbox::GetCursor*` getters (Ultimata's arrow-key change to every text box is not taken),
  the CNFG channel dialog checks the particle still exists, crackerk.lua frames the new buttons.
- Known limitation (Ultimata's own): its circuit solver writes currents into the integer `tmp4`, so currents
  under 1 A read as 0 and the electric field (made by RSPK from `tmp4`) only appears with strong currents.
  Measured: 1000 V across 14 RSTR pixels gives 0.071 A in the solver, 0 in the particles.
- Tests (`tests/browser/ultimata-tools.spec.mjs`, 8): tools listed in Tools; TEXT writes "HI" in DMND at the
  clicked spot; RULR draws a 200 px line and creates nothing; TRFM turns a 20x10 block into 9x20 (180 of 200
  pixels kept, Ultimata's nearest-pixel rotation; its "Auto-fill gaps" option fills them); CNFG sets WIFI to
  channel 5 (473.15 K); Shift+Tab draws a ring with an empty centre and back; EM and stress switches change the
  Lua state, the time-dilation view tints a SLOW field (14,785 pixels); PRP2 sets temp 500 and life 7 together.
  Full suite 36/36.

## 18. Challenge mode

50 challenges in 9 categories (list: `docs/CHALLENGES.md`), playable from the page (Challenges…) or in the game
(★ button at the bottom of the second side-bar column, a Lua window).
- **Game side (C++):** a challenge session. `Simulation::challenge` holds the ink budget, the ink used and locked
  rectangles; `Simulation::userAction` is set by `GameController` around the player's drawing, line, box, flood
  and tool actions, so only the player's particles count as ink (reactions don't) and locked rectangles refuse
  only the player (`UserMayChange`, checked in `CreatePartFlags`, `Tool` and `CreateWalls`). `GameModel` keeps
  the allowed tools: menus, favourites and the page's Elements panel only show those, `SetActiveTool` refuses
  others (page, Lua, keyboard), placing stamps/saves is refused, undo history is cleared, and the menu of the
  first allowed tool opens. The eraser and the sample tool always work. Lua: `challenge.begin{tools, ink,
  locked}`, `challenge.finish()`, `challenge.ink()`, `challenge.active()`, `challenge.notify(json)` (calls
  `Module.tptOnChallenge` on the page). The page runs Lua in the game loop with `tpt_lua_command`.
- **Challenges (Lua, built in):** `src/lua/luascripts/challenges.lua`, loaded always (even when the optional
  scripts are off). Each challenge is one table: scene setup, allowed tools, ink, time, a goal check run after
  every simulation frame (progress 0..1, or a failure reason), star thresholds, hints (each costs a star) and a
  **reference solution**. Stars are saved in `challenges-progress.json` in the game's data folder (IndexedDB in
  the browser). Solution runs (`challenges.command("solve <id>")`) never earn stars.
- **Every challenge was tuned by running its solution:** a native harness (ASan build, Lua autorun) played every
  solution and every do-nothing run; designs that didn't hold up were changed or dropped (an air-pressure
  "big bang" goal that explosions here never reach, a sponge goal the eraser could fake). Found on the way:
  plasma melts the wires it is meant to heat, glass passes lava heat on to a stickman's feet, stone is a powder,
  titanium melts below the furnace temperature first chosen, sorting dust from water needs a 5 s hold because
  dust falls faster.
- **Reliability:** repeated native runs turned out to be identical (same random seed every start), so they
  say nothing about luck. In the browser every run is different: a stress script ran every solution 6-8 times
  and every do-nothing case twice there, and each challenge that ever failed (Crater, Wildfire, Etching, Lava to
  stone, Desalination) got a wider margin: a stronger reference solution within the ink limit or a slightly
  easier goal.
- **Tests** (`tests/browser/challenges.spec.mjs`, 4): all 50 solutions win and all 50 do-nothing runs lose (the
  page steps the simulation with `tpt_benchmark`, about 2-3 minutes each); "First light" played by hand
  through the page (only BTRY/METL/PSCN plus eraser and sample offered, WATR refused from the page and PLSM
  from Lua, the locked LED survives the eraser, ink = particles drawn, win with stars, stars still there after a
  reload); ink budget stops a big brush at exactly 300, the locked flag survives, hint text, restart clears the
  scene and ink, the in-game window starts a challenge. They found two real bugs: the game crashed ("vector")
  when a tool was selected during a challenge, because `GameController::SetActiveTool` looked up the gravity
  wall by its position in the walls menu (now by identifier), and the challenge bar never hid (CSS `display`
  overrode `hidden`).
- **Page:** Challenges dialog (categories, stars, difficulty, tools, ink, Play), a bar above the game while one
  runs (progress, time, ink, Hint, Restart, Next challenge, Stop); the in-game HUD shows the same.

## 19. Fixes from newer official TPT, batch A (crashes and correctness)

Upstream (The Powder Toy, 98.0 to 100.1 and later master) cloned and its fix commits since January 2023 read
one by one; the ones whose code exists here were ported by hand (the code bases have diverged, so as small
equivalent edits, each marked with the upstream commit in a comment):
- **Simulation core:** NaN velocity no longer loops forever (51508113); a portal-captured particle is killed
  properly instead of only clearing its type (82ec6b27); every pmap/photons clear checks the entry is non-empty
  first (2eee738a); `create_part` checks the old position is inside the map (a5b12b71); swapped particles take
  the other's exact position, so piles no longer stack oddly near power-of-two positions (76dca483); gases lose
  a 0.25 px bias that pushed them down and right (d21a2293); `flood_water` checks the right span neighbour
  (9481bf05) and moves particles with `move()` (5fc8770e); placing a save partly outside the simulation no
  longer writes walls, pressure or signs out of bounds (a2056128); the air advection boundary read one cell too
  far (3b19f846) and its "far away" step only worked in one direction (2e27cbfd).
- **Elements:** PROT returns after killing itself (6433eb04); BOMB bounds-checks its outer ring (0d9031bb);
  ACID eats one particle fewer, as intended (b5872657); MERC clamps its temperature before dividing (d6f66f5c);
  BOYL stops after turning into water (48df5cda); EMP's sparking area is symmetric (32033af6); SOAP checks its
  links' indices (ce250e31) and, in loop edge mode only, measures bubbles across the edges (d8da9ecc,
  7f0bd154); SPRK(TESC) and SPNG cooling stay above 0 K (7623e04e); LITH respects the temperature limits
  (79f2bbca); FUSE's sparks land around it instead of always at one spot (ca6558bd); GLOW multiplies photons
  and NEUT makes Cherenkov photons in glass 1 in 30 / 1 in 10 times, not 29 in 30 / 9 in 10 (875f92b2); GLOW's
  colour uses the right 0 C constant (46730cfc).
- **Already fine or not applicable here:** DLAY, ETRD, VIBR heat (this code already had the checks); FIRE's
  missing returns (present); stickman instant death, many-headed stickmen, save size, gravity-wall and
  "infinite air heat" fixes (they patch upstream's newer code, which this version doesn't have).
- **Tests** (`tests/browser/upstream-fixes.spec.mjs`, 3): a 60x60 gas cloud in a centred wall box drifts
  0.4 px / 1.2 px in 10 s (the production build before this: 2.6 px right, 5.8 px down, so the test fails
  there); a DMND + PRTI + NEUT setup from the upstream report leaves pmap/photons consistent; NaN velocity and
  MERC at -1.5 K run without hanging. Honest note: the last two do not fail on the old build either (this older
  engine clamps temperatures and moves NaN particles differently), so they are regression checks only.
  All 50 challenges re-checked: ASan run 50/50 solutions win, no sanitizer reports in changed code; browser
  stress 3 solution + 1 do-nothing runs each, after widening Wildfire's firebreak (the unbiased fire spread
  further).

## 20. Physics from newer official TPT, batch B

- **Heat capacity** (upstream 74f6a129, 85ee427e, e0d14786, e7c708a0, df5d90cc): elements have a
  `HeatCapacity` (1.0 by default, BASE 1.5 as upstream; Lua `elem.property(id, "HeatCapacity", v)`).
  Neighbour heat exchange, air heat exchange, HEAC and heat pipes weight temperatures by it, and a pipe
  with something in it counts the capacity of both. With every capacity at 1 the exchange is the plain
  average it was before, so existing saves behave the same apart from BASE.
- **Vorticity confinement** (b77a1cba, 4d1364b8): an air setting that keeps swirls going. Off by default
  (0, the old behaviour; upstream now defaults to 0.1), `sim.vorticityCoeff([0..1])`, saved in saves as
  `vorticityCoeff` when on. Upstream's display mode and options box are not ported.
- **Caps:** ACEL keeps direction when capping velocity, FRAY and RPEL cap velocity (95b9d671, bfd1199b);
  DEST, LIGH, SING, THDR and WARP clamp the pressure they add (0c47112c); air temperature is clamped to the
  game's limits (feb3f9de).
- **Not ported:** upstream's rewritten ambient heat convection and its settings, ambient air pressure and
  velocity, gravity in saves: they sit on upstream's newer air and gravity code, which this version (with
  Ultimata's time dilation inside the air loop) doesn't share. Porting them means porting that code first.
- **Tests** (`upstream-fixes.spec.mjs`, now 5): a 1000 K METL block against a 300 K TTAN block settles at
  650 K / 650 K; with TTAN's capacity set to 3 at 475 K / 475 K, which is the energy balance
  (1000 + 3 x 300) / 4. The same air disturbance past a wall block leaves a total |curl| of 65 with
  confinement off and 97 with 0.5; a stamp opened as a save brings the setting back (0.3). All 50 challenges
  re-checked (ASan 50/50; browser 3 solution + 1 do-nothing runs each, all as expected).

## 21. Reactions from newer official TPT, batch C

- **New reactions** (upstream 2d7f5fa2): PROT accelerated into TUNG makes AMTR (elsewhere still SING);
  molten gold below -200 pressure turns into molten PTNM (1 in 20,000 per frame); molten SLCN touching
  molten SALT becomes molten LITH.
- **FOG and RIME absorb GAS** (56ede6ca): each soaks up to 10; RIME with more than 5 melts into ACID instead
  of water, and ACID/CAUS don't eat such acidic FOG/RIME. BCLN/PBCN no longer freeze FOG, so they can
  clone it (8ad965b1). CAUS's "life >= 50" is the same off-by-one as ACID's (fixed in batch A).
- **LN2 condenses on chemically strengthened GLAS** in very cold, pressurised air (7fcc3a2c).
- **STOR holds radiation particles** such as PHOT (68a8b055). **DRAY copies fighters** with their own
  fighter slot (858a0bce). **Pipes keep the deco of what they carry** (e2e25682), using pipe flag 0x2;
  saves with such pipes get the same "heatPipes" marker as heat pipes so the flag survives loading.
- **Not ported:** RIME/FOG remembering DSTW (needs upstream's CarriesTypeIn data), ETRD tmp/tmp2 distances
  (conflicts with this version's ETRD and HUD), TESC lightning deco (needs a renderer flag this version
  lacks), PROP_PHOTPASS (a refactor), PSTN tmp3 (Ultimata uses tmp3 here), SEED (Cracker has its own).
- **Tests** (`upstream-fixes.spec.mjs`, 6): molten SLCN/SALT checkerboard at 2500 K gives 12 LITH in 5 s;
  molten gold in a sealed wall box at -256 pressure gives 170 PTNM in 10 s; RIME ringed by cold GAS ends
  with 102 acidic RIME, melting into 93 ACID; STOR next to a PHOT holds it. On the production build (before
  this) the LITH check fails with 0, and the heat capacity test fails as the property doesn't exist.
  Challenges re-checked: ASan 50/50 solutions; browser 2 solution + 1 do-nothing runs each, all as expected.

## 22. Batches D and E (views, tools, Lua): what was taken

- **Taken:** binary values in PROP and PRP2 (`0b1011`, upstream ccb671bd, with the crash fix bff38ce6 done as
  a strict parser that rejects anything but 0/1 digits). Test: PRP2 with `life:0b1011` and
  `tmp:0b11111111` sets 11 and 255.
- **Not taken, and why:** the dynamic/variable heat display and the destructibility view (they extend upstream's
  rewritten renderer, which this mod replaced with Cracker's and Ultimata's own display modes); stamp search
  (upstream's stamp browser was redesigned); FPS/draw cap settings (the browser sets the frame rate here);
  `beforesimdraw`/`aftersimdraw` events (they need Lua drawing redirected into the simulation buffer; this
  version draws Lua graphics on the interface layer); Lua-defined tools, `MenuSort` and `!get` (they build on
  upstream's reworked Lua API, which this mod's scripts do not use). Each could be done on its own later.

## 23. Threads: a threaded build for cross-origin isolated pages

- **Phase 0, measured** (`perf.spec.mjs` gained a gravity scene; `tpt_benchmark` includes the gravity
  update): Newtonian gravity only costs anything when its field changes, i.e. with moving gravity sources
  (GRVT, eating black holes). Then it takes about 2-4 ms per frame on the main thread (scene of GRVT and NBHL:
  1.2 ms per frame without gravity, 3.5 ms with). Most other time is the particle loop, which TPT runs on one
  thread on every platform; rendering is 0.3-3 ms.
- **Phase 1, spike:** a small JSPI + pthreads program (a worker counting while the main thread sleeps
  through JSPI) works in Chromium on a cross-origin isolated page (counter 81k -> 444k over five 200 ms
  sleeps, thread joined) and fails without isolation, as expected (SharedArrayBuffer not allowed).
- **Phase 2, built:** `WASM_THREADS=1` builds a second sysroot with `-pthread` (Emscripten's `-mt` port
  variants for SDL2/libpng) and the game with `-Dweb_threads=true` (`-pthread`, a pool of 2 workers created
  up front, `TPT_WEB_THREADS`), into `build-wasm-mt/`; `dist/` carries it as `powder-mt.js`/`.wasm` next to
  the single-threaded build. Only the gravity thread runs for real; background tasks and search stay on the
  main thread (their file system access would cross threads). The site now sends COOP/COEP headers
  (`wasm/vercel.json`), the page picks `powder-mt.*` when `crossOriginIsolated` (and `?threads=0` forces the
  single-threaded build); without the headers or offline it falls back. The offline ZIP only has the
  single-threaded build. CI builds both and validates both binaries.
- **Tests** (`threads.spec.mjs`): over HTTP the page is isolated, downloads `powder-mt.wasm` and runs
  threaded; a dust particle 50 px from a block of black holes is pulled in and eaten in 2 s in both builds;
  with a changing gravity field, gravity adds 0.04 ms per frame to the main thread threaded and 2.3 ms
  single-threaded (`?threads=0`). With the COOP/COEP headers stripped from every response the page is not
  isolated, uses the single-threaded build and never downloads `powder-mt.*`.
- **Not done:** upstream's render thread (it needs upstream's renderer split); the gain would be the
  0.3-3 ms of rendering per frame.
