# AXTP Runtime Update Flow

AXTP Spec releases are the source of truth for runtime, SDK, tooling, and mock
server updates. Runtime repositories must implement a released Spec tag or an
explicit commit; they must not track the AXTP `main` branch as a reproducible
build dependency.

## Release Contract

- AXTP Spec tags use `spec/vMAJOR.MINOR.PATCH`, for example `spec/v0.3.0`.
- The main AXTP repository publishes release artifacts with `contract/`,
  `specs/`, `conformance/`, role/product docs, release docs, a changelog, and
  a manifest.
- Runtime repositories record their binding in `AXTP_SPEC.lock.yaml`.
- Runtime repositories expose runtime-local upgrade and lock-check scripts
  named `upgrade-axtp-spec.sh spec/vX.Y.Z` and `check-axtp-spec-lock.sh`.
- Runtime/tool GitHub release tags extend the AXTP Spec version with an
  implementation revision: `spec/vX.Y.Z` maps to `vX.Y.Z.0` for the first
  runtime/tool release, and later runtime-only fixes use `vX.Y.Z.R`.
- Runtime upgrades create pull requests, auto-merge after checks pass, create
  runtime/tool tags, and then create GitHub Releases.

## Mode A: Spec Tag Automation

The default release path is fully automated:

1. AXTP maintainers push `spec/vX.Y.Z`.
2. The `spec-release-dispatch` workflow builds `dist/axtp-spec-vX.Y.Z.zip`.
3. The workflow creates or updates the AXTP Spec GitHub Release.
4. The workflow sends `axtp_spec_released` repository dispatch events to the
   runtime/tool repositories.
5. Runtime/tool repositories update `AXTP_SPEC.lock.yaml`, set their own
   release version to `X.Y.Z.0`, regenerate artifacts, and open upgrade PRs.
6. Upgrade PR checks run and GitHub auto-merge is enabled for automation
   branches only.
7. After merge to `main`, the runtime/tool repository creates `vX.Y.Z.0`.
8. The runtime/tool `vX.Y.Z.0` tag creates the GitHub Release.

## Mode B: Manual Backfill Dispatch

Manual workflow dispatch remains available for backfills and dry runs:

1. AXTP maintainers run the `notify-runtimes` workflow with
   `spec_tag=spec/vX.Y.Z`.
2. In `dry_run` mode it prints the repositories and payload without calling the
   GitHub API.
3. With `dry_run=false`, it sends the same `axtp_spec_released` event used by
   tag automation.

Both workflows use the `AXTP_RUNTIME_DISPATCH_TOKEN` secret. This PAT must be
configured in the `axtp` repository and must be able to send
`repository_dispatch` events to the runtime/tool repositories.

The dispatch payload includes:

- `spec_tag`
- `spec_version`
- `spec_repository`
- `spec_commit`

## Verification

For an end-to-end test release such as `spec/v0.0.2`, verify:

- The `axtp` release workflow uploads `dist/axtp-spec-v0.0.2.zip`.
- Each runtime/tool repository opens or updates
  `automation/upgrade-axtp-spec-v0.0.2`.
- Each generated manifest records AXTP Spec `0.0.2` and runtime/tool version
  `0.0.2.0`.
- Each automation PR auto-merges only after checks pass.
- Each runtime/tool repository creates `v0.0.2.0` and a GitHub Release.

## Version Contract

Runtime repositories keep three version records. The AXTP Spec version remains
three-part SemVer, while the runtime/tool GitHub release tag appends a fourth
runtime revision:

- AXTP Spec Version from the main repository tag, for example `spec/v0.3.0`.
- Runtime/tool GitHub Release Version from the runtime/tool repository, for
  example `v0.3.0.0` or `v0.3.0.1`.
- Runtime Generated Artifact Version from the runtime repository generated
  manifest or release metadata.

Runtime repositories use runtime tags `vX.Y.Z.R`; they must not tag runtime
releases as `spec/vX.Y.Z`.

The first runtime/tool release for a Spec tag uses revision `0`. Later fixes
that do not change the AXTP Spec lock increment only the fourth field:

```text
spec/v0.8.4 -> runtime/tool v0.8.4.0
spec/v0.8.4 -> runtime/tool v0.8.4.1
spec/v0.8.5 -> runtime/tool v0.8.5.0
```

Package-manager versions may need ecosystem-specific projections when a
four-part version is not legal. In that case, the GitHub release tag and
generated manifest remain canonical, and the package metadata must keep the
AXTP Spec lock visible.

AXTP Spec releases do not publish npm, pub, PyPI, Docker, or other package
registries. Runtime/tool GitHub Releases are created only from runtime/tool
`vX.Y.Z.R` tags.

## Runtime Repositories

The real runtime and mock repositories are:

- `axtp-c-runtime`
- `axtp-cpp-runtime`
- `axtp-flutter-runtime`
- `axtp-ts-runtime`
- `axtp-python-runtime`
- `axtp-mock-server`

## Non-Goals

- Do not automatically merge ordinary development PRs.
- Do not publish npm, pub, PyPI, Docker, or registry packages from an AXTP Spec
  release.
- Do not make runtime repositories depend on AXTP `main`.
- Do not redefine specs, registry facts, schemas, or conformance material in
  runtime repositories.

## Required Repository Settings

- The `axtp` repository must define `AXTP_RUNTIME_DISPATCH_TOKEN`.
- Runtime/tool repositories must allow GitHub Actions to create pull requests.
- Runtime/tool repositories must allow GitHub auto-merge.
- Runtime/tool repositories must allow GitHub Actions to create tags and
  releases.
- Runtime/tool repositories should define `AXTP_RUNTIME_AUTOMATION_TOKEN` when
  automation-created PRs must trigger downstream `pull_request` checks. If this
  secret is absent, workflows fall back to the repository `GITHUB_TOKEN`.
