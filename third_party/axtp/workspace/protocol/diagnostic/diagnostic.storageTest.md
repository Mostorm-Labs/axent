---
status: draft
contract: false
generated: false
domain: diagnostic
feature: diagnostic.storageTest
registry:
lastReviewed: 2026-06-15
---

# diagnostic.storageTest

## 0. 速读结论

| 项目 | 内容 |
|---|---|
| 这个能力做什么 | 存储读写、容量和健康测试。 |
| 当前状态 | draft |
| 是否可直接实现 | 否。本文是 protocol draft；正式实现以 registry / generated 为准。 |
| 主要交互 | RPC + EVENT |
| 是否使用 STREAM | 否 |
| Registry readiness | partial |
| Conformance | needed |
| 主要未决问题 | schema 字段、错误模型、legacy 映射和 conformance case 仍需人工确认。 |

## 1. 功能说明

`diagnostic.storageTest` 用于存储读写、容量和健康测试。

## 2. 能力边界

| 类型 | 内容 |
|---|---|
| 包含 | diagnostic.storageTest 的能力发现、配置、状态、动作或事件。 |
| 包含 | 与 diagnostic.storageTest 直接相关的 method/event/schema 草案。 |
| 包含 | 已确认 legacy 协议到 diagnostic.storageTest 的语义归类。 |
| 不包含 | 不承载其他 capability feature 的业务语义；跨域关系通过 schema 字段、引用或数据面 stream/file 表达。 |
| 不包含 | method/event 数值 ID 分配；数值以 contract/registry/generated 为准。 |
| 不包含 | 未确认旧协议 payload 的稳定映射。 |
| 数据面 | 本 feature 默认不定义 STREAM payload，所有操作均通过 RPC method/event 完成。 |

## 3. 方法 Methods

### 3.0 方法速览

| Method | 调用类型 | 用途 | Params Schema | Result Schema | 是否触发事件 | 状态 |
|---|---|---|---|---|---|---|
| `diagnostic.getStorageTestCapabilities` | query | 查询 diagnostic.storageTest 能力范围 | `GetStorageTestCapabilitiesParams` | `GetStorageTestCapabilitiesResult` | 否 | draft |
| `diagnostic.getStorageTestConfig` | query | 查询 diagnostic.storageTest 配置 | `GetStorageTestConfigParams` | `GetStorageTestConfigResult` | 否 | draft |
| `diagnostic.setStorageTestConfig` | command | 设置 diagnostic.storageTest 配置 | `SetStorageTestConfigParams` | `SetStorageTestConfigResult` | 是，`diagnostic.storageTestConfigChanged` | draft |
| `diagnostic.resetStorageTestConfig` | action | 恢复 diagnostic.storageTest 默认配置 | `ResetStorageTestConfigParams` | `ResetStorageTestConfigResult` | 是，`diagnostic.storageTestConfigChanged` | draft |

### 3.1 `diagnostic.getStorageTestCapabilities`

**用途**：查询 diagnostic.storageTest 能力范围。

| 项 | 内容 |
|---|---|
| 调用类型 | query |
| Params Schema | `GetStorageTestCapabilitiesParams` |
| Result Schema | `GetStorageTestCapabilitiesResult` |
| 是否触发事件 | 否 |
| 幂等性 / 异步性 | 幂等；同步返回当前快照。 |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_ARGUMENT`, `PERMISSION_DENIED`, `UNAVAILABLE` |

#### 3.1.1 请求参数 Params：`GetStorageTestCapabilitiesParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `target` | string | no | target id | `default` | 示例值 `storage-internal`；查询对象。 |

#### 3.1.2 返回结果 Result：`GetStorageTestCapabilitiesResult`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `state` | object | yes | see schema | none | 当前结果对象；示例字段包括 `target`、`supportedTests`、`maxDurationSeconds`、`reportSupported`。 |
| `sampledAt` | string timestamp | no | RFC 3339 | omitted | 结果采样时间；客户端可用于缓存和校准。 |

#### 3.1.3 d block 示例

request:

```json
{
  "id": 101,
  "method": "diagnostic.getStorageTestCapabilities",
  "params": {
    "target": "storage-internal"
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
      "target": "storage-internal",
      "supportedTests": [
        "readWrite",
        "capacity",
        "wearLevel"
      ],
      "maxDurationSeconds": 300,
      "reportSupported": true
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

### 3.2 `diagnostic.getStorageTestConfig`

**用途**：查询 diagnostic.storageTest 配置。

| 项 | 内容 |
|---|---|
| 调用类型 | query |
| Params Schema | `GetStorageTestConfigParams` |
| Result Schema | `GetStorageTestConfigResult` |
| 是否触发事件 | 否 |
| 幂等性 / 异步性 | 幂等；同步返回当前快照。 |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_ARGUMENT`, `PERMISSION_DENIED`, `UNAVAILABLE` |

