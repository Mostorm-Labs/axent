---
status: draft
contract: false
generated: false
domain: file
feature: file.transfer
registry:
lastReviewed: 2026-06-15
---

# file.transfer

## 0. 速读结论

| 项目 | 内容 |
|---|---|
| 这个能力做什么 | 通用文件上传、下载、分片、校验、断点续传和传输状态。 |
| 当前状态 | draft |
| 是否可直接实现 | 否。本文是 protocol draft；正式实现以 registry / generated 为准。 |
| 主要交互 | RPC + EVENT |
| 是否使用 STREAM | 否 |
| Registry readiness | partial |
| Conformance | needed |
| 主要未决问题 | schema 字段、错误模型、legacy 映射和 conformance case 仍需人工确认。 |

## 1. 功能说明

`file.transfer` 用于通用文件上传、下载、分片、校验、断点续传和传输状态。

## 2. 能力边界

| 类型 | 内容 |
|---|---|
| 包含 | file.transfer 的能力发现、配置、状态、动作或事件。 |
| 包含 | 与 file.transfer 直接相关的 method/event/schema 草案。 |
| 包含 | 已确认 legacy 协议到 file.transfer 的语义归类。 |
| 不包含 | 业务域决定传输什么文件；file 域只负责通用文件数据面和文件传输状态。 |
| 不包含 | method/event 数值 ID 分配；数值以 contract/registry/generated 为准。 |
| 不包含 | 未确认旧协议 payload 的稳定映射。 |
| 数据面 | 本 feature 默认不定义 STREAM payload，所有操作均通过 RPC method/event 完成。 |

## 3. 方法 Methods

### 3.0 方法速览

| Method | 调用类型 | 用途 | Params Schema | Result Schema | 是否触发事件 | 状态 |
|---|---|---|---|---|---|---|
| `file.beginTransfer` | action | 开始文件上传或下载传输 | `BeginTransferParams` | `BeginTransferResult` | 是，`file.transferStateChanged` | draft |
| `file.endTransfer` | action | 结束文件传输 | `EndTransferParams` | `EndTransferResult` | 是，`file.transferStateChanged` | draft |
| `file.cancelTransfer` | action | 取消文件传输 | `CancelTransferParams` | `CancelTransferResult` | 是，`file.transferStateChanged` | draft |
| `file.resumeTransfer` | action | 恢复文件传输 | `ResumeTransferParams` | `ResumeTransferResult` | 是，`file.transferStateChanged` | draft |
| `file.getTransferState` | query | 查询文件传输状态 | `GetTransferStateParams` | `GetTransferStateResult` | 否 | draft |

### 3.1 `file.beginTransfer`

**用途**：开始文件上传或下载传输。

| 项 | 内容 |
|---|---|
| 调用类型 | action |
| Params Schema | `BeginTransferParams` |
| Result Schema | `BeginTransferResult` |
| 是否触发事件 | 是，状态实际变化后触发 `file.transferStateChanged`。 |
| 幂等性 / 异步性 | 建议幂等；重复提交相同目标状态应成功，可不重复触发事件。 |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_ARGUMENT`, `INVALID_STATE`, `BUSY`, `PERMISSION_DENIED` |

#### 3.1.1 请求参数 Params：`BeginTransferParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `target` | string | no | target id | `default` | 动作对象；具体 target 集合由 capability 声明。 |
| `reason` | string | no | caller-defined reason | omitted | 调用方给出的动作原因。 |

#### 3.1.2 返回结果 Result：`BeginTransferResult`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `accepted` | boolean | yes | `true`, `false` | none | 设备是否接受动作请求。 |
| `actionId` | string | no | opaque action id | omitted | 动作 ID，用于日志或异步关联。 |

#### 3.1.3 d block 示例

request:

```json
{
  "id": 101,
  "method": "file.beginTransfer",
  "params": {
    "target": "file-transfer",
    "reason": "transfer_progress"
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
    "accepted": true,
    "actionId": "file-begintransfer-20260615-001"
  }
}
```

#### 3.1.4 可能触发的事件

| Event | 触发条件 | Payload Schema | 客户端处理建议 |
|---|---|---|---|
| `file.transferStateChanged` | 该方法导致状态、配置或动作状态实际变化。 | `TransferStateChangedEvent` | 可直接更新 UI；需要完整状态时调用对应 get method 校准。 |

