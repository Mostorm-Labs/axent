---
status: generated
contract: true
generated: true
domain: cast
feature: cast.audio
registry: ../../../../contract/registry/domains/cast/domain.yaml
lastReviewed: 2026-06-22
---

# cast.audio

## 0. 采纳状态

| 项目 | 内容 |
|---|---|
| 当前状态 | generated；已写入 `../../../../contract/registry/domains/cast/domain.yaml`，并已刷新到 `contract/protocol/axtp.protocol.yaml` 与 `contract/generated/**`。 |
| 是否可直接实现 | 是，但实现合同以 `contract/protocol/axtp.protocol.yaml` / `contract/generated/**` 为准。 |
| 本次采纳 | `cast.getAudio`、`cast.setAudio`、`cast.setMuted`、`cast.setAudioDelay`、`cast.audioChanged` 和 `CastAudioState` 字段外形。 |
| 未采纳 | Review Items 中仍待确认的行为策略不属于已生成合同；后续语义变更走 `amend-adopted-protocol`。 |

## 1. Purpose

控制 AirPlay 投屏音频在接收端本地是否播放、是否静音，以及接收端本地音频播放延迟补偿。它不改变 AirPlay 媒体协商，也不定义音频 STREAM payload。

## 2. Candidate Surface

| Method / Event | Purpose | Schema | Notes |
|---|---|---|---|
| `cast.getAudio` | 查询本地播放和静音状态。 | `CastGetAudioParams` -> `CastAudioState` | query |
| `cast.setAudio` | 设置接收端本地是否播放投屏音频。 | `CastSetAudioParams` -> `CastAudioState` | command，触发 `cast.audioChanged` |
| `cast.setMuted` | 设置静音状态。 | `CastSetMutedParams` -> `CastAudioState` | candidate，是否独立保留待确认 |
| `cast.setAudioDelay` | 设置接收端本地音频播放延迟补偿。 | `CastSetAudioDelayParams` -> `CastAudioState` | command，触发 `cast.audioChanged` |
| `cast.audioChanged` | 音频状态变化。 | `CastAudioChangedEvent` | event |

## 3. Methods

### 3.0 方法速览

方法概览见第 2 章；本节只保留每个 method 的最小 request / success 示例。

### 3.1 `cast.getAudio`

查询当前投屏音频状态。`includeEffective=true` 时返回当前会话下的实际播放状态。

#### 3.1.1 d block 示例

request:

```json
{
  "id": 3201,
  "method": "cast.getAudio",
  "params": {
    "includeEffective": true
  }
}
```

success:

```json
{
  "id": 3201,
  "status": {
    "ok": true,
    "code": 0
  },
  "result": {
    "enabled": false,
    "muted": false,
    "effectivePlayback": false,
    "scope": "default",
    "sessionId": "cast_sess_001",
    "source": "defaultConfig",
    "reason": "receiverDefault",
    "updatedAt": "2026-06-22T10:30:00Z"
  }
}
```

### 3.2 `cast.setAudio`

设置是否允许接收端本地播放投屏音频。默认业务值为 `enabled=false`。

#### 3.2.1 d block 示例

request:

```json
{
  "id": 3202,
  "method": "cast.setAudio",
  "params": {
    "enabled": true,
    "scope": "currentSession"
  }
}
```

success:

```json
{
  "id": 3202,
  "status": {
    "ok": true,
    "code": 0
  },
  "result": {
    "enabled": true,
    "muted": false,
    "effectivePlayback": true,
    "scope": "currentSession",
    "sessionId": "cast_sess_001",
    "source": "externalSet",
    "changedFields": [
      "enabled",
      "effectivePlayback"
    ],
    "updatedAt": "2026-06-22T10:31:00Z"
  }
}
```

### 3.3 `cast.setMuted`

设置本地静音；它只影响接收端输出，不要求 source 改变发送音频。

#### 3.3.1 d block 示例

request:

```json
{
  "id": 3203,
  "method": "cast.setMuted",
  "params": {
    "muted": true,
    "scope": "currentSession"
  }
}
```

success:

```json
{
  "id": 3203,
  "status": {
    "ok": true,
    "code": 0
  },
  "result": {
    "enabled": true,
    "muted": true,
    "effectivePlayback": false,
    "scope": "currentSession",
    "sessionId": "cast_sess_001",
    "source": "externalSet",
    "changedFields": [
      "muted",
      "effectivePlayback"
    ],
    "updatedAt": "2026-06-22T10:32:00Z"
  }
}
```