#### 3.2.1 请求参数 Params：`GetStorageTestConfigParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `target` | string | no | target id | `default` | 示例值 `storage-internal`；查询对象。 |

#### 3.2.2 返回结果 Result：`GetStorageTestConfigResult`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `state` | object | yes | see schema | none | 当前结果对象；示例字段包括 `target`、`lastRunId`、`lastResult`、`completedAt`。 |
| `sampledAt` | string timestamp | no | RFC 3339 | omitted | 结果采样时间；客户端可用于缓存和校准。 |

#### 3.2.3 d block 示例

request:

```json
{
  "id": 102,
  "method": "diagnostic.getStorageTestConfig",
  "params": {
    "target": "storage-internal"
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
      "target": "storage-internal",
      "lastRunId": "storageTest-run-20260615-001",
      "lastResult": "passed",
      "completedAt": "2026-06-15T08:00:00Z"
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

### 3.3 `diagnostic.setStorageTestConfig`

**用途**：设置 diagnostic.storageTest 配置。

| 项 | 内容 |
|---|---|
| 调用类型 | command |
| Params Schema | `SetStorageTestConfigParams` |
| Result Schema | `SetStorageTestConfigResult` |
| 是否触发事件 | 是，状态实际变化后触发 `diagnostic.storageTestConfigChanged`。 |
| 幂等性 / 异步性 | 建议幂等；重复提交相同目标状态应成功，可不重复触发事件。 |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_ARGUMENT`, `INVALID_STATE`, `BUSY`, `PERMISSION_DENIED` |

#### 3.3.1 请求参数 Params：`SetStorageTestConfigParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `target` | string | no | target id | `default` | 设置对象；具体 target 集合由 capability 声明。 |
| `config` | object | yes | see schema | none | 目标配置或状态片段；字段需在采纳前确认。 |

#### 3.3.2 返回结果 Result：`SetStorageTestConfigResult`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `accepted` | boolean | yes | `true`, `false` | none | 设备是否接受并应用请求。 |
| `state` | object | no | see schema | omitted | 设置后的状态或配置快照。 |

#### 3.3.3 d block 示例

request:

```json
{
  "id": 103,
  "method": "diagnostic.setStorageTestConfig",
  "params": {
    "target": "storage-internal",
    "config": {
      "enabledTests": [
        "readWrite",
        "capacity"
      ],
      "stopOnFailure": false,
      "reportLevel": "summary"
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
      "target": "storage-internal",
      "enabledTests": [
        "readWrite",
        "capacity"
      ],
      "stopOnFailure": false,
      "reportLevel": "summary"
    }
  }
}
```

#### 3.3.4 可能触发的事件

| Event | 触发条件 | Payload Schema | 客户端处理建议 |
|---|---|---|---|
| `diagnostic.storageTestConfigChanged` | 该方法导致状态、配置或动作状态实际变化。 | `StorageTestConfigChangedEvent` | 可直接更新 UI；需要完整状态时调用对应 get method 校准。 |

#### 3.3.5 错误

| 错误 | 场景 | 返回建议 |
|---|---|---|
| `NOT_SUPPORTED` | 设备不支持该 feature、method、target 或 scope。 | 返回 unsupported feature/method/target。 |
| `INVALID_ARGUMENT` | 请求字段非法、枚举非法或范围非法。 | 返回具体字段路径和合法范围。 |
| `PERMISSION_DENIED` | 调用方无权执行该操作。 | 返回权限错误。 |
| `BUSY` | 设备正在处理冲突操作。 | 建议稍后重试。 |

### 3.4 `diagnostic.resetStorageTestConfig`

**用途**：恢复 diagnostic.storageTest 默认配置。

| 项 | 内容 |
|---|---|
| 调用类型 | action |
| Params Schema | `ResetStorageTestConfigParams` |
| Result Schema | `ResetStorageTestConfigResult` |
| 是否触发事件 | 是，状态实际变化后触发 `diagnostic.storageTestConfigChanged`。 |
| 幂等性 / 异步性 | 建议幂等；重复提交相同目标状态应成功，可不重复触发事件。 |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_ARGUMENT`, `INVALID_STATE`, `BUSY`, `PERMISSION_DENIED` |

#### 3.4.1 请求参数 Params：`ResetStorageTestConfigParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `target` | string | no | target id | `default` | 动作对象；具体 target 集合由 capability 声明。 |
| `reason` | string | no | caller-defined reason | omitted | 调用方给出的动作原因。 |

