# 协议草案模板

创建或重写 `workspace/protocol/<domain>/<domain.feature>.md` 时，使用这份轻量中文模板。

草案目标是让评审者快速看懂协议意图、接口形状、字段含义、测试重点和采纳风险。它不是最终机器事实源。正式事实源仍然是 `contract/registry/**/*.yaml`、`contract/protocol/axtp.protocol.yaml`、`contract/generated/**` 和 `conformance/**`。

公共 RPC envelope、成功/失败响应读法、错误约定、schema 展开和 flow example 写法维护在 `workspace/protocol/draft-conventions.md`。每个 method 保留一个最小可读的 `d block 示例`，用 `request:` / `success:` 展示可调用形状；复杂错误、事件、STREAM、异步状态机、权限分支和 legacy 字段转换再补 feature-specific 示例。

````markdown
---
status: draft
contract: false
generated: false
domain: <domain>
feature: <domain.feature>
registry:
lastReviewed: YYYY-MM-DD
---

# <domain.feature>

## 0. 速读结论

| 项目 | 内容 |
|---|---|
| 这个能力做什么 | <一句话说明> |
| 当前状态 | draft / review-ok / generated / deprecated |
| 是否可直接实现 | 否。draft/review-ok 阶段仅供评审；正式实现以 registry / generated 为准。 |
| 主要交互 | RPC / RPC + EVENT / RPC + STREAM |
| 是否使用 STREAM | 是 / 否 |
| Registry readiness | none / partial / candidate / ready |
| Conformance | none / needed / ready |
| 主要未决问题 | <一句话列出，若无则写“暂无”> |

## 1. 功能说明

用 3-5 句话说明这个 feature 解决什么问题、面向哪些调用方、适用于哪些设备或场景。公共 envelope、错误、schema 展开和 flow 写法遵循 [Protocol Draft Conventions](../draft-conventions.md)。

## 2. 能力边界

| 类型 | 内容 |
|---|---|
| 包含 | <能力 1> |
| 包含 | <能力 2> |
| 不包含 | <不属于本文的能力；说明应归属哪个 feature> |
| 数据面 | 本 feature 是否定义或绑定 STREAM；如果没有，写“所有操作均通过 RPC method/event 完成”。 |

## 3. 方法 Methods

### 3.0 方法速览

| Method | 调用类型 | 用途 | Params Schema | Result Schema | 是否触发事件 | 状态 |
|---|---|---|---|---|---|---|
| `<domain.feature>.get` | query | 查询当前状态或配置 | `GetXxxParams` | `XxxState` | 否 | draft / review-ok |
| `<domain.feature>.set` | command | 设置目标状态或配置 | `SetXxxParams` | `XxxState` | 是，`<domain.feature>.changed` | draft / review-ok |

### 3.1 `<domain.feature>.<method>`

**用途**：<说明 method 做什么。>

