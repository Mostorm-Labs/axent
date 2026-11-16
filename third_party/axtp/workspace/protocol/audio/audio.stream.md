---
status: generated
contract: true
generated: true
domain: audio
feature: audio.stream
registry: ../../../../contract/registry/domains/audio/domain.yaml
lastReviewed: 2026-06-15
---

# audio.stream

## 0. 速读结论

| 项目 | 内容 |
|---|---|
| 这个能力做什么 | 通过 `audio.openStream` / `audio.closeStream` 建立和关闭 AXTP 实时音频业务流，并用 STREAM 数据面承载音频 chunk。 |
| 当前状态 | generated；已写入 `../../../../contract/registry/domains/audio/domain.yaml`，并已刷新到 `contract/protocol/axtp.protocol.yaml` 与 `contract/generated/**`。 |
| 是否可直接实现 | 是，但实现合同以 `contract/protocol/axtp.protocol.yaml` / `contract/generated/**` 为准；本文保留的 open review markers 不属于已生成合同。 |
| 主要交互 | RPC + EVENT + STREAM |
| 是否使用 STREAM | 是。RPC 负责建流、关流和状态；音频数据走 `PayloadType=STREAM`。 |
| Registry readiness | ready；P0 / confirmed subset 已写入 registry source 并生成。 |
| Conformance | needed |
| 主要未决问题 | AAC 透传的 `transportFormat` 仍需确认；source state event、`peerRole` 命名和 retained source 策略需评审。 |

## 1. 功能说明

`audio.stream` 是音频域内的实时媒体流能力。它负责让一个端点把某个音频 source 输出到另一端点，并返回 STREAM 数据面所需的 `streamId`、codec/format、采样率、通道数、profile、时钟和同步元数据。

NA20/NT10 投屏场景中，NT10 插入源端 PC 后自动向 NA20 推送上游 AAC 音频。NA20 只把该上游 source 暴露成 `wireless_cast_audio` source，并通过 `audio.openStream` 建立 `NA20 -> MediaHost` 下游音频 stream。该下游 stream 可以由 NA20 producer 主动请求 Host 接收，也可以由 MediaHost receiver 在 source 仍 `available/receiving` 时主动拉取。

投屏 MVP 确认使用 AAC 透传，不要求 NA20 解码为 PCM 再发送给 MediaHost。AAC 的具体封装 `transportFormat` 是 ADTS、LATM、raw AAC 还是多种都支持，仍需设备/固件确认。

当 `cast.setVideoStreamParams` 调整活动投屏的 NT10 视频编码参数时，audio stream 与 video stream 作为同一事务一起关闭并重开；replacement 使用新的 audio/video `streamId` 和新的共享 `syncGroupId`，而 audio 保持原有 AAC media/timing 语义。

## 2. 能力边界

| 类型 | 内容 |
|---|---|
| 包含 | 查询实时音频 stream capability、source 能力和 source 当前状态。 |
| 包含 | `audio.openStream` 同时支持 producer-initiated open 和 receiver-initiated pull。 |
| 包含 | `audio.closeStream` 可由任一端发起，且只关闭指定 downstream stream。 |
| 包含 | `cast.setVideoStreamParams` 活动重配置中与 video 协调关闭、重开完整音视频 stream 对。 |
| 包含 | 音频 stream lifecycle event、source available/receiving event 和统计 event。 |
| 包含 | NA20/NT10 投屏路径的 AAC 透传音频、时间戳和与视频的同步关联。 |
| 不包含 | 长时间录制、问题定位抓音和文件化录制，归 `audio.recording`。 |
| 不包含 | 设备本地播放任务，归 `audio.playback`；音量、路由、输入、输出、算法/EQ 分别归对应 audio feature。 |
| 不包含 | NT10 到 NA20 的 Wi-Fi 媒体协议、配对、重传和加密。 |
| 数据面 | 使用 AXTP `PayloadType=STREAM` 固定 16B header。STREAM header 不新增 codec、sampleRate、timestamp 或 domain 字段；音频业务元数据放在 `AudioChunkHeaderV1` 或 open/result context 中。 |

## 3. 方法 Methods

已生成 methodId、eventId、bitOffset 和 schema fieldId 以 contract/registry/generated 为准；本文不重新分配正式 ID，保留的 draft/review 标记仅作为后续修订输入。

### 3.0 方法速览

| Method | 调用类型 | 用途 | Params Schema | Result Schema | 是否触发事件 | 状态 |
|---|---|---|---|---|---|---|
| `audio.getStreamCapabilities` | query | 查询实时音频流能力、source、codec/format 和限制。 | `AudioGetStreamCapabilitiesParams` | `AudioStreamCapabilities` | 否 | draft |
| `audio.openStream` | command | 建立一路音频 downstream stream，支持 producer 主动 open 和 receiver 主动 pull。 | `AudioOpenStreamParams` | `AudioOpenStreamResult` | 是，`audio.streamStateChanged` | draft |
| `audio.closeStream` | command | 正常关闭一路音频 stream。 | `AudioCloseStreamParams` | `AudioCloseStreamResult` | 是，`audio.streamStateChanged` | draft |
| `audio.getStreamState` | query | 查询已建立音频 stream 的业务状态。 | `AudioGetStreamStateParams` | `AudioStreamState` | 否 | draft |
| `audio.getStreamSourceState` | query | 查询音频 source 自身的 available/receiving/stopped 状态。 | `AudioGetStreamSourceStateParams` | `AudioStreamSourceState` | 否 | draft |

### 3.1 `audio.getStreamCapabilities`

**用途**：查询设备可打开的音频 source、codec、format、profile、同步字段和双入口 open 支持情况。

