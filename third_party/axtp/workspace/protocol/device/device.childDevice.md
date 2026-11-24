---
status: draft
contract: false
generated: false
domain: device
feature: device.childDevice
registry:
lastReviewed: 2026-08-25
---

# device.childDevice

## 0. 速读结论

| 项目 | 内容 |
|---|---|
| 这个能力做什么 | 当前 AXTP endpoint 代理、管理或挂载的子设备/级联设备发现、详情读取、可选拓扑读取和子设备状态变化通知。可寻址 child 同时暴露稳定 `endpointId`，供 Core RPC `m.src` / `m.dst` 使用。 |
| 当前状态 | draft |
| 是否可直接实现 | 否。本文是 protocol draft；正式 method/schema 实现仍以 registry / generated 为准。Core Endpoint Relay 规则见 `specs/20-core.md`。 |
| 主要交互 | RPC + EVENT |
| 是否使用 STREAM | 否 |
| Registry readiness | partial |
| Conformance | Endpoint Relay 的 envelope 行为由 core conformance 验收；本 feature 的 method/schema 仍需独立采纳。 |
| 主要未决问题 | method/schema 数值事实、legacy 映射和本 feature 的完整 conformance case 仍需人工确认。 |

## 1. 功能说明

`device.childDevice` 用于当前 AXTP endpoint 代理、管理或挂载的子设备/级联设备发现、详情读取、可选拓扑读取和子设备状态变化通知。

对于可被 AXTP 单独寻址的 child，本 feature SHOULD 返回稳定 `endpointId`。上层获得该 ID 后，可直接在对象编码 RPC envelope 中使用 `m.dst=<child endpointId>`；Agent/relay 自己维护 `endpointId -> provider` 映射，不向调用方暴露真实的多级 Agent path。

`childId` 与 `endpointId` 不等价：

- `childId` 是当前 parent/provider 视角下的设备、拓扑或 adapter-local 标识，可以用于 topology/detail lookup；
- `endpointId` 是跨 session/relay 的稳定逻辑地址，遵循 `specs/20-core.md` 的 Endpoint identity 规则；
- child 能被独立 RPC 寻址时 SHOULD 提供 `endpointId`；无法建立稳定 identity 的 child MAY 暂时缺失 `endpointId`，此时不能作为稳定 `m.dst` 暴露给上游。

## 2. 能力边界

| 类型 | 内容 |
|---|---|
| 包含 | device.childDevice 的能力发现、状态查询、配置或动作控制。 |
| 包含 | 与 device.childDevice 直接相关的 method/event/schema 草案。 |
| 包含 | child entry 对稳定 `endpointId` 的暴露，用于与 Core Endpoint Relay 对齐。 |
| 不包含 | 不承载其他 capability feature 的业务语义；跨域关系通过 schema 字段、引用或数据面 stream/file 表达。 |
| 不包含 | 不定义 `route`、`routeId`、`nextHop`、`ttl`、`hops`；这些不是 childDevice wire contract。 |
| 不包含 | method/event 数值 ID 分配；数值以 contract/registry/generated 为准。 |
| 数据面 | 本 feature 默认不定义 STREAM payload，所有操作均通过 RPC method/event 完成。 |

### 2.1 Endpoint Projection

当 Agent A 管理 Agent B，而 Agent B 管理 Camera 时，Agent A MAY 把 Camera 的最终 `endpointId` 投影到自己的可见 child 集合：

```text
Cloud
  -> Agent A
      -> Agent B
          -> Camera(ep-camera-001)
```

Cloud 只需要知道：

```text
ep-camera-001 -> Agent A session
```

Agent A 本地再解析：

```text
ep-camera-001 -> Agent B provider/session
```

上游 RPC 始终使用单个 `m.dst="ep-camera-001"`，不携带 Agent B 或完整 path。

## 3. 方法 Methods

### 3.0 方法速览

| Method | 调用类型 | 用途 | Params Schema | Result Schema | 是否触发事件 | 状态 |
|---|---|---|---|---|---|---|
| `device.getInfo` | query | 查询设备基础信息摘要 | `GetInfoParams` | `GetInfoResult` | 否 | draft |
| `device.getTopology` | query | 查询主设备与子设备的拓扑关系 | `GetTopologyParams` | `DeviceTopology` | 否 | draft |
| `device.getChildren` | query | 查询当前可见的子设备列表及其可选 `endpointId` | `GetChildrenParams` | `GetChildrenResult` | 否 | draft |
| `device.getChildInfo` | query | 查询指定子设备的详细信息及可选 `endpointId` | `GetChildInfoParams` | `ChildDeviceInfo` | 否 | draft |

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
| `target` | string | no | target id | `default` | 示例值 `child-device-bus`；查询对象。 |

