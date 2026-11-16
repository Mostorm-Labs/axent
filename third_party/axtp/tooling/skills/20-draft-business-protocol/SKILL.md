---
name: draft-business-protocol
description: Stage 20 drafting skill for AXTP business protocol proposals. Use when business intake or flow planning has identified a concrete AXTP protocol gap that needs a workspace/protocol domain-feature draft, including method/event/schema/error/capability/profile candidates, JSON examples, stream or firmware.update behavior, or protocol classification. Writes workspace/protocol drafts only by default and must not write registry YAML or generated artifacts.
---

# Draft Business Protocol

Stage 20. Create or update an AXTP business protocol draft in `workspace/protocol/<domain>/<domain.feature>.md`. Use this stage after business intake or flow planning has identified concrete protocol semantics that need review.

## Boundaries

- Edit only `workspace/protocol/**` unless the user explicitly asks for documentation around the draft.
- Do not edit `contract/registry/**`, `contract/registry/domains/**`, `contract/protocol/axtp.protocol.yaml`, `contract/generated/**`, `contract/mcp/**`, or `contract/test-vectors/**`.
- Do not assign final numeric IDs unless they already exist in YAML/specs; use `TBD after adoption` for new methodId/eventId/errorCode/fieldId values.
- Do not introduce new PayloadType, Frame Header business fields, WebSocket STREAM support, or runtime Header Profile negotiation.
- Do not use this skill for raw PRD intake. If the user is still describing product intent, customer value, UI goals, or unresolved business scope, route to `business-intake`.
- Every method must keep one compact `d block 示例` subsection with `request:` and `success:` examples so reviewers can see the callable shape without hunting through tables.
- Do not split method examples into separate request and success headings. Generic error envelope examples are not copied into every method.
- Follow `workspace/protocol/draft-conventions.md` for JSON envelope, error, schema expansion, flow example, and contract-boundary conventions. Drafts should link to the common convention instead of repeating those rules.
- Business feature examples assume the RPC Session is already `APP_READY`; show only feature-specific RPC `d` blocks after the common convention link.
- Method examples use one heading named `d block 示例` and label the two blocks as `request:` and `success:`. Error Response examples are only kept when they show feature-specific errors, special `details`, recovery behavior, or state consequences. Event examples live in the Events section, use a `d block 示例` heading, and are only kept when payload shape, async state, cache semantics, or trigger behavior would be unclear without one.
- Do not use JSON-RPC 2.0 (`jsonrpc: "2.0"`) as an AXTP wire example unless explicitly documenting an external adapter representation.
- Always leave `[REVIEW-*]` markers for human review. Unconfirmed facts must be `[REVIEW-ASK]`, `[REVIEW-DRAFT]`, `[REVIEW-FIX]`, or `[REVIEW-BLOCKER]`.

## Required Workflow

### 1. Locate AXTP Context

Work from the repository root containing `specs`, `workspace/protocol`, and `contract/registry`. Read only the relevant parts of:

```text
workspace/protocol/README.md
workspace/protocol/draft-conventions.md
specs/30-registry.md
specs/50-tooling.md
specs/40-codec.md
```

For stream, firmware.update, transport-sensitive, HID media, or low-bandwidth work, also read:

```text
specs/20-core.md
```

Search `workspace/protocol/**`, `contract/registry/**`, and `contract/registry/domains/**` for the requirement keywords and likely English equivalents.

### 2. Decide Reuse, Modify, Or Create

Make one decision and explain it in the draft and final response:

| Decision | Use when | Action |
|---|---|---|
| Reuse existing draft | Existing `workspace/protocol/<domain>/<domain.feature>.md` already covers the requirement | Add a short review note or leave unchanged if complete |
| Modify existing draft | Existing domain.feature is right but lacks scenario, method, event, schema, error, or boundary details | Patch that draft; preserve existing review markers and user edits |
| Create new draft | No existing feature has the right boundary | Create `workspace/protocol/<domain>/<domain.feature>.md` from `references/protocol-draft-template.md` |

Choose `domain.feature` by `specs/30-registry.md`: feature is a capability block, not a field name. Keep `stream` for common data plane only; business streams are owned by the business domain and bind to STREAM through RPC-created `streamId`.

### 3. Inspect Implementation Degree

Check whether matching facts already exist:

```text
contract/registry/**/*.yaml
contract/registry/domains/**/*.yaml
contract/generated/protocol.md
contract/protocol/axtp.protocol.yaml
```

