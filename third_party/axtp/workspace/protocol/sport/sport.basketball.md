---
status: generated
contract: true
generated: true
domain: sport
feature: sport.basketball
registry: contract/registry/domains/sport/domain.yaml
lastReviewed: 2026-09-07
---

# sport.basketball

本文是 `sport.eventDetection` 的篮球专项扩展草案。它只定义篮球项目的能力 descriptor、`shot` / `goal` 事件语义、专项 `details` schema、投篮与进球关联及测试边界；公共检测开关、运行状态、订阅、事件 envelope 和重连规则统一复用 [sport.eventDetection 通用草案](sport.eventDetection.md)。公共 RPC envelope、Session、错误规则、schema 展开和 flow example 写法遵循 [Protocol Draft Conventions](../draft-conventions.md)。本文不是 runtime 实现合同；正式事实仍以 `contract/registry/**`、Protocol IR、generated 和 conformance 为准。

> Adoption note (2026-08-07): 用户已确认并采纳篮球专项默认决策：shot 表示一次有效投篮尝试；goal 必须引用一个 shotId，且一次 shot 最多一个 goal；关闭时允许已上报 shot 的在途 goal 完成，disabled 后不再发送新事件；MVP 不做断线补发或历史查询。公共控制面和 wire event 统一复用 `sport.eventDetection`，篮球 details 在 wire 上使用 `SportEventDetails`，本文件的 Shot/Goal Details 是专项语义投影。

> Amendment note (2026-09-07): 篮球专项复用 `sport.eventDetection` 新增的 Goal/Shot 水印和事件视频片段窗口配置。App 通过 `macAddress` + `sportType=basketball` 定位目标 VM33PRO；两类配置均提供英文 `get` / `set` method，set 采用 status-only 成功响应。

## 0. 速读结论

| 项目 | 内容 |
|---|---|
| 这个能力做什么 | 声明 VM33PRO 的篮球投篮/进球事件语义，并为通用事件检测能力提供专项 payload。 |
| 当前状态 | generated；已按方案 C 正式采纳，并于 2026-09-07 完成配置扩展和 Stage 50 刷新。 |
| 本次决策 | Adopted：篮球作为 `sport.basketball` 专项 capability，公共控制面和统一事件 envelope 复用 `sport.eventDetection`。 |
| 是否可直接实现 | 是；Registry 源事实与 generated 合同已刷新，运行时应绑定包含本 amendment 的 spec commit/tag。 |
| 主要交互 | 复用 `sport.eventDetection` 的 RPC + EVENT；篮球配置使用公共的水印与事件视频片段窗口 method。 |
| 是否使用 STREAM | 否。投篮/进球是低频业务事件；原始视频、轨迹和训练报告不在本 feature。 |
| Registry readiness | adopted：`sport.basketball` 已登记为 `sport` domain 下的专项 capability。 |
| Conformance | needed |
| 主要未决问题 | MVP 无阻塞性未决项；未来专项字段或恢复能力须走 amendment。 |

## 1. 功能说明

VM33PRO 在篮球训练场景下检测一次投篮尝试，并在判定命中时产生进球结果。设备通过 `sport.eventDetection` 的公共控制面开启/关闭篮球检测，并通过统一的 `sport.eventDetected` Event 上报：

```text
sportType=basketball, eventType=shot -> SportBasketballShotDetails
sportType=basketball, eventType=goal -> SportBasketballGoalDetails
```

App 应先查询通用 capability，确认 `basketball` 支持 `shot` / `goal`，再调用通用 `sport.getEventDetectionConfig` 和 `sport.setEventDetectionConfig`，而不是调用篮球专属的重复开关方法。

## 2. 能力边界

| 类型 | 内容 |
|---|---|
| 包含 | `sport.basketball` 专项 capability descriptor。 |
| 包含 | `shot` 投篮尝试和 `goal` 进球结果的触发语义。 |
| 包含 | `eventId`、`sequence`、`shotId`、`goalId`、`occurredAt`、`confidence` 等专项字段。 |
| 包含 | goal 与 shot 的关联、乱序、重复和未知引用处理建议。 |
| 包含 | 通过 `sport.eventDetection` 复用能力查询、开关、状态变化和事件订阅。 |
| 不包含 | 篮球检测开关、公共运行状态和通用 event envelope 的重新定义。 |
| 不包含 | 未命中事件、比分、队伍、回合、比赛计时、训练报告和云端统计，除非后续单独采纳。 |
| 不包含 | 算法模型、判定阈值、摄像头安装、球场标定和硬件实现。 |
| 不包含 | 原始视频、视频预览、录像和连续媒体；若需要，另走 `video` / `camera` 相关能力。 |
| 不包含 | 旧 VM33 HTTP JSON `Seq/Class/Method/Param` 扩展。 |
| 数据面 | 只使用通用 RPC Event，不使用 STREAM。 |

