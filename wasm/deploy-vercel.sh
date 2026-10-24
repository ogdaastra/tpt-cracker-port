#!/usr/bin/env bash
# Deploys the already-built and tested dist/ to Vercel and prints the deployment URL on stdout.
#
#   ./wasm/deploy-vercel.sh          preview deployment (default)
#   ./wasm/deploy-vercel.sh --prod   production deployment (only after the preview passed its tests)
#
# Needs VERCEL_TOKEN (a Vercel access token) in the environment. Never pass it as an argument or
# write it to a file. VERCEL_ORG_ID / VERCEL_PROJECT_ID default to the project created for this port.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DIST="${DIST:-$ROOT/dist}"
VERCEL_CLI="vercel@62.7.0"

: "${VERCEL_TOKEN:?VERCEL_TOKEN is not set (create one at https://vercel.com/account/settings/tokens)}"
export VERCEL_TOKEN
export VERCEL_ORG_ID="${VERCEL_ORG_ID:-team_Hd1pTA8fuMgOK5kVYAJlPhbd}"
export VERCEL_PROJECT_ID="${VERCEL_PROJECT_ID:-prj_HfIKw7K397cl1CqEp6UKmk8AkOsf}"

for f in index.html powder.js powder.wasm vercel.json; do
	[ -f "$DIST/$f" ] || { echo "missing $DIST/$f, run ./wasm/build.sh first" >&2; exit 1; }
done

args=(deploy "$DIST" --yes)
if [ "${1:-}" = "--prod" ]; then
	args+=(--prod)
fi
# The CLI prints the deployment URL on stdout and progress on stderr.
npx --yes "$VERCEL_CLI" "${args[@]}"