| 项 | 内容 |
|---|---|
| 调用类型 | query / command / action |
| Params Schema | `<ParamsSchema>` |
| Result Schema | `<ResultSchema>` |
| 是否触发事件 | 否 / 是，说明事件名和触发条件 |
| 幂等性 / 异步性 | <同步/异步、是否幂等、是否 accepted 后由事件收敛> |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_ARGUMENT`, `PERMISSION_DENIED`, `BUSY` |

#### 3.1.1 请求参数 Params：`<ParamsSchema>`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `<field>` | `<type>` | yes/no | <range or enum> | <default or omitted> | <说明> |

#### 3.1.2 返回结果 Result：`<ResultSchema>`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `<field>` | `<type>` | yes/no | <range or enum> | <default or omitted> | <说明> |

#### 3.1.3 d block 示例

request:

```json
{
  "id": 101,
  "method": "<domain.feature>.<method>",
  "params": {
    "<field>": "<value>"
  }
}
```

success:

```json
{
  "id": 101,
  "status": {
    "ok": true,
    "code": 0
  },
  "result": {
    "<field>": "<value>"
  }
}
```

示例只展示 RPC `d` block。公共 `op`、`sid`、成功/失败 envelope 和 `id` 回显规则见 [Protocol Draft Conventions](../draft-conventions.md)。如果 method 无 `params` 或无业务 `result`，可以省略对应对象；不要拆成独立 Request / Success Response 标题。

#### 3.1.4 可能触发的事件

| Event | 触发条件 | Payload Schema | 客户端处理建议 |
|---|---|---|---|
| `<domain.feature>.changed` | <状态实际变化时> | `<ChangedEvent>` | <直接更新 UI / 调用 get 校准 / 可忽略> |

#### 3.1.5 错误

| 错误 | 场景 | 返回建议 |
|---|---|---|
| `NOT_SUPPORTED` | 设备不支持 feature、method、target 或 scope。 | 返回 unsupported detail。 |
| `INVALID_ARGUMENT` | 请求字段非法。 | 返回字段路径和合法范围。 |
| `BUSY` | 设备正在处理冲突操作。 | 稍后重试或等待状态事件。 |

#### 3.1.6 规则

- <补充本 method 的 feature-specific 协议约束。>
- query method SHOULD NOT 因查询本身触发状态变化事件。
- command/action 成功后是否触发事件，必须在本 method 中明确。

## 4. 事件 Events

### 4.0 事件速览

| Event | 触发条件 | Payload Schema | 客户端处理建议 | 状态 |
|---|---|---|---|---|
| `<domain.feature>.changed` | <触发条件> | `<ChangedEvent>` | <直接更新 UI / 调用 get 校准 / 可忽略> | draft / review-ok |

### 4.1 `<domain.feature>.changed`

**触发条件**：

- <RPC 设置导致状态或配置实际变化。>
- <本地按键、设备策略、profile、restore、factory reset 或自动算法导致状态变化。>

#### 4.1.1 Payload：`<ChangedEvent>`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `<field>` | `<type>` | yes/no | <range or enum> | <default or omitted> | <说明> |
| `source` | string enum | no | feature-specific | `unknown` | 状态变化来源。 |
| `reason` | string enum | no | feature-specific | `unknown` | 状态变化原因。 |

#### 4.1.2 d block 示例

```json
{
  "event": "<domain.feature>.changed",
  "intent": 1,
  "data": {
    "<field>": "<value>",
    "source": "remoteApp",
    "reason": "user_request"
  }
}
```

事件 payload 不直观、包含嵌套对象、字段转换、状态机分支或客户端缓存语义时，应保留 feature-specific event `d` block 示例。简单 changed event 可以只保留 payload 字段表和客户端处理建议。

#### 4.1.3 客户端处理建议

| 场景 | 建议 |
|---|---|
| payload 是完整状态 | 可直接更新 UI 或本地缓存。 |
| payload 是变化片段 | 调用对应 get method 校准完整状态。 |
| event 丢失或重连 | 重连后主动调用 get method 校准。 |

#### 4.1.4 规则

- Event payload MUST 放在 `d.data` 中。
- Event MUST NOT 携带 request `id`。
- 如果 event payload 是部分变化，文档必须明确客户端是否需要调用 get method 校准。

## 5. Capability

Capability name: `<domain.feature>`。

设备通过 capability 声明是否支持该 feature，以及支持哪些范围、模式、对象或约束。Capability 字段只描述“设备能做什么”，不得混入 method params/result 或 event payload。

| 能力字段 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `capability` | string | yes | fixed `<domain.feature>` | none | capability 名称。 |
| `<field>` | `<type>` | yes/no | <range or enum> | <default or omitted> | <说明> |

## 6. 字段 / Schemas

### 6.1 Schema 层级速览

说明本 feature 有哪些核心数据对象，以及它们分别用于 method params、method result、event payload 还是 capability。

Schema 展开模式必须二选一：

- 简单 feature：method/event 章节已经直接展开字段表和最小 `d block 示例`，本章只保留 schema 索引，避免重复。
- 复杂 feature：method/event 章节保留关键字段；本章集中展开复杂对象；必要时在 method/event `d block 示例` 中展示关键 payload。

### 6.2 请求与响应 Schemas

#### `<ParamsSchema>`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `<field>` | `<type>` | yes/no | <range or enum> | <default or omitted> | <说明> |

#### `<ResultSchema>`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `<field>` | `<type>` | yes/no | <range or enum> | <default or omitted> | <说明> |

### 6.3 Capability Schemas

如 capability 字段较复杂，在这里展开能力对象。

### 6.4 Event Schemas

如 event payload 字段较复杂，在这里展开事件对象。

### 6.5 State / Config / Object Schemas

如存在状态对象、配置对象、数组元素对象，在这里展开。对象字段较多时，每个对象单独成表。

## 7. 交互流程示例 Flow Examples

只在存在真实端到端顺序时保留本章，例如 capability discovery -> set method -> changed event、action accepted -> progress event、failure request -> no event、STREAM open -> STREAM data -> close、reconnect -> get state calibration。

单个 method/event 的普通用法不要写成本章；放在对应 method/event 的字段表、规则和 `d block 示例` 中。

## 8. 错误

通用错误读法见 `workspace/protocol/draft-conventions.md`。本章只保留会影响 feature 语义的候选错误、特殊触发条件、状态后果或 legacy 兼容说明。

| 错误 | 适用场景 | 说明 |
|---|---|---|
| `<NamedCandidateError>` | 候选业务错误。 | 只有存在真实业务语义时保留；采纳前确认是否需要 feature-specific ErrorCode。 |

## 9. Legacy 映射

Legacy 映射是迁移证据，不是 runtime 合同。如果没有 legacy 映射，写“暂无”。

| legacy 项 | 候选映射 | 状态 | 说明 |
|---|---|---|---|
| `<legacy command / field>` | `<domain.method>` / `<event>` / adapter-only | `[REVIEW-ASK]` / `[REVIEW-OK]` / `[REVIEW-DRAFT]` | <说明> |

## 10. 测试重点与采纳风险

不要在每篇草案里复制固定 Registry / Conformance 状态表，也不要单独复述 frontmatter 里的草案状态。草案状态以 frontmatter、Product Domain Status 和 registry/generated 事实为准。本章只写 feature-specific 采纳风险和测试重点。

| 类型 | 要点 |
|---|---|
| registry readiness | <none / partial / candidate / ready，以及具体阻塞项> |
| conformance focus | <需要覆盖的 feature-specific 行为> |
| happy path | <正常查询/设置/动作> |
| event path | <事件触发和客户端处理> |
| boundary case | <边界值、默认值、省略字段、非法枚举> |
| error case | <权限、busy、unsupported、invalid argument> |
| compatibility | <旧字段/旧命令迁移、可选字段兼容> |

## 11. 待确认问题

| 问题 | 影响 | 当前建议 | 状态 |
|---|---|---|---|
| <问题> | schema / registry / conformance / legacy / product behavior | <当前建议> | open / decided / blocked |

## 可选附录：复杂 / 高风险 feature 增强

reset / factory restore、firmware.ota、security/auth、network.config、storage.format、lifecycle/reboot/shutdown，或任何会导致断连、数据清除、权限变化、软件版本变化、设备身份变化的能力，可以追加以下附录。

### 附录 A. 协议审核标记

| 标记 | 条目 | 审核结论 | 后续动作 |
|---|---|---|---|
| `[REVIEW-OK]` | <item> | <已确认结论> | <后续动作> |
| `[REVIEW-DRAFT]` | <item> | <草案结论> | <后续动作> |
| `[REVIEW-ASK]` | <item> | <待确认问题> | <需要谁确认> |

### 附录 B. 协议决策记录

| 决策点 | 结论 | 理由 |
|---|---|---|
| <decision> | <result> | <reason> |

### 附录 C. Registry 草案输入

如果该 feature 已经接近 registry review，可以给出 YAML 候选片段。不得分配正式 methodId / eventId / errorCode / fieldId；ID 使用 `TBD after adoption`。

### 附录 D. 采纳检查清单

- [ ] domain.feature 边界已确认。
- [ ] methods/events/schemas/capability 已确认。
- [ ] methodId/eventId/fieldId/errorCode 将由 registry 采纳时分配。
- [ ] legacy 映射已人工确认。
- [ ] conformance cases 已规划。
````

## 示例片段：method 示例减肥后的写法

下面展示每个 method 保留一个紧凑 `d block 示例` 的写法，不表示正式 registry 命名或 ID。复杂错误和事件示例不要塞进 method；除非它们直接影响 method 评审，否则放到错误或事件章节。

````markdown
#### 3.2.3 d block 示例

request:

```json
{
  "id": 101,
  "method": "audio.volume.set",
  "params": {
    "target": "master",
    "level": 42,
    "muted": false
  }
}
```

success:

```json
{
  "id": 101,
  "status": {
    "ok": true,
    "code": 0
  },
  "result": {
    "target": "master",
    "level": 42,
    "muted": false
  }
}
```

读法：`result` 返回设置后的完整状态，而不是变化片段；状态实际变化时由 `audio.volume.changed` 事件通知。

#### 4.1.2 d block 示例

```json
{
  "event": "audio.volume.changed",
  "intent": 1,
  "data": {
    "changedFields": [
      "level"
    ],
    "state": {
      "target": "master",
      "level": 42,
      "muted": false
    },
    "reason": "user_request"
  }
}
```
````
