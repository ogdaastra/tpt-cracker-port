#!/bin/bash
# usage: chal-run.sh LOG [IDS] [MODE]
cd ${DDIR:-/root/tools/chal-ddir} && rm -f powder.pref challenges-progress.json
IDS="$2" MODE="${3:-solve}" ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=print_stacktrace=1 SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy timeout 3000 /root/tools/build-asan/powder ddir ${DDIR:-/root/tools/chal-ddir} > "$1" 2>&1
echo "exit=$?"
grep -E 'RESULT|DONE|error|ERROR: AddressSanitizer|runtime error' "$1" | grep -v FontReader | head -80
