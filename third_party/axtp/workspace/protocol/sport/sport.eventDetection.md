---
status: generated
contract: true
generated: true
domain: sport
feature: sport.eventDetection
registry: contract/registry/domains/sport/domain.yaml
lastReviewed: 2026-09-07
---

# sport.eventDetection

本文是跨运动项目的设备事件检测通用协议草案，供产品、算法、固件、App、测试和协议维护者评审。它为篮球、足球、棒球、网球等专项 feature 提供统一的能力发现、检测开关、运行状态、事件订阅和事件可靠性语义。公共 RPC envelope、Session、错误规则、schema 展开和 flow example 写法遵循 [Protocol Draft Conventions](../draft-conventions.md)。本文不是 runtime 实现合同；正式事实仍以 `contract/registry/**`、Protocol IR、generated 和 conformance 为准。

> Adoption note (2026-08-07): 用户已确认并采纳方案 C。正式采用 `sport` domain、`sport.eventDetection` 公共控制面和 `sport.basketball` 专项 capability；MVP 单设备一次只启用一个 `sportType`，检测开关持久化，goal 必须引用 shot，关闭时允许已上报 shot 的在途 goal 完成，disabled 后不再发送新事件，MVP 不提供断线补发/历史查询。Wire event 的 `details` 采用已注册的 `SportEventDetails` discriminator-qualified wrapper；专项 variant schema 作为篮球语义投影，未来新运动通过可选字段或专项 event schema amendment 扩展。

> Amendment note (2026-09-07): 为一个 App 管理多台设备的场景新增按 `macAddress` + `sportType` 定位的 Goal/Shot 水印配置和事件视频片段窗口配置。两类配置均提供英文命名的 `get` / `set` method；`set` 原子更新完整配置，并只通过标准 RPC `status` 表达成功或失败。

## 0. 速读结论

| 项目 | 内容 |
|---|---|
| 这个能力做什么 | 为多个运动项目提供统一的事件检测开关、状态同步、能力发现、Goal/Shot 水印与事件视频片段窗口配置，以及设备到 App 的业务事件 envelope。 |
| 当前状态 | generated；已按方案 C 正式采纳，并于 2026-09-07 完成配置扩展和 Stage 50 刷新。 |
| 本次决策 | Adopted：将公共控制面和事件 envelope 固化为 `sport.eventDetection`，篮球语义由 `sport.basketball` 承载。 |
| 是否可直接实现 | 是；Registry 源事实与 generated 合同已刷新，运行时应绑定包含本 amendment 的 spec commit/tag。 |
| 主要交互 | RPC + EVENT |
| 是否使用 STREAM | 否。投篮、进球等是低频业务事件；原始视频、轨迹或连续遥测另归各自数据域。 |
| Registry readiness | adopted：`sport` domain、domain YAML、公共 capability/method/event 和篮球专项 capability 已登记。 |
| Conformance | needed |
| 主要未决问题 | MVP 无阻塞性未决项；未来运动项目或恢复能力的语义变更须先 amendment。 |

## 1. 功能说明

`sport.eventDetection` 描述设备对一个或多个运动项目进行语义事件检测的公共协议表面。设备是 AXTP Logical Server，App / SDK 是 Logical Client。App 先查询设备支持的 `sportType` 和 `eventType`，再按项目设置检测开关；设备通过统一状态事件和 `sport.eventDetected` 上报实际检测结果。一个 App 管理多台设备时，水印和事件视频片段窗口配置使用 `macAddress` 与 `sportType` 共同定位目标配置。

篮球的 `shot`、`goal` 及其 `shotId` 关联由 [sport.basketball 专项草案](sport.basketball.md) 定义；未来专项草案应复用本稿的控制、订阅、顺序、去重和重连边界，不重复定义公共 envelope。

## 2. 能力边界

| 类型 | 内容 |
|---|---|
| 包含 | 运动项目和事件类型能力发现。 |
| 包含 | 按 `sportType` 查询、开启/关闭检测及返回有效运行状态。 |
| 包含 | 按 `macAddress` + `sportType` 查询或原子设置 Goal/Shot 水印是否显示。 |
| 包含 | 按 `macAddress` + `sportType` 查询或原子设置以 Goal/Shot 事件为中心的视频片段前后时间窗口。 |
| 包含 | 统一的状态变化事件和检测结果事件 envelope。 |
| 包含 | 事件唯一标识、顺序、时间、置信度和专项 payload 引用语义。 |
| 包含 | 多运动项目能力声明；是否支持同时运行由 capability 明确表达。 |
| 不包含 | 篮球、足球、棒球、网球的专项事件字段；这些进入 `sport.<discipline>` 专项 feature。 |
| 不包含 | 算法模型、训练数据、判定阈值、摄像头安装、球场标定和硬件实现。 |
| 不包含 | 原始视频、视频预览、录像生成/传输、连续媒体和原始 sensor telemetry；本 feature 只配置事件视频片段时间窗口，媒体数据仍归 `video` / `camera` / `sensor` 等域。 |
| 不包含 | 比赛计分、队伍、赛程、训练报告、云端持久化和多端同步；这些需另行评估 `sport.match`、`sport.session` 或产品业务域。 |
| 不包含 | 旧 VM33 HTTP JSON `Seq/Class/Method/Param` 扩展；新业务按 AXTP 演进。 |
| 数据面 | 不定义 STREAM；所有操作和通知均通过 RPC method/event 完成。 |

## 3. 方法 Methods

### 3.0 方法速览

