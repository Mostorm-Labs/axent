---
status: generated
contract: true
generated: true
domain: cast
feature: cast.status
registry: ../../../../contract/registry/domains/cast/domain.yaml
lastReviewed: 2026-06-25
---

# cast.status

## 0. 采纳状态

| 项目 | 内容 |
|---|---|
| 当前状态 | generated；已写入 `../../../../contract/registry/domains/cast/domain.yaml`，并已刷新到 `contract/protocol/axtp.protocol.yaml` 与 `contract/generated/**`。 |
| 是否可直接实现 | 是，但实现合同以 `contract/protocol/axtp.protocol.yaml` / `contract/generated/**` 为准。 |
| 本次采纳 | `cast.getStatus` 快照查询、receiver/session/PIN/audio/window/backend/flowControl 摘要字段外形。 |
| 未采纳 | `cast.statusChanged` 持续聚合事件；后续若需要多 method 一次性读取，应优先走通用 RPC batch，而不是恢复 cast 专属状态事件。 |

## 1. Purpose

提供投屏接收端的当前快照，用于 UI 初始加载、控制端重连和事件丢失后的校准。它不替代各 feature 的 get method，也不提供持续状态事件；明文 PIN 等敏感字段需要标明可见性并保持日志脱敏。

## 2. Candidate Surface

| Method / Event | Purpose | Schema | Notes |
|---|---|---|---|
| `cast.getStatus` | 查询 receiver、session、PIN、audio、window、backend、flowControl 当前快照摘要。 | `CastGetStatusParams` -> `CastStatus` | query；仅按需调用，不产生持续事件 |

## 3. Methods

### 3.0 方法速览

方法概览见第 2 章；本节只保留每个 method 的最小 request / success 示例。

### 3.1 `cast.getStatus`

返回当前快照；`includeSensitive=true` 表示请求包含明文 PIN 等敏感摘要，具体是否返回由可见性策略决定。

#### 3.1.1 d block 示例

request:

```json
{
  "id": 3701,
  "method": "cast.getStatus",
  "params": {
    "include": [
      "session",
      "pinCode",
      "audio",
      "window",
      "backend",
      "flowControl"
    ],
    "includeSensitive": false
  }
}
```

success:

```json
{
  "id": 3701,
  "status": {
    "ok": true,
    "code": 0
  },
  "result": {
    "receiver": {
      "role": "receiver",
      "protocols": [
        "airplay"
      ],
      "state": "ready",
      "receiverPhase": "rendering"
    },
    "session": {
      "sessionId": "cast_sess_001",
      "receiverPhase": "rendering",
      "sessionState": "casting",
      "protocol": "airplay",
      "sourceName": "Qing iPhone"
    },
    "pinCode": {
      "enabled": true,
      "hasPinCode": true,
      "pinDisplay": "authorizedClients",
      "pinCode": null,
      "redacted": true
    },
    "audio": {
      "enabled": false,
      "muted": false,
      "effectivePlayback": false
    },
    "window": {
      "hasWindow": true,
      "visible": true,
      "mode": "fullscreen",
      "fullscreen": true,
      "alwaysOnTop": false
    },
    "backend": {
      "backendType": "uxplay",
      "state": "ready",
      "discoverable": true
    },
    "flowControl": {
      "targetRenderFps": 10,
      "inputFps": 25,
      "renderFps": 10,
      "dropMode": "drop-late",
      "videoQueueDepth": 1
    },
    "sampledAt": "2026-06-22T10:30:00Z"
  }
}
```

## 4. Snapshot Sections

| Section | Meaning |
|---|---|
| `receiver` | receiver role、protocol、runtime 可用性和 `receiverPhase` 摘要。 |
| `session` | 当前会话摘要，复用 `cast.session` 语义；`receiverPhase` 是 UI / 重连校准优先读取的阶段。 |
| `pinCode` | 密码保护摘要；明文 PIN 按可见性策略返回或置空。 |
| `audio` | 本地播放和静音摘要。 |
| `window` | 投屏窗口摘要。 |
| `backend` | backend type、状态和错误摘要。 |
| `flowControl` | target / input / render fps、队列和丢帧摘要。 |

`cast.getStatus` 是一次性 snapshot，不是订阅入口；持续变化仍由 feature-specific events 表达，客户端事件丢失时再调用 `cast.getStatus` 校准。

## 5. Rules

