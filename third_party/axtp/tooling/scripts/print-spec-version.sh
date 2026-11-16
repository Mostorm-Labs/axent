#!/usr/bin/env bash
set -euo pipefail

current_commit="$(git rev-parse HEAD)"
current_tag="$(git describe --tags --exact-match 2>/dev/null || echo 'no exact tag')"
latest_tag="$(git tag --list 'spec/v*' --sort=-v:refname | head -n 1)"

if [[ -z "$latest_tag" ]]; then
  echo "No spec/v* tag found" >&2
  exit 1
fi

version="${latest_tag#spec/v}"

if [[ "${1:-}" == "--verbose" ]]; then
  echo "Current commit: $current_commit"
  echo "Current tag: $current_tag"
  echo "Latest spec tag: $latest_tag"
  echo "Latest spec version: $version"
else
  echo "$version"
fi
