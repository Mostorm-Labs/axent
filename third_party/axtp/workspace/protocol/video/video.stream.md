---
status: generated
contract: true
generated: true
domain: video
feature: video.stream
registry: ../../../../contract/registry/domains/video/domain.yaml
lastReviewed: 2026-06-15
---

# video.stream

## 0. 速读结论

| 项目 | 内容 |
|---|---|
| 这个能力做什么 | 通过 `video.openStream` / `video.closeStream` 建立和关闭 AXTP 内部视频业务流，并用 STREAM 数据面承载视频 chunk。 |
| 当前状态 | generated；已写入 `../../../../contract/registry/domains/video/domain.yaml`，并已刷新到 `contract/protocol/axtp.protocol.yaml` 与 `contract/generated/**`。 |
| 是否可直接实现 | 是，但实现合同以 `contract/protocol/axtp.protocol.yaml` / `contract/generated/**` 为准；本文保留的 open review markers 不属于已生成合同。 |
| 主要交互 | RPC + EVENT + STREAM |
| 是否使用 STREAM | 是。RPC 只负责建流、关流、状态和关键帧请求；视频数据走 `PayloadType=STREAM`。 |
| Registry readiness | ready；P0 / confirmed subset 已写入 registry source 并生成。 |
| Conformance | needed |
| 主要未决问题 | `peerRole` 字段命名、source state event 是否进入 MVP、legacy 映射细节和 streamId 复用策略仍需评审。 |

## 1. 功能说明

`video.stream` 是视频域内的 AXTP 业务流能力。它负责让一个端点把某个视频 source 输出到另一端点，并返回 STREAM 数据面所需的 `streamId`、profile、codec、时钟和同步元数据。

NA20/NT10 投屏场景中，NT10 插入源端 PC 后自动向 NA20 推送上游 H.264 视频。NA20 只把该上游 source 暴露成 `wireless_cast` source，并通过 `video.openStream` 建立 `NA20 -> MediaHost` 下游视频 stream。该下游 stream 可以由 NA20 producer 主动请求 Host 接收，也可以由 MediaHost receiver 在 source 仍 `available/receiving` 时主动拉取。

Nearcast 控制面可通过 `cast.setVideoStreamParams` 为当前 cast session 设置 NT10 编码器的目标 `frameRate` / `bitrateKbps`。这两个字段也可直接放在 `video.openStream` 中；它们表示 source encoder target，不是 MediaHost 本地 render fps。活动 downstream stream 变更时，Nearcast/NA20 把 video 和 audio 作为一个事务关闭并重开，为 replacement 分配新的 video/audio `streamId` 和新的 `syncGroupId`。

本文不定义 NT10 到 NA20 的无线投屏协议，也不定义音频流。音频同步字段需要与 `audio.stream` 保持一致。

## 2. 能力边界

| 类型 | 内容 |
|---|---|
| 包含 | 查询视频 stream capability、source 能力和 source 当前状态。 |
| 包含 | `video.openStream` 同时支持 producer-initiated open 和 receiver-initiated pull。 |
| 包含 | `video.closeStream` 可由任一端发起，且只关闭指定 downstream stream。 |
| 包含 | `video.openStream.frameRate` / `bitrateKbps` 的 source encoder target，以及由 `cast.setVideoStreamParams` 协调的音视频 close -> open 重配置。 |
| 包含 | 视频 stream lifecycle event、source available/receiving event、统计 event 和关键帧请求。 |
| 包含 | H.264 Annex-B 投屏路径，`wireless_cast` source 的 SPS/PPS 随关键帧发送。 |
| 不包含 | NT10 到 NA20 的 Wi-Fi 媒体协议、重传、加密、配对和 source 内部控制。 |
| 不包含 | 音频建流和 AAC payload，归 `audio.stream`。 |
| 不包含 | NDI、RTSP、RTMP/SRT 外部推流地址配置，归 `video.ndi`、`video.rtsp` 或后续 `video.pushStream`。 |
| 数据面 | 使用 AXTP `PayloadType=STREAM` 固定 16B header。STREAM header 不新增 codec、frameId、timestamp 或 domain 字段；视频业务元数据放在 `VideoChunkHeaderV1` 或 open/result context 中。 |

## 3. 方法 Methods

已生成 methodId、eventId、bitOffset 和 schema fieldId 以 contract/registry/generated 为准；本文不重新分配正式 ID，保留的 draft/review 标记仅作为后续修订输入。

### 3.0 方法速览

| Method | 调用类型 | 用途 | Params Schema | Result Schema | 是否触发事件 | 状态 |
|---|---|---|---|---|---|---|
| `video.getStreamCapabilities` | query | 查询视频流能力、source、codec 和限制。 | `VideoGetStreamCapabilitiesParams` | `VideoStreamCapabilities` | 否 | draft |
| `video.openStream` | command | 建立一路视频 downstream stream，支持 producer 主动 open 和 receiver 主动 pull。 | `VideoOpenStreamParams` | `VideoOpenStreamResult` | 是，`video.streamStateChanged` | draft |
| `video.closeStream` | command | 正常关闭一路视频 stream。 | `VideoCloseStreamParams` | `VideoCloseStreamResult` | 是，`video.streamStateChanged` | draft |
| `video.getStreamState` | query | 查询已建立视频 stream 的业务状态。 | `VideoGetStreamStateParams` | `VideoStreamState` | 否 | draft |
| `video.getStreamSourceState` | query | 查询视频 source 自身的 available/receiving/stopped 状态。 | `VideoGetStreamSourceStateParams` | `VideoStreamSourceState` | 否 | draft |
| `video.requestKeyFrame` | command | 请求视频 producer 输出关键帧。 | `VideoRequestKeyFrameParams` | `VideoRequestKeyFrameResult` | 可触发后续 `video.streamStatsReported` | draft |

### 3.1 `video.getStreamCapabilities`

**用途**：查询设备可打开的视频 source、codec、profile、同步字段和双入口 open 支持情况。

| 项 | 内容 |
|---|---|
| 调用类型 | query |
| Params Schema | `VideoGetStreamCapabilitiesParams` |
| Result Schema | `VideoStreamCapabilities` |
| 是否触发事件 | 否 |
| 幂等性 / 异步性 | 幂等；同步返回当前能力快照。 |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_ARGUMENT`, `PERMISSION_DENIED`, `UNAVAILABLE` |

#### 3.1.1 请求参数 Params：`VideoGetStreamCapabilitiesParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `source` | string | no | source id | omitted | 可选按 source 查询；省略表示返回全部可见视频 source。 |
| `includeRuntimeState` | bool | no | `true`, `false` | `false` | 是否同时返回 source 当前 `available/receiving` 状态。 |