| 项 | 内容 |
|---|---|
| 调用类型 | query |
| Params Schema | `AudioGetStreamCapabilitiesParams` |
| Result Schema | `AudioStreamCapabilities` |
| 是否触发事件 | 否 |
| 幂等性 / 异步性 | 幂等；同步返回当前能力快照。 |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_ARGUMENT`, `PERMISSION_DENIED`, `UNAVAILABLE` |

#### 3.1.1 请求参数 Params：`AudioGetStreamCapabilitiesParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `source` | string | no | source id | omitted | 可选按 source 查询；省略表示返回全部可见音频 source。 |
| `includeRuntimeState` | bool | no | `true`, `false` | `false` | 是否同时返回 source 当前 `available/receiving` 状态。 |

#### 3.1.2 返回结果 Result：`AudioStreamCapabilities`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `capability` | string | yes | fixed `audio.stream` | none | capability 名称。 |
| `sources` | `AudioStreamSource[]` | yes | array | none | 可打开的实时音频 source 列表。 |
| `streamProfiles` | string[] | yes | `media.audio` | none | 支持的 stream profile。 |
| `openModes` | string[] | yes | `producer_open`, `receiver_pull` | none | 是否支持生产者主动 open 和接收者主动拉取。 |
| `peerRoles` | string[] | yes | `receiver`, `transmitter` | none | `openStream` / `closeStream` 的对端媒体角色枚举。 |
| `supportsSourceStateEvent` | bool | yes | `true`, `false` | none | 是否支持 `audio.streamSourceStateChanged`。 |
| `supportsSyncGroup` | bool | yes | `true`, `false` | none | 是否支持与视频流共享同步组。 |
| `flowControlManagedByRuntime` | bool | yes | `true`, `false` | `true` | 普通业务 App 是否无需直接调用 `stream.flowControl`。 |

#### 3.1.3 d block 示例

request:

```json
{
  "id": 101,
  "method": "audio.getStreamCapabilities",
  "params": {
    "source": "wireless_cast_audio",
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
    "capability": "audio.stream",
    "sources": [
      {
        "source": "wireless_cast_audio",
        "codecs": [
          "aac"
        ],
        "state": "receiving"
      }
    ],
    "streamProfiles": [
      "media.audio"
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
| `NOT_SUPPORTED` | 设备不支持 `audio.stream`。 | 返回 unsupported feature。 |
| `INVALID_ARGUMENT` | `source` 字段格式非法。 | 返回字段路径和合法 source 约束。 |

### 3.2 `audio.openStream`

**用途**：建立一路实时音频业务 stream，成功后返回 `streamId`，随后 producer 使用 AXTP STREAM 发送音频 chunk。

| 项 | 内容 |
|---|---|
| 调用类型 | command |
| Params Schema | `AudioOpenStreamParams` |
| Result Schema | `AudioOpenStreamResult` |
| 是否触发事件 | 是，进入 `opening` / `streaming` 后触发 `audio.streamStateChanged` |
| 幂等性 / 异步性 | 非幂等；每次成功 open 返回新的 `streamId`。accepted 前 producer 不得发送 STREAM。 |
| 常见错误 | `INVALID_ARGUMENT`, `BUSY`, `RESOURCE_EXHAUSTED`, `MEDIA_SOURCE_NOT_FOUND`, `MEDIA_SOURCE_UNAVAILABLE`, `MEDIA_CODEC_UNSUPPORTED`, `MEDIA_STREAM_START_FAILED` |

`audio.openStream` 支持两种调用方向：

| 场景 | Requester | Responder | `peerRole` | 结果 |
|---|---|---|---|---|
| producer-initiated open | NA20 producer | MediaHost receiver | `receiver` | NA20 请求 MediaHost 接收音频。 |
| receiver-initiated pull | MediaHost receiver | NA20 producer | `transmitter` | MediaHost 请求 NA20 发送音频。 |

两种方式都只建立 `NA20 -> MediaHost` downstream stream，不隐式停止、断开或重建 `NT10 -> NA20` upstream source。

#### 3.2.1 请求参数 Params：`AudioOpenStreamParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `source` | string | yes | source id | none | 音频源。NA20/NT10 投屏为 `wireless_cast_audio`。 |
| `peerRole` | enum | yes | `receiver`, `transmitter` | none | 声明接收本请求的对端应扮演的媒体角色。 |
| `codec` | enum | yes | `aac`, `opus`, `pcm` | none | NA20/NT10 投屏 MVP 使用 `aac`。 |
| `transportFormat` | enum | no | `adts`, `latm`, `raw_aac` | omitted | AAC 透传封装；采纳前确认默认值和支持集合。 |
| `sampleRate` | uint32 | no | capability range | omitted | 采样率，例如 48000。 |
| `channels` | uint8 | no | capability range | omitted | 声道数。 |
| `sampleFormat` | enum | no | `aac`, `pcm_s16le`, `pcm_f32le` | codec default | `codec=aac` 时通常为 `aac`。 |
| `chunkDurationMs` | uint32 | no | capability range | omitted | 建议音频 chunk 时长。 |
| `streamProfile` | string | no | `media.audio` | `media.audio` | STREAM profile。 |
| `cursorUnit` | enum | no | `timestampUs`, `sampleIndex`, `sourceCaptureTimestampUs` | `sourceCaptureTimestampUs` for wireless cast | STREAM 16B header 中 `cursor` 的业务单位；NA20/NT10 投屏使用 NT10 源端采集时间戳。 |
| `syncGroupId` | string | no | product/session scoped | omitted | 与 video stream 绑定的同步组，可由 requester 指定或 responder 返回。 |
| `castSessionId` | string | no | product/session scoped | omitted | 投屏会话关联 ID，不单独创建 cast domain。 |
| `clockDomain` | string | no | `nt10_media_clock`, source-defined | `nt10_media_clock` for wireless cast | 媒体时间戳来源。 |
| `receiverClockDomain` | string | no | `na20_receive_clock`, receiver-defined | omitted | NA20 接收时钟域，用于 jitter/诊断。 |
| `maxDataSize` | uint32 | no | transport/profile limit | omitted | 单个 STREAM payload data 最大大小建议。 |
| `audioPtsMode` | enum | no | `derivedFromSeq`, `explicit` | `derivedFromSeq` for wireless cast | 音频 PTS 来源；固定包模式由 STREAM `seqId` 推导。 |
| `timebase` | uint32 | no | ticks/sec | `48000` for wireless cast | audio PTS 的 timebase。 |
| `samplesPerPacket` | uint32 | no | samples | `1024` for wireless cast | 固定包模式下每个 STREAM packet 消耗的样本数。 |
| `firstMediaSeqId` | uint32 | no | uint32 | `0` | 推导音频 PTS 的首个媒体 seq。 |
| `audioPtsBase` | uint64 | no | ticks | `0` | `firstMediaSeqId` 对应的音频 PTS。 |
| `packetizationMode` | enum | no | `fixedSamplesPerPacket`, `explicit` | `fixedSamplesPerPacket` for wireless cast | 音频 packetization 模式；固定模式不得在 NA20 重打包。 |

