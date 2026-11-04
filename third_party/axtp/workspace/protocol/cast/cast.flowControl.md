---
status: generated
contract: true
generated: true
domain: cast
feature: cast.flowControl
registry: ../../../../contract/registry/domains/cast/domain.yaml
lastReviewed: 2026-06-22
---

# cast.flowControl

## 0. 采纳状态

| 项目 | 内容 |
|---|---|
| 当前状态 | generated；已写入 `../../../../contract/registry/domains/cast/domain.yaml`，并已刷新到 `contract/protocol/axtp.protocol.yaml` 与 `contract/generated/**`。 |
| 是否可直接实现 | 是，但实现合同以 `contract/protocol/axtp.protocol.yaml` / `contract/generated/**` 为准。 |
| 本次采纳 | `cast.getFlowControlState`、`cast.setRenderFps`、`cast.setFlowPolicy`、`cast.flowControlChanged` 和低频统计字段外形。 |
| 未采纳 | 外部 keyframe request 维持不暴露；后续语义变更走 `amend-adopted-protocol`。 |

## 1. Purpose

控制投屏接收端本地渲染 fps、视频队列上限、late frame 阈值、丢帧策略和 overlay；同时可在支持的 Nearcast/NT10 source 上设置编码帧率和码率。`cast.setRenderFps` 仍只影响接收端本地渲染，不要求 source 降低输入 fps，也不定义 STREAM 数据面的通用流控。

## 2. Candidate Surface

| Method / Event | Purpose | Schema | Notes |
|---|---|---|---|
| `cast.getFlowControlState` | 查询本地流控配置和统计。 | `CastGetFlowControlStateParams` -> `CastFlowControlState` | query |
| `cast.setRenderFps` | 设置本地目标渲染 fps。 | `CastSetRenderFpsParams` -> `CastFlowControlState` | command |
| `cast.setFlowPolicy` | 设置队列、late frame、drop policy 和 overlay。 | `CastSetFlowPolicyParams` -> `CastFlowControlState` | command |
| `cast.setVideoStreamParams` | 设置当前 cast session 的 NT10/source 编码帧率和码率；必要时协调重建完整的音视频 downstream stream 对。 | `CastSetVideoStreamParamsParams` -> `CastSetVideoStreamParamsResult` | command；可触发 `cast.flowControlChanged` |
| `cast.flowControlChanged` | 流控状态或统计变化。 | `CastFlowControlChangedEvent` | event |

## 3. Methods

### 3.0 方法速览

方法概览见第 2 章；本节只保留每个 method 的最小 request / success 示例。

### 3.1 `cast.getFlowControlState`

返回目标 fps、输入 fps、实际渲染 fps、队列和丢帧统计。

#### 3.1.1 d block 示例

request:

```json
{
  "id": 3601,
  "method": "cast.getFlowControlState",
  "params": {
    "includeStats": true,
    "includePolicy": true
  }
}
```

success:

```json
{
  "id": 3601,
  "status": {
    "ok": true,
    "code": 0
  },
  "result": {
    "targetRenderFps": 10,
    "inputFps": 25,
    "renderFps": 10.1,
    "dropMode": "drop-late",
    "videoQueueFrames": 3,
    "videoQueueDepth": 2,
    "audioQueueDepth": 1,
    "lateFrameThresholdMs": 120,
    "overlayEnabled": true,
    "droppedFrames": 240,
    "lateFrames": 18,
    "keyframeRequestCount": 1,
    "sampledAt": "2026-06-22T10:30:00Z"
  }
}
```

### 3.2 `cast.setRenderFps`

设置接收端本地目标渲染 fps；`fps=0` 表示不限速。

#### 3.2.1 d block 示例

request:

```json
{
  "id": 3602,
  "method": "cast.setRenderFps",
  "params": {
    "fps": 10,
    "scope": "currentSession"
  }
}
```

success:

```json
{
  "id": 3602,
  "status": {
    "ok": true,
    "code": 0
  },
  "result": {
    "targetRenderFps": 10,
    "inputFps": 25,
    "renderFps": 10,
    "dropMode": "drop-late",
    "videoQueueFrames": 3,
    "videoQueueDepth": 1,
    "lateFrameThresholdMs": 120,
    "overlayEnabled": true,
    "changedFields": [
      "targetRenderFps"
    ],
    "sampledAt": "2026-06-22T10:31:00Z"
  }
}
```

### 3.3 `cast.setFlowPolicy`