#### 3.1.2 返回结果 Result：`GetInfoResult`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `state` | object | yes | see schema | none | 当前结果对象；示例字段包括 `target`、`children`。可寻址 child SHOULD 包含 `endpointId`。 |
| `sampledAt` | string timestamp | no | RFC 3339 | omitted | 结果采样时间；客户端可用于缓存和校准。 |

#### 3.1.3 d block 示例

request:

```json
{
  "id": 101,
  "method": "device.getInfo",
  "params": {
    "target": "child-device-bus"
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
      "target": "child-device-bus",
      "children": [
        {
          "childId": "camera-main",
          "endpointId": "ep-camera-001",
          "type": "camera",
          "online": true
        }
      ]
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

### 3.2 `device.getTopology`

**用途**：查询设备基础信息摘要。

| 项 | 内容 |
|---|---|
| 调用类型 | query |
| Params Schema | `GetTopologyParams` |
| Result Schema | `DeviceTopology` |
| 是否触发事件 | 否 |
| 幂等性 / 异步性 | 幂等；同步返回当前快照。 |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_ARGUMENT`, `PERMISSION_DENIED`, `UNAVAILABLE` |

#### 3.2.1 请求参数 Params：`GetTopologyParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `target` | string | no | target id | `default` | 示例值 `child-device-bus`；查询对象。 |

#### 3.2.2 返回结果 Result：`DeviceTopology`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `state` | object | yes | see schema | none | 当前结果对象；示例字段包括 `target`、`children`。可寻址 child SHOULD 包含 `endpointId`。 |
| `sampledAt` | string timestamp | no | RFC 3339 | omitted | 结果采样时间；客户端可用于缓存和校准。 |

#### 3.2.3 d block 示例

request:

```json
{
  "id": 102,
  "method": "device.getTopology",
  "params": {
    "target": "child-device-bus"
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
      "target": "child-device-bus",
      "children": [
        {
          "childId": "camera-main",
          "endpointId": "ep-camera-001",
          "type": "camera",
          "online": true
        }
      ]
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

### 3.3 `device.getChildren`

**用途**：查询当前可见的子设备列表。对能够稳定寻址的 child，结果 SHOULD 返回 `endpointId`，供调用方后续直接作为对象编码 RPC `m.dst` 使用。

| 项 | 内容 |
|---|---|
| 调用类型 | query |
| Params Schema | `GetChildrenParams` |
| Result Schema | `GetChildrenResult` |
| 是否触发事件 | 否 |
| 幂等性 / 异步性 | 幂等；同步返回当前快照。 |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_ARGUMENT`, `PERMISSION_DENIED`, `UNAVAILABLE` |

#### 3.3.1 请求参数 Params：`GetChildrenParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `target` | string | no | target id | `default` | 示例值 `child-device-bus`；查询对象。 |

#### 3.3.2 返回结果 Result：`GetChildrenResult`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `state` | object | yes | see schema | none | 当前结果对象；`children[]` 中 `childId` 是 provider-local identity，`endpointId` 是可选稳定 AXTP address。 |
| `sampledAt` | string timestamp | no | RFC 3339 | omitted | 结果采样时间；客户端可用于缓存和校准。 |

#### 3.3.3 d block 示例

request:

```json
{
  "id": 103,
  "method": "device.getChildren",
  "params": {
    "target": "child-device-bus"
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
      "target": "child-device-bus",
      "children": [
        {
          "childId": "camera-main",
          "endpointId": "ep-camera-001",
          "type": "camera",
          "online": true
        }
      ]
    },
    "sampledAt": "2026-06-15T08:00:03Z"
  }
}
```

上层拿到 `ep-camera-001` 后的调用示意属于 Core RPC envelope，而不是本 method 的 `d` schema：

```json
{
  "sid": "12345678",
  "op": 7,
  "m": {
    "dst": "ep-camera-001"
  },
  "d": {
    "id": 201,
    "method": "camera.getStatus",
    "params": {}
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

### 3.4 `device.getChildInfo`

**用途**：查询指定子设备的详细信息，并在该 child 可稳定寻址时返回其 `endpointId`。

| 项 | 内容 |
|---|---|
| 调用类型 | query |
| Params Schema | `GetChildInfoParams` |
| Result Schema | `ChildDeviceInfo` |
| 是否触发事件 | 否 |
| 幂等性 / 异步性 | 幂等；同步返回当前快照。 |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_ARGUMENT`, `PERMISSION_DENIED`, `UNAVAILABLE` |

#### 3.4.1 请求参数 Params：`GetChildInfoParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `target` | string | no | target id | `default` | 示例值 `child-device-bus`；查询对象。 |

