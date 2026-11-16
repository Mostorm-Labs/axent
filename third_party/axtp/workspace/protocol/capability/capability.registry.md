---
status: draft
contract: false
generated: false
domain: capability
feature: capability.registry
registry:
lastReviewed: 2026-06-15
---

# capability.registry

## 0. 速读结论

| 项目 | 内容 |
|---|---|
| 这个能力做什么 | 运行时能力注册表查询、domain.feature 建模发现、supported methods/events 查询、权限/可用性摘要，以及能力变化通知。 |
| 当前状态 | draft |
| 是否可直接实现 | 否。本文是 protocol draft；正式实现以 registry / generated 为准。 |
| 主要交互 | RPC + EVENT |
| 是否使用 STREAM | 否 |
| Registry readiness | partial |
| Conformance | needed |
| 主要未决问题 | schema 字段、错误模型、legacy 映射和 conformance case 仍需人工确认。 |

## 1. 功能说明

`capability.registry` 用于运行时能力注册表查询、domain.feature 建模发现、supported methods/events 查询、权限/可用性摘要，以及能力变化通知。

## 2. 能力边界

| 类型 | 内容 |
|---|---|
| 包含 | capability.registry 的能力发现、状态查询、配置或动作控制。 |
| 包含 | 与 capability.registry 直接相关的 method/event/schema 草案。 |
| 不包含 | 不承载其他 capability feature 的业务语义；跨域关系通过 schema 字段、引用或数据面 stream/file 表达。 |
| 不包含 | method/event 数值 ID 分配；数值以 contract/registry/generated 为准。 |
| 数据面 | 本 feature 默认不定义 STREAM payload，所有操作均通过 RPC method/event 完成。 |

## 3. 方法 Methods

### 3.0 方法速览

| Method | 调用类型 | 用途 | Params Schema | Result Schema | 是否触发事件 | 状态 |
|---|---|---|---|---|---|---|
| `device.getInfo` | query | 查询设备基础信息摘要 | `GetInfoParams` | `GetInfoResult` | 否 | draft |
| `capability.getRegistry` | query | 查询完整 capability registry 摘要 | `GetCapabilityRegistryParams` | `CapabilityRegistry` | 否 | draft |
| `capability.getDomainRegistry` | query | 查询指定 domain 的 capability registry | `GetDomainRegistryParams` | `CapabilityRegistry` | 否 | draft |
| `capability.getFeatureRegistry` | query | 查询指定 feature 的 registry 详情 | `GetFeatureRegistryParams` | `GetFeatureRegistryResult` | 否 | draft |

### 3.1 `device.getInfo`

**用途**：查询设备基础信息摘要。

| 项 | 内容 |
|---|---|
| 调用类型 | query |
| Params Schema | `GetInfoParams` |
| Result Schema | `GetInfoResult` |
| 是否触发事件 | 否 |
| 幂等性 / 异步性 | 幂等；同步返回当前快照。 |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_ARGUMENT`, `PERMISSION_DENIED`, `UNAVAILABLE` |

#### 3.1.1 请求参数 Params：`GetInfoParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `target` | string | no | target id | `default` | 示例值 `runtime`；查询对象。 |

#### 3.1.2 返回结果 Result：`GetInfoResult`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `state` | object | yes | see schema | none | 当前结果对象；示例字段包括 `target`、`domains`、`featureCount`、`generatedVersion`。 |
| `sampledAt` | string timestamp | no | RFC 3339 | omitted | 结果采样时间；客户端可用于缓存和校准。 |

#### 3.1.3 d block 示例

request:

```json
{
  "id": 101,
  "method": "device.getInfo",
  "params": {
    "target": "runtime"
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
      "target": "runtime",
      "domains": [
        "audio",
        "video"
      ],
      "featureCount": 55,
      "generatedVersion": "0.8.7"
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

### 3.2 `capability.getRegistry`

**用途**：查询设备基础信息摘要。

| 项 | 内容 |
|---|---|
| 调用类型 | query |
| Params Schema | `GetCapabilityRegistryParams` |
| Result Schema | `CapabilityRegistry` |
| 是否触发事件 | 否 |
| 幂等性 / 异步性 | 幂等；同步返回当前快照。 |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_ARGUMENT`, `PERMISSION_DENIED`, `UNAVAILABLE` |

#### 3.2.1 请求参数 Params：`GetCapabilityRegistryParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `target` | string | no | target id | `default` | 示例值 `runtime`；查询对象。 |

#### 3.2.2 返回结果 Result：`CapabilityRegistry`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `state` | object | yes | see schema | none | 当前结果对象；示例字段包括 `target`、`domains`、`featureCount`、`generatedVersion`。 |
| `sampledAt` | string timestamp | no | RFC 3339 | omitted | 结果采样时间；客户端可用于缓存和校准。 |

#### 3.2.3 d block 示例

request:

```json
{
  "id": 102,
  "method": "capability.getRegistry",
  "params": {
    "target": "runtime"
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
      "target": "runtime",
      "domains": [
        "audio",
        "video"
      ],
      "featureCount": 55,
      "generatedVersion": "0.8.7"
    },
    "sampledAt": "2026-06-15T08:00:02Z"
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

### 3.3 `capability.getDomainRegistry`

**用途**：查询设备基础信息摘要。

| 项 | 内容 |
|---|---|
| 调用类型 | query |
| Params Schema | `GetDomainRegistryParams` |
| Result Schema | `CapabilityRegistry` |
| 是否触发事件 | 否 |
| 幂等性 / 异步性 | 幂等；同步返回当前快照。 |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_ARGUMENT`, `PERMISSION_DENIED`, `UNAVAILABLE` |