## 3. Shared Methods / Events

本专项不新增独立 method 或独立 event name。所有公共交互均由 `sport.eventDetection` 定义：

| 公共事实 | 用于篮球的方式 |
|---|---|
| `sport.getEventDetectionCapabilities` | 查询 `sportType=basketball`、`supportedEvents=["shot","goal"]` 和 `detailsSchemas`。 |
| `sport.getEventDetectionConfig` | 使用 `params.sportType=basketball` 查询有效开关和运行态。 |
| `sport.setEventDetectionConfig` | 使用 `params.sportType=basketball` 设置 `enabled`。 |
| `sport.getGoalShotWatermarkConfig` / `sport.setGoalShotWatermarkConfig` | 使用 `params.macAddress` + `params.sportType=basketball` 查询/原子设置 Goal/Shot 水印显示。 |
| `sport.getEventClipConfig` / `sport.setEventClipConfig` | 使用 `params.macAddress` + `params.sportType=basketball` 查询/原子设置以 shot/goal 事件时间为中心的视频片段前后秒数。 |
| `sport.eventDetectionStateChanged` | `state.sportType=basketball` 时更新篮球开关和算法运行态。 |
| `sport.eventDetected` | 通过 `sportType=basketball` 和 `eventType=shot/goal` 路由到本专项 schema。 |

