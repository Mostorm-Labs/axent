# AXTP 业务模块审计 Checklist

> Date: 2026-06-10
> Scope: `workspace/protocol/**`, `workspace/business/**`, `workspace/flows/**`, `workspace/legacy-migration/**`, `contract/registry/domains/**`, `contract/generated/**`, `docs/conformance/**`
> Authority: specs 定义规则；registry YAML / Protocol IR / generated artifacts 才是 runtime implementation contract。

本 checklist 用于审计单个 `domain.feature` 业务模块是否可以从 draft/review 输入进入 registry、generated 和 conformance。审计时不要把 `workspace/protocol/**` 草案、`workspace/registry-planning/candidates/**` 候选表或 legacy evidence 当成 runtime 合同。

## 1. Classification

- [ ] 模块有明确 `domain.feature`，且 feature 是可评审、可测试、可演进的能力块。
- [ ] 模块没有使用字段级词汇作为 feature，例如 `Config`、`State`、`Scan`、`Connection`。
- [ ] 模块边界能说明它负责什么、不负责什么。
- [ ] 模块能说明与相邻 feature 的关系和差异。
- [ ] 模块没有把 transport、encoding、PayloadType、profile 名称误当成业务 feature。
- [ ] 模块能追溯到 `workspace/business/**`、`workspace/flows/**`、`workspace/protocol/**` 或 legacy evidence。

## 2. Lifecycle Status

- [ ] 标记了当前状态：`business-input`、`flow-planned`、`draft`、`review-ok`、`generated`、`deprecated` 或 `adapter-only`。
- [ ] 如果声明 `generated`，已确认 `contract/registry/domains/<domain>/domain.yaml` 或相关 registry YAML 存在。
- [ ] 如果声明 `generated`，已确认 `contract/generated/**` 中存在对应 method/event/capability/schema。
- [ ] 如果仍有 `[REVIEW-ASK]`、`[REVIEW-FIX]` 或 `[REVIEW-BLOCKER]`，不得进入 registry。
- [ ] 已采纳模块的语义变更走 amendment，而不是普通 draft。
- [ ] 草案版本、review 状态、Roadmap milestone 和 spec tag 没有混用。

## 3. Method Model

- [ ] 每个 method 名称符合 `domain.actionObject` 或已采纳命名模板。
- [ ] method 的 request schema 和 response schema 都明确。
- [ ] 无参数/无返回也有明确 empty schema 约定。
- [ ] method 声明可能返回的 error。
- [ ] method 声明关联 capability。
- [ ] method 与可能触发的 event 关系明确。
- [ ] method 没有把大块连续数据塞入普通 RPC response。
- [ ] method 没有要求修改 Frame Header、PayloadType、RPC op 或 STREAM header。

## 4. Event Model

- [ ] 每个 event 名称表达状态变化、结果、进度或上报语义。
- [ ] event 不替代同步 RPC response。
- [ ] event payload schema 明确。
- [ ] event 触发条件明确。
- [ ] event 与 method/capability/profile 的关系明确。
- [ ] event 是否需要 `eventMasks` 订阅已说明。
- [ ] event 不承载连续媒体/文件/日志数据；连续数据走 STREAM 或 file/stream profile。

## 5. Schema Model

- [ ] params/result/event payload 可以 schema 化。
- [ ] object 字段有稳定的 schema-local `fieldId` 规划。
- [ ] 字段名使用 lowerCamelCase，schema name 使用 PascalCase。
- [ ] 字段类型来自 AXTP type system 或已注册 schema。
- [ ] required/optional/default/range/enum/bitmap 语义明确。
- [ ] 新增字段兼容性明确；stable 字段不复用 fieldId。
- [ ] JSON 表达和 TLV/JSON_BINARY 表达没有语义分叉。
- [ ] legacy-only 字段没有污染 stable schema。

## 6. Capability Model