设置视频队列和丢帧策略。默认 `dropMode=drop-late`。

#### 3.3.1 d block 示例

request:

```json
{
  "id": 3603,
  "method": "cast.setFlowPolicy",
  "params": {
    "videoQueueFrames": 3,
    "lateFrameThresholdMs": 120,
    "dropMode": "drop-late",
    "overlayEnabled": true
  }
}
```

success:

```json
{
  "id": 3603,
  "status": {
    "ok": true,
    "code": 0
  },
  "result": {
    "targetRenderFps": 10,
    "inputFps": 25,
    "renderFps": 10,
    "dropMode": "drop-late",
    "videoQueueFrames": 3,
    "videoQueueDepth": 1,
    "lateFrameThresholdMs": 120,
    "overlayEnabled": true,
    "keyFrameOnDropBurst": true,
    "changedFields": [
      "videoQueueFrames",
      "lateFrameThresholdMs",
      "dropMode",
      "overlayEnabled"
    ],
    "sampledAt": "2026-06-22T10:32:00Z"
  }
}
```

### 3.4 `cast.setVideoStreamParams`

在支持 source 编码控制的 Nearcast/NA20/NT10 路径上，设置当前 cast session 的目标编码帧率和码率。该方法不写入设备默认配置；省略 `sessionId` 时使用当前 cast session。`frameRate`、`bitrateKbps` 至少提供一个，或者通过 `resetFields` 清除一个或多个已设置字段。

`resetFields` 的元素只能是 `frameRate` 或 `bitrateKbps`，同一字段不得同时出现在请求字段和 `resetFields` 中。显式 `0` 对这两个字段均非法。省略 `frameRate` 或 `bitrateKbps` 表示保持当前 session 的对应字段不变；若该字段在当前 session 中从未设置，则继续保持 unset。只有将字段放入 `resetFields` 才会清除 session 值，并恢复按 source/profile 默认值协商。

活动投屏的参数变更是一个完整的音视频事务，而不是仅替换视频 stream。事务阶段使用已有的 `CastVideoStreamParamsState.state` / `phase` 值，按 `pending` -> `closing` -> `opening` -> `streaming` 推进；不得为该事务新增 schema 或枚举值。

#### 3.4.1 请求参数 Params：`CastSetVideoStreamParamsParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `sessionId` | string | no | maxLength=128 | 当前 session | 目标 cast session；省略时使用当前会话。 |
| `frameRate` | uint32 | no | `>=1`（`0` 非法） | omitted | NT10/source 编码器目标帧率；省略时保持当前 session 值（首次未设置则保持 unset），`resetFields` 才会清除并恢复 source/profile 默认值。 |
| `bitrateKbps` | uint32 | no | `>=1`（`0` 非法） | omitted | NT10/source 编码器目标码率，单位 kbps；省略时保持当前 session 值（首次未设置则保持 unset），`resetFields` 才会清除并恢复 source/profile 默认值。 |
| `resetFields` | string[] | no | `frameRate`, `bitrateKbps` | omitted | 将指定字段恢复为 source/profile 默认值。 |

#### 3.4.2 返回结果 Result：`CastSetVideoStreamParamsResult`

| 字段名 | 类型 | 必填 | 说明 |
|---|---|---:|---|
| `accepted` | bool | yes | 是否接受该次参数更新；`pending` 也表示请求已接受。 |
| `state` | enum | yes | `pending`、`applied`、`failed`、`rolledBack`、`unchanged`。 |
| `sessionId` | string | no | 实际关联的 cast session。 |
| `reconfigureId` | string | no | 本次 source video 重配置操作 ID。 |
| `previousStreamId` | uint32 | no | 重开前的 downstream video stream ID。 |
| `activeStreamId` | uint32 | no | 应用成功后新的 downstream video stream ID。 |
| `sourceVideo` | `CastVideoStreamParamsState` | yes | 请求完成或进行中的 source video 参数状态。 |

#### 3.4.3 d block 示例

request:

```json
{
  "id": 3604,
  "method": "cast.setVideoStreamParams",
  "params": {
    "frameRate": 30,
    "bitrateKbps": 6000
  }
}
```

当音视频流正在传输时，控制面可以先同步返回 `pending`，随后通过 `cast.flowControlChanged` 报告 `closing`、`opening` 和 `streaming` 阶段。异步路径中，这个 `pending` 是 `cast.setVideoStreamParams` 唯一的 RPC result，不会再发送第二个 result；完成状态通过 `cast.flowControlChanged` 的 `sourceVideo.state=streaming` 和状态查询确认：