#### 3.1.2 返回结果 Result：`VideoStreamCapabilities`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `capability` | string | yes | fixed `video.stream` | none | capability 名称。 |
| `sources` | `VideoStreamSource[]` | yes | array | none | 可打开的视频 source 列表。 |
| `streamProfiles` | string[] | yes | `media.video` | none | 支持的 stream profile。 |
| `openModes` | string[] | yes | `producer_open`, `receiver_pull` | none | 是否支持生产者主动 open 和接收者主动拉取。 |
| `peerRoles` | string[] | yes | `receiver`, `transmitter` | none | `openStream` / `closeStream` 的对端媒体角色枚举。 |
| `supportsSourceStateEvent` | bool | yes | `true`, `false` | none | 是否支持 `video.streamSourceStateChanged`。 |
| `supportsSyncGroup` | bool | yes | `true`, `false` | none | 是否支持与音频流共享同步组。 |
| `flowControlManagedByRuntime` | bool | yes | `true`, `false` | `true` | 普通业务 App 是否无需直接调用 `stream.flowControl`。 |

#### 3.1.3 d block 示例

request:

```json
{
  "id": 101,
  "method": "video.getStreamCapabilities",
  "params": {
    "source": "wireless_cast_video",
    "includeRuntimeState": true
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
    "capability": "video.stream",
    "sources": [
      {
        "source": "wireless_cast_video",
        "codecs": [
          "h264"
        ],
        "state": "receiving"
      }
    ],
    "streamProfiles": [
      "media.video"
    ],
    "openModes": [
      "producer_open",
      "receiver_pull"
    ],
    "peerRoles": [
      "receiver",
      "transmitter"
    ],
    "supportsSourceStateEvent": true,
    "supportsSyncGroup": true,
    "flowControlManagedByRuntime": true
  }
}
```

#### 3.1.4 可能触发的事件

| Event | 触发条件 | Payload Schema | 客户端处理建议 |
|---|---|---|---|
| 无 | 查询不应改变状态。 | none | 无需处理。 |

#### 3.1.5 错误

| 错误 | 场景 | 返回建议 |
|---|---|---|
| `NOT_SUPPORTED` | 设备不支持 `video.stream`。 | 返回 unsupported feature。 |
| `INVALID_ARGUMENT` | `source` 字段格式非法。 | 返回字段路径和合法 source 约束。 |

### 3.2 `video.openStream`

**用途**：建立一路视频业务 stream，成功后返回 `streamId`，随后 producer 使用 AXTP STREAM 发送视频 chunk。

| 项 | 内容 |
|---|---|
| 调用类型 | command |
| Params Schema | `VideoOpenStreamParams` |
| Result Schema | `VideoOpenStreamResult` |
| 是否触发事件 | 是，进入 `opening` / `streaming` 后触发 `video.streamStateChanged` |
| 幂等性 / 异步性 | 非幂等；每次成功 open 返回新的 `streamId`。accepted 前 producer 不得发送 STREAM。 |
| 常见错误 | `INVALID_ARGUMENT`, `BUSY`, `RESOURCE_EXHAUSTED`, `MEDIA_SOURCE_NOT_FOUND`, `MEDIA_SOURCE_UNAVAILABLE`, `MEDIA_CODEC_UNSUPPORTED`, `MEDIA_STREAM_START_FAILED` |

`video.openStream` 支持两种调用方向：

| 场景 | Requester | Responder | `peerRole` | 结果 |
|---|---|---|---|---|
| producer-initiated open | NA20 producer | MediaHost receiver | `receiver` | NA20 请求 MediaHost 接收视频。 |
| receiver-initiated pull | MediaHost receiver | NA20 producer | `transmitter` | MediaHost 请求 NA20 发送视频。 |

两种方式都只建立 `NA20 -> MediaHost` downstream stream，不隐式停止、断开或重建 `NT10 -> NA20` upstream source。

#### 3.2.1 请求参数 Params：`VideoOpenStreamParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `source` | string | yes | source id | none | 视频源。NA20/NT10 投屏为 `wireless_cast`。 |
| `peerRole` | enum | yes | `receiver`, `transmitter` | none | 声明接收本请求的对端应扮演的媒体角色。 |
| `codec` | enum | yes | `h264`, `mjpeg`, `raw` | none | NA20/NT10 投屏 MVP 使用 `h264`。 |
| `codecFormat` | enum | no | `annexb`, `avcc` | capability default | H.264 格式。`source=wireless_cast` 固定为 `annexb`。 |
| `stream` | string | no | `main`, `sub`, source-defined | `main` | 同一 source 的码流档位。 |
| `width` | uint32 | no | capability range | omitted | 期望宽度；省略表示由 source/设备决定。 |
| `height` | uint32 | no | capability range | omitted | 期望高度。 |
| `frameRate` | uint32 | no | capability range，且 `>=1`（`0` 非法） | omitted | 选定 source encoder 的目标帧率；不是接收端本地 render fps。省略时使用 session/source 默认值。 |
| `bitrateKbps` | uint32 | no | capability range，且 `>=1`（`0` 非法） | omitted | 选定 source encoder 的目标码率，单位 kbps。省略时使用 session/source 默认值。 |
| `streamProfile` | string | no | `media.video` | `media.video` | STREAM profile。 |
| `cursorUnit` | enum | no | `timestampUs`, `sourceCaptureTimestampUs` | `sourceCaptureTimestampUs` for wireless cast | STREAM 16B header 中 `cursor` 的业务单位；NA20/NT10 投屏使用 NT10 源端采集时间戳。 |
| `syncGroupId` | string | no | product/session scoped | omitted | 与 audio stream 绑定的同步组，可由 requester 指定或 responder 返回。 |
| `castSessionId` | string | no | product/session scoped | omitted | 投屏会话关联 ID，不单独创建 cast domain。 |
| `clockDomain` | string | no | `nt10_media_clock`, source-defined | `nt10_media_clock` for wireless cast | 媒体时间戳来源。 |
| `receiverClockDomain` | string | no | `na20_receive_clock`, receiver-defined | omitted | NA20 接收时钟域，用于 jitter/诊断。 |
| `maxDataSize` | uint32 | no | transport/profile limit | omitted | 单个 STREAM payload data 最大大小建议。 |
| `videoPtsMode` | enum | no | `sameAsCursor`, `explicit` | `sameAsCursor` for wireless cast | 视频 PTS 来源；MVP 中 video PTS 等于 STREAM `cursor`。 |
| `timebase` | uint32 | no | ticks/sec | `1000000` for wireless cast | video PTS 的 timebase。 |
| `packetizationMode` | enum | no | `completeFrame`, `fragmentedFrame` | `completeFrame` for wireless cast | 视频 packetization 模式；MVP 每个 STREAM packet 是完整帧。 |

#### 3.2.1a 编码参数优先级与重配置

`video.openStream` 的显式 `frameRate` / `bitrateKbps` 只作用于本次 open，不写回 cast session；显式 `0` 对这两个字段均非法。省略字段时继承当前 session 的 `cast.setVideoStreamParams` 值，不能用省略来清除 session 配置；清除必须调用 `cast.setVideoStreamParams(resetFields=[...])`。未显式提供时，Nearcast 按以下顺序选择 source encoder target：

