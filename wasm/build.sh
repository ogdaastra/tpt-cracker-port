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
#   WASM_THREADS=1 the threaded variant into build-wasm-mt/ (needs WASM_THREADS=1 ./wasm/build-deps.sh);
#                  dist/ then holds both builds, see docs/NEXT-PLANS.md section 5
#
# Link-time optimisation (b_lto) made a 72k-particle water/fire scene about a third faster per frame
# (PORT_LOG.md, section 14); an existing build directory keeps its old settings until RECONFIGURE=1.
set -euo pipefail

# shellcheck source=wasm/env.sh
source "$(dirname "${BASH_SOURCE[0]}")/env.sh"
cd "$WASM_REPO_ROOT"

if [ "$WASM_THREADS" = 1 ]; then
	BUILD_DIR="${BUILD_DIR:-build-wasm-mt}"
else
	BUILD_DIR="${BUILD_DIR:-build-wasm}"
fi
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
		-Dworkaround_elusive_bzip2=false \
		-Dweb_threads="$([ "$WASM_THREADS" = 1 ] && echo true || echo false)" \
		-Db_lto=true
fi

"$MESON" compile -C "$BUILD_DIR"

if [ "$WASM_THREADS" = 1 ]; then
	# dist/ is assembled around the single-threaded build, picking this one up as powder-mt.*
	if [ -f build-wasm/powder.wasm ]; then
		MT_BUILD_DIR="$BUILD_DIR" ./wasm/assemble-dist.sh build-wasm
	fi
else
	./wasm/assemble-dist.sh "$BUILD_DIR"
fi