- `cast.status` 是重连校准入口，只提供按需快照查询。
- `receiver.receiverPhase` 和 `session.receiverPhase` 同时出现时必须一致；客户端应优先用它展示整体接收阶段，再按需读取 feature-specific 细节。
- 明文 PIN、错误详情和诊断字段需要标明可见性；普通日志和诊断输出必须脱敏。
- 聚合状态只返回摘要，不承载高频媒体统计。
- feature-specific 方法仍是详细状态事实源。
- 不提供 `cast.statusChanged`；持续变化由 `cast.session*`、`cast.audioChanged`、`cast.pinCodeChanged`、`cast.windowChanged`、`cast.backendChanged` 和 `cast.flowControlChanged` 承担。

## 6. Errors

| Error | Scenario |
|---|---|
| `INVALID_ARGUMENT` | 请求了未知 include 段。 |
| `UNAVAILABLE` | receiver control surface 不可用。 |

## 7. Review Items

| 问题 | 影响 | 当前建议 | 状态 |
|---|---|---|---|
| 快照是否返回全部 feature 摘要，还是只返回 UI 首页最小集合？ | schema / performance | MVP 覆盖 receiver、session、pinCode、audio、window、backend、flowControl，客户端可用 include 裁剪。 | `[REVIEW-OK]` |
| 是否保留 `cast.statusChanged` 持续聚合事件？ | event model | 不保留；持续变化由 feature events 承担，事件丢失后调用 `cast.getStatus` 校准。 | `[REVIEW-OK]` |
| 敏感字段不可见时返回 null 还是省略字段？ | security | 默认保留字段并置空，同时标记 `redacted=true`。 | `[REVIEW-DRAFT]` |

## 8. Amendment History

| Date | Change | Compatibility |
|---|---|---|
| 2026-06-25 | 保留 `cast.getStatus` 作为当前快照查询；移除 `cast.statusChanged`、`CastStatusChangedEvent` 和 `supportsStatusChangedEvent`。 | draft 事件删除；`cast.getStatus` methodId、bitOffset 和 snapshot schema field IDs 保持不变。 |

## 9. Schema Reference

> 本节按当前 `contract/registry/domains/cast/domain.yaml` 整理字段事实；`Required=yes` 表示编码数据必须携带该字段，`Required=no` 表示可省略。`Empty` schema 无字段，未展开。

### CastGetStatusParams

Selector for current cast receiver snapshot.

| Field | Required | Type | Field ID | Constraints / default | Description |
|---|---:|---|---:|---|---|
| `include` | no | `Array<string>` | `0x01` | itemType=string | Optional status sections to include. |
| `includeSensitive` | no | `bool` | `0x02` | default=false | Whether authorized callers request sensitive summary fields. |

### CastStatus

Current cast receiver snapshot for reconnect and event-loss recovery.

| Field | Required | Type | Field ID | Constraints / default | Description |
|---|---:|---|---:|---|---|
| `receiver` | yes | `CastReceiverSummary` | `0x01` | - | Receiver role and phase summary. |
| `session` | no | `CastSessionStatusSummary` | `0x02` | - | Active session summary. |
| `pinCode` | no | `CastPinCodeStatusSummary` | `0x03` | - | PIN protection summary. |
| `audio` | no | `CastAudioState` | `0x04` | - | Local audio summary. |
| `window` | no | `CastWindowState` | `0x05` | - | Cast window summary. |
| `backend` | no | `CastBackendStatus` | `0x06` | - | Backend summary. |
| `flowControl` | no | `CastFlowControlState` | `0x07` | - | Flow control summary. |
| `sampledAt` | yes | `string` | `0x08` | maxLength=64 | Timestamp for this status snapshot. |
| `redacted` | no | `bool` | `0x09` | - | Whether any sensitive snapshot fields were withheld. |

### CastReceiverSummary

Snapshot receiver role and protocol-neutral phase summary.

| Field | Required | Type | Field ID | Constraints / default | Description |
|---|---:|---|---:|---|---|
| `role` | yes | `enum` | `0x01` | enum=receiver | Cast endpoint role. |
| `protocols` | yes | `Array<string>` | `0x02` | itemType=string | Supported or active cast protocol paths. |
| `state` | yes | `enum` | `0x03` | enum=disabled/starting/ready/busy/failed | Receiver service availability state. |
| `receiverPhase` | yes | `enum` | `0x04` | enum=idle/incoming/authenticating/streamStarting/streaming/rendering/interrupted/stopping/ended/failed | Protocol-neutral receiver phase. |

