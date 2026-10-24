#!/usr/bin/env bash
# Builds the dependency sysroot for the WebAssembly build of TPT / Cracker1000's Mod.
#
#   * Lua 5.1.5        built from source, compiled as C++ (exceptions instead of longjmp), C linkage
#   * jsoncpp 1.9.5    built from source
#   * fftw 3.3.10      built from source, single precision (fftw3f), only if GRAVFFT=1 (default)
#   * SDL2, zlib, libpng, bzip2   Emscripten ports, exposed to Meson through pkg-config files
#
# Every source is pinned by sha256 (tarballs) or commit id (git).
#
# Environment:
#   GRAVFFT=0            skip fftw3f (then configure the game with -Dgravfft=false)
#   TPT_SEED_PORTS=1     pre-seed Emscripten's port cache from pinned git tags / tarballs instead of
#                        letting emcc download GitHub archives (needed where github.com/.../archive/
#                        is blocked, e.g. some sandboxes). Same versions, same sources.
set -euo pipefail

# shellcheck source=wasm/env.sh
source "$(dirname "${BASH_SOURCE[0]}")/env.sh"
source "$WASM_REPO_ROOT/wasm/flags.sh"

GRAVFFT="${GRAVFFT:-1}"
SRC="$WASM_WORK/src"
mkdir -p "$SRC" "$WASM_PREFIX/include" "$WASM_PREFIX/lib/pkgconfig"
JOBS="$(nproc 2>/dev/null || echo 4)"

log() { echo "[build-deps] $*" >&2; }

fetch_tarball() { # url sha256 -> path
	local url="$1" sha="$2" out="$SRC/$(basename "$1")"
	if [ ! -f "$out" ] || ! echo "$sha  $out" | sha256sum -c --quiet - 2>/dev/null; then
		log "downloading $url"
		curl -fsSL --retry 2 -o "$out.part" "$url"
		mv "$out.part" "$out"
	fi
	echo "$sha  $out" | sha256sum -c --quiet - || { echo "checksum mismatch for $out" >&2; exit 1; }
	echo "$out"
}

fetch_git() { # url tag commit dest
	local url="$1" tag="$2" commit="$3" dest="$4"
	if [ ! -d "$dest" ]; then
		log "cloning $url @ $tag"
		git -c advice.detachedHead=false clone --quiet --depth 1 --branch "$tag" "$url" "$dest"
	fi
	local actual
	actual="$(git -C "$dest" rev-parse HEAD)"
	if [ "$actual" != "$commit" ]; then
		echo "commit mismatch for $url $tag: expected $commit got $actual" >&2
		exit 1
	fi
}

# ---------------------------------------------------------------- Emscripten ports
EM_CACHE="$(em-config CACHE)"
seed_port() { # name marker-url subdir source-dir
	local name="$1" url="$2" subdir="$3" source="$4" dir="$EM_CACHE/ports/$1"
	if [ -f "$dir/.emscripten_url" ] && [ "$(cat "$dir/.emscripten_url")" = "$url" ] && [ -d "$dir/$subdir" ]; then
		return
	fi
	log "seeding port $name from $source"
	rm -rf "$dir"
	mkdir -p "$dir"
	cp -a "$source" "$dir/$subdir"
	rm -rf "$dir/$subdir/.git"
	echo "$url" > "$dir/.emscripten_url"
}

if [ "${TPT_SEED_PORTS:-0}" = 1 ]; then
	# These must match the versions in $EMSDK/upstream/emscripten/tools/ports/*.py for emsdk 4.0.23.
	fetch_git https://github.com/libsdl-org/SDL.git release-2.32.8 98d1f3a45aae568ccd6ed5fec179330f47d4d356 "$SRC/SDL-release-2.32.8"
	seed_port sdl2 "https://github.com/libsdl-org/SDL/archive/release-2.32.8.zip" SDL-release-2.32.8 "$SRC/SDL-release-2.32.8"
	zlib_tgz="$(fetch_tarball https://zlib.net/fossils/zlib-1.3.1.tar.gz 9a93b2b7dfdac77ceba5a558a580e74667dd6fede4585b91eefb60f03b72df23)"
	rm -rf "$SRC/zlib-1.3.1" && tar -C "$SRC" -xzf "$zlib_tgz"
	seed_port zlib "https://github.com/madler/zlib/archive/refs/tags/v1.3.1.tar.gz" zlib-1.3.1 "$SRC/zlib-1.3.1"
	fetch_git https://github.com/emscripten-ports/bzip2.git 1.0.6 60ce9dfe9a75f7ee92956aba8a83d1a37625bc1d "$SRC/bzip2-1.0.6"
	seed_port bzip2 "https://github.com/emscripten-ports/bzip2/archive/1.0.6.zip" bzip2-1.0.6 "$SRC/bzip2-1.0.6"
fi

log "building Emscripten ports"
# Build the exact variants the final link will use (wasm exceptions => wasm setjmp/longjmp for libpng).
embuilder build sdl2 zlib bzip2 libpng-legacysjlj

