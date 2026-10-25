# shellcheck shell=bash
# Shared environment for the WebAssembly build. Source this, don't run it.
#
#   EMSDK_DIR     where emsdk lives (default: <repo>/.cache/emsdk); installed on first use
#   WASM_WORK     scratch dir for dependency sources/builds (default: <repo>/build-wasm-deps)
#   WASM_PREFIX   install prefix for the dependency sysroot (default: $WASM_WORK/prefix)

WASM_REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TPT_EMSDK_VERSION="$(tr -d '[:space:]' < "$WASM_REPO_ROOT/wasm/emsdk-version.txt")"
: "${EMSDK_DIR:=$WASM_REPO_ROOT/.cache/emsdk}"
: "${WASM_WORK:=$WASM_REPO_ROOT/build-wasm-deps}"
: "${WASM_THREADS:=0}"
if [ "$WASM_THREADS" = 1 ]; then
	: "${WASM_PREFIX:=$WASM_WORK/prefix-mt}"
else
	: "${WASM_PREFIX:=$WASM_WORK/prefix}"
fi
export EMSDK_DIR WASM_WORK WASM_PREFIX TPT_EMSDK_VERSION WASM_REPO_ROOT WASM_THREADS

if [ ! -x "$EMSDK_DIR/emsdk" ]; then
	echo "[env] cloning emsdk into $EMSDK_DIR"
	git clone --quiet https://github.com/emscripten-core/emsdk.git "$EMSDK_DIR"
fi
if ! grep -qs "$TPT_EMSDK_VERSION" "$EMSDK_DIR/.emscripten_version_installed" 2>/dev/null; then
	echo "[env] installing emsdk $TPT_EMSDK_VERSION"
	(cd "$EMSDK_DIR" && ./emsdk install "$TPT_EMSDK_VERSION" && ./emsdk activate "$TPT_EMSDK_VERSION") || return 1
	echo "$TPT_EMSDK_VERSION" > "$EMSDK_DIR/.emscripten_version_installed"
fi
# shellcheck disable=SC1091
source "$EMSDK_DIR/emsdk_env.sh" > /dev/null 2>&1

actual="$(emcc --version | head -n1 | sed -E 's/.* ([0-9]+\.[0-9]+\.[0-9]+) .*/\1/')"
if [ "$actual" != "$TPT_EMSDK_VERSION" ]; then
	echo "[env] expected emcc $TPT_EMSDK_VERSION, got $actual" >&2
	return 1
fi

export PKG_CONFIG_PATH="$WASM_PREFIX/lib/pkgconfig"
export PKG_CONFIG_LIBDIR="$WASM_PREFIX/lib/pkgconfig"
