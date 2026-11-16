---
status: generated
contract: true
generated: true
domain: stream
feature: stream.flowControl
registry: ../../../../contract/registry/domains/stream/domain.yaml
lastReviewed: 2026-07-08
---

# stream.flowControl

## 0. 速读结论

| 项目 | 内容 |
|---|---|
| 这个能力做什么 | 提供 STREAM 数据面的通用运行期控制、统计、ACK/window、暂停/恢复/中止，以及 `stream.clockReport` 时钟诊断事件。 |
| 当前状态 | generated；已写入 `../../../../contract/registry/domains/stream/domain.yaml`，并已刷新到 `contract/protocol/axtp.protocol.yaml` 与 `contract/generated/**`。 |
| 是否可直接实现 | 是，但实现合同以 `contract/protocol/axtp.protocol.yaml` / `contract/generated/**` 为准；本文保留的说明性边界不替代生成合同。 |
| 主要交互 | RPC + EVENT |
| 是否修改 STREAM 16B header | 否。`streamId`、`seqId`、`cursor` 保持不变。 |
| 是否使用 STREAM | 本 feature 管理已有 STREAM，不定义媒体或文件 payload。 |
| 主要边界 | 不定义具体 profile 建流、codec、payload envelope 或业务状态。 |

## 1. 功能说明

`stream.flowControl` 是 STREAM 数据面的公共运行期 feature。它只处理已经由业务 feature 建立的 stream context，包括查询通用能力、查询 runtime state/stats、发送 ACK、更新接收窗口、暂停/恢复发送，以及中止 stream。

NA20/NT10 投屏场景中，`stream.clockReport` 也放在本 feature 下。它由 NA20 发给 MediaHost，内容来自 NT10 source clock report，并补充 NA20 relay 时间戳。MediaHost 用该事件建立 NT10 源媒体时间线、NT10 单调时钟、NA20 中转时钟和本地接收时钟之间的映射，用于 drift/offset/relay delay 诊断。

## 2. 能力边界

| 类型 | 内容 |
|---|---|
| 包含 | 通用 STREAM runtime capability、state、stats、ACK、window update、pause、resume 和 abort。 |
| 包含 | `stream.clockReport` 诊断事件；latest-wins，丢失单个 report 不阻塞媒体。 |
| 不包含 | 视频、音频、文件、固件等 profile 的 open/close/control 方法。 |
| 不包含 | codec、sampleRate、resolution、AAC/H.264 payload envelope 或业务 source 状态。 |
| 不包含 | 修改 STREAM 16B header；header 仍仅包含 `streamId`、`seqId`、`cursor`。 |

## 3. 方法 Methods

### 3.0 方法速览

| Method | 调用类型 | 用途 | Params Schema | Result Schema | 是否触发事件 | 状态 |
|---|---|---|---|---|---|---|
| `stream.getCapabilities` | query | 查询通用 STREAM flow-control 能力和 clock report 支持。 | `Empty` | `StreamFlowControlCapabilities` | 否 | draft |
| `stream.getState` | query | 查询单个或全部 STREAM runtime 状态。 | `StreamSelector` | `StreamState` | 否 | draft |
| `stream.getStats` | query | 查询单个或全部 STREAM 统计。 | `StreamSelector` | `StreamStats` | 否 | draft |
| `stream.ack` | command | 确认已接收的 STREAM 包范围。 | `StreamAckParams` | `StreamAckResult` | 可触发 `stream.flowControlChanged` | draft |
| `stream.windowUpdate` | command | 更新接收窗口或可发送额度。 | `StreamWindowUpdateParams` | `StreamWindowUpdateResult` | 是，`stream.flowControlChanged` | draft |
| `stream.pause` | command | 暂停指定 STREAM 的发送。 | `StreamPauseParams` | `StreamActionResult` | 是，`stream.flowControlChanged` | draft |
| `stream.resume` | command | 恢复指定 STREAM 的发送。 | `StreamResumeParams` | `StreamActionResult` | 是，`stream.flowControlChanged` | draft |
| `stream.abort` | command | 中止指定 STREAM。 | `StreamAbortParams` | `StreamActionResult` | 是，`stream.stateChanged` | draft |