1. 本次 `video.openStream` 的显式字段；
2. 当前 cast session 通过 `cast.setVideoStreamParams` 设置的值；
3. `VideoStreamSource` / stream profile 声明的 source 默认值。

`cast.setVideoStreamParams` 应用到活动投屏时，使用完整的音视频 replacement 事务，并按 `pending` -> `closing` -> `opening` -> `streaming` 推进。`closing` 阶段对旧 video `streamId` 发送 `video.closeStream(reason=encodingReconfigure)`，同时对旧 audio `streamId` 发送 `audio.closeStream`；audio close 的 `reason` 可以省略。两个 close 的顺序不作规范性要求，但旧 video/audio 都达到 terminal `closed` 后才能进入 `opening`。

进入 `opening` 时，旧 video/audio `streamId` 和旧 `syncGroupId` 都已退役。Nearcast/NA20 分配新的 `syncGroupId`：`video.openStream` 使用 `peerRole=transmitter`、新的 `frameRate` / `bitrateKbps`，并保持原有 video source、codec/format、stream/profile、cursor、PTS、packetization 和时钟等上下文；配对的 `audio.openStream` 保持原有 AAC media/timing 语义。两次 open 都携带新组并各自返回新的 `streamId`。只有两次 open 都成功且 NT10 新编码参数实际生效后，事务才进入 `streaming` 并由 cast 控制面报告 `applied`。这里的 `idle` 严格表示不存在任何 audio 或 video downstream stream；只有完全无流时才可省略 close、直接用新 `syncGroupId` 打开完整的 video/audio 对。若存在单边或其他 partial media state，必须先 cleanup/normalize，不能按 idle direct-open。

#### 3.2.2 返回结果 Result：`VideoOpenStreamResult`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `streamId` | uint32 | yes | `1..0x7FFFFFFF` | none | STREAM 数据面 stream id。 |
| `state` | enum | yes | `opening`, `streaming` | none | 初始业务状态。 |
| `source` | string | yes | source id | none | 实际绑定 source。 |
| `peerRole` | enum | yes | `receiver`, `transmitter` | none | 被请求端确认的对端媒体角色。 |
| `codec` | enum | yes | `h264`, `mjpeg`, `raw` | none | 实际 codec。 |
| `width` | uint32 | no | negotiated width | omitted | 协商后的帧宽度。 |
| `height` | uint32 | no | negotiated height | omitted | 协商后的帧高度。 |
| `frameRate` | uint32 | no | negotiated value | omitted | source encoder 实际生效的帧率。 |
| `bitrateKbps` | uint32 | no | negotiated value | omitted | source encoder 实际生效的码率，单位 kbps。 |
| `codecFormat` | enum | no | `annexb`, `avcc` | omitted | H.264 格式。 |
| `streamProfile` | string | yes | `media.video` | none | 归一化后的 profile。 |
| `cursorUnit` | enum | yes | `timestampUs`, `sourceCaptureTimestampUs` | none | STREAM `cursor` 单位。 |
| `syncGroupId` | string | no | product/session scoped | omitted | 与音频同步组。 |
| `castSessionId` | string | no | product/session scoped | omitted | 投屏会话关联 ID。 |
| `clockDomain` | string | no | source-defined | omitted | 源媒体时钟域。 |
| `receiverClockDomain` | string | no | receiver-defined | omitted | 接收时钟域。 |
| `maxDataSize` | uint32 | no | negotiated limit | omitted | 每个 STREAM chunk data 最大长度。 |
| `parameterSetsInKeyFrame` | bool | no | `true`, `false` | capability default | `wireless_cast` H.264 Annex-B 必须为 `true`。 |
| `videoPtsMode` | enum | no | `sameAsCursor`, `explicit` | omitted | 协商后的视频 PTS 来源。 |
| `timebase` | uint32 | no | ticks/sec | omitted | 协商后的 video PTS timebase。 |
| `packetizationMode` | enum | no | `completeFrame`, `fragmentedFrame` | omitted | 协商后的视频 packetization 模式。 |

#### 3.2.3 d block 示例

request:

```json
{
  "id": 102,
  "method": "video.openStream",
  "params": {
    "source": "wireless_cast_video",
    "peerRole": "receiver",
    "codec": "h264",
    "frameRate": 30,
    "bitrateKbps": 6000
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
    "streamId": 4097,
    "state": "streaming",
    "source": "wireless_cast_video",
    "peerRole": "receiver",
    "codec": "h264",
    "frameRate": 30,
    "bitrateKbps": 6000,
    "streamProfile": "media.video",
    "cursorUnit": "sourceCaptureTimestampUs",
    "parameterSetsInKeyFrame": true,
    "videoPtsMode": "sameAsCursor",
    "timebase": 1000000,
    "packetizationMode": "completeFrame"
  }
}
```

#### 3.2.4 可能触发的事件

| Event | 触发条件 | Payload Schema | 客户端处理建议 |
|---|---|---|---|
| `video.streamStateChanged` | stream 进入 `opening`、`streaming`、`closed` 或 `failed`。 | `VideoStreamStateChangedEvent` | 更新播放器 pipeline；必要时调用 `video.getStreamState` 校准。 |
| `video.streamSourceStateChanged` | producer-open 被拒后 source 仍可用，或 source lifecycle 变化。 | `VideoStreamSourceStateChangedEvent` | 缓存 source 状态；receiver ready 后可主动 `video.openStream`。 |

#### 3.2.5 错误

| 错误 | 场景 | 返回建议 |
|---|---|---|
| `MEDIA_SOURCE_NOT_FOUND` | `source` 不存在。 | 不创建 stream。 |
| `MEDIA_SOURCE_UNAVAILABLE` | `source` 存在但未 available/receiving。 | Host 可等待 source event 后重试。 |
| `MEDIA_CODEC_UNSUPPORTED` | 请求 codec 或格式不支持。 | 返回支持的 codec/format 线索。 |
| `BUSY` / `RESOURCE_EXHAUSTED` | 同一 source/mediaKind 已有 active downstream stream 或资源不足。 | 返回可重试提示；不得发送 STREAM。 |
| `MEDIA_STREAM_START_FAILED` | 建立 stream context 或 producer 绑定失败。 | 返回失败原因；可触发 `video.streamStateChanged(state=failed)`。 |

### 3.3 `video.closeStream`

**用途**：关闭一路已建立的视频 stream。该方法关闭 downstream stream，不默认停止 upstream source。`reason=encodingReconfigure` 表示为 source encoder 参数变更让出旧 stream；在 `cast.setVideoStreamParams` 事务中，调用方还必须关闭配对的旧 audio stream，并等待两侧都为 `closed` 后再重开完整音视频对。

| 项 | 内容 |
|---|---|
| 调用类型 | command |
| Params Schema | `VideoCloseStreamParams` |
| Result Schema | `VideoCloseStreamResult` |
| 是否触发事件 | 是，`video.streamStateChanged(state=closed/failed)` |
| 幂等性 / 异步性 | 幂等；重复 close 或双方同时 close 最终收敛到同一 terminal state。 |
| 常见错误 | `STREAM_NOT_FOUND`, `STREAM_CLOSED`, `INVALID_STATE`, `MEDIA_STREAM_STOP_FAILED` |

