---
status: draft
contract: false
generated: false
domain: audio
feature: audio.recording
registry:
lastReviewed: 2026-06-15
---

# audio.recording

## 0. 速读结论

| 项目 | 内容 |
|---|---|
| 这个能力做什么 | 音频调试、产测抓音、问题定位、上位机实时监听、算法前后数据录制、文件化录制导出。 |
| 当前状态 | draft |
| 是否可直接实现 | 否。本文是 protocol draft；正式实现以 registry / generated 为准。 |
| 主要交互 | RPC + EVENT |
| 是否使用 STREAM | 否 |
| Registry readiness | partial |
| Conformance | needed |
| 主要未决问题 | schema 字段、错误模型、legacy 映射和 conformance case 仍需人工确认。 |

## 1. 功能说明

`audio.recording` 用于音频调试、产测抓音、问题定位、上位机实时监听、算法前后数据录制、文件化录制导出。

## 2. 能力边界

| 类型 | 内容 |
|---|---|
| 包含 | audio.recording 的能力发现、状态查询、配置或动作控制。 |
| 包含 | 与 audio.recording 直接相关的 method/event/schema 草案。 |
| 不包含 | 不承载其他 capability feature 的业务语义；跨域关系通过 schema 字段、引用或数据面 stream/file 表达。 |
| 不包含 | method/event 数值 ID 分配；数值以 contract/registry/generated 为准。 |
| 数据面 | 本 feature 默认不定义 STREAM payload，所有操作均通过 RPC method/event 完成。 |

## 3. 方法 Methods

### 3.0 方法速览

| Method | 调用类型 | 用途 | Params Schema | Result Schema | 是否触发事件 | 状态 |
|---|---|---|---|---|---|---|
| `audio.startRecording` | action | 开始音频录制任务 | `StartRecordingParams` | `StartRecordingResult` | 是，`audio.recordingStateChanged` | draft |
| `audio.stopRecording` | action | 停止音频录制任务并生成录制结果 | `StopRecordingParams` | `StopRecordingResult` | 是，`audio.recordingStateChanged` | draft |
| `audio.cancelRecording` | action | 取消正在进行的音频录制任务 | `CancelRecordingParams` | `CancelRecordingResult` | 是，`audio.recordingStateChanged` | draft |
| `audio.getRecordingState` | query | 查询音频录制任务状态 | `GetRecordingStateParams` | `GetRecordingStateResult` | 否 | draft |
| `stream.open` | action | 建立通用 STREAM 会话并返回 streamId | `OpenParams` | `OpenResult` | 是，`audio.recordingStateChanged` | draft |
| `file.getInfo` | query | 查询文件元数据和下载条件 | `GetInfoParams` | `GetInfoResult` | 否 | draft |
| `file.download` | action | 请求下载文件或绑定 STREAM 下载 | `DownloadParams` | `DownloadResult` | 是，`audio.recordingStateChanged` | draft |
| `file.delete` | action | 删除指定文件或录制结果 | `DeleteParams` | `DeleteResult` | 是，`audio.recordingStateChanged` | draft |
| `audio.getRecordingCapabilities` | query | 查询录制能力、默认值和约束 | `GetRecordingCapabilitiesParams` | `GetRecordingCapabilitiesResult` | 否 | draft |

### 3.1 `audio.startRecording`

**用途**：开始音频录制任务。

| 项 | 内容 |
|---|---|
| 调用类型 | action |
| Params Schema | `StartRecordingParams` |
| Result Schema | `StartRecordingResult` |
| 是否触发事件 | 是，状态实际变化后触发 `audio.recordingStateChanged`。 |
| 幂等性 / 异步性 | 建议幂等；重复提交相同目标状态应成功，可不重复触发事件。 |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_ARGUMENT`, `INVALID_STATE`, `BUSY`, `PERMISSION_DENIED` |

#### 3.1.1 请求参数 Params：`StartRecordingParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `target` | string | no | target id | `default` | 动作对象；具体 target 集合由 capability 声明。 |
| `reason` | string | no | caller-defined reason | omitted | 调用方给出的动作原因。 |