| Method | 调用类型 | 用途 | Params Schema | Result Schema | 是否触发事件 | 状态 |
|---|---|---|---|---|---|---|
| `sport.getEventDetectionCapabilities` | query | 查询支持的运动项目、事件类型和运行约束。 | `GetEventDetectionCapabilitiesParams` | `GetEventDetectionCapabilitiesResult` | 否 | `[REVIEW-OK]` |
| `sport.getEventDetectionConfig` | query | 查询一个运动项目当前有效的检测配置和运行态。 | `GetEventDetectionConfigParams` | `SportEventDetectionState` | 否 | `[REVIEW-OK]` |
| `sport.setEventDetectionConfig` | command | 设置一个运动项目的检测开关。 | `SetEventDetectionConfigParams` | `SetEventDetectionConfigResult` | 是，实际状态变化后触发 `sport.eventDetectionStateChanged` | `[REVIEW-OK]` |
| `sport.getGoalShotWatermarkConfig` | query | 查询某台设备、某运动项目的 Goal/Shot 水印显示配置。 | `SportConfigTargetParams` | `SportGoalShotWatermarkConfig` | 否 | `[REVIEW-OK]` |
| `sport.setGoalShotWatermarkConfig` | command | 原子设置某台设备、某运动项目的 Goal/Shot 水印显示配置。 | `SportSetGoalShotWatermarkConfigParams` | `Empty` | 否 | `[REVIEW-OK]` |
| `sport.getEventClipConfig` | query | 查询某台设备、某运动项目的事件视频片段前后窗口。 | `SportConfigTargetParams` | `SportEventClipConfig` | 否 | `[REVIEW-OK]` |
| `sport.setEventClipConfig` | command | 原子设置某台设备、某运动项目的事件视频片段前后窗口。 | `SportSetEventClipConfigParams` | `Empty` | 否 | `[REVIEW-OK]` |

### 3.1 `sport.getEventDetectionCapabilities`

**用途**：返回设备支持的运动项目、每个项目支持的事件类型、是否可开关及多项目运行约束。

| 项 | 内容 |
|---|---|
| 调用类型 | query |
| Params Schema | `GetEventDetectionCapabilitiesParams` |
| Result Schema | `GetEventDetectionCapabilitiesResult` |
| 是否触发事件 | 否；查询不得改变检测状态。 |
| 幂等性 / 异步性 | 幂等；同步返回 capability snapshot。 |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_ARGUMENT`, `PERMISSION_DENIED`, `UNAVAILABLE` |

#### 3.1.1 请求参数 Params：`GetEventDetectionCapabilitiesParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `sportTypes` | array<string> | no | `basketball`, `football`, `baseball`, `tennis` 或已注册项目 | omitted | 只返回指定项目；省略表示返回设备全部支持的项目。 |
| `includeRuntimeState` | bool | no | `true` / `false` | `false` | 是否附带每个项目的当前运行态摘要；详细状态仍以 config query 为准。[REVIEW-OK] |

#### 3.1.2 返回结果 Result：`GetEventDetectionCapabilitiesResult`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `capability` | `SportEventDetectionCapabilities` | yes | see Section 5 | none | 通用能力描述对象。 |

#### 3.1.3 d block 示例

request:

```json
{
  "id": 201,
  "method": "sport.getEventDetectionCapabilities",
  "params": {
    "sportTypes": ["basketball", "football"]
  }
}
```

success:

```json
{
  "id": 201,
  "status": {
    "ok": true,
    "code": 0
  },
  "result": {
    "capability": {
      "capability": "sport.eventDetection",
      "supportedSports": [
        {
          "sportType": "basketball",
          "supportedEvents": ["shot", "goal"],
          "supportsToggle": true,
          "supportedConfigMethods": [
            "sport.getGoalShotWatermarkConfig",
            "sport.setGoalShotWatermarkConfig",
            "sport.getEventClipConfig",
            "sport.setEventClipConfig"
          ]
        },
        {
          "sportType": "football",
          "supportedEvents": ["kick", "goal"],
          "supportsToggle": false
        }
      ],
      "concurrentSportTypes": false
    }
  }
}
```

#### 3.1.4 可能触发的事件

| Event | 触发条件 | Payload Schema | 客户端处理建议 |
|---|---|---|---|
| 无 | capability query 不应产生状态或业务结果事件。 | none | 无需处理。 |

#### 3.1.5 错误

| 错误 | 场景 | 返回建议 |
|---|---|---|
| `INVALID_ARGUMENT` | `sportTypes` 含空值、重复值或格式非法。 | 返回字段路径和合法项目格式。 |
| `UNAVAILABLE` | 能力服务暂时不可查询。 | App 保持 unknown 状态并允许重试。 |

#### 3.1.6 规则

- `supportedEvents` 是项目内事件类型名，不是全局 event name；专项 feature 负责定义其 payload。
- `concurrentSportTypes=false` 表示同一时刻最多有一个项目处于有效检测态；实际约束必须以设备 capability 为准。[REVIEW-OK]
- capability discovery 不替代 config query；App 不得从 capability 推断当前 enabled 状态。

### 3.2 `sport.getEventDetectionConfig`

**用途**：查询一个运动项目当前有效的检测开关和运行态。App 在设置页、重连、设备重启或 event 丢失后使用该 method 校准本地状态。

| 项 | 内容 |
|---|---|
| 调用类型 | query |
| Params Schema | `GetEventDetectionConfigParams` |
| Result Schema | `SportEventDetectionState` |
| 是否触发事件 | 否；query 本身不触发 state changed event。 |
| 幂等性 / 异步性 | 幂等；同步返回 snapshot。 |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_ARGUMENT`, `PERMISSION_DENIED`, `UNAVAILABLE` |

#### 3.2.1 请求参数 Params：`GetEventDetectionConfigParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `sportType` | string | yes | 已由 capability 声明的项目 | none | 查询目标项目。 |