write_pc() { # name version cflags libs
	cat > "$WASM_PREFIX/lib/pkgconfig/$1.pc" <<EOF
prefix=$WASM_PREFIX
Name: $1
Description: $1 (Emscripten)
Version: $2
Cflags: $3
Libs: $4
EOF
}
write_pc sdl2     2.32.8 "-sUSE_SDL=2"   "-sUSE_SDL=2"
write_pc zlib     1.3.1  "-sUSE_ZLIB=1"  "-sUSE_ZLIB=1"
write_pc libpng16 1.6.39 "-sUSE_LIBPNG=1" "-sUSE_LIBPNG=1"
write_pc bzip2    1.0.6  "-sUSE_BZIP2=1" "-sUSE_BZIP2=1"

# ---------------------------------------------------------------- Lua 5.1.5 (as C++)
if [ ! -f "$WASM_PREFIX/lib/liblua5.1-c++.a" ]; then
	lua_tgz="$(fetch_tarball https://www.lua.org/ftp/lua-5.1.5.tar.gz 2640fc56a795f29d28ef15e13c34a47e223960b0240e8cb0a82d9b0738695333)"
	rm -rf "$SRC/lua-5.1.5" && tar -C "$SRC" -xzf "$lua_tgz"
	cd "$SRC/lua-5.1.5/src"
	# Compiled as C++ so lua_error() unwinds with C++ exceptions (LUAI_THROW in luaconf.h) rather than
	# longjmp, which would skip destructors of the game's C++ frames. This is what the meson.build
	# 'lua5.1-c++' dependency asks for. The public API keeps C linkage because the game includes the
	# Lua headers inside extern "C" (src/lua/LuaCompat.h), and LuaCompat.c is plain C.
	python3 - luaconf.h <<'EOF'
import sys
p = sys.argv[1]
s = open(p).read()
old = "#define LUA_API\t\textern\n"
new = "#ifdef __cplusplus\n#define LUA_API\t\textern \"C\"\n#else\n#define LUA_API\t\textern\n#endif\n"
assert s.count(old) == 1, "LUA_API definition not found"
open(p, "w").write(s.replace(old, new))
EOF
	log "building Lua 5.1.5 as C++"
	objs=()
	for f in lapi lcode ldebug ldo ldump lfunc lgc llex lmem lobject lopcodes lparser lstate lstring ltable ltm lundump lvm lzio lauxlib lbaselib ldblib liolib lmathlib loslib ltablib lstrlib loadlib linit; do
		em++ -x c++ -std=c++17 $DEP_CFLAGS -c "$f.c" -o "$f.o"
		objs+=("$f.o")
	done
	emar rcs liblua5.1-c++.a "${objs[@]}"
	mkdir -p "$WASM_PREFIX/include/lua5.1"
	cp lua.h luaconf.h lualib.h lauxlib.h ../etc/lua.hpp "$WASM_PREFIX/include/lua5.1/"
	cp liblua5.1-c++.a "$WASM_PREFIX/lib/"
	cd - > /dev/null
fi
write_pc lua5.1-c++ 5.1.5 "-I$WASM_PREFIX/include/lua5.1" "-L$WASM_PREFIX/lib -llua5.1-c++"

# ---------------------------------------------------------------- jsoncpp 1.9.5
if [ ! -f "$WASM_PREFIX/lib/libjsoncpp.a" ]; then
	fetch_git https://github.com/open-source-parsers/jsoncpp.git 1.9.5 5defb4ed1a4293b8e2bf641e16b156fb9de498cc "$SRC/jsoncpp-1.9.5"
	log "building jsoncpp 1.9.5"
	cd "$SRC/jsoncpp-1.9.5"
	mkdir -p build-em
	for f in json_reader json_value json_writer; do
		em++ -std=c++17 $DEP_CFLAGS -Iinclude -Isrc/lib_json -c "src/lib_json/$f.cpp" -o "build-em/$f.o"
	done
	emar rcs build-em/libjsoncpp.a build-em/json_reader.o build-em/json_value.o build-em/json_writer.o
	rm -rf "$WASM_PREFIX/include/json"
	cp -r include/json "$WASM_PREFIX/include/json"
	cp build-em/libjsoncpp.a "$WASM_PREFIX/lib/"
	cd - > /dev/null
fi
write_pc jsoncpp 1.9.5 "-I$WASM_PREFIX/include" "-L$WASM_PREFIX/lib -ljsoncpp"

# ---------------------------------------------------------------- fftw 3.3.10 (float)
if [ "$GRAVFFT" = 1 ] && [ ! -f "$WASM_PREFIX/lib/libfftw3f.a" ]; then
	fftw_tgz="$(fetch_tarball https://www.fftw.org/fftw-3.3.10.tar.gz 56c932549852cddcfafdab3820b0200c7742675be92179e59e6215b340e26467)"
	rm -rf "$SRC/fftw-3.3.10" && tar -C "$SRC" -xzf "$fftw_tgz"
	log "building fftw 3.3.10 (single precision)"
	cd "$SRC/fftw-3.3.10"
	CFLAGS="$DEP_CFLAGS" emconfigure ./configure --prefix="$WASM_PREFIX" \
		--enable-float --enable-static --disable-shared --disable-fortran --disable-threads \
		--disable-openmp --disable-doc > configure.log 2>&1 || { tail -40 configure.log; exit 1; }
	emmake make -j"$JOBS" > make.log 2>&1 || { tail -40 make.log; exit 1; }
	emmake make install > install.log 2>&1 || { tail -40 install.log; exit 1; }
	cd - > /dev/null
fi

log "sysroot ready in $WASM_PREFIX"
ls -1 "$WASM_PREFIX/lib/pkgconfig"