### 3.1 `stream.getCapabilities`

返回通用 STREAM flow-control 能力。典型字段包括 `supportsAck`、`supportsWindowUpdate`、`supportsPauseResume`、`supportsAbort`、`supportsStats`、`supportsClockReport`、默认 ACK/window 策略和建议 clock report interval。

#### 3.1.1 d block 示例

request:

```json
{"sid":"1234abcd","op":7,"d":{"id":1,"method":"stream.getCapabilities","params":{}}}
```

success:

```json
{"sid":"1234abcd","op":8,"d":{"id":1,"status":{"ok":true,"code":0},"result":{"capability":"stream.flowControl"}}}
```

### 3.2 `stream.getState`

查询 STREAM runtime 状态。`streamId` 省略时可返回当前 session 的聚合状态；指定时返回单个 stream。结果包含 `state`、`paused`、`windowBytes`、`ackedSeqId`、`lastSeqId`、`lastCursor` 等公共字段。

#### 3.2.1 d block 示例

request:

```json
{"sid":"1234abcd","op":7,"d":{"id":2,"method":"stream.getState","params":{}}}
```

success:

```json
{"sid":"1234abcd","op":8,"d":{"id":2,"status":{"ok":true,"code":0},"result":{"state":"aggregate"}}}
```

### 3.3 `stream.getStats`

查询 STREAM 统计。结果包含 `packets`、`bytes`、`droppedPackets`、`seqGaps`、`jitterMs`、`lastSeqId` 和 `lastCursor` 等公共字段。

#### 3.3.1 d block 示例

request:

```json
{"sid":"1234abcd","op":7,"d":{"id":3,"method":"stream.getStats","params":{}}}
```

success:

```json
{"sid":"1234abcd","op":8,"d":{"id":3,"status":{"ok":true,"code":0},"result":{"packets":0,"bytes":0}}}
```

### 3.4 `stream.ack`

接收端确认已接收的 STREAM 包范围，用于释放发送侧窗口。ACK 不改变媒体 PTS，不替代 profile 的业务 close。

#### 3.4.1 d block 示例

request:

```json
{"sid":"1234abcd","op":7,"d":{"id":4,"method":"stream.ack","params":{"streamId":4098,"seqId":48}}}
```

success:

```json
{"sid":"1234abcd","op":8,"d":{"id":4,"status":{"ok":true,"code":0},"result":{}}}
```

### 3.5 `stream.windowUpdate`

接收端通知发送端当前可用窗口。实时媒体实现可用它做背压，但低延迟媒体仍可按 profile 策略丢弃旧数据。

#### 3.5.1 d block 示例

request:

```json
{"sid":"1234abcd","op":7,"d":{"id":5,"method":"stream.windowUpdate","params":{"streamId":4098,"windowBytes":65536}}}
```

success:

```json
{"sid":"1234abcd","op":8,"d":{"id":5,"status":{"ok":true,"code":0},"result":{}}}
```

### 3.6 `stream.pause`

请求暂停指定 STREAM 的数据发送。暂停只影响数据面发送，不隐式关闭业务 stream，也不停止 upstream source。

#### 3.6.1 d block 示例

request:

```json
{"sid":"1234abcd","op":7,"d":{"id":6,"method":"stream.pause","params":{"streamId":4098}}}
```

success:

```json
{"sid":"1234abcd","op":8,"d":{"id":6,"status":{"ok":true,"code":0},"result":{}}}
```

### 3.7 `stream.resume`

恢复已暂停的 STREAM 数据发送。恢复后是否需要关键帧、配置帧或重新同步，由对应业务 profile 处理。

