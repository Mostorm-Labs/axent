---
status: draft
contract: false
generated: false
domain: video
feature: video.ndi
registry:
lastReviewed: 2026-06-15
---

# video.ndi

## 0. 速读结论

| 项目 | 内容 |
|---|---|
| 这个能力做什么 | NDI 视频输出服务的启用、发现、源名称、输出规格和服务状态。 |
| 当前状态 | draft |
| 是否可直接实现 | 否。本文是 protocol draft；正式实现以 registry / generated 为准。 |
| 主要交互 | RPC + EVENT |
| 是否使用 STREAM | 否 |
| Registry readiness | partial |
| Conformance | needed |
| 主要未决问题 | schema 字段、错误模型、legacy 映射和 conformance case 仍需人工确认。 |

## 1. 功能说明

`video.ndi` 用于NDI 视频输出服务的启用、发现、源名称、输出规格和服务状态。

## 2. 能力边界

| 类型 | 内容 |
|---|---|
| 包含 | video.ndi 的能力发现、配置、状态、动作或事件。 |
| 包含 | 与 video.ndi 直接相关的 method/event/schema 草案。 |
| 包含 | 已确认 legacy 协议到 video.ndi 的语义归类。 |
| 不包含 | NDI 服务配置不等同于 AXTP video.stream 数据面，也不归基础 network。 |
| 不包含 | method/event 数值 ID 分配；数值以 contract/registry/generated 为准。 |
| 不包含 | 未确认旧协议 payload 的稳定映射。 |
| 数据面 | 本 feature 默认不定义 STREAM payload，所有操作均通过 RPC method/event 完成。 |

## 3. 方法 Methods

### 3.0 方法速览

| Method | 调用类型 | 用途 | Params Schema | Result Schema | 是否触发事件 | 状态 |
|---|---|---|---|---|---|---|
| `video.getNdiCapabilities` | query | 查询 NDI 服务能力 | `GetNdiCapabilitiesParams` | `GetNdiCapabilitiesResult` | 否 | draft |
| `video.setNdiConfig` | command | 设置 NDI 服务配置 | `SetNdiConfigParams` | `SetNdiConfigResult` | 是，`video.ndiConfigChanged` | draft |
| `video.getNdiConfig` | query | 查询 NDI 服务配置 | `GetNdiConfigParams` | `GetNdiConfigResult` | 否 | draft |
| `video.resetNdiConfig` | action | 恢复 NDI 默认配置 | `ResetNdiConfigParams` | `ResetNdiConfigResult` | 是，`video.ndiConfigChanged` | draft |

### 3.1 `video.getNdiCapabilities`

**用途**：查询 NDI 服务能力。

| 项 | 内容 |
|---|---|
| 调用类型 | query |
| Params Schema | `GetNdiCapabilitiesParams` |
| Result Schema | `GetNdiCapabilitiesResult` |
| 是否触发事件 | 否 |
| 幂等性 / 异步性 | 幂等；同步返回当前快照。 |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_ARGUMENT`, `PERMISSION_DENIED`, `UNAVAILABLE` |

#### 3.1.1 请求参数 Params：`GetNdiCapabilitiesParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `target` | string | no | target id | `default` | 示例值 `ndi-source-main`；查询对象。 |

#### 3.1.2 返回结果 Result：`GetNdiCapabilitiesResult`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `state` | object | yes | see schema | none | 当前结果对象；示例字段包括 `target`、`discoverySupported`、`groupsSupported`、`maxSources`。 |
| `sampledAt` | string timestamp | no | RFC 3339 | omitted | 结果采样时间；客户端可用于缓存和校准。 |

#### 3.1.3 d block 示例

request:

```json
{
  "id": 101,
  "method": "video.getNdiCapabilities",
  "params": {
    "target": "ndi-source-main"
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
      "target": "ndi-source-main",
      "discoverySupported": true,
      "groupsSupported": true,
      "maxSources": 16
    },
    "sampledAt": "2026-06-15T08:00:01Z"
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

### 3.2 `video.setNdiConfig`

**用途**：设置 NDI 服务配置。

| 项 | 内容 |
|---|---|
| 调用类型 | command |
| Params Schema | `SetNdiConfigParams` |
| Result Schema | `SetNdiConfigResult` |
| 是否触发事件 | 是，状态实际变化后触发 `video.ndiConfigChanged`。 |
| 幂等性 / 异步性 | 建议幂等；重复提交相同目标状态应成功，可不重复触发事件。 |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_ARGUMENT`, `INVALID_STATE`, `BUSY`, `PERMISSION_DENIED` |

#### 3.2.1 请求参数 Params：`SetNdiConfigParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `target` | string | no | target id | `default` | 设置对象；具体 target 集合由 capability 声明。 |
| `config` | object | yes | see schema | none | 目标配置或状态片段；字段需在采纳前确认。 |

#### 3.2.2 返回结果 Result：`SetNdiConfigResult`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `accepted` | boolean | yes | `true`, `false` | none | 设备是否接受并应用请求。 |
| `state` | object | no | see schema | omitted | 设置后的状态或配置快照。 |

#### 3.2.3 d block 示例

