---
name: plan-protocol-flow
description: Stage 10 flow-planning skill for AXTP protocol interactions. Use when business scenarios, user stories, UI prototypes, product workflows, or end-to-end feature requirements need to be mapped into actors, screens, protocol calls/events/capabilities, existing protocol coverage, gaps, and follow-up draft-business-protocol or amend-adopted-protocol work. Writes workspace/flows Markdown only by default and must not write registry YAML or generated artifacts.
---

# Plan Protocol Flow

Stage 10. Turn a business scenario, user story, UI prototype, or end-to-end workflow into a reviewable AXTP protocol interaction plan under `workspace/flows/**`.

This skill sits before `draft-business-protocol`: it discovers which existing protocols are needed across the whole story, records what is already adopted or drafted, and identifies gaps that should be handed to `draft-business-protocol`, `amend-adopted-protocol`, or runtime implementation.

## Boundaries

- Edit `workspace/flows/**` by default.
- Do not edit `contract/registry/**`, `contract/registry/domains/**`, `contract/protocol/axtp.protocol.yaml`, `contract/generated/**`, `contract/mcp/**`, or `contract/test-vectors/**`.
- Do not change `workspace/protocol/**` unless the user explicitly asks to continue into the draft/update stage.
- Do not invent adopted method names. Use generated/YAML facts when adopted, and mark unadopted candidates as gaps.
- If an adopted protocol needs a semantic change, route to `amend-adopted-protocol`, not `draft-business-protocol`.
- If a missing or draft-only method/schema is needed, record the gap and route to `draft-business-protocol`.

## Required Workflow

### 1. Collect Scenario Inputs

Extract:

- product or device family
- user goal and success criteria
- actors: user, App, cloud/server, device, firmware services, SDK/tooling
- UI screens, controls, fields, default values, save/apply/reset behavior, and validation messages
- story steps and alternate/error paths
- expected device behavior, events, latency, persistence, and restart requirements
- known legacy commands, existing docs, images, PDFs, or prototype files

If a UI image is attached, inspect it and list visible controls and fields in the flow doc. If the image is missing or inaccessible, leave `[REVIEW-ASK]` questions instead of guessing.

### 2. Locate AXTP Evidence

Read enough context to avoid guessing:

```text
docs/README.md
workspace/protocol/README.md
contract/generated/protocol.md
contract/protocol/axtp.protocol.yaml
contract/registry/**/*.yaml
contract/registry/domains/**/*.yaml
workspace/protocol/**
workspace/legacy-migration/classification/** when legacy evidence is relevant
workspace/legacy-migration/plans/** when migration strategy is relevant
```

Search with requirement keywords and likely English equivalents. For "算法参数调整", search at least:

```bash
rg -n "algorithm|算法|audio\\.algorithm|getAlgorithm|setAlgorithm|resetAlgorithm|capabilit" workspace/protocol contract/registry contract/generated contract/protocol
```

### 3. Build The Story Map

Split the scenario into steps:

| Step type | Examples |
|---|---|
| discovery | connect, identify, supported methods, capability query |
| read state | open page, load current config, load limits/defaults |
| user edit | toggle, slider, select, text input, reset |
| validation | local range check, device-side validation, permission check |
| apply | set config, start action, create stream, commit |
| observe | event, polling, progress, state changed |
| rollback | reset, cancel, restore defaults |
| error | unsupported method, invalid argument, busy, permission denied |

For each step, decide whether it is:

- local UI behavior with no protocol call
- existing adopted/generated protocol
- draft-only protocol
- missing protocol gap
- runtime/SDK implementation detail

### 4. Classify Protocol Coverage

Use these states:

| Coverage | Meaning | Next action |
|---|---|---|
| generated | `contract/generated/**` or protocol IR already define the method/event/schema/capability | Reference generated docs and use exact names |
| adopted | Registry YAML already defines the fact, but generated output is not the direct evidence in this flow | Reference registry path and verify generated status |
| draft | `workspace/protocol/**` has a candidate but YAML/generated do not | Route to `draft-business-protocol` after review |
| missing | No suitable draft or adopted/generated fact exists | Propose candidate `domain.feature` and route to `draft-business-protocol` |
| local-only | App/UI/runtime local behavior with no AXTP call | Keep as implementation/UI behavior |
| non-protocol | Product policy, manual operation, business rule, or documentation-only concern | Keep out of protocol work |

