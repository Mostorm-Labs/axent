---
status: generated
contract: true
generated: true
domain: cast
feature: cast.session
registry: ../../../../contract/registry/domains/cast/domain.yaml
lastReviewed: 2026-06-22
---

# cast.session

## 0. 采纳状态

| 项目 | 内容 |
|---|---|
| 当前状态 | generated；已写入 `../../../../contract/registry/domains/cast/domain.yaml`，并已刷新到 `contract/protocol/axtp.protocol.yaml` 与 `contract/generated/**`。 |
| 是否可直接实现 | 是，但实现合同以 `contract/protocol/axtp.protocol.yaml` / `contract/generated/**` 为准。 |
| 本次采纳 | `receiverPhase` 阶段骨架、AirPlay 名称、session stop、incoming/started/stopped/failed 事件和字段编号。 |
| 未采纳 | AirPlay 名称 schema、无活动会话 stop 行为等 Review Items 不属于已生成合同；后续语义变更走 `amend-adopted-protocol`。 |

## 1. Purpose

管理 AirPlay receiver 名称、接收端统一阶段和 UxPlay 投屏会话生命周期。`receiverPhase` 用于 UI / 重连校准和跨协议对齐；AirPlay 细节仍保留在 `sessionState`。窗口、音频、PIN、backend 和本地流控分别归属其他 `cast.*` feature。

## 2. Candidate Surface

| Method / Event | Purpose | Schema | Notes |
|---|---|---|---|
| `cast.getSession` | 查询当前 receiver 和活动会话摘要。 | `CastGetSessionParams` -> `CastSessionState` | query |
| `cast.stopSession` | 停止当前活动投屏。 | `CastStopSessionParams` -> `CastStopSessionResult` | command |
| `cast.getAirPlayName` | 查询 AirPlay 显示名称。 | `CastGetAirPlayNameParams` -> `CastAirPlayNameState` | query |
| `cast.setAirPlayName` | 设置显示名称并重新发布服务发现名称。 | `CastSetAirPlayNameParams` -> `CastAirPlayNameState` | command |
| `cast.sessionIncoming` | source 发起连接。 | `CastSessionIncomingEvent` | event |
| `cast.sessionStateChanged` | 接收端阶段、AirPlay 细节状态或低频媒体状态变化。 | `CastSessionStateChangedEvent` | event |
| `cast.sessionStarted` | 首帧渲染或本地播放开始，用户可见投屏已开始。 | `CastSessionStartedEvent` | event |
| `cast.sessionStopped` | 会话结束。 | `CastSessionStoppedEvent` | event |
| `cast.sessionFailed` | 连接、鉴权、协商或 backend 处理失败。 | `CastSessionFailedEvent` | event |

## 3. Methods

### 3.0 方法速览

方法概览见第 2 章；本节只保留每个 method 的最小 request / success 示例。

### 3.1 `cast.getSession`

查询当前 receiver、活动会话和可选摘要段。

#### 3.1.1 d block 示例

request:

```json
{
  "id": 3101,
  "method": "cast.getSession",
  "params": {
    "include": [
      "source",
      "media",
      "airPlayName"
    ]
  }
}
```

success:

```json
{
  "id": 3101,
  "status": {
    "ok": true,
    "code": 0
  },
  "result": {
    "receiverState": "ready",
    "sessionId": "cast_sess_001",
    "receiverPhase": "rendering",
    "sessionState": "casting",
    "protocol": "airplay",
    "airPlayName": "NearCast Room 01",
    "source": {
      "name": "Qing iPhone",
      "model": "iPhone17,2",
      "address": "192.168.31.24"
    },
    "media": {
      "firstFrame": true,
      "width": 1920,
      "height": 1080,
      "orientation": "landscape",
      "inputFps": 25
    },
    "backendState": "ready",
    "reason": "sessionStarted",
    "updatedAt": "2026-06-22T10:30:00Z"
  }
}
```

### 3.2 `cast.stopSession`

停止当前活动投屏；无活动会话时返回成功还是错误仍待确认。

#### 3.2.1 d block 示例

request:

```json
{
  "id": 3102,
  "method": "cast.stopSession",
  "params": {
    "reason": "externalRequest"
  }
}
```

success:

```json
{
  "id": 3102,
  "status": {
    "ok": true,
    "code": 0
  },
  "result": {
    "accepted": true,
    "sessionId": "cast_sess_001",
    "previousReceiverPhase": "rendering",
    "receiverPhase": "stopping",
    "previousState": "casting",
    "sessionState": "stopping",
    "reason": "externalRequest",
    "noActiveSession": false,
    "updatedAt": "2026-06-22T10:31:00Z"
  }
}
```

### 3.3 `cast.getAirPlayName`

查询当前 AirPlay 显示名称及其来源。

#### 3.3.1 d block 示例

request:

```json
{
  "id": 3103,
  "method": "cast.getAirPlayName",
  "params": {}
}
```

success:

```json
{
  "id": 3103,
  "status": {
    "ok": true,
    "code": 0
  },
  "result": {
    "displayName": "NearCast Room 01",
    "source": "configured",
    "publishState": "published",
    "backendType": "uxplay",
    "updatedAt": "2026-06-22T10:30:00Z"
  }
}
```

### 3.4 `cast.setAirPlayName`

设置 AirPlay 显示名称，并要求 backend 重新发布 mDNS / Bonjour 名称。

#### 3.4.1 d block 示例

request:

```json
{
  "id": 3104,
  "method": "cast.setAirPlayName",
  "params": {
    "displayName": "NearCast Room 01",
    "apply": "immediate"
  }
}
```

success:

```json
{
  "id": 3104,
  "status": {
    "ok": true,
    "code": 0
  },
  "result": {
    "displayName": "NearCast Room 01",
    "previousDisplayName": "NearCast",
    "source": "configured",
    "apply": "immediate",
    "publishState": "republishing",
    "backendType": "uxplay",
    "updatedAt": "2026-06-22T10:32:00Z"
  }
}
```

## 4. State And Events

| Field | Meaning |
|---|---|
| `receiverPhase` | 跨协议接收端阶段：`idle`、`incoming`、`authenticating`、`streamStarting`、`streaming`、`rendering`、`interrupted`、`stopping`、`ended`、`failed`。 |
| `sessionState` | AirPlay / UxPlay 细节状态：`idle`、`incoming`、`waitingForPassword`、`authenticated`、`preparing`、`casting`、`interrupted`、`stopping`、`ended`、`failed`。 |
| `sessionId` | receiver-local 会话 id。 |
| `source` | source 名称、地址或设备摘要。 |
| `protocol` | 当前为 `airplay`。 |
| `airPlayName` | 当前发布的显示名称。 |
| `media` | 低频媒体摘要，例如分辨率、fps、first frame 状态。 |
| `reason` | `externalRequest`、`sourceClosed`、`backendRestart`、`backendExited`、`authFailed`、`error`。 |

事件拆分：

- `sessionIncoming`：source 发起连接。
- `sessionStateChanged`：`receiverPhase`、等待密码、鉴权、preparing、streaming、interrupted、stopping 等阶段变化。
- `sessionStarted`：正式进入 `receiverPhase=rendering`。
- `sessionStopped`：正常或强制结束。
- `sessionFailed`：连接、鉴权、媒体协商或 backend 异常失败。

`receiverPhase=streaming` 表示媒体数据已经开始流动；`receiverPhase=rendering` 表示 Render Core 已经渲染首帧或本地音频播放已经开始。HID/NA20 聚合状态可以复用同一阶段集合，但其 source / stream 细节仍由 `video.stream` 和 `audio.stream` 表达。

### 4.1 Event 示例

started:

```json
{
  "event": "cast.sessionStarted",
  "intent": 1,
  "data": {
    "sessionId": "cast_sess_001",
    "receiverPhase": "rendering",
    "sessionState": "casting",
    "protocol": "airplay",
    "source": {
      "name": "Qing iPhone",
      "address": "192.168.31.24"
    },
    "media": {
      "firstFrame": true,
      "width": 1920,
      "height": 1080,
      "orientation": "landscape",
      "inputFps": 25.0
    },
    "startedAt": "2026-06-22T10:30:00Z"
  }
}
```

state changed:

```json
{
  "event": "cast.sessionStateChanged",
  "intent": 1,
  "data": {
    "sessionId": "cast_sess_001",
    "previousReceiverPhase": "streamStarting",
    "receiverPhase": "streaming",
    "previousState": "preparing",
    "sessionState": "casting",
    "protocol": "airplay",
    "authRequired": false,
    "media": {
      "firstFrame": false,
      "inputFps": 25.0,
      "renderFps": 0.0
    },
    "reason": "mediaFlowStarted",
    "updatedAt": "2026-06-22T10:29:58Z"
  }
}
```