Use the result to label implementation degree:

| State | Meaning |
|---|---|
| Not drafted | No matching protocol draft found |
| Drafted only | Exists in `workspace/protocol`, not in YAML |
| Partially adopted | Some facts exist in YAML, but draft has gaps |
| Adopted | YAML/generated already cover the feature |

If adopted, do not create a duplicate draft. If the user asks for a semantic change, field removal, deprecation, rename, narrowing, or extension to adopted facts, route to `amend-adopted-protocol`; otherwise add a review note explaining the existing path and any remaining gap.

### 4. Write The Draft

Use `apply_patch` for manual edits. A draft must include:

- frontmatter status and contract state
- a `0. 速读结论` table summarizing purpose, state, implementation contract status, main interaction type, STREAM usage, registry readiness, conformance state, and unresolved questions
- a short convention reference, usually in the intro or quick-read section, linking to `workspace/protocol/draft-conventions.md`; do not add a standalone boilerplate JSON convention section
- concise function description
- capability boundary: included, excluded, and data-plane usage
- method section with two layers: `3.0 方法速览` and one independent subsection per method
- method overview table columns: Method, 调用类型, 用途, Params Schema, Result Schema, 是否触发事件, 状态
- per-method detail blocks with purpose, call type, Params Schema, Result Schema, event trigger behavior, idempotency/async notes, common errors, params fields, result fields, one compact `d block 示例`, possible events, method-specific error table, and method rules
- params/result field table headings that include the schema name, such as `请求参数 Params：SetVolumeParams` and `返回结果 Result：AudioVolumeState`
- event section with two layers: `4.0 事件速览` and one independent subsection per event
- event overview table columns: Event, 触发条件, Payload Schema, 客户端处理建议, 状态
- per-event detail blocks with trigger conditions, Payload Schema, payload field table, one compact event `d block 示例` when needed, client handling advice, and event rules
- event payload field table headings that include the schema name, such as `Payload：XxxChangedEvent`
- capability table with field, type, required, range/enum, and description
- schema field tables with field, type, required, range/enum, default, and description
- one compact method `d block 示例` for every method; add extra error/event examples only for complex payloads, event payloads, STREAM setup, async transitions, permission/error branches, or legacy field conversions
- a `7. 交互流程示例 Flow Examples` section for end-to-end multi-method/event flows only; do not use it as the only place for single method/event API examples
- candidate errors
- legacy mapping candidates or `[REVIEW-ASK]`
- contract/registry/conformance status
- test notes
- a `12. 待确认问题` table with issue, impact, current recommendation, and status

Prefer concise tables. Do not force a heavy Purpose/Scope/Lifecycle/Stream Usage/Non-goals template on simple features. Complex features may add state machines, interaction flows, STREAM details, or more examples only when useful. Use `TBD after adoption` for numeric IDs.

The default `workspace/protocol/<domain>/<domain.feature>.md` draft is a human-readable review artifact, not the machine truth source and not a runtime implementation contract. It should help product/architecture understand scope, engineers see method/event/schema details, test owners derive conformance cases, migration owners map legacy commands, and protocol maintainers judge registry readiness. Formal machine truth remains in `contract/registry/**/*.yaml`, `contract/protocol/axtp.protocol.yaml`, `contract/generated/**`, and `conformance/**`.

For schema-heavy features, follow `workspace/protocol/draft-conventions.md#schema-展开约定` and organize the schema section for reading, not just completeness:

- Start with a schema hierarchy overview that explains the main data blocks and how they relate.
- Separate request/response schemas, capability schemas, runtime config/state schemas, and event payload schemas.
- For object families, split each object into its own field table instead of putting every nested field into one long flat table.
- Explain `Capabilities` / descriptor schemas as "what the device can do" and `Config` / `State` schemas as "what currently applies or will be changed".
- Include range, enum, unit, default, and restart/apply notes close to the field table where readers need them.
- Choose one schema expansion mode and stay consistent.
- For simple features, expand params/result/payload fields directly under each method/event, and keep the schema section as an index.
- For complex features, keep method/event blocks readable by naming the schema and linking to the exact schema subsection, but still include the key fields and feature-specific JSON `d` block examples needed to understand the method/event without hunting through the whole document.
- Never make readers infer whether a field table is method params, method result, event payload, shared schema, or capability. The heading and nearby prose must say which schema it belongs to.
- Keep capability fields only in the Capability section; do not mix capability descriptors into method params/results or event payloads.