#### 3.1.2 返回结果 Result：`StartRecordingResult`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `accepted` | boolean | yes | `true`, `false` | none | 设备是否接受动作请求。 |
| `actionId` | string | no | opaque action id | omitted | 动作 ID，用于日志或异步关联。 |

#### 3.1.3 d block 示例

request:

```json
{
  "id": 101,
  "method": "audio.startRecording",
  "params": {
    "target": "mic-array",
    "profile": "meeting",
    "container": "wav",
    "reason": "user_request"
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
    "actionId": "recording-20260621-001"
  }
}
```

#### 3.1.4 可能触发的事件

| Event | 触发条件 | Payload Schema | 客户端处理建议 |
|---|---|---|---|
| `audio.recordingStateChanged` | 该方法导致状态、配置或动作状态实际变化。 | `RecordingStateChangedEvent` | 可直接更新 UI；需要完整状态时调用对应 get method 校准。 |

#### 3.1.5 错误

| 错误 | 场景 | 返回建议 |
|---|---|---|
| `NOT_SUPPORTED` | 设备不支持该 feature、method、target 或 scope。 | 返回 unsupported feature/method/target。 |
| `INVALID_ARGUMENT` | 请求字段非法、枚举非法或范围非法。 | 返回具体字段路径和合法范围。 |
| `PERMISSION_DENIED` | 调用方无权执行该操作。 | 返回权限错误。 |
| `BUSY` | 设备正在处理冲突操作。 | 建议稍后重试。 |

### 3.2 `audio.stopRecording`

**用途**：开始音频录制任务。

| 项 | 内容 |
|---|---|
| 调用类型 | action |
| Params Schema | `StopRecordingParams` |
| Result Schema | `StopRecordingResult` |
| 是否触发事件 | 是，状态实际变化后触发 `audio.recordingStateChanged`。 |
| 幂等性 / 异步性 | 建议幂等；重复提交相同目标状态应成功，可不重复触发事件。 |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_ARGUMENT`, `INVALID_STATE`, `BUSY`, `PERMISSION_DENIED` |

#### 3.2.1 请求参数 Params：`StopRecordingParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `target` | string | no | target id | `default` | 动作对象；具体 target 集合由 capability 声明。 |
| `reason` | string | no | caller-defined reason | omitted | 调用方给出的动作原因。 |

#### 3.2.2 返回结果 Result：`StopRecordingResult`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `accepted` | boolean | yes | `true`, `false` | none | 设备是否接受动作请求。 |
| `actionId` | string | no | opaque action id | omitted | 动作 ID，用于日志或异步关联。 |

#### 3.2.3 d block 示例

request:

```json
{
  "id": 102,
  "method": "audio.stopRecording",
  "params": {
    "target": "mic-array",
    "recordingId": "recording-20260621-001",
    "reason": "user_request"
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
    "fileId": "audio-file-001"
  }
}
```

#### 3.2.4 可能触发的事件

| Event | 触发条件 | Payload Schema | 客户端处理建议 |
|---|---|---|---|
| `audio.recordingStateChanged` | 该方法导致状态、配置或动作状态实际变化。 | `RecordingStateChangedEvent` | 可直接更新 UI；需要完整状态时调用对应 get method 校准。 |

#### 3.2.5 错误

| 错误 | 场景 | 返回建议 |
|---|---|---|
| `NOT_SUPPORTED` | 设备不支持该 feature、method、target 或 scope。 | 返回 unsupported feature/method/target。 |
| `INVALID_ARGUMENT` | 请求字段非法、枚举非法或范围非法。 | 返回具体字段路径和合法范围。 |
| `PERMISSION_DENIED` | 调用方无权执行该操作。 | 返回权限错误。 |
| `BUSY` | 设备正在处理冲突操作。 | 建议稍后重试。 |

### 3.3 `audio.cancelRecording`

