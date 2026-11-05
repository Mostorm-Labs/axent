#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cli="$root/tooling/generators/dist/cli.js"

if [[ ! -f "$cli" ]]; then
  echo "Missing generator build output: $cli" >&2
  echo "Run: pnpm --dir tooling/generators build" >&2
  exit 1
fi

tmp_base="$(cd "${TMPDIR:-/tmp}" && pwd -P)"
tmp="$(mktemp -d "$tmp_base/axtp-generated-drift.XXXXXX")"
case "$tmp" in
  "$tmp_base"/axtp-generated-drift.*) ;;
  *) echo "Unsafe temporary output path" >&2; exit 1 ;;
esac
trap 'rm -rf "$tmp"' EXIT

mkdir -p "$tmp/contract/protocol" "$tmp/generated"

node "$cli" generate \
  --spec "$root" \
  --protocol-out "$tmp/contract/protocol/axtp.protocol.yaml" \
  --out "$tmp/generated" >/tmp/axtp-generated-drift.log

diff -u "$root/contract/protocol/axtp.protocol.yaml" "$tmp/contract/protocol/axtp.protocol.yaml"

diff -u "$root/contract/generated/protocol.md" "$tmp/generated/protocol.md"
diff -u "$root/contract/generated/protocol.json" "$tmp/generated/protocol.json"

for file in \
  capability_registry.generated.md \
  error_code.generated.md \
  event_registry.generated.md \
  legacy_mapping.generated.md \
  method_registry.generated.md
do
  diff -u "$root/contract/generated/$file" "$tmp/generated/docs/$file"
done

for file in \
  capability_registry.generated.json \
  error_code.generated.json \
  event_registry.generated.json \
  legacy_mapping.generated.json \
  method_registry.generated.json \
  schema.generated.json
do
  diff -u "$root/contract/mcp/$file" "$tmp/generated/json/$file"
done

for file in "$root"/contract/test-vectors/*; do
  base="$(basename "$file")"
  diff -u "$file" "$tmp/generated/test_vectors/$base"
done

echo "[OK] generated artifacts match contract/registry/domain YAML sources"
