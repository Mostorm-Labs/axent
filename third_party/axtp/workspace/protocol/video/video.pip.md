---
status: draft
contract: false
generated: false
domain: video
feature: video.pip
registry:
lastReviewed: 2026-06-15
---

# video.pip

## 0. 速读结论

| 项目 | 内容 |
|---|---|
| 这个能力做什么 | 画中画主画面、小窗、位置和 PIP 配置。 |
| 当前状态 | draft |
| 是否可直接实现 | 否。本文是 protocol draft；正式实现以 registry / generated 为准。 |
| 主要交互 | RPC + EVENT |
| 是否使用 STREAM | 否 |
| Registry readiness | partial |
| Conformance | needed |
| 主要未决问题 | schema 字段、错误模型、legacy 映射和 conformance case 仍需人工确认。 |

## 1. 功能说明

`video.pip` 用于画中画主画面、小窗、位置和 PIP 配置。

## 2. 能力边界

| 类型 | 内容 |
|---|---|
| 包含 | video.pip 的能力发现、配置、状态、动作或事件。 |
| 包含 | 与 video.pip 直接相关的 method/event/schema 草案。 |
| 包含 | 已确认 legacy 协议到 video.pip 的语义归类。 |
| 不包含 | 不承载其他 capability feature 的业务语义；跨域关系通过 schema 字段、引用或数据面 stream/file 表达。 |
| 不包含 | method/event 数值 ID 分配；数值以 contract/registry/generated 为准。 |
| 不包含 | 未确认旧协议 payload 的稳定映射。 |
| 数据面 | 本 feature 默认不定义 STREAM payload，所有操作均通过 RPC method/event 完成。 |

## 3. 方法 Methods

### 3.0 方法速览

| Method | 调用类型 | 用途 | Params Schema | Result Schema | 是否触发事件 | 状态 |
|---|---|---|---|---|---|---|
| `video.getPipCapabilities` | query | 查询 PIP 能力 | `GetPipCapabilitiesParams` | `GetPipCapabilitiesResult` | 否 | draft |
| `video.setPipConfig` | command | 设置 PIP 主画面、小窗和位置 | `SetPipConfigParams` | `SetPipConfigResult` | 是，`video.pipConfigChanged` | draft |
| `video.getPipConfig` | query | 查询 PIP 配置 | `GetPipConfigParams` | `GetPipConfigResult` | 否 | draft |
| `video.resetPipConfig` | action | 恢复 PIP 默认值 | `ResetPipConfigParams` | `ResetPipConfigResult` | 是，`video.pipConfigChanged` | draft |

### 3.1 `video.getPipCapabilities`

**用途**：查询 PIP 能力。

| 项 | 内容 |
|---|---|
| 调用类型 | query |
| Params Schema | `GetPipCapabilitiesParams` |
| Result Schema | `GetPipCapabilitiesResult` |
| 是否触发事件 | 否 |
| 幂等性 / 异步性 | 幂等；同步返回当前快照。 |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_ARGUMENT`, `PERMISSION_DENIED`, `UNAVAILABLE` |

#### 3.1.1 请求参数 Params：`GetPipCapabilitiesParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `target` | string | no | target id | `default` | 示例值 `program`；查询对象。 |

#### 3.1.2 返回结果 Result：`GetPipCapabilitiesResult`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `state` | object | yes | see schema | none | 当前结果对象；示例字段包括 `target`、`enabled`、`sourceId`、`position`。 |
| `sampledAt` | string timestamp | no | RFC 3339 | omitted | 结果采样时间；客户端可用于缓存和校准。 |

#### 3.1.3 d block 示例

request:

```json
{
  "id": 101,
  "method": "video.getPipCapabilities",
  "params": {
    "target": "program"
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
    "capability": "video.pip",
    "maxPipWindows": 2,
    "positions": [
      "topLeft",
      "topRight",
      "bottomLeft",
      "bottomRight"
    ],
    "minSizePercent": 10,
    "maxSizePercent": 50
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

### 3.2 `video.setPipConfig`

**用途**：设置 PIP 主画面、小窗和位置。

| 项 | 内容 |
|---|---|
| 调用类型 | command |
| Params Schema | `SetPipConfigParams` |
| Result Schema | `SetPipConfigResult` |
| 是否触发事件 | 是，状态实际变化后触发 `video.pipConfigChanged`。 |
| 幂等性 / 异步性 | 建议幂等；重复提交相同目标状态应成功，可不重复触发事件。 |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_ARGUMENT`, `INVALID_STATE`, `BUSY`, `PERMISSION_DENIED` |