#### 3.1.5 错误

| 错误 | 场景 | 返回建议 |
|---|---|---|
| `NOT_SUPPORTED` | 设备不支持该 feature、method、target 或 scope。 | 返回 unsupported feature/method/target。 |
| `INVALID_ARGUMENT` | 请求字段非法、枚举非法或范围非法。 | 返回具体字段路径和合法范围。 |
| `PERMISSION_DENIED` | 调用方无权执行该操作。 | 返回权限错误。 |
| `BUSY` | 设备正在处理冲突操作。 | 建议稍后重试。 |

### 3.2 `file.endTransfer`

**用途**：结束文件传输。

| 项 | 内容 |
|---|---|
| 调用类型 | action |
| Params Schema | `EndTransferParams` |
| Result Schema | `EndTransferResult` |
| 是否触发事件 | 是，状态实际变化后触发 `file.transferStateChanged`。 |
| 幂等性 / 异步性 | 建议幂等；重复提交相同目标状态应成功，可不重复触发事件。 |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_ARGUMENT`, `INVALID_STATE`, `BUSY`, `PERMISSION_DENIED` |

#### 3.2.1 请求参数 Params：`EndTransferParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `target` | string | no | target id | `default` | 动作对象；具体 target 集合由 capability 声明。 |
| `reason` | string | no | caller-defined reason | omitted | 调用方给出的动作原因。 |

#### 3.2.2 返回结果 Result：`EndTransferResult`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `accepted` | boolean | yes | `true`, `false` | none | 设备是否接受动作请求。 |
| `actionId` | string | no | opaque action id | omitted | 动作 ID，用于日志或异步关联。 |

#### 3.2.3 d block 示例

request:

```json
{
  "id": 102,
  "method": "file.endTransfer",
  "params": {
    "target": "file-transfer",
    "reason": "transfer_progress"
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
    "actionId": "file-endtransfer-20260615-001"
  }
}
```

#### 3.2.4 可能触发的事件

| Event | 触发条件 | Payload Schema | 客户端处理建议 |
|---|---|---|---|
| `file.transferStateChanged` | 该方法导致状态、配置或动作状态实际变化。 | `TransferStateChangedEvent` | 可直接更新 UI；需要完整状态时调用对应 get method 校准。 |

#### 3.2.5 错误

| 错误 | 场景 | 返回建议 |
|---|---|---|
| `NOT_SUPPORTED` | 设备不支持该 feature、method、target 或 scope。 | 返回 unsupported feature/method/target。 |
| `INVALID_ARGUMENT` | 请求字段非法、枚举非法或范围非法。 | 返回具体字段路径和合法范围。 |
| `PERMISSION_DENIED` | 调用方无权执行该操作。 | 返回权限错误。 |
| `BUSY` | 设备正在处理冲突操作。 | 建议稍后重试。 |

### 3.3 `file.cancelTransfer`

**用途**：取消文件传输。

| 项 | 内容 |
|---|---|
| 调用类型 | action |
| Params Schema | `CancelTransferParams` |
| Result Schema | `CancelTransferResult` |
| 是否触发事件 | 是，状态实际变化后触发 `file.transferStateChanged`。 |
| 幂等性 / 异步性 | 建议幂等；重复提交相同目标状态应成功，可不重复触发事件。 |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_ARGUMENT`, `INVALID_STATE`, `BUSY`, `PERMISSION_DENIED` |

#### 3.3.1 请求参数 Params：`CancelTransferParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `target` | string | no | target id | `default` | 动作对象；具体 target 集合由 capability 声明。 |
| `reason` | string | no | caller-defined reason | omitted | 调用方给出的动作原因。 |

#### 3.3.2 返回结果 Result：`CancelTransferResult`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `accepted` | boolean | yes | `true`, `false` | none | 设备是否接受动作请求。 |
| `actionId` | string | no | opaque action id | omitted | 动作 ID，用于日志或异步关联。 |

#### 3.3.3 d block 示例

request:

```json
{
  "id": 103,
  "method": "file.cancelTransfer",
  "params": {
    "target": "file-transfer",
    "reason": "transfer_progress"
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
    "actionId": "file-canceltransfer-20260615-001"
  }
}
```

#### 3.3.4 可能触发的事件