### 3.4 `cast.setAudioDelay`

设置接收端本地音频播放延迟补偿。该值用于把本地音频输出整体向后推，例如先缓存 `audioDelayMs` 对应的 PCM 帧后再启动音频输出；它不改变音频 cursor、视频调度、AirPlay 媒体协商或 STREAM payload。默认建议为 `250ms`，`0` 表示关闭补偿。

#### 3.4.1 d block 示例

request:

```json
{
  "id": 3204,
  "method": "cast.setAudioDelay",
  "params": {
    "audioDelayMs": 250,
    "scope": "default"
  }
}
```

success:

```json
{
  "id": 3204,
  "status": {
    "ok": true,
    "code": 0
  },
  "result": {
    "enabled": true,
    "muted": false,
    "effectivePlayback": true,
    "audioDelayMs": 250,
    "scope": "default",
    "sessionId": "cast_sess_001",
    "source": "externalSet",
    "changedFields": [
      "audioDelayMs"
    ],
    "updatedAt": "2026-06-22T10:33:00Z"
  }
}
```

## 4. State And Events

| Field | Meaning |
|---|---|
| `enabled` | 是否允许本地播放投屏音频。 |
| `muted` | 是否静音本地输出。 |
| `effectivePlayback` | 当前是否真的有本地声音输出，通常由 `enabled && !muted && sessionActive` 决定。 |
| `audioDelayMs` | 接收端本地音频播放延迟补偿，单位毫秒；`0` 表示关闭补偿。 |
| `scope` | `currentSession` 或 `default`。 |
| `reason` | `externalSet`、`localUi`、`sessionStarted`、`sessionStopped` 等变化原因。 |

`cast.audioChanged` 应携带变化后的 `CastAudioState` 摘要；payload 不完整或事件丢失时，客户端调用 `cast.getAudio` 校准。完整 JSON RPC 事件仍然使用外层 `sid/op/d` envelope；`sid` 不放进业务 `data`，而是由 RPC session envelope 承载。

### 4.1 Event 示例

event:

```json
{
  "sid": "12345678",
  "op": 6,
  "d": {
    "event": "cast.audioChanged",
    "intent": 1,
    "data": {
      "changedFields": [
        "enabled",
        "effectivePlayback"
      ],
      "state": {
        "enabled": true,
        "muted": false,
        "effectivePlayback": true,
        "scope": "currentSession",
        "sessionId": "cast_sess_001"
      },
      "reason": "externalSet",
      "updatedAt": "2026-06-22T10:31:00Z"
    }
  }
}
```

## 5. Rules

- 默认 `enabled=false`。
- `enabled=false` 时，`muted` 可保存但 `effectivePlayback=false`。
- `setAudio` 和 `setMuted` 是朴素可调用状态操作，不在本 feature 内拆分权限 scope。
- `audioDelayMs` 范围为 `0..1000`，默认建议为 `250`；`0` 表示不做接收端音频延迟补偿。
- `scope=default` 的音频延迟用于后续新会话；`scope=currentSession` 只影响当前接收端本地会话。新 stream open / close / device reopen / format change 时 runtime 应清空内部 PCM delay buffer，并按当前配置重新累积。
- 音频延迟补偿只调整接收端本地输出时机，不改变媒体 cursor，不替代 AV drift 校正，也不要求视频队列或 flow control 行为变化。
- 音频开关不影响 AirPlay 连接、鉴权、编码或媒体协商。

## 6. Errors

| Error | Scenario |
|---|---|
| `INVALID_ARGUMENT` | 非法 scope 或字段组合。 |
| `OUT_OF_RANGE` | `audioDelayMs` 超出 receiver 支持范围。 |
| `UNAVAILABLE` | 本地音频输出服务不可用。 |

## 7. Review Items

| 问题 | 影响 | 当前建议 | 状态 |
|---|---|---|---|
| `enabled` 与 `muted` 的有效状态公式是否进入 registry？ | schema / UI | 使用 `effectivePlayback` 承载实际结果。 | `[REVIEW-ASK]` |
| `cast.setMuted` 是否独立保留？ | method model | 保留候选；采纳时可合并到 `setAudio`。 | `[REVIEW-ASK]` |

