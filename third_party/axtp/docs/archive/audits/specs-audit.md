# specs Audit Report

> Audit date: 2026-06-10
> Scope: `specs/**/*.md`
> Constraint: this report does not rewrite specs, registry YAML, Protocol IR, generated outputs, conformance assets, or protocol semantics.

## 1. Executive Summary

`specs` already contains most of the material needed for the AXTP formal design trunk, but it is still shaped like a historical design workspace: normative rules, legacy migration notes, generated reference tables, roadmap planning, examples, generator workflow, and Codex skill workflow are often mixed in the same files.

The highest-impact governance change is not a large directory move. It is to make each spec document carry one responsibility:

- Core specs should define stable wire format, session behavior, state machines, and runtime requirements.
- Registry specs should define meta-models, naming, ID allocation rules, and stability rules, not current full generated tables.
- Codec specs should define data model, binary/JSON/TLV encoding, schema numbering, and validation rules.
- Tooling specs should define Source YAML to Protocol IR to generated output contracts, not human review workflow or legacy migration playbooks.
- Legacy migration, long examples, and implementation recipes should move out of the formal spec trunk and be referenced from it.

Runtime implementers can use several current files as implementation input, especially Core and Codec docs, but should treat some sections as draft/future/appendix material until the specs are normalized.

## 2. Audit Method

The audit scanned every Markdown file under `specs`, reviewed section structure and keyword concentration, and checked the specific overlap/mixing concerns requested by the repository maintainer. The review focused on document responsibility and source-of-truth boundaries, not protocol correctness.

Signals used:

- heading structure and document status text;
- presence of legacy migration material;
- presence of roadmap/P1/P2/Future planning;
- presence of generated/current ID tables;
- presence of long examples or tooling workflow;
- whether the document can be used directly by a runtime implementer.

## 3. Cross-Cutting Findings

### 3.1 Specs are carrying too many document classes

Many files combine:

- normative protocol rules;
- design rationale;
- roadmap;
- generated/current registry tables;
- legacy migration mapping;
- examples and adapter snippets;
- generator or Codex workflow instructions.

This creates two risks:

- Runtime implementers cannot tell which paragraphs are mandatory.
- Generated outputs and specs can become competing sources of truth.

Recommendation: every spec should clearly mark main-body normative content versus examples, appendix, future, and legacy notes.

### 3.2 Generated tables should not live in specs as complete references

The registry documents often define both the meta-model and current full ID tables. Once YAML and `contract/generated` exist, complete current tables in specs become stale-risk duplicates.

Recommendation: keep allocation ranges, invariants, status semantics, and entry schemas in specs. Move current concrete method/event/error/capability/profile lists to generated outputs.

### 3.3 Legacy migration is overrepresented in formal specs

Legacy material is valuable, but much of it is a migration playbook rather than formal AXTP design. This is most visible in `4-tooling/03-Versioning.md`, and also appears in registry, codec, RPC, and STREAM documents.

Recommendation: formal specs should contain only compatibility invariants that protect the protocol. Mapping from old CmdValue, old JSON-RPC, old firmware/file/log protocols, adapter architecture, and migration phases should live under `workspace/legacy-migration`.

### 3.4 Future/P1 content should be appendices or separate profile specs

CONTROL, STREAM, low-bandwidth, and generator docs contain significant P1/P2/Future content. This is useful planning, but it weakens the runtime contract if it appears beside P0/MVP rules.

Recommendation: keep P0/MVP normative rules in the main body. Move future/reserved behavior into explicit appendices, or later split into profile-specific specs.

### 3.5 Examples should be small and normative

Examples help implementers, but long business scenarios in core specs make the implementation contract harder to see.

Recommendation: specs should include one minimal valid and one minimal invalid example where useful. Full flows, business scenarios, MCP adaptation, and end-to-end JSON examples should move to `docs/guides`.

## 4. Per-Document Audit