request:

```json
{
  "id": 102,
  "method": "video.setNdiConfig",
  "params": {
    "target": "ndi-source-main",
    "config": {
      "sourceName": "AXTP Camera Main",
      "groups": [
        "studio"
      ],
      "lowLatency": true
    }
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
    "accepted": true,
    "state": {
      "target": "ndi-source-main",
      "sourceName": "AXTP Camera Main",
      "groups": [
        "studio"
      ],
      "lowLatency": true
    }
  }
}
```

#### 3.2.4 可能触发的事件

| Event | 触发条件 | Payload Schema | 客户端处理建议 |
|---|---|---|---|
| `video.ndiConfigChanged` | 该方法导致状态、配置或动作状态实际变化。 | `NdiConfigChangedEvent` | 可直接更新 UI；需要完整状态时调用对应 get method 校准。 |

#### 3.2.5 错误

| 错误 | 场景 | 返回建议 |
|---|---|---|
| `NOT_SUPPORTED` | 设备不支持该 feature、method、target 或 scope。 | 返回 unsupported feature/method/target。 |
| `INVALID_ARGUMENT` | 请求字段非法、枚举非法或范围非法。 | 返回具体字段路径和合法范围。 |
| `PERMISSION_DENIED` | 调用方无权执行该操作。 | 返回权限错误。 |
| `BUSY` | 设备正在处理冲突操作。 | 建议稍后重试。 |

### 3.3 `video.getNdiConfig`

**用途**：查询 NDI 服务配置。

| 项 | 内容 |
|---|---|
| 调用类型 | query |
| Params Schema | `GetNdiConfigParams` |
| Result Schema | `GetNdiConfigResult` |
| 是否触发事件 | 否 |
| 幂等性 / 异步性 | 幂等；同步返回当前快照。 |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_ARGUMENT`, `PERMISSION_DENIED`, `UNAVAILABLE` |

#### 3.3.1 请求参数 Params：`GetNdiConfigParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `target` | string | no | target id | `default` | 示例值 `ndi-source-main`；查询对象。 |

#### 3.3.2 返回结果 Result：`GetNdiConfigResult`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `state` | object | yes | see schema | none | 当前结果对象；示例字段包括 `target`、`sourceName`、`connected`、`bitrateKbps`。 |
| `sampledAt` | string timestamp | no | RFC 3339 | omitted | 结果采样时间；客户端可用于缓存和校准。 |

#### 3.3.3 d block 示例

request:

```json
{
  "id": 103,
  "method": "video.getNdiConfig",
  "params": {
    "target": "ndi-source-main"
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
    "state": {
      "target": "ndi-source-main",
      "sourceName": "AXTP Camera Main",
      "connected": true,
      "bitrateKbps": 12000
    },
    "sampledAt": "2026-06-15T08:00:03Z"
  }
}
```

#### 3.3.4 可能触发的事件

| Event | 触发条件 | Payload Schema | 客户端处理建议 |
|---|---|---|---|
| 无 | query method 不应因查询触发状态变化事件。 | none | 无需处理。 |

#### 3.3.5 错误

| 错误 | 场景 | 返回建议 |
|---|---|---|
| `NOT_SUPPORTED` | 设备不支持该 feature、method、target 或 scope。 | 返回 unsupported feature/method/target。 |
| `INVALID_ARGUMENT` | 请求字段非法、枚举非法或范围非法。 | 返回具体字段路径和合法范围。 |
| `PERMISSION_DENIED` | 调用方无权执行该操作。 | 返回权限错误。 |
| `BUSY` | 设备正在处理冲突操作。 | 建议稍后重试。 |

### 3.4 `video.resetNdiConfig`

**用途**：恢复 NDI 默认配置。

| 项 | 内容 |
|---|---|
| 调用类型 | action |
| Params Schema | `ResetNdiConfigParams` |
| Result Schema | `ResetNdiConfigResult` |
| 是否触发事件 | 是，状态实际变化后触发 `video.ndiConfigChanged`。 |
| 幂等性 / 异步性 | 建议幂等；重复提交相同目标状态应成功，可不重复触发事件。 |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_ARGUMENT`, `INVALID_STATE`, `BUSY`, `PERMISSION_DENIED` |

#### 3.4.1 请求参数 Params：`ResetNdiConfigParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `target` | string | no | target id | `default` | 动作对象；具体 target 集合由 capability 声明。 |
| `reason` | string | no | caller-defined reason | omitted | 调用方给出的动作原因。 |

#### 3.4.2 返回结果 Result：`ResetNdiConfigResult`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `accepted` | boolean | yes | `true`, `false` | none | 设备是否接受动作请求。 |
| `actionId` | string | no | opaque action id | omitted | 动作 ID，用于日志或异步关联。 |

#### 3.4.3 d block 示例

request:

```json
{
  "id": 104,
  "method": "video.resetNdiConfig",
  "params": {
    "target": "ndi-source-main",
    "reason": "ndi_state_update"
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
    "actionId": "video-resetndiconfig-20260615-001"
  }
}
```

#### 3.4.4 可能触发的事件