#### 3.3.1 请求参数 Params：`VideoCloseStreamParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `streamId` | uint32 | yes | active stream id | none | 要关闭的 video stream。 |
| `peerRole` | enum | no | `receiver`, `transmitter` | omitted | 被请求端在该 stream 中的媒体角色。Host 关闭 NA20 producer 时可填 `transmitter`。 |
| `reason` | enum | no | `receiver_closed`, `user_stop`, `not_needed`, `source_disconnected`, `producer_stopped`, `session_lost`, `receiver_timeout`, `encodingReconfigure`, `error` | `user_stop` | 关闭原因；编码参数替换使用 `encodingReconfigure`。 |
| `finalCursor` | uint64 | no | cursorUnit-defined | omitted | 调用方最后处理到的 cursor。 |

#### 3.3.2 返回结果 Result：`VideoCloseStreamResult`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `streamId` | uint32 | yes | stream id | none | 被关闭的 stream。 |
| `state` | enum | yes | `closing`, `closed`, `failed` | none | close 后状态。 |
| `reason` | enum | no | same as params（含 `encodingReconfigure`） | omitted | 最终关闭原因。 |
| `alreadyClosed` | bool | no | `true`, `false` | `false` | 是否此前已经进入 terminal state。 |

#### 3.3.3 d block 示例

request:

```json
{
  "id": 103,
  "method": "video.closeStream",
  "params": {
    "streamId": 4097,
    "reason": "user_stop"
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
    "streamId": 4097,
    "state": "closed",
    "reason": "user_stop",
    "alreadyClosed": false
  }
}
```

#### 3.3.4 可能触发的事件

| Event | 触发条件 | Payload Schema | 客户端处理建议 |
|---|---|---|---|
| `video.streamStateChanged` | stream 关闭、失败或重复 close 收敛。 | `VideoStreamStateChangedEvent` | 释放 decoder、buffer 和 UI 状态。 |

#### 3.3.5 错误

| 错误 | 场景 | 返回建议 |
|---|---|---|
| `STREAM_NOT_FOUND` | streamId 不属于当前 AXTP session。 | 调用方本地清理旧 context。 |
| `STREAM_CLOSED` | stream 已关闭且实现选择返回错误。 | 也可返回 `alreadyClosed=true` 的成功结果。 |
| `MEDIA_STREAM_STOP_FAILED` | 设备无法停止 producer 或关闭旧 stream。 | 返回 typed error，并尽快进入 `failed` terminal state；若由编码参数重配置触发，按回退规则恢复旧 encoder/audio 语义的完整音视频对。 |

#### 3.3.6 音视频编码重配置失败与回退

Nearcast 将 `cast.setVideoStreamParams` 的 close -> open 视为一个完整的音视频重配置事务：

- 旧 video/audio 都关闭后，其 `streamId` 和共享的旧 `syncGroupId` 不得复用；关闭事件和后续数据包都以旧流为 terminal。
- 新 video 或 audio 只有一侧 open 成功时，该 replacement 是不完整音视频对，必须关闭已经成功的一侧，再开始回退。
- 新 `video.openStream` 返回 `MEDIA_FRAMERATE_UNSUPPORTED`、`MEDIA_BITRATE_UNSUPPORTED` 或其他任一 open 失败时，Nearcast 恢复 session 中的旧参数，并以旧 encoder/audio 语义重开完整音视频对。回退必须分配全新的 video/audio `streamId` 和另一个全新的 `syncGroupId`。
- 只有回退的 video/audio 两侧都恢复成功时，cast 控制面才报告 `state=rolledBack`；任一侧失败时关闭部分恢复的 stream，报告 `state=failed`，明确当前没有完整的 active 音视频对，并保留最后一个 typed error。
- `NOT_SUPPORTED` 仅表示能力或 source/profile 不支持该操作，不应关闭 AXTP/cast session；调用方可继续使用其他 RPC。

### 3.4 `video.getStreamState`

**用途**：查询已建立视频 stream 的业务状态、codec、source、同步元数据和统计摘要。

| 项 | 内容 |
|---|---|
| 调用类型 | query |
| Params Schema | `VideoGetStreamStateParams` |
| Result Schema | `VideoStreamState` |
| 是否触发事件 | 否 |
| 幂等性 / 异步性 | 幂等；同步返回当前快照。 |
| 常见错误 | `STREAM_NOT_FOUND`, `STREAM_CLOSED` |

#### 3.4.1 请求参数 Params：`VideoGetStreamStateParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `streamId` | uint32 | yes | stream id | none | 查询目标 stream。 |

#### 3.4.2 返回结果 Result：`VideoStreamState`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `streamId` | uint32 | yes | stream id | none | 视频 stream id。 |
| `state` | enum | yes | `opening`, `streaming`, `closing`, `closed`, `failed` | none | 业务流状态。 |
| `source` | string | yes | source id | none | 绑定 source。 |
| `codec` | enum | yes | `h264`, `mjpeg`, `raw` | none | 当前 codec。 |
| `frameRate` | uint32 | no | effective value | omitted | source encoder 当前实际帧率。 |
| `bitrateKbps` | uint32 | no | effective value | omitted | source encoder 当前实际码率，单位 kbps。 |
| `syncGroupId` | string | no | product/session scoped | omitted | 同步组。 |
| `castSessionId` | string | no | product/session scoped | omitted | 投屏会话关联 ID。 |
| `lastSeqId` | uint32 | no | uint32 | omitted | 最近发送或接收的 STREAM seq。 |
| `lastCursor` | uint64 | no | cursorUnit-defined | omitted | 最近 cursor。 |
| `reason` | string | no | state reason | omitted | terminal 或异常原因。 |

#### 3.4.3 d block 示例

request:

```json
{
  "id": 104,
  "method": "video.getStreamState",
  "params": {
    "streamId": 4097
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
    "streamId": 4097,
    "state": "streaming",
    "source": "wireless_cast_video",
    "codec": "h264",
    "streamProfile": "media.video"
  }
}
```

#### 3.4.4 可能触发的事件

| Event | 触发条件 | Payload Schema | 客户端处理建议 |
|---|---|---|---|
| 无 | 查询不改变状态。 | none | 无需处理。 |

#### 3.4.5 错误

| 错误 | 场景 | 返回建议 |
|---|---|---|
| `STREAM_NOT_FOUND` | streamId 不存在或已随 session lost 失效。 | 释放本地 context。 |

### 3.5 `video.getStreamSourceState`

**用途**：查询视频 source 自身状态。source state 与 downstream stream state 解耦。

| 项 | 内容 |
|---|---|
| 调用类型 | query |
| Params Schema | `VideoGetStreamSourceStateParams` |
| Result Schema | `VideoStreamSourceState` |
| 是否触发事件 | 否 |
| 幂等性 / 异步性 | 幂等；同步返回当前 source 快照。 |
| 常见错误 | `MEDIA_SOURCE_NOT_FOUND` |

