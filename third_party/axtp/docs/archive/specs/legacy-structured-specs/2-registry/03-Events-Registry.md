# 2-registry/03《AXTP Event Registry 规范》

> 状态： AXTP v1 规范性 registry 规范
> 范围：`events:` 条目、eventId 分配、event payload schema 绑定和 event 准入规则
> 权威边界：当前已采纳 event 事实来自 registry YAML、Protocol IR 和 generated artifacts。

## 文档目的

本文档定义 AXTP event registry 的准入、命名、编号、订阅掩码和生成规则。Event 是 RPC 层的异步通知，用于描述状态变化、进度、结果或上报；Event 不替代 RPC Response。

## 范围

本文档覆盖 event 条目结构、eventId/bitOffset 稳定性、event payload schema、severity、trigger、capability 引用规则，以及 domain-scoped `eventMasks` 与 event bitOffset 的关系。本文档不维护完整 EventId 大表；历史和候选规划表保存在非规范附录：[appendix/event-candidates.md](appendix/event-candidates.md)。Runtime MUST NOT 从该 appendix 实现协议。

## 规范规则

1. Event MUST 通过 `PayloadType=RPC` 且 `rpcOp=EVENT` 承载；eventId MUST NOT 出现在 Frame Header、CONTROL payload 或 STREAM payload header 中。
2. Event name MUST 全局唯一，并采用 `domain.<eventName>` 形式；domain 前缀 MUST 与条目的 `domain` 一致。
3. Event name MUST 表达变化、结果、进度或上报语义，SHOULD 使用 `Changed`、`Completed`、`Failed`、`Progressed` 等后缀。
4. `eventId` MUST 是全局唯一的 `uint16`。stable eventId MUST NOT 改变语义或复用。
5. `bitOffset` MUST 在同一 domain 内唯一，并用于 domain-scoped `eventMasks`。stable bitOffset MUST NOT 复用。
6. 每个 event MUST 绑定 event payload schema；不得只注册事件名而没有 payload 模型。
7. Event MUST NOT 表达一次 request 的同步成败；同步成败由 RPC Response 表达。
8. 高频连续数据 MUST 通过 STREAM 承载；Event 只能通知状态、进度、结果或有限大小数据。
9. 新增业务 event 默认进入 `contract/registry/domains/<domain>/domain.yaml`。Core/shared event 晋升后才进入核心 event registry；不得重复定义。

## Registry / Schema / Tooling 模型

最小 event 条目模型：

```yaml
events:
  - id: 0x0901
    name: audio.algorithmConfigChanged
    domain: audio
    status: stable
    bitOffset: 0
    since: 1.0.0
    event_schema: AudioAlgorithmConfigChangedEvent
    severity: info
```

| 字段 | 要求 | 说明 |
|---|---|---|
| `id` | MUST | `uint16` eventId，二进制 RPC EVENT 使用 |
| `name` | MUST | event name，全局唯一 |
| `domain` | MUST | 业务 domain，必须与 name 前缀一致 |
| `status` | MUST | 生命周期状态 |
| `bitOffset` | MUST | domain-scoped `eventMasks` 位号 |
| `since` | MUST | 首次进入 registry 的版本 |
| `event_schema` | MUST | 事件 payload schema 引用 |
| `severity` | SHOULD | `info`、`warning`、`error`、`critical` 等 |
| `trigger` | MAY | 触发来源，用于文档和测试 |
| `capabilities` | MAY | 关联 capability |

`eventMasks` 按 domain 分段。domain 内第 N bit 置 1 表示订阅 `bitOffset=N` 的 event。`eventMasks` 的完整 RPC 会话行为由 `specs/1-core/06-RPC-Session.md` 定义。

## 校验规则

Generator MUST 至少校验：

1. `id` 在所有 event 中唯一，并处于允许的 eventId 范围；
2. `name` 全局唯一，且 `domain` 与 name 前缀一致；
3. 同一 domain 内 `bitOffset` 唯一；
4. `event_schema`、`capabilities[]` 引用存在；
5. stable/deprecated/reserved eventId 没有被新条目复用；
6. event payload schema 可生成 JSON/TLV validation；
7. Protocol IR 与 generated docs 中的 event facts 与 source YAML 一致。

## 兼容规则

- 新增 event 通常是兼容变更；修改 stable eventId、name、payload 字段含义或触发语义通常是 breaking change。
- 废弃 event MUST 保留 id 和 generated enum，状态标记为 `deprecated`，不得把 id 分配给新 event。
- 接收端 MUST 能忽略未知 event 或未订阅 event，不得因为未知 event 导致 session 失效。
- event payload schema 演进 MUST 遵守 schema numbering 与 type-system 兼容规则。

## 实现要求

- Runtime MUST 使用 generated registry 或 Protocol IR 解析 eventId/name。
- Runtime SHOULD 在发送 event 前确认当前 session 的订阅策略；如果 profile 明确允许全量广播，可忽略 masks。
- SDK SHOULD 生成强类型 event payload decoder，并保留忽略未知 event 的路径。
- Conformance tests SHOULD 覆盖 event id/name 映射、eventMasks、payload validation 和未知 event 行为。

## 示例

```text
audio.algorithmConfigChanged
firmware.updateProgressed
system.rebootScheduled
```

不推荐：

```text
audio.changeConfig
stream.videoFrame
```

## 非目标 / 未来

本文档不列出完整 EventId 规划表。历史 planning/current 表已移至 [appendix/event-candidates.md](appendix/event-candidates.md)，仅用于审计和迁移参考。正式实现 MUST 以 `contract/registry/**/*.yaml`、`contract/protocol/axtp.protocol.yaml` 和 `contract/generated/**` 为准。