success:

```json
{
  "id": 3604,
  "status": {
    "ok": true,
    "code": 0
  },
  "result": {
    "accepted": true,
    "state": "pending",
    "sessionId": "cast-session-17",
    "reconfigureId": "vr-20260716-001",
    "previousStreamId": 4097,
    "sourceVideo": {
      "sessionId": "cast-session-17",
      "source": "wireless_cast_video",
      "desiredFrameRate": 30,
      "desiredBitrateKbps": 6000,
      "streamProfile": "media.video",
      "reconfigureId": "vr-20260716-001",
      "state": "pending",
      "phase": "closing",
      "previousStreamId": 4097,
      "rollbackApplied": false,
      "changedFields": [
        "frameRate",
        "bitrateKbps"
      ]
    }
  }
}
```

只有新的 video/audio open 都成功且 NT10 编码参数已实际生效，事务才进入 `streaming`。如果整个事务在返回 RPC 前同步完成，`cast.setVideoStreamParams` 的唯一 result 可以是 `state=applied` 并返回新 `activeStreamId`；如果先返回 `pending`，则不得补发 `state=applied` result，调用方应从 `cast.flowControlChanged` 的 `sourceVideo.state=streaming` 及状态查询确认完成。`previousStreamId` 和 `activeStreamId` 始终是 video stream ID；audio 的旧、新 stream ID 通过 `audio.closeStream` / `audio.openStream` 响应或 `audio.streamStateChanged` 观察，不扩展 cast result schema。这里的 `idle` 严格表示当前不存在任何 audio 或 video downstream stream；只有这种状态才可不发送 close、直接使用同一个新 `syncGroupId` 打开完整 video/audio 对。若存在单边或其他 partial media state，必须先 cleanup/normalize，不能按 idle direct-open；该过程若未能在 RPC 返回前完成，同样保持初始 result 为 `pending`。

#### 3.4.4 参数优先级与重配置规则

| 优先级 | 参数来源 | 作用 |
|---:|---|---|
| 1 | `video.openStream.frameRate` / `video.openStream.bitrateKbps` | 本次 open 的显式值，覆盖 session 配置且不写回 session。 |
| 2 | 当前 session 的 `cast.setVideoStreamParams` | 当前 session 后续 open 的默认目标。session 结束后清除。 |
| 3 | source/profile 默认值 | 前两者都未提供时使用。 |

- 活动音视频 downstream stream 对变更时，Nearcast/NA20 进入 `closing`：对旧 video stream 发送 `video.closeStream(reason=encodingReconfigure)`，并对旧 audio stream 发送 `audio.closeStream`。audio close 的 `reason` 可以省略；不得把 `encodingReconfigure` 加入 audio 的既有 reason 枚举。两个 close 的调用顺序不作规范性要求，但旧 video/audio 都达到 terminal `closed` 是进入 `opening` 的屏障。
- 旧 video/audio `streamId` 和旧 `syncGroupId` 在关闭后全部退役。Nearcast/NA20 必须分配新的 `syncGroupId`，并以该组分别调用 `video.openStream` 和 `audio.openStream`；两次成功 open 都必须产生新的 `streamId`。
- 新 `video.openStream` 使用新的 `frameRate` / `bitrateKbps`，并保持原有 video source、codec/format、stream/profile、cursor、PTS、packetization 和时钟等上下文；新 `audio.openStream` 保持原有 AAC codec/transport、采样、packetization、PTS 和时钟等 media/timing 语义。两者都携带新的 `syncGroupId`。
- 任一新 open 失败时，已经成功打开的 replacement stream 属于不完整音视频对，必须先关闭。随后恢复旧 session 编码参数，并以旧 encoder/audio 语义重开完整音视频对；回退使用各自全新的 video/audio `streamId` 和另一个全新的 `syncGroupId`，不得复用退役标识。
- 仅当回退的 video/audio 都恢复成功时才报告 `rolledBack`、`sourceVideo.rollbackApplied=true`；否则报告 `failed`，关闭任何部分恢复的 stream，确保当前没有完整的活动音视频对，并在 `lastError` 中保留诊断信息。
- 仅当 session 完全没有任何 audio 或 video downstream stream 时才视为 idle，可省略 close，直接进入 `opening` 并创建完整的新音视频对；若存在单边或其他 partial media state，必须先 cleanup/normalize，不能直接 open。只有两个 open 都成功且 NT10 新编码参数实际生效后才进入 `streaming`。同步完成的调用可在唯一 RPC result 中报告 `state=applied`，异步调用则保留初始 `pending`，由 `cast.flowControlChanged` 和状态查询报告完成。

