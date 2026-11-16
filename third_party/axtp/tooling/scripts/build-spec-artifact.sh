#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 ]]; then
  echo "Usage: $0 MAJOR.MINOR.PATCH" >&2
  exit 2
fi

version="$1"
if [[ ! "$version" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
  echo "Expected version format MAJOR.MINOR.PATCH, for example 0.3.0" >&2
  exit 2
fi

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
template="$root/tooling/release/manifest.template.yaml"
contract="$root/tooling/release/artifact-contract.json"
artifact_name="axtp-spec-v$version"
dist_dir="$root/dist"
artifact_dir="$dist_dir/$artifact_name"
zip_path="$dist_dir/$artifact_name.zip"
commit="$(git -C "$root" rev-parse HEAD)"
released_at="$(date -u +%Y-%m-%dT%H:%M:%SZ)"

if [[ ! -f "$template" ]]; then
  echo "Missing manifest template: $template" >&2
  exit 1
fi

if [[ ! -f "$contract" ]]; then
  echo "Missing artifact contract: $contract" >&2
  exit 1
fi

rm -rf "$artifact_dir" "$zip_path"
mkdir -p "$artifact_dir"

copy_dir() {
  local src="$1"
  local dest="$2"
  local label="$3"
  if [[ -d "$src" ]]; then
    mkdir -p "$(dirname "$dest")"
    if command -v rsync >/dev/null 2>&1; then
      rsync -a --delete --exclude ".DS_Store" "$src/" "$dest/"
    else
      rm -rf "$dest"
      cp -R "$src" "$dest"
      find "$dest" -name ".DS_Store" -delete
    fi
    echo "[OK] copied $label"
  else
    echo "[SKIP] missing $label: $src"
  fi
}

copy_file() {
  local src="$1"
  local dest="$2"
  local label="$3"
  if [[ -f "$src" ]]; then
    mkdir -p "$(dirname "$dest")"
    cp "$src" "$dest"
    echo "[OK] copied $label"
  else
    echo "[SKIP] missing $label: $src"
  fi
}

copy_file "$root/README.md" "$artifact_dir/README.md" "README.md"
copy_file "$root/LICENSE" "$artifact_dir/LICENSE" "LICENSE"
copy_file "$root/docs/README.md" "$artifact_dir/docs/README.md" "docs README"
copy_dir "$root/contract/protocol" "$artifact_dir/contract/protocol" "Protocol IR"
copy_dir "$root/docs/guides" "$artifact_dir/docs/guides" "role guides"
copy_dir "$root/docs/product" "$artifact_dir/docs/product" "product status"
copy_dir "$root/contract/generated" "$artifact_dir/contract/generated" "generated protocol references"
copy_dir "$root/specs" "$artifact_dir/specs" "specs"
copy_dir "$root/contract/registry" "$artifact_dir/contract/registry" "registry"
copy_dir "$root/contract/mcp" "$artifact_dir/contract/mcp" "MCP registry artifacts"
copy_dir "$root/contract/test-vectors" "$artifact_dir/contract/test-vectors" "test vectors"
copy_dir "$root/conformance" "$artifact_dir/conformance" "conformance"
copy_dir "$root/release" "$artifact_dir/release" "release docs"
copy_file "$root/release/CHANGELOG.md" "$artifact_dir/CHANGELOG.md" "CHANGELOG.md"

sed \
  -e "s|{{VERSION}}|$version|g" \
  -e "s|{{COMMIT}}|$commit|g" \
  -e "s|{{DATE}}|$released_at|g" \
  "$template" > "$artifact_dir/manifest.yaml"
echo "[OK] wrote manifest.yaml"

find "$artifact_dir" -name ".DS_Store" -delete

python3 - "$artifact_dir" "$contract" <<'PY'
import json
import os
import sys

artifact_dir, contract_path = sys.argv[1], sys.argv[2]
contract = json.load(open(contract_path, encoding="utf-8"))
excluded_prefixes = list(contract.get("excluded_prefixes", []))
excluded_prefixes.extend(
    f"specs/{legacy_dir}/"
    for legacy_dir in contract.get("excluded_legacy_specs_dirs", [])
)

missing = [
    rel
    for rel in contract["required_paths"]
    if not os.path.exists(os.path.join(artifact_dir, rel))
]
if missing:
    print("Release artifact missing required paths:", file=sys.stderr)
    for rel in missing:
        print(f"- {rel}", file=sys.stderr)
    sys.exit(1)

violations = []
for dirpath, _, filenames in os.walk(artifact_dir):
    for filename in filenames:
        full = os.path.join(dirpath, filename)
        rel = os.path.relpath(full, artifact_dir).replace(os.sep, "/")
        if any(rel.endswith(suffix) for suffix in contract.get("excluded_suffixes", [])):
            violations.append((rel, "excluded suffix"))
        if any(rel.startswith(prefix) for prefix in excluded_prefixes):
            violations.append((rel, "excluded prefix"))
        if any(token in rel for token in contract.get("excluded_contains", [])):
            violations.append((rel, "excluded path"))

if violations:
    print("Release artifact contains excluded paths:", file=sys.stderr)
    for rel, reason in violations[:40]:
        print(f"- {rel}: {reason}", file=sys.stderr)
    sys.exit(1)
PY

mkdir -p "$dist_dir"
if command -v zip >/dev/null 2>&1; then
  (cd "$dist_dir" && zip -qr "$artifact_name.zip" "$artifact_name")
elif command -v python3 >/dev/null 2>&1; then
  (cd "$dist_dir" && python3 -m zipfile -c "$artifact_name.zip" "$artifact_name")
else
  echo "Need zip or python3 to create $zip_path" >&2
  exit 1
fi

echo "[OK] built artifact: $artifact_dir"
echo "[OK] built archive: $zip_path"
