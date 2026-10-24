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
