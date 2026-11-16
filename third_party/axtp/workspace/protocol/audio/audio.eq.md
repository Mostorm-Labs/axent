---
status: review-ok
contract: false
generated: false
domain: audio
feature: audio.eq
registry:
lastReviewed: 2026-06-15
---

# audio.eq

## 0. 速读结论

| 项目 | 内容 |
|---|---|
| 这个能力做什么 | 管理音频路径上的 EQ preset、band、filter、frequency、gain 和 Q 值。 |
| 当前状态 | review-ok |
| 是否可直接实现 | 否。本文是 protocol draft；正式实现以 registry / generated 为准。 |
| 主要交互 | RPC + EVENT |
| 是否使用 STREAM | 否 |
| Registry readiness | candidate |
| Conformance | needed |
| 主要未决问题 | schema 字段、错误模型、legacy 映射和 conformance case 仍需人工确认。 |

## 1. 功能说明

`audio.eq` 用于管理音频路径上的 EQ preset、band、filter、frequency、gain 和 Q 值。

## 2. 能力边界

| 类型 | 内容 |
|---|---|
| 包含 | audio.eq 的能力发现、状态查询、配置或动作控制。 |
| 包含 | 与 audio.eq 直接相关的 method/event/schema 草案。 |
| 不包含 | 不承载其他 capability feature 的业务语义；跨域关系通过 schema 字段、引用或数据面 stream/file 表达。 |
| 不包含 | method/event 数值 ID 分配；数值以 contract/registry/generated 为准。 |
| 数据面 | 本 feature 默认不定义 STREAM payload，所有操作均通过 RPC method/event 完成。 |

## 3. 方法 Methods

### 3.0 方法速览

| Method | 调用类型 | 用途 | Params Schema | Result Schema | 是否触发事件 | 状态 |
|---|---|---|---|---|---|---|
| `audio.getEqCapabilities` | query | 查询设备支持的 EQ 路径、preset、band、范围、默认值和更新策略 | `AudioGetEqCapabilitiesRequest` | `AudioGetEqCapabilitiesResponse` | 否 | candidate |
| `audio.getEqConfig` | query | 查询当前生效的 EQ 配置 | `AudioGetEqConfigRequest` | `AudioEqConfig` | 否 | candidate |
| `audio.setEqConfig` | command | 更新一个或多个路径的 EQ 配置 | `AudioSetEqConfigRequest` | `AudioSetEqConfigResponse` | 是，`audio.eqConfigChanged` | candidate |
| `audio.resetEqConfig` | action | 将全部路径、指定路径或指定字段恢复到能力声明的默认配置 | `AudioResetEqConfigRequest` | `AudioSetEqConfigResponse` | 是，`audio.eqConfigChanged` | candidate |

### 3.1 `audio.getEqCapabilities`

**用途**：查询设备支持的 EQ 路径、preset、band、范围、默认值和更新策略。

| 项 | 内容 |
|---|---|
| 调用类型 | query |
| Params Schema | `AudioGetEqCapabilitiesRequest` |
| Result Schema | `AudioGetEqCapabilitiesResponse` |
| 是否触发事件 | 否 |
| 幂等性 / 异步性 | 幂等；同步返回当前快照。 |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_ARGUMENT`, `PERMISSION_DENIED`, `UNAVAILABLE` |

#### 3.1.1 请求参数 Params：`AudioGetEqCapabilitiesRequest`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `target` | string | no | target id | `default` | 示例值 `main-output`；查询对象。 |

#### 3.1.2 返回结果 Result：`AudioGetEqCapabilitiesResponse`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `state` | object | yes | see schema | none | 当前结果对象；示例字段包括 `target`、`bandCount`、`filterTypes`、`gainDbRange`、`frequencyHzRange`、`qRange`。 |
| `sampledAt` | string timestamp | no | RFC 3339 | omitted | 结果采样时间；客户端可用于缓存和校准。 |

#### 3.1.3 d block 示例

request:

```json
{
  "id": 101,
  "method": "audio.getEqCapabilities",
  "params": {
    "target": "main-output"
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
    "state": {
      "target": "main-output",
      "bandCount": 5,
      "filterTypes": [
        "peak",
        "lowShelf",
        "highShelf"
      ],
      "gainDbRange": {
        "min": -12,
        "max": 12,
        "step": 0.5
      },
      "frequencyHzRange": {
        "min": 20,
        "max": 20000
      },
      "qRange": {
        "min": 0.3,
        "max": 10
      }
    },
    "sampledAt": "2026-06-15T08:00:00Z"
  }
}
```

#### 3.1.4 可能触发的事件

| Event | 触发条件 | Payload Schema | 客户端处理建议 |
|---|---|---|---|
| 无 | query method 不应因查询触发状态变化事件。 | none | 无需处理。 |

#### 3.1.5 错误

| 错误 | 场景 | 返回建议 |
|---|---|---|
| `NOT_SUPPORTED` | 设备不支持该 feature、method、target 或 scope。 | 返回 unsupported feature/method/target。 |
| `INVALID_ARGUMENT` | 请求字段非法、枚举非法或范围非法。 | 返回具体字段路径和合法范围。 |
| `PERMISSION_DENIED` | 调用方无权执行该操作。 | 返回权限错误。 |
| `BUSY` | 设备正在处理冲突操作。 | 建议稍后重试。 |

### 3.2 `audio.getEqConfig`

**用途**：查询当前生效的 EQ 配置。

| 项 | 内容 |
|---|---|
| 调用类型 | query |
| Params Schema | `AudioGetEqConfigRequest` |
| Result Schema | `AudioEqConfig` |
| 是否触发事件 | 否 |
| 幂等性 / 异步性 | 幂等；同步返回当前快照。 |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_ARGUMENT`, `PERMISSION_DENIED`, `UNAVAILABLE` |