#### 3.4.2 返回结果 Result：`ResetStorageTestConfigResult`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `accepted` | boolean | yes | `true`, `false` | none | 设备是否接受动作请求。 |
| `actionId` | string | no | opaque action id | omitted | 动作 ID，用于日志或异步关联。 |

#### 3.4.3 d block 示例

request:

```json
{
  "id": 104,
  "method": "diagnostic.resetStorageTestConfig",
  "params": {
    "target": "storage-internal",
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
    "actionId": "diagnostic-resetstoragetestconfig-20260615-001"
  }
}
```

#### 3.4.4 可能触发的事件

| Event | 触发条件 | Payload Schema | 客户端处理建议 |
|---|---|---|---|
| `diagnostic.storageTestConfigChanged` | 该方法导致状态、配置或动作状态实际变化。 | `StorageTestConfigChangedEvent` | 可直接更新 UI；需要完整状态时调用对应 get method 校准。 |

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
| `diagnostic.storageTestConfigChanged` | diagnostic.storageTest 配置变化 | `StorageTestConfigChangedEvent` | 更新 UI 或调用对应 get method 校准 | draft |

### 4.1 `diagnostic.storageTestConfigChanged`

**触发条件**：diagnostic.storageTest 配置变化。

#### 4.1.1 Payload：`StorageTestConfigChangedEvent`

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
  "event": "diagnostic.storageTestConfigChanged",
  "intent": 1,
  "data": {
    "changedFields": [
      "lastResult"
    ],
    "state": {
      "target": "storage-internal",
      "enabledTests": [
        "readWrite",
        "capacity"
      ],
      "stopOnFailure": false,
      "reportLevel": "summary"
    },
    "source": "remoteApp",
    "reason": "test_completed",
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

Capability name: `diagnostic.storageTest`。

设备通过 capability 声明是否支持该 feature，以及支持哪些范围、模式、对象或约束。Capability 字段只描述“设备能做什么”，不得混入 method params/result 或 event payload。

| 能力字段 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `capability` | string | yes | fixed `diagnostic.storageTest` | none | capability 名称。 |
| `supportedTargets` | string[] | no | target id array | omitted | 支持的对象、通道、端口、组件或 scope。 |

## 6. 字段 / Schemas

### 6.1 Schema 层级速览

```text
StorageTestCapability
  capability / supportedTargets
StorageTestState
  target / status / sampledAt
StorageTestChangedEvent
  changedFields / state / source / reason / stateRevision
```

### 6.2 请求与响应 Schemas

| Schema | 用途 | 字段定义 |
|---|---|---|
| `GetStorageTestCapabilitiesParams` | `diagnostic.getStorageTestCapabilities` request params | 见 `diagnostic.getStorageTestCapabilities` 方法小节。 |
| `GetStorageTestCapabilitiesResult` | `diagnostic.getStorageTestCapabilities` result | 见 `diagnostic.getStorageTestCapabilities` 方法小节。 |
| `GetStorageTestConfigParams` | `diagnostic.getStorageTestConfig` request params | 见 `diagnostic.getStorageTestConfig` 方法小节。 |
| `GetStorageTestConfigResult` | `diagnostic.getStorageTestConfig` result | 见 `diagnostic.getStorageTestConfig` 方法小节。 |
| `SetStorageTestConfigParams` | `diagnostic.setStorageTestConfig` request params | 见 `diagnostic.setStorageTestConfig` 方法小节。 |
| `SetStorageTestConfigResult` | `diagnostic.setStorageTestConfig` result | 见 `diagnostic.setStorageTestConfig` 方法小节。 |
| `ResetStorageTestConfigParams` | `diagnostic.resetStorageTestConfig` request params | 见 `diagnostic.resetStorageTestConfig` 方法小节。 |
| `ResetStorageTestConfigResult` | `diagnostic.resetStorageTestConfig` result | 见 `diagnostic.resetStorageTestConfig` 方法小节。 |

### 6.3 Capability Schemas


### 6.4 Event Schemas

| Schema | Event | 字段定义 |
|---|---|---|
| `StorageTestConfigChangedEvent` | `diagnostic.storageTestConfigChanged` | 见 `diagnostic.storageTestConfigChanged` 事件小节。 |