#### 3.7.1 d block 示例

request:

```json
{"sid":"1234abcd","op":7,"d":{"id":7,"method":"stream.resume","params":{"streamId":4098}}}
```

success:

```json
{"sid":"1234abcd","op":8,"d":{"id":7,"status":{"ok":true,"code":0},"result":{}}}
```

### 3.8 `stream.abort`

中止指定 STREAM runtime context。abort 是异常路径，正常业务关闭仍由对应 profile feature 表达。

#### 3.8.1 d block 示例

request:

```json
{"sid":"1234abcd","op":7,"d":{"id":8,"method":"stream.abort","params":{"streamId":4098}}}
```

success:

```json
{"sid":"1234abcd","op":8,"d":{"id":8,"status":{"ok":true,"code":0},"result":{}}}
```

## 4. 事件 Events

### 4.0 事件速览

| Event | 触发条件 | Payload Schema | 客户端处理建议 | 状态 |
|---|---|---|---|---|
| `stream.stateChanged` | 通用 STREAM runtime 状态变化或 abort 收敛。 | `StreamStateChangedEvent` | 更新 runtime cache；必要时调用 `stream.getState` 校准。 | generated draft |
| `stream.statsReported` | 周期统计、丢包、jitter 或诊断采样。 | `StreamStatsReportedEvent` | 更新诊断 UI；不改变播放状态。 | generated draft |
| `stream.flowControlChanged` | ACK/window/pause/resume 改变发送条件。 | `StreamFlowControlChangedEvent` | 更新 window/paused 状态。 | generated draft |
| `stream.clockReport` | NA20 转发或生成源时钟锚点报告。 | `StreamClockReportEvent` | 记录本地 receive monotonic time，更新 drift/offset/relay delay 估计。 | generated draft |

### 4.1 `stream.clockReport`

`stream.clockReport` 是诊断采样事件，不是媒体包，不要求和每个音视频 packet 一一对应。事件丢失时接收端保持上一份映射，下一次 report 到达后继续拟合。

#### Payload：`StreamClockReportEvent`

| 字段名 | 类型 | 必填 | 说明 |
|---|---|---:|---|
| `reportSeq` | uint32 | yes | NT10 source report 序号或 NA20 转发序号。 |
| `syncGroupId` | string | no | 关联 audio/video stream 的同步组。 |
| `sourceDeviceId` | string | no | 源设备标识，例如 NT10。 |
| `sourceClockDomain` | string | no | 源时钟域，例如 `nt10_media_clock`。 |
| `nt10ReportMonotonicUs` | uint64 | yes | NT10 生成 report 时的源端单调时钟采样时间。 |
| `sentAtNt10MonotonicUs` | uint64 | no | NT10 发出 source report 时的源端单调时钟采样时间。 |
| `na20ReceivedAtUs` | uint64 | no | NA20 收到 NT10 report 或对应媒体锚点的本地单调时间。 |
| `na20SentAtUs` | uint64 | no | NA20 发出 AXTP event 的本地单调时间。 |
| `audio` | `StreamClockMediaAnchor` | no | 音频媒体时间线锚点；固定音频模式下 `mediaPts` 等价 sample-count timeline。 |
| `video` | `StreamClockMediaAnchor` | no | 视频媒体时间线锚点；`videoPtsMode=sameAsCursor` 时 `mediaPts` 等于 video cursor。 |
| `discontinuity` | bool | no | 是否紧随媒体或源时钟 discontinuity。 |
| `reason` | enum | no | `periodic`, `streamOpened`, `streamResumed`, `discontinuity`, `sourceReset`, `diagnosticSample`, `unknown`。 |

`StreamClockMediaAnchor` 字段：

