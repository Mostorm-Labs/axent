# Protocol Draft Conventions

本页集中维护 `workspace/protocol/**` 草案的公共写法。业务草案只保留 feature-specific 的方法、事件、字段、错误候选、legacy 映射和待确认问题，不在每篇草案里重复通用合同边界和 JSON envelope 说明。

## 合同边界

`workspace/protocol/<domain>/<domain.feature>.md` 是协议草案和评审输入，不是新的机器事实源。draft / review-ok 阶段不得作为 runtime 实现合同。

正式实现必须以 `contract/registry/**/*.yaml`、`contract/registry/domains/**/*.yaml`、`contract/protocol/axtp.protocol.yaml`、`contract/generated/**` 和 `conformance/**` 为准。草案状态以 frontmatter、速读结论和 [Product Domain Status](../../docs/product/domain-status.md) 共同判断。

## JSON 示例约定

草案中的 JSON 示例默认 RPC Session 已进入 `APP_READY`，`sid` 已由 Server 分配。Hello、Identify、Identified 属于 [RPC Session Spec](../../specs/20-core.md)，不在每篇业务 feature 草案中重复。

示例使用 AXTP RPC JSON envelope：

```json
{ "sid": "12345678", "op": 7, "d": {} }
```

除本页 envelope 速查外，业务草案 SHOULD 只展示 RPC `d` 数据块，并在标题中标明对应 `op`：

| op | 名称 | 用途 |
|---:|---|---|
| `6` | Event | 设备向客户端推送事件。 |
| `7` | Request | 客户端调用业务 method。 |
| `8` | RequestResponse | 设备返回业务 method 结果或错误。 |

`sid="12345678"`、`id=101`、`intent=1` 均为示例值。正式 methodId、eventId、fieldId、errorCode、intent bit 由 registry 采纳后分配。

草案示例默认只展示 feature-specific 的 RPC `d` block；字段集合若仍是候选或占位，必须在采纳前按真实 schema 收敛。不要在每个 method/event 示例下重复这句公共读法。

业务草案不得使用 JSON-RPC 2.0 外层格式作为 AXTP wire 示例；不要在 AXTP 示例中写 `jsonrpc`、JSON-RPC 外层 `id/method/params`，或把 JSON-RPC envelope 当作 AXTP envelope。

## 错误约定

错误处理和 numeric code 规则以 [Errors Registry](../../specs/30-registry.md) 为准。草案不得随意分配正式 numeric errorCode。

如果 registry 尚未采纳 feature-specific error，JSON 示例可以使用最近的通用错误码作为占位，并在 `status.details.candidateError` 中放候选错误名。采纳时必须由 registry 分配正式 numeric code。

失败响应仍使用 `op=8`，必须回显请求 `d.id`，`status.ok=false` 或非零 `status.code` 时不得携带业务 `result`。

草案不得保留 `<FEATURE_SPECIFIC_ERROR>` 这类无语义占位行；确实需要业务错误时，写出候选错误名、触发条件和是否可复用现有错误码。

## Schema 展开约定

简单 feature：method/event 小节直接展开 Params / Result / Payload 字段表，本章只保留 schema 索引。

复杂 feature：method/event 小节给出关键字段和 JSON `d` block 示例，第 6 章集中展开复杂对象；method/event 小节必须明确引用 schema 小节，不能只给 schema 名称让读者自己找。

草案如果暂时使用 `target`、`state`、`sampledAt` 这类通用壳字段，method 表格必须至少给出示例 target 值和 `state` 中的关键业务字段摘要。不要只写“当前状态、配置或查询结果”。采纳进入 registry 前，应把这些候选对象收敛为正式 schema 字段。

Capability 字段只描述“设备能做什么”，不得混入 method params/result 或 event payload。

## Capability Discovery 约定

草案可以声明 feature capability，但不要在每篇草案里重复 `supportedMethods` / `supportedEvents` 的通用字段说明。是否支持某个 method 或 event，采纳后应由 capability discovery、`contract/registry/**` 和 `contract/generated/**` 表达。

通用 capability 字段读法如下：

| 字段 | 用途 |
|---|---|
| `capability` | capability 名称，通常为 `domain.feature`。 |
| `supportedMethods` | 可选 method name array；正式支持情况以 generated contract 为准。 |
| `supportedEvents` | 可选 event name array；正式支持情况以 generated contract 为准。 |
| `supportedTargets` | 可选 target / channel / port / component / scope 列表。 |
| `constraints` | feature-specific 能力范围、限制、模式或策略摘要。 |

