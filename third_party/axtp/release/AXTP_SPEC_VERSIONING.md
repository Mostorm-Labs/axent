# AXTP Spec Versioning

AXTP uses Git tags and GitHub Releases to publish immutable protocol standard versions. The AXTP repository is the single source of truth for the spec: text specs, registry YAML, Protocol IR, generated protocol references, conformance material, product guides, release docs, and the artifact manifest.

Runtime repositories implement AXTP Spec versions. They must not redefine protocol facts, and they must not depend on the `main` branch for reproducible builds.

## Version Kinds

AXTP separates the protocol standard version from language runtime package versions.

```text
AXTP Spec Version:       spec/v0.3.0
Runtime Package Version: axtp-cpp-runtime v0.3.x
Runtime Package Version: @mostorm/axtp-ts-runtime 0.3.x
Runtime Package Version: axtp_flutter_runtime 0.3.x
```

Runtime versions may align their major/minor numbers with the compatible spec range, but they are still separate package versions. For example, runtime `0.3.x` means the runtime is intended to implement or remain compatible with AXTP `spec/v0.3.x`.

Runtime GitHub Release tags use a four-part coordination version when they are
derived from a fixed AXTP Spec tag:

```text
Runtime Release Version: vSPEC_MAJOR.SPEC_MINOR.SPEC_PATCH.RUNTIME_REVISION
```

The first runtime release for `spec/vX.Y.Z` uses `vX.Y.Z.0`. Runtime-only
fixes, SDK fixes, tooling fixes, platform packaging changes, and similar
implementation updates that keep the same `AXTP_SPEC.lock.yaml` increment only
the fourth field, for example `vX.Y.Z.1`.

Language package versions may need an ecosystem-specific projection if the
package manager does not accept four numeric fields. The GitHub Release tag and
generated manifest remain the canonical runtime release identity.

## Tag Format

AXTP Spec tags use this format:

```text
spec/vMAJOR.MINOR.PATCH
```

Examples:

```text
spec/v0.1.0
spec/v0.2.0
spec/v0.3.0
```

Tags should be annotated:

```bash
git tag -a spec/v0.3.0 -m "AXTP Spec v0.3.0"
git push origin spec/v0.3.0
```

GitHub Release names should use:

```text
AXTP Spec v0.3.0
```

## Version Semantics

| Part | Meaning | Examples | Runtime impact |
|---|---|---|---|
| MAJOR | Incompatible protocol change. | Frame/header/session/RPC semantics change in a breaking way. | Runtimes must explicitly adapt; they must not assume compatibility. |
| MINOR | Backward-compatible capability addition. | New optional field, capability, method, event, schema, or transport profile. | Runtimes may add support for new facts; existing capabilities should remain compatible. |
| PATCH | Non-breaking correction. | Documentation fix, schema description correction, non-breaking registry metadata correction. | Runtimes are not required to upgrade unless they need the correction. |

Patch releases must not change wire compatibility. Minor releases may expand the generated registry and machine-readable facts without invalidating previous minor functionality. Major releases are explicit compatibility boundaries.

Runtime revision `R` in `vX.Y.Z.R` is not an AXTP Spec version field. It is a
runtime repository release counter scoped to the locked Spec version `X.Y.Z`.

## Release Content

Each GitHub Release should summarize:

- Protocol changes
- Registry changes
- Schema changes
- Conformance changes
- Migration changes
- Compatibility notes
- Runtime impact

Release notes should link to `release/CHANGELOG.md` and mention the exact tag and commit.

## Compatibility With Existing AXTP Version Fields

The release tag identifies the whole AXTP Spec snapshot. It does not replace wire-level or registry-level fields already defined in the spec, such as `specVersion`, `registryVersion`, `wire_version`, or generated protocol metadata.

Use `spec/vMAJOR.MINOR.PATCH` for repository release identity. Use protocol metadata and generated registry facts for runtime negotiation, validation, and code generation.

## Rules

- Do not use runtime package versions as AXTP Spec versions.
- Do not publish runtime builds that implicitly track AXTP `main`.
- Do not require every runtime to use Git submodules; package ecosystems may use metadata, lock files, or spec packages.
- Do not put large runtime implementations into the AXTP spec repository just to support versioning.
- Do not hand-edit generated outputs to prepare a release; update the source facts and regenerate first.
