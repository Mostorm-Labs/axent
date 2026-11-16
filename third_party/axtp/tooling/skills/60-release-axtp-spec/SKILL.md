---
name: release-axtp-spec
description: Release AXTP Spec versions from the AXTP repository. Use when the user asks to publish, release, tag, or create a GitHub Release for an AXTP Spec version, including Chinese prompts such as "发布spec v0.3.0", "发布 AXTP spec v0.3.0", "创建 spec/v0.3.0 tag", or "release AXTP Spec v0.3.0". This skill performs release checks, CHANGELOG/release-note preparation, annotated spec/vX.Y.Z tagging, tag push, and optional GitHub Release creation.
---

# Release AXTP Spec

Publish an immutable AXTP Spec release tag from the AXTP repository. This is the release stage after protocol facts and generated artifacts have already been reviewed and validated.

## Boundaries

- Only release tags with format `spec/vMAJOR.MINOR.PATCH`.
- Do not create runtime package releases.
- Do not make runtimes depend on `main`.
- Do not change protocol facts during release except for explicit release metadata such as `release/CHANGELOG.md`.
- Do not tag a dirty repository unless the only dirty changes are release metadata prepared in this workflow and they are committed before tagging.
- Do not push a tag if validation fails, if the tag already exists, or if the changelog entry is still placeholder-only.

## Required Context

Work from the repository root containing `release`, `contract/registry`, `tooling/generators`, and `README.md`.

Read as needed:

```text
release/AXTP_SPEC_VERSIONING.md
release/AXTP_SPEC_RELEASE_CHECKLIST.md
release/AXTP_RUNTIME_SPEC_LOCK.md
release/CHANGELOG.md
README.md
```

Use the bundled validator:

```bash
tooling/skills/60-release-axtp-spec/scripts/validate_release.sh spec/vX.Y.Z
```

## Workflow

### 1. Parse The Requested Version

Accept these user forms and normalize to `spec/vX.Y.Z`:

```text
发布spec v0.3.0
发布 AXTP spec v0.3.0
release AXTP Spec v0.3.0
创建 spec/v0.3.0 tag
```

Reject versions that are not strict `MAJOR.MINOR.PATCH`.

### 2. Inspect Repository State

Run:

```bash
git status --short
git tag --list 'spec/v*' --sort=-v:refname | head -20
bash tooling/scripts/print-spec-version.sh --verbose
```

If non-release files are dirty, stop and report them. If the requested tag already exists locally or remotely, stop.

### 3. Prepare CHANGELOG And Release Notes

Ensure `release/CHANGELOG.md` has a section:

```markdown
## spec/vX.Y.Z

### Protocol
### Registry
### Schemas
### Conformance
### Migration
### Runtime Impact
```

If the section is missing, add it. If no previous `spec/v*` tag exists, describe the release as the initial tagged AXTP Spec release from the current repository snapshot. If a previous tag exists, summarize changed files and recent commits since that tag, grouped by the changelog headings.

Do not publish with `TBD` placeholders remaining in the target section. If release impact cannot be inferred safely, ask the user for release-note wording before tagging.

### 4. Run Release Validation

Run the normal release validator:

```bash
tooling/skills/60-release-axtp-spec/scripts/validate_release.sh spec/vX.Y.Z
```

Also run the generator validation pipeline before final tagging:

```bash
pnpm --dir tooling/generators build
pnpm --dir tooling/generators test
pnpm --dir tooling/generators validate:sources
pnpm --dir tooling/generators generate
pnpm --dir tooling/generators validate:protocol
git diff --check
```

If `pnpm` dependency approval blocks scripts, retry with:

```bash
pnpm --dir tooling/generators --config.verify-deps-before-run=false build
pnpm --dir tooling/generators --config.verify-deps-before-run=false test
pnpm --dir tooling/generators --config.verify-deps-before-run=false validate:sources
pnpm --dir tooling/generators --config.verify-deps-before-run=false generate
pnpm --dir tooling/generators --config.verify-deps-before-run=false validate:protocol
git diff --check
```

If `generate` changes generated outputs, inspect them. Commit intentional generated/release metadata changes before tagging; otherwise stop and report that Stage 50 generation must be completed first.

### 5. Commit Release Metadata If Needed

If `release/CHANGELOG.md` or other release docs changed during this workflow, stage and commit them before creating the tag:

```bash
git add README.md release tooling/scripts/print-spec-version.sh
git commit -m "Prepare AXTP Spec vX.Y.Z release"
```

Keep unrelated user changes unstaged. If unrelated dirty files prevent a clean tag, stop.

### 6. Create And Push The Spec Tag

Create an annotated tag:

```bash
git tag -a spec/vX.Y.Z -m "AXTP Spec vX.Y.Z"
git push origin spec/vX.Y.Z
```

After pushing, run:

```bash
bash tooling/scripts/print-spec-version.sh --verbose
```

### 7. Create GitHub Release When Available

If `gh` is installed and authenticated, create the GitHub Release:

```bash
gh release create spec/vX.Y.Z \
  --title "AXTP Spec vX.Y.Z" \
  --notes-file <release-notes-file>
```

Use the `release/CHANGELOG.md` target section as release notes. If `gh` is unavailable or unauthenticated, report the exact command for the user to run; do not fake a GitHub Release.

## Final Report

Report:

- Normalized tag and commit.
- Validation commands and results.
- Files committed for release metadata, if any.
- Whether annotated tag was created and pushed.
- Whether GitHub Release was created or skipped with a manual command.
- Confirmation that no runtime package release was created.