At the story level, summarize overall protocol coverage as:

| Summary | Meaning |
|---|---|
| complete | All protocol-relevant steps are generated/adopted and no gaps remain |
| partial | Some steps are covered, but at least one draft/missing/amendment gap remains |
| missing | The flow is protocol-relevant but has no adequate AXTP coverage |
| non-protocol | The flow does not require AXTP protocol work |

For the algorithm parameter example, prefer the adopted `audio.algorithm` path when it covers the UI:

- `audio.getAlgorithmCapabilities`
- `audio.getAlgorithmConfig`
- `audio.setAlgorithmConfig`
- `audio.resetAlgorithmConfig`
- `audio.algorithmConfigChanged`

If the UI needs fields or algorithms not present in generated/YAML, mark the affected step as `generated` or `adopted` evidence with an explicit amendment gap, and route that gap to `amend-adopted-protocol`.

### 5. Write The Flow Plan

Create or update:

```text
workspace/flows/<scenario-slug>.md
```

Use `references/flow-plan-template.md` as the structure. A flow plan must include:

- title and status
- `0. 速读结论` with flow goal, overall protocol coverage, involved `domain.feature`, adopted/generated facts, gaps, legacy/STREAM flags, and next action
- source inputs and UI/prototype observations
- device/system state observations when state, lifecycle, readiness, reset, pairing, connection, or fallback behavior matters
- actors and system boundaries
- business story summary
- assumptions and non-goals
- protocol coverage summary
- end-to-end sequence diagram
- step-by-step interaction table with actor, action, capability/precondition, protocol call/event, payload fields, result/state change, coverage, and error/fallback
- state changes and events table before protocol details
- protocol call/event details
- gaps and routed follow-up skill
- test/conformance notes using Given/When/Then and protocol evidence
- open questions as a table with impact, owner, status, and next action

Keep flow docs scenario-oriented. Do not duplicate full protocol schemas; link to `contract/generated/protocol.md` for generated facts, registry YAML for adopted facts, and `workspace/protocol/**` for drafts.

Flow docs are a business interaction and coverage-routing artifact. They should:

- describe business scenarios and interaction steps
- judge each step's protocol coverage
- identify protocol gaps
- route gaps to candidate `domain.feature`

Flow docs must not:

- define complete method/event/schema/capability contracts
- assign methodId/eventId/errorCode/fieldId values
- act as runtime implementation contracts
- replace `workspace/protocol/<domain>/<feature>.md`

Complete method/event/schema/capability definitions must be written in `workspace/protocol/<domain>/<feature>.md`.

### 6. Route Follow-Up Work

After writing the flow plan:

| Finding | Route |
|---|---|
| All required facts are adopted/generated | Runtime/App/server implementation can proceed from generated protocol |
| Missing new business capability or method | Use `tooling/skills/20-draft-business-protocol/SKILL.md` |
| Draft exists but lacks scenario fields | Use `draft-business-protocol` to modify that draft |
| Adopted fact needs semantic correction, field addition/removal, rename, or deprecation | Use `tooling/skills/40-amend-adopted-protocol/SKILL.md` |
| YAML facts are updated and outputs need refresh | Use `tooling/skills/50-generate-axtp-protocol/SKILL.md` |

Do not automatically jump into the next skill unless the user asked to implement the follow-up in the same turn or the workflow instruction explicitly requires it.

## Final Report

Report:

- flow plan file path
- scenario coverage decision
- adopted/generated protocols used
- draft-only or missing protocol gaps
- next skill for each gap
- files changed
- validation commands and results
- confirmation that no contract/registry/generated/protocol IR files were modified

## Useful Commands

```bash
rg --files workspace/flows workspace/protocol contract/registry contract/generated
rg -n "keyword1|keyword2|关键词" workspace/protocol contract/registry contract/generated contract/protocol
git diff --check -- workspace/flows tooling/skills
git status --short workspace/flows tooling/skills workspace/protocol contract/registry contract/protocol contract/generated
```