#### 3.5.1 请求参数 Params：`VideoGetStreamSourceStateParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `source` | string | yes | source id | none | 查询目标 source。 |

#### 3.5.2 返回结果 Result：`VideoStreamSourceState`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `source` | string | yes | source id | none | source id。 |
| `mediaKind` | enum | yes | `video` | `video` | 媒体类型。 |
| `state` | enum | yes | `idle`, `available`, `receiving`, `stopped`, `failed` | none | source 当前状态。 |
| `codecs` | enum[] | no | `h264`, `mjpeg`, `raw` | omitted | 当前可用 codec。 |
| `castSessionId` | string | no | product/session scoped | omitted | 投屏会话关联 ID。 |
| `retainable` | bool | no | `true`, `false` | omitted | receiver close 后是否可保留 upstream source。 |
| `activeStreamId` | uint32 | no | stream id | omitted | 当前 active downstream stream；没有则省略。 |
| `lastOpenRejectedReason` | string | no | reason enum/string | omitted | 最近一次 producer-open 被拒原因。 |
| `receiverTimestampUs` | uint64 | no | microseconds | omitted | NA20 接收时钟时间戳。 |

#### 3.5.3 d block 示例

request:

```json
{
  "id": 105,
  "method": "video.getStreamSourceState",
  "params": {
    "source": "wireless_cast_video"
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
    "source": "wireless_cast_video",
    "state": "receiving",
    "available": true
  }
}
```

#### 3.5.4 可能触发的事件

| Event | 触发条件 | Payload Schema | 客户端处理建议 |
|---|---|---|---|
| 无 | 查询不改变状态。 | none | 无需处理。 |

#### 3.5.5 错误

| 错误 | 场景 | 返回建议 |
|---|---|---|
| `MEDIA_SOURCE_NOT_FOUND` | source 不存在。 | 清理 source cache。 |

### 3.6 `video.requestKeyFrame`

**用途**：MediaHost 检测 H.264 解码失败、seq gap 或窗口重新打开时，请求 producer 尽快输出关键帧。

| 项 | 内容 |
|---|---|
| 调用类型 | command |
| Params Schema | `VideoRequestKeyFrameParams` |
| Result Schema | `VideoRequestKeyFrameResult` |
| 是否触发事件 | 不要求立即触发；后续可通过 stats 或 STREAM 关键帧体现。 |
| 幂等性 / 异步性 | 可重复调用；producer 可合并短时间内多次请求。 |
| 常见错误 | `STREAM_NOT_FOUND`, `MEDIA_CODEC_UNSUPPORTED`, `NOT_SUPPORTED`, `BUSY` |

#### 3.6.1 请求参数 Params：`VideoRequestKeyFrameParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `streamId` | uint32 | yes | active video stream id | none | 目标视频 stream。 |
| `reason` | enum | no | `seq_gap`, `decode_error`, `receiver_reopen`, `startup`, `manual` | `manual` | 请求原因。 |

#### 3.6.2 返回结果 Result：`VideoRequestKeyFrameResult`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `streamId` | uint32 | yes | stream id | none | 目标 stream。 |
| `accepted` | bool | yes | `true`, `false` | none | producer 是否接受请求。 |
| `expectedWithinMs` | uint32 | no | implementation-defined | omitted | 预计关键帧时间。 |

#### 3.6.3 d block 示例

request:

```json
{
  "id": 106,
  "method": "video.requestKeyFrame",
  "params": {
    "streamId": 4097,
    "reason": "user_request"
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
    "streamId": 4097,
    "accepted": true
  }
}
```

#### 3.6.4 可能触发的事件

| Event | 触发条件 | Payload Schema | 客户端处理建议 |
|---|---|---|---|
| `video.streamStatsReported` | producer 上报关键帧计数或恢复状态。 | `VideoStreamStatsReportedEvent` | 可用于诊断；播放恢复以 STREAM 数据为准。 |

#### 3.6.5 错误

| 错误 | 场景 | 返回建议 |
|---|---|---|
| `STREAM_NOT_FOUND` | stream 不存在。 | 停止请求，等待新 stream。 |
| `MEDIA_CODEC_UNSUPPORTED` | 当前 codec 不支持关键帧请求。 | 播放器按 codec 策略恢复。 |

## 4. 事件 Events

### 4.0 事件速览

| Event | 触发条件 | Payload Schema | 客户端处理建议 | 状态 |
|---|---|---|---|---|
| `video.streamStateChanged` | 视频 stream lifecycle 变化。 | `VideoStreamStateChangedEvent` | 创建/释放 decoder，必要时查询 state 校准。 | draft |
| `video.streamSourceStateChanged` | 视频 source lifecycle 变化，或 producer-open 被拒后 source 仍可用。 | `VideoStreamSourceStateChangedEvent` | 更新 source cache；receiver ready 后可主动 pull。 | draft |
| `video.streamStatsReported` | 周期统计、丢帧、背压或诊断上报。 | `VideoStreamStatsReportedEvent` | 更新诊断 UI；不作为媒体数据。 | draft |

### 4.1 `video.streamStateChanged`

**触发条件**：

- `video.openStream` accepted 后进入 `opening` 或 `streaming`。
- `video.closeStream`、source stopped、producer failure、receiver timeout 或 session cleanup 导致 terminal state。
- 同一 stream 发生重复 close 或交叉 close，最终状态收敛。

#### Payload：`VideoStreamStateChangedEvent`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `streamId` | uint32 | yes | stream id | none | 变化的 stream。 |
| `state` | enum | yes | `opening`, `streaming`, `closing`, `closed`, `failed` | none | 新状态。 |
| `source` | string | yes | source id | none | 绑定 source。 |
| `codec` | enum | no | `h264`, `mjpeg`, `raw` | omitted | 当前 codec。 |
| `frameRate` | uint32 | no | effective value | omitted | 事件时 source encoder 实际帧率。 |
| `bitrateKbps` | uint32 | no | effective value | omitted | 事件时 source encoder 实际码率，单位 kbps。 |
| `reason` | string | no | reason enum/string | omitted | 变化原因。 |
| `closeOrigin` | enum | no | `producer`, `receiver`, `session`, `unknown` | `unknown` | terminal state 来源。 |
| `syncGroupId` | string | no | product/session scoped | omitted | 同步组。 |
| `castSessionId` | string | no | product/session scoped | omitted | 投屏会话关联 ID。 |
| `lastSeqId` | uint32 | no | uint32 | omitted | 最近 seq。 |
| `lastCursor` | uint64 | no | cursorUnit-defined | omitted | 最近 cursor。 |

#### d block 示例

```json
{
  "event": "video.streamStateChanged",
  "intent": 1,
  "data": {
    "streamId": 4097,
    "state": "streaming",
    "source": "wireless_cast_video",
    "reason": "user_request"
  }
}
```


#### 客户端处理建议