#### 3.4.5 错误

| 错误 | 场景 |
|---|---|
| `NOT_SUPPORTED` | `cast.flowControl.supportsVideoStreamParams` 或 source encoder capability 不支持；若当前已有 active video stream，还要求 `supportsActiveVideoReconfigure`。session 保持可用。 |
| `INVALID_ARGUMENT` | 未设置字段且未提供 `resetFields`，字段重复 reset，或 reset 字段名非法。 |
| `INVALID_STATE` | 没有可用的当前 cast session。 |
| `OUT_OF_RANGE` | 请求值不在 source/profile 声明的范围内。 |
| `BUSY` | 另一项音视频重配置事务正在进行。 |
| `MEDIA_SOURCE_UNAVAILABLE` | NT10 source 当前不可用。 |
| `MEDIA_FRAMERATE_UNSUPPORTED` / `MEDIA_BITRATE_UNSUPPORTED` | source 不支持请求的帧率或码率。 |
| `MEDIA_STREAM_START_FAILED` / `MEDIA_STREAM_STOP_FAILED` | close/open 或回退过程中的 stream 生命周期失败。 |
| `UNAVAILABLE` | 控制面或媒体 pipeline 暂时不可用。 |

## 4. State And Events

| Field | Meaning |
|---|---|
| `targetRenderFps` | 配置目标；`0` 表示不限速。 |
| `inputFps` | 当前输入 fps，不因本地 cap 自动降低。 |
| `renderFps` | 实际本地渲染 fps；通常不高于 `inputFps` 和非零 `targetRenderFps`。 |
| `dropMode` | `drop-late`、`drop-oldest`、`render-latest`。 |
| `videoQueueFrames` | 视频队列上限。 |
| `lateFrameThresholdMs` | late frame 判定阈值。 |
| `overlayEnabled` | 是否显示诊断 overlay。 |
| `droppedFrames` / `lateFrames` | 低频诊断统计。 |
| `sourceVideo` | 当前 session 的 source video 编码目标、实际生效值和重配置状态；不支持时可省略。 |

`cast.flowControlChanged` 只承载低频状态和统计摘要；高频媒体帧不进入该事件。

### 4.1 Event 示例

event:

```json
{
  "event": "cast.flowControlChanged",
  "intent": 1,
  "data": {
    "changedFields": [
      "targetRenderFps",
      "renderFps",
      "videoQueueDepth",
      "sourceVideo",
      "reconfigureId"
    ],
    "state": {
      "targetRenderFps": 10,
      "inputFps": 25.0,
      "renderFps": 10.0,
      "dropMode": "drop-late",
      "videoQueueFrames": 3,
      "videoQueueDepth": 1,
      "droppedFrames": 244,
      "lateFrames": 19,
      "overlayEnabled": true,
      "sourceVideo": {
        "source": "wireless_cast_video",
        "desiredFrameRate": 30,
        "desiredBitrateKbps": 6000,
        "effectiveFrameRate": 30,
        "effectiveBitrateKbps": 6000,
        "streamProfile": "media.video",
        "state": "streaming",
        "phase": "streaming",
        "activeStreamId": 4098,
        "rollbackApplied": false,
        "changedFields": [
          "frameRate",
          "bitrateKbps"
        ]
      }
    },
    "reconfigureId": "vr-20260716-001",
    "reason": "videoStreamReconfigure",
    "sampledAt": "2026-06-22T10:31:00Z"
  }
}
```

## 5. Rules

- 目标 fps 高于输入 fps 时，`targetRenderFps` 保留配置值，实际 `renderFps` 按输入 fps 运行。
- 手动 fps cap 不得阻塞 HID 输入消费或 AXTP 数据解析。
- 视频队列必须有上限，未渲染帧不得无限排队。
- 外部控制端不直接请求关键帧；内部可根据丢帧、解码和 fps 变化自动触发。
- overlay / diagnostics 是正式能力，但不得变成高频状态流。

## 6. Errors

| Error | Scenario |
|---|---|
| `INVALID_ARGUMENT` | 非法 drop mode 或字段组合。 |
| `OUT_OF_RANGE` | fps、queue 或 threshold 越界。 |
| `UNAVAILABLE` | media / render pipeline 不可用。 |

## 7. Review Items