| File | Current responsibility | Overlap / mixed content | Runtime implementation basis | Recommendation |
|---|---|---|---|---|
| `specs/README.md` | Specs entrypoint and reading order. | Correctly mentions authority boundaries, but still lists files that currently mix normative and generated/planning content. | Yes as navigation only. | Keep. After cleanup, add status labels such as normative, supplemental, appendix, and generated-reference link. |
| `specs/00-Glossary.md` | Global AXTP terminology and cross-runtime domain language. | Contains a small amount of governance and generated-reference terminology, but this is appropriate for a glossary. | No as runtime contract; yes as terminology prerequisite. | Keep as `00`. Continue linking first uses of key terms here. |
| `specs/20-core.md` | High-level AXTP layering, two transport paths, CONTROL/RPC/STREAM overview. | Overlaps strongly with `1-core/02-Protocol-Framework.md` on layering, PayloadType, session flow, and capability strategy. Includes some generated/tooling framing. | Partial; useful orientation, not detailed enough for implementation. | Keep as public overview. Remove or link out duplicated formal framework content. |
| `specs/20-core.md` | Core architecture framework and layered model. | Duplicates `01-Overview` on the same concepts. Some future/profile discussion appears beside core structure. | Partial; can become implementation basis if narrowed. | Either merge into `01`, or shrink to a formal architecture boundary document that only defines layer responsibilities and normative separation. |
| `specs/20-core.md` | Standard Frame, PayloadType, CRC, fragmentation, parser boundaries. | Mostly focused. Has a few examples and future references but not excessive. | Yes for framed runtime parsing. | Keep as normative core spec. Make examples minimal and ensure generated enum values are linked, not duplicated beyond stable core constants. |
| `specs/20-core.md` | Transport profile behavior and mapping between framed/unframed paths. | Mostly focused, with some tooling/profile planning language. | Yes for transport adapters and profile selection. | Keep. Add clear per-profile requirement tables and make future/optional profiles visibly non-normative. |
| `specs/20-core.md` | CONTROL payload, OPEN/ACCEPT/HEARTBEAT/CLOSE, TLV body, framed link context. | Mixes many P1/P2/Future opcodes and TLVs such as ACK/NACK, RESUME, SESSION_RESET, WINDOW_UPDATE, PING/PONG, GOAWAY, VENDOR. | Partial. P0 sections are runtime-critical; future sections are not immediate contract. | Keep P0 CONTROL rules in main body. Move reserved/future opcode semantics to appendix or a future reliability/control profile spec. |
| `specs/20-core.md` | RPC envelope, Hello/Identify/Identified, sid/op/d, request/response/event, encodings, JSON_BINARY. | Mixes MCP compatibility, legacy CmdValue, long `audio.algorithm` examples, generated business method/event ranges, and guidance-style material. | Yes for RPC session once non-normative sections are separated. | Keep as unique authority for Hello/Identify/Identified fields and RPC behavior. Move MCP adapter content to guides/tooling, legacy mapping to legacy-migration, long examples to guides, generated lists to `contract/generated`. |
| `specs/20-core.md` | STREAM 16B header, stream context, lifecycle, data-plane parser requirements. | Mixes future reliability/resume/ACK/NACK profiles, firmware/file/log examples, and old protocol mapping. | Partial. Header/context/parser rules are implementation basis; future reliability is not P0. | Keep STREAM header and lifecycle normative. Move reliability/resume to appendix or future profile spec, business examples to guides/flows, legacy mapping to legacy-migration. |
| `specs/20-core.md` | Low-bandwidth/compact transport degradation proposal. | Contains P1/P2 migration path, compact frame proposal, future generator/conformance notes. | No for v1 P0 runtime unless implementing an explicitly selected low-bandwidth profile. | Keep only if marked supplemental/future. Long term, move to a transport profile appendix or separate `profiles/low-bandwidth.md`. |
| `specs/30-registry.md` | Domain/feature/method/event naming model and taxonomy governance. | Overlaps with `docs/architecture/domain-feature-classification.md`. Contains many product/domain examples and current repository impact notes. | Partial for codegen naming validation; not runtime wire behavior. | Keep normative naming grammar and anti-patterns. Move product examples and current impact lists to architecture/protocol guidance. |
| `specs/30-registry.md` | Method registry entry model, MethodId allocation, method status and semantics. | Combines meta-model with full MethodId planning/current generated tables and legacy intake. | Partial for generator and SDK metadata; not runtime transport behavior. | Keep method entry schema, allocation rules, stability/deprecation rules. Move concrete current lists to generated docs and draft planning to protocol docs. |
| `specs/30-registry.md` | Event registry entry model, EventId allocation, event naming and masks. | Combines meta-model with current generated event schema/table and legacy event mapping. | Partial for SDK metadata and event subscription behavior. | Keep event model, event ID/mask invariants, and status rules. Move current event lists to generated docs and legacy mapping to legacy-migration. |
| `specs/30-registry.md` | ErrorCode model, ranges, status mapping, and compatibility rules. | Combines error meta-model with complete error tables and legacy error/status mapping. | Partial for runtime status handling. | Keep categories, code ranges, response mapping, non-reuse/deprecation rules. Move complete current tables to generated docs and legacy mappings to legacy-migration. |
| `specs/30-registry.md` | Profile model and MVP profile requirements. | Mixes profile meta-model, MVP concrete tables, current generated methods/events/errors/capabilities, and legacy compatibility MVP. | Partial for runtime support-level declaration. | Keep profile entry schema and runtime support semantics. Move current membership tables to generated docs and legacy compatibility notes to legacy-migration. |
| `specs/40-codec.md` | Scalar/composite type system, nullability, numeric ranges, JSON/TLV type mapping. | Mostly focused. Has small legacy and error reference fragments. | Yes for codec implementation. | Keep as normative codec spec. Move legacy notes to appendix and align error references to the ErrorCode registry when rewritten. |
| `specs/40-codec.md` | Schema meta-model, capability model, capability IDs, supported method/event masks. | It is doing two jobs: schema system and capability contract/registry/model. Also includes capability ID tables, v2 capability query examples, and legacy capability mapping. | Partial; capability discovery parts are important but overloaded. | Split conceptually into schema model and capability model. Generated/current capability tables should move to generated docs. Legacy capability mapping should move to legacy-migration. |
| `specs/40-codec.md` | TLV schema encoding, field rules, parser behavior, JSON/TLV mapping. | Mostly focused, but includes old protocol adapter notes and business stream/control examples. | Yes for TLV codec once examples are trimmed. | Keep encoding and parser rules. Move legacy adapter notes and long business examples to guides or legacy-migration. |
| `specs/40-codec.md` | Stable fieldId allocation and schema evolution rules. | Contains legacy field mapping examples and local error names that may not align with the generated ErrorCode registry. | Yes for schema evolution and codegen validation. | Keep numbering/evolution rules. Move legacy mapping example out; use registry-linked error names in future rewrite. |
| `specs/50-tooling.md` | Source YAML to Protocol IR mapping, domain YAML shape, ID/domain planning. | Also carries registry governance, domain registry, domain-scoped masks, and Codex skill workflow. | No for runtime behavior; yes for generator/source tooling. | Keep YAML-to-IR mapping here. Move registry governance to registry specs, skill workflow to `tooling/skills`, generated layout details to Generator spec. |
| `specs/50-tooling.md` | Generator v1 implementation contract, generated output shape, CLI, validation, test requirements. | Mixes implementation contract with protocol lifecycle workflow, Codex skill boundaries, current P0 completion state, P1/P2 roadmap, and generated examples. | No for runtime behavior; yes for generator maintainers. | Keep Source Model, IR, output, validation, CLI, and CI contract. Move skill workflow to `tooling/skills`, roadmap/status to `docs/product/roadmap.md` or release docs, long examples to guides. |
| `specs/50-tooling.md` | Compatibility/versioning plus legacy migration rules. | Heavily overloaded: wire freeze rules are followed by full legacy migration architecture, CmdValue mapping, old JSON-RPC/BinaryRPC/Firmware/RawStream/Log/File/HID migration, C++ adapter requirements, test vectors, and migration phases. | Partial. Freeze/versioning sections are important; most of the document is not runtime AXTP core spec. | Extract a short formal versioning spec. Move the legacy migration manual to `workspace/legacy-migration`, and move adapter implementation examples to guides or migration planning docs. |

