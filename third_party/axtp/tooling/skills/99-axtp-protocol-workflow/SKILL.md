---
name: axtp-protocol-workflow
description: Stage 99 top-level router for AXTP protocol lifecycle work. Use when the user asks to add, change, amend, migrate, adopt, generate, release, or implement an AXTP business capability, method, event, schema/type, error, capability, profile, stream/firmware.update flow, scenario interaction flow, UI-prototype-driven protocol plan, PRD/business requirement, or legacy mapping but has not specified the exact lifecycle stage. Routes rough requirements to business-intake, scenarios to plan-protocol-flow, protocol gaps to draft-business-protocol, reviewed drafts to adopt-protocol-draft, already-adopted semantic changes to amend-adopted-protocol, YAML refresh to generate-axtp-protocol, and spec publishing to release-axtp-spec.
---

# AXTP Protocol Workflow

Stage 99. Use this skill as the top-level router for AXTP protocol lifecycle work. It decides whether the task belongs in business intake, flow planning, protocol drafting, adoption, post-adoption amendment, generation, release, direct maintenance, or runtime implementation.

This skill is a coordinator. It should not create protocol content itself unless the task is narrow governance maintenance that is explicitly safe.

## Workflow Decision

Classify the request before editing anything:

| User state | Correct workflow | Allowed edits |
|---|---|---|
| Rough product, architecture, customer, legacy, or UI idea not ready for flow/draft | Use `tooling/skills/00-business-intake/SKILL.md` | `workspace/business/**` only |
| Business scenario, user story, UI prototype, or end-to-end interaction needs protocol mapping | Use `tooling/skills/10-plan-protocol-flow/SKILL.md` | `workspace/flows/**` only |
| Concrete missing capability, method, event, schema, error, profile, or feature semantics need reviewable protocol design | Use `tooling/skills/20-draft-business-protocol/SKILL.md` | `workspace/protocol/**` only |
| Existing `workspace/protocol/<domain>/<domain.feature>.md` draft needs review or refinement | Use `draft-business-protocol` | `workspace/protocol/**` only |
| Reviewed draft should become formal protocol | Use `tooling/skills/30-adopt-protocol-draft/SKILL.md` | `workspace/protocol/**`, needed specs, `contract/registry/**`, `contract/registry/domains/**` |
| Already-adopted/generated protocol needs semantic correction, field removal, deprecation, rename, or extension | Use `tooling/skills/40-amend-adopted-protocol/SKILL.md` | adopted proposal, needed specs/YAML, generated artifacts via Generator |
| YAML facts are ready and artifacts need refresh | Use `tooling/skills/50-generate-axtp-protocol/SKILL.md` | generated artifacts only, unless validation exposes a source bug |
| Verified spec needs tag/release artifact/runtime notification | Use `tooling/skills/60-release-axtp-spec/SKILL.md` | release metadata, tag, artifact, dispatch |
| Governance-confirmed registry maintenance without a draft | Use this skill directly only for narrow, non-design maintenance | Scoped YAML and then `generate-axtp-protocol` |
| Runtime/SDK/tool implementation after protocol adoption | Use generated docs, tooling JSON, test vectors, and the relevant runtime repository | runtime/SDK/tool repository only |

Default to `business-intake`, `plan-protocol-flow`, `draft-business-protocol`, adoption, or amendment. Direct registry edits are exceptional.

## Lifecycle

```text
business-intake
  rough requirement / product idea -> workspace/business/<topic>.md
                                  -> route to flow, draft, amendment, implementation, or no protocol work

plan-protocol-flow
  scenario / UI prototype / user story -> workspace/flows/<scenario>.md
                                      -> gap list for draft/amend workflows

draft-business-protocol
  concrete protocol gap -> workspace/protocol/<domain>/<domain.feature>.md

adopt-protocol-draft
  reviewed draft -> specs alignment if needed
                 -> formalized workspace/protocol proposal
                 -> contract/registry/**/*.yaml + contract/registry/domains/**/*.yaml

amend-adopted-protocol
  adopted/generated protocol change
                 -> amendment note in workspace/protocol proposal
                 -> specs/YAML alignment if needed
                 -> generated artifacts refreshed by Generator

generate-axtp-protocol
  registry YAML -> contract/protocol/axtp.protocol.yaml
                -> contract/generated/*
                -> tooling/*
                -> validation report

release-axtp-spec
  validated generated state -> spec/vX.Y.Z tag
                            -> release artifact
                            -> optional runtime notification
```

## Non-Negotiables

- Do not convert rough requirements or scenario flow plans directly into YAML.
- Do not skip `workspace/business/**` when the request is still product intent rather than protocol semantics.
- Do not put scenario-only UI behavior into protocol drafts; keep it in `workspace/flows/**`.
- Do not adopt unresolved `[REVIEW-ASK]`, `[REVIEW-FIX]`, or `[REVIEW-BLOCKER]` facts.
- Do not edit `contract/protocol/axtp.protocol.yaml`, `contract/generated/*`, `contract/mcp/*`, or `contract/test-vectors/*` by hand.
- New business features default to business intake, flow, or protocol draft first, then `contract/registry/domains/<domain>/domain.yaml` after adoption.
- Already-adopted semantic changes must update the proposal and YAML facts first, then regenerate; do not patch generated files directly.
- Use core `contract/registry/` files only for core constants, shared schemas, MVP/Core adopted entries, profile governance, and accepted legacy mappings.
- Preserve stable wire values. Never reuse deprecated or stable IDs, field IDs, or `bitOffset` values for different semantics.

## Direct Maintenance Guardrail

Before direct registry maintenance, report:

```text
Reason direct maintenance is safe:
Evidence read:
YAML targets:
IDs/fieldIds/bitOffset preservation plan:
Generated outputs expected:
Open questions:
```

If the user is still describing a product need, route to `business-intake`. If the user is describing an end-to-end story or UI interaction, route to `plan-protocol-flow`. If protocol semantics are already being designed, route to `draft-business-protocol`.

## Final Report

Report:

- Which lifecycle stage was used.
- Which skill handled the work.
- Evidence read.
- Files changed.
- Generated files changed by Generator, if any.
- Validation commands and results.
- Unresolved questions or skipped sections.

