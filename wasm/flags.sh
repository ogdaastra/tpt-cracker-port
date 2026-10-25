# shellcheck shell=bash
# Compile flags shared by the dependency sysroot and the game. All C++ code in the final binary must
# agree on the exception model: native WebAssembly exceptions (-fwasm-exceptions), which JSPI supports
# and which also selects wasm-based setjmp/longjmp (used by libpng error handling).
DEP_CFLAGS="-O2 -fwasm-exceptions"
# WASM_THREADS=1: the threaded variant (section 5 of docs/NEXT-PLANS.md). Everything linked into a
# shared-memory (pthreads) binary must be compiled with -pthread, so it gets its own sysroot.
if [ "${WASM_THREADS:-0}" = 1 ]; then
	DEP_CFLAGS="$DEP_CFLAGS -pthread"
fi
export DEP_CFLAGS
