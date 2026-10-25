# Plans: recording, Ultimata tools, upstream features, threads, GPU rendering, challenges

Six features asked for on 24 Oct 2026 (ideas #2, #5, #6, #8, #9, #11 from the idea list). This file is the
plan only; nothing here is built yet. Each section says what it is, how it would be built, how it would be
tested, what could go wrong, and roughly how big it is.

**Suggested order** (small and safe first, risky and slow last):

| # | Feature | Size | Why this position |
|---|---|---|---|
| 1 | Record GIF / video (#2) | small | quick win, page-only, no game changes |
| 2 | Ultimata's missing tools (#5) | medium | code already exists in Ultimata, same TPT base |
| 3 | Challenge mode (#11) | large (mostly content) | needs a small game API, then lots of challenges |
| 4 | Upstream TPT features (#6) | large, done in batches | ongoing; each batch is its own PR |
| 5 | Threads (#8) | large, risky | needs a spike first; may hit a hard limit |
| 6 | GPU rendering (#9) | large | best after threads; smaller payoff than it sounds |

Ground rules for all of them (same as so far): every feature gets browser tests, the PERF benchmark is
run when the simulation or renderer changes, ASan runs when C++ simulation code changes, and nothing is
pushed, merged or deployed without asking first.

---

## 1. Record a GIF or video (#2)

**What you get:** a **Record** button in the page header. Choose *Video* (MP4/WebM, smooth, small file) or
*GIF* (plays anywhere, bigger file), *simulation only* or *whole window*, and a length (5/10/20 s, or stop
manually). When it stops, the file downloads. A red dot and timer show while it records. Keyboard
shortcut on the page: `Ctrl+Shift+R`.

**How:**
- *Video:* `canvas.captureStream(60)` + `MediaRecorder`. Pick the best supported type: MP4/H.264
  (Chrome/Edge 126+) else WebM/VP9. Cropping to the simulation area: copy each frame into a hidden
  612x384 canvas and record that one.
- *GIF:* the game canvas is WebGL, so frames must be copied right after the game draws them. Add a tiny hook
  in `src/PowderToySDL.cpp` after `blit()` that calls `Module.tptOnFrame()` when recording; the page copies
  the canvas into a 2D canvas there. Frames go to a Web Worker that encodes with **gifenc** (MIT licence,
  ~10 KB, vendored with its licence in `THIRD_PARTY_NOTICES.txt`). 20 fps, palette per frame, optional
  half-size to keep files small.
- Also expose the game's own screenshot (F2 writes a PNG into the virtual file system): the page offers it
  as a download.
- Files: `web/tpt-web.js` (recorder module), `web/index.html`, `web/style.css`, `web/gif-worker.js`,
  `wasm/assemble-dist.sh` (copy the worker), `src/PowderToySDL.cpp` (frame hook).

**Tests** (`tests/browser/record.spec.mjs`): record 2 s of a running scene as video → the Blob has the
expected type, size > 0, and a `<video>` element reads a duration of ~2 s; record as GIF → the file starts
with `GIF89a` and contains ≥ 30 frames; simulation-only crop gives 612x384; recording stops cleanly when the
game shows a dialog. Offline file:// copy too (MediaRecorder works there).

**Risks:** GIF encoding of 20 s at full size is slow and large → cap length/size, show progress. Firefox and
Safari don't run this build anyway (no JSPI), so only Chromium matters.

---

## 2. TPT Ultimata's missing tools (#5)

Ultimata has these UI pieces that were not ported (checked in its source, `src/gui/game/`):

| Ultimata piece | What it does |
|---|---|
| **CNFG** config tool (`ConfigTool.cpp` + `config_tool/*.h`) | click a particle to configure it with a proper dialog: WIFI/QLOG channels, CRAY/DRAY, detectors (ctype, range), FFGN, FRME, PSTN, TRBN, CLUD, temperature, wavelength, LIFE rules, quantum |
| **PRP2** multi-property tool (`PropertyTool2.cpp`) | set several properties at once while drawing |
| **TEXT** tool (`TextTool.cpp` + `texterfonts/`) | type text and place it as particles, several fonts |
| **RULR** ruler (`RulerTool.cpp`) | click two points, shows distance and angle |
| **TRFM** transform (`TransformTool.cpp`) | select a rectangle, then rotate and scale it |
| Triangle and hollow brushes | extra brush shapes (Tab cycles) |
| Field quick options | switches for time-dilation view, EM field on/off, electric view, magnetic view, stress on/off, stress view (the renderer code for these views is already in our build, only the switches are missing) |

**How:**
- Ultimata and Cracker are based on the same official TPT commit, so the files port nearly as they are
  (the same kind of small API fixes as for the elements: `RNG::Ref()`, `PIXPACK`, renderer calls).
- Tools go in the Tools menu and the Elements panel. The six field switches go into the **second side-bar
  column, above the five new menus** (that space, y 1-135, is free), so the classic column and the theme
  stay unchanged.
- Lua access, as Ultimata has: `tpt.stress_enable()`, `tpt.emfield_enable()` etc. if missing.
- Files: `src/gui/game/{ConfigTool,PropertyTool2,TextTool,RulerTool,TransformTool}.cpp`, `config_tool/`,
  `texterfonts/`, `Tool.h`, `GameModel.cpp` (register tools, brushes), `QuickOptions.{h,cpp}`,
  `GameView.cpp` (second-column quick options), `meson.build`.

**Tests** (`tests/browser/ultimata-tools.spec.mjs`): each tool driven through the real UI. TEXT writes "HI"
and the right particles appear; RULR shows the right length for a known drag; TRFM rotates a 10x20 block by
90° into a 20x10 block; CNFG changes a WIFI channel through its dialog (checked with Lua); PRP2 sets temp +
life together; every field switch flips the matching Lua state and changes the picture (pixel check);
brushes cycle through the new shapes. ASan run with the tools exercised.

**Risks:** Ultimata's own notes call it buggy. Expect the same kind of memory bugs as with its elements, and
plan an ASan pass. TRFM on huge selections can be slow; limit size.

---

## 3. Challenge mode (#11): lots of challenges

**Status: built** (50 challenges, see `docs/CHALLENGES.md` and PORT_LOG section 18). The list below was the
starting plan; some were changed or replaced while writing and testing the reference solutions.

### What you get
A **Challenges** button on the page opens a panel: categories, each challenge with a title, a short
description, difficulty (1-5), your best stars (★ to ★★★), and Play. Playing loads the challenge's scene, limits
which elements/tools you may use and how many particles you may place (the "ink" budget), shows a live
goal bar in the game (top of the screen), and a timer. On success: stars based on time and ink used, saved in
the browser, next challenge unlocked in that category. Hints (up to 3 per challenge) cost a star. Reset and
Quit buttons. Works offline.

### How it's built
- **Game side (C++, small):** a "challenge session" in `GameModel`:
  - *Allowed tools list.* When set, menus and the Elements panel show only those tools, and selecting any
    other tool (search, Lua, keyboard) is refused.
  - *Ink counter.* Particles created by the player's tools (not by reactions) are counted;
    drawing stops when the budget is used up.
  - *Locked areas.* Rectangles the player's tools cannot change (so the puzzle pieces can't just be
    erased).
  - Lua API `challenge.start{...}`, `challenge.ink()`, `challenge.finish(stars)`, events to the page via
    `Module.tptOnChallenge(json)`.
- **Challenge definitions (Lua, bundled):** `src/lua/luascripts/challenges.lua`. Each challenge is a table:
  `id, title, category, difficulty, text, hints, setup()` (builds the scene with `sim.partCreate`, walls,
  or an embedded stamp), `allowed = {...}`, `ink = N`, `time = seconds`, `check(tick) -> progress 0..1,
  failed?`, star thresholds. Adding a challenge = adding one table.
- **Page side:** `web/challenges.js` (panel, progress in `localStorage`, unlocks).
- **Every challenge ships with a reference solution** (`solution()` in Lua that places a valid answer within the
  rules). CI runs: (a) solution → must reach 100% and succeed, (b) doing nothing → must not succeed within
  the time limit. This proves every challenge is winnable and that its checker isn't trivially satisfied.
  Challenges whose mechanics turn out unreliable during this are fixed or dropped, never shipped untested.

### The challenges (56, in 8 categories)
Checks use only what Lua can read: particle counts/types in regions, temperatures, pressure, sparks
(SPRK/LED life), positions over time.

**Heat (H)**
1. *Kettle* (1★): turn 90% of a WATR pool into steam. Allowed: FIRE, PLSM. Ink 60.
2. *Deep freeze* (1★): freeze all water in a tank. Allowed: LN2, CRYO-like coolants. Ink 150.
3. *Glassworks* (2★): make 400 GLAS from the sand pile. Allowed: LAVA, THRM, FIRE. Ink 80.
4. *Heat pipe delivery* (3★): heat the far target block above 500 °C; a wall of INSL is in the way. Allowed: PIPE, HEAC, BRCK. Ink 200. (Uses the new heat pipes.)
5. *Don't melt* (2★): a LAVA river flows for 30 s; the METL bridge must keep 95% of its particles. Allowed: INSL, DMND-like insulators. Ink 40.
6. *Thermostat* (3★): keep a water tank between 40 and 60 °C for 30 s while a heater runs. Allowed: COND, ICEI, PTCT, NTCT, METL. Ink 120.
7. *Steam power* (4★): light the LED using only steam pressure (no BTRY/SPRK). Allowed: WATR, FIRE, TURB, METL. Ink 150.

**Electronics (E)**
8. *First light* (1★): light the LED. Allowed: BTRY, METL. Ink 60.
9. *Blinker* (2★): make the LED switch on and off at least 5 times in 20 s. Allowed: BTRY, METL, PSCN, NSCN, DLAY, SWCH. Ink 120.
10. *NOT gate* (2★): the output LED must be the inverse of the input pad (a scripted test pattern of 8 steps).
11. *AND gate* (3★), 12. *OR gate* (2★), 13. *XOR gate* (4★): same, two inputs, all 4 combinations tested.
14. *Through the wall* (2★): get a spark to the far side of an INSL wall. Allowed: WIFI, METL, BTRY. Ink 40.
15. *Clock* (3★): spark the output pad regularly, every 30 ± 5 frames, for 10 periods.
16. *Ohm's corner* (3★, Ultimata circuits): make current flow through the RSTR so RSPK reaches the target. Allowed: VOLT, METL, ZINC, PSCN, NSCN. Ink 100.
17. *Three-note tune* (2★): play NOTE C, E, G in that order. Allowed: NOTE, METL, BTRY, DLAY. Ink 100.

**Explosives (X)**
18. *Demolition* (1★): destroy 95% of the brick tower. Allowed: BOMB, GUNP, FIRE. Ink 30.
19. *Surgical strike* (3★): destroy the tower but keep 95% of the house next to it. Ink 25.
20. *Chain reaction* (2★): set off all 5 separate bombs with one spark. Allowed: FUSE, IGNT, SPRK (1 use).
21. *Fireworks show* (2★): 3 firework bursts above the line within 20 s. Allowed: FIRW, FWRK, FIRE.
22. *Hotter than the sun* (4★): reach 9000 °C anywhere. Allowed: URAN, PLUT, DEUT, NEUT. Ink 200.

**Chemistry and liquids (C)**
23. *Fill it up* (1★): fill the glass to the line without spilling (WATR in the glass ≥ target, on the floor < 20).
24. *Desalination* (2★): make 200 DSTW from the salt water.
25. *Neutralise* (2★): remove every ACID particle without breaking the container. Allowed: BASE, WATR, SOAP. Ink 150.
26. *Salt flats* (2★): get 100 SALT out of the SLTW.
27. *Rust bucket* (3★): rust 100 IRON into BRMT.
28. *Make TNT* (2★): make 50 BANG (NITR + CLST).
29. *Electrolysis* (3★): split water into at least 50 H2 and 50 O2.
30. *Neon sign* (2★): make PLSM from NBLE with sparks.

**Machines and force (P)**
31. *Sorting* (3★): separate the mixed DUST and WATR into the two bins, 90% purity each.
32. *Uphill water* (2★): move 200 WATR from the low tank to the high tank. Allowed: PUMP, PIPE, PPIP, FRAY. Ink 150.
33. *Pipe dream* (2★): route 100 particles through the maze into the goal box with PIPE.
34. *Press* (2★): push the block 50 px to the right with PSTN.
35. *Basketball* (3★): get 20 DUST into the basket using fans and accelerators only.
36. *Pressure cooker* (2★): keep pressure above 50 in the chamber for 10 s.
37. *Vacuum* (2★): get pressure below -100 in the box.
38. *Portal plumbing* (3★): get 300 WATR into the sealed tank using PRTI/PRTO.

**Gravity and space (G)**
39. *Orbit* (4★): keep one particle orbiting the black hole for 10 s (stays in the ring zone).
40. *Slingshot* (4★): hit the target box using a gravity well to bend the path.
41. *Floating island* (3★): hold 100 SAND above the line for 15 s. Allowed: GRAV wall, GPMP, ACEL.
42. *Time warp* (3★): slow the falling sand so none reaches the floor for 20 s. Allowed: TIME, CBMB. (Ultimata time dilation.)

**Life and nature (L)**
43. *Green thumb* (1★): grow 500 PLNT from one SEED.
44. *Ant farm* (2★): ANT dig a tunnel from the top of the soil to the bottom.
45. *Aquarium* (3★): keep 5 FISH alive for 60 s.
46. *Beekeeper* (3★): make 50 HONY.
47. *Glider* (2★): build a Game of Life glider that travels 50 px.
48. *Outbreak* (3★): stop the virus before it reaches the city (no VIRS in the city zone for 30 s). Allowed: SOAP, CURE-type elements, walls.

**Characters (S)**
49. *Commute* (2★): get STKM from the left flag to the right flag alive (you steer).
50. *Arena* (3★): defeat 3 FIGH.
51. *Tank course* (3★): drive the TANK to the finish line.

**Puzzles and records (M)**
52. *Minimalist* (★ by ink): light the LED with as few particles as possible (3★ ≤ 10).
53. *Speedrun* (★ by time): melt the 50x50 IRON block as fast as you can.
54. *Faraday's cage* (3★): keep the circuit from being sparked during 20 s of lightning. Allowed: FARADAY wall, INSL.
55. *Heat pipe relay* (4★): carry heat 400 px through three heat pipes to boil the far water.
56. *Grand finale* (5★): power a lamp, boil water and launch a firework from one single spark.

The list is a starting set. Numbers (ink, times, thresholds) are tuned while writing each reference solution.

**Tests:** `tests/browser/challenges.spec.mjs` runs every challenge's reference solution and its
do-nothing case in the real game (about 2-3 minutes of game time in total, split across CI jobs if needed),
plus UI tests: panel lists all challenges, stars save and survive reload, a forbidden element can't be
selected (search, Lua, keys), the ink budget stops drawing, locked areas can't be erased.

**Risks:** some mechanics behave randomly (fire, plants, creatures). Those checks use thresholds with margins
and get tested many times (10 runs) before shipping. Lua challenge scripts run with full Lua access (they are
part of the game, not user content).

---

## 4. Features from newer official TPT (#6)

**Status: done for what applies** (PORT_LOG sections 19-22): batch A crash/correctness fixes, batch B heat
capacity, vorticity confinement and caps, batch C reactions, and PROP binary values from batch D. The rest of D
and E, and the parts of A-C that patch upstream's rewritten air/gravity/renderer code, are listed there as not
ported, with the reason.

This mod is based on official TPT from January 2023 (around snapshot 358, before 98.0). Official TPT has
since released 98.0 to 100.1 (latest: snapshot 401, Oct 2026). Its new elements (RSST/RSSS, BASE, SEED,
LITH) already exist here in Cracker's/Ultimata's own versions, so the useful part is fixes, physics,
reactions and tools. Port them **by hand in batches**, not as a merge: upstream rewrote a lot (C++20,
`SimulationData`, per-simulation RNG, a separate render thread), and Cracker and Ultimata changed many of
the same files.

**Batch A: crash and correctness fixes** (highest value, low risk)
- two rare pmap corruption fixes; infinite loop on NaN velocity; temperatures going below absolute zero (MERC crash)
- infinite air heat near the edges; gases drifting down-right; water levelling lag (algorithm bug); asymmetric gas HotAir
- ACID eating one extra particle; PHOT odd colours; EMP radius; stacking near power-of-two positions
- gravity wall leaking for a frame on undo; SOAP in non-loop edge modes; saves one cell too large; stamp nudge rounding
- crash fixes for corrupted saves and bad Lua calls (100.1)

**Batch B: physics**
- per-element **heat capacity** (then heat pipes mix temperatures properly, as upstream)
- ambient heat convection options, capped air temperature, air heat not updated under insulators
- **vorticity confinement** (swirlier air) with its display mode (Shift+0)
- ambient air pressure/velocity controls; gravity stored in saves
- velocity caps on ACEL, FRAY, RPEL; pressure caps on DEST, LIGH, SING, THDR, WARP

**Batch C: reactions and elements**
- AMTR from PROT + TUNG; PTNM from molten gold at low pressure; LITH from molten SLCN + SALT
- FOG/RIME absorb gases and turn acidic; LN2 condenses on cold GLAS; STOR stores radiation particles
- photon-transparency property (PROP_PHOTPASS); decoration kept through pipes; PSTN `tmp3` flags
- BCLN/PBCN can clone FOG; DRAY copies fighters correctly; ETRD distance via tmp/tmp2; SEED growing on SPNG

**Batch D: views and tools**
- dynamic heat display (Shift+6), destructibility view, variable heat display scale
- stamp browser search; PROP tool binary values; FPS/draw cap in settings; grid checkerboard

**Batch E: Lua**
- Lua-defined tools (`elem.allocate` for tools), `MenuSort`, `beforesimdraw`/`aftersimdraw` events, `!get`

**Not taken:** upstream's removal of the CRACKER menu sections (this mod uses them); the April fools
releases (98.1/98.2); the render thread (belongs to section 5).

**How, per batch:** clone upstream, find each change's commit between the base and master
(`git log -S`/`-G` on the relevant code), check whether Cracker or Ultimata changed the same code, port it,
add a test that shows the new behaviour (a reaction happening, a crash no longer happening), run ASan and
the suite. Features that change how saves behave are guarded the same way as heat pipes (a save marker,
since this game writes save version 97). One PR per batch.

**Risks:** some fixes depend on upstream refactors (e.g. RNG per simulation, `SimulationData`), so they need
rewriting rather than copying. Physics changes (batch B) change how existing saves behave; each will be
listed in the PR so you can veto any.

---

## 5. Threads (#8)

**Honest expectation first.** The profile says most time is the particle update loop, which TPT runs on one
thread on every platform (particles interact with neighbours in order). Threads help with:
- **Newtonian gravity** (FFT, already written as a background thread in TPT, switched off in this build), a big
  win when gravity is on;
- **rendering on its own thread** (upstream's "SRT" since 99.0), up to ~10-15% in heavy scenes;
- air simulation, partly.

So: large gains with Newtonian gravity, moderate otherwise.

**Phase 0: measure.** Add Newtonian-gravity scenes to the PERF benchmark and time gravity, air and render
separately with `tpt_benchmark`.

**Phase 1: spike (decides everything).** Threads in browsers need `SharedArrayBuffer`, which needs the site
to be "cross-origin isolated" (two HTTP headers: `Cross-Origin-Opener-Policy: same-origin` and
`Cross-Origin-Embedder-Policy: require-corp`, set in `vercel.json`). The open question is whether
Emscripten's **JSPI** (how this port lets the game's blocking loop yield to the browser) works together with
**pthreads**. Spike: a small test program with JSPI + `-pthread`, a worker thread, `emscripten_sleep` on the
main thread, then the same with SDL2. Upstream TPT's own web build uses pthreads but a different main-loop
design (no JSPI), so it shows the game's threads work in browsers, not that this combination does.
- If JSPI + pthreads works: Phase 2.
- If not: the alternative is `PROXY_TO_PTHREAD` (the whole game runs in a worker where blocking is allowed,
  drawing through an OffscreenCanvas). That is a much bigger change (input, audio, file system all cross threads);
  I'd stop and ask you before going that way.

**Phase 2: threaded build.**
- Rebuild the dependency sysroot (SDL2, Lua, FFTW, zlib, bzip2, libpng) with `-pthread`.
- Turn on TPT's gravity thread; then port upstream's render thread (ties in with section 4).
- **Two builds**: threaded for the website when the browser is cross-origin isolated, single-threaded as
  fallback and for the offline ZIP (file:// pages can never be cross-origin isolated). The page picks at
  start (`crossOriginIsolated`). CI builds both.
- Memory: shared memory with growth is slower in JS; set a fixed larger initial memory and measure.

**Tests:** full suite on both builds; PERF benchmark with and without gravity, before/after; a test that the
headers are served and `crossOriginIsolated` is true on the deployment; a test that the fallback starts
when headers are missing.

**Risks:** the spike may fail (then ask); the COEP header blocks loading anything from other sites (nothing
on the page does today; keep it that way); Lua scripts touching the simulation from another thread
(upstream solved this with locks around the render copy).

---

## 6. GPU rendering (#9)

**Honest expectation.** In the heavy scenes, drawing costs roughly 10-20% of a frame (`render_parts`
5-9%, `render_fire` 1-6%, plus uploading the picture). Moving that to the GPU saves at most that much,
and each element's look is decided by C++ (and Lua) graphics functions that must stay on the CPU. So this is
a "make the remaining 15% cheaper, and allow nicer effects" job, not a doubling.

**Stage 1: cheaper CPU drawing** (low risk). Profile each display mode; speed up the fire/glow blur loops
(they suit SIMD now that it's enabled); avoid clearing and redrawing layers twice; skip empty regions.

**Stage 2: GPU effects and presentation.**
- Replace SDL's texture upload/present with our own WebGL2 path (SDL still owns the context): the particle
  colour buffer is uploaded once per frame as before, and the **fire, glow, blur and heat-gradient layers
  are computed in shaders** from small textures (fire is stored per 4x4 cell, so its texture is tiny).
- Upstream once had an OpenGL renderer (removed years ago) with shaders for exactly these effects; use it as
  a reference.
- Optional extras once it works: smooth scaling, bloom on hot things, CRT-style filter: all cheap on the GPU.

**Tests:** for every display mode, a screenshot from the CPU renderer and the GPU renderer must match
within a small tolerance (pixel-diff test); PERF benchmark render ms before/after; a switch in settings to
fall back to the CPU renderer.

**Risks:** visual differences in edge cases (pixel tests catch them); WebGL context loss (handle and
restore); software-GL machines (like the CI runners) may be slower with shaders, so keep the CPU path as
default there.

---

## What I need from you before building

1. Order: OK to go 1 → 6 as in the table, one PR per section (section 4 one PR per batch)?
2. Challenges: is the list above the kind of thing you want? Add, remove or reorder freely. Should
   challenges also be reachable inside the game (a menu like Cr-Menu), or is the page panel enough?
3. Upstream batch B changes how some existing saves behave (air and heat physics). Take it, take it as an
   option, or skip it?