#### 3.4.2 返回结果 Result：`ChildDeviceInfo`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `state` | object | yes | see schema | none | 当前结果对象；可寻址 child SHOULD 包含 `endpointId`。 |
| `sampledAt` | string timestamp | no | RFC 3339 | omitted | 结果采样时间；客户端可用于缓存和校准。 |

#### 3.4.3 d block 示例

request:

```json
{
  "id": 104,
  "method": "device.getChildInfo",
  "params": {
    "target": "child-device-bus"
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
      "target": "child-device-bus",
      "children": [
        {
          "childId": "camera-main",
          "endpointId": "ep-camera-001",
          "type": "camera",
          "online": true
        }
      ]
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
| `device.childDeviceStateChanged` | 低频连接、在线、关系、摘要状态变化通过 RPC Event | `ChildDeviceStateChangedEvent` | 更新 UI 或调用对应 get method 校准 | draft |

### 4.1 `device.childDeviceStateChanged`

**触发条件**：低频连接、在线、关系、摘要状态变化通过 RPC Event。

#### 4.1.1 Payload：`ChildDeviceStateChangedEvent`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `changedFields` | string[] | no | field path array | omitted | 变化字段路径。 |
| `state` | object | no | see schema | omitted | 变化后的状态、配置或摘要；涉及可寻址 child 时 SHOULD 包含 `endpointId`。 |
| `source` | string enum | no | `remoteApp`, `localPanel`, `devicePolicy`, `adapter`, `unknown` | `unknown` | 状态变化来源。 |
| `reason` | string enum | no | feature-specific | `unknown` | 状态变化原因。 |
| `stateRevision` | uint32 | no | monotonic counter | omitted | 状态版本，用于多端同步和去重。 |

#### 4.1.2 d block 示例

```json
{
  "event": "device.childDeviceStateChanged",
  "intent": 1,
  "data": {
    "changedFields": [
      "children.camera-main.online"
    ],
    "state": {
      "target": "child-device-bus",
      "endpointId": "ep-camera-001",
      "discoveryEnabled": true,
      "includeOfflineDevices": false
    },
    "source": "remoteApp",
    "reason": "child_device_online",
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
- Event 的逻辑来源 Endpoint 与 fanout 规则由 Core RPC `m.src` / `m.dst` 定义；本 feature 不重复定义多播地址结构。

## 5. Capability

Capability name: `device.childDevice`。

设备通过 capability 声明是否支持该 feature，以及支持哪些范围、模式、对象或约束。Capability 字段只描述“设备能做什么”，不得混入 method params/result 或 event payload。

| 能力字段 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `capability` | string | yes | fixed `device.childDevice` | none | capability 名称。 |
| `supportedTargets` | string[] | no | target id array | omitted | 支持的对象、通道、端口、组件或 scope。 |

## 6. 字段 / Schemas

### 6.1 Schema 层级速览

```text
ChildDeviceCapability
  capability / supportedTargets
ChildDeviceEntry
  childId / endpointId? / type / online / ...
ChildDeviceState
  target / children[] / status / sampledAt
ChildDeviceChangedEvent
  changedFields / state / source / reason / stateRevision
```

`ChildDeviceEntry.endpointId` 是本轮与 Core Endpoint Relay 对齐的候选字段：类型为 string、optional，建议最大 128 UTF-8 bytes；当 child 拥有稳定 AXTP address 时 SHOULD present。它不替代 `childId`。

### 6.2 请求与响应 Schemas

| Schema | 用途 | 字段定义 |
|---|---|---|
| `GetInfoParams` | `device.getInfo` request params | 见 `device.getInfo` 方法小节。 |
| `GetInfoResult` | `device.getInfo` result | 见 `device.getInfo` 方法小节。 |
| `GetTopologyParams` | `device.getTopology` request params | 见 `device.getTopology` 方法小节。 |
| `DeviceTopology` | `device.getTopology` result | 见 `device.getTopology` 方法小节。 |
| `GetChildrenParams` | `device.getChildren` request params | 见 `device.getChildren` 方法小节。 |
| `GetChildrenResult` | `device.getChildren` result | 见 `device.getChildren` 方法小节。 |
| `GetChildInfoParams` | `device.getChildInfo` request params | 见 `device.getChildInfo` 方法小节。 |
| `ChildDeviceInfo` | `device.getChildInfo` result | 见 `device.getChildInfo` 方法小节。 |

### 6.3 Capability Schemas


### 6.4 Event Schemas

| Schema | Event | 字段定义 |
|---|---|---|
| `ChildDeviceStateChangedEvent` | `device.childDeviceStateChanged` | 见 `device.childDeviceStateChanged` 事件小节。 |