通用 method 的完整 Params/Result 表、`request` / `success` d block 和公共错误分支见 [sport.eventDetection 第 3 章](sport.eventDetection.md#3-方法-methods)。本文件不复制这些示例，避免形成两套可冲突的控制合同。设备应在 `sport.eventDetection` capability 的 `supportedSports[]` 中，通过 `supportedConfigMethods` 声明本篮球实现支持的配置 method。

篮球配置的具体规则：

- `macAddress` 必须使用大写、冒号分隔形式，例如 `AA:BB:CC:DD:EE:FF`；每个 App 请求都必须显式携带目标 MAC。
- `sportType` 固定为 `basketball`，但保留在请求中以复用跨运动项目配置 method。
- `sport.setGoalShotWatermarkConfig` 的 `goalVisible` 和 `shotVisible` 一次原子更新，成功响应不带业务 `result`。
- `sport.setEventClipConfig` 的 `beforeOffsetSeconds` 和 `afterOffsetSeconds` 一次原子更新；二者表示以 `shot` / `goal` 的 `occurredAt` 为中心的视频片段窗口。
- 目标设备不存在返回 `NOT_FOUND`；篮球能力或配置不支持返回 `NOT_SUPPORTED`；MAC 格式或 offset 非法分别返回 `INVALID_ARGUMENT` / `OUT_OF_RANGE`。

## 4. Basketball Events

### 4.0 事件速览

| Event envelope | `eventType` | 触发条件 | Details Schema | 状态 |
|---|---|---|---|---|
| `sport.eventDetected` | `shot` | 检测服务判定一次篮球投篮尝试。 | `SportBasketballShotDetails` | `[REVIEW-OK]` |
| `sport.eventDetected` | `goal` | 检测服务判定一次篮球进球结果。 | `SportBasketballGoalDetails` | `[REVIEW-OK]` |

公共事件 envelope：

```json
{
  "event": "sport.eventDetected",
  "intent": 1,
  "data": {
    "eventId": "evt-1024",
    "sequence": 1024,
    "sportType": "basketball",
    "eventType": "shot",
    "details": {
      "shotId": "shot-1024"
    }
  }
}
```

### 4.1 `sport.eventDetected` / `eventType=shot`

**触发条件**：篮球检测处于通用 state 的 `runtimeState=ready`，或产品确认允许的 `degraded` 状态时，算法判定一条投篮尝试。该事件只表达投篮，不表达是否命中。

#### 4.1.1 Details：`SportBasketballShotDetails`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `shotId` | string | yes | opaque non-empty | none | 该次投篮的业务关联 ID。 |
| `releaseType` | enum | no | `unknown`, `setShot`, `jumpShot`, `layup`, `other` | omitted | 可选出手类型。 |

#### 4.1.2 d block 示例

```json
{
  "event": "sport.eventDetected",
  "intent": 1,
  "data": {
    "eventId": "shot-event-1024",
    "sequence": 1024,
    "sportType": "basketball",
    "eventType": "shot",
    "occurredAt": "<device-time>",
    "confidence": 0.94,
    "details": {
      "shotId": "shot-1024",
      "releaseType": "jumpShot"
    }
  }
}
```

#### 4.1.3 客户端处理建议

- App 按通用 `eventId` / `sequence` 去重和排序，并记录 `shotId`。
- 收到 shot 不得自动计为进球；等待同一训练语义下的 goal 事件或产品定义的未命中结果。
- 事件 payload 不包含原始视频帧、图片或连续媒体数据。

#### 4.1.4 规则

- “投篮”表示一次有效投篮尝试；设备重试同一事件时必须保持通用 `eventId` 和专项 `shotId` 不变。
- 检测关闭、不可用或未订阅时不得发送 optional shot event。

### 4.2 `sport.eventDetected` / `eventType=goal`

**触发条件**：篮球检测服务判定一次投篮命中。每个 goal 必须携带对应的 `shotId`；一次 shot 最多产生一个 goal。

#### 4.2.1 Details：`SportBasketballGoalDetails`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `goalId` | string | yes | opaque non-empty | none | 进球结果关联 ID。 |
| `shotId` | string | yes | known shot id | none | 必须引用已检测到的对应投篮 ID。 |
| `goalType` | enum | no | `fieldGoal`, `freeThrow`, `unknown` | omitted | 可选进球类型。 |

#### 4.2.2 d block 示例

```json
{
  "event": "sport.eventDetected",
  "intent": 1,
  "data": {
    "eventId": "goal-event-1025",
    "sequence": 1025,
    "sportType": "basketball",
    "eventType": "goal",
    "occurredAt": "<device-time>",
    "confidence": 0.99,
    "details": {
      "goalId": "goal-1025",
      "shotId": "shot-1024",
      "goalType": "fieldGoal"
    }
  }
}
```

#### 4.2.3 客户端处理建议

- App 按 `shotId` 将 goal 归属于对应投篮；未知引用不得静默绑定到最近一条投篮，应暂存、记录关联异常或触发校准。
- 重复事件必须通过通用 `eventId` 去重。

#### 4.2.4 规则

- goal event 不替代任何 set method 的同步 response。
- goal 不允许脱离 shot event；一个 shot 最多一个 goal。
- 关闭检测后，已经发出的 shot 对应的在途 goal 允许完成；进入 `disabled` 后不再发送新的 shot/goal 事件。

## 5. Capability

Capability name: `sport.basketball`（adopted specialization）。它描述篮球事件语义和专项 details schema，不重复声明通用控制 method；通用 capability `sport.eventDetection` 的 `supportedSports` 引用本专项。

| 能力字段 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `capability` | string | yes | 固定 `sport.basketball` | none | 篮球专项 capability 名称。 |
| `sportType` | string | yes | 固定 `basketball` | `basketball` | 与通用 event envelope discriminator 一致。 |
| 项目事件类型集合 | array<string> | yes | `shot`, `goal` | none | 本设备可上报的篮球事件类型；正式字段名和支持范围以 Registry capability descriptor 为准。 |
| `detailsSchemas` | array<string> | yes | `SportBasketballShotDetails`, `SportBasketballGoalDetails` | none | 事件类型到专项 payload 的映射。 |
| `requiresEventDetectionToggle` | bool | yes | `true` / `false` | none | 是否依赖通用 `sport.setEventDetectionConfig` 开关。 |
| `goalRequiresShotReference` | bool | yes | `true` | `true` | MVP 固定要求每个 goal 引用一个 `shotId`。 |

通用 capability descriptor 对 `sportType=basketball` 的推荐 `supportedConfigMethods` 值为：

```json
[
  "sport.getGoalShotWatermarkConfig",
  "sport.setGoalShotWatermarkConfig",
  "sport.getEventClipConfig",
  "sport.setEventClipConfig"
]
```

## 6. Schemas

### 6.0 Schema hierarchy

```text
SportBasketballCapability
  sportType / supportedEvents / detailsSchemas / goalRequiresShotReference

SportBasketballShotDetails
  shotId / releaseType

SportBasketballGoalDetails
  goalId / shotId / goalType
```

### 6.1 `SportBasketballShotDetails`

字段定义见第 4.1.1 节。`shotId` 是篮球专项关联 ID；通用事件 envelope 的 `eventId` 仍是传输去重权威字段。

### 6.2 `SportBasketballGoalDetails`

字段定义见第 4.2.1 节。`goalId` 是进球专项关联 ID；`shotId` 为必填，且每个 shot 最多关联一个 goal。

### 6.3 Shared schema references

| Shared schema | 用途 | 来源 |
|---|---|---|
| `SportEventDetectionState` | 篮球开关和运行态 | [sport.eventDetection 第 6.1 节](sport.eventDetection.md#61-runtime-state-schema-sporteventdetectionstate) |
| `SportEventDetectionStateChangedEvent` | 篮球状态变化事件 | [sport.eventDetection 第 4.1 节](sport.eventDetection.md#41-sporteventdetectionstatechanged) |
| `SportEventDetectedEvent` | 篮球 shot/goal 通用 envelope，wire details 使用 `SportEventDetails` | [sport.eventDetection 第 4.2 节](sport.eventDetection.md#42-sporteventdetected) |

专项 schema 的 fieldId 已在 Registry YAML 中分配；公共 method/event/capability IDs 和 domain bitOffset 也已写入同一源文件。后续语义变更必须先更新本 formal proposal，再使用 amend-adopted-protocol。

## 7. 交互流程示例 Flow Examples

### 7.1 Basketball capability -> common enable -> shot/goal

```text
1. App 调用 sport.getEventDetectionCapabilities。
2. App 确认 supportedSports 中存在 basketball，且 supportedEvents 包含 shot/goal。
3. App 调用 sport.getEventDetectionConfig(sportType=basketball)。
4. 用户打开开关，App 调用 sport.setEventDetectionConfig(sportType=basketball, enabled=true)。
5. App 等待 sport.eventDetectionStateChanged 收敛到 runtimeState=ready。
6. 设备用 `sport.eventDetected` + `SportEventDetails` wrapper 上报投篮；按 `(sportType=basketball,eventType=shot)` 解释为 `SportBasketballShotDetails`。
7. 设备用同一 wrapper 上报进球；按 `(sportType=basketball,eventType=goal)` 解释为 `SportBasketballGoalDetails`，且必须关联 `shotId`。
```

### 7.2 Goal 与 Shot 乱序/断线

```text
1. App 按通用 eventId/sequence 去重，不以到达顺序直接覆盖业务关联。
2. goal 引用已知 shot 时建立关联；引用未知 shot 时暂存、记录关联异常或触发校准。
3. WebSocket 重连后重新订阅并查询篮球 state。
4. MVP 不补发断线期间 shot/goal，也不提供历史查询；需要时另行采纳 recovery 语义。
```

## 8. Common Error Mapping

本专项全部复用 `sport.eventDetection` 的 common errors，不新增 feature-specific error 或 numeric errorCode。

| 场景 | 采用的 common error | 说明 |
|---|---|---|---|
| goal 无法引用有效 shot | `INVALID_STATE` 或 `UNAVAILABLE` | 不发送不符合合同的 goal；具体原因写入状态/错误 details。 |
| 篮球算法、摄像头或资源不可用 | `UNAVAILABLE` | 在 state `reason` 中表达专项原因。 |

通用 method 未注册、项目不支持、参数非法、状态冲突、忙、权限不足和服务不可用的映射以 [sport.eventDetection Common Error Mapping](sport.eventDetection.md#8-common-error-mapping) 为准。

## 9. Legacy Mapping

本次未采纳 legacy mapping。没有足够的 VM33 旧命令或事件证据形成可验证映射；后续兼容需求须补充具体版本化证据并单独走迁移或 amendment 流程。

## 10. Registry / Conformance Status

| Item | Current state |
|---|---|
| Draft status | Adopted：本文件记录已采纳的 `sport.basketball` formal proposal。 |
| Domain registry | Adopted：`sport` 使用 high-byte `0x18`，状态为 `draft`。 |
| Domain YAML | Adopted：`contract/registry/domains/sport/domain.yaml`。 |
| Generated protocol | Generated：已通过 Stage 50 从 Registry YAML 刷新。 |
| Protocol IR / MCP / test vectors | 已由 Generator 更新 Protocol IR、generated docs 和 MCP registry；测试向量无内容变化。 |
| Conformance | needed：capability descriptor、shot/goal details、关联/乱序/重复、通用 state event、reconnect、multi-device target、watermark 和 event clip atomic update cases。 |

专项采纳前置条件：无。用户已确认方案 C 及全部推荐决策；后续已采纳语义变更须使用 `amend-adopted-protocol`。

## 11. Test Notes

| Case | Given | When | Then |
|---|---|---|---|
| basketball capability | 通用 capability 声明 basketball | App 查询 capability | 返回 shot/goal 和专项 details schema。 |
| enable | basketball 可用 | App 用通用 set method 开启 | 返回 `sportType=basketball` 的 state；状态变化用公共 state event 通知。 |
| shot event | state ready 且已订阅 | 算法判定投篮 | 发送 `sport.eventDetected`，eventType=shot，details 含唯一 shotId。 |
| goal event | 已发送且可关联的 shot 存在 | 算法判定进球 | 发送 eventType=goal，details 必须携带对应 shotId；一个 shot 最多一个 goal。 |
| unknown correlation | goal 引用未知 shot | App 收到 goal | 不静默绑定最近 shot；暂存、校准或展示未关联结果。 |
| duplicate / out-of-order | 重试或事件并发 | App 收到重复/乱序 | 通用 eventId/sequence 去重排序，专项关联稳定。 |
| no STREAM | AXTP-WS-JSON | 设备产生篮球事件 | 只发送 RPC Event，不发送 STREAM 或原始视频。 |
| multi-device config | 一个 App 管理多个 VM33PRO | App 携带不同 `macAddress` 调用配置 method | 设备只处理指定 MAC 的篮球配置，不使用隐式当前设备。 |
| watermark visibility | `sportType=basketball` | App 设置 Goal/Shot 水印 | set 只返回标准 status；后续 get 返回完整 `goalVisible` / `shotVisible`。 |
| event clip window | 检测到 shot/goal | App 设置前后偏移 | 片段窗口以事件 `occurredAt` 为中心，两个 offset 原子更新。 |

## 12. Adoption Decisions / Closed Questions

| 问题 | 影响 | 当前建议 | 状态 |
|---|---|---|---|
| “投篮事件”的定义 | shot trigger / algorithm / App UI | 一次有效投篮尝试。 | closed |
| 进球是否必须引用一条投篮？ | goal schema / statistics | 必须；`shotId` required。 | closed |
| 每次投篮最多几个进球？ | correlation / statistics | 一个 shot 最多一个 goal。 | closed |
| 是否采用统一 event？ | event registry / subscription | 采用 `sport.eventDetected`，`sportType` + `eventType` 路由专项 details。 | closed |
| 公共 envelope 的可选字段 | payload size / analytics | `eventId`、`sequence`、`occurredAt` required；`confidence`、`trainingSessionId` optional。 | closed |
| 关闭检测后的在途事件 | in-flight policy / UX | 已发出的 shot 对应的在途 goal 允许完成；`disabled` 后不再发送新事件。 | closed |
| 断线期间是否补发或历史查询？ | reliability / storage | MVP 不提供断线补发、历史查询或 exactly-once。 | closed |

## 附录 A. Adoption Record

- 用户已确认方案 C 及全部推荐决策；本稿无未解决审核阻塞、修复或询问项。
- method/event/capability/schema 的机器事实以 `contract/registry/domains/sport/domain.yaml` 为唯一来源。
- 后续已采纳语义变更必须先更新本 formal proposal，再使用 `amend-adopted-protocol`。

## 附录 C. Amendment History

| 日期 | 变更 | 兼容性 |
|---|---|---|
| 2026-09-07 | 复用 `sport.eventDetection` 新增 `sport.getGoalShotWatermarkConfig` / `sport.setGoalShotWatermarkConfig` 和 `sport.getEventClipConfig` / `sport.setEventClipConfig`；请求包含 `macAddress` 与 `sportType`。 | 向后兼容的 draft method/schema addition；既有篮球事件合同不变。 |
| 2026-09-07 | 通过通用 capability descriptor 的可选 `supportedConfigMethods` 声明篮球设备支持的 4 个配置 method。 | 向后兼容的 optional capability metadata addition；既有篮球事件字段和 method ID 不变。 |

## 附录 B. Registry 草案输入

以下是已确认的 Stage 30 adoption 输入摘要；正式机器事实见 `contract/registry/domains/sport/domain.yaml`：

| Fact type | Adopted fact | ID / status |
|---|---|---|
| Capability | `sport.basketball` | `0x1802`; `draft` |
| Event types | `shot`, `goal` | 由 `sport.eventDetection` 统一 event 绑定；goal 必须引用 shotId。 |
| Schemas | `SportBasketballShotDetails`, `SportBasketballGoalDetails` | field IDs `0x01` onward in accepted order; wire wrapper is `SportEventDetails` |
| Errors | Common `NOT_SUPPORTED`, `INVALID_ARGUMENT`, `INVALID_STATE`, `UNAVAILABLE` and shared control errors | no feature-specific error allocated |