## 5. Focus Area Review

### 5.1 `1-core/01-Overview.md` vs `1-core/02-Protocol-Framework.md`

These two files currently explain the same mental model:

- AXTP layering;
- CONTROL / RPC / STREAM separation;
- two production paths, framed and WebSocket JSON;
- PayloadType boundaries;
- high-level session flow;
- capability/registry relationship.

Recommended decision:

- Keep `01-Overview.md` as the human-facing entrypoint.
- Turn `02-Protocol-Framework.md` into either:
  - a short formal boundary spec for L1/L2/Registry/Business responsibilities; or
  - remove it from the runtime reading path and fold unique content into `01`.

Minimum change: add a top note to `02` saying it is the formal layer-boundary companion to `01`, then delete duplicated overview paragraphs in a later cleanup PR.

### 5.2 `1-core/05-Control-Session.md`

The document is useful, but it mixes P0 implementation contract and future control-plane design. P0 runtime implementers mainly need:

- CONTROL payload header layout;
- OPEN / ACCEPT behavior;
- HEARTBEAT behavior;
- CLOSE behavior;
- TLV parsing rules;
- required/optional field behavior;
- lifecycle and failure handling.

Future or non-P0 content should not sit in the same normative lane:

- ACK / NACK semantics;
- RESUME;
- SESSION_RESET;
- WINDOW_UPDATE;
- PING / PONG;
- GOAWAY;
- VENDOR extension behavior;
- P1/P2 TLV fields.