stopped by backend restart:

```json
{
  "event": "cast.sessionStopped",
  "intent": 1,
    "data": {
      "sessionId": "cast_sess_001",
      "previousReceiverPhase": "rendering",
      "receiverPhase": "ended",
      "previousState": "casting",
      "sessionState": "ended",
      "reason": "backendRestart",
    "backendType": "uxplay",
    "stoppedAt": "2026-06-22T10:33:00Z"
  }
}
```

## 5. Rules

- AirPlay 名称属于 `cast.session`，不新增 `cast.runtime`。
- `stopSession` 和 AirPlay 名称设置按朴素可调用状态操作设计，不在本 feature 内拆分权限 scope。
- backend 重启会强制结束活动会话，并使用 `reason=backendRestart`。
- PIN 等待和鉴权失败不得误报为 `receiverPhase=streaming` 或 `receiverPhase=rendering`。
- `sessionState` 是 AirPlay / UxPlay 细节状态，不能作为 HID/NA20 等其他投屏路径的唯一 UI 状态模型。
- 高频 frame stats 不进入 `cast.session`；诊断统计优先放到 `cast.flowControl`。

## 6. Errors

| Error | Scenario |
|---|---|
| `INVALID_ARGUMENT` | AirPlay 名称、include 段或 reason 非法。 |
| `INVALID_STATE` | 无活动会话、会话已结束或当前状态不能停止。 |
| `UNAVAILABLE` | backend 或服务发现不可用。 |

## 7. Review Items

| 问题 | 影响 | 当前建议 | 状态 |
|---|---|---|---|
| AirPlay 名称长度、字符集、立即生效策略和失败恢复如何定义？ | backend / schema | 采纳前补 name schema 和 republish 失败语义。 | `[REVIEW-ASK]` |
| `receiverPhase` 是否完整采用当前集合？ | schema / conformance | 先采用统一阶段集合；采纳前补 conformance case。 | `[REVIEW-DRAFT]` |
| `stopSession` 无活动会话时幂等成功还是错误？ | behavior | 待产品确认。 | `[REVIEW-ASK]` |
| first frame / interrupted 是否需要专门事件？ | event model | first frame 由 `sessionStarted(receiverPhase=rendering)` 表达；interrupted 默认并入 `sessionStateChanged`。 | `[REVIEW-DRAFT]` |

## 8. Schema Reference

> 本节按当前 `contract/registry/domains/cast/domain.yaml` 整理字段事实；`Required=yes` 表示编码数据必须携带该字段，`Required=no` 表示可省略。`Empty` schema 无字段，未展开。

### CastGetSessionParams

Selector for cast receiver and active session summary.

| Field | Required | Type | Field ID | Constraints / default | Description |
|---|---:|---|---:|---|---|
| `include` | no | `Array<string>` | `0x01` | itemType=string | Optional summary sections, such as source, media, or airPlayName. |
| `sessionId` | no | `string` | `0x02` | maxLength=128 | Optional receiver-local session id to query. |

### CastSessionState

Current receiver phase and active session state.

| Field | Required | Type | Field ID | Constraints / default | Description |
|---|---:|---|---:|---|---|
| `receiverState` | yes | `enum` | `0x01` | enum=disabled/starting/ready/busy/failed | Receiver service availability state. |
| `sessionId` | no | `string` | `0x02` | maxLength=128 | Receiver-local active session id. |
| `receiverPhase` | yes | `enum` | `0x03` | enum=idle/incoming/authenticating/streamStarting/streaming/rendering/interrupted/stopping/ended/failed | Protocol-neutral receiver phase used by UI and reconnection calibration. |
| `sessionState` | no | `enum` | `0x04` | enum=idle/incoming/waitingForPassword/authenticated/preparing/casting/interrupted/stopping/ended/failed | AirPlay or backend-specific session state detail. |
| `protocol` | no | `enum` | `0x05` | enum=airplay/hid/unknown | Cast protocol path currently represented by this state. |
| `airPlayName` | no | `string` | `0x06` | maxLength=128 | Current published AirPlay receiver display name. |
| `source` | no | `CastSourceSummary` | `0x07` | - | Source device summary. |
| `media` | no | `CastMediaSummary` | `0x08` | - | Low-frequency media summary. |
| `backendState` | no | `enum` | `0x09` | enum=starting/ready/restarting/exited/failed/disabled | Current backend state summary. |
| `reason` | no | `enum` | `0x0A` | enum=sessionStarted/mediaFlowStarted/externalRequest/sourceClosed/backendRestart/backendExited/authFailed/error/unknown | Last state transition reason. |
| `authRequired` | no | `bool` | `0x0B` | - | Whether this session path currently requires authentication. |
| `updatedAt` | no | `string` | `0x0C` | maxLength=64 | Timestamp for this state snapshot. |