#### 3.2.2 返回结果 Result：`AudioOpenStreamResult`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `streamId` | uint32 | yes | `1..0x7FFFFFFF` | none | STREAM 数据面 stream id。 |
| `state` | enum | yes | `opening`, `streaming` | none | 初始业务状态。 |
| `source` | string | yes | source id | none | 实际绑定 source。 |
| `peerRole` | enum | yes | `receiver`, `transmitter` | none | 被请求端确认的对端媒体角色。 |
| `codec` | enum | yes | `aac`, `opus`, `pcm` | none | 实际 codec。 |
| `transportFormat` | enum | no | `adts`, `latm`, `raw_aac` | omitted | AAC 透传封装。 |
| `sampleRate` | uint32 | yes | Hz | none | 实际采样率。 |
| `channels` | uint8 | yes | count | none | 实际声道数。 |
| `sampleFormat` | enum | no | format enum | omitted | 实际采样格式。 |
| `streamProfile` | string | yes | `media.audio` | none | 归一化后的 profile。 |
| `cursorUnit` | enum | yes | `timestampUs`, `sampleIndex`, `sourceCaptureTimestampUs` | none | STREAM `cursor` 单位。 |
| `syncGroupId` | string | no | product/session scoped | omitted | 与视频同步组。 |
| `castSessionId` | string | no | product/session scoped | omitted | 投屏会话关联 ID。 |
| `clockDomain` | string | no | source-defined | omitted | 源媒体时钟域。 |
| `receiverClockDomain` | string | no | receiver-defined | omitted | 接收时钟域。 |
| `maxDataSize` | uint32 | no | negotiated limit | omitted | 每个 STREAM chunk data 最大长度。 |
| `audioPtsMode` | enum | no | `derivedFromSeq`, `explicit` | omitted | 协商后的音频 PTS 来源。 |
| `timebase` | uint32 | no | ticks/sec | omitted | 协商后的 audio PTS timebase。 |
| `samplesPerPacket` | uint32 | no | samples | omitted | 固定包模式下每包样本数。 |
| `firstMediaSeqId` | uint32 | no | uint32 | omitted | PTS 推导起始 seq。 |
| `audioPtsBase` | uint64 | no | ticks | omitted | PTS 推导起始值。 |
| `packetizationMode` | enum | no | `fixedSamplesPerPacket`, `explicit` | omitted | 协商后的 packetization 模式。 |

#### 3.2.3 d block 示例

request:

```json
{
  "id": 102,
  "method": "audio.openStream",
  "params": {
    "source": "wireless_cast_audio",
    "peerRole": "receiver",
    "codec": "aac"
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
    "source": "wireless_cast_audio",
    "peerRole": "receiver",
    "codec": "aac",
    "sampleRate": 48000,
    "channels": 2,
    "streamProfile": "media.audio",
    "cursorUnit": "sourceCaptureTimestampUs",
    "audioPtsMode": "derivedFromSeq",
    "timebase": 48000,
    "samplesPerPacket": 1024,
    "firstMediaSeqId": 0,
    "audioPtsBase": 0,
    "packetizationMode": "fixedSamplesPerPacket"
  }
}
```

#### 3.2.4 可能触发的事件

| Event | 触发条件 | Payload Schema | 客户端处理建议 |
|---|---|---|---|
| `audio.streamStateChanged` | stream 进入 `opening`、`streaming`、`closed` 或 `failed`。 | `AudioStreamStateChangedEvent` | 更新音频 decoder/jitter buffer；必要时调用 `audio.getStreamState` 校准。 |
| `audio.streamSourceStateChanged` | producer-open 被拒后 source 仍可用，或 source lifecycle 变化。 | `AudioStreamSourceStateChangedEvent` | 缓存 source 状态；receiver ready 后可主动 `audio.openStream`。 |

#### 3.2.5 错误

| 错误 | 场景 | 返回建议 |
|---|---|---|
| `MEDIA_SOURCE_NOT_FOUND` | `source` 不存在。 | 不创建 stream。 |
| `MEDIA_SOURCE_UNAVAILABLE` | `source` 存在但未 available/receiving。 | Host 可等待 source event 后重试。 |
| `MEDIA_CODEC_UNSUPPORTED` | 请求 codec、AAC 封装或 PCM fallback 不支持。 | 返回支持的 codec/format 线索。 |
| `BUSY` / `RESOURCE_EXHAUSTED` | 同一 source/mediaKind 已有 active downstream stream 或资源不足。 | 返回可重试提示；不得发送 STREAM。 |
| `MEDIA_STREAM_START_FAILED` | 建立 stream context 或 producer 绑定失败。 | 返回失败原因；可触发 `audio.streamStateChanged(state=failed)`。 |

### 3.3 `audio.closeStream`

**用途**：关闭一路已建立的音频 stream。该方法关闭 downstream stream，不默认停止 upstream source；也用于 `cast.setVideoStreamParams` 协调重建完整音视频对。

