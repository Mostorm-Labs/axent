---
name: adopt-protocol-draft
description: Stage 30 adoption skill for reviewed AXTP workspace/protocol domain-feature drafts. Use when an already-reviewed protocol draft should be approved, formalized, adopted, activated, landed, or converted into Registry/Capability Types specs alignment, Profiles Registry alignment when profiles/MVP change, and contract/registry/domain YAML facts. Fixes the draft as the formal accepted proposal but does not manually edit generated artifacts.
---

# Adopt Protocol Draft

Stage 30. Convert a reviewed AXTP protocol draft into formal protocol facts after `draft-business-protocol`: align the accepted proposal with `specs/30-registry.md` and `specs/40-codec.md`, freeze the draft as the formal proposal, and write the confirmed facts into YAML.

## Hard Boundaries

- Start from an existing `workspace/protocol/<domain>/<domain.feature>.md` draft.
- Do not adopt drafts containing unresolved `[REVIEW-BLOCKER]`, `[REVIEW-FIX]`, or `[REVIEW-ASK]` facts unless the user provides exact confirmed facts and the adoption is scoped around them.
- Do not invent method/event/error/capability/profile semantics that are not present in the reviewed draft or specs.
- Do not edit `contract/protocol/axtp.protocol.yaml`, `contract/generated/*`, `contract/mcp/*`, or `contract/test-vectors/*` by hand.
- Do not run full generated artifact emission as the main goal of this skill; use `generate-axtp-protocol` after adoption.
- New business features default to `contract/registry/domains/<domain>/domain.yaml`.
- Use core `contract/registry/` files only for Core/MVP adopted facts, shared schemas, core constants, profile governance, or accepted legacy mappings.
- Never duplicate the same method/event/error/schema/capability/profile in both core registry files and domain YAML.
- Preserve stable IDs, field IDs, `bitOffset`, and names.

## Required Evidence

Read enough local evidence to avoid guessing:

```text
workspace/protocol/README.md
workspace/protocol/<domain>/<domain.feature>.md
specs/30-registry.md
specs/50-tooling.md
specs/40-codec.md
contract/registry/**/*.yaml
contract/registry/domains/**/*.yaml
contract/generated/protocol.md
contract/protocol/axtp.protocol.yaml
```

For stream, firmware.update, transport-sensitive, or low-bandwidth features, also read Core wire/session specs. For legacy adoption, read cited legacy evidence under `workspace/legacy-migration/evidence/**`, `workspace/legacy-migration/plans/**`, or `contract/registry/legacy/legacy_mapping.yaml` if it already exists.

## Workflow

### 1. Confirm Draft Eligibility

Classify the draft:

| State | Meaning | Action |
|---|---|---|
| Adopted | YAML/specs already fully cover it | Do not duplicate; report existing facts |
| Partially adopted | Some facts exist, draft adds reviewed gaps | Patch only missing confirmed facts |
| Ready for adoption | Draft has `[REVIEW-OK]` or equivalent confirmation and no unresolved blockers | Proceed |
| Not ready | Unresolved review markers, missing field shapes, unclear domain, or no review evidence | Stop and list blockers |

If a draft has mixed readiness, adopt only explicitly confirmed sections and leave unresolved sections in the report.

### 2. Extract Accepted Facts

Extract only confirmed facts:

- `domain.feature` and domain boundary
- capability names and capability type/schema
- methods, request schema, response schema, linked events, errors
- events and event schemas
- schemas, field names, field types, required/optional status, limits, descriptions
- domain-specific errors
- stream/profile additions
- concrete legacy mappings
- open questions that must remain out of specs/YAML

### 3. Align Registry/Capability Types/Profiles specs

Reverse-confirm the accepted proposal into specs where needed:

| Spec | Confirm or update |
|---|---|
| `specs/30-registry.md` | domain.feature naming, method/event/error/profile registry rules, feature taxonomy, and compatibility boundaries |
| `specs/40-codec.md` | schema/type/capability rules and capability placement |
| `specs/50-tooling.md` | YAML source placement, ID ranges, DomainId, bitOffset, deterministic generation, and release binding |