| 问题 | 影响 | 当前建议 | 状态 |
|---|---|---|---|
| 是否暴露外部 keyframe request？ | scope | 不暴露；内部自动触发。 | `[REVIEW-OK]` |

## 8. Schema Reference

> 本节按当前 `contract/registry/domains/cast/domain.yaml` 整理字段事实；`Required=yes` 表示编码数据必须携带该字段，`Required=no` 表示可省略。`Empty` schema 无字段，未展开。

### CastGetFlowControlStateParams

Selector for cast receiver flow control state.

| Field | Required | Type | Field ID | Constraints / default | Description |
|---|---:|---|---:|---|---|
| `includeStats` | no | `bool` | `0x01` | default=true | Whether to include low-frequency diagnostic counters. |
| `includePolicy` | no | `bool` | `0x02` | default=true | Whether to include current flow policy fields. |
| `sessionId` | no | `string` | `0x03` | maxLength=128 | Optional receiver-local session id. |

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
| `sourceVideo` | no | `CastVideoStreamParamsState` | `0x10` | - | Effective source video parameters and reconfiguration state. |

### CastSetVideoStreamParamsParams

Request to update the active cast video stream frame rate or bitrate, optionally resetting selected fields.

| Field | Required | Type | Field ID | Constraints / default | Description |
|---|---:|---|---:|---|---|
| `sessionId` | no | `string` | `0x01` | maxLength=128 | Optional receiver-local cast session id. |
| `frameRate` | no | `uint32` | `0x02` | min=1; 0 invalid | Optional target encoded video frame rate; omission preserves the current session value (or leaves it unset), while resetFields clears it and restores source/profile default selection. |
| `bitrateKbps` | no | `uint32` | `0x03` | min=1; 0 invalid | Optional target encoded video bitrate in kbps; omission preserves the current session value (or leaves it unset), while resetFields clears it and restores source/profile default selection. |
| `resetFields` | no | `Array<string>` | `0x04` | itemType=string | Video parameter field names to reset to source/profile defaults. |

### CastSetVideoStreamParamsResult

Result and reconfiguration status for cast video stream parameter changes.

| Field | Required | Type | Field ID | Constraints / default | Description |
|---|---:|---|---:|---|---|
| `accepted` | yes | `bool` | `0x01` | - | Whether the receiver accepted the requested change. |
| `state` | yes | `enum` | `0x02` | pending/applied/failed/rolledBack/unchanged | Reconfiguration lifecycle state. |
| `sessionId` | no | `string` | `0x03` | maxLength=128 | Cast session associated with the result. |
| `reconfigureId` | no | `string` | `0x04` | maxLength=128 | Video stream reconfiguration operation id. |
| `previousStreamId` | no | `uint32` | `0x05` | - | Previously active video stream id. |
| `activeStreamId` | no | `uint32` | `0x06` | - | Active video stream id after the operation. |
| `sourceVideo` | yes | `CastVideoStreamParamsState` | `0x07` | - | Effective source video parameter state. |

### CastVideoStreamParamsState

Desired and effective cast source video stream parameters and reconfiguration state.

| Field | Required | Type | Field ID | Constraints / default | Description |
|---|---:|---|---:|---|---|
| `sessionId` | no | `string` | `0x01` | maxLength=128 | Cast session id. |
| `source` | no | `string` | `0x02` | maxLength=128 | Source identifier. |
| `desiredFrameRate` | no | `uint32` | `0x03` | min=1; 0 invalid | Requested encoder frame rate. |
| `desiredBitrateKbps` | no | `uint32` | `0x04` | min=1; 0 invalid | Requested encoder bitrate in kbps. |
| `effectiveFrameRate` | no | `uint32` | `0x05` | min=1; 0 invalid | Effective encoded frame rate. |
| `effectiveBitrateKbps` | no | `uint32` | `0x06` | min=1; 0 invalid | Effective encoded bitrate in kbps. |
| `streamProfile` | no | `string` | `0x07` | maxLength=64 | Effective video STREAM profile. |
| `encoder` | no | `string` | `0x08` | maxLength=128 | Encoder/profile selected by the source. |
| `reconfigureId` | no | `string` | `0x09` | maxLength=128 | Most recent reconfiguration operation id. |
| `state` | no | `enum` | `0x0A` | idle/pending/closing/opening/streaming/failed/rolledBack | Current reconfiguration state. |
| `phase` | no | `enum` | `0x0B` | idle/pending/closing/opening/streaming/failed/rolledBack | Current lifecycle phase. |
| `previousStreamId` | no | `uint32` | `0x0C` | - | Previous active downstream stream id. |
| `activeStreamId` | no | `uint32` | `0x0D` | - | Active downstream stream id. |
| `rollbackApplied` | no | `bool` | `0x0E` | - | Whether old parameters were restored after failure. |
| `lastError` | no | `CastLastError` | `0x0F` | - | Last reconfiguration or stream lifecycle error. |
| `changedFields` | no | `Array<string>` | `0x10` | itemType=string | Fields changed by the latest operation. |