| Event | 触发条件 | Payload Schema | 客户端处理建议 |
|---|---|---|---|
| `video.ndiConfigChanged` | 该方法导致状态、配置或动作状态实际变化。 | `NdiConfigChangedEvent` | 可直接更新 UI；需要完整状态时调用对应 get method 校准。 |

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
| `video.ndiConfigChanged` | NDI 配置变化 | `NdiConfigChangedEvent` | 更新 UI 或调用对应 get method 校准 | draft |
| `video.ndiStateChanged` | NDI 运行状态变化 | `NdiStateChangedEvent` | 更新 UI 或调用对应 get method 校准 | draft |

### 4.1 `video.ndiConfigChanged`

**触发条件**：NDI 配置变化。

#### 4.1.1 Payload：`NdiConfigChangedEvent`

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
  "event": "video.ndiConfigChanged",
  "intent": 1,
  "data": {
    "changedFields": [
      "connected",
      "bitrateKbps"
    ],
    "state": {
      "target": "ndi-source-main",
      "sourceName": "AXTP Camera Main",
      "groups": [
        "studio"
      ],
      "lowLatency": true
    },
    "source": "remoteApp",
    "reason": "ndi_state_update",
    "stateRevision": 1
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

### 4.2 `video.ndiStateChanged`

**触发条件**：NDI 运行状态变化。

#### 4.2.1 Payload：`NdiStateChangedEvent`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `changedFields` | string[] | no | field path array | omitted | 变化字段路径。 |
| `state` | object | no | see schema | omitted | 变化后的状态、配置或摘要。 |
| `source` | string enum | no | `remoteApp`, `localPanel`, `devicePolicy`, `adapter`, `unknown` | `unknown` | 状态变化来源。 |
| `reason` | string enum | no | feature-specific | `unknown` | 状态变化原因。 |
| `stateRevision` | uint32 | no | monotonic counter | omitted | 状态版本，用于多端同步和去重。 |

#### 4.2.2 d block 示例

```json
{
  "event": "video.ndiStateChanged",
  "intent": 1,
  "data": {
    "changedFields": [
      "connected",
      "bitrateKbps"
    ],
    "state": {
      "target": "ndi-source-main",
      "sourceName": "AXTP Camera Main",
      "groups": [
        "studio"
      ],
      "lowLatency": true
    },
    "source": "remoteApp",
    "reason": "ndi_state_update",
    "stateRevision": 1
  }
}
```

#### 4.2.3 客户端处理建议

| 场景 | 建议 |
|---|---|
| payload 是完整状态 | 可直接更新 UI 或本地缓存。 |
| payload 是变化片段 | 调用对应 get method 校准完整状态。 |
| event 丢失或重连 | 重连后主动调用 get method 校准。 |

#### 4.2.4 规则

- Event MUST 使用 `op=6`。
- Event MUST NOT 携带 `d.id`。
- Event payload MUST 放在 `d.data` 中。

## 5. Capability

Capability name: `video.ndi`。

设备通过 capability 声明是否支持该 feature，以及支持哪些范围、模式、对象或约束。Capability 字段只描述“设备能做什么”，不得混入 method params/result 或 event payload。

| 能力字段 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `capability` | string | yes | fixed `video.ndi` | none | capability 名称。 |
| `supportedTargets` | string[] | no | target id array | omitted | 支持的对象、通道、端口、组件或 scope。 |

## 6. 字段 / Schemas

### 6.1 Schema 层级速览

```text
NdiCapability
  capability / supportedTargets
NdiState
  target / status / sampledAt
NdiChangedEvent
  changedFields / state / source / reason / stateRevision
```

### 6.2 请求与响应 Schemas

| Schema | 用途 | 字段定义 |
|---|---|---|
| `GetNdiCapabilitiesParams` | `video.getNdiCapabilities` request params | 见 `video.getNdiCapabilities` 方法小节。 |
| `GetNdiCapabilitiesResult` | `video.getNdiCapabilities` result | 见 `video.getNdiCapabilities` 方法小节。 |
| `SetNdiConfigParams` | `video.setNdiConfig` request params | 见 `video.setNdiConfig` 方法小节。 |
| `SetNdiConfigResult` | `video.setNdiConfig` result | 见 `video.setNdiConfig` 方法小节。 |
| `GetNdiConfigParams` | `video.getNdiConfig` request params | 见 `video.getNdiConfig` 方法小节。 |
| `GetNdiConfigResult` | `video.getNdiConfig` result | 见 `video.getNdiConfig` 方法小节。 |
| `ResetNdiConfigParams` | `video.resetNdiConfig` request params | 见 `video.resetNdiConfig` 方法小节。 |
| `ResetNdiConfigResult` | `video.resetNdiConfig` result | 见 `video.resetNdiConfig` 方法小节。 |

### 6.3 Capability Schemas


### 6.4 Event Schemas

| Schema | Event | 字段定义 |
|---|---|---|
| `NdiConfigChangedEvent` | `video.ndiConfigChanged` | 见 `video.ndiConfigChanged` 事件小节。 |
| `NdiStateChangedEvent` | `video.ndiStateChanged` | 见 `video.ndiStateChanged` 事件小节。 |