Recommended decision: keep future opcode and TLV allocation ranges visible, but move detailed semantics to an appendix or future reliability/control profile spec.

### 5.3 `1-core/06-RPC-Session.md`

This should remain the unique authority for RPC session behavior. It should fully own:

- Hello / Identify / Identified field rules;
- `sid` format and lifecycle;
- `op` behavior;
- `requestId` behavior;
- event subscription and `eventMasks`;
- `axtpVersion` compatibility check;
- JSON / CBOR / MSGPACK / JSON_BINARY envelope behavior.

Material that should be moved out:

- MCP compatibility details;
- legacy CmdValue mapping;
- long business examples such as `audio.algorithm`;
- current generated method/event range tables;
- guide-style migration examples.

Recommended decision: keep field rules and state machine in the main body; replace long scenarios with links to `docs/guides`, `workspace/legacy-migration`, and `contract/generated`.

### 5.4 `1-core/07-Stream-Data-Plane.md`

The core STREAM contract should be narrow:

- 16B STREAM header;
- `streamId`, `seqId`, `cursor` semantics;
- stream context creation through RPC;
- lifecycle;
- parser and ordering requirements;
- relationship to control plane and business profile.

Current content also discusses future reliability, resume, ACK/NACK, and business-specific transfers such as firmware/file/log. Those are useful, but they are not the base STREAM wire contract.

Recommended decision: keep the base data-plane spec as P0. Move reliability/resume to a future profile appendix and business scenarios to guides/flows.

### 5.5 `2-registry/*.md`

The registry files have the same structural problem: they define the registry meta-model and also attempt to carry complete current tables.

Recommended split:

- Specs own: entry shape, status lifecycle, ID ranges, allocation rules, deprecation/non-reuse rules, validation rules.
- YAML owns: concrete source facts.
- `contract/generated` owns: current rendered tables.
- `workspace/protocol` owns: draft/planning candidate tables before adoption.
- `workspace/legacy-migration` owns: old protocol mapping evidence.

This applies to methods, events, errors, profiles, and capability-related tables.

### 5.6 `3-codec/02-Capability-Types.md`

This file currently contains two major models:

- schema/type meta-model;
- capability model and capability ID registry.

Recommended decision:

- Move schema-model rules into a codec spec, for example `3-codec/02-Schema-Model.md`.
- Move capability model, capability IDs, masks, and discovery semantics into a contract/registry/capability spec, for example `2-registry/06-Capabilities-Registry.md`.
- Keep concrete current capability lists in generated docs.

### 5.7 `4-tooling/01-YAML-Mapping.md`

The document should focus on deterministic mapping from hand-written source YAML to Protocol IR and generated outputs. It currently also carries governance and workflow rules.

Recommended split:

- Keep YAML shape, normalization, merge, conflict detection, and Protocol IR mapping in `4-tooling/01`.
- Move registry governance to `2-registry`.
- Move Codex skill/lifecycle workflow to `tooling/skills` and `docs/guides/protocol-maintainer.md`.
- Move generated output rendering rules to `4-tooling/02-Generator-V1.md`.

### 5.8 `4-tooling/03-Versioning.md`

The first section on wire format freeze, compatible/incompatible changes, ID non-reuse, reserved fields, frame profile switching, and spec/registry version separation is appropriate for a formal spec.

Most later sections are a legacy migration guide:

- adapter architecture;
- old CmdValue mapping;
- old JSON-RPC/BinaryRPC mapping;
- firmware/file/log/raw stream migration;
- HID/KVM migration;
- legacy capability and error mapping;
- C++ adapter examples;
- migration phases and test vector plans.