### CastSessionStatusSummary

Snapshot active session summary for status views.

| Field | Required | Type | Field ID | Constraints / default | Description |
|---|---:|---|---:|---|---|
| `sessionId` | no | `string` | `0x01` | maxLength=128 | Receiver-local session id. |
| `receiverPhase` | no | `enum` | `0x02` | enum=idle/incoming/authenticating/streamStarting/streaming/rendering/interrupted/stopping/ended/failed | Protocol-neutral receiver phase. |
| `sessionState` | no | `enum` | `0x03` | enum=idle/incoming/waitingForPassword/authenticated/preparing/casting/interrupted/stopping/ended/failed | Backend-specific session state. |
| `protocol` | no | `enum` | `0x04` | enum=airplay/hid/unknown | Protocol path represented by the active session. |
| `sourceName` | no | `string` | `0x05` | maxLength=128 | User-visible source name. |

### CastPinCodeStatusSummary

Snapshot PIN summary for status views.

| Field | Required | Type | Field ID | Constraints / default | Description |
|---|---:|---|---:|---|---|
| `enabled` | yes | `bool` | `0x01` | - | Whether PIN protection is enabled. |
| `hasPinCode` | yes | `bool` | `0x02` | - | Whether a current PIN exists. |
| `pinDisplay` | no | `enum` | `0x03` | enum=hidden/authorizedClients/localUi/both | Where the current PIN may be displayed. |
| `pinCode` | no | `string` | `0x04` | - | Plaintext PIN value when visible to the caller. |
| `redacted` | no | `bool` | `0x05` | - | Whether sensitive fields were withheld. |
| `visibility` | no | `enum` | `0x06` | enum=hidden/authorizedOnly/localUi/both | Visibility policy applied to this summary. |

### CastAudioState

Local cast audio playback and mute state.

| Field | Required | Type | Field ID | Constraints / default | Description |
|---|---:|---|---:|---|---|
| `enabled` | yes | `bool` | `0x01` | default=false | Whether local receiver playback is enabled. |
| `muted` | yes | `bool` | `0x02` | default=false | Whether local receiver output is muted. |
| `effectivePlayback` | yes | `bool` | `0x03` | - | Whether audio is effectively playing locally after state and session conditions are applied. |
| `scope` | no | `enum` | `0x04` | enum=currentSession/default | State target hint represented by this snapshot. |
| `sessionId` | no | `string` | `0x05` | maxLength=128 | Receiver-local session id for session-specific state. |
| `source` | no | `enum` | `0x06` | enum=defaultConfig/externalSet/localUi/sessionStarted/sessionStopped/unknown | Source of the latest state value. |
| `reason` | no | `enum` | `0x07` | enum=receiverDefault/externalSet/localUi/sessionStarted/sessionStopped/unknown | Latest audio state transition reason. |
| `changedFields` | no | `Array<string>` | `0x08` | itemType=string | Field names changed by the latest operation or event. |
| `updatedAt` | no | `string` | `0x09` | maxLength=64 | Timestamp for this audio state. |

### CastWindowState

Cast window visibility, mode, and bounds state.

| Field | Required | Type | Field ID | Constraints / default | Description |
|---|---:|---|---:|---|---|
| `hasWindow` | yes | `bool` | `0x01` | - | Whether a cast window currently exists. |
| `visible` | yes | `bool` | `0x02` | - | Whether the cast window is visible. |
| `mode` | yes | `enum` | `0x03` | enum=normal/fullscreen | Current cast window mode. |
| `fullscreen` | yes | `bool` | `0x04` | - | Whether the cast window is fullscreen. |
| `alwaysOnTop` | yes | `bool` | `0x05` | - | Whether the cast window is topmost. |
| `sessionId` | no | `string` | `0x06` | maxLength=128 | Receiver-local session id associated with the window. |
| `bounds` | no | `CastRect` | `0x07` | - | Current window bounds when available. |
| `changedFields` | no | `Array<string>` | `0x0A` | itemType=string | Field names changed by the latest operation or event. |
| `updatedAt` | no | `string` | `0x0B` | maxLength=64 | Timestamp for this window state. |

### CastRect

Cast window rectangle in screen coordinates.

