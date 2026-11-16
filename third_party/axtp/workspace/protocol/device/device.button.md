---
status: draft
contract: false
generated: false
domain: device
feature: device.button
registry:
lastReviewed: 2026-06-15
---

# device.button

## 0. 速读结论

| 项目 | 内容 |
|---|---|
| 这个能力做什么 | 设备本地按键、按键配置和按键事件。 |
| 当前状态 | draft |
| 是否可直接实现 | 否。本文是 protocol draft；正式实现以 registry / generated 为准。 |
| 主要交互 | RPC + EVENT |
| 是否使用 STREAM | 否 |
| Registry readiness | partial |
| Conformance | needed |
| 主要未决问题 | schema 字段、错误模型、legacy 映射和 conformance case 仍需人工确认。 |

## 1. 功能说明

`device.button` 用于设备本地按键、按键配置和按键事件。

## 2. 能力边界

| 类型 | 内容 |
|---|---|
| 包含 | device.button 的能力发现、配置、状态、动作或事件。 |
| 包含 | 与 device.button 直接相关的 method/event/schema 草案。 |
| 包含 | 已确认 legacy 协议到 device.button 的语义归类。 |
| 不包含 | 不承载其他 capability feature 的业务语义；跨域关系通过 schema 字段、引用或数据面 stream/file 表达。 |
| 不包含 | method/event 数值 ID 分配；数值以 contract/registry/generated 为准。 |
| 不包含 | 未确认旧协议 payload 的稳定映射。 |
| 数据面 | 本 feature 默认不定义 STREAM payload，所有操作均通过 RPC method/event 完成。 |

## 3. 方法 Methods

### 3.0 方法速览

| Method | 调用类型 | 用途 | Params Schema | Result Schema | 是否触发事件 | 状态 |
|---|---|---|---|---|---|---|
| `device.getButtonCapabilities` | query | 查询 device.button 能力范围 | `GetButtonCapabilitiesParams` | `GetButtonCapabilitiesResult` | 否 | draft |
| `device.getButtonConfig` | query | 查询 device.button 配置 | `GetButtonConfigParams` | `GetButtonConfigResult` | 否 | draft |
| `device.setButtonConfig` | command | 设置 device.button 配置 | `SetButtonConfigParams` | `SetButtonConfigResult` | 是，`device.buttonConfigChanged` | draft |
| `device.resetButtonConfig` | action | 恢复 device.button 默认配置 | `ResetButtonConfigParams` | `ResetButtonConfigResult` | 是，`device.buttonConfigChanged` | draft |

### 3.1 `device.getButtonCapabilities`

**用途**：查询 device.button 能力范围。

| 项 | 内容 |
|---|---|
| 调用类型 | query |
| Params Schema | `GetButtonCapabilitiesParams` |
| Result Schema | `GetButtonCapabilitiesResult` |
| 是否触发事件 | 否 |
| 幂等性 / 异步性 | 幂等；同步返回当前快照。 |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_ARGUMENT`, `PERMISSION_DENIED`, `UNAVAILABLE` |

#### 3.1.1 请求参数 Params：`GetButtonCapabilitiesParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `target` | string | no | target id | `default` | 示例值 `front-panel`；查询对象。 |

#### 3.1.2 返回结果 Result：`GetButtonCapabilitiesResult`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `state` | object | yes | see schema | none | 当前结果对象；示例字段包括 `target`、`buttonIds`、`supportedGestures`、`remappable`。 |
| `sampledAt` | string timestamp | no | RFC 3339 | omitted | 结果采样时间；客户端可用于缓存和校准。 |

#### 3.1.3 d block 示例

request:

```json
{
  "id": 101,
  "method": "device.getButtonCapabilities",
  "params": {
    "target": "front-panel"
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
      "target": "front-panel",
      "buttonIds": [
        "power",
        "privacy",
        "preset1"
      ],
      "supportedGestures": [
        "press",
        "longPress",
        "doublePress"
      ],
      "remappable": true
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

### 3.2 `device.getButtonConfig`

**用途**：查询 device.button 配置。

| 项 | 内容 |
|---|---|
| 调用类型 | query |
| Params Schema | `GetButtonConfigParams` |
| Result Schema | `GetButtonConfigResult` |
| 是否触发事件 | 否 |
| 幂等性 / 异步性 | 幂等；同步返回当前快照。 |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_ARGUMENT`, `PERMISSION_DENIED`, `UNAVAILABLE` |

