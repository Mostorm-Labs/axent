#!/usr/bin/env bash
set -euo pipefail

usage() {
  echo "Usage: $0 spec/vMAJOR.MINOR.PATCH|vMAJOR.MINOR.PATCH|MAJOR.MINOR.PATCH" >&2
}

raw_version="${1:-}"
if [[ -z "${raw_version}" ]]; then
  usage
  exit 2
fi

version="${raw_version}"
version="${version#spec/}"
version="${version#v}"

if [[ ! "${version}" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
  echo "Invalid AXTP Spec version: ${raw_version}" >&2
  echo "Expected MAJOR.MINOR.PATCH, for example spec/v0.3.0" >&2
  exit 2
fi

tag="spec/v${version}"
dry_run_version=0
if [[ "${version}" == "0.0.0" ]]; then
  dry_run_version=1
fi
repo_root="$(git rev-parse --show-toplevel)"
cd "${repo_root}"

required_files=(
  "README.md"
  "release/CHANGELOG.md"
  "release/AXTP_SPEC_VERSIONING.md"
  "release/AXTP_SPEC_RELEASE_CHECKLIST.md"
  "release/AXTP_RUNTIME_SPEC_LOCK.md"
  "tooling/scripts/print-spec-version.sh"
)

for path in "${required_files[@]}"; do
  if [[ ! -f "${path}" ]]; then
    echo "Missing required release file: ${path}" >&2
    exit 1
  fi
done

if [[ "${dry_run_version}" != "1" ]] && git rev-parse -q --verify "refs/tags/${tag}" >/dev/null; then
  echo "Tag already exists locally: ${tag}" >&2
  exit 1
fi

if [[ "${dry_run_version}" != "1" ]] && git ls-remote --exit-code --tags origin "refs/tags/${tag}" >/dev/null 2>&1; then
  echo "Tag already exists on origin: ${tag}" >&2
  exit 1
fi

if [[ "${ALLOW_DIRTY:-0}" != "1" ]] && [[ -n "$(git status --porcelain)" ]]; then
  echo "Working tree is dirty. Commit release metadata before tagging." >&2
  git status --short >&2
  exit 1
fi

changelog="release/CHANGELOG.md"

if [[ "${dry_run_version}" != "1" ]] && ! grep -q "^## ${tag}$" "${changelog}"; then
  echo "${changelog} is missing section: ## ${tag}" >&2
  exit 1
fi

if [[ "${dry_run_version}" != "1" ]]; then
  section="$(
    awk -v tag="${tag}" '
      $0 == "## " tag { in_section = 1; next }
      in_section && /^## / { exit }
      in_section { print }
    ' "${changelog}"
  )"

  if [[ -z "${section//[[:space:]]/}" ]]; then
    echo "${changelog} section for ${tag} is empty." >&2
    exit 1
  fi

  if grep -q "TBD" <<<"${section}"; then
    echo "${changelog} section for ${tag} still contains TBD placeholders." >&2
    exit 1
  fi
fi

git diff --check
bash tooling/scripts/print-spec-version.sh >/dev/null
node tooling/scripts/check-draft-noise.mjs >/dev/null
node tooling/scripts/check-frontstage-language.mjs >/dev/null
node tooling/scripts/report-protocol-draft-health.mjs --check docs/product/protocol-draft-health.md >/dev/null
tooling/scripts/check-release-artifact.sh "${version}" >/dev/null
node tooling/scripts/check-links.mjs "dist/axtp-spec-v${version}" >/dev/null

echo "AXTP Spec release validation passed for ${tag}"
