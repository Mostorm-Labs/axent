# 2-registry/05《AXTP Profile Registry 规范》

> 状态： AXTP v1 规范性 registry 规范
> 范围：implementation profile、transport/profile 要求和 runtime 支持等级声明
> 权威边界：当前已采纳 profile 事实来自 registry YAML、Protocol IR、generated artifacts 和 conformance 文档。

## 文档目的

本文档定义 AXTP profile registry 的准入、组合、稳定性和实现声明规则。Profile 是 method、event、type、error、capability、transport 和 frame profile 的实现要求集合；Profile 不改变 wire format。

## 范围

本文档覆盖 implementation profile 条目结构、profile 对 method/event/type/error/capability/transport/frame profile 的引用规则、runtime 支持等级声明和 conformance 关系。本文档不维护完整 MVP/Profile 大表；历史和候选规划表保存在非规范附录：[appendix/profile-candidates.md](appendix/profile-candidates.md)。Runtime MUST NOT 从该 appendix 实现协议。

## 规范规则

1. Profile MUST 引用已经定义的 method、event、type、error、capability 和 transport profile；不得定义新的 wire 字段。
2. Profile MUST NOT 改变 methodId、eventId、errorCode、PayloadType、frame header 或 STREAM header 语义。
3. 一个 Standard Framed profile MAY 要求 CONTROL、RPC、STREAM；WebSocket Unframed JSON profile MUST NOT 要求 CONTROL 或 STREAM。
4. AXTP v1 不允许在同一 session 内动态切换 frame profile。
5. `frameProfile` 或 `frameProfiles` MUST 与所引用 transport profile 的承载方式一致。
6. Low-bandwidth profile 必须显式标记为 profile-specific 或 future，不得反向修改 Standard Framed Binary 的核心定义。
7. Runtime 支持某 profile 时，MUST 满足该 profile 引用的 required methods/events/errors/types/transports，并通过对应 conformance scope。

## Registry / Schema / Tooling 模型

最小 profile 条目模型：

```yaml
profiles:
  - name: AXTP-MVP-HID
    since: 1.0.0
    status: stable
    requiredMethods: []
    requiredEvents: []
    requiredErrors:
      - SUCCESS
      - RPC_METHOD_NOT_FOUND
    transportProfiles:
      - AXTP-USB-HID
    frameProfile: STANDARD_FRAME
```

| 字段 | 要求 | 说明 |
|---|---|---|
| `name` | MUST | profile name，全局唯一 |
| `since` | MUST | 首次进入 registry 的版本 |
| `status` | MUST | 生命周期状态 |
| `requiredMethods` | MUST | method name 引用，可为空 |
| `requiredEvents` | MUST | event name 引用，可为空 |
| `requiredTypes` | SHOULD | schema/type 引用 |
| `requiredErrors` | MUST | error name 引用 |
| `requiredCapabilities` | MAY | capability 引用 |
| `transportProfiles` | MUST | transport profile 引用 |
| `frameProfile` / `frameProfiles` | 适用时 MUST | frame profile 约束 |

## 校验规则

Generator MUST 至少校验：

1. profile name 全局唯一；
2. required method/event/type/error/capability/transport 引用全部存在；
3. profile 的 frame profile 与 transport profile 约束一致；
4. WebSocket Unframed JSON profile 没有要求 CONTROL/STREAM；
5. low-bandwidth profile 没有冒充 v1 Core required profile；
6. generated docs 和 conformance coverage 能定位每个 profile 的 required 集合。

## 兼容规则

- 给 profile 增加 required method/event/error/type 通常会提高实现门槛，可能是 breaking change。
- 给 profile 增加 optional capability 通常是兼容变更。
- deprecated profile MUST 保留 generated 描述和 release 记录，直到绑定该 profile 的 runtimes 迁移完成。
- Runtime 仓库 MUST 绑定 spec tag、明确 commit 或 release artifact，不得依赖浮动 main 声明 profile 支持。

## 实现要求

- Runtime MUST 明确声明支持的 profile 名称、spec tag 或 commit、conformance 结果和不支持范围。
- Generator SHOULD 针对 profile 输出 generated reference、profile descriptor 和 conformance 输入。
- Conformance MUST 以 profile 为最小验收单元之一，区分 Required、Optional 和 Future。
- SDK SHOULD 暴露 profile support metadata，帮助应用选择可用 transport 和功能集合。

## 示例

```text
AXTP-MVP-HID             # USB HID 上的 Standard Framed profile
AXTP-WS-JSON             # transport profile，仅 RPC，没有 CONTROL/STREAM
AXTP-HID-64-COMPACT      # low-bandwidth/future profile，不是 v1 Core 必需项
```

## 非目标 / 未来

本文档不列出完整 MVP/Profile 规划表。历史 planning/current 表已移至 [appendix/profile-candidates.md](appendix/profile-candidates.md)，仅用于审计和迁移参考。正式实现 MUST 以 `contract/registry/**/*.yaml`、`contract/protocol/axtp.protocol.yaml`、`contract/generated/**` 和 `conformance/**` 为准。