| 项 | 内容 |
|---|---|
| 调用类型 | command |
| Params Schema | `AudioCloseStreamParams` |
| Result Schema | `AudioCloseStreamResult` |
| 是否触发事件 | 是，`audio.streamStateChanged(state=closed/failed)` |
| 幂等性 / 异步性 | 幂等；重复 close 或双方同时 close 最终收敛到同一 terminal state。 |
| 常见错误 | `STREAM_NOT_FOUND`, `STREAM_CLOSED`, `INVALID_STATE`, `MEDIA_STREAM_STOP_FAILED` |

#### 3.3.1 请求参数 Params：`AudioCloseStreamParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `streamId` | uint32 | yes | active stream id | none | 要关闭的 audio stream。 |
| `peerRole` | enum | no | `receiver`, `transmitter` | omitted | 被请求端在该 stream 中的媒体角色。Host 关闭 NA20 producer 时可填 `transmitter`。 |
| `reason` | enum | no | `receiver_closed`, `user_stop`, `not_needed`, `source_disconnected`, `producer_stopped`, `session_lost`, `receiver_timeout`, `error` | `user_stop` | 关闭原因。 |
| `finalCursor` | uint64 | no | cursorUnit-defined | omitted | 调用方最后处理到的 cursor。 |

#### 3.3.2 返回结果 Result：`AudioCloseStreamResult`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `streamId` | uint32 | yes | stream id | none | 被关闭的 stream。 |
| `state` | enum | yes | `closing`, `closed`, `failed` | none | close 后状态。 |
| `reason` | enum | no | same as params | omitted | 最终关闭原因。 |
| `alreadyClosed` | bool | no | `true`, `false` | `false` | 是否此前已经进入 terminal state。 |

#### 3.3.3 d block 示例

request:

```json
{
  "id": 103,
  "method": "audio.closeStream",
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
| `audio.streamStateChanged` | stream 关闭、失败或重复 close 收敛。 | `AudioStreamStateChangedEvent` | 释放 decoder、buffer 和音频播放状态。 |

#### 3.3.5 错误

| 错误 | 场景 | 返回建议 |
|---|---|---|
| `STREAM_NOT_FOUND` | streamId 不属于当前 AXTP session。 | 调用方本地清理旧 context。 |
| `STREAM_CLOSED` | stream 已关闭且实现选择返回错误。 | 也可返回 `alreadyClosed=true` 的成功结果。 |
| `MEDIA_STREAM_STOP_FAILED` | 设备无法停止 producer。 | 返回 typed error，并尽快进入 `failed` terminal state。 |

#### 3.3.6 与视频编码重配置协调重启

活动投屏执行 `cast.setVideoStreamParams` 时，audio lifecycle 参与同一个 `pending` -> `closing` -> `opening` -> `streaming` 事务：

- `closing` 阶段对旧 audio `streamId` 调用 `audio.closeStream`，同时对旧 video `streamId` 调用 `video.closeStream(reason=encodingReconfigure)`。audio close 的 `reason` 可以省略；`AudioCloseStreamParams.reason` 不新增 `encodingReconfigure` 或任何其他枚举值。
- 两个 close 的顺序不作规范性要求，但旧 audio/video 都达到 terminal `closed` 是进入 `opening` 的屏障。此后旧 audio/video `streamId` 和旧 `syncGroupId` 全部退役。
- `opening` 阶段分配新的 `syncGroupId`。`audio.openStream` 携带该组并保持旧 audio 的 source、AAC codec/transport、采样、packetization、PTS 和时钟等 media/timing 语义；配对的 `video.openStream` 携带同一组、原 video 上下文以及新的 `frameRate` / `bitrateKbps`。两次 open 都必须返回新的 `streamId`。
- 只有 audio/video 两次 open 都成功且 NT10 新视频编码参数实际生效后，事务才进入 `streaming` / `applied`。cast result 的 `previousStreamId` / `activeStreamId` 仍只表示 video stream ID；audio ID 通过 audio RPC 响应或 `audio.streamStateChanged` 观察。
- 任一 replacement open 失败时，必须关闭已成功打开的部分。回退以旧 encoder/audio 语义重开完整音视频对，但使用全新的 audio/video `streamId` 和另一个全新的 `syncGroupId`；只有两侧都恢复成功才报告 `rolledBack`，否则关闭部分恢复、报告 `failed`，此时没有完整的活动音视频对。
- 仅当 session 完全没有任何 audio 或 video downstream stream 时才视为 idle；此时不执行 close，直接使用一个新的共享 `syncGroupId` 打开完整 audio/video 对。若存在单边或其他 partial media state，必须先 cleanup/normalize，不能按 idle direct-open。

### 3.4 `audio.getStreamState`

**用途**：查询已建立音频 stream 的业务状态、codec/format、source、同步元数据和统计摘要。

| 项 | 内容 |
|---|---|
| 调用类型 | query |
| Params Schema | `AudioGetStreamStateParams` |
| Result Schema | `AudioStreamState` |
| 是否触发事件 | 否 |
| 幂等性 / 异步性 | 幂等；同步返回当前快照。 |
| 常见错误 | `STREAM_NOT_FOUND`, `STREAM_CLOSED` |

#### 3.4.1 请求参数 Params：`AudioGetStreamStateParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `streamId` | uint32 | yes | stream id | none | 查询目标 stream。 |

#### 3.4.2 返回结果 Result：`AudioStreamState`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `streamId` | uint32 | yes | stream id | none | 音频 stream id。 |
| `state` | enum | yes | `opening`, `streaming`, `closing`, `closed`, `failed` | none | 业务流状态。 |
| `source` | string | yes | source id | none | 绑定 source。 |
| `codec` | enum | yes | `aac`, `opus`, `pcm` | none | 当前 codec。 |
| `transportFormat` | enum | no | `adts`, `latm`, `raw_aac` | omitted | AAC 透传封装。 |
| `sampleRate` | uint32 | no | Hz | omitted | 采样率。 |
| `channels` | uint8 | no | count | omitted | 声道数。 |
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
  "method": "audio.getStreamState",
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
    "source": "wireless_cast_audio",
    "codec": "aac",
    "streamProfile": "media.audio"
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