| 场景 | 建议 |
|---|---|
| `opening` / `streaming` | 创建或保持 decoder、jitter buffer 和 A/V sync。 |
| `closed` / `failed` | 释放 decoder、buffer 和 UI 播放状态。 |
| event 丢失或重连 | 重连后不能复用旧 `streamId`，必须重新 open。 |

### 4.2 `video.streamSourceStateChanged`

**触发条件**：

- NT10 接入 NA20 后，`wireless_cast` 视频 source 进入 `available` 或 `receiving`。
- NA20 producer-open 被 MediaHost 拒绝，但 source 仍 `available/receiving`。
- NT10 拔出、断连或停止发送视频，source 进入 `stopped` / `failed`。
- MediaHost receiver close 后，NA20 保留 upstream source 且 downstream stream 已关闭。

#### Payload：`VideoStreamSourceStateChangedEvent`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `source` | string | yes | source id | none | source id。 |
| `mediaKind` | enum | yes | `video` | `video` | 媒体类型。 |
| `state` | enum | yes | `idle`, `available`, `receiving`, `stopped`, `failed` | none | source 状态。 |
| `codecs` | enum[] | no | `h264`, `mjpeg`, `raw` | omitted | 当前可用 codec。 |
| `castSessionId` | string | no | product/session scoped | omitted | 投屏会话关联 ID。 |
| `activeStreamId` | uint32 | no | stream id | omitted | 若已有 active downstream stream，则提供。 |
| `retainable` | bool | no | `true`, `false` | omitted | receiver close 后是否可保留 upstream source。 |
| `reason` | string | no | `nt10_inserted`, `producer_open_rejected`, `receiver_closed`, `source_disconnected`, `timeout`, `unknown` | `unknown` | source 状态原因。 |
| `lastOpenRejectedReason` | string | no | `receiver_not_ready`, `policy_rejected`, `resource_exhausted`, `unsupported`, `unknown` | omitted | producer-open 被拒原因。 |
| `receiverTimestampUs` | uint64 | no | microseconds | omitted | NA20 接收时钟时间。 |

#### d block 示例

```json
{
  "event": "video.streamSourceStateChanged",
  "intent": 1,
  "data": {
    "source": "wireless_cast_video",
    "mediaKind": "video",
    "state": "receiving",
    "reason": "nt10_inserted"
  }
}
```


#### 客户端处理建议

| 场景 | 建议 |
|---|---|
| `available` / `receiving` 且无 active stream | MediaHost 可在 receiver service ready 后调用 `video.openStream(peerRole=transmitter)` 主动拉取。 |
| producer-open 被拒后收到事件 | 不视为错误终态；等待用户打开窗口或服务恢复后 pull。 |
| `stopped` / `failed` | 清理 source cache；已打开 stream 等待 close/terminal state 或本地释放。 |

### 4.3 `video.streamStatsReported`

**触发条件**：

- 设备周期上报视频 stream 统计。
- 出现丢帧、seq gap、背压、关键帧请求或诊断采样。

#### Payload：`VideoStreamStatsReportedEvent`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `streamId` | uint32 | yes | stream id | none | 统计目标 stream。 |
| `state` | enum | yes | stream state | none | 当前状态。 |
| `bytesSent` | uint64 | no | bytes | omitted | 已发送字节数。 |
| `chunksSent` | uint64 | no | count | omitted | 已发送 chunk 数。 |
| `framesSent` | uint64 | no | count | omitted | 已发送帧数。 |
| `droppedFrames` | uint64 | no | count | omitted | 丢弃帧数。 |
| `keyFramesSent` | uint64 | no | count | omitted | 关键帧数。 |
| `lastSeqId` | uint32 | no | uint32 | omitted | 最近 seq。 |
| `lastCursor` | uint64 | no | cursorUnit-defined | omitted | 最近 cursor。 |

#### d block 示例

```json
{
  "event": "video.streamStatsReported",
  "intent": 1,
  "data": {
    "streamId": 4097,
    "state": "streaming"
  }
}
```


#### 客户端处理建议

| 场景 | 建议 |
|---|---|
| 周期统计 | 更新诊断 UI，不改变播放状态。 |
| 丢帧/解码失败 | 可调用 `video.requestKeyFrame`。 |

## 5. Capability

Capability name: `video.stream`。

| 能力字段 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `capability` | string | yes | fixed `video.stream` | none | capability 名称。 |
| `sources` | `VideoStreamSource[]` | yes | array | none | 可打开 source。 |
| `streamProfiles` | string[] | yes | `media.video` | none | 支持 profile。 |
| `openModes` | string[] | yes | `producer_open`, `receiver_pull` | none | 支持的建流发起方式。 |
| `peerRoles` | string[] | yes | `receiver`, `transmitter` | none | 对端媒体角色枚举。 |
| `maxConcurrentStreams` | uint32 | no | `1..N` | device-defined | 当前 session 可同时打开的视频 stream 数。 |
| `sameSourceMaxActiveStreams` | uint32 | no | `1..N` | `1` | 同一 source/mediaKind active downstream stream 数。 |
| `supportsSourceStateEvent` | bool | yes | bool | none | 是否支持 source available/receiving event。 |
| `supportsReceiverPull` | bool | yes | bool | none | MediaHost 是否可主动拉取。 |
| `supportsProducerOpen` | bool | yes | bool | none | NA20 是否可主动 open。 |
| `supportsRequestKeyFrame` | bool | no | bool | `true` for H.264 | 是否支持关键帧请求。 |
| `flowControlManagedByRuntime` | bool | yes | bool | `true` | 普通业务 App 是否无需直接调用公共流控。 |

`video.stream` 的 `VideoStreamCapabilities` 不新增 cast 控制方法字段：`cast.setVideoStreamParams` 的方法能力由 `cast.flowControl.supportsVideoStreamParams` / `supportsActiveVideoReconfigure` 声明；所选 source profile 的范围和重配置字段由 `VideoStreamSource.frameRates`、`bitratesKbps`、`supportsReconfigure`、`reconfigureFields` 声明。

## 6. 字段 / Schemas

### 6.1 Schema 层级速览

本草案采用复杂 feature 展开模式：方法和事件章节已经列出关键字段，本章集中定义共享对象和 STREAM payload envelope。

```text
VideoStreamCapabilities
  sources: VideoStreamSource[]
VideoStreamSource
  source, displayName, codecs, resolutions, frameRates, state,
  bitratesKbps, encoder, supportsReconfigure, reconfigureFields
VideoOpenStreamParams -> VideoOpenStreamResult
VideoStreamStateChangedEvent -> VideoStreamState
VideoStreamSourceStateChangedEvent -> VideoStreamSourceState
VideoChunkHeaderV1 + H.264 bytes
```