**用途**：开始音频录制任务。

| 项 | 内容 |
|---|---|
| 调用类型 | action |
| Params Schema | `CancelRecordingParams` |
| Result Schema | `CancelRecordingResult` |
| 是否触发事件 | 是，状态实际变化后触发 `audio.recordingStateChanged`。 |
| 幂等性 / 异步性 | 建议幂等；重复提交相同目标状态应成功，可不重复触发事件。 |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_ARGUMENT`, `INVALID_STATE`, `BUSY`, `PERMISSION_DENIED` |

#### 3.3.1 请求参数 Params：`CancelRecordingParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `target` | string | no | target id | `default` | 动作对象；具体 target 集合由 capability 声明。 |
| `reason` | string | no | caller-defined reason | omitted | 调用方给出的动作原因。 |

#### 3.3.2 返回结果 Result：`CancelRecordingResult`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `accepted` | boolean | yes | `true`, `false` | none | 设备是否接受动作请求。 |
| `actionId` | string | no | opaque action id | omitted | 动作 ID，用于日志或异步关联。 |

#### 3.3.3 d block 示例

request:

```json
{
  "id": 103,
  "method": "audio.cancelRecording",
  "params": {
    "target": "mic-array",
    "recordingId": "recording-20260621-001",
    "reason": "discard_test_take"
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
    "discarded": true
  }
}
```

#### 3.3.4 可能触发的事件

| Event | 触发条件 | Payload Schema | 客户端处理建议 |
|---|---|---|---|
| `audio.recordingStateChanged` | 该方法导致状态、配置或动作状态实际变化。 | `RecordingStateChangedEvent` | 可直接更新 UI；需要完整状态时调用对应 get method 校准。 |

#### 3.3.5 错误

| 错误 | 场景 | 返回建议 |
|---|---|---|
| `NOT_SUPPORTED` | 设备不支持该 feature、method、target 或 scope。 | 返回 unsupported feature/method/target。 |
| `INVALID_ARGUMENT` | 请求字段非法、枚举非法或范围非法。 | 返回具体字段路径和合法范围。 |
| `PERMISSION_DENIED` | 调用方无权执行该操作。 | 返回权限错误。 |
| `BUSY` | 设备正在处理冲突操作。 | 建议稍后重试。 |

### 3.4 `audio.getRecordingState`

**用途**：开始音频录制任务。

| 项 | 内容 |
|---|---|
| 调用类型 | query |
| Params Schema | `GetRecordingStateParams` |
| Result Schema | `GetRecordingStateResult` |
| 是否触发事件 | 否 |
| 幂等性 / 异步性 | 幂等；同步返回当前快照。 |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_ARGUMENT`, `PERMISSION_DENIED`, `UNAVAILABLE` |

#### 3.4.1 请求参数 Params：`GetRecordingStateParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `target` | string | no | target id | `default` | 示例值 `mic-array`；查询对象。 |

#### 3.4.2 返回结果 Result：`GetRecordingStateResult`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `recordingId` | string | yes | see example | none | 标识符。 |
| `state` | string | yes | see example | none | 状态摘要对象。 |
| `elapsedMs` | uint32 | yes | see example | none | 录制已持续时间，单位毫秒。 |
| `outputFileId` | string | yes | see example | none | 标识符。 |
| `sampledAt` | string timestamp | no | RFC 3339 | omitted | 结果采样时间；客户端可用于缓存和校准。 |

#### 3.4.3 d block 示例

request:

```json
{
  "id": 104,
  "method": "audio.getRecordingState",
  "params": {
    "target": "mic-array"
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
    "recordingId": "recording-20260621-001",
    "state": "recording",
    "elapsedMs": 120000,
    "outputFileId": "audio-file-001"
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

### 3.5 `stream.open`

**用途**：开始音频录制任务。

| 项 | 内容 |
|---|---|
| 调用类型 | action |
| Params Schema | `OpenParams` |
| Result Schema | `OpenResult` |
| 是否触发事件 | 是，状态实际变化后触发 `audio.recordingStateChanged`。 |
| 幂等性 / 异步性 | 建议幂等；重复提交相同目标状态应成功，可不重复触发事件。 |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_ARGUMENT`, `INVALID_STATE`, `BUSY`, `PERMISSION_DENIED` |

