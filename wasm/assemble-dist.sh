#!/usr/bin/env bash
# Assembles the static site in dist/ from a finished build directory:
#   index.html, style.css, tpt-web.js         page (web/)
#   powder.js, powder.wasm                    the game (Emscripten output)
#   build-info.json                           provenance shown on the page
#   LICENSE.txt, SOURCE.html,                 GPL-3.0 text, where the corresponding source is,
#   THIRD_PARTY_NOTICES.txt, source/          bundled source archives
#   vercel.json                               hosting headers
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$(cd "${1:-$ROOT/build-wasm}" && pwd)"
DIST="${DIST:-$ROOT/dist}"
WORK="${WASM_WORK:-$ROOT/build-wasm-deps}"
cd "$ROOT"

for f in powder.js powder.wasm; do
	[ -f "$BUILD_DIR/$f" ] || { echo "missing $BUILD_DIR/$f" >&2; exit 1; }
done

rm -rf "$DIST"
mkdir -p "$DIST/source"
cp web/index.html web/style.css web/tpt-web.js "$DIST/"
cp "$BUILD_DIR/powder.js" "$BUILD_DIR/powder.wasm" "$DIST/"
cp LICENSE "$DIST/LICENSE.txt"
cp wasm/vercel.json "$DIST/vercel.json"

commit="$(git rev-parse HEAD 2>/dev/null || echo unknown)"
dirty=false
if [ -n "$(git status --porcelain --untracked-files=no 2>/dev/null)" ]; then
	dirty=true
	echo "warning: working tree has uncommitted changes; source archive is HEAD plus source/uncommitted.patch" >&2
fi
emcc_version="$(tr -d '[:space:]' < wasm/emsdk-version.txt)"
wasm_bytes="$(stat -c %s "$DIST/powder.wasm")"
cat > "$DIST/build-info.json" <<EOF
{
	"name": "Cracker1000's Mod v55.1 (The Powder Toy 97.0) for WebAssembly",
	"commit": "$commit",
	"dirty": $dirty,
	"emscripten": "$emcc_version",
	"wasmBytes": $wasm_bytes,
	"built": "$(date -u +%Y-%m-%dT%H:%M:%SZ)"
}
EOF

# Corresponding source (GPL-3.0 section 6): this repository at the built commit...
git archive --format=tar.gz --prefix=tpt-cracker-port/ -o "$DIST/source/tpt-cracker-port-$commit.tar.gz" HEAD
if [ "$dirty" = true ]; then
	git diff HEAD > "$DIST/source/uncommitted.patch"
fi
# ...plus FFTW, the one GPL-licensed library linked into powder.wasm (others are permissive).
if [ -f "$WORK/src/fftw-3.3.10.tar.gz" ]; then
	cp "$WORK/src/fftw-3.3.10.tar.gz" "$DIST/source/"
fi

sed -e "s/@COMMIT@/$commit/g" -e "s/@EMSDK@/$emcc_version/g" web/SOURCE.html > "$DIST/SOURCE.html"

# License texts of everything linked into powder.wasm.
EM_PORTS="$(em-config CACHE 2>/dev/null)/ports"
notice() { # title file
	if [ -f "$2" ]; then
		printf '\n==============================================================================\n%s\n==============================================================================\n\n' "$1"
		cat "$2"
	else
		printf '\n%s: license file not found at build time (%s)\n' "$1" "$2"
	fi
}
{
	echo "Third-party software linked into powder.wasm"
	echo "The game itself (The Powder Toy, Cracker1000's Mod, plus elements and engine code from TPT Ultimata"
	echo "by Bowserinator and elements from Jacob1's Mod) is GPL-3.0, see LICENSE.txt."
	notice "Eigen 3.3.7 (MPL-2.0; a few files BSD or LGPL-2.1, see COPYING.README), used by the TPT Ultimata circuit solver" "src/eigen/COPYING.README"
	notice "Eigen: MPL-2.0" "src/eigen/COPYING.MPL2"
	notice "Eigen: BSD (parts)" "src/eigen/COPYING.BSD"
	notice "Eigen: LGPL-2.1 (parts: SimplicialCholesky, AMD ordering, IncompleteLUT)" "src/eigen/COPYING.LGPL"
	notice "Eigen: MINPACK (parts)" "src/eigen/COPYING.MINPACK"
	notice "Lua 5.1.5 (MIT)" "$WORK/src/lua-5.1.5/COPYRIGHT"
	notice "JsonCpp 1.9.5 (MIT / public domain)" "$WORK/src/jsoncpp-1.9.5/LICENSE"
	notice "FFTW 3.3.10 (GPL-2.0-or-later; source in source/fftw-3.3.10.tar.gz)" "$WORK/src/fftw-3.3.10/COPYRIGHT"
	notice "SDL 2.32.8 (zlib)" "$EM_PORTS/sdl2/SDL-release-2.32.8/LICENSE.txt"
	notice "zlib 1.3.1 (zlib)" "$EM_PORTS/zlib/zlib-1.3.1/LICENSE"
	notice "libpng 1.6.39 (libpng-2.0)" "$EM_PORTS/libpng/libpng-1.6.39/LICENSE"
	notice "bzip2 1.0.6 (bzip2 license)" "$EM_PORTS/bzip2/bzip2-1.0.6/LICENSE"
	notice "Emscripten runtime and system libraries (MIT / UIUC; musl: MIT; libc++: Apache-2.0 WITH LLVM-exception)" "$(dirname "$(command -v emcc)")/LICENSE"
} > "$DIST/THIRD_PARTY_NOTICES.txt"

echo "dist ready: $DIST"
ls -la "$DIST" "$DIST/source"