#### 3.2.1 请求参数 Params：`AudioGetEqConfigRequest`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `target` | string | no | target id | `default` | 示例值 `main-output`；查询对象。 |

#### 3.2.2 返回结果 Result：`AudioEqConfig`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `state` | object | yes | see schema | none | 当前结果对象；示例字段包括 `target`、`preset`、`bands`。 |
| `sampledAt` | string timestamp | no | RFC 3339 | omitted | 结果采样时间；客户端可用于缓存和校准。 |

#### 3.2.3 d block 示例

request:

```json
{
  "id": 102,
  "method": "audio.getEqConfig",
  "params": {
    "target": "main-output"
  }
}
```

success:

```json
{
  "id": 102,
  "status": {
    "ok": true,
    "code": 0
  },
  "result": {
    "state": {
      "target": "main-output",
      "preset": "speech",
      "bands": [
        {
          "bandId": "low",
          "type": "lowShelf",
          "frequencyHz": 120,
          "gainDb": -1.5,
          "q": 0.7
        },
        {
          "bandId": "mid",
          "type": "peak",
          "frequencyHz": 2500,
          "gainDb": 2,
          "q": 1.2
        }
      ]
    },
    "sampledAt": "2026-06-15T08:00:01Z"
  }
}
```

#### 3.2.4 可能触发的事件

| Event | 触发条件 | Payload Schema | 客户端处理建议 |
|---|---|---|---|
| 无 | query method 不应因查询触发状态变化事件。 | none | 无需处理。 |

#### 3.2.5 错误

| 错误 | 场景 | 返回建议 |
|---|---|---|
| `NOT_SUPPORTED` | 设备不支持该 feature、method、target 或 scope。 | 返回 unsupported feature/method/target。 |
| `INVALID_ARGUMENT` | 请求字段非法、枚举非法或范围非法。 | 返回具体字段路径和合法范围。 |
| `PERMISSION_DENIED` | 调用方无权执行该操作。 | 返回权限错误。 |
| `BUSY` | 设备正在处理冲突操作。 | 建议稍后重试。 |

### 3.3 `audio.setEqConfig`

**用途**：更新一个或多个路径的 EQ 配置。