#### 3.5.1 请求参数 Params：`OpenParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `target` | string | no | target id | `default` | 动作对象；具体 target 集合由 capability 声明。 |
| `reason` | string | no | caller-defined reason | omitted | 调用方给出的动作原因。 |

#### 3.5.2 返回结果 Result：`OpenResult`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `accepted` | boolean | yes | `true`, `false` | none | 设备是否接受动作请求。 |
| `actionId` | string | no | opaque action id | omitted | 动作 ID，用于日志或异步关联。 |

#### 3.5.3 d block 示例

request:

```json
{
  "id": 105,
  "method": "stream.open",
  "params": {
    "profile": "audio.recording",
    "target": "mic-array",
    "direction": "device_to_host"
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
    "streamId": "stream-audio-recording-1",
    "accepted": true
  }
}
```

#### 3.5.4 可能触发的事件

| Event | 触发条件 | Payload Schema | 客户端处理建议 |
|---|---|---|---|
| `audio.recordingStateChanged` | 该方法导致状态、配置或动作状态实际变化。 | `RecordingStateChangedEvent` | 可直接更新 UI；需要完整状态时调用对应 get method 校准。 |

#### 3.5.5 错误

| 错误 | 场景 | 返回建议 |
|---|---|---|
| `NOT_SUPPORTED` | 设备不支持该 feature、method、target 或 scope。 | 返回 unsupported feature/method/target。 |
| `INVALID_ARGUMENT` | 请求字段非法、枚举非法或范围非法。 | 返回具体字段路径和合法范围。 |
| `PERMISSION_DENIED` | 调用方无权执行该操作。 | 返回权限错误。 |
| `BUSY` | 设备正在处理冲突操作。 | 建议稍后重试。 |

### 3.6 `file.getInfo`

**用途**：开始音频录制任务。

| 项 | 内容 |
|---|---|
| 调用类型 | query |
| Params Schema | `GetInfoParams` |
| Result Schema | `GetInfoResult` |
| 是否触发事件 | 否 |
| 幂等性 / 异步性 | 幂等；同步返回当前快照。 |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_ARGUMENT`, `PERMISSION_DENIED`, `UNAVAILABLE` |

#### 3.6.1 请求参数 Params：`GetInfoParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `target` | string | no | target id | `default` | 查询对象。 |

#### 3.6.2 返回结果 Result：`GetInfoResult`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `fileId` | string | yes | see example | none | 文件 ID。 |
| `sizeBytes` | uint32 | yes | see example | none | 文件大小，单位 byte。 |
| `mediaType` | string | yes | see example | none | 媒体类型。 |
| `durationMs` | uint32 | yes | see example | none | 媒体时长，单位毫秒。 |
| `sampledAt` | string timestamp | no | RFC 3339 | omitted | 结果采样时间；客户端可用于缓存和校准。 |

#### 3.6.3 d block 示例

request:

```json
{
  "id": 106,
  "method": "file.getInfo",
  "params": {
    "fileId": "audio-file-001"
  }
}
```

success:

```json
{
  "id": 106,
  "status": {
    "ok": true,
    "code": 0
  },
  "result": {
    "fileId": "audio-file-001",
    "sizeBytes": 5242880,
    "mediaType": "audio/wav",
    "durationMs": 120000
  }
}
```

#### 3.6.4 可能触发的事件

| Event | 触发条件 | Payload Schema | 客户端处理建议 |
|---|---|---|---|
| 无 | query method 不应因查询触发状态变化事件。 | none | 无需处理。 |

#### 3.6.5 错误