| 字段名 | 类型 | 必填 | 说明 |
|---|---|---:|---|
| `streamId` | uint32 | no | 该媒体时间线对应的 STREAM id。 |
| `mediaPts` | uint64 | yes | 该锚点的媒体 PTS。 |
| `timebase` | uint32 | yes | 媒体 PTS timebase，例如 audio 48000，video 1000000。 |
| `anchorNt10MonotonicUs` | uint64 | yes | `mediaPts` 对应的 NT10 源端单调时钟。 |
| `seqId` | uint32 | no | 可选，对应该锚点的 STREAM seqId。 |
| `cursor` | uint64 | no | 可选，对应该锚点的 STREAM cursor。 |

d block 示例：

```json
{
  "event": "stream.clockReport",
  "intent": 1,
  "data": {
    "reportSeq": 42,
    "syncGroupId": "cast-20260708-001",
    "sourceDeviceId": "nt10",
    "sourceClockDomain": "nt10_media_clock",
    "nt10ReportMonotonicUs": 88421024000,
    "na20ReceivedAtUs": 11264012000,
    "na20SentAtUs": 11264013000,
    "audio": {
      "streamId": 4098,
      "mediaPts": 49152,
      "timebase": 48000,
      "anchorNt10MonotonicUs": 88421020000,
      "seqId": 48,
      "cursor": 88421020
    },
    "video": {
      "streamId": 4097,
      "mediaPts": 1024000,
      "timebase": 1000000,
      "anchorNt10MonotonicUs": 88421024000,
      "seqId": 31,
      "cursor": 1024000
    },
    "reason": "periodic"
  }
}
```

## 5. Capability

Capability name: `stream.flowControl`。

| 能力字段 | 类型 | 必填 | 说明 |
|---|---|---:|---|
| `capability` | string | yes | fixed `stream.flowControl`。 |
| `supportsAck` | bool | yes | 是否支持 `stream.ack`。 |
| `supportsWindowUpdate` | bool | yes | 是否支持 `stream.windowUpdate`。 |
| `supportsPauseResume` | bool | yes | 是否支持 `stream.pause` / `stream.resume`。 |
| `supportsAbort` | bool | yes | 是否支持 `stream.abort`。 |
| `supportsStats` | bool | yes | 是否支持 state/stats 查询和统计事件。 |
| `supportsClockReport` | bool | yes | 是否支持 `stream.clockReport`。 |
| `defaultWindowBytes` | uint32 | no | 默认接收窗口。 |
| `clockReportIntervalMs` | uint32 | no | 建议 clock report 周期。 |

## 6. 字段 / Schemas

```text
StreamFlowControlCapabilities
StreamSelector
StreamState / StreamStats
StreamAckParams / StreamAckResult
StreamWindowUpdateParams / StreamWindowUpdateResult
StreamPauseParams / StreamResumeParams / StreamAbortParams / StreamActionResult
StreamStateChangedEvent / StreamStatsReportedEvent / StreamFlowControlChangedEvent / StreamClockReportEvent
```

正式 fieldId 和必填性以 registry/generated 为准。

## 7. 约束

| 约束 | 说明 |
|---|---|
| 不改 STREAM header | `streamId`、`seqId`、`cursor` 的 wire layout 不变。 |
| 不做业务建流 | 业务 stream 由对应 profile 的 open/close 方法建立和释放。 |
| clock report latest-wins | 单个 report 丢失不阻塞媒体；接收端用最近可用 report 做拟合。 |
| 不用本地 now 减设备 cursor 判定超时 | MediaHost 必须结合 arrival interval、seq gap、jitter buffer 和 clock report 估计传输健康。 |

## 8. 测试要点

| 类型 | 要点 |
|---|---|
| boundary | `stream.flowControl` 不包含 profile-specific 方法。 |
| clock report | NA20 周期发出 `stream.clockReport`，MediaHost 记录本地接收时间并更新 drift/offset 估计。 |
| compatibility | 老客户端忽略未知 `stream.clockReport` 不影响媒体 STREAM 数据。 |
| header invariant | STREAM 16B header 仍只有 `streamId`、`seqId`、`cursor`。 |
