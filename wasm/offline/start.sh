#!/bin/sh
# Starts a tiny local web server for the game (Python 3 or Node.js) and opens it in the browser.
cd "$(dirname "$0")" || exit 1
if command -v python3 >/dev/null 2>&1; then exec python3 serve.py; fi
if command -v node >/dev/null 2>&1; then exec node serve.mjs; fi
echo "Neither python3 nor node found; opening index.html directly from disk instead."
(xdg-open index.html || open index.html) >/dev/null 2>&1