| 项 | 内容 |
|---|---|
| 调用类型 | command |
| Params Schema | `AudioSetEqConfigRequest` |
| Result Schema | `AudioSetEqConfigResponse` |
| 是否触发事件 | 是，状态实际变化后触发 `audio.eqConfigChanged`。 |
| 幂等性 / 异步性 | 建议幂等；重复提交相同目标状态应成功，可不重复触发事件。 |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_ARGUMENT`, `INVALID_STATE`, `BUSY`, `PERMISSION_DENIED` |

#### 3.3.1 请求参数 Params：`AudioSetEqConfigRequest`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `target` | string | no | target id | `default` | 设置对象；具体 target 集合由 capability 声明。 |
| `config` | object | yes | see schema | none | 目标配置或状态片段；字段需在采纳前确认。 |

#### 3.3.2 返回结果 Result：`AudioSetEqConfigResponse`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `accepted` | boolean | yes | `true`, `false` | none | 设备是否接受并应用请求。 |
| `state` | object | no | see schema | omitted | 设置后的状态或配置快照。 |

#### 3.3.3 d block 示例

request:

```json
{
  "id": 103,
  "method": "audio.setEqConfig",
  "params": {
    "target": "main-output",
    "config": {
      "preset": "custom",
      "bands": [
        {
          "bandId": "mid",
          "type": "peak",
          "frequencyHz": 2500,
          "gainDb": 3,
          "q": 1.2
        }
      ]
    }
  }
}
```

success:

```json
{
  "id": 103,
  "status": {
    "ok": true,
    "code": 0
  },
  "result": {
    "accepted": true,
    "state": {
      "target": "main-output",
      "preset": "custom",
      "appliedBands": [
        "mid"
      ]
    }
  }
}
```

#### 3.3.4 可能触发的事件

| Event | 触发条件 | Payload Schema | 客户端处理建议 |
|---|---|---|---|
| `audio.eqConfigChanged` | 该方法导致状态、配置或动作状态实际变化。 | `AudioEqConfigChangedEvent` | 可直接更新 UI；需要完整状态时调用对应 get method 校准。 |

#### 3.3.5 错误

| 错误 | 场景 | 返回建议 |
|---|---|---|
| `NOT_SUPPORTED` | 设备不支持该 feature、method、target 或 scope。 | 返回 unsupported feature/method/target。 |
| `INVALID_ARGUMENT` | 请求字段非法、枚举非法或范围非法。 | 返回具体字段路径和合法范围。 |
| `PERMISSION_DENIED` | 调用方无权执行该操作。 | 返回权限错误。 |
| `BUSY` | 设备正在处理冲突操作。 | 建议稍后重试。 |

### 3.4 `audio.resetEqConfig`

**用途**：将全部路径、指定路径或指定字段恢复到能力声明的默认配置。

| 项 | 内容 |
|---|---|
| 调用类型 | action |
| Params Schema | `AudioResetEqConfigRequest` |
| Result Schema | `AudioSetEqConfigResponse` |
| 是否触发事件 | 是，状态实际变化后触发 `audio.eqConfigChanged`。 |
| 幂等性 / 异步性 | 建议幂等；重复提交相同目标状态应成功，可不重复触发事件。 |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_ARGUMENT`, `INVALID_STATE`, `BUSY`, `PERMISSION_DENIED` |

#### 3.4.1 请求参数 Params：`AudioResetEqConfigRequest`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `target` | string | no | target id | `default` | 动作对象；具体 target 集合由 capability 声明。 |
| `reason` | string | no | caller-defined reason | omitted | 调用方给出的动作原因。 |

#### 3.4.2 返回结果 Result：`AudioSetEqConfigResponse`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `accepted` | boolean | yes | `true`, `false` | none | 设备是否接受动作请求。 |
| `actionId` | string | no | opaque action id | omitted | 动作 ID，用于日志或异步关联。 |

#### 3.4.3 d block 示例

request:

```json
{
  "id": 104,
  "method": "audio.resetEqConfig",
  "params": {
    "target": "main-output",
    "reason": "restore_flat_eq"
  }
}
```

success:

```json
{
  "id": 104,
  "status": {
    "ok": true,
    "code": 0
  },
  "result": {
    "accepted": true,
    "actionId": "eq-reset-20260615-001"
  }
}
```

#### 3.4.4 可能触发的事件

| Event | 触发条件 | Payload Schema | 客户端处理建议 |
|---|---|---|---|
| `audio.eqConfigChanged` | 该方法导致状态、配置或动作状态实际变化。 | `AudioEqConfigChangedEvent` | 可直接更新 UI；需要完整状态时调用对应 get method 校准。 |

#### 3.4.5 错误

| 错误 | 场景 | 返回建议 |
|---|---|---|
| `NOT_SUPPORTED` | 设备不支持该 feature、method、target 或 scope。 | 返回 unsupported feature/method/target。 |
| `INVALID_ARGUMENT` | 请求字段非法、枚举非法或范围非法。 | 返回具体字段路径和合法范围。 |
| `PERMISSION_DENIED` | 调用方无权执行该操作。 | 返回权限错误。 |
| `BUSY` | 设备正在处理冲突操作。 | 建议稍后重试。 |

## 4. 事件 Events

