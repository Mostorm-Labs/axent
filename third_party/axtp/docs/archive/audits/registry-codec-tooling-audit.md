# Registry / Codec / Tooling Specs Refactor Audit

> Date: 2026-06-10
> Scope: `specs/2-registry/**`, `specs/3-codec/**`, `specs/4-tooling/**`, `specs/README.md`
> Constraint: no protocol semantic changes; no changes to `contract/registry/**`, `protocol/**`, `contract/generated/**`, `docs/conformance/**`, `workspace/protocol/**`, `workspace/legacy-migration/**`, or root `README.md`.

## Summary

Fourth-stage refactor converted registry / codec / tooling specs from mixed historical design notes, large planning tables, examples, and legacy migration material into normative admission and implementation rules for business modules entering registry, generating Protocol IR, producing SDK/runtime views, and feeding conformance.

The refactor did not change methodId, eventId, errorCode, profileId, capabilityId, schema fields, fieldId, registry YAML, Protocol IR, or generated artifacts.

## Modified Files

| File | New responsibility |
|---|---|
| `specs/README.md` | Formal specs entry and authority map; explicitly marks registry appendices as non-normative |
| `specs/30-registry.md` | `domain.feature` naming, taxonomy, and registry admission governance |
| `specs/30-registry.md` | Method registry rules: naming, methodId/bitOffset stability, schema/error/event binding |
| `specs/30-registry.md` | Event registry rules: eventId/bitOffset, eventMasks, payload schema, event compatibility |
| `specs/30-registry.md` | Error registry rules: errorCode ranges, CONTROL/RPC/STREAM mappings, stability |
| `specs/30-registry.md` | Implementation profile rules, runtime support declarations, conformance relationship |
| `specs/40-codec.md` | AXTP type system: scalar/string/bytes/enum/bitmap/array/object/optional/nullability boundaries |
| `specs/40-codec.md` | Split internal responsibility into Schema Model and Capability Model; capability discovery boundaries |
| `specs/40-codec.md` | TLV byte encoding rules, arrays/objects, unknown fields, canonical encoding |
| `specs/40-codec.md` | schema-local fieldId allocation, reserved/deprecated, compatibility rules |
| `specs/50-tooling.md` | Source registry YAML to Protocol IR mapping and business module admission checklist |
| `specs/50-tooling.md` | Generator v1 contract: inputs, outputs, validation, generated-not-handwritten, CLI/CI |
| `specs/50-tooling.md` | Versioning and compatibility contract: spec tag, registry version, breaking changes, runtime binding |

## Appendix Moves

The following large tables were removed from normative spec bodies and preserved as non-normative appendices:

| Appendix | Source | Content moved |
|---|---|---|
| `workspace/registry-planning/candidates/method-candidates.md` | `2-registry/02-Methods-Registry.md` | Historical/current MethodId planning tables, generated method snapshot, legacy method intake, method relationship notes |
| `workspace/registry-planning/candidates/event-candidates.md` | `2-registry/03-Events-Registry.md` | Historical/current EventId planning tables, generated event snapshot, event schema notes, legacy event intake |
| `workspace/registry-planning/candidates/error-candidates.md` | `2-registry/04-Errors-Registry.md` | Historical/current ErrorCode planning tables, error range tables, legacy status mapping |
| `workspace/registry-planning/candidates/profile-candidates.md` | `2-registry/05-Profiles-Registry.md` | MVP/profile planning tables, generated registry snapshots, legacy-compatible MVP notes |
| `workspace/registry-planning/candidates/capability-candidates.md` | `3-codec/02-Capability-Types.md` | Historical/current CapabilityId planning tables, capability query candidates, generated capability snapshot |

Each appendix begins with the required non-normative notice:

```text
This appendix is non-normative.
It preserves historical planning tables and candidate registry entries.
Runtime implementations MUST NOT treat this appendix as the implementation contract.
Adopted entries must come from registry YAML and generated protocol artifacts.
```

No status/source/registry_path/generated columns were inferred, because doing so safely would require comparing historical planning rows against current registry YAML and generated artifacts row by row. The original planning content was preserved instead of guessing adoption status.

## Non-normative Content Downgraded

- Complete Method/Event/Error/Profile/Capability planning tables are now appendix-only.
- Legacy adapter details are referenced as `workspace/legacy-migration/**` material instead of normative tooling/versioning rules.
- Codex skill workflow and agent operation steps were removed from Generator normative body and referenced as `tooling/skills/**` responsibility.
- Long business examples and generated table snapshots were removed from codec/tooling normative bodies.
- Roadmap/P1/P2 planning is now treated as future/non-goal language rather than implementation requirement.

## Remaining Suggested Moves

| Destination | Suggested future move |
|---|---|
| `contract/generated/**` | Current adopted Method/Event/Error/Profile/Capability reference tables should be generated only from registry YAML |
| `workspace/legacy-migration/**` | Detailed legacy CmdValue, legacy JSON-RPC, status-code, adapter skeleton, and migration-stage material |
| `docs/guides/**` | End-to-end tutorials, long examples, and operator-facing generator walkthroughs |
| `tooling/skills/**` | Agent workflows such as plan-protocol-flow, adopt-protocol-draft, amend-adopted-protocol, generate-axtp-protocol |
| `docs/conformance/**` | Profile-specific runtime validation cases derived from contract/registry/generated facts |

## Potential Duplicates Or Conflicts Found

No protocol semantic conflict was intentionally introduced or found during this refactor. The main structural conflicts addressed were documentation authority conflicts:

- Specs body previously mixed normative registry rules with planning/current tables, making Markdown look like an implementation contract.
- `3-codec/02-Capability-Types.md` previously combined schema model, capability model, capability discovery, and large CapabilityId planning tables.
- `4-tooling/01-YAML-Mapping.md` previously mixed YAML-to-IR mapping with broad registry governance and legacy rules.
- `4-tooling/02-Generator-V1.md` previously mixed generator contract with agent workflow, roadmap, status reports, and long output examples.
- `4-tooling/03-Versioning.md` previously mixed versioning rules with extensive legacy migration design.

## Protocol Semantics

No protocol semantics were changed:

- No registry YAML was edited.
- No Protocol IR was edited.
- No generated artifact was edited.
- No methodId, eventId, errorCode, profileId, capabilityId, schema field, or fieldId was changed.
- No CONTROL/RPC/STREAM wire field was added, removed, or redefined.

## Fifth-stage Business Module Audit Suggestions

1. Audit `workspace/protocol/**` drafts against the new `domain.feature` taxonomy and mark which drafts are ready for registry admission.
2. For each adopted business domain, compare `workspace/protocol/**`, `contract/registry/domains/<domain>/domain.yaml`, `contract/generated/protocol.md`, and conformance coverage.
3. Move any remaining current-method/event/error/capability reference tables out of human-authored specs into generated output.
4. Add conformance case stubs for each stable business method/event, especially schema validation, error handling, and event emission behavior.
5. Review appendix rows against current registry YAML only if those rows are needed for adoption; otherwise leave appendices as historical planning material.
