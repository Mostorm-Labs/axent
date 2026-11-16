# AXTP Spec Release Checklist

Use this checklist before creating an AXTP Spec tag and GitHub Release.

## Pre-Release Checks

1. Confirm `specs/` protocol documents are updated.
2. Confirm `contract/registry/` method, event, capability, error, profile, and shared registry files are updated.
3. Confirm schema source files and domain YAML schema definitions are updated.
4. Confirm generated protocol references, tooling JSON, test vectors, and runtime generated headers are refreshed when source facts changed.
5. Confirm conformance or test-vector material is updated for new or changed protocol behavior.
6. Confirm `workspace/legacy-migration/plans/` legacy mapping and migration notes are updated when legacy behavior changes.
7. Confirm `release/CHANGELOG.md` records the release under `spec/vX.Y.Z`.
8. Confirm `README.md` identifies AXTP Spec versioning and runtime dependency rules.
9. Confirm compatibility notes clearly state runtime impact.
10. Confirm `git diff --check` passes.

## Tag And Release

Create an annotated tag:

```bash
git tag -a spec/vX.Y.Z -m "AXTP Spec vX.Y.Z"
git push origin spec/vX.Y.Z
```

Create a GitHub Release named:

```text
AXTP Spec vX.Y.Z
```

The GitHub Release body should include:

- Protocol changes
- Registry changes
- Schema changes
- Conformance changes
- Migration changes
- Compatibility notes
- Runtime impact
- Links to `release/CHANGELOG.md` and relevant generated protocol docs

## Runtime Automation

After the release:

1. Confirm the `spec-release-dispatch` workflow dispatched `axtp_spec_released` to all real runtime/mock repositories.
2. Confirm each runtime/tool repository opened or updated `automation/upgrade-axtp-spec-vX.Y.Z`.
3. Confirm each upgrade PR updates `AXTP_SPEC.lock.yaml`, runtime/tool version files, generated code, and runtime release metadata.
4. Confirm each runtime generated manifest or release metadata records AXTP Spec `X.Y.Z` and runtime/tool version `X.Y.Z.0`.
5. Confirm each automation PR auto-merges only after checks pass.
6. Confirm each runtime/tool repository creates `vX.Y.Z.0` and a GitHub Release.
7. Do not ask runtimes to depend on `main`; every runtime dependency must point to a tag or commit.

## Non-Goals

- Do not publish npm, pub, PyPI, Docker, or registry packages from an AXTP Spec release.
- Do not force every runtime repository to use the same dependency mechanism.
- Do not publish an AXTP Spec tag until source facts and generated outputs are aligned.