### 6.2 共享对象：`VideoStreamSource`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `source` | string | yes | source id | none | 例如 `wireless_cast_video`。 |
| `displayName` | string | no | maxLength=128 | omitted | 用户可见的 source 名称。 |
| `codecs` | string[] | yes | source-defined codec ids | none | 支持的 video codec 列表。 |
| `resolutions` | string[] | no | source-defined descriptors | omitted | 支持的分辨率描述。 |
| `frameRates` | number[] | no | source-defined values | omitted | source encoder 支持的帧率候选。 |
| `state` | enum | no | `available`, `receiving`, `stopped`, `unavailable` | omitted | source 当前 runtime 状态。 |
| `bitratesKbps` | uint32[] | no | source-defined values | omitted | source encoder 支持的码率候选，单位 kbps。 |
| `encoder` | string | no | maxLength=128 | omitted | source 使用的编码器或编码 profile。 |
| `supportsReconfigure` | bool | no | bool | `false` | 是否支持活动视频编码参数重配置；为 false 时相关请求返回 `NOT_SUPPORTED`。 |
| `reconfigureFields` | string[] | no | `frameRate`, `bitrateKbps` | omitted | 可通过编码重配置变更的 source video 字段。 |

### 6.3 STREAM payload envelope：`VideoChunkHeaderV1`

`VideoChunkHeaderV1` 是 STREAM data 内的视频业务 envelope，位于 STREAM 16B header 之后。二进制字段编号和布局以 contract/registry/generated schema 为准；不得把这些字段加入 STREAM 16B header。

NA20/NT10 MVP 使用 `packetizationMode=completeFrame` 和 `videoPtsMode=sameAsCursor`。该模式下每个 STREAM packet 是完整视频帧，video PTS 等于 STREAM `cursor`，`timebase=1000000`；媒体层不需要 `frameOffset` / `frameLength` 来描述拆包，AXTP transport fragmentation 必须在 STREAM parser 前完成重组。

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `headerLength` | uint16 | yes | bytes | none | envelope 长度。 |
| `flags` | bitmap | yes | `keyFrame`, `config`, `discontinuity` | none | 完整帧标记。 |
| `frameId` | uint32 | yes | uint32 | none | 视频帧 ID。 |
| `sourceCaptureTimestampUs` | uint64 | no | microseconds | STREAM `cursor` | 可省略；默认使用 STREAM `cursor` 表达 NT10 源端采集时间戳。 |
| `payloadBytes` | bytes | yes | H.264 Annex-B / other codec bytes | none | 实际视频数据。 |

### 6.4 State / lifecycle 约束

| 约束 | 说明 |
|---|---|
| accepted 前不得发送 STREAM | open 被接受并返回 `streamId` 前，producer 不得发送该 stream 的 STREAM 数据。 |
| receiver close 不停止 source | `closeStream(reason=receiver_closed/user_stop/not_needed)` 只关闭 downstream stream，不默认停止 `wireless_cast` upstream source。 |
| source stopped 立即终止 stream | NT10 断连或停止视频后，NA20 应关闭 active downstream stream，或发 terminal state。 |
| session lost 清理全部 stream | AXTP session/transport lost 后，旧 `streamId`、open/close response 和 STREAM packet 全部失效。 |
| streamId 不快速复用 | 同一 AXTP session 内不应快速复用媒体 streamId；若复用，需要额外 instance guard。 |

## 7. 交互流程示例 Flow Examples

本章只保留端到端评审需要的关键流。单个 method 的 request / success 形状见第 3 章；公共 envelope、错误响应和 flow 写法见 [Protocol Draft Conventions](../draft-conventions.md)。

### 7.1 查询视频 STREAM 能力和 source 状态

客户端先确认编码、分辨率、方向和 source 状态，再决定是否打开 STREAM。

```json
{
  "id": 701,
  "method": "video.getStreamCapabilities",
  "params": {
    "source": "wireless_cast_video",
    "includeRuntimeState": true
  }
}
```

```json
{
  "id": 702,
  "method": "video.getStreamSourceState",
  "params": {
    "source": "wireless_cast_video"
  }
}
```

### 7.2 打开 H.264 视频 STREAM

`video.openStream` 成功只表示控制面接受建流；实际数据面是否开始传输，以 `video.streamStateChanged` 和 STREAM frame 状态为准。

```json
{
  "id": 703,
  "method": "video.openStream",
  "params": {
    "source": "wireless_cast_video",
    "peerRole": "receiver",
    "codec": "h264",
    "codecFormat": "annexb",
    "stream": "main",
    "streamProfile": "media.video"
  }
}
```

```json
{
  "event": "video.streamStateChanged",
  "intent": 1,
  "data": {
    "streamId": 4097,
    "state": "streaming",
    "source": "wireless_cast_video",
    "codec": "h264"
  }
}
```

### 7.3 请求关键帧和关闭 STREAM

关键帧请求用于弱网恢复或新订阅端追帧；关闭 STREAM 后客户端应停止期待后续 media payload。

```json
{
  "id": 704,
  "method": "video.requestKeyFrame",
  "params": {
    "streamId": 4097,
    "reason": "receiver_reopen"
  }
}
```

```json
{
  "id": 705,
  "method": "video.closeStream",
  "params": {
    "streamId": 4097,
    "reason": "user_stop"
  }
}
```

### 7.4 Nearcast 调整 NT10 编码参数

`cast.setVideoStreamParams` 在 WS 控制面设置 session 级 encoder target；`video.openStream` 显式字段只覆盖当前一次 open。活动投屏的调整是一个完整音视频事务：

```text
Nearcast WS client -> cast.setVideoStreamParams(frameRate, bitrateKbps)
Nearcast -> NA20: video.closeStream(streamId=oldVideo, reason=encodingReconfigure)
Nearcast -> NA20: audio.closeStream(streamId=oldAudio, reason omitted)
NA20 -> Nearcast: both close state=closed (barrier; close order is non-normative)
Nearcast: retire oldVideo/oldAudio/oldSyncGroupId; allocate newSyncGroupId
Nearcast -> NA20: video.openStream(peerRole=transmitter,
                                   original video context,
                                   syncGroupId=newSyncGroupId,
                                   frameRate/bitrateKbps=new)
Nearcast -> NA20: audio.openStream(peerRole=transmitter,
                                   original AAC media/timing context,
                                   syncGroupId=newSyncGroupId)
NA20 -> Nearcast: both open results(streamId=newVideo/newAudio)
Nearcast: confirm NT10 frameRate/bitrateKbps effective
Nearcast -> WS client: cast.flowControlChanged(sourceVideo.state=streaming)
```

若 session 完全没有任何 audio 或 video downstream stream，则视为 idle，省略 close，直接使用新 `syncGroupId` 执行两次 open；若存在单边或其他 partial media state，必须先 cleanup/normalize，不能直接 open。任一 replacement open 失败时先关闭已打开的部分，再使用旧 encoder/audio 语义、全新的 stream IDs 和另一个全新的同步组重开完整音视频对；两侧都恢复才报告 `rolledBack`，否则关闭部分恢复并报告 `failed`。不支持编码参数或活动重配置时返回 `NOT_SUPPORTED`，不得关闭 AXTP/cast session。

## 8. 错误

