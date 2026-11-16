---
status: draft
contract: false
generated: false
domain: stream
feature: stream.profile
registry:
lastReviewed: 2026-06-15
---

# stream.profile

## 0. 速读结论

| 项目 | 内容 |
|---|---|
| 这个能力做什么 | STREAM profile 查询、profile 能力和低带宽/业务数据面档案约束。 |
| 当前状态 | draft |
| 是否可直接实现 | 否。本文是 protocol draft；正式实现以 registry / generated 为准。 |
| 主要交互 | RPC |
| 是否使用 STREAM | 是 |
| Registry readiness | partial |
| Conformance | needed |
| 主要未决问题 | schema 字段、错误模型、legacy 映射和 conformance case 仍需人工确认。 |

## 1. 功能说明

`stream.profile` 用于STREAM profile 查询、profile 能力和低带宽/业务数据面档案约束。

## 2. 能力边界

| 类型 | 内容 |
|---|---|
| 包含 | stream.profile 的能力发现、配置、状态、动作或事件。 |
| 包含 | 与 stream.profile 直接相关的 method/event/schema 草案。 |
| 包含 | 已确认 legacy 协议到 stream.profile 的语义归类。 |
| 不包含 | 不承载其他 capability feature 的业务语义；跨域关系通过 schema 字段、引用或数据面 stream/file 表达。 |
| 不包含 | method/event 数值 ID 分配；数值以 contract/registry/generated 为准。 |
| 不包含 | 未确认旧协议 payload 的稳定映射。 |
| 数据面 | 本 feature 可能涉及 STREAM，但具体 streamId 建立、关闭和 payload 语义必须由业务 method 或 core STREAM 规范确认。 |

## 3. 方法 Methods

### 3.0 方法速览

| Method | 调用类型 | 用途 | Params Schema | Result Schema | 是否触发事件 | 状态 |
|---|---|---|---|---|---|---|
| `stream.getProfiles` | query | 查询 stream profile 列表 | `GetProfilesParams` | `GetProfilesResult` | 否 | draft |
| `stream.getProfileCapabilities` | query | 查询 profile 能力和限制 | `GetProfileCapabilitiesParams` | `GetProfileCapabilitiesResult` | 否 | draft |

### 3.1 `stream.getProfiles`

**用途**：查询 stream profile 列表。

| 项 | 内容 |
|---|---|
| 调用类型 | query |
| Params Schema | `GetProfilesParams` |
| Result Schema | `GetProfilesResult` |
| 是否触发事件 | 否 |
| 幂等性 / 异步性 | 幂等；同步返回当前快照。 |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_ARGUMENT`, `PERMISSION_DENIED`, `UNAVAILABLE` |

#### 3.1.1 请求参数 Params：`GetProfilesParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `target` | string | no | target id | `default` | 示例值 `transport`；查询对象。 |

#### 3.1.2 返回结果 Result：`GetProfilesResult`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `state` | object | yes | see schema | none | 当前结果对象；示例字段包括 `target`、`profiles`。 |
| `sampledAt` | string timestamp | no | RFC 3339 | omitted | 结果采样时间；客户端可用于缓存和校准。 |

#### 3.1.3 d block 示例

request:

```json
{
  "id": 101,
  "method": "stream.getProfiles",
  "params": {
    "target": "transport"
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
      "target": "transport",
      "profiles": [
        {
          "profile": "stream.lowBandwidth",
          "payloadKinds": [
            "firmware.update",
            "log.export"
          ],
          "maxChunkBytes": 1024,
          "resumeSupported": true
        },
        {
          "profile": "stream.media",
          "payloadKinds": [
            "audio.recording",
            "video.recording"
          ],
          "maxChunkBytes": 16384,
          "resumeSupported": true
        }
      ]
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

### 3.2 `stream.getProfileCapabilities`

**用途**：查询 profile 能力和限制。

| 项 | 内容 |
|---|---|
| 调用类型 | query |
| Params Schema | `GetProfileCapabilitiesParams` |
| Result Schema | `GetProfileCapabilitiesResult` |
| 是否触发事件 | 否 |
| 幂等性 / 异步性 | 幂等；同步返回当前快照。 |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_ARGUMENT`, `PERMISSION_DENIED`, `UNAVAILABLE` |

#### 3.2.1 请求参数 Params：`GetProfileCapabilitiesParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `target` | string | no | target id | `default` | 查询对象。 |

#### 3.2.2 返回结果 Result：`GetProfileCapabilitiesResult`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `state` | object | yes | see schema | none | 当前结果对象；示例字段包括 `profile`、`maxStreamCount`、`maxChunkBytes`、`ackPolicy`、`resumeSupported`、`crcRequired`。 |
| `sampledAt` | string timestamp | no | RFC 3339 | omitted | 结果采样时间；客户端可用于缓存和校准。 |

#### 3.2.3 d block 示例

request:

```json
{
  "id": 102,
  "method": "stream.getProfileCapabilities",
  "params": {
    "profile": "stream.lowBandwidth"
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
      "profile": "stream.lowBandwidth",
      "maxStreamCount": 2,
      "maxChunkBytes": 1024,
      "ackPolicy": "required",
      "resumeSupported": true,
      "crcRequired": true
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

## 4. 事件 Events

### 4.0 事件速览

| Event | 触发条件 | Payload Schema | 客户端处理建议 | 状态 |
|---|---|---|---|---|
| 无 | 当前草案暂不定义事件 | none | 无需订阅；客户端按需轮询 get method | draft |

### 4.1 无事件

当前草案暂不定义事件。若后续业务需要异步状态通知，应新增明确的 event payload schema，并补齐 conformance case。

## 5. Capability

Capability name: `stream.profile`。

设备通过 capability 声明是否支持该 feature，以及支持哪些范围、模式、对象或约束。Capability 字段只描述“设备能做什么”，不得混入 method params/result 或 event payload。

| 能力字段 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `capability` | string | yes | fixed `stream.profile` | none | capability 名称。 |
| `supportedTargets` | string[] | no | target id array | omitted | 支持的对象、通道、端口、组件或 scope。 |

## 6. 字段 / Schemas

### 6.1 Schema 层级速览

```text
ProfileCapability
  capability / supportedTargets
ProfileState
  target / status / sampledAt
ProfileChangedEvent
  changedFields / state / source / reason / stateRevision
```

### 6.2 请求与响应 Schemas

| Schema | 用途 | 字段定义 |
|---|---|---|
| `GetProfilesParams` | `stream.getProfiles` request params | 见 `stream.getProfiles` 方法小节。 |
| `GetProfilesResult` | `stream.getProfiles` result | 见 `stream.getProfiles` 方法小节。 |
| `GetProfileCapabilitiesParams` | `stream.getProfileCapabilities` request params | 见 `stream.getProfileCapabilities` 方法小节。 |
| `GetProfileCapabilitiesResult` | `stream.getProfileCapabilities` result | 见 `stream.getProfileCapabilities` 方法小节。 |

### 6.3 Capability Schemas


### 6.4 Event Schemas

当前草案暂不定义 event payload schema。
