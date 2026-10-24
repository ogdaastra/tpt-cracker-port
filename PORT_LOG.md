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
session. Emscripten's ports (SDL2, zlib, libpng) normally download GitHub archives, so the build script
supports `EMCC_LOCAL_PORTS` with git clones as a fallback. lua.org, fftw.org, sourceware.org,
storage.googleapis.com, registry.npmjs.org are reachable.