草案内只展开 feature-specific 的 target、constraints、枚举、范围或对象结构；不要把 capability discovery 当成 method params/result 的替代品，也不要在每篇草案尾部重复“Capability 字段见第 5 章”这类章节指路。

## Flow Example 约定

`交互流程示例` 只展示多个 method/event 组成的端到端业务流程。单个 method 的 Request / Success Response / Error Response 示例写在对应 method 小节；单个 event 的 Event 示例写在对应 event 小节。

Flow example 应说明调用顺序、关键 `d` block、客户端状态更新、事件订阅和异常处理，不承担完整协议合同定义。

不要在模板草案里复制“读取或修改 `<domain.feature>`”这类通用流程。只有当流程包含真实业务顺序、跨 method 依赖、事件订阅策略、状态机分支或异常恢复时，才保留 flow example。

## Method Example 约定

草案 method 小节应保留 Params / Result 字段表、触发事件、feature-specific 规则，以及最小可读的 Request / Success Response `d` block 示例。Error Response 示例只在能展示 feature-specific 错误、候选错误名、特殊 `details` 或状态后果时保留。通用读法如下：

| 示例类型 | 公共规则 |
|---|---|
| Request | `op=7`，`d.id` 在同一 RPC session 内未完成前不得复用，`d.method` 使用候选 method name。 |
| Success Response | `op=8`，必须回显 Request 的 `d.id`，`status.ok=true`，业务结果放在 `d.result`。 |
| Error Response | `op=8`，必须回显 Request 的 `d.id`，`status.ok=false` 或非零 `status.code`，不得携带业务 `result`。 |

Request / Success 示例用于帮助评审者看懂 payload 形状，不需要在示例后重复“result 是某 schema 快照，正式字段以 registry 为准”这类公共说明。Error 示例只有当它展示 feature-specific payload、状态机分支、特殊错误细节或 legacy 映射时，才留在草案正文。

推荐的 method 示例形态是一个 `d block 示例` 小节，内部用 `request:` / `success:` 两段展示；不要再为同一 method 分散创建两个英文 request / success 标题。

事件示例同理：除非需要说明 feature-specific payload、状态更新规则或重连恢复语义，不要在每个 event 后重复“事件不携带 `d.id`”这类公共读法。

通用错误（例如 unsupported feature/method、permission denied、busy、invalid argument）不需要在每篇草案的错误表里重复。错误表只保留 feature-specific 的错误候选、特殊触发条件、状态后果或 legacy 兼容说明。

## 测试约定

通用测试矩阵不在每篇草案中重复。进入 registry review 前，维护者至少应覆盖以下方向，并只在草案里记录 feature-specific 的补充项：

| 类型 | 通用要点 |
|---|---|
| happy path | capability discovery 后调用主要 query / command / action method，返回成功响应。 |
| event path | 会改变状态的 method 成功后，按需产生 changed / progress / state event。 |
| boundary case | 省略可选字段、非法 target、非法枚举、越界值、空列表和最大对象数量。 |
| error case | unsupported feature/method、permission denied、busy、invalid argument、version/capability mismatch。 |
| compatibility | 新旧 App / 设备组合下，未知可选字段可忽略，未知必填语义必须返回标准错误。 |

## Review 问题约定

草案默认都必须在采纳前检查 method/event 命名是否和 generated 事实重复、legacy 映射是否有证据、错误码是否复用现有 registry。不要在每篇草案里复制这些通用 open question。

`待确认问题` 章节只保留会影响该 feature 的具体问题，例如字段范围、状态机、legacy payload 语义、权限边界、profile 绑定或 conformance case 缺口。

不要在每篇草案里保留“`<feature>` 采纳前还需确认哪些 schema、事件和 conformance 细节？”这类万能问题。它只说明所有草案进入 registry 前都要做 review，并不会告诉评审者该 feature 真正卡在哪里。需要记录时，应改写为具体问题，例如“`resetConfig` 是否重置 `displayName`”、“legacy `CommonSetPanTiltZoom` 如何拆分”或“跨午夜 schedule 如何判定”。

`Registry / Conformance 状态` 表也是通用草案状态信息；除非某个 feature 已经有真实 registry path、conformance case ID 或 profile 声明，否则用 frontmatter 和 Product Domain Status 表达即可，不要在每篇草案重复固定表。

同理，不要单独写“采纳状态”章节来重复 frontmatter。草案是否 generated、是否可实现、是否进入 registry review，应由 frontmatter、速读结论和产品状态页承担；正文只保留 feature-specific 的接口、字段、示例、legacy 映射和待确认问题。
