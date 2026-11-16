---
name: amend-adopted-protocol
description: Stage 40 amendment skill for already-adopted AXTP protocol facts. Use after a reviewed draft has been formalized into specs, contract/registry/domain YAML, and generated artifacts, and the user asks to modify, correct, remove, deprecate, rename, narrow, or extend an adopted or generated method, event, schema field, error, capability, profile, or legacy mapping.
---

# Amend Adopted Protocol

Stage 40. Change a protocol that has already passed adoption and entered YAML/generated artifacts. This workflow is for post-adoption corrections and semantic refinements, such as removing an unnecessary draft schema field, deprecating a stable field, renaming a generated type, or tightening an already-adopted method.

## Hard Boundaries

- Start from existing adopted facts in `workspace/protocol/<domain>/<domain.feature>.md` plus `contract/registry/**/*.yaml` or `contract/registry/domains/**/*.yaml`.
- Do not infer new facts from generated files. Generated files are evidence of current output only.
- Do not hand-edit `contract/protocol/axtp.protocol.yaml`, `contract/generated/*`, `contract/mcp/*`, or `contract/test-vectors/*`.
- Do not silently break `stable` or `mvp` wire contracts. For stable/MVP facts, prefer `deprecated`, optional additions, or versioned replacements over deletion or semantic mutation.
- Preserve IDs, `bitOffset`, field IDs, method/event names, and capability IDs for unchanged semantics.
- Do not reuse a removed/deprecated stable ID or field ID for a different meaning.
- If the requested change lacks exact confirmed facts, update the adopted draft with `[REVIEW-ASK]` / `[REVIEW-FIX]` only and do not patch YAML.
- Legacy mappings require concrete legacy command IDs, payload/status mapping evidence, and an explicit target method/field path.

## Required Evidence

Read only what is needed, but include enough evidence to avoid guessing:

```text
workspace/protocol/README.md
workspace/protocol/<domain>/<domain.feature>.md
specs/30-registry.md
specs/50-tooling.md
specs/40-codec.md
contract/registry/**/*.yaml
contract/registry/domains/**/*.yaml
contract/protocol/axtp.protocol.yaml
contract/generated/protocol.md
```

For stream, firmware.update, transport-sensitive, or low-bandwidth changes, also read Core wire/session specs.

## Workflow

### 1. Confirm Amendment Eligibility

Classify the target before editing:

| State | Meaning | Action |
|---|---|---|
| Not adopted | Draft has no YAML/generated fact yet | Route to `draft-business-protocol` or `adopt-protocol-draft` |
| Adopted draft/experimental | Facts are adopted but not stable/MVP | Direct source amendment may be allowed after exact confirmation |
| Adopted stable/MVP | Wire contract is stable or required | Deprecate/version/add optional replacement; do not hard-delete |
| Generated-only mismatch | YAML is correct but generated output is stale | Route to `generate-axtp-protocol` |
| Unclear request | Missing exact field/method/semantic decision | Stop or update draft with `[REVIEW-ASK]`; do not patch YAML |

Report the eligibility decision before broad edits.

### 2. Capture Confirmed Change Facts

Extract only explicit facts from the user request and adopted draft:

- target `domain.feature`
- target method/event/type/capability/error/profile/legacy mapping
- fields to add, remove, deprecate, rename, or narrow
- whether existing status is `draft`, `experimental`, `mvp`, `stable`, or `deprecated`
- compatibility intent: breaking correction, deprecation, versioned replacement, or optional extension
- rationale and review source
- open questions that must remain out of YAML

Example: removing `mode` from draft audio algorithm schemas is a draft-field removal. If the same field were stable, the safer action would be deprecation or a versioned schema/method.

### 3. Choose Compatibility Action

Use this table:

| Change | draft/experimental | stable/mvp |
|---|---|---|
| Remove unnecessary optional field | May delete from draft, YAML, and generated output | Mark deprecated or keep ignored; do not reuse field ID |
| Remove required field | Allowed only with explicit breaking correction confirmation | Version the schema/method or keep field optional/deprecated |
| Rename field or method | Prefer replace before broad use; preserve semantics only if name is not wire-visible | Add new name/version and deprecate old |
| Narrow enum/range | Allowed if confirmed and not shipped | Treat as breaking; add version or compatibility rule |
| Add optional field | Append next safe field ID | Append next safe field ID |
| Change method/event/capability ID | Do not change unless fact was never generated and is explicitly corrected | Never change; allocate a new ID |

When removing draft fields, field IDs may disappear from draft YAML. When deprecating stable fields, keep the field entry with `deprecated: true` and do not reuse the field ID.

### 4. Update Adopted Proposal

Patch `workspace/protocol/<domain>/<domain.feature>.md` first:

- Add or update an `## Amendment History` / `## 修订记录` section.
- Record date, change summary, rationale, compatibility decision, and YAML targets.
- Remove or update stale examples, parameter tables, acceptance criteria, and registry notes.
- Keep unresolved or deferred facts as `[REVIEW-ASK]`; do not move them to YAML.
- Preserve useful adoption history.

### 5. Update Registry/Capability Types/Profiles specs Only Where Needed

Specs are governance, not the machine source. Update only when the amendment changes:

- domain.feature naming or method/event template guidance
- formal method/event/capability planning tables
- schema/type/capability rules
- error placement or domain-specific error tables
- field numbering/deprecation guidance
- profile membership, MVP requirements, or profile registry semantics

Do not copy full business prose into specs.

### 6. Patch YAML Sources

Patch `contract/registry/**/*.yaml` or `contract/registry/domains/**/*.yaml` with `apply_patch`.

Rules:

- Edit the existing source of truth; do not duplicate the same fact in core and domain YAML.
- Preserve unchanged IDs, `bitOffset`, field IDs, names, and statuses.
- Append new field IDs in accepted order.
- Remove draft-only fields only when confirmed and not stable/MVP.
- Deprecate stable/MVP fields instead of deleting.
- Update method request/response/event schema references if a type is renamed or versioned.
- Keep generated outputs untouched until the generator runs.

### 7. Validate Sources

Run:

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

If source validation fails, fix source YAML or specs alignment before generating.

### 8. Regenerate Artifacts

After source validation passes, run the generator stage:

```bash
pnpm --dir tooling/generators generate
pnpm --dir tooling/generators validate:protocol
git diff --check
```

If pnpm dependency approval blocks scripts, use:

```bash
pnpm --dir tooling/generators --config.verify-deps-before-run=false generate
pnpm --dir tooling/generators --config.verify-deps-before-run=false validate:protocol
git diff --check
```

If tests fail because snapshots intentionally changed, inspect the diff and update snapshots with the repo's Vitest snapshot workflow only after confirming the generated output matches the amended YAML facts.

### 9. Verify Output Coverage

Use targeted `rg` or JSON checks to confirm:

- removed facts no longer appear in protocol IR/generated docs when removal was intended
- deprecated facts remain visible with deprecation metadata when deprecation was intended
- new or replacement facts appear in `contract/protocol/axtp.protocol.yaml`, `contract/generated/protocol.md`, MCP JSON, C++ generated headers, and snapshots
- generated files changed only through the generator

## Final Report

Report:

- amendment target and eligibility decision
- confirmed change facts and compatibility action
- proposal/spec/YAML files changed
- generated files changed by Generator
- IDs, `bitOffset`, field IDs preserved, removed, deprecated, or newly assigned
- open questions or skipped sections
- validation and generation commands with results

If blocked, report the exact missing confirmation and the minimum draft/spec/YAML decision needed.
