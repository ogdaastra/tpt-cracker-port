# shellcheck shell=bash
# Compile flags shared by the dependency sysroot and the game. All C++ code in the final binary must
# agree on the exception model: native WebAssembly exceptions (-fwasm-exceptions), which JSPI supports
# and which also selects wasm-based setjmp/longjmp (used by libpng error handling).
DEP_CFLAGS="-O2 -fwasm-exceptions"
export DEP_CFLAGS