#### 3.2.2 返回结果 Result：`SportEventDetectionState`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `sportType` | string | yes | requested project | none | 状态所属项目。 |
| `effectiveEnabled` | bool | yes | `true` / `false` | none | 设备当前是否实际开启该项目检测。 |
| `runtimeState` | enum | yes | `disabled`, `enabling`, `ready`, `degraded`, `unavailable`, `disabling` | none | 当前算法运行态。 |
| `applyState` | enum | no | `applied`, `pending` | omitted | 最近一次开关操作的生效状态。 |
| `reason` | string | no | feature-specific reason | omitted | 状态变化或不可用原因。 |
| `stateRevision` | uint32 | no | monotonic revision | omitted | 状态版本，用于多端校准和 compare-and-set。 |
| `updatedAt` | string | no | device timestamp | omitted | snapshot 更新时间；不能替代事件顺序依据。 |

#### 3.2.3 d block 示例

request:

```json
{
  "id": 202,
  "method": "sport.getEventDetectionConfig",
  "params": {
    "sportType": "basketball"
  }
}
```

success:

```json
{
  "id": 202,
  "status": {
    "ok": true,
    "code": 0
  },
  "result": {
    "sportType": "basketball",
    "effectiveEnabled": true,
    "runtimeState": "ready",
    "applyState": "applied",
    "stateRevision": 7,
    "updatedAt": "<device-time>"
  }
}
```

#### 3.2.4 可能触发的事件

| Event | 触发条件 | Payload Schema | 客户端处理建议 |
|---|---|---|---|
| 无 | query 不应产生 state changed event。 | none | 需要实时变化时依赖已订阅的 `sport.eventDetectionStateChanged`。 |

#### 3.2.5 错误

| 错误 | 场景 | 返回建议 |
|---|---|---|
| `NOT_SUPPORTED` | 设备未实现该 `sportType` 的检测能力。 | App 隐藏或禁用该项目入口。 |
| `INVALID_ARGUMENT` | `sportType` 未声明或格式非法。 | 返回合法项目列表或字段路径。 |
| `UNAVAILABLE` | 状态服务暂时不可用。 | App 展示 unknown/unavailable 并允许重试。 |

#### 3.2.6 规则

- 重连后以设备返回的 `effectiveEnabled` 为准，不得仅恢复 App 本地 toggle 缓存。
- `runtimeState=unavailable` 不自动等同于用户已关闭 `effectiveEnabled`。
- event 丢失或 revision gap 时，App SHOULD 重新调用该 method 校准。

### 3.3 `sport.setEventDetectionConfig`

**用途**：设置一个运动项目的检测开关。设备校验权限、资源、隐私和算法前置条件后返回 accepted/effective state；若启停异步完成，再通过 `sport.eventDetectionStateChanged` 收敛。

| 项 | 内容 |
|---|---|
| 调用类型 | command |
| Params Schema | `SetEventDetectionConfigParams` |
| Result Schema | `SetEventDetectionConfigResult` |
| 是否触发事件 | 是；仅在有效状态实际变化后触发 `sport.eventDetectionStateChanged`。失败响应不得触发状态变化事件。 |
| 幂等性 / 异步性 | SHOULD 幂等；相同目标状态重复提交应返回当前有效状态。是否允许 pending 由固件状态机确认。 |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_ARGUMENT`, `INVALID_STATE`, `BUSY`, `PERMISSION_DENIED`, `UNAVAILABLE` |

#### 3.3.1 请求参数 Params：`SetEventDetectionConfigParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `sportType` | string | yes | 已由 capability 声明的项目 | none | 要控制的运动项目。 |
| `enabled` | bool | yes | `true` / `false` | none | 用户期望的检测开关状态。 |
| `expectedStateRevision` | uint32 | no | 当前 state revision | omitted | 可选 compare-and-set，避免多个 App 覆盖彼此状态。[REVIEW-OK] |

#### 3.3.2 返回结果 Result：`SetEventDetectionConfigResult`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `accepted` | bool | yes | `true` / `false` | none | 设备是否接受该控制请求。 |
| `state` | `SportEventDetectionState` | yes | see Section 6.1 | none | 请求处理后的有效或暂定状态。 |

#### 3.3.3 d block 示例

request:

```json
{
  "id": 203,
  "method": "sport.setEventDetectionConfig",
  "params": {
    "sportType": "basketball",
    "enabled": true
  }
}
```

success:

```json
{
  "id": 203,
  "status": {
    "ok": true,
    "code": 0
  },
  "result": {
    "accepted": true,
    "state": {
      "sportType": "basketball",
      "effectiveEnabled": true,
      "runtimeState": "enabling",
      "applyState": "pending",
      "stateRevision": 8
    }
  }
}
```

#### 3.3.4 可能触发的事件

| Event | 触发条件 | Payload Schema | 客户端处理建议 |
|---|---|---|---|
| `sport.eventDetectionStateChanged` | 检测有效状态实际变化，或异步启停从 pending 收敛到 ready/unavailable。 | `SportEventDetectionStateChangedEvent` | 更新对应运动项目 toggle 和运行态；必要时调用 get method 校准。 |

#### 3.3.5 错误

| 错误 | 场景 | 返回建议 |
|---|---|---|
| `NOT_SUPPORTED` | 设备未实现该运动项目或当前 profile 不可用。 | 只降级当前 operation，session 保持可用。 |
| `INVALID_STATE` | 摄像头、隐私、训练模式或其他运行态不允许切换。 | 返回状态原因；不触发 changed event。 |
| `BUSY` | 检测算法正在启动/停止，或设备不允许并行切换。 | App 保持 pending 或稍后重试。 |
| `UNAVAILABLE` | 算法或必要设备资源不可用。 | 复用 common error；可在 details 中表达专项原因。 |