#### 3.2.1 请求参数 Params：`SetPipConfigParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `target` | string | no | target id | `default` | 设置对象；具体 target 集合由 capability 声明。 |
| `config` | object | yes | see schema | none | 目标配置或状态片段；字段需在采纳前确认。 |

#### 3.2.2 返回结果 Result：`SetPipConfigResult`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `accepted` | boolean | yes | `true`, `false` | none | 设备是否接受并应用请求。 |
| `state` | object | no | see schema | omitted | 设置后的状态或配置快照。 |

#### 3.2.3 d block 示例

request:

```json
{
  "id": 102,
  "method": "video.setPipConfig",
  "params": {
    "target": "program",
    "config": {
      "enabled": true,
      "sourceId": "camera-2",
      "position": "topRight",
      "widthPercent": 25,
      "heightPercent": 25
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
      "target": "program",
      "enabled": true,
      "sourceId": "camera-2",
      "position": "topRight"
    }
  }
}
```

#### 3.2.4 可能触发的事件

| Event | 触发条件 | Payload Schema | 客户端处理建议 |
|---|---|---|---|
| `video.pipConfigChanged` | 该方法导致状态、配置或动作状态实际变化。 | `PipConfigChangedEvent` | 可直接更新 UI；需要完整状态时调用对应 get method 校准。 |

#### 3.2.5 错误

| 错误 | 场景 | 返回建议 |
|---|---|---|
| `NOT_SUPPORTED` | 设备不支持该 feature、method、target 或 scope。 | 返回 unsupported feature/method/target。 |
| `INVALID_ARGUMENT` | 请求字段非法、枚举非法或范围非法。 | 返回具体字段路径和合法范围。 |
| `PERMISSION_DENIED` | 调用方无权执行该操作。 | 返回权限错误。 |
| `BUSY` | 设备正在处理冲突操作。 | 建议稍后重试。 |

### 3.3 `video.getPipConfig`

**用途**：查询 PIP 配置。

| 项 | 内容 |
|---|---|
| 调用类型 | query |
| Params Schema | `GetPipConfigParams` |
| Result Schema | `GetPipConfigResult` |
| 是否触发事件 | 否 |
| 幂等性 / 异步性 | 幂等；同步返回当前快照。 |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_ARGUMENT`, `PERMISSION_DENIED`, `UNAVAILABLE` |

#### 3.3.1 请求参数 Params：`GetPipConfigParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `target` | string | no | target id | `default` | 示例值 `program`；查询对象。 |

#### 3.3.2 返回结果 Result：`GetPipConfigResult`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `target` | string | yes | see example | none | 查询对象。 |
| `enabled` | bool | yes | see example | none | 是否启用。 |
| `sourceId` | string | yes | see example | none | 标识符。 |
| `position` | string | yes | see example | none | PIP 窗口位置。 |
| `widthPercent` | uint32 | yes | see example | none | 窗口宽度百分比。 |
| `heightPercent` | uint32 | yes | see example | none | 窗口高度百分比。 |
| `sampledAt` | string timestamp | no | RFC 3339 | omitted | 结果采样时间；客户端可用于缓存和校准。 |

#### 3.3.3 d block 示例

request:

```json
{
  "id": 103,
  "method": "video.getPipConfig",
  "params": {
    "target": "program"
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
    "target": "program",
    "enabled": true,
    "sourceId": "camera-2",
    "position": "topRight",
    "widthPercent": 25,
    "heightPercent": 25
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

### 3.4 `video.resetPipConfig`

**用途**：恢复 PIP 默认值。

| 项 | 内容 |
|---|---|
| 调用类型 | action |
| Params Schema | `ResetPipConfigParams` |
| Result Schema | `ResetPipConfigResult` |
| 是否触发事件 | 是，状态实际变化后触发 `video.pipConfigChanged`。 |
| 幂等性 / 异步性 | 建议幂等；重复提交相同目标状态应成功，可不重复触发事件。 |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_ARGUMENT`, `INVALID_STATE`, `BUSY`, `PERMISSION_DENIED` |