| Field | Required | Type | Field ID | Constraints / default | Description |
|---|---:|---|---:|---|---|
| `x` | yes | `int32` | `0x01` | - | Rectangle left coordinate. |
| `y` | yes | `int32` | `0x02` | - | Rectangle top coordinate. |
| `width` | yes | `uint32` | `0x03` | - | Rectangle width in pixels. |
| `height` | yes | `uint32` | `0x04` | - | Rectangle height in pixels. |

### CastBackendStatus

Cast backend state, process summary, discoverability, and last error.

| Field | Required | Type | Field ID | Constraints / default | Description |
|---|---:|---|---:|---|---|
| `backendType` | yes | `enum` | `0x01` | enum=uxplay/unknown | Backend implementation type. |
| `state` | yes | `enum` | `0x02` | enum=starting/ready/restarting/exited/failed/disabled | Backend runtime state. |
| `discoverable` | yes | `bool` | `0x03` | - | Whether the cast service is discoverable by sources. |
| `pid` | no | `uint32` | `0x04` | - | Backend process id when available. |
| `version` | no | `string` | `0x05` | maxLength=128 | Backend version or build identifier. |
| `activeSessionId` | no | `string` | `0x06` | maxLength=128 | Active cast session id currently owned by the backend. |
| `restartInProgress` | yes | `bool` | `0x07` | default=false | Whether a backend restart is currently in progress. |
| `lastError` | no | `CastLastError` | `0x08` | - | Last backend error summary when requested and available. |
| `updatedAt` | no | `string` | `0x09` | maxLength=64 | Timestamp for this backend status. |

### CastLastError

Redactable backend or receiver error summary.

| Field | Required | Type | Field ID | Constraints / default | Description |
|---|---:|---|---:|---|---|
| `code` | no | `string` | `0x01` | maxLength=64 | Backend-local or AXTP-visible error code. |
| `message` | no | `string` | `0x02` | maxLength=512 | Human-readable error summary suitable for authorized clients. |
| `occurredAt` | no | `string` | `0x03` | maxLength=64 | Timestamp when the error was observed. |
| `redacted` | no | `bool` | `0x04` | default=false | Whether sensitive details were removed from this summary. |

### CastFlowControlState

Receiver-local cast flow control policy and low-frequency statistics.

| Field | Required | Type | Field ID | Constraints / default | Description |
|---|---:|---|---:|---|---|
| `targetRenderFps` | yes | `number` | `0x01` | min=0 | Configured target render fps; zero means uncapped. |
| `inputFps` | no | `number` | `0x02` | min=0 | Estimated incoming media frame rate. |
| `renderFps` | no | `number` | `0x03` | min=0 | Estimated local render frame rate. |
| `dropMode` | yes | `enum` | `0x04` | enum=drop-late/drop-oldest/render-latest | Local frame drop policy. |
| `videoQueueFrames` | yes | `uint32` | `0x05` | min=1 | Maximum queued video frames. |
| `videoQueueDepth` | no | `uint32` | `0x06` | - | Current queued video frame depth. |
| `audioQueueDepth` | no | `uint32` | `0x07` | - | Current queued audio frame depth when known. |
| `lateFrameThresholdMs` | yes | `uint32` | `0x08` | - | Late-frame threshold in milliseconds. |
| `overlayEnabled` | yes | `bool` | `0x09` | - | Whether diagnostics overlay is enabled. |
| `droppedFrames` | no | `uint64` | `0x0A` | - | Low-frequency dropped-frame counter. |
| `lateFrames` | no | `uint64` | `0x0B` | - | Low-frequency late-frame counter. |
| `keyframeRequestCount` | no | `uint32` | `0x0C` | - | Internal keyframe requests triggered by receiver policy. |
| `keyFrameOnDropBurst` | no | `bool` | `0x0D` | - | Whether the receiver may internally request a keyframe after a drop burst. |
| `changedFields` | no | `Array<string>` | `0x0E` | itemType=string | Field names changed by the latest operation or event. |
| `sampledAt` | no | `string` | `0x0F` | maxLength=64 | Timestamp for this flow sample. |

### CastStatusCapability

Capability descriptor for cast.status snapshot query.

| Field | Required | Type | Field ID | Constraints / default | Description |
|---|---:|---|---:|---|---|
| `sections` | yes | `Array<string>` | `0x01` | itemType=string | Status snapshot sections the receiver can report. |
| `supportsSensitiveRedaction` | no | `bool` | `0x02` | default=true | Whether sensitive status fields can be withheld and marked redacted. |
