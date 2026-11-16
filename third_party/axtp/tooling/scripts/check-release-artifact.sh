#!/usr/bin/env bash
set -euo pipefail

version="${1:-0.0.0}"
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
artifact="axtp-spec-v$version"
zip_path="$root/dist/$artifact.zip"
contract="$root/tooling/release/artifact-contract.json"
template="$root/tooling/release/manifest.template.yaml"

"$root/tooling/scripts/build-spec-artifact.sh" "$version" >/tmp/axtp-release-artifact.log

if [[ ! -f "$zip_path" ]]; then
  echo "Missing release archive: $zip_path" >&2
  exit 1
fi

if [[ ! -f "$contract" ]]; then
  echo "Missing artifact contract: $contract" >&2
  exit 1
fi

if [[ ! -f "$template" ]]; then
  echo "Missing manifest template: $template" >&2
  exit 1
fi

python3 - "$zip_path" "$artifact" "$contract" "$template" <<'PY'
import json
import sys
import zipfile

zip_path, artifact, contract_path, template_path = sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4]
contract = json.load(open(contract_path, encoding="utf-8"))
excluded_prefixes = list(contract.get("excluded_prefixes", []))
excluded_prefixes.extend(
    f"specs/{legacy_dir}/"
    for legacy_dir in contract.get("excluded_legacy_specs_dirs", [])
)

def parse_manifest_contents(path):
    sections = {
        "contract_paths": [],
        "compatibility_paths": [],
        "supplemental_paths": [],
        "excluded_paths": [],
        "excluded_legacy_specs_dirs": [],
    }
    in_contents = False
    current = None
    for raw in open(path, encoding="utf-8"):
        line = raw.rstrip("\n")
        if line == "contents:":
            in_contents = True
            current = None
            continue
        if in_contents and line and not line.startswith(" "):
            break
        if not in_contents:
            continue
        stripped = line.strip()
        if stripped.endswith(":") and stripped[:-1] in sections:
            current = stripped[:-1]
            continue
        if current and stripped.startswith("- "):
            sections[current].append(stripped[2:].strip().strip('"').strip("'"))
    return sections

def is_covered(path, patterns):
    for pattern in patterns:
        if pattern in ("**/.DS_Store", ".DS_Store") and path.endswith(".DS_Store"):
            return True
        if pattern.endswith("/") and path.startswith(pattern):
            return True
        if path == pattern:
            return True
    return False

manifest = parse_manifest_contents(template_path)
manifest_included = (
    manifest["contract_paths"]
    + manifest["compatibility_paths"]
    + manifest["supplemental_paths"]
)
manifest_excluded = manifest["excluded_paths"]
manifest_excluded.extend(
    f"specs/{legacy_dir}/"
    for legacy_dir in manifest["excluded_legacy_specs_dirs"]
)

uncovered_required = sorted(
    rel for rel in contract["required_paths"] if not is_covered(rel, manifest_included)
)
if uncovered_required:
    print("[FAIL] manifest.template.yaml contents do not cover required artifact paths", file=sys.stderr)
    for item in uncovered_required:
        print(f"- {item}", file=sys.stderr)
    sys.exit(1)

contract_excluded = []
contract_excluded.extend(excluded_prefixes)
contract_excluded.extend(contract.get("excluded_contains", []))
contract_excluded.extend(contract.get("excluded_suffixes", []))
uncovered_excluded = sorted(
    rel for rel in contract_excluded if not is_covered(rel, manifest_excluded)
)
if uncovered_excluded:
    print("[FAIL] manifest.template.yaml excluded paths do not cover artifact-contract exclusions", file=sys.stderr)
    for item in uncovered_excluded:
        print(f"- {item}", file=sys.stderr)
    sys.exit(1)

manifest_conflicts = sorted(
    rel for rel in manifest_included if is_covered(rel, contract_excluded)
)
if manifest_conflicts:
    print("[FAIL] manifest.template.yaml includes paths excluded by artifact-contract", file=sys.stderr)
    for item in manifest_conflicts:
        print(f"- {item}", file=sys.stderr)
    sys.exit(1)

with zipfile.ZipFile(zip_path) as archive:
    names = set(archive.namelist())

artifact_prefix = f"{artifact}/"
relative_names = {
    name[len(artifact_prefix):]
    for name in names
    if name.startswith(artifact_prefix) and not name.endswith("/")
}

missing = sorted(set(contract["required_paths"]) - relative_names)
if missing:
    print("[FAIL] release archive is missing required paths", file=sys.stderr)
    for item in missing:
        print(f"- {item}", file=sys.stderr)
    sys.exit(1)

violations = []
for rel in sorted(relative_names):
    if any(rel.endswith(suffix) for suffix in contract.get("excluded_suffixes", [])):
        violations.append((rel, "excluded suffix"))
    if any(rel.startswith(prefix) for prefix in excluded_prefixes):
        violations.append((rel, "excluded prefix"))
    if any(token in rel for token in contract.get("excluded_contains", [])):
        violations.append((rel, "excluded path"))

if violations:
    print("[FAIL] release archive contains excluded paths", file=sys.stderr)
    for item, reason in violations[:40]:
        print(f"- {item}: {reason}", file=sys.stderr)
    sys.exit(1)

print("[OK] release archive contract paths verified")
PY

node "$root/tooling/scripts/check-links.mjs" "$root/dist/$artifact"
node "$root/tooling/scripts/check-frontstage-language.mjs" "$root/dist/$artifact"