示例性的 unavailable response（numeric code 15 复用现有 `UNAVAILABLE`）：

```json
{
  "id": 203,
  "status": {
    "ok": false,
    "code": 15,
    "details": {
      "reason": "SPORT_EVENT_DETECTOR_UNAVAILABLE",
      "sportType": "basketball",
      "runtimeState": "unavailable"
    }
  }
}
```

#### 3.3.6 规则

- `accepted=true` 只表示设备接受请求；App 最终应以返回的 `state` 或后续 state event 为准。
- `accepted=false` 或 RPC failure 不得携带业务 `result`，也不得发送 state changed event。
- `concurrentSportTypes=false` 时，开启一个项目可能需要先关闭另一个项目；设备应返回 `BUSY` 或 `INVALID_STATE`，不得静默切换。[REVIEW-OK]

### 3.4 `sport.getGoalShotWatermarkConfig`

**用途**：查询由 `macAddress` 和 `sportType` 唯一定位的 Goal/Shot 水印显示配置。`goalVisible` 与 `shotVisible` 相互独立。

| 项 | 内容 |
|---|---|
| 调用类型 | query |
| Params Schema | `SportConfigTargetParams` |
| Result Schema | `SportGoalShotWatermarkConfig` |
| 是否触发事件 | 否。 |
| 幂等性 / 异步性 | 幂等；同步返回完整配置 snapshot。 |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_ARGUMENT`, `NOT_FOUND`, `PERMISSION_DENIED`, `UNAVAILABLE` |

#### 3.4.1 d block 示例

request:

```json
{
  "id": 204,
  "method": "sport.getGoalShotWatermarkConfig",
  "params": {
    "macAddress": "AA:BB:CC:DD:EE:FF",
    "sportType": "basketball"
  }
}
```

success:

```json
{
  "id": 204,
  "status": { "ok": true, "code": 0 },
  "result": {
    "macAddress": "AA:BB:CC:DD:EE:FF",
    "sportType": "basketball",
    "goalVisible": true,
    "shotVisible": true
  }
}
```

### 3.5 `sport.setGoalShotWatermarkConfig`

**用途**：一次请求原子设置目标设备、目标运动项目的 Goal 和 Shot 水印显示状态。两个 visibility 字段均为必填，不采用 partial update。

| 项 | 内容 |
|---|---|
| 调用类型 | command |
| Params Schema | `SportSetGoalShotWatermarkConfigParams` |
| Result Schema | `Empty` |
| 是否触发事件 | 否；需要确认最终值时重新调用 get method。 |
| 幂等性 / 异步性 | 幂等；相同完整配置重复提交应成功。 |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_ARGUMENT`, `NOT_FOUND`, `INVALID_STATE`, `BUSY`, `PERMISSION_DENIED`, `UNAVAILABLE` |

#### 3.5.1 d block 示例

request:

```json
{
  "id": 205,
  "method": "sport.setGoalShotWatermarkConfig",
  "params": {
    "macAddress": "AA:BB:CC:DD:EE:FF",
    "sportType": "basketball",
    "goalVisible": true,
    "shotVisible": false
  }
}
```

success:

（无业务 `result`。）

```json
{
  "id": 205,
  "status": { "ok": true, "code": 0 }
}
```

### 3.6 `sport.getEventClipConfig`

**用途**：查询目标设备、目标运动项目当前的事件视频片段窗口。窗口以 `shot` 或 `goal` 的 `occurredAt` 为中心，分别向前、向后扩展指定秒数。

| 项 | 内容 |
|---|---|
| 调用类型 | query |
| Params Schema | `SportConfigTargetParams` |
| Result Schema | `SportEventClipConfig` |
| 是否触发事件 | 否。 |
| 幂等性 / 异步性 | 幂等；同步返回完整配置 snapshot。 |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_ARGUMENT`, `NOT_FOUND`, `PERMISSION_DENIED`, `UNAVAILABLE` |

#### 3.6.1 d block 示例

request:

```json
{
  "id": 206,
  "method": "sport.getEventClipConfig",
  "params": {
    "macAddress": "AA:BB:CC:DD:EE:FF",
    "sportType": "basketball"
  }
}
```

success:

```json
{
  "id": 206,
  "status": { "ok": true, "code": 0 },
  "result": {
    "macAddress": "AA:BB:CC:DD:EE:FF",
    "sportType": "basketball",
    "beforeOffsetSeconds": 5,
    "afterOffsetSeconds": 10
  }
}
```

### 3.7 `sport.setEventClipConfig`

**用途**：一次请求原子设置目标设备、目标运动项目的事件视频片段前后窗口。两个 offset 均为非负整数秒且均为必填，不拆成两个 method，也不采用 partial update。

| 项 | 内容 |
|---|---|
| 调用类型 | command |
| Params Schema | `SportSetEventClipConfigParams` |
| Result Schema | `Empty` |
| 是否触发事件 | 否；需要确认最终值时重新调用 get method。 |
| 幂等性 / 异步性 | 幂等；相同完整配置重复提交应成功。 |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_ARGUMENT`, `OUT_OF_RANGE`, `NOT_FOUND`, `INVALID_STATE`, `BUSY`, `PERMISSION_DENIED`, `UNAVAILABLE` |

#### 3.7.1 d block 示例

request:

```json
{
  "id": 207,
  "method": "sport.setEventClipConfig",
  "params": {
    "macAddress": "AA:BB:CC:DD:EE:FF",
    "sportType": "basketball",
    "beforeOffsetSeconds": 5,
    "afterOffsetSeconds": 10
  }
}
```

success:

（无业务 `result`。）