### 4.0 事件速览

| Event | 触发条件 | Payload Schema | 客户端处理建议 | 状态 |
|---|---|---|---|---|
| `audio.eqConfigChanged` | `audio.setEqConfig` 或 `audio.resetEqConfig` 成功改变配置；profile、device policy、restore、factory reset 改变 EQ。 | `AudioEqConfigChangedEvent` | 更新 UI 或调用对应 get method 校准 | candidate |

### 4.1 `audio.eqConfigChanged`

**触发条件**：`audio.setEqConfig` 或 `audio.resetEqConfig` 成功改变配置；profile、device policy、restore、factory reset 改变 EQ。

#### 4.1.1 Payload：`AudioEqConfigChangedEvent`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `changedFields` | string[] | no | field path array | omitted | 变化字段路径。 |
| `state` | object | no | see schema | omitted | 变化后的状态、配置或摘要。 |
| `source` | string enum | no | `remoteApp`, `localPanel`, `devicePolicy`, `adapter`, `unknown` | `unknown` | 状态变化来源。 |
| `reason` | string enum | no | feature-specific | `unknown` | 状态变化原因。 |
| `stateRevision` | uint32 | no | monotonic counter | omitted | 状态版本，用于多端同步和去重。 |

#### 4.1.2 d block 示例

```json
{
  "event": "audio.eqConfigChanged",
  "intent": 1,
  "data": {
    "changedFields": [
      "bands.mid.gainDb"
    ],
    "state": {
      "target": "main-output",
      "preset": "custom",
      "bands": [
        {
          "bandId": "mid",
          "gainDb": 3
        }
      ]
    },
    "source": "remoteApp",
    "reason": "user_request",
    "stateRevision": 42
  }
}
```

#### 4.1.3 客户端处理建议

| 场景 | 建议 |
|---|---|
| payload 是完整状态 | 可直接更新 UI 或本地缓存。 |
| payload 是变化片段 | 调用对应 get method 校准完整状态。 |
| event 丢失或重连 | 重连后主动调用 get method 校准。 |

#### 4.1.4 规则

- Event MUST 使用 `op=6`。
- Event MUST NOT 携带 `d.id`。
- Event payload MUST 放在 `d.data` 中。

## 5. Capability

Capability name: `audio.eq`。

设备通过 capability 声明是否支持该 feature，以及支持哪些范围、模式、对象或约束。Capability 字段只描述“设备能做什么”，不得混入 method params/result 或 event payload。

| 能力字段 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `capability` | string | yes | fixed `audio.eq` | none | capability 名称。 |
| `supportedTargets` | string[] | no | target id array | omitted | 支持的对象、通道、端口、组件或 scope。 |

## 6. 字段 / Schemas

### 6.1 Schema 层级速览

```text
EqCapability
  capability / supportedTargets
EqState
  target / status / sampledAt
EqChangedEvent
  changedFields / state / source / reason / stateRevision
```

### 6.2 请求与响应 Schemas

| Schema | 用途 | 字段定义 |
|---|---|---|
| `AudioGetEqCapabilitiesRequest` | `audio.getEqCapabilities` request params | 见 `audio.getEqCapabilities` 方法小节。 |
| `AudioGetEqCapabilitiesResponse` | `audio.getEqCapabilities` result | 见 `audio.getEqCapabilities` 方法小节。 |
| `AudioGetEqConfigRequest` | `audio.getEqConfig` request params | 见 `audio.getEqConfig` 方法小节。 |
| `AudioEqConfig` | `audio.getEqConfig` result | 见 `audio.getEqConfig` 方法小节。 |
| `AudioSetEqConfigRequest` | `audio.setEqConfig` request params | 见 `audio.setEqConfig` 方法小节。 |
| `AudioSetEqConfigResponse` | `audio.setEqConfig` result | 见 `audio.setEqConfig` 方法小节。 |
| `AudioResetEqConfigRequest` | `audio.resetEqConfig` request params | 见 `audio.resetEqConfig` 方法小节。 |
| `AudioSetEqConfigResponse` | `audio.resetEqConfig` result | 见 `audio.resetEqConfig` 方法小节。 |

### 6.3 Capability Schemas


### 6.4 Event Schemas

| Schema | Event | 字段定义 |
|---|---|---|
| `AudioEqConfigChangedEvent` | `audio.eqConfigChanged` | 见 `audio.eqConfigChanged` 事件小节。 |