When creating a new draft or materially updating an existing draft, keep method examples compact and consistent. Keep the common rules in `workspace/protocol/draft-conventions.md` and keep each draft focused on the feature:

- Every method gets one `d block 示例` subsection containing `request:` and `success:` examples.
- Keep examples minimal: include only fields needed to understand target/scope, params, result shape, accepted-vs-final-state semantics, created IDs, STREAM setup, credentials, or legacy field conversion.
- Do not create separate Request / Success Response headings just to show `op=7`, `op=8`, `status.ok`, or `id` echoing; those are common conventions.
- Add Error Response examples only when the feature has a meaningful error branch, permission boundary, candidate error, partial-apply rule, or recovery instruction.
- Keep Event examples in the Events section when payload shape, intent, changedFields, state snapshot, async transition, or client cache behavior needs review. Use `d block 示例` as the heading; do not use the old English `Event d block Example (op=6)` heading.
- Examples SHOULD show only the RPC `d` block to keep long feature drafts readable.
- Request examples must include `id`, `method`, and optional `params`; `id` must be non-zero.
- Response examples must echo request `id`, include `status.ok`, and use numeric `status.code`; use `0` for success.
- Event examples must include `event`, numeric `intent`, and optional `data`; events must not include request `id`.
- A failure response still uses `op=8`, uses `status.ok=false`, and must not carry business `result`.
- Do not use string error names in `status.code`. Use adopted numeric ErrorCode values from specs/YAML/generated. If a draft-only candidate error lacks a numeric code, either use the nearest adopted common numeric code in the JSON example and put the candidate name in `status.details.candidateError`, or state that the final code is `TBD after adoption` in prose outside JSON.
- Validate JSON examples mentally or with a parser when practical; examples should be readable and syntactically valid.
- Use placeholders for secrets, credentials, tokens, keys, personal data, serial numbers, MAC addresses, and IP addresses when exact values are not confirmed.
- Mark unconfirmed example fields or behavior in nearby prose with `[REVIEW-ASK]`.
- For existing drafts, refresh method examples when method params/results are missing, still use split Request/Success headings, or no longer match the field tables.

The old centralized `JSON 示例` section is deprecated. Section 7 in new drafts should be `交互流程示例 Flow Examples` and should only show end-to-end flows such as capability discovery -> set method -> changed event, action accepted -> state event, failure request -> no event, or reconnect -> get state calibration.

For complex or high-risk features, append optional review appendices instead of forcing them into every draft. Use these appendices for reset/factory restore, firmware update, security/auth, network config, storage format, lifecycle/reboot/shutdown, or any capability that may disconnect, delete data, change permissions, change software versions, or alter device identity:

- `附录 A. 协议审核标记`
- `附录 B. 协议决策记录`
- `附录 C. Registry 草案输入`
- `附录 D. 采纳检查清单`

Appendix Registry draft input is only draft input: never assign final methodId/eventId/errorCode/fieldId values in Markdown; use `TBD after adoption`.

### 5. Final Response

Return:

- decision: reuse, modify, or create
- draft file path
- what was added or changed
- method `d block 示例` examples added or refreshed, plus any feature-specific error/event examples kept
- key review questions / `[REVIEW-*]` blockers
- confirmation that no contract/registry/generated/protocol files were modified
- next step: after human review, use `tooling/skills/30-adopt-protocol-draft/SKILL.md`

## Review Marker Semantics

| Marker | Meaning |
|---|---|
| `[REVIEW-DRAFT]` | Reasonable draft candidate, not yet accepted |
| `[REVIEW-OK]` | Naming/boundary/interface direction looks acceptable |
| `[REVIEW-FIX]` | Must revise before adoption |
| `[REVIEW-ASK]` | Needs product, architecture, device, or legacy confirmation |
| `[REVIEW-BLOCKER]` | Would mislead adoption or generation if formalized as-is |

Never move `[REVIEW-ASK]`, `[REVIEW-FIX]`, or `[REVIEW-BLOCKER]` facts into YAML in this skill.

## Useful Commands

```bash
rg --files workspace/protocol contract/registry specs
rg -n "keyword1|keyword2|关键词" workspace/protocol contract/registry specs
git diff --check -- workspace/protocol
git status --short workspace/protocol specs contract/registry contract/protocol contract/generated
```