### CastSourceSummary

Summary of a cast source device or local AXTP sender.

| Field | Required | Type | Field ID | Constraints / default | Description |
|---|---:|---|---:|---|---|
| `name` | no | `string` | `0x01` | maxLength=128 | User-visible source name when known. |
| `model` | no | `string` | `0x02` | maxLength=128 | Source model identifier when known. |
| `address` | no | `string` | `0x03` | maxLength=128 | Network or transport address summary when safe to expose. |
| `sourceId` | no | `string` | `0x04` | maxLength=128 | Receiver-local source identifier. |
| `protocol` | no | `enum` | `0x05` | enum=airplay/hid/unknown | Protocol path that produced the source summary. |

### CastMediaSummary

Low-frequency media summary for a cast session.

| Field | Required | Type | Field ID | Constraints / default | Description |
|---|---:|---|---:|---|---|
| `firstFrame` | no | `bool` | `0x01` | - | Whether the first visible frame has rendered. |
| `width` | no | `uint32` | `0x02` | - | Current media width in pixels. |
| `height` | no | `uint32` | `0x03` | - | Current media height in pixels. |
| `orientation` | no | `enum` | `0x04` | enum=landscape/portrait/unknown | Current media orientation summary. |
| `inputFps` | no | `number` | `0x05` | min=0 | Estimated incoming media frame rate. |
| `renderFps` | no | `number` | `0x06` | min=0 | Estimated local render frame rate. |
| `audioActive` | no | `bool` | `0x07` | - | Whether local receiver audio output is active. |

### CastStopSessionParams

Request to stop an active cast session.

| Field | Required | Type | Field ID | Constraints / default | Description |
|---|---:|---|---:|---|---|
| `sessionId` | no | `string` | `0x01` | maxLength=128 | Optional receiver-local session id; omitted means current active session. |
| `reason` | no | `enum` | `0x02` | enum=externalRequest/localUi/backendRestart/shutdown/unknown | Caller-visible reason for stopping the session. |
| `force` | no | `bool` | `0x03` | default=false | Whether the receiver may force backend/session cleanup. |

### CastStopSessionResult

Result of a cast session stop request.

| Field | Required | Type | Field ID | Constraints / default | Description |
|---|---:|---|---:|---|---|
| `accepted` | yes | `bool` | `0x01` | - | Whether the receiver accepted the stop request. |
| `sessionId` | no | `string` | `0x02` | maxLength=128 | Session affected by the request. |
| `previousReceiverPhase` | no | `enum` | `0x03` | enum=idle/incoming/authenticating/streamStarting/streaming/rendering/interrupted/stopping/ended/failed | Receiver phase before the stop transition. |
| `receiverPhase` | yes | `enum` | `0x04` | enum=idle/incoming/authenticating/streamStarting/streaming/rendering/interrupted/stopping/ended/failed | Receiver phase after accepting the stop request. |
| `previousState` | no | `enum` | `0x05` | enum=idle/incoming/waitingForPassword/authenticated/preparing/casting/interrupted/stopping/ended/failed | Backend-specific state before the stop transition. |
| `sessionState` | no | `enum` | `0x06` | enum=idle/incoming/waitingForPassword/authenticated/preparing/casting/interrupted/stopping/ended/failed | Backend-specific state after accepting the stop request. |
| `reason` | no | `enum` | `0x07` | enum=externalRequest/sourceClosed/backendRestart/backendExited/shutdown/unknown | Applied stop reason. |
| `noActiveSession` | no | `bool` | `0x08` | - | Whether no active session existed when the request was processed. |
| `updatedAt` | no | `string` | `0x09` | maxLength=64 | Timestamp for the result. |