### CastSetRenderFpsParams

Request to set receiver-local target render fps.

| Field | Required | Type | Field ID | Constraints / default | Description |
|---|---:|---|---:|---|---|
| `fps` | yes | `number` | `0x01` | min=0 | Target local render fps; zero means uncapped. |
| `sessionId` | no | `string` | `0x02` | maxLength=128 | Optional receiver-local session id. |
| `scope` | no | `enum` | `0x03` | enum=currentSession/default | State target hint; this is not an authorization scope. |

### CastSetFlowPolicyParams

Request to update receiver-local queue, late-frame, drop, and overlay policy.

| Field | Required | Type | Field ID | Constraints / default | Description |
|---|---:|---|---:|---|---|
| `videoQueueFrames` | no | `uint32` | `0x01` | min=1 | Maximum queued video frames. |
| `lateFrameThresholdMs` | no | `uint32` | `0x02` | - | Late-frame threshold in milliseconds. |
| `dropMode` | no | `enum` | `0x03` | enum=drop-late/drop-oldest/render-latest | Local frame drop policy. |
| `overlayEnabled` | no | `bool` | `0x04` | - | Whether receiver diagnostics overlay is enabled. |
| `sessionId` | no | `string` | `0x05` | maxLength=128 | Optional receiver-local session id. |
| `scope` | no | `enum` | `0x06` | enum=currentSession/default | State target hint; this is not an authorization scope. |

### CastFlowControlChangedEvent

Event payload for cast flow control changes or low-frequency diagnostic samples.

| Field | Required | Type | Field ID | Constraints / default | Description |
|---|---:|---|---:|---|---|
| `changedFields` | yes | `Array<string>` | `0x01` | itemType=string | Field names changed or sampled by this event. |
| `state` | yes | `CastFlowControlState` | `0x02` | - | Flow control state after the change or sample. |
| `reason` | no | `enum` | `0x03` | enum=manualFlowControl/videoStreamParams/videoStreamReconfigure/diagnosticsSample/sessionStarted/sessionStopped/unknown | Change or sampling reason. |
| `sampledAt` | no | `string` | `0x04` | maxLength=64 | Timestamp for this event. |
| `sourceVideo` | no | `CastVideoStreamParamsState` | `0x05` | - | Effective source video parameter state for this event. |
| `reconfigureId` | no | `string` | `0x06` | maxLength=128 | Associated video stream reconfiguration operation id. |

### CastFlowControlCapability

Capability descriptor for cast.flowControl.

| Field | Required | Type | Field ID | Constraints / default | Description |
|---|---:|---|---:|---|---|
| `supportsRenderFps` | no | `bool` | `0x01` | default=true | Whether receiver-local target render fps can be controlled. |
| `supportsQueuePolicy` | no | `bool` | `0x02` | default=true | Whether queue and late-frame policy can be controlled. |
| `supportsOverlay` | no | `bool` | `0x03` | default=true | Whether diagnostics overlay can be controlled. |
| `supportsStats` | no | `bool` | `0x04` | default=true | Whether low-frequency flow diagnostics are reported. |
| `exposesExternalKeyframeRequest` | no | `bool` | `0x05` | default=false | Whether a public keyframe request method is exposed; current draft keeps this false. |
| `supportsVideoStreamParams` | no | `bool` | `0x06` | default=false | Whether source video frame rate or bitrate can be updated. |
| `supportsActiveVideoReconfigure` | no | `bool` | `0x07` | default=false | Whether an active video stream can be reconfigured/replaced. |
| `supportedVideoStreamProfiles` | no | `Array<string>` | `0x08` | itemType=string | Video STREAM profiles supported by the method. |
| `supportedVideoEncoders` | no | `Array<string>` | `0x09` | itemType=string | Encoder/profile identifiers exposed for source video. |
| `supportsSourceSpecificVideoParams` | no | `bool` | `0x0A` | default=true | Whether support and ranges vary by source/profile. |
