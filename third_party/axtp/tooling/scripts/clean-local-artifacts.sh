#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
dist="$root/dist"
dry_run=false

if [[ "${1:-}" == "--dry-run" ]]; then
  dry_run=true
elif [[ $# -gt 0 ]]; then
  echo "Usage: $0 [--dry-run]" >&2
  exit 2
fi

if [[ ! -d "$dist" ]]; then
  echo "[OK] no dist/ directory"
  exit 0
fi

shopt -s nullglob
artifacts=("$dist"/axtp-spec-v*)
shopt -u nullglob

if [[ ${#artifacts[@]} -eq 0 ]]; then
  echo "[OK] no local AXTP spec artifacts in dist/"
  exit 0
fi

if [[ "$dry_run" == true ]]; then
  printf "[DRY-RUN] would remove %d local artifact(s):\n" "${#artifacts[@]}"
  printf "%s\n" "${artifacts[@]#$root/}"
  exit 0
fi

rm -rf "${artifacts[@]}"
printf "[OK] removed %d local artifact(s) from dist/\n" "${#artifacts[@]}"
