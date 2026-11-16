---
name: business-intake
description: Stage 00 business intake skill for AXTP product, architecture, legacy, or customer requirements that are not yet ready for flow planning or protocol drafting. Use when the user describes a rough business need, PRD idea, product capability, scenario goal, customer request, legacy clue, UI intention, or unresolved problem and needs a workspace/business requirement brief before routing to plan-protocol-flow or draft-business-protocol. Writes workspace/business Markdown only by default.
---

# Business Intake

Stage 00. Turn a rough product, architecture, customer, legacy, or UI-driven idea into a business requirement brief under `workspace/business/**`.

This skill exists before flow planning and protocol drafting. Its job is to capture business intent clearly enough that later stages can decide whether to create a flow, draft a protocol, amend adopted facts, or do nothing protocol-level.

## Boundaries

- Edit `workspace/business/**` by default.
- Do not edit `workspace/flows/**`, `workspace/protocol/**`, `contract/registry/**`, `contract/registry/domains/**`, `contract/protocol/axtp.protocol.yaml`, `contract/generated/**`, `contract/mcp/**`, or `contract/test-vectors/**`.
- Do not invent final method, event, schema, error, capability, field ID, or registry names.
- Do not treat PRD text as runtime implementation contract.
- If the request already contains a complete end-to-end interaction, route to `plan-protocol-flow`.
- If the request already specifies concrete method/event/schema semantics, route to `draft-business-protocol`.
- If the request is already about adopted/generated protocol changes, route to `amend-adopted-protocol`.

## Required Workflow

### 1. Collect Business Inputs

Extract what is known:

- product, device family, or customer context
- user / operator / system goal
- actors such as App, cloud, device, firmware, SDK, runtime, factory, support
- success criteria and release pressure
- main scenario and important alternate/error paths
- visible UI controls, fields, states, or prototype observations when available
- constraints: permissions, latency, persistence, offline behavior, transport, low bandwidth, security, privacy, compatibility
- legacy commands, documents, screenshots, logs, or customer examples
- unknowns that block flow planning or protocol drafting

If details are missing, write `[REVIEW-ASK]` questions instead of guessing.

### 2. Read Existing Context

Read enough repo context to avoid duplicating existing business inputs:

```text
workspace/business/README.md
workspace/business/**
workspace/flows/README.md
workspace/protocol/README.md
docs/product/domain-status.md
specs/30-registry.md
contract/generated/protocol.md
workspace/legacy-migration/README.md when legacy is involved
```

Search with product terms, feature names, likely domain names, and legacy command names.

### 3. Decide Create Or Update

| Decision | Use when | Action |
|---|---|---|
| Reuse existing business doc | Existing `workspace/business/<slug>.md` already captures the need | Add only a short update or final note if needed |
| Update existing business doc | Existing doc has the right topic but lacks new constraints, actors, goals, or open questions | Patch that doc |
| Create new business doc | No existing business doc covers the requirement | Create `workspace/business/<scenario-or-capability>.md` |

Prefer slugs based on product capability or scenario, not proposed protocol method names.

### 4. Write The Brief

A business brief should be concise and useful to product, architecture, protocol maintainers, runtime teams, and test owners. Include:

- title and status
- source inputs
- business goal
- target users / actors
- in-scope and out-of-scope behavior
- main scenario
- alternate and error scenarios
- product constraints and non-functional requirements
- known legacy references
- expected device / App / cloud / runtime responsibilities
- possible AXTP impact without defining final protocol facts
- acceptance criteria in product language
- open questions table with owner, impact, and next action
- recommended next stage: none, `plan-protocol-flow`, `draft-business-protocol`, `amend-adopted-protocol`, runtime implementation, or legacy migration

Keep the document at business level. It may mention candidate domains such as `audio`, `video`, `device`, or `system`, but final `domain.feature`, method/event/schema names, IDs, and wire behavior belong to later stages.

## Routing Rules

| Finding | Next stage |
|---|---|
| Need is only product/background context | Stay in `workspace/business/**` |
| End-to-end actors, sequence, UI flow, or failure paths matter | `tooling/skills/10-plan-protocol-flow/SKILL.md` |
| A clear protocol capability/method/event/schema gap exists | `tooling/skills/20-draft-business-protocol/SKILL.md` |
| Adopted/generated facts need semantic change | `tooling/skills/40-amend-adopted-protocol/SKILL.md` |
| Existing generated protocol already covers the need | Runtime / SDK / App implementation can proceed from generated protocol |
| Need is mostly old-protocol evidence or adapter mapping | Legacy migration guide and classification flow |

Do not automatically continue into the next stage unless the user explicitly asks to keep going.

## Final Report

Report:

- business brief file path
- decision: reuse, update, or create
- main business goal and acceptance criteria captured
- unresolved `[REVIEW-ASK]` questions
- recommended next stage
- confirmation that no flow, protocol draft, registry, Protocol IR, generated, tooling, or conformance files were modified

## Useful Commands

```bash
rg --files workspace/business workspace/flows workspace/protocol docs/product contract/generated workspace/legacy-migration
rg -n "keyword1|keyword2|关键词" workspace/business workspace/flows workspace/protocol docs/product contract/generated workspace/legacy-migration
git diff --check -- workspace/business tooling/skills
git status --short workspace/business tooling/skills workspace/flows workspace/protocol contract/registry contract/protocol contract/generated
```