#### 3.2.1 请求参数 Params：`GetButtonConfigParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `target` | string | no | target id | `default` | 示例值 `front-panel`；查询对象。 |

#### 3.2.2 返回结果 Result：`GetButtonConfigResult`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `state` | object | yes | see schema | none | 当前结果对象；示例字段包括 `target`、`bindings`、`lockout`。 |
| `sampledAt` | string timestamp | no | RFC 3339 | omitted | 结果采样时间；客户端可用于缓存和校准。 |

#### 3.2.3 d block 示例

request:

```json
{
  "id": 102,
  "method": "device.getButtonConfig",
  "params": {
    "target": "front-panel"
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
      "target": "front-panel",
      "bindings": [
        {
          "buttonId": "privacy",
          "gesture": "press",
          "action": "privacy.toggleCover"
        }
      ],
      "lockout": false
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

### 3.3 `device.setButtonConfig`

**用途**：设置 device.button 配置。

| 项 | 内容 |
|---|---|
| 调用类型 | command |
| Params Schema | `SetButtonConfigParams` |
| Result Schema | `SetButtonConfigResult` |
| 是否触发事件 | 是，状态实际变化后触发 `device.buttonConfigChanged`。 |
| 幂等性 / 异步性 | 建议幂等；重复提交相同目标状态应成功，可不重复触发事件。 |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_ARGUMENT`, `INVALID_STATE`, `BUSY`, `PERMISSION_DENIED` |

#### 3.3.1 请求参数 Params：`SetButtonConfigParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `target` | string | no | target id | `default` | 设置对象；具体 target 集合由 capability 声明。 |
| `config` | object | yes | see schema | none | 目标配置或状态片段；字段需在采纳前确认。 |

#### 3.3.2 返回结果 Result：`SetButtonConfigResult`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `accepted` | boolean | yes | `true`, `false` | none | 设备是否接受并应用请求。 |
| `state` | object | no | see schema | omitted | 设置后的状态或配置快照。 |

#### 3.3.3 d block 示例

request:

```json
{
  "id": 103,
  "method": "device.setButtonConfig",
  "params": {
    "target": "front-panel",
    "config": {
      "bindings": [
        {
          "buttonId": "privacy",
          "gesture": "press",
          "action": "privacy.toggleCover"
        }
      ],
      "debounceMs": 40,
      "longPressMs": 800
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
      "target": "front-panel",
      "bindings": [
        {
          "buttonId": "privacy",
          "gesture": "press",
          "action": "privacy.toggleCover"
        }
      ],
      "debounceMs": 40,
      "longPressMs": 800
    }
  }
}
```

#### 3.3.4 可能触发的事件

| Event | 触发条件 | Payload Schema | 客户端处理建议 |
|---|---|---|---|
| `device.buttonConfigChanged` | 该方法导致状态、配置或动作状态实际变化。 | `ButtonConfigChangedEvent` | 可直接更新 UI；需要完整状态时调用对应 get method 校准。 |

#### 3.3.5 错误

| 错误 | 场景 | 返回建议 |
|---|---|---|
| `NOT_SUPPORTED` | 设备不支持该 feature、method、target 或 scope。 | 返回 unsupported feature/method/target。 |
| `INVALID_ARGUMENT` | 请求字段非法、枚举非法或范围非法。 | 返回具体字段路径和合法范围。 |
| `PERMISSION_DENIED` | 调用方无权执行该操作。 | 返回权限错误。 |
| `BUSY` | 设备正在处理冲突操作。 | 建议稍后重试。 |

### 3.4 `device.resetButtonConfig`

**用途**：恢复 device.button 默认配置。

| 项 | 内容 |
|---|---|
| 调用类型 | action |
| Params Schema | `ResetButtonConfigParams` |
| Result Schema | `ResetButtonConfigResult` |
| 是否触发事件 | 是，状态实际变化后触发 `device.buttonConfigChanged`。 |
| 幂等性 / 异步性 | 建议幂等；重复提交相同目标状态应成功，可不重复触发事件。 |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_ARGUMENT`, `INVALID_STATE`, `BUSY`, `PERMISSION_DENIED` |

#### 3.4.1 请求参数 Params：`ResetButtonConfigParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `target` | string | no | target id | `default` | 动作对象；具体 target 集合由 capability 声明。 |
| `reason` | string | no | caller-defined reason | omitted | 调用方给出的动作原因。 |