| Event | 触发条件 | Payload Schema | 客户端处理建议 |
|---|---|---|---|
| `file.transferStateChanged` | 该方法导致状态、配置或动作状态实际变化。 | `TransferStateChangedEvent` | 可直接更新 UI；需要完整状态时调用对应 get method 校准。 |

#### 3.3.5 错误

| 错误 | 场景 | 返回建议 |
|---|---|---|
| `NOT_SUPPORTED` | 设备不支持该 feature、method、target 或 scope。 | 返回 unsupported feature/method/target。 |
| `INVALID_ARGUMENT` | 请求字段非法、枚举非法或范围非法。 | 返回具体字段路径和合法范围。 |
| `PERMISSION_DENIED` | 调用方无权执行该操作。 | 返回权限错误。 |
| `BUSY` | 设备正在处理冲突操作。 | 建议稍后重试。 |

### 3.4 `file.resumeTransfer`

**用途**：恢复文件传输。

| 项 | 内容 |
|---|---|
| 调用类型 | action |
| Params Schema | `ResumeTransferParams` |
| Result Schema | `ResumeTransferResult` |
| 是否触发事件 | 是，状态实际变化后触发 `file.transferStateChanged`。 |
| 幂等性 / 异步性 | 建议幂等；重复提交相同目标状态应成功，可不重复触发事件。 |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_ARGUMENT`, `INVALID_STATE`, `BUSY`, `PERMISSION_DENIED` |

#### 3.4.1 请求参数 Params：`ResumeTransferParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `target` | string | no | target id | `default` | 动作对象；具体 target 集合由 capability 声明。 |
| `reason` | string | no | caller-defined reason | omitted | 调用方给出的动作原因。 |

#### 3.4.2 返回结果 Result：`ResumeTransferResult`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `accepted` | boolean | yes | `true`, `false` | none | 设备是否接受动作请求。 |
| `actionId` | string | no | opaque action id | omitted | 动作 ID，用于日志或异步关联。 |

#### 3.4.3 d block 示例

request:

```json
{
  "id": 104,
  "method": "file.resumeTransfer",
  "params": {
    "target": "file-transfer",
    "reason": "transfer_progress"
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
    "actionId": "file-resumetransfer-20260615-001"
  }
}
```

#### 3.4.4 可能触发的事件

| Event | 触发条件 | Payload Schema | 客户端处理建议 |
|---|---|---|---|
| `file.transferStateChanged` | 该方法导致状态、配置或动作状态实际变化。 | `TransferStateChangedEvent` | 可直接更新 UI；需要完整状态时调用对应 get method 校准。 |

#### 3.4.5 错误

| 错误 | 场景 | 返回建议 |
|---|---|---|
| `NOT_SUPPORTED` | 设备不支持该 feature、method、target 或 scope。 | 返回 unsupported feature/method/target。 |
| `INVALID_ARGUMENT` | 请求字段非法、枚举非法或范围非法。 | 返回具体字段路径和合法范围。 |
| `PERMISSION_DENIED` | 调用方无权执行该操作。 | 返回权限错误。 |
| `BUSY` | 设备正在处理冲突操作。 | 建议稍后重试。 |

### 3.5 `file.getTransferState`

**用途**：查询文件传输状态。

| 项 | 内容 |
|---|---|
| 调用类型 | query |
| Params Schema | `GetTransferStateParams` |
| Result Schema | `GetTransferStateResult` |
| 是否触发事件 | 否 |
| 幂等性 / 异步性 | 幂等；同步返回当前快照。 |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_ARGUMENT`, `PERMISSION_DENIED`, `UNAVAILABLE` |

#### 3.5.1 请求参数 Params：`GetTransferStateParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `target` | string | no | target id | `default` | 示例值 `file-transfer`；查询对象。 |

#### 3.5.2 返回结果 Result：`GetTransferStateResult`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `state` | object | yes | see schema | none | 当前结果对象；示例字段包括 `target`、`transferId`、`state`、`transferredBytes`。 |
| `sampledAt` | string timestamp | no | RFC 3339 | omitted | 结果采样时间；客户端可用于缓存和校准。 |

#### 3.5.3 d block 示例

request:

```json
{
  "id": 105,
  "method": "file.getTransferState",
  "params": {
    "target": "file-transfer"
  }
}
```