```json
{
  "id": 207,
  "status": { "ok": true, "code": 0 }
}
```

### 3.8 多设备目标与公共配置规则

- `macAddress` MUST 使用 `AA:BB:CC:DD:EE:FF` 形式的大写、冒号分隔 MAC 地址；格式非法返回 `INVALID_ARGUMENT`。
- `macAddress` 对应设备不可见或不存在时返回 `NOT_FOUND`；已注册但目标设备、固件或 `sportType` 不提供该配置时返回 `NOT_SUPPORTED`。
- get/set 都必须携带 `macAddress` 和 `sportType`，App 不得用当前 UI 选中项或当前连接隐式推断目标。
- 两个 set method 只以标准 RPC `status` 表达成功或失败；成功响应不返回业务 `result`。
- offset 的产品上限由设备能力和资源约束决定；超出设备支持范围时返回 `OUT_OF_RANGE`，失败时不得只应用前偏移或后偏移中的一个。

## 4. 事件 Events

### 4.0 事件速览

| Event | 触发条件 | Payload Schema | 客户端处理建议 | 状态 |
|---|---|---|---|---|
| `sport.eventDetectionStateChanged` | 某个运动项目的有效状态、运行态或可用性变化。 | `SportEventDetectionStateChangedEvent` | 更新对应项目状态；event 丢失时调用 `sport.getEventDetectionConfig`。 | `[REVIEW-OK]` |
| `sport.eventDetected` | 设备对已启用运动项目判定一个已注册业务事件。 | `SportEventDetectedEvent` | 按 `eventId`/`sequence` 去重排序，根据 `sportType` 交给专项处理器。 | `[REVIEW-OK]` |

### 4.1 `sport.eventDetectionStateChanged`

**触发条件**：`sport.setEventDetectionConfig` 导致状态实际变化、设备重启/恢复默认、设备策略或算法前置条件改变时，设备向已授权且已订阅的 peer 推送。

#### 4.1.1 Payload：`SportEventDetectionStateChangedEvent`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `state` | `SportEventDetectionState` | yes | see Section 6.1 | none | 变化后的完整项目状态。 |
| `source` | enum | yes | `remoteApp`, `localPolicy`, `restart`, `restore`, `unknown` | none | 状态变化来源。 |
| `reason` | string | no | feature-specific reason | omitted | 变化原因或不可用摘要。 |

#### 4.1.2 d block 示例

```json
{
  "event": "sport.eventDetectionStateChanged",
  "intent": 1,
  "data": {
    "state": {
      "sportType": "basketball",
      "effectiveEnabled": true,
      "runtimeState": "ready",
      "applyState": "applied",
      "stateRevision": 8
    },
    "source": "remoteApp",
    "reason": "detector_ready"
  }
}
```

#### 4.1.3 客户端处理建议

- 完整状态可直接更新对应项目的 toggle、运行态和不可用提示。
- 收到部分状态、revision gap、未知项目或无法解析的专项字段时，调用 `sport.getEventDetectionConfig` 校准。
- 重连后重新订阅事件，并以设备查询结果覆盖 App 本地缓存。

#### 4.1.4 规则

- Event 使用 RPC `op=6`，不携带 request `d.id`；`sport.eventDetectionStateChanged` 的 domain event bitOffset 为 `0`。
- 失败的 set request 不得触发该 event。
- 设备不得向未声明 `sport.eventDetection` capability 或未订阅该 event 的 peer 发送 optional event。

### 4.2 `sport.eventDetected`

**触发条件**：设备在某个项目的 `runtimeState=ready`，或产品确认允许的 `degraded` 状态下，判定一个已注册的业务事件。

#### 4.2.1 Payload：`SportEventDetectedEvent`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `eventId` | string | yes | opaque non-empty | none | 设备事件唯一标识，用于去重。 |
| `sequence` | uint64 | yes | monotonic sequence | none | 检测事件顺序；同一设备会话内用于排序，重复事件保持原值。 |
| `sportType` | string | yes | capability 中声明的项目 | none | 运动项目 discriminator。 |
| `eventType` | string | yes | 该项目 capability 中声明的事件类型 | none | 业务事件 discriminator，例如 `shot` 或 `goal`。 |
| `occurredAt` | string | yes | device timestamp | none | 算法判定时间；不作为跨设备排序的唯一依据。 |
| `confidence` | number | no | `0.0..1.0` | omitted | 可选算法置信度。 |
| `trainingSessionId` | string | no | opaque id | omitted | 可选训练会话关联。 |
| `details` | `SportEventDetails` | yes | discriminator-qualified wrapper | none | Wire 上的通用 details carrier；字段按 `(sportType,eventType)` 解释，专项语义由 `sport.<discipline>` 定义。 |

#### 4.2.2 d block 示例

```json
{
  "event": "sport.eventDetected",
  "intent": 1,
  "data": {
    "eventId": "evt-1024",
    "sequence": 1024,
    "sportType": "basketball",
    "eventType": "shot",
    "occurredAt": "<device-time>",
    "confidence": 0.94,
    "details": {
      "shotId": "shot-1024"
    }
  }
}
```

#### 4.2.3 客户端处理建议

- App 先按 `eventId`/`sequence` 去重和排序，再按 `sportType` 路由到专项处理器。
- App 不得根据 `eventType` 单独推断字段；必须按 `(sportType,eventType)` 解析 `details`。
- 未知的可选 `sportType`、`eventType` 或 `details` 应只降级该事件，不得关闭 session 或阻断无关 RPC。
- 设备重试同一事件时必须保持 `eventId` 和专项业务关联 ID 不变。[REVIEW-OK]

#### 4.2.4 规则