### CastAirPlayNameState

AirPlay display name and backend publish state.

| Field | Required | Type | Field ID | Constraints / default | Description |
|---|---:|---|---:|---|---|
| `displayName` | yes | `string` | `0x01` | maxLength=128 | Current AirPlay display name. |
| `previousDisplayName` | no | `string` | `0x02` | maxLength=128 | Previous display name when a change was applied. |
| `source` | no | `enum` | `0x03` | enum=configured/default/backend/unknown | Source of the current display name. |
| `apply` | no | `enum` | `0x04` | enum=immediate/onNextBackendStart | Apply timing used for the latest update. |
| `publishState` | yes | `enum` | `0x05` | enum=published/republishing/pending/failed/unpublished | Bonjour or backend service publish state. |
| `backendType` | no | `enum` | `0x06` | enum=uxplay/unknown | Backend that owns the published name. |
| `updatedAt` | no | `string` | `0x07` | maxLength=64 | Timestamp for this name state. |

### CastSetAirPlayNameParams

Request to set the AirPlay receiver display name.

| Field | Required | Type | Field ID | Constraints / default | Description |
|---|---:|---|---:|---|---|
| `displayName` | yes | `string` | `0x01` | maxLength=128 | Target AirPlay display name. |
| `apply` | no | `enum` | `0x02` | enum=immediate/onNextBackendStart | Requested backend apply timing. |

### CastSessionIncomingEvent

Event payload for a new incoming cast session.

| Field | Required | Type | Field ID | Constraints / default | Description |
|---|---:|---|---:|---|---|
| `sessionId` | no | `string` | `0x01` | maxLength=128 | Receiver-local session id assigned to the incoming session. |
| `receiverPhase` | yes | `enum` | `0x02` | enum=incoming/authenticating | Receiver phase entered for the incoming session. |
| `protocol` | no | `enum` | `0x03` | enum=airplay/hid/unknown | Protocol path used by the incoming session. |
| `source` | no | `CastSourceSummary` | `0x04` | - | Source summary when available. |
| `authRequired` | no | `bool` | `0x05` | - | Whether this incoming session requires authentication. |
| `incomingAt` | no | `string` | `0x06` | maxLength=64 | Timestamp when the incoming session was observed. |

### CastSessionStateChangedEvent

Event payload for receiver phase or backend session state changes.

| Field | Required | Type | Field ID | Constraints / default | Description |
|---|---:|---|---:|---|---|
| `sessionId` | no | `string` | `0x01` | maxLength=128 | Receiver-local session id. |
| `previousReceiverPhase` | no | `enum` | `0x02` | enum=idle/incoming/authenticating/streamStarting/streaming/rendering/interrupted/stopping/ended/failed | Previous receiver phase. |
| `receiverPhase` | yes | `enum` | `0x03` | enum=idle/incoming/authenticating/streamStarting/streaming/rendering/interrupted/stopping/ended/failed | New receiver phase. |
| `previousState` | no | `enum` | `0x04` | enum=idle/incoming/waitingForPassword/authenticated/preparing/casting/interrupted/stopping/ended/failed | Previous backend-specific session state. |
| `sessionState` | no | `enum` | `0x05` | enum=idle/incoming/waitingForPassword/authenticated/preparing/casting/interrupted/stopping/ended/failed | New backend-specific session state. |
| `protocol` | no | `enum` | `0x06` | enum=airplay/hid/unknown | Protocol path represented by this event. |
| `authRequired` | no | `bool` | `0x07` | - | Whether the current session phase requires authentication. |
| `media` | no | `CastMediaSummary` | `0x08` | - | Low-frequency media summary at the time of transition. |
| `reason` | no | `enum` | `0x09` | enum=sessionStarted/mediaFlowStarted/externalRequest/sourceClosed/backendRestart/backendExited/authFailed/error/unknown | Transition reason. |
| `updatedAt` | no | `string` | `0x0A` | maxLength=64 | Timestamp for the transition. |

### CastSessionStartedEvent

Event payload for a user-visible cast session start.