### 3.5 `audio.getStreamSourceState`

**用途**：查询音频 source 自身状态。source state 与 downstream stream state 解耦。

| 项 | 内容 |
|---|---|
| 调用类型 | query |
| Params Schema | `AudioGetStreamSourceStateParams` |
| Result Schema | `AudioStreamSourceState` |
| 是否触发事件 | 否 |
| 幂等性 / 异步性 | 幂等；同步返回当前 source 快照。 |
| 常见错误 | `MEDIA_SOURCE_NOT_FOUND` |

#### 3.5.1 请求参数 Params：`AudioGetStreamSourceStateParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `source` | string | yes | source id | none | 查询目标 source。 |

#### 3.5.2 返回结果 Result：`AudioStreamSourceState`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `source` | string | yes | source id | none | source id。 |
| `mediaKind` | enum | yes | `audio` | `audio` | 媒体类型。 |
| `state` | enum | yes | `idle`, `available`, `receiving`, `stopped`, `failed` | none | source 当前状态。 |
| `codecs` | enum[] | no | `aac`, `opus`, `pcm` | omitted | 当前可用 codec。 |
| `transportFormats` | enum[] | no | `adts`, `latm`, `raw_aac` | omitted | AAC 可用封装。 |
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
  "method": "audio.getStreamSourceState",
  "params": {
    "source": "wireless_cast_audio"
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
    "source": "wireless_cast_audio",
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

## 4. 事件 Events

### 4.0 事件速览

| Event | 触发条件 | Payload Schema | 客户端处理建议 | 状态 |
|---|---|---|---|---|
| `audio.streamStateChanged` | 音频 stream lifecycle 变化。 | `AudioStreamStateChangedEvent` | 创建/释放 decoder、jitter buffer 和播放 pipeline。 | draft |
| `audio.streamSourceStateChanged` | 音频 source lifecycle 变化，或 producer-open 被拒后 source 仍可用。 | `AudioStreamSourceStateChangedEvent` | 更新 source cache；receiver ready 后可主动 pull。 | draft |
| `audio.streamStatsReported` | 周期统计、丢包、背压或诊断上报。 | `AudioStreamStatsReportedEvent` | 更新诊断 UI；不作为音频数据。 | draft |

### 4.1 `audio.streamStateChanged`

**触发条件**：

- `audio.openStream` accepted 后进入 `opening` 或 `streaming`。
- `audio.closeStream`、source stopped、producer failure、receiver timeout 或 session cleanup 导致 terminal state。
- 同一 stream 发生重复 close 或交叉 close，最终状态收敛。

#### Payload：`AudioStreamStateChangedEvent`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `streamId` | uint32 | yes | stream id | none | 变化的 stream。 |
| `state` | enum | yes | `opening`, `streaming`, `closing`, `closed`, `failed` | none | 新状态。 |
| `source` | string | yes | source id | none | 绑定 source。 |
| `codec` | enum | no | `aac`, `opus`, `pcm` | omitted | 当前 codec。 |
| `transportFormat` | enum | no | `adts`, `latm`, `raw_aac` | omitted | AAC 透传封装。 |
| `reason` | string | no | reason enum/string | omitted | 变化原因。 |
| `closeOrigin` | enum | no | `producer`, `receiver`, `session`, `unknown` | `unknown` | terminal state 来源。 |
| `syncGroupId` | string | no | product/session scoped | omitted | 同步组。 |
| `castSessionId` | string | no | product/session scoped | omitted | 投屏会话关联 ID。 |
| `lastSeqId` | uint32 | no | uint32 | omitted | 最近 seq。 |
| `lastCursor` | uint64 | no | cursorUnit-defined | omitted | 最近 cursor。 |

#### d block 示例

```json
{
  "event": "audio.streamStateChanged",
  "intent": 1,
  "data": {
    "streamId": 4097,
    "state": "streaming",
    "source": "wireless_cast_audio",
    "reason": "user_request"
  }
}
```


#### 客户端处理建议

| 场景 | 建议 |
|---|---|
| `opening` / `streaming` | 创建或保持 audio decoder、jitter buffer 和 A/V sync。 |
| `closed` / `failed` | 释放 decoder、buffer 和播放状态。 |
| event 丢失或重连 | 重连后不能复用旧 `streamId`，必须重新 open。 |

### 4.2 `audio.streamSourceStateChanged`

**触发条件**：

- NT10 接入 NA20 后，`wireless_cast_audio` source 进入 `available` 或 `receiving`。
- NA20 producer-open 被 MediaHost 拒绝，但 source 仍 `available/receiving`。
- NT10 拔出、断连或停止发送音频，source 进入 `stopped` / `failed`。
- MediaHost receiver close 后，NA20 保留 upstream source 且 downstream stream 已关闭。

#### Payload：`AudioStreamSourceStateChangedEvent`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `source` | string | yes | source id | none | source id。 |
| `mediaKind` | enum | yes | `audio` | `audio` | 媒体类型。 |
| `state` | enum | yes | `idle`, `available`, `receiving`, `stopped`, `failed` | none | source 状态。 |
| `codecs` | enum[] | no | `aac`, `opus`, `pcm` | omitted | 当前可用 codec。 |
| `transportFormats` | enum[] | no | `adts`, `latm`, `raw_aac` | omitted | AAC 可用封装。 |
| `castSessionId` | string | no | product/session scoped | omitted | 投屏会话关联 ID。 |
| `activeStreamId` | uint32 | no | stream id | omitted | 若已有 active downstream stream，则提供。 |
| `retainable` | bool | no | `true`, `false` | omitted | receiver close 后是否可保留 upstream source。 |
| `reason` | string | no | `nt10_inserted`, `producer_open_rejected`, `receiver_closed`, `source_disconnected`, `timeout`, `unknown` | `unknown` | source 状态原因。 |
| `lastOpenRejectedReason` | string | no | `receiver_not_ready`, `policy_rejected`, `resource_exhausted`, `unsupported`, `unknown` | omitted | producer-open 被拒原因。 |
| `receiverTimestampUs` | uint64 | no | microseconds | omitted | NA20 接收时钟时间。 |

#### d block 示例

```json
{
  "event": "audio.streamSourceStateChanged",
  "intent": 1,
  "data": {
    "source": "wireless_cast_audio",
    "mediaKind": "audio",
    "state": "receiving",
    "reason": "nt10_inserted"
  }
}
```


#### 客户端处理建议

| 场景 | 建议 |
|---|---|
| `available` / `receiving` 且无 active stream | MediaHost 可在 receiver service ready 后调用 `audio.openStream(peerRole=transmitter)` 主动拉取。 |
| producer-open 被拒后收到事件 | 不视为错误终态；等待用户打开窗口或服务恢复后 pull。 |
| `stopped` / `failed` | 清理 source cache；已打开 stream 等待 close/terminal state 或本地释放。 |

### 4.3 `audio.streamStatsReported`

**触发条件**：

- 设备周期上报音频 stream 统计。
- 出现缺包、背压、jitter 异常或诊断采样。

#### Payload：`AudioStreamStatsReportedEvent`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `streamId` | uint32 | yes | stream id | none | 统计目标 stream。 |
| `state` | enum | yes | stream state | none | 当前状态。 |
| `bytesSent` | uint64 | no | bytes | omitted | 已发送字节数。 |
| `chunksSent` | uint64 | no | count | omitted | 已发送 chunk 数。 |
| `droppedChunks` | uint64 | no | count | omitted | 丢弃 chunk 数。 |
| `bufferMs` | uint32 | no | milliseconds | omitted | 接收或发送侧缓冲估计。 |
| `jitterUs` | uint32 | no | microseconds | omitted | jitter 估计。 |
| `lastSeqId` | uint32 | no | uint32 | omitted | 最近 seq。 |
| `lastCursor` | uint64 | no | cursorUnit-defined | omitted | 最近 cursor。 |

#### d block 示例

```json
{
  "event": "audio.streamStatsReported",
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
| 缺包 | 实时音频优先静音补偿或丢弃过期 chunk，不要求重传历史音频。 |

## 5. Capability

Capability name: `audio.stream`。

| 能力字段 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `capability` | string | yes | fixed `audio.stream` | none | capability 名称。 |
| `sources` | `AudioStreamSource[]` | yes | array | none | 可打开 source。 |
| `streamProfiles` | string[] | yes | `media.audio` | none | 支持 profile。 |
| `openModes` | string[] | yes | `producer_open`, `receiver_pull` | none | 支持的建流发起方式。 |
| `peerRoles` | string[] | yes | `receiver`, `transmitter` | none | 对端媒体角色枚举。 |
| `maxConcurrentStreams` | uint32 | no | `1..N` | device-defined | 当前 session 可同时打开的音频 stream 数。 |
| `sameSourceMaxActiveStreams` | uint32 | no | `1..N` | `1` | 同一 source/mediaKind active downstream stream 数。 |
| `supportsSourceStateEvent` | bool | yes | bool | none | 是否支持 source available/receiving event。 |
| `supportsReceiverPull` | bool | yes | bool | none | MediaHost 是否可主动拉取。 |
| `supportsProducerOpen` | bool | yes | bool | none | NA20 是否可主动 open。 |
| `aacTransportFormats` | string[] | no | `adts`, `latm`, `raw_aac` | omitted | AAC 透传封装支持集合；采纳前确认。 |
| `flowControlManagedByRuntime` | bool | yes | bool | `true` | 普通业务 App 是否无需直接调用公共流控。 |

## 6. 字段 / Schemas

### 6.1 Schema 层级速览

本草案采用复杂 feature 展开模式：方法和事件章节已经列出关键字段，本章集中定义共享对象和 STREAM payload envelope。

```text
AudioStreamCapabilities
  sources: AudioStreamSource[]
AudioStreamSource
  sourceId, type, codecs, transportFormats
AudioOpenStreamParams -> AudioOpenStreamResult
AudioStreamStateChangedEvent -> AudioStreamState
AudioStreamSourceStateChangedEvent -> AudioStreamSourceState
AudioChunkHeaderV1 + AAC bytes
```

### 6.2 共享对象：`AudioStreamSource`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `sourceId` | string | yes | source id | none | 例如 `wireless_cast_audio`、`mic_processed`、`line_in`。 |
| `type` | enum | yes | `wireless_cast`, `mic`, `line_in`, `uac`, `mixed`, `playback_tap` | none | source 类型。 |
| `codecs` | enum[] | yes | `aac`, `opus`, `pcm` | none | 支持 codec。 |
| `transportFormats` | enum[] | no | `adts`, `latm`, `raw_aac` | omitted | AAC 封装候选。 |
| `sampleRates` | uint32[] | no | Hz list | omitted | 支持采样率。 |
| `channels` | uint8[] | no | count list | omitted | 支持声道数。 |
| `currentState` | enum | no | `idle`, `available`, `receiving`, `stopped`, `failed` | omitted | 若请求包含 runtime state，可返回当前状态。 |

### 6.3 STREAM payload envelope：`AudioChunkHeaderV1`

`AudioChunkHeaderV1` 是 STREAM data 内的音频业务 envelope，位于 STREAM 16B header 之后。二进制字段编号和布局以 contract/registry/generated schema 为准；不得把这些字段加入 STREAM 16B header。

NA20/NT10 MVP 使用 `packetizationMode=fixedSamplesPerPacket` 和 `audioPtsMode=derivedFromSeq`。该模式下每个 STREAM packet 固定消耗 `samplesPerPacket=1024`，MediaHost 使用 `audioPtsBase + (extendedSeqId - firstMediaSeqId) * samplesPerPacket` 推导 audio PTS；音频 chunk header 不需要每包重复 `mediaPts`、`durationUs` 或 `sampleCount`。如果未来允许变长包、静音抑制、合包/拆包或 NA20 重打包，必须切换到显式 audio media PTS 模式。

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `headerLength` | uint16 | yes | bytes | none | envelope 长度。 |
| `flags` | bitmap | yes | `accessUnitStart`, `accessUnitEnd`, `config`, `discontinuity` | none | chunk 标记。 |
| `sourceCaptureTimestampUs` | uint64 | no | microseconds | STREAM `cursor` | 可省略；默认使用 STREAM `cursor` 表达 NT10 源端采集时间戳。 |
| `payloadBytes` | bytes | yes | AAC access unit / ADTS frame / LATM / raw AAC bytes | none | 实际音频数据。 |

### 6.4 State / lifecycle 约束

| 约束 | 说明 |
|---|---|
| accepted 前不得发送 STREAM | open 被接受并返回 `streamId` 前，producer 不得发送该 stream 的 STREAM 数据。 |
| receiver close 不停止 source | `closeStream(reason=receiver_closed/user_stop/not_needed)` 只关闭 downstream stream，不默认停止 `wireless_cast_audio` upstream source。 |
| source stopped 立即终止 stream | NT10 断连或停止音频后，NA20 应关闭 active downstream stream，或发 terminal state。 |
| session lost 清理全部 stream | AXTP session/transport lost 后，旧 `streamId`、open/close response 和 STREAM packet 全部失效。 |
| streamId 不快速复用 | 同一 AXTP session 内不应快速复用媒体 streamId；若复用，需要额外 instance guard。 |

## 7. 交互流程示例 Flow Examples

本章只保留端到端评审需要的关键流。单个 method 的 request / success 形状见第 3 章；公共 envelope、错误响应和 flow 写法见 [Protocol Draft Conventions](../draft-conventions.md)。

### 7.1 查询音频 STREAM 能力和 source 状态

客户端先查询能力和 source 状态，再决定使用 receiver pull 还是 producer push。

```json
{
  "id": 701,
  "method": "audio.getStreamCapabilities",
  "params": {
    "source": "wireless_cast_audio",
    "includeRuntimeState": true
  }
}
```

```json
{
  "id": 702,
  "method": "audio.getStreamSourceState",
  "params": {
    "source": "wireless_cast_audio"
  }
}
```

### 7.2 打开 AAC 透传音频 STREAM

`audio.openStream` 成功只表示控制面接受建流；实际数据面是否开始传输，以 `audio.streamStateChanged` 和 STREAM frame 状态为准。

```json
{
  "id": 703,
  "method": "audio.openStream",
  "params": {
    "source": "wireless_cast_audio",
    "peerRole": "receiver",
    "codec": "aac",
    "transportFormat": "adts",
    "sampleRate": 48000,
    "channels": 2,
    "streamProfile": "media.audio"
  }
}
```

```json
{
  "event": "audio.streamStateChanged",
  "intent": 1,
  "data": {
    "streamId": 4097,
    "state": "streaming",
    "source": "wireless_cast_audio",
    "codec": "aac",
    "transportFormat": "adts"
  }
}
```

### 7.3 producer-open 被拒后的 fallback

如果接收端暂不可用，producer-open 不创建 `streamId`，producer 不发送 STREAM。source 仍可用时，设备可发送 source event，MediaHost 后续再用 `peerRole=transmitter` 主动拉取。

```json
{
  "id": 704,
  "status": {
    "ok": false,
    "code": 5,
    "msg": "Receiver is not ready.",
    "details": {
      "candidateError": "AUDIO_RECEIVER_NOT_READY",
      "source": "wireless_cast_audio"
    }
  }
}
```

```json
{
  "event": "audio.streamSourceStateChanged",
  "intent": 1,
  "data": {
    "source": "wireless_cast_audio",
    "mediaKind": "audio",
    "state": "receiving",
    "codecs": [
      "aac"
    ],
    "transportFormats": [
      "adts"
    ],
    "reason": "producer_open_rejected",
    "lastOpenRejectedReason": "receiver_not_ready"
  }
}
```

```json
{
  "id": 705,
  "method": "audio.openStream",
  "params": {
    "source": "wireless_cast_audio",
    "peerRole": "transmitter",
    "codec": "aac",
    "transportFormat": "adts",
    "streamProfile": "media.audio"
  }
}
```

## 8. 错误

| 错误 | 适用场景 | 说明 |
|---|---|---|
| `NOT_SUPPORTED` | 设备不支持 `audio.stream`、method、codec 或 format。 | 优先复用通用错误。 |
| `INVALID_ARGUMENT` / `RPC_PARAM_INVALID` | 参数非法、枚举非法、字段组合冲突。 | 应指出具体字段。 |
| `BUSY` | 同 source active stream 冲突或 receiver 暂不可用。 | 可重试；如果是 producer-open 被拒，可后续发 source event。 |
| `RESOURCE_EXHAUSTED` | stream context、decoder、buffer 或带宽不足。 | 不创建 stream。 |
| `STREAM_NOT_FOUND` / `STREAM_CLOSED` | close/get 请求使用了无效或已关闭 streamId。 | 调用方本地清理旧 context。 |
| `MEDIA_SOURCE_NOT_FOUND` | source 不存在。 | source id 错误或 capability 过期。 |
| `MEDIA_SOURCE_UNAVAILABLE` | source 当前未 available/receiving。 | 等待 source event 或用户重新插入 NT10。 |
| `MEDIA_CODEC_UNSUPPORTED` | codec、AAC 封装或 PCM fallback 不支持。 | 对 `wireless_cast_audio`，AAC 透传是 MVP。 |
| `MEDIA_STREAM_START_FAILED` / `MEDIA_STREAM_STOP_FAILED` | producer pipeline 绑定或释放失败。 | 可配合 terminal state event。 |
| `AUDIO_RECEIVER_NOT_READY` | 候选业务错误；MediaHost 暂不可接收 NA20 producer-open。 | `[REVIEW-DRAFT]`；采纳前确认是否需要独立 errorCode，或复用 `BUSY` / `UNAVAILABLE`。 |

## 9. Legacy 映射

Legacy 映射是迁移证据，不是 runtime 合同。

| legacy 项 | 候选映射 | 状态 | 说明 |
|---|---|---|---|
| `stream.hidMedia` | `video.stream` / `audio.stream` / `stream.flowControl` | `[REVIEW-DRAFT]` | 历史 HID media 草案应拆分为业务域 stream 和公共流控。 |
| AXDP / Rooms / VM33 / Signage audio stream 条目 | `audio.stream`、`audio.recording` 或 `audio.playback` | `[REVIEW-ASK]` | 需按业务语义区分实时播放、录制和本地播放。 |
| NA20/NT10 private wireless cast | `audio.openStream(source=wireless_cast_audio)` + `audio.streamSourceStateChanged` | `[REVIEW-DRAFT]` | NT10->NA20 无线协议不进入 AXTP wire。 |

## 10. 测试要点

| 类型 | 要点 |
|---|---|
| happy path | NA20 producer-open accepted 后，Host 接收 AAC passthrough STREAM，并与 video stream 共享 `syncGroupId`。 |
| receiver pull | Host 在 source available/receiving 时主动 `audio.openStream(peerRole=transmitter)`，成功创建新 downstream streamId。 |
| event path | producer-open 被拒后，NA20 发送 `audio.streamSourceStateChanged(state=receiving)`，Host 后续 pull。 |
| boundary case | 只 audio source available；同 source active stream 限制；重复 close；同一 session streamId 不快速复用。 |
| error case | source unavailable、codec/transportFormat unsupported、PCM fallback unsupported、receiver not ready、session lost。 |
| compatibility | `stream.open` 不作为音频业务建流入口；`audio.recording` 不表达投屏播放音频。 |

## 11. 待确认问题

| 问题 | 影响 | 当前建议 | 状态 |
|---|---|---|---|
| AAC 透传的 `transportFormat` 是 ADTS、LATM、raw AAC，还是多种都支持？ | schema / conformance | 保留 `transportFormat` 字段，采纳前固定默认值和枚举。 | `[REVIEW-ASK]` |
| `peerRole` 字段是否采用该名称，还是 `peerMediaRole` / `remoteRole`？ | schema / SDK | 保留 `peerRole` 作为草案短名，语义固定为“接收本 request 的对端角色”。 | open |
| `audio.streamSourceStateChanged` 是否进入 MVP 必选？ | registry / conformance | 为支持 rejected producer-open 后 Host pull，建议进入 MVP。 | open |
| 同一 source/mediaKind 是否允许多个 active downstream stream？ | product / runtime | NA20/NT10 MVP 建议同一 session 内最多 1 路。 | open |
| receiver close 后 source 保留多久？ | firmware / product | 由设备策略决定，但不得默认停止 upstream source。 | open |
| 是否需要独立 `AUDIO_RECEIVER_NOT_READY` errorCode？ | registry / errors | 初期 JSON 示例复用 `BUSY`，候选错误保留在 details。 | open |

## 附录 A. 协议审核标记

| 标记 | 条目 | 审核结论 | 后续动作 |
|---|---|---|---|
| `[REVIEW-OK]` | domain.feature 边界 | 实时音频业务流归 `audio.stream`；公共 STREAM 留在 `stream.flowControl` / core。 | 采纳时复核命名。 |
| `[REVIEW-OK]` | AAC 透传 | NA20/NT10 投屏 MVP 使用 AAC 透传，不要求 PCM fallback。 | 采纳时固定 format 字段。 |
| `[REVIEW-OK]` | 不新增 `cast.streaming` | 整体投屏由 video/audio state 聚合，`castSessionId` 只是关联字段。 | 不创建 cast domain。 |
| `[REVIEW-DRAFT]` | 双入口 open | producer-open 和 receiver-pull 共享 `audio.openStream`。 | 评审 `peerRole` 字段和 role policy。 |
| `[REVIEW-DRAFT]` | source state event | source available/receiving event 支持 Host 后续拉取。 | 评审是否 MVP。 |
| `[REVIEW-DRAFT]` | AAC `transportFormat` | 透传封装未最终确认；具体 open review 见待确认问题。 | 设备/固件确认。 |
| `[REVIEW-DRAFT]` | legacy 映射 | AXDP / Rooms / VM33 / Signage 音频条目未字段级确认；具体 open review 见 legacy 映射表。 | 采纳前补 legacy evidence。 |

## 附录 B. 协议决策记录

| 决策点 | 结论 | 理由 |
|---|---|---|
| 业务建流入口 | 使用 `audio.openStream`，不使用常规 `stream.open`。 | 业务 codec/source/sync 属于 audio domain；STREAM 只是不透明数据面。 |
| open 发起方 | Identified 后任一端可作为 requester，但 method 方向由 feature role policy 约束。 | 符合 core RPC session 双向 request 语义。 |
| receiver close | 只关闭 downstream stream，不停止 upstream source。 | MediaHost 关闭窗口后可快速重新拉取。 |
| hard-disconnect | 不要求补发 `closeStream`。 | 对端可能已经不可通信，按 session/transport lost 本地清理。 |
| PCM fallback | NA20/NT10 MVP 不支持。 | 用户已确认 AAC 透传方案。 |