- `details` 必须使用已登记的 `SportEventDetails` wrapper；专项 feature 通过 discriminator 约束其中字段，新增专用 schema 需走 amendment。
- `sport.eventDetected` 不替代任何同步 RPC Response。
- AXTP RPC Event 默认不保证 exactly-once；如业务要求断线补发，应另行采纳 history/replay method。
- 若设备同时支持多个项目，事件必须携带明确 `sportType`，不得依赖当前 UI 或当前 active project 推断。

## 5. Capability

Capability name: `sport.eventDetection`（adopted）。它位于 `sport` domain，并作为跨运动项目公共控制面和统一事件 envelope 的能力声明。

Capability 只描述设备能做什么，不混入具体 method params/result 或专项 event payload。

| 能力字段 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `capability` | string | yes | 固定 `sport.eventDetection` | none | 通用能力名称。 |
| `supportedSports` | array<object> | yes | one or more sport descriptors | none | 设备支持的运动项目及事件类型集合。 |
| `concurrentSportTypes` | bool | yes | `true` / `false` | none | 是否允许多个项目同时处于 enabled/ready。 |
| `supportsStateEvent` | bool | yes | `true` / `false` | none | 是否支持公共状态变化事件。 |
| `supportsUnifiedEvent` | bool | yes | `true` / `false` | none | 是否支持 `sport.eventDetected` 统一事件。 |
| `persistencePolicy` | enum | yes | `volatile`, `persistent`, `profileControlled` | `persistent` | VM33PRO 的检测开关持久化；恢复出厂后默认关闭。 |

`supportedSports` 的 descriptor 结构：

| 字段 | 类型 | 必填 | 说明 |
|---|---|---:|---|
| `sportType` | string | yes | 稳定的运动项目 discriminator，例如 `basketball`。 |
| 项目事件类型集合 | array<string> | yes | 该项目可通过 `sport.eventDetected` 上报的事件类型；正式字段名和支持范围以 Registry capability descriptor 为准。 |
| `supportsToggle` | bool | yes | 是否支持 `sport.setEventDetectionConfig`。 |
| `detailsSchemas` | array<string> | no | `(sportType,eventType)` 对应的专项 schema 名称。 |
| `supportedConfigMethods` | array<string> | no | 该运动项目支持的已注册配置 method 名称；省略表示设备未声明运动项目专属配置 method。 |

## 6. Schemas

### 6.0 Schema hierarchy

```text
SportEventDetectionCapabilities
  supportedSports[]
    sportType / supportedEvents / supportsToggle / detailsSchemas / supportedConfigMethods

SportEventDetectionState
  sportType / effectiveEnabled / runtimeState / applyState / stateRevision

SportEventDetectionStateChangedEvent
  state / source / reason

SportEventDetectedEvent
  eventId / sequence / sportType / eventType / metadata / details
```

### 6.1 Runtime state schema：`SportEventDetectionState`

| 字段 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `sportType` | string | yes | requested project | none | 状态所属项目。 |
| `effectiveEnabled` | bool | yes | true/false | none | 设备实际是否启用该项目检测。 |
| `runtimeState` | enum | yes | disabled/enabling/ready/degraded/unavailable/disabling | none | 当前算法运行态。 |
| `applyState` | enum | no | applied/pending | omitted | 最近一次控制的生效状态。 |
| `reason` | string | no | feature-specific | omitted | 不可用、降级或状态变化原因。 |
| `stateRevision` | uint32 | no | monotonic | omitted | 状态版本，用于客户端校准和可选 compare-and-set。 |
| `updatedAt` | string | no | device timestamp | omitted | 状态更新时间。 |

### 6.2 Event metadata and discriminated payload

`SportEventDetectedEvent` 的通用 metadata 是稳定公共结构；Wire `details` 使用 `SportEventDetails` common wrapper；字段合法性由 `(sportType,eventType)` discriminator 约束。专项 feature 可以增加可选字段或在后续 amendment 中注册 dedicated event schema，但不得改变通用字段语义。

| 字段 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `eventId` | string | yes | opaque non-empty | none | 事件去重 ID。 |
| `sequence` | uint64 | yes | monotonic sequence | none | 事件顺序依据。 |
| `sportType` | string | yes | capability-supported | none | 专项 schema discriminator。 |
| `eventType` | string | yes | sport-specific registered type | none | 事件类型 discriminator。 |
| `occurredAt` | string | no | device timestamp | omitted | 设备时间。 |
| `confidence` | number | no | 0.0..1.0 | omitted | 算法置信度。 |
| `trainingSessionId` | string | no | opaque id | omitted | 可选训练会话关联。 |
| `details` | `SportEventDetails` | yes | discriminator-qualified wrapper | none | 运动项目专项结果的通用载体。 |

### 6.3 Request / result schema index

| Schema | 用途 | 说明 |
|---|---|---|
| `GetEventDetectionCapabilitiesParams` | capabilities query request | 见 3.1.1。 |
| `GetEventDetectionCapabilitiesResult` | capabilities query result | `capability` wrapper，见 3.1.2；descriptor 见第 5 章。 |
| `GetEventDetectionConfigParams` | state/config query request | 见 3.2.1。 |
| `SetEventDetectionConfigParams` | set request | 见 3.3.1。 |
| `SetEventDetectionConfigResult` | set result | `accepted` + `SportEventDetectionState`，见 3.3.2。 |

### 6.4 Event schema index

| Schema | Event | 说明 |
|---|---|---|
| `SportEventDetectionStateChangedEvent` | `sport.eventDetectionStateChanged` | 见 4.1.1。 |
| `SportEventDetectedEvent` | `sport.eventDetected` | 通用 metadata + 专项 `details`，见 4.2.1。 |