| Field | Required | Type | Field ID | Constraints / default | Description |
|---|---:|---|---:|---|---|
| `sessionId` | yes | `string` | `0x01` | maxLength=128 | Receiver-local started session id. |
| `receiverPhase` | yes | `enum` | `0x02` | enum=rendering | Receiver phase after first visible frame or local playback starts. |
| `sessionState` | no | `enum` | `0x03` | enum=casting | Backend-specific state after session start. |
| `protocol` | no | `enum` | `0x04` | enum=airplay/hid/unknown | Protocol path represented by the started session. |
| `source` | no | `CastSourceSummary` | `0x05` | - | Source summary when available. |
| `media` | no | `CastMediaSummary` | `0x06` | - | Media summary at session start. |
| `startedAt` | no | `string` | `0x07` | maxLength=64 | Timestamp when the session became user-visible. |

### CastSessionStoppedEvent

Event payload for a stopped cast session.

| Field | Required | Type | Field ID | Constraints / default | Description |
|---|---:|---|---:|---|---|
| `sessionId` | no | `string` | `0x01` | maxLength=128 | Receiver-local stopped session id. |
| `previousReceiverPhase` | no | `enum` | `0x02` | enum=idle/incoming/authenticating/streamStarting/streaming/rendering/interrupted/stopping/ended/failed | Receiver phase before stop completion. |
| `receiverPhase` | yes | `enum` | `0x03` | enum=ended/failed/idle | Receiver phase after stop completion. |
| `previousState` | no | `enum` | `0x04` | enum=idle/incoming/waitingForPassword/authenticated/preparing/casting/interrupted/stopping/ended/failed | Backend-specific state before stop completion. |
| `sessionState` | no | `enum` | `0x05` | enum=ended/failed/idle | Backend-specific state after stop completion. |
| `reason` | no | `enum` | `0x06` | enum=externalRequest/sourceClosed/backendRestart/backendExited/shutdown/error/unknown | Stop reason. |
| `backendType` | no | `enum` | `0x07` | enum=uxplay/unknown | Backend type associated with the stopped session. |
| `stoppedAt` | no | `string` | `0x08` | maxLength=64 | Timestamp when the stop was observed. |

### CastSessionFailedEvent

Event payload for cast session failure.

| Field | Required | Type | Field ID | Constraints / default | Description |
|---|---:|---|---:|---|---|
| `sessionId` | no | `string` | `0x01` | maxLength=128 | Receiver-local failed session id when assigned. |
| `receiverPhase` | yes | `enum` | `0x02` | enum=failed | Receiver phase after failure. |
| `sessionState` | no | `enum` | `0x03` | enum=failed | Backend-specific failed state. |
| `protocol` | no | `enum` | `0x04` | enum=airplay/hid/unknown | Protocol path represented by the failed session. |
| `source` | no | `CastSourceSummary` | `0x05` | - | Source summary when available. |
| `reason` | no | `enum` | `0x06` | enum=connectionFailed/authFailed/negotiationFailed/backendFailed/mediaFailed/unknown | Failure reason. |
| `error` | no | `CastLastError` | `0x07` | - | Redactable error summary. |
| `failedAt` | no | `string` | `0x08` | maxLength=64 | Timestamp when the failure was observed. |

### CastLastError

Redactable backend or receiver error summary.

| Field | Required | Type | Field ID | Constraints / default | Description |
|---|---:|---|---:|---|---|
| `code` | no | `string` | `0x01` | maxLength=64 | Backend-local or AXTP-visible error code. |
| `message` | no | `string` | `0x02` | maxLength=512 | Human-readable error summary suitable for authorized clients. |
| `occurredAt` | no | `string` | `0x03` | maxLength=64 | Timestamp when the error was observed. |
| `redacted` | no | `bool` | `0x04` | default=false | Whether sensitive details were removed from this summary. |

### CastSessionCapability

Capability descriptor for cast.session.

| Field | Required | Type | Field ID | Constraints / default | Description |
|---|---:|---|---:|---|---|
| `protocols` | yes | `Array<string>` | `0x01` | itemType=string | Cast protocol paths represented by the receiver. |
| `receiverPhases` | yes | `Array<string>` | `0x02` | itemType=string | Supported protocol-neutral receiver phases. |
| `supportsAirPlayName` | no | `bool` | `0x03` | default=true | Whether AirPlay display name query and update are supported. |
| `supportsStopSession` | no | `bool` | `0x04` | default=true | Whether active cast sessions can be stopped through cast.stopSession. |
| `backendTypes` | no | `Array<string>` | `0x05` | itemType=string | Backend implementations represented by this receiver. |
