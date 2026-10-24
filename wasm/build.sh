#!/usr/bin/env bash
# Configures and builds the WebAssembly version of the game into build-wasm/, then assembles the
# static site in dist/.
#
#   ./wasm/build-deps.sh   # once: dependency sysroot
#   ./wasm/build.sh        # configure (first time) + build + assemble dist/
#
# Environment:
#   BUILD_DIR      build directory (default: build-wasm)
#   BUILDTYPE      meson buildtype (default: release)
#   RECONFIGURE=1  wipe and reconfigure the build directory
#   GRAVFFT=0      build without fftw (must match build-deps.sh)
set -euo pipefail

# shellcheck source=wasm/env.sh
source "$(dirname "${BASH_SOURCE[0]}")/env.sh"
cd "$WASM_REPO_ROOT"

BUILD_DIR="${BUILD_DIR:-build-wasm}"
BUILDTYPE="${BUILDTYPE:-release}"
GRAVFFT="${GRAVFFT:-1}"
MESON="${MESON:-meson}"

if [ "${RECONFIGURE:-0}" = 1 ]; then
	rm -rf "$BUILD_DIR"
fi

if [ ! -f "$BUILD_DIR/build.ninja" ]; then
	gravfft_opt=true
	[ "$GRAVFFT" = 1 ] || gravfft_opt=false
	"$MESON" setup "$BUILD_DIR" \
		--cross-file cross-examples/emscripten.ini \
		--buildtype="$BUILDTYPE" \
		-Db_ndebug=if-release \
		-Dstatic=none \
		-Dlua=lua5.1 \
		-Dhttp=false \
		-Dgravfft="$gravfft_opt" \
		-Dignore_updates=true \
		-Dworkaround_elusive_bzip2=false
fi

"$MESON" compile -C "$BUILD_DIR"

./wasm/assemble-dist.sh "$BUILD_DIR"