## 8. Schema Reference

> 本节按当前 `contract/registry/domains/cast/domain.yaml` 整理字段事实；`Required=yes` 表示编码数据必须携带该字段，`Required=no` 表示可省略。`Empty` schema 无字段，未展开。

### CastGetAudioParams

Selector for local cast audio state.

| Field | Required | Type | Field ID | Constraints / default | Description |
|---|---:|---|---:|---|---|
| `includeEffective` | no | `bool` | `0x01` | default=true | Whether to include effective local playback state. |
| `sessionId` | no | `string` | `0x02` | maxLength=128 | Optional receiver-local session id. |

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
| `audioDelayMs` | no | `uint32` | `0x0A` | min=0, max=1000, default=250 | Configured receiver-local audio playback delay in milliseconds; zero disables compensation. |

### CastSetAudioParams

Request to enable or disable local cast audio playback.

| Field | Required | Type | Field ID | Constraints / default | Description |
|---|---:|---|---:|---|---|
| `enabled` | yes | `bool` | `0x01` | - | Whether local receiver playback is enabled. |
| `sessionId` | no | `string` | `0x02` | maxLength=128 | Optional receiver-local session id. |
| `scope` | no | `enum` | `0x03` | enum=currentSession/default | State target hint; this is not an authorization scope. |

### CastSetMutedParams

Request to mute or unmute local cast audio output.

| Field | Required | Type | Field ID | Constraints / default | Description |
|---|---:|---|---:|---|---|
| `muted` | yes | `bool` | `0x01` | - | Whether local receiver output is muted. |
| `sessionId` | no | `string` | `0x02` | maxLength=128 | Optional receiver-local session id. |
| `scope` | no | `enum` | `0x03` | enum=currentSession/default | State target hint; this is not an authorization scope. |

### CastSetAudioDelayParams

Request to set receiver-local audio playback delay compensation.

| Field | Required | Type | Field ID | Constraints / default | Description |
|---|---:|---|---:|---|---|
| `audioDelayMs` | yes | `uint32` | `0x01` | min=0, max=1000 | Target local audio playback delay in milliseconds; zero disables delay compensation. |
| `sessionId` | no | `string` | `0x02` | maxLength=128 | Optional receiver-local session id. |
| `scope` | no | `enum` | `0x03` | enum=currentSession/default | State target hint; default persists the receiver delay for future sessions. |

### CastAudioChangedEvent

Event payload for local cast audio state changes.

| Field | Required | Type | Field ID | Constraints / default | Description |
|---|---:|---|---:|---|---|
| `changedFields` | yes | `Array<string>` | `0x01` | itemType=string | Field names changed by this event. |
| `state` | yes | `CastAudioState` | `0x02` | - | State after the change. |
| `reason` | no | `enum` | `0x03` | enum=externalSet/localUi/sessionStarted/sessionStopped/unknown | Change reason. |
| `updatedAt` | no | `string` | `0x04` | maxLength=64 | Timestamp for this event. |

### CastAudioCapability

Capability descriptor for cast.audio.

| Field | Required | Type | Field ID | Constraints / default | Description |
|---|---:|---|---:|---|---|
| `defaultEnabled` | no | `bool` | `0x01` | default=false | Default local playback enablement for received cast audio. |
| `supportsMute` | no | `bool` | `0x02` | default=true | Whether local mute state can be controlled separately. |
| `reportsEffectivePlayback` | no | `bool` | `0x03` | default=true | Whether the receiver reports effective local playback state. |
| `supportsAudioDelay` | no | `bool` | `0x04` | default=true | Whether receiver-local audio playback delay compensation can be configured. |
| `defaultAudioDelayMs` | no | `uint32` | `0x05` | min=0, max=1000, default=250 | Default receiver-local audio playback delay in milliseconds. |
| `maxAudioDelayMs` | no | `uint32` | `0x06` | min=0, default=1000 | Maximum supported receiver-local audio playback delay in milliseconds. |

## 9. Amendment History

| Date | Change | Compatibility |
|---|---|---|
| 2026-07-06 | Added `cast.setAudioDelay`, optional `CastAudioState.audioDelayMs`, and audio delay capability fields for receiver-local audio playback delay compensation. | Draft-compatible optional extension; existing method IDs, event IDs, bit offsets, and field IDs are preserved. |
