#!/usr/bin/env bash
# Packs the built site (dist/) into an offline ZIP: the site itself, powder-wasm.js so index.html also
# works when opened straight from disk (file://), local launchers that need no administrator rights,
# a README, and the GPL source archives already in dist/source/. The reference powder.exe is never
# included. Usage: ./wasm/make-offline-zip.sh [output.zip]
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DIST="$ROOT/dist"
OUT="${1:-$ROOT/artifacts/offline/cracker1000-mod-wasm-offline.zip}"
NAME="cracker1000-mod-wasm"

[ -f "$DIST/powder.wasm" ] || { echo "run ./wasm/build.sh first" >&2; exit 1; }
STAGE="$(mktemp -d)"
trap 'rm -rf "$STAGE"' EXIT
mkdir -p "$STAGE/$NAME"

cp -r "$DIST/." "$STAGE/$NAME/"
rm -f "$STAGE/$NAME/vercel.json" # hosting config, meaningless offline
# file:// pages may not fetch() files, but may load scripts: ship the binary as a script as well.
{
	printf 'window.TPT_WASM_BASE64="'
	base64 -w0 "$DIST/powder.wasm"
	printf '";\n'
} > "$STAGE/$NAME/powder-wasm.js"
cp "$ROOT/wasm/offline/serve.py" "$ROOT/wasm/offline/serve.mjs" "$ROOT/wasm/offline/start.sh" \
	"$ROOT/wasm/offline/Start Game (Windows).cmd" "$ROOT/wasm/offline/README-OFFLINE.txt" "$STAGE/$NAME/"

if find "$STAGE" -iname '*.exe' | grep -q .; then
	echo "refusing to package an .exe" >&2
	exit 1
fi

mkdir -p "$(dirname "$OUT")"
rm -f "$OUT"
(cd "$STAGE" && zip -qr -X "$OUT" "$NAME")
echo "offline bundle: $OUT ($(du -h "$OUT" | cut -f1))"
unzip -l "$OUT" | tail -n +1 | awk '{print $1, $4, $5, $6}' | sed -n '1,40p'