所有已采纳的 schema field ID、method/event ID 和 domain event bitOffset 均以 `contract/registry/domains/sport/domain.yaml` 为准；本文件只保留可读的语义说明，不另行分配编号。

## 7. 交互流程示例 Flow Examples

### 7.1 Discover basketball support -> enable -> event

```text
1. App 调用 sport.getEventDetectionCapabilities，确认 basketball 支持 shot/goal。
2. App 调用 sport.getEventDetectionConfig(sportType=basketball) 校准当前状态。
3. 用户打开开关，App 调用 sport.setEventDetectionConfig(sportType=basketball, enabled=true)。
4. 若返回 applyState=pending，App 等待 sport.eventDetectionStateChanged。
5. 设备 ready 后通过 sport.eventDetected 上报 sportType=basketball 的 shot/goal；App 路由到 sport.basketball 专项解析器。
```

### 7.2 Add another sport without changing common control

```text
1. App 查询 capability，发现 supportedSports 同时包含 football。
2. App 对 football 复用相同的 get/set config 和 state event 流程。
3. 设备通过同一个 sport.eventDetected event 上报 sportType=football。
4. App 只新增 football details schema/parser，不改变通用去重、订阅和重连逻辑。
```

### 7.3 Reconnect -> state calibration

```text
1. WebSocket 断开，App 标记实时事件可能不完整。
2. SDK 重建 AXTP-WS-JSON，完成 session、Identify 和 event subscription。
3. App 按已启用的 sportType 逐项调用 sport.getEventDetectionConfig。
4. App 使用设备返回的 state 覆盖本地 toggle；事件补发/历史查询只有在另行采纳 recovery 语义后才可宣称支持。
```

## 8. Common Error Mapping

本通用 feature 全部复用现有 common errors，不新增 feature-specific error 或 numeric errorCode。

| 场景 | 采用的 common error | 说明 |
|---|---|---|---|
| 请求的运动项目未被 capability 声明 | `NOT_SUPPORTED` | 不新增 `SPORT_TYPE_NOT_SUPPORTED`。 |
| 事件类型未注册或参数枚举非法 | `INVALID_ARGUMENT` / `NOT_SUPPORTED` | 按请求错误或设备能力缺失区分。 |
| 检测器、摄像头、隐私或资源暂不可用 | `UNAVAILABLE` | 在 state `reason` 或错误 details 中表达专项原因。 |
| `expectedStateRevision` 冲突 | `INVALID_STATE` | 返回当前 state/revision。 |

通用错误映射：

| 场景 | 推荐现有错误 |
|---|---|
| method 未进入正式 registry | `RPC_METHOD_NOT_FOUND` |
| 已注册但设备/能力/profile 不支持 | `NOT_SUPPORTED` |
| 参数 malformed 或项目/事件枚举非法 | `INVALID_ARGUMENT` |
| 当前状态不允许切换 | `INVALID_STATE` |
| 算法或资源正在切换 | `BUSY` |
| 权限不足 | `PERMISSION_DENIED` |
| 服务暂时不可用 | `UNAVAILABLE` |
| 内部执行失败 | `INTERNAL_ERROR` |

## 9. Legacy Mapping

本次未采纳 legacy mapping。现有 VM33 旧 HTTP JSON 或私有 WebSocket 线索不足以形成可验证的一一映射；如后续需要兼容旧实现，应提供具体旧字段/命令/版本证据并单独走迁移或 amendment 流程。

## 10. Registry / Conformance Status

| Item | Current state |
|---|---|
| Draft status | Adopted：本文件记录已采纳的 `sport.eventDetection` formal proposal。 |
| Domain registry | Adopted：`sport` 使用 high-byte `0x18`，状态为 `draft`。 |
| Domain YAML | Adopted：`contract/registry/domains/sport/domain.yaml`。 |
| Generated protocol | Generated：已通过 Stage 50 从 Registry YAML 刷新。 |
| Protocol IR / MCP / test vectors | 已由 Generator 更新 Protocol IR、generated docs 和 MCP registry；测试向量无内容变化。 |
| Conformance | needed：多项目 capability、通用 state、统一 event envelope、专项 details、subscription、error、reconnect 和 dedupe cases。 |

Adoption blockers：无。2026-09-07 amendment 已确认新增 4 个配置 method、MAC + sportType 目标选择器、原子更新与 status-only set response，并写入 Registry YAML。

## 11. Test Notes

| Case | Given | When | Then |
|---|---|---|---|
| multi-sport capability | 设备支持 basketball 和 football | App 查询 capability | 返回每个项目的 supportedEvents、toggle 和并发约束。 |
| unsupported sport | App 请求未声明的 `sportType` | 调用 get/set | 返回 `NOT_SUPPORTED` 或 `INVALID_ARGUMENT`，session 保持可用。 |
| common enable flow | basketball capability available | App set enabled=true | 返回项目 state；pending 时后续发送公共 state event。 |
| unified event | detector ready and subscribed | 设备判定 shot/goal | 发送 `sport.eventDetected`，携带明确的 `sportType`、`eventType` 和专项 details。 |
| unknown details | App 尚未支持新专项 event | 收到未知 `(sportType,eventType)` | 只忽略/记录该事件，不关闭 session。 |
| duplicate / out-of-order | transport retry or concurrent events | App 收到重复/乱序事件 | 按 eventId/sequence 去重排序；专项关联不被静默错误绑定。 |
| reconnect | WebSocket lost during detection | SDK reconnects and resubscribes | 按已启用项目查询 state；不默认承诺 exactly-once。 |
| no STREAM | App uses AXTP-WS-JSON | detector produces events | 只发送 RPC Event，不发送 STREAM 或原始视频。 |
| multi-device target | 一个 App 管理多台设备 | 对不同 `macAddress` 调用配置 get/set | 每个请求只读写 MAC + sportType 唯一定位的配置。 |
| watermark round trip | 目标设备支持篮球水印 | set 后调用 get | `goalVisible` 与 `shotVisible` 按完整配置原子落地，set 成功响应无业务 result。 |
| clip round trip | 目标设备支持事件视频片段 | set 前后秒数后调用 get | 两个 offset 原子落地，get 返回相同完整窗口。 |
| invalid MAC | App 发送非规范 MAC | 调用任一新增 method | 返回 `INVALID_ARGUMENT`，不修改任何配置。 |
| missing device | MAC 不对应可见设备 | 调用任一新增 method | 返回 `NOT_FOUND`，session 保持可用。 |