| 错误 | 场景 | 返回建议 |
|---|---|---|
| `NOT_SUPPORTED` | 设备不支持该 feature、method、target 或 scope。 | 返回 unsupported feature/method/target。 |
| `INVALID_ARGUMENT` | 请求字段非法、枚举非法或范围非法。 | 返回具体字段路径和合法范围。 |
| `PERMISSION_DENIED` | 调用方无权执行该操作。 | 返回权限错误。 |
| `BUSY` | 设备正在处理冲突操作。 | 建议稍后重试。 |

### 3.7 `file.download`

**用途**：开始音频录制任务。

| 项 | 内容 |
|---|---|
| 调用类型 | action |
| Params Schema | `DownloadParams` |
| Result Schema | `DownloadResult` |
| 是否触发事件 | 是，状态实际变化后触发 `audio.recordingStateChanged`。 |
| 幂等性 / 异步性 | 建议幂等；重复提交相同目标状态应成功，可不重复触发事件。 |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_ARGUMENT`, `INVALID_STATE`, `BUSY`, `PERMISSION_DENIED` |

#### 3.7.1 请求参数 Params：`DownloadParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `target` | string | no | target id | `default` | 动作对象；具体 target 集合由 capability 声明。 |
| `reason` | string | no | caller-defined reason | omitted | 调用方给出的动作原因。 |

#### 3.7.2 返回结果 Result：`DownloadResult`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `accepted` | boolean | yes | `true`, `false` | none | 设备是否接受动作请求。 |
| `actionId` | string | no | opaque action id | omitted | 动作 ID，用于日志或异步关联。 |

#### 3.7.3 d block 示例

request:

```json
{
  "id": 107,
  "method": "file.download",
  "params": {
    "fileId": "audio-file-001",
    "transferMode": "stream"
  }
}
```

success:

```json
{
  "id": 107,
  "status": {
    "ok": true,
    "code": 0
  },
  "result": {
    "accepted": true,
    "streamId": "stream-file-audio-001"
  }
}
```

#### 3.7.4 可能触发的事件

| Event | 触发条件 | Payload Schema | 客户端处理建议 |
|---|---|---|---|
| `audio.recordingStateChanged` | 该方法导致状态、配置或动作状态实际变化。 | `RecordingStateChangedEvent` | 可直接更新 UI；需要完整状态时调用对应 get method 校准。 |

#### 3.7.5 错误

| 错误 | 场景 | 返回建议 |
|---|---|---|
| `NOT_SUPPORTED` | 设备不支持该 feature、method、target 或 scope。 | 返回 unsupported feature/method/target。 |
| `INVALID_ARGUMENT` | 请求字段非法、枚举非法或范围非法。 | 返回具体字段路径和合法范围。 |
| `PERMISSION_DENIED` | 调用方无权执行该操作。 | 返回权限错误。 |
| `BUSY` | 设备正在处理冲突操作。 | 建议稍后重试。 |

### 3.8 `file.delete`

**用途**：开始音频录制任务。

| 项 | 内容 |
|---|---|
| 调用类型 | action |
| Params Schema | `DeleteParams` |
| Result Schema | `DeleteResult` |
| 是否触发事件 | 是，状态实际变化后触发 `audio.recordingStateChanged`。 |
| 幂等性 / 异步性 | 建议幂等；重复提交相同目标状态应成功，可不重复触发事件。 |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_ARGUMENT`, `INVALID_STATE`, `BUSY`, `PERMISSION_DENIED` |

#### 3.8.1 请求参数 Params：`DeleteParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `target` | string | no | target id | `default` | 动作对象；具体 target 集合由 capability 声明。 |
| `reason` | string | no | caller-defined reason | omitted | 调用方给出的动作原因。 |

#### 3.8.2 返回结果 Result：`DeleteResult`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `accepted` | boolean | yes | `true`, `false` | none | 设备是否接受动作请求。 |
| `actionId` | string | no | opaque action id | omitted | 动作 ID，用于日志或异步关联。 |

#### 3.8.3 d block 示例

request:

```json
{
  "id": 108,
  "method": "file.delete",
  "params": {
    "fileId": "audio-file-001",
    "reason": "retention_policy"
  }
}
```

success:

```json
{
  "id": 108,
  "status": {
    "ok": true,
    "code": 0
  },
  "result": {
    "accepted": true,
    "deleted": true
  }
}
```

#### 3.8.4 可能触发的事件

| Event | 触发条件 | Payload Schema | 客户端处理建议 |
|---|---|---|---|
| `audio.recordingStateChanged` | 该方法导致状态、配置或动作状态实际变化。 | `RecordingStateChangedEvent` | 可直接更新 UI；需要完整状态时调用对应 get method 校准。 |

#### 3.8.5 错误

| 错误 | 场景 | 返回建议 |
|---|---|---|
| `NOT_SUPPORTED` | 设备不支持该 feature、method、target 或 scope。 | 返回 unsupported feature/method/target。 |
| `INVALID_ARGUMENT` | 请求字段非法、枚举非法或范围非法。 | 返回具体字段路径和合法范围。 |
| `PERMISSION_DENIED` | 调用方无权执行该操作。 | 返回权限错误。 |
| `BUSY` | 设备正在处理冲突操作。 | 建议稍后重试。 |

### 3.9 `audio.getRecordingCapabilities`

**用途**：查询录制能力、默认值和约束。

| 项 | 内容 |
|---|---|
| 调用类型 | query |
| Params Schema | `GetRecordingCapabilitiesParams` |
| Result Schema | `GetRecordingCapabilitiesResult` |
| 是否触发事件 | 否 |
| 幂等性 / 异步性 | 幂等；同步返回当前快照。 |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_ARGUMENT`, `PERMISSION_DENIED`, `UNAVAILABLE` |

#### 3.9.1 请求参数 Params：`GetRecordingCapabilitiesParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `target` | string | no | target id | `default` | 示例值 `mic-array`；查询对象。 |

#### 3.9.2 返回结果 Result：`GetRecordingCapabilitiesResult`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `capability` | string | yes | see example | none | capability 名称。 |
| `containers` | string[] | yes | see example | none | 列表。 |
| `maxDurationSeconds` | uint32 | yes | see example | none | 支持的最大录制时长，单位秒。 |
| `streamDownloadSupported` | bool | yes | see example | none | 是否支持该能力。 |
| `sampledAt` | string timestamp | no | RFC 3339 | omitted | 结果采样时间；客户端可用于缓存和校准。 |

#### 3.9.3 d block 示例

request:

```json
{
  "id": 109,
  "method": "audio.getRecordingCapabilities",
  "params": {
    "target": "mic-array"
  }
}
```

success:

```json
{
  "id": 109,
  "status": {
    "ok": true,
    "code": 0
  },
  "result": {
    "capability": "audio.recording",
    "containers": [
      "wav",
      "aac"
    ],
    "maxDurationSeconds": 7200,
    "streamDownloadSupported": true
  }
}
```

#### 3.9.4 可能触发的事件

| Event | 触发条件 | Payload Schema | 客户端处理建议 |
|---|---|---|---|
| 无 | query method 不应因查询触发状态变化事件。 | none | 无需处理。 |

#### 3.9.5 错误

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
| `audio.recordingStateChanged` | 录制状态、任务 ID 或结果文件变化 | `RecordingStateChangedEvent` | 更新 UI 或调用对应 get method 校准 | draft |
| `audio.recordingProgressReported` | 音频录制进度或已写入字节数上报 | `RecordingProgressReportedEvent` | 更新 UI 或调用对应 get method 校准 | draft |

### 4.1 `audio.recordingStateChanged`

**触发条件**：录制状态、任务 ID 或结果文件变化。