#### 3.4.1 请求参数 Params：`ResetPipConfigParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `target` | string | no | target id | `default` | 动作对象；具体 target 集合由 capability 声明。 |
| `reason` | string | no | caller-defined reason | omitted | 调用方给出的动作原因。 |

#### 3.4.2 返回结果 Result：`ResetPipConfigResult`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `accepted` | boolean | yes | `true`, `false` | none | 设备是否接受动作请求。 |
| `actionId` | string | no | opaque action id | omitted | 动作 ID，用于日志或异步关联。 |

#### 3.4.3 d block 示例

request:

```json
{
  "id": 104,
  "method": "video.resetPipConfig",
  "params": {
    "target": "program",
    "reason": "disable_pip"
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
    "actionId": "reset-pip-program"
  }
}
```

#### 3.4.4 可能触发的事件

| Event | 触发条件 | Payload Schema | 客户端处理建议 |
|---|---|---|---|
| `video.pipConfigChanged` | 该方法导致状态、配置或动作状态实际变化。 | `PipConfigChangedEvent` | 可直接更新 UI；需要完整状态时调用对应 get method 校准。 |

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
| `video.pipConfigChanged` | PIP 配置变化 | `PipConfigChangedEvent` | 更新 UI 或调用对应 get method 校准 | draft |

### 4.1 `video.pipConfigChanged`

**触发条件**：PIP 配置变化。

#### 4.1.1 Payload：`PipConfigChangedEvent`

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
  "event": "video.pipConfigChanged",
  "intent": 1,
  "data": {
    "changedFields": [
      "enabled",
      "position"
    ],
    "state": {
      "target": "program",
      "enabled": true,
      "sourceId": "camera-2",
      "position": "topRight"
    },
    "source": "remoteApp",
    "reason": "layout_update",
    "stateRevision": 9
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

Capability name: `video.pip`。

设备通过 capability 声明是否支持该 feature，以及支持哪些范围、模式、对象或约束。Capability 字段只描述“设备能做什么”，不得混入 method params/result 或 event payload。

| 能力字段 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `capability` | string | yes | fixed `video.pip` | none | capability 名称。 |
| `supportedTargets` | string[] | no | target id array | omitted | 支持的对象、通道、端口、组件或 scope。 |

## 6. 字段 / Schemas

### 6.1 Schema 层级速览

```text
PipCapability
  capability / supportedTargets
PipState
  target / status / sampledAt
PipChangedEvent
  changedFields / state / source / reason / stateRevision
```

### 6.2 请求与响应 Schemas

| Schema | 用途 | 字段定义 |
|---|---|---|
| `GetPipCapabilitiesParams` | `video.getPipCapabilities` request params | 见 `video.getPipCapabilities` 方法小节。 |
| `GetPipCapabilitiesResult` | `video.getPipCapabilities` result | 见 `video.getPipCapabilities` 方法小节。 |
| `SetPipConfigParams` | `video.setPipConfig` request params | 见 `video.setPipConfig` 方法小节。 |
| `SetPipConfigResult` | `video.setPipConfig` result | 见 `video.setPipConfig` 方法小节。 |
| `GetPipConfigParams` | `video.getPipConfig` request params | 见 `video.getPipConfig` 方法小节。 |
| `GetPipConfigResult` | `video.getPipConfig` result | 见 `video.getPipConfig` 方法小节。 |
| `ResetPipConfigParams` | `video.resetPipConfig` request params | 见 `video.resetPipConfig` 方法小节。 |
| `ResetPipConfigResult` | `video.resetPipConfig` result | 见 `video.resetPipConfig` 方法小节。 |

### 6.3 Capability Schemas


### 6.4 Event Schemas

| Schema | Event | 字段定义 |
|---|---|---|
| `PipConfigChangedEvent` | `video.pipConfigChanged` | 见 `video.pipConfigChanged` 事件小节。 |