Recommended decision: split the formal versioning spec from the legacy migration manual. The formal spec should stay under `specs`; the migration manual should move to `workspace/legacy-migration`.

## 6. Reorganization Proposal

### 6.1 Minimal Changes Without Large Directory Moves

Keep the current top-level directories and avoid mass moves:

1. Add a standard status block to each spec:
   - `Normative`
   - `Supplemental`
   - `Appendix`
   - `Future`
   - `Legacy Migration Reference`

2. Add a short "Authority" section to each spec:
   - what this file owns;
   - what it links to;
   - which generated/source files own concrete current facts.

3. In Core specs:
   - keep P0/MVP normative content in the main body;
   - move Future/P1/P2 to appendix headings;
   - replace long business examples with links.

4. In Registry specs:
   - keep meta-model and allocation rules;
   - replace current complete ID tables with links to `../generated` outputs.

5. In Codec specs:
   - separate type/schema rules from capability registry rules;
   - keep codec parser requirements directly implementable.

6. In Tooling specs:
   - keep deterministic generator/tooling contracts;
   - move lifecycle/skill workflow and roadmap/status content out.

7. Update `specs/README.md` to mark which files are normative and which are supplemental.

### 6.2 Ideal Long-Term Directory Structure

The following structure is more explicit, but should be treated as a later cleanup because the current constraint is to avoid large moves:

```text
specs/
  00-Glossary.md
  01-Overview.md
  core/
    frame-and-payload.md
    transport-profiles.md
    control-session.md
    rpc-session.md
    stream-data-plane.md
    low-bandwidth-profile.md
  contract/registry/
    naming-and-taxonomy.md
    registry-governance.md
    methods-model.md
    events-model.md
    errors-model.md
    profiles-model.md
    capabilities-model.md
  codec/
    type-system.md
    schema-model.md
    tlv-encoding.md
    schema-numbering.md
  tooling/
    yaml-to-protocol-ir.md
    generator-v1.md
    versioning-and-compatibility.md
  appendices/
    future-control-reliability.md
    future-stream-reliability.md
    compact-frame-profile.md
```

### 6.3 Target Responsibility by Document

| Target document | Target responsibility |
|---|---|
| `00-Glossary.md` | Shared terminology only. |
| `1-core/01-Overview.md` | Entry overview and reading map; no detailed field duplication. |
| `1-core/02-Protocol-Framework.md` | Formal layer-boundary model, or merged into overview. |
| `1-core/03-Frame-and-Payload.md` | Standard Frame and PayloadType normative wire format. |
| `1-core/04-Transport-Profiles.md` | Per-transport profile behavior and required/optional capabilities. |
| `1-core/05-Control-Session.md` | P0 CONTROL session wire/state contract. |
| `1-core/06-RPC-Session.md` | RPC session, envelope, ops, Hello/Identify/Identified, event subscription. |
| `1-core/07-Stream-Data-Plane.md` | Base STREAM data-plane wire/state contract. |
| `1-core/08-Low-Bandwidth-Degradation.md` | Supplemental low-bandwidth profile proposal, not base v1 runtime contract. |
| `2-registry/01-Naming-and-Taxonomy.md` | Normative naming grammar, domain/feature taxonomy rules, anti-patterns. |
| `2-registry/02-Methods-Registry.md` | Method entry model, MethodId allocation, lifecycle, validation. |
| `2-registry/03-Events-Registry.md` | Event entry model, EventId allocation, event mask rules, lifecycle. |
| `2-registry/04-Errors-Registry.md` | ErrorCode model, ranges, status mapping, lifecycle. |
| `2-registry/05-Profiles-Registry.md` | Profile model and runtime support-level declaration. |
| `3-codec/01-Type-System.md` | Scalar/composite type rules across encodings. |
| `3-codec/02-Capability-Types.md` | Split target: schema model and capability model. |
| `3-codec/03-TLV-Encoding.md` | TLV wire encoding and parser requirements. |
| `3-codec/04-Schema-Numbering.md` | FieldId allocation and schema evolution. |
| `4-tooling/01-YAML-Mapping.md` | Source YAML to Protocol IR mapping. |
| `4-tooling/02-Generator-V1.md` | Generator contract, outputs, validation, CLI, CI. |
| `4-tooling/03-Versioning.md` | Formal versioning, compatibility, freeze, and deprecation policy only. |