success:

```json
{
  "id": 105,
  "status": {
    "ok": true,
    "code": 0
  },
  "result": {
    "state": {
      "target": "file-transfer",
      "transferId": "transfer-20260615-001",
      "state": "running",
      "transferredBytes": 524288
    },
    "sampledAt": "2026-06-15T08:00:05Z"
  }
}
```

#### 3.5.4 可能触发的事件

| Event | 触发条件 | Payload Schema | 客户端处理建议 |
|---|---|---|---|
| 无 | query method 不应因查询触发状态变化事件。 | none | 无需处理。 |

#### 3.5.5 错误

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
| `file.transferStateChanged` | 文件传输状态变化 | `TransferStateChangedEvent` | 更新 UI 或调用对应 get method 校准 | draft |
| `file.transferProgressReported` | 文件传输进度上报 | `TransferProgressReportedEvent` | 更新 UI 或调用对应 get method 校准 | draft |

### 4.1 `file.transferStateChanged`

**触发条件**：文件传输状态变化。

#### 4.1.1 Payload：`TransferStateChangedEvent`

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
  "event": "file.transferStateChanged",
  "intent": 1,
  "data": {
    "changedFields": [
      "state",
      "transferredBytes"
    ],
    "state": {
      "target": "file-transfer",
      "direction": "download",
      "fileId": "logs/system.log",
      "resume": true
    },
    "source": "remoteApp",
    "reason": "transfer_progress",
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

### 4.2 `file.transferProgressReported`

**触发条件**：文件传输进度上报。

#### 4.2.1 Payload：`TransferProgressReportedEvent`

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
  "event": "file.transferProgressReported",
  "intent": 1,
  "data": {
    "changedFields": [
      "state",
      "transferredBytes"
    ],
    "state": {
      "target": "file-transfer",
      "direction": "download",
      "fileId": "logs/system.log",
      "resume": true
    },
    "source": "remoteApp",
    "reason": "transfer_progress",
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

Capability name: `file.transfer`。

设备通过 capability 声明是否支持该 feature，以及支持哪些范围、模式、对象或约束。Capability 字段只描述“设备能做什么”，不得混入 method params/result 或 event payload。

| 能力字段 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `capability` | string | yes | fixed `file.transfer` | none | capability 名称。 |
| `supportedTargets` | string[] | no | target id array | omitted | 支持的对象、通道、端口、组件或 scope。 |

## 6. 字段 / Schemas

### 6.1 Schema 层级速览

```text
TransferCapability
  capability / supportedTargets
TransferState
  target / status / sampledAt
TransferChangedEvent
  changedFields / state / source / reason / stateRevision
```

### 6.2 请求与响应 Schemas

| Schema | 用途 | 字段定义 |
|---|---|---|
| `BeginTransferParams` | `file.beginTransfer` request params | 见 `file.beginTransfer` 方法小节。 |
| `BeginTransferResult` | `file.beginTransfer` result | 见 `file.beginTransfer` 方法小节。 |
| `EndTransferParams` | `file.endTransfer` request params | 见 `file.endTransfer` 方法小节。 |
| `EndTransferResult` | `file.endTransfer` result | 见 `file.endTransfer` 方法小节。 |
| `CancelTransferParams` | `file.cancelTransfer` request params | 见 `file.cancelTransfer` 方法小节。 |
| `CancelTransferResult` | `file.cancelTransfer` result | 见 `file.cancelTransfer` 方法小节。 |
| `ResumeTransferParams` | `file.resumeTransfer` request params | 见 `file.resumeTransfer` 方法小节。 |
| `ResumeTransferResult` | `file.resumeTransfer` result | 见 `file.resumeTransfer` 方法小节。 |
| `GetTransferStateParams` | `file.getTransferState` request params | 见 `file.getTransferState` 方法小节。 |
| `GetTransferStateResult` | `file.getTransferState` result | 见 `file.getTransferState` 方法小节。 |

### 6.3 Capability Schemas


### 6.4 Event Schemas

| Schema | Event | 字段定义 |
|---|---|---|
| `TransferStateChangedEvent` | `file.transferStateChanged` | 见 `file.transferStateChanged` 事件小节。 |
| `TransferProgressReportedEvent` | `file.transferProgressReported` | 见 `file.transferProgressReported` 事件小节。 |