#### 3.3.1 请求参数 Params：`GetDomainRegistryParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `target` | string | no | target id | `default` | 示例值 `runtime`；查询对象。 |

#### 3.3.2 返回结果 Result：`CapabilityRegistry`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `state` | object | yes | see schema | none | 当前结果对象；示例字段包括 `target`、`domains`、`featureCount`、`generatedVersion`。 |
| `sampledAt` | string timestamp | no | RFC 3339 | omitted | 结果采样时间；客户端可用于缓存和校准。 |

#### 3.3.3 d block 示例

request:

```json
{
  "id": 103,
  "method": "capability.getDomainRegistry",
  "params": {
    "target": "runtime"
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
      "target": "runtime",
      "domains": [
        "audio",
        "video"
      ],
      "featureCount": 55,
      "generatedVersion": "0.8.7"
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

### 3.4 `capability.getFeatureRegistry`

**用途**：查询设备基础信息摘要。

| 项 | 内容 |
|---|---|
| 调用类型 | query |
| Params Schema | `GetFeatureRegistryParams` |
| Result Schema | `GetFeatureRegistryResult` |
| 是否触发事件 | 否 |
| 幂等性 / 异步性 | 幂等；同步返回当前快照。 |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_ARGUMENT`, `PERMISSION_DENIED`, `UNAVAILABLE` |

#### 3.4.1 请求参数 Params：`GetFeatureRegistryParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `target` | string | no | target id | `default` | 示例值 `runtime`；查询对象。 |

#### 3.4.2 返回结果 Result：`GetFeatureRegistryResult`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `state` | object | yes | see schema | none | 当前结果对象；示例字段包括 `target`、`domains`、`featureCount`、`generatedVersion`。 |
| `sampledAt` | string timestamp | no | RFC 3339 | omitted | 结果采样时间；客户端可用于缓存和校准。 |

#### 3.4.3 d block 示例

request:

```json
{
  "id": 104,
  "method": "capability.getFeatureRegistry",
  "params": {
    "target": "runtime"
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
    "state": {
      "target": "runtime",
      "domains": [
        "audio",
        "video"
      ],
      "featureCount": 55,
      "generatedVersion": "0.8.7"
    },
    "sampledAt": "2026-06-15T08:00:04Z"
  }
}
```

#### 3.4.4 可能触发的事件

| Event | 触发条件 | Payload Schema | 客户端处理建议 |
|---|---|---|---|
| 无 | query method 不应因查询触发状态变化事件。 | none | 无需处理。 |

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
| `capability.registryChanged` | 能力动态启停、子模块变化、权限变化或固件/软件更新后通知 | `CapabilityRegistryChangedEvent` | 更新 UI 或调用对应 get method 校准 | draft |

### 4.1 `capability.registryChanged`

**触发条件**：能力动态启停、子模块变化、权限变化或固件/软件更新后通知。

#### 4.1.1 Payload：`CapabilityRegistryChangedEvent`

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
  "event": "capability.registryChanged",
  "intent": 1,
  "data": {
    "changedFields": [
      "generatedVersion"
    ],
    "state": {
      "target": "runtime",
      "includeDraftCapabilities": false,
      "preferredLanguage": "zh-CN"
    },
    "source": "remoteApp",
    "reason": "registry_refreshed",
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

Capability name: `capability.registry`。

设备通过 capability 声明是否支持该 feature，以及支持哪些范围、模式、对象或约束。Capability 字段只描述“设备能做什么”，不得混入 method params/result 或 event payload。

| 能力字段 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `capability` | string | yes | fixed `capability.registry` | none | capability 名称。 |
| `supportedTargets` | string[] | no | target id array | omitted | 支持的对象、通道、端口、组件或 scope。 |

## 6. 字段 / Schemas

### 6.1 Schema 层级速览

```text
RegistryCapability
  capability / supportedTargets
RegistryState
  target / status / sampledAt
RegistryChangedEvent
  changedFields / state / source / reason / stateRevision
```

### 6.2 请求与响应 Schemas

| Schema | 用途 | 字段定义 |
|---|---|---|
| `GetInfoParams` | `device.getInfo` request params | 见 `device.getInfo` 方法小节。 |
| `GetInfoResult` | `device.getInfo` result | 见 `device.getInfo` 方法小节。 |
| `GetCapabilityRegistryParams` | `capability.getRegistry` request params | 见 `capability.getRegistry` 方法小节。 |
| `CapabilityRegistry` | `capability.getRegistry` result | 见 `capability.getRegistry` 方法小节。 |
| `GetDomainRegistryParams` | `capability.getDomainRegistry` request params | 见 `capability.getDomainRegistry` 方法小节。 |
| `CapabilityRegistry` | `capability.getDomainRegistry` result | 见 `capability.getDomainRegistry` 方法小节。 |
| `GetFeatureRegistryParams` | `capability.getFeatureRegistry` request params | 见 `capability.getFeatureRegistry` 方法小节。 |
| `GetFeatureRegistryResult` | `capability.getFeatureRegistry` result | 见 `capability.getFeatureRegistry` 方法小节。 |

### 6.3 Capability Schemas


### 6.4 Event Schemas

| Schema | Event | 字段定义 |
|---|---|---|
| `CapabilityRegistryChangedEvent` | `capability.registryChanged` | 见 `capability.registryChanged` 事件小节。 |