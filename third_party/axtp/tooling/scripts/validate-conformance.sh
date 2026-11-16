#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

if [[ ! -d "$root/tooling/generators/node_modules" ]]; then
  echo "Missing tooling/generators/node_modules. Run: pnpm --dir tooling/generators install --frozen-lockfile" >&2
  exit 1
fi

node "$root/tooling/scripts/validate-conformance.mjs" "$root"