### 6.4 Content to Move to `docs/guides`

- Quickstart-style Hello / Identify examples beyond the minimal normative example.
- MCP adapter usage and integration path.
- End-to-end JSON RPC examples.
- Full business scenarios such as audio algorithm control, stream open examples, firmware/file/log operational flows.
- Runtime troubleshooting and implementation recipes.
- Human how-to workflows for using generated docs.

### 6.5 Content to Move to `workspace/legacy-migration`

- Old CmdValue to MethodId mapping strategy and examples.
- Old JSON-RPC and BinaryRPC adapter behavior.
- Old firmware update, RawStream, media, log, file transfer, HID/KVM migration plans.
- Legacy capability table migration.
- Legacy status/error mapping.
- Legacy adapter architecture, C++ adapter examples, and sniffing examples.
- Migration phases and legacy test vectors.

### 6.6 Content Owned by `contract/generated`

- Current MethodId table.
- Current EventId table.
- Current ErrorCode table.
- Current CapabilityId table.
- Current Profile membership tables.
- Current generated business method/event/schema lists.
- Normalized protocol reference generated from Protocol IR.

Specs may show stable core constants and allocation ranges, but should link to generated references for complete current inventories.

### 6.7 Content to Move to `tooling/skills`

- Codex skill boundaries.
- Stage 10/20/30/40/50/60 workflow details.
- Protocol draft adoption workflow checklists.
- "When to run generator" agent instructions.
- Review gates and agent safety rules.

Specs can link to skill docs for repository workflow, but should not embed agent operating procedure as normative protocol design.

## 7. Unified Spec Template

Use this template for future specs and for gradual normalization of existing files.

```markdown
# <Area>/<Number> <Spec Title>

> Status: Normative | Supplemental | Appendix | Future
> Spec Version: <version or tag>
> Change Policy: <policy>
> Scope: <one-line scope>
> Authority: <what this file owns>

## Purpose

Describe why this spec exists and which implementation decisions it makes authoritative.

## Scope

Define what is covered and which transports, payload types, profiles, or registries are in scope.

## Normative Rules

List mandatory rules using MUST / MUST NOT / SHOULD / MAY semantics, or the Chinese equivalents used consistently in the repo.

## Data Model / Wire Format

Define fields, types, byte order, length, ranges, enum values, and source-of-truth links.

## State Machine / Lifecycle

Define states, transitions, initiator/responder responsibilities, timeout behavior, and failure transitions.

## Validation Rules

Define parser validation, generator validation, registry validation, and compatibility checks.

## Implementation Requirements

Define minimum runtime behavior, error handling, unknown field behavior, reserved field behavior, and support-level declaration.

## Versioning

Define compatibility promises, deprecation rules, ID non-reuse, extension points, and relationship to spec tags or registry versions.

## Examples

Include only minimal examples needed to remove ambiguity. Link to guides for long examples.

## Non-goals / Future

List explicitly excluded behavior and future work. Future work must not be confused with the current runtime contract.
```

## 8. Suggested Cleanup Phasing

| Phase | Goal | Suggested changes |
|---|---|---|
| Phase 1 | Make authority visible | Add status/authority blocks and mark future/legacy/example sections. No semantic changes. |
| Phase 2 | Remove duplicate generated facts | Replace complete current registry tables with links to `contract/generated` references. |
| Phase 3 | Separate legacy migration | Extract legacy-heavy sections from core/contract/registry/codec/tooling specs into `workspace/legacy-migration`. |
| Phase 4 | Normalize core runtime specs | Make `03` to `07` directly usable by runtime implementers with clear P0 normative text. |
| Phase 5 | Split overloaded model docs | Split capability/schema and versioning/migration responsibilities. |
| Phase 6 | Optional directory cleanup | Apply the ideal long-term structure only after links and readers are stable. |

## 9. Final Recommendation

Do not start by moving files. First make the current `specs` tree self-declaring:

- every file says what it owns;
- every file says what it does not own;
- generated facts are linked, not copied;
- future and legacy material is visibly non-normative;
- runtime implementers can identify the P0 contract without reading migration history.

After that, the repository can gradually extract guides, legacy migration manuals, generated references, and skill workflows without changing protocol semantics.