#### 4.1.1 Payload：`RecordingStateChangedEvent`

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
  "event": "audio.recordingStateChanged",
  "intent": 1,
  "data": {
    "changedFields": [
      "state",
      "elapsedMs"
    ],
    "state": {
      "target": "audio-recorder",
      "profile": "speech",
      "container": "wav",
      "maxDurationSeconds": 1800
    },
    "source": "remoteApp",
    "reason": "recording_progress",
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

### 4.2 `audio.recordingProgressReported`

**触发条件**：音频录制进度或已写入字节数上报。

#### 4.2.1 Payload：`RecordingProgressReportedEvent`

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
  "event": "audio.recordingProgressReported",
  "intent": 1,
  "data": {
    "changedFields": [
      "state",
      "elapsedMs"
    ],
    "state": {
      "target": "audio-recorder",
      "profile": "speech",
      "container": "wav",
      "maxDurationSeconds": 1800
    },
    "source": "remoteApp",
    "reason": "recording_progress",
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

Capability name: `audio.recording`。

设备通过 capability 声明是否支持该 feature，以及支持哪些范围、模式、对象或约束。Capability 字段只描述“设备能做什么”，不得混入 method params/result 或 event payload。

| 能力字段 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `capability` | string | yes | fixed `audio.recording` | none | capability 名称。 |
| `supportedTargets` | string[] | no | target id array | omitted | 支持的对象、通道、端口、组件或 scope。 |

## 6. 字段 / Schemas

### 6.1 Schema 层级速览

```text
RecordingCapability
  capability / supportedTargets
RecordingState
  target / status / sampledAt
RecordingChangedEvent
  changedFields / state / source / reason / stateRevision
```

### 6.2 请求与响应 Schemas

| Schema | 用途 | 字段定义 |
|---|---|---|
| `StartRecordingParams` | `audio.startRecording` request params | 见 `audio.startRecording` 方法小节。 |
| `StartRecordingResult` | `audio.startRecording` result | 见 `audio.startRecording` 方法小节。 |
| `StopRecordingParams` | `audio.stopRecording` request params | 见 `audio.stopRecording` 方法小节。 |
| `StopRecordingResult` | `audio.stopRecording` result | 见 `audio.stopRecording` 方法小节。 |
| `CancelRecordingParams` | `audio.cancelRecording` request params | 见 `audio.cancelRecording` 方法小节。 |
| `CancelRecordingResult` | `audio.cancelRecording` result | 见 `audio.cancelRecording` 方法小节。 |
| `GetRecordingStateParams` | `audio.getRecordingState` request params | 见 `audio.getRecordingState` 方法小节。 |
| `GetRecordingStateResult` | `audio.getRecordingState` result | 见 `audio.getRecordingState` 方法小节。 |
| `OpenParams` | `stream.open` request params | 见 `stream.open` 方法小节。 |
| `OpenResult` | `stream.open` result | 见 `stream.open` 方法小节。 |
| `GetInfoParams` | `file.getInfo` request params | 见 `file.getInfo` 方法小节。 |
| `GetInfoResult` | `file.getInfo` result | 见 `file.getInfo` 方法小节。 |
| `DownloadParams` | `file.download` request params | 见 `file.download` 方法小节。 |
| `DownloadResult` | `file.download` result | 见 `file.download` 方法小节。 |
| `DeleteParams` | `file.delete` request params | 见 `file.delete` 方法小节。 |
| `DeleteResult` | `file.delete` result | 见 `file.delete` 方法小节。 |
| `GetRecordingCapabilitiesParams` | `audio.getRecordingCapabilities` request params | 见 `audio.getRecordingCapabilities` 方法小节。 |
| `GetRecordingCapabilitiesResult` | `audio.getRecordingCapabilities` result | 见 `audio.getRecordingCapabilities` 方法小节。 |

### 6.3 Capability Schemas


### 6.4 Event Schemas

| Schema | Event | 字段定义 |
|---|---|---|
| `RecordingStateChangedEvent` | `audio.recordingStateChanged` | 见 `audio.recordingStateChanged` 事件小节。 |
| `RecordingProgressReportedEvent` | `audio.recordingProgressReported` | 见 `audio.recordingProgressReported` 事件小节。 |