- [ ] capability 使用 `domain.feature` 命名。
- [ ] capability 描述“设备当前支持什么”，不表达具体 method 调用动作。
- [ ] capability 与 methods/events/schema/profile 的引用关系明确。
- [ ] capability availability 与 methodId 存在性没有混淆。
- [ ] capability discovery 不塞进 CONTROL OPEN/ACCEPT。
- [ ] 模块内细粒度 capability 查询与全局 capability discovery 的关系明确。

## 7. Error Model

- [ ] 模块复用已有 ErrorCode，避免发明局部错误码。
- [ ] 参数错误、范围错误、不支持、资源忙、权限错误、状态错误都有映射。
- [ ] 业务失败走 RPC response status，不靠 Frame/CONTROL 表达。
- [ ] STREAM 相关错误区分业务 source/codec 失败与 STREAM 数据面错误。
- [ ] legacy status code 映射只作为 adapter 元数据，不改变 AXTP ErrorCode 语义。
- [ ] 错误 details 中的 machine-readable 字段边界明确。

## 8. Stream Model

- [ ] 如涉及连续数据，已说明由哪个 RPC method 建立 Stream Context。
- [ ] `streamId`、`seqId`、`cursor` 的语义没有和 `requestId`、Frame `messageId` 混用。
- [ ] STREAM payload 只传连续数据；业务类型来自 Stream Context/profile。
- [ ] profile metadata 不写入 16B STREAM header。
- [ ] 可靠重传、resume、复杂 ACK/NACK 未作为 v1 required。
- [ ] WebSocket Unframed JSON 没有被要求承载 STREAM。
- [ ] 音视频同步、clockDomain、sourceId 等业务字段放在 RPC schema/profile metadata。

## 9. Registry Readiness

- [ ] 有明确 domain YAML 目标路径。
- [ ] method/event/error/capability/schema/profile 引用可以解析。
- [ ] methodId/eventId/errorCode/capabilityId/fieldId 不与现有 YAML 冲突。
- [ ] 草案临时 ID 未进入正式 registry。
- [ ] generated artifacts 能由 registry YAML 生成，不需要手写。
- [ ] 采纳后需要更新的 `contract/generated/**`、Protocol IR 和 generated JSON 已列出。
- [ ] 如涉及 profile，已同步确认 Profiles Registry 规则。

## 10. Conformance Readiness

- [ ] 有至少一个 happy path RPC case。
- [ ] 有 schema validation case：required 缺失、类型错误、范围错误。
- [ ] 有 error path case：unknown method、invalid params、unsupported capability 或 resource busy。
- [ ] 有 event emission/subscription case。
- [ ] 如涉及 STREAM，有 open/data/close 和 unknown streamId/invalid seq/cursor case。
- [ ] 有 capability-method binding case。
- [ ] runtime 支持等级、profile 和 spec tag/commit 可声明。

## 11. Legacy Mapping

- [ ] legacy evidence 来源、文件和行号明确。
- [ ] legacy command/event/config 与 AXTP method/event/schema 字段路径逐项映射。
- [ ] legacy mapping 的 confidence 明确。
- [ ] 待确认项没有写入 stable YAML。
- [ ] adapter-only 行为没有伪装成新 AXTP capability。
- [ ] legacy status/error 映射到 AXTP ErrorCode 或 adapter detail。
- [ ] legacy 默认值、factory reset、profile reset 等语义已和 AXTP reset/default 区分。

## 12. Final Action

- [ ] `adopt`: 边界清晰、schema/error/event/capability/conformance 准备充分，可进入 registry。
- [ ] `amend`: 已 generated 模块需要兼容性修订或字段补充。
- [ ] `split`: 一个 feature 同时承担多个控制范围，需拆分。
- [ ] `merge`: 与相邻 feature 重复，需合并或改为 schema target。
- [ ] `defer`: 业务价值存在，但 schema/flow/legacy 证据不足，暂缓采纳。
- [ ] `adapter-only`: 只用于旧协议兼容，不进入正式 registry。
- [ ] `block`: 与 specs 冲突或会误导 runtime 实现，先修正文档。