Re-check `specs/30-registry.md` profile sections if the proposal changes profile membership, MVP requirements, or profile registry semantics.

Do not turn specs into the machine fact source. Specs are governance and normative explanation; YAML remains the machine input.

### 4. Freeze The Draft As Formal Proposal

Update the draft only enough to record adoption:

- Mark accepted sections as `[REVIEW-OK]` or add an adoption note/table.
- Preserve unresolved sections as open questions and keep them out of YAML.
- Record that future unadopted facts must update the `workspace/protocol` proposal first, then re-run adoption; already-adopted semantic changes must use `amend-adopted-protocol`.
- Do not remove useful review history unless it is misleading.

### 5. Choose YAML Targets

Use these defaults:

| Fact | Default target |
|---|---|
| New business method/event/schema/error/capability/profile | `contract/registry/domains/<domain>/domain.yaml` |
| Core/MVP stable method/event/capability | `contract/registry/method`, `contract/registry/event`, or `contract/registry/capability` only after governance confirmation; do not keep empty placeholder files |
| Shared schema used by multiple domains | `contract/registry/schema/*.yaml` only if truly shared |
| Legacy mapping | `contract/registry/legacy/legacy_mapping.yaml` only with concrete evidence; create the file only when at least one mapping is adopted |
| Core constants | `contract/registry/core/*.yaml` only after spec change confirmation |

If no valid target exists, stop and report the required governance change.

### 6. Assign IDs And bitOffset values

Compute from specs and existing YAML:

- Method IDs must be globally unique and in the correct domain range.
- Event IDs must be globally unique and in the correct domain range.
- Capability IDs and DomainId/Domain-Scoped Mask rules must follow YAML Mapping and Capability Types specs.
- `bitOffset` must be unique within the domain and should remain contiguous unless existing YAML intentionally reserves gaps.
- New schema field IDs start from `0x01` in accepted field order unless explicitly specified.
- Existing schema field IDs must be preserved; append with the next safe ID.
- Prefer existing common errors before adding domain-specific errors.

Never reuse deprecated or stable values for different semantics.

### 7. Edit YAML Sources

Use `apply_patch`. Keep edits scoped to Registry/Capability Types specs, Profiles Registry when applicable, the adopted draft, and chosen YAML sources.

Rules:

- Use source field names already used by the repo, such as `id`, `bitOffset`, `request_schema`, `response_schema`, and `event_schema`.
- Keep status as `draft` unless the draft and governance explicitly say MVP/stable.
- Preserve existing ordering patterns.
- Do not copy review prose, `[REVIEW-*]`, open questions, or `TBD` values into YAML.
- Do not create legacy mappings without concrete old values.

### 8. Validate Adoption Sources

Run source-level checks after specs/YAML edits:

```bash
pnpm --dir tooling/generators build
pnpm --dir tooling/generators test
pnpm --dir tooling/generators validate:sources
git diff --check
```

If pnpm dependency approval blocks scripts, use and report:

```bash
pnpm --dir tooling/generators --config.verify-deps-before-run=false build
pnpm --dir tooling/generators --config.verify-deps-before-run=false test
pnpm --dir tooling/generators --config.verify-deps-before-run=false validate:sources
git diff --check
```

Do not manually edit generated outputs in this skill. The next step is `generate-axtp-protocol`.

## Final Report

Report:

- Draft adopted and eligibility decision.
- Registry/Capability Types/Profiles specs files changed, if any.
- Draft file changes and adoption marker.
- YAML source files changed.
- Assigned IDs, `bitOffset`, and schema field IDs.
- Open questions or skipped draft sections.
- Validation commands and results.
- Next step: run `tooling/skills/50-generate-axtp-protocol/SKILL.md`.
- Future post-adoption semantic changes must use `tooling/skills/40-amend-adopted-protocol/SKILL.md`.

If adoption is blocked, do not edit YAML. Report the exact blocker and the minimum draft/spec update needed.