| 错误 | 适用场景 | 说明 |
|---|---|---|
| `NOT_SUPPORTED` | 设备不支持 `video.stream`、method、codec 或 keyframe request。 | 优先复用通用错误。 |
| `INVALID_ARGUMENT` / `RPC_PARAM_INVALID` | 参数非法、枚举非法、字段组合冲突。 | 应指出具体字段。 |
| `BUSY` | 同 source active stream 冲突或 receiver 暂不可用。 | 可重试；如果是 producer-open 被拒，可后续发 source event。 |
| `RESOURCE_EXHAUSTED` | stream context、decoder、buffer 或带宽不足。 | 不创建 stream。 |
| `STREAM_NOT_FOUND` / `STREAM_CLOSED` | close/get/keyframe 请求使用了无效或已关闭 streamId。 | 调用方本地清理旧 context。 |
| `MEDIA_SOURCE_NOT_FOUND` | source 不存在。 | source id 错误或 capability 过期。 |
| `MEDIA_SOURCE_UNAVAILABLE` | source 当前未 available/receiving。 | 等待 source event 或用户重新插入 NT10。 |
| `MEDIA_CODEC_UNSUPPORTED` | codec 或 H.264 format 不支持。 | 对 `wireless_cast`，H.264 Annex-B 是 MVP。 |
| `MEDIA_FRAMERATE_UNSUPPORTED` | source/profile 不支持请求的编码帧率。 | 读取 `VideoStreamSource.frameRates` 后调整请求。 |
| `MEDIA_BITRATE_UNSUPPORTED` | source/profile 不支持请求的编码码率。 | 读取 `VideoStreamSource.bitratesKbps` 后调整请求。 |
| `MEDIA_STREAM_START_FAILED` / `MEDIA_STREAM_STOP_FAILED` | producer pipeline 绑定或释放失败。 | 可配合 terminal state event。 |
| `VIDEO_RECEIVER_NOT_READY` | 候选业务错误；MediaHost 暂不可接收 NA20 producer-open。 | `[REVIEW-DRAFT]`；采纳前确认是否需要独立 errorCode，或复用 `BUSY` / `UNAVAILABLE`。 |

## 9. Legacy 映射

Legacy 映射是迁移证据，不是 runtime 合同。

| legacy 项 | 候选映射 | 状态 | 说明 |
|---|---|---|---|
| `stream.hidMedia` | `video.stream` / `audio.stream` / `stream.flowControl` | `[REVIEW-DRAFT]` | 历史 HID media 草案应拆分为业务域 stream 和公共流控。 |
| Rooms `StartLiveStream` | `video.openStream` | `[REVIEW-ASK]` | 需确认是 AXTP 内部视频流而非外部直播推流。 |
| Rooms `StopLiveStream` | `video.closeStream` | `[REVIEW-ASK]` | 需确认关闭的是 downstream stream 还是外部服务。 |
| Rooms `LiveStreamStatus` | `video.streamStateChanged` / `video.getStreamState` | `[REVIEW-ASK]` | 需字段级确认。 |
| VM33 `PushStream.Start` / `PushStream.Stop` | `video.openStream` / `video.closeStream` 或后续 `video.pushStream` | `[REVIEW-ASK]` | 若目的地是 RTMP/SRT/公网地址，不应归 `video.stream`。 |
| NA20/NT10 wireless cast | `video.openStream(source=wireless_cast)` + `video.streamSourceStateChanged` | `[REVIEW-DRAFT]` | NT10->NA20 无线协议不进入 AXTP wire。 |

## 10. 测试要点

| 类型 | 要点 |
|---|---|
| happy path | NA20 producer-open accepted 后，Host 接收 H.264 Annex-B STREAM，SPS/PPS 随关键帧。 |
| receiver pull | Host 在 source available/receiving 时主动 `video.openStream(peerRole=transmitter)`，成功创建新 downstream streamId。 |
| event path | producer-open 被拒后，NA20 发送 `video.streamSourceStateChanged(state=receiving)`，Host 后续 pull。 |
| boundary case | 只 video source available；同 source active stream 限制；重复 close；同一 session streamId 不快速复用。 |
| error case | source unavailable、codec unsupported、receiver not ready、resource exhausted、session lost。 |
| compatibility | `stream.open` 不作为视频业务建流入口；legacy PushStream 需确认是否为内部 stream。 |

## 11. 待确认问题

| 问题 | 影响 | 当前建议 | 状态 |
|---|---|---|---|
| `peerRole` 字段是否采用该名称，还是 `peerMediaRole` / `remoteRole`？ | schema / SDK | 保留 `peerRole` 作为草案短名，语义固定为“接收本 request 的对端角色”。 | open |
| `video.streamSourceStateChanged` 是否进入 MVP 必选？ | registry / conformance | 为支持 rejected producer-open 后 Host pull，建议进入 MVP。 | open |
| 同一 source/mediaKind 是否允许多个 active downstream stream？ | product / runtime | NA20/NT10 MVP 建议同一 session 内最多 1 路。 | open |
| receiver close 后 source 保留多久？ | firmware / product | 由设备策略决定，但不得默认停止 upstream source。 | open |
| 是否需要独立 `VIDEO_RECEIVER_NOT_READY` errorCode？ | registry / errors | 初期 JSON 示例复用 `BUSY`，候选错误保留在 details。 | open |

## 附录 A. 协议审核标记

| 标记 | 条目 | 审核结论 | 后续动作 |
|---|---|---|---|
| `[REVIEW-OK]` | domain.feature 边界 | 视频业务流归 `video.stream`；公共 STREAM 留在 `stream.flowControl` / core。 | 采纳时复核命名。 |
| `[REVIEW-OK]` | 不新增 `cast.streaming` | 整体投屏由 video/audio state 聚合，`castSessionId` 只是关联字段。 | 不创建 cast domain。 |
| `[REVIEW-DRAFT]` | 双入口 open | producer-open 和 receiver-pull 共享 `video.openStream`。 | 评审 `peerRole` 字段和 role policy。 |
| `[REVIEW-DRAFT]` | source state event | source available/receiving event 支持 Host 后续拉取。 | 评审是否 MVP。 |
| `[REVIEW-DRAFT]` | legacy 映射 | Rooms / VM33 / AXDP 字段级含义未完全确认；具体 open review 见 legacy 映射表。 | 采纳前补 legacy evidence。 |

## 附录 B. 协议决策记录

| 决策点 | 结论 | 理由 |
|---|---|---|
| 业务建流入口 | 使用 `video.openStream`，不使用常规 `stream.open`。 | 业务 codec/source/sync 属于 video domain；STREAM 只是不透明数据面。 |
| open 发起方 | Identified 后任一端可作为 requester，但 method 方向由 feature role policy 约束。 | 符合 core RPC session 双向 request 语义。 |
| receiver close | 只关闭 downstream stream，不停止 upstream source。 | MediaHost 关闭窗口后可快速重新拉取。 |
| hard-disconnect | 不要求补发 `closeStream`。 | 对端可能已经不可通信，按 session/transport lost 本地清理。 |