#### 3.4.2 返回结果 Result：`ResetButtonConfigResult`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `accepted` | boolean | yes | `true`, `false` | none | 设备是否接受动作请求。 |
| `actionId` | string | no | opaque action id | omitted | 动作 ID，用于日志或异步关联。 |

#### 3.4.3 d block 示例

request:

```json
{
  "id": 104,
  "method": "device.resetButtonConfig",
  "params": {
    "target": "front-panel",
    "reason": "restore_default_config"
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
    "actionId": "device-resetbuttonconfig-20260615-001"
  }
}
```

#### 3.4.4 可能触发的事件

| Event | 触发条件 | Payload Schema | 客户端处理建议 |
|---|---|---|---|
| `device.buttonConfigChanged` | 该方法导致状态、配置或动作状态实际变化。 | `ButtonConfigChangedEvent` | 可直接更新 UI；需要完整状态时调用对应 get method 校准。 |

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
| `device.buttonConfigChanged` | device.button 配置变化 | `ButtonConfigChangedEvent` | 更新 UI 或调用对应 get method 校准 | draft |

### 4.1 `device.buttonConfigChanged`

**触发条件**：device.button 配置变化。

#### 4.1.1 Payload：`ButtonConfigChangedEvent`

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
  "event": "device.buttonConfigChanged",
  "intent": 1,
  "data": {
    "changedFields": [
      "bindings.privacy.action"
    ],
    "state": {
      "target": "front-panel",
      "bindings": [
        {
          "buttonId": "privacy",
          "gesture": "press",
          "action": "privacy.toggleCover"
        }
      ],
      "debounceMs": 40,
      "longPressMs": 800
    },
    "source": "remoteApp",
    "reason": "user_mapping_update",
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

## 5. Capability

Capability name: `device.button`。

设备通过 capability 声明是否支持该 feature，以及支持哪些范围、模式、对象或约束。Capability 字段只描述“设备能做什么”，不得混入 method params/result 或 event payload。

| 能力字段 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `capability` | string | yes | fixed `device.button` | none | capability 名称。 |
| `supportedTargets` | string[] | no | target id array | omitted | 支持的对象、通道、端口、组件或 scope。 |

## 6. 字段 / Schemas

### 6.1 Schema 层级速览

```text
ButtonCapability
  capability / supportedTargets
ButtonState
  target / status / sampledAt
ButtonChangedEvent
  changedFields / state / source / reason / stateRevision
```

### 6.2 请求与响应 Schemas

| Schema | 用途 | 字段定义 |
|---|---|---|
| `GetButtonCapabilitiesParams` | `device.getButtonCapabilities` request params | 见 `device.getButtonCapabilities` 方法小节。 |
| `GetButtonCapabilitiesResult` | `device.getButtonCapabilities` result | 见 `device.getButtonCapabilities` 方法小节。 |
| `GetButtonConfigParams` | `device.getButtonConfig` request params | 见 `device.getButtonConfig` 方法小节。 |
| `GetButtonConfigResult` | `device.getButtonConfig` result | 见 `device.getButtonConfig` 方法小节。 |
| `SetButtonConfigParams` | `device.setButtonConfig` request params | 见 `device.setButtonConfig` 方法小节。 |
| `SetButtonConfigResult` | `device.setButtonConfig` result | 见 `device.setButtonConfig` 方法小节。 |
| `ResetButtonConfigParams` | `device.resetButtonConfig` request params | 见 `device.resetButtonConfig` 方法小节。 |
| `ResetButtonConfigResult` | `device.resetButtonConfig` result | 见 `device.resetButtonConfig` 方法小节。 |

### 6.3 Capability Schemas


### 6.4 Event Schemas

| Schema | Event | 字段定义 |
|---|---|---|
| `ButtonConfigChangedEvent` | `device.buttonConfigChanged` | 见 `device.buttonConfigChanged` 事件小节。 |