## 12. Adoption Decisions / Closed Questions

| 问题 | 影响 | 当前建议 | 状态 |
|---|---|---|---|
| 一个设备是否允许同时启用多个 `sportType`？ | state schema / resource arbitration / UI | MVP 固定 `concurrentSportTypes=false`，同一时间最多启用一个项目。 | closed |
| `sport.eventDetected` 是否覆盖所有专项？ | event registry / payload complexity | 统一 envelope 为默认路径；专项 event 仅在未来 amendment 明确需要时新增。 | closed |
| `eventType` 命名方式 | registry / compatibility | 使用项目内短名，`sportType` 作为 discriminator。 | closed |
| `details` 的 wire 表达 | codec / generated schema | 使用已登记的 `SportEventDetails` common wrapper；篮球语义由 discriminator-qualified projection 表达。 | closed |
| 未来是否需要比赛、队伍、比分、回合和赛程？ | scope / future features | 不放入 eventDetection；另行评估其他 sport feature。 | out of scope |
| 开关持久化和恢复默认 | state lifecycle / UX | VM33PRO 持久化；恢复出厂后默认关闭，重连以 state query 为准。 | closed |
| 断线期间事件可靠性 | reliability / storage | MVP 不提供断线补发、历史查询或 exactly-once；需要时新增 bounded recovery method。 | closed |
| 一个 App 如何定位多台设备中的目标？ | routing / consistency | 新增配置 method 的 request 必须同时携带 `macAddress` 与 `sportType`。 | closed |
| 水印 method 命名 | registry / SDK API | 使用 `sport.getGoalShotWatermarkConfig` / `sport.setGoalShotWatermarkConfig`。 | closed |
| 打点的业务含义 | clip timing / media integration | 表示以 `shot` / `goal` 事件发生时刻为中心的视频片段窗口，使用 `sport.getEventClipConfig` / `sport.setEventClipConfig`。 | closed |
| 前后偏移是否拆分 method | atomicity / error handling | 不拆分；一次请求原子更新 `beforeOffsetSeconds` 和 `afterOffsetSeconds`。 | closed |
| 新增配置是否需要 get method | reconnect / state calibration | 两类配置均提供 get 和 set；set 成功无业务 result，必要时由 get 校准。 | closed |

## 13. Amendment History

| 日期 | 变更 | 理由 | 兼容性 | YAML 目标 |
|---|---|---|---|---|
| 2026-09-07 | 新增 Goal/Shot 水印配置与事件视频片段窗口配置，共 4 个 get/set method；请求按 `macAddress` + `sportType` 定位；set 完整原子更新且仅返回标准 status。 | 支持一个 App 控制多台设备，并为未来足球、棒球、网球等运动项目保留统一目标维度。 | 向后兼容的 draft method/schema addition；保留既有 method ID、event ID、bitOffset 和 field ID。 | `contract/registry/domains/sport/domain.yaml` |
| 2026-09-07 | 在 `SportEventDetectionSportDescriptor` 增加可选 `supportedConfigMethods`，用于按运动项目发现水印和事件片段配置 method。 | 让 App 在调用配置 method 前能判断目标运动项目是否实际支持该能力。 | 向后兼容的 optional field addition；既有 descriptor field ID 保持不变。 | `contract/registry/domains/sport/domain.yaml` |

## 附录 A. Adoption Record

- 用户已确认方案 C 及全部推荐决策；本稿无未解决审核阻塞、修复或询问项。
- method/event/capability/schema 的机器事实以 `contract/registry/domains/sport/domain.yaml` 为唯一来源。
- 后续已采纳语义变更必须先更新本 formal proposal，再使用 `amend-adopted-protocol`。

## 附录 B. Registry 草案输入

以下是已确认的 Stage 30 adoption 输入摘要；正式机器事实见 `contract/registry/domains/sport/domain.yaml`：

| Fact type | Adopted fact | ID / status |
|---|---|---|
| Domain | `sport` | high-byte `0x18`; `draft` |
| Capability | `sport.eventDetection` | `0x1801`; `draft` |
| Methods | 原 3 个 event detection method；新增 `sport.getGoalShotWatermarkConfig`, `sport.setGoalShotWatermarkConfig`, `sport.getEventClipConfig`, `sport.setEventClipConfig` | 原 ID `0x1801`–`0x1803` 保持；新增 ID `0x1804`–`0x1807`，domain bitOffset `3`–`6` |
| Events | `sport.eventDetectionStateChanged`, `sport.eventDetected` | `0x1801` / `0x1802`; domain bitOffset `0` / `1` |
| Basketball capability | `sport.basketball` | `0x1802`; `draft` |
| Schemas | Public and basketball schemas | field IDs `0x01` onward in accepted order; exact source is domain YAML |
| Errors | Common `NOT_SUPPORTED`, `INVALID_ARGUMENT`, `INVALID_STATE`, `BUSY`, `PERMISSION_DENIED`, `UNAVAILABLE`, `INTERNAL_ERROR` | no feature-specific error allocated |
