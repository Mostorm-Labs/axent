# NA20/NT10 Device Streaming Audio And Video Protocol Interaction Flow

> Status: flow design
> Scope: NA20 receiver USB-HID media bridge, NT10 wireless transmitter, and upper-host MediaHost audio/video playback
> Source inputs: `workspace/business/device-streaming.md`, `workspace/flows/cast-rxtx-pairing.md`, `contract/generated/protocol.md`, `specs/20-core.md`, `workspace/protocol/video/video.stream.md`, `workspace/protocol/audio/audio.stream.md`, `workspace/protocol/stream/stream.flowControl.md`
> Protocol lifecycle: Stage 10 `plan-protocol-flow`

本文根据 NA20/NT10 投屏设备业务需求，梳理上位机 MediaHost、NA20、NT10 和 AXTP 协议之间的音视频投屏交互流程、已有协议覆盖状态和后续协议缺口。

本文不是最终协议事实源。已采纳事实以 `contract/registry/**/*.yaml`、`contract/registry/domains/**/*.yaml`、`contract/protocol/axtp.protocol.yaml` 和 `contract/generated/**` 为准；新增或修改协议必须进入 `workspace/protocol/**` 草案，并经过后续采纳和生成流程。

Flow 文档负责描述业务场景和交互步骤、判断每一步协议覆盖状态、识别协议缺口，并将缺口路由到 candidate `domain.feature`。Flow 文档不负责定义完整 method/event/schema/capability，不分配任何方法、事件、字段编号或错误码，也不能替代 `workspace/protocol/<domain>/<feature>.md`。

## 0. 速读结论

| 项目 | 内容 |
|---|---|
| Flow 目标 | NA20 通过 USB HID 向上位机 MediaHost 暴露 NT10 无线投屏产生的 H.264 视频和 AAC 透传音频；NT10 插入触发的是 NT10->NA20 upstream source ready/receiving；NA20 可据此主动向 MediaHost 发起 `video.openStream` / `audio.openStream` 请求 Host 接收，MediaHost 也可在 source 仍 available/receiving 时主动 `openStream` 拉取；两种方式都只建立 NA20->MediaHost 下游 receiver stream。已建立的 stream 支持 NA20 或 MediaHost 任一方发起 `closeStream`；MediaHost 结束投屏只关闭 NA20->MediaHost 的 AXTP 下行 stream，不默认断开 NT10->NA20 的无线 source；NA20 被拔出、重启或 MediaHost 崩溃时按 AXTP session/transport hard-disconnect 处理，所有 stream context 本地立即失效。 |
| 当前协议覆盖 | partial |
| 涉及 domain.feature | draft `video.stream`, draft `audio.stream`, draft `stream.flowControl`; pairing 前置依赖 draft `network.ap` / `network.wifi` |
| 已有 adopted/generated | `AXTP-USB-HID`, `PayloadType=STREAM`, STREAM 16B header (`streamId`, `seqId`, `cursor`), STREAM common error codes, generated `audio.stream` / `video.stream` timing metadata, generated `stream.flowControl` / `stream.clockReport`。 |
| 缺口 | `video.stream` / `audio.stream` 的 producer-initiated open、receiver-initiated pull open、source available/receiving 事件、bidirectional close、对端媒体角色字段、close 幂等与竞态规则仍需继续评审；receiver close 不停止 upstream source、Host 重新打开窗口后的 retained source reopen 规则、异常硬断流的 reason taxonomy、session-lost 清理规则和重连重建流规则需在草案或 runtime profile 中固化；AAC `transportFormat` 仍需确认；generated 输出仍需与双向 RPC request 语义对齐。 |
| 是否需要新增协议草案 | no new clock feature；当前继续维护 generated `video.stream` / `audio.stream` / `stream.flowControl`，后续只补齐 profile lifecycle 与 source-state 语义。 |
| 是否涉及 Legacy | no，业务文档未提供旧协议命令或字段级证据；NA20/NT10 内部无线投屏实现仅作为设备实现边界。 |
| 是否涉及 STREAM | yes，音频和视频媒体数据必须走 AXTP STREAM 数据面。 |
| 下一步 | 评审并更新 `video.stream` / `audio.stream` 的 lifecycle/source-state 语义；明确 open 可由媒体生产方主动请求接收，也可由媒体消费方主动拉取；close 可由任一端发起，并用对端 receiver/transmitter 角色表达双方职责；同步确认 source available/receiving 事件、AXTP RPC 双向 request 语义、close 竞态规则、session-lost 硬断流清理规则和 AAC transportFormat。 |

## 1. Story Summary

| Item | Content |
|---|---|
| User goal | 用户将 NA20 作为投屏接收端接入上位机 MediaHost，将 NT10 作为投屏发射端插入源端 PC；NT10 开始向 NA20 推流后，上位机能从 NA20 的 USB HID 通道同时接收并播放 H.264 视频和 AAC 透传音频。 |
| Trigger | NA20/NT10 已完成配对，MediaHost 与 NA20 的 AXTP session 已就绪；用户将 NT10 插入源端 PC 后，NT10 自动向 NA20 发起无线投屏，NA20 分别进入 video/audio upstream source ready/receiving；NA20 可主动向 MediaHost 发起对应 `openStream`，MediaHost 也可在 source available/receiving 时主动 `openStream` 拉取。 |
| Success result | 不论由 NA20 producer 主动请求，还是由 MediaHost receiver 主动拉取，最终都只建立 NA20->MediaHost 的下游 receiver stream；可能只收到视频或只收到音频 open，AV MVP 成功仍要求两路都成功建流。 |
| Stop trigger | MediaHost 可能提前停止接收，或用户拔出 NT10、源端 PC 断电、NT10 停止向 NA20 发流；MediaHost 或 NA20 均可发起对应 `closeStream`，最终收敛到同一个 stream terminal state。MediaHost 提前停止接收只表示关闭 Host 接收侧和 NA20->MediaHost 下行 stream，不默认停止 NT10->NA20 的无线 source。若 NA20 被直接拔出、重启、掉电，或 MediaHost 崩溃/进程退出，则走 session/transport hard-disconnect 路径，本地终止所有 stream，不要求 `closeStream` 往返确认。 |
| Primary actors | User, Source PC, NT10 cast transmitter, NA20 AXTP endpoint/media bridge, upper-host MediaHost, MediaHost player |
| Product scope | NA20 receiver AP + USB HID media bridge；NT10 Wi-Fi STA transmitter；上位机 MediaHost/播放器软件。 |

## 2. Source Observations

### 2.1 UI / Prototype

| Screen or control | Observed behavior | Protocol relevance |
|---|---|---|
| USB 插入行为 | 用户将 NA20 插到上位机，作为投屏接收设备。 | MediaHost 需要建立到 NA20 的 `AXTP-USB-HID` 会话，并准备接收 NA20 发起的 stream open/close 请求。 |
| NT10 插入源端 PC | 用户将 NT10 插到源端 PC，作为投屏发射端；该动作更接近 User/NT10 源端侧。 | NT10 接入 NA20 后自动向 NA20 推送无线投屏媒体流；NT10 到 NA20 的内部控制由设备实现决定。 |
| 开始投屏 | NT10 开始向 NA20 发 H.264 + AAC 后，NA20 分别检测 video/audio upstream source ready/receiving。 | NA20 可向 MediaHost 分别发起 `video.openStream` / `audio.openStream` 请求 Host 接收；也可发出分媒体 source available/receiving 事件，让 MediaHost 在准备好时主动 openStream 拉取。 |
| 上位机播放 | NA20 通过 USB HID 将音视频数据给上位机，由 MediaHost/播放器播放。 | 需要 NA20 暴露视频 streamId、音频 streamId、时间戳/同步组和运行状态。 |
| NA20 主动 open 被拒绝 | MediaHost 可能因为窗口未打开、receiver service 未 ready、资源不足或策略限制拒绝 NA20 发起的 open。 | NA20 不发送 STREAM；若 upstream source 仍 available/receiving，可发出分媒体 source available/receiving 事件，提示 MediaHost 后续可主动 `openStream` 拉取。 |
| 上位机提前停止接收 | 用户关闭 MediaHost 播放窗口、切换 source、释放播放器资源，或 MediaHost 因资源不足决定停止一路或两路 stream。 | MediaHost 可向 NA20 发起 `video.closeStream` / `audio.closeStream`；NA20 必须接受该关闭请求并停止对应 streamId 的 STREAM 数据，但不默认停止 NT10->NA20 的无线 source。 |
| 上位机重新打开投屏窗口 | MediaHost 在用户重新打开播放窗口或 receiver service 恢复后，再次表达接收兴趣。 | 若 NA20 仍保留 `wireless_cast` upstream source，MediaHost 可基于该 source 重新建立 NA20->MediaHost 下行 stream；无需重建 NT10->NA20 无线推流。 |
| NT10 拔出或停止发流 | 用户拔出 NT10、源端 PC 不再供电，或 NT10 停止向 NA20 发送无线投屏。 | NA20 主动发起 `video.closeStream` / `audio.closeStream` 或 stream terminal state，并立即关闭 stream；MediaHost 释放播放器资源。 |
| NA20 被拔出/重启/掉电 | 用户直接拔出 NA20，或 NA20 firmware 重启、掉电，MediaHost 不再能通过 USB HID 与 NA20 通信。 | 这是 AXTP session/transport hard-disconnect，不要求 NA20 再发 `closeStream`；MediaHost 本地将该 session 下所有 video/audio stream 标记为 failed/closed 并释放播放器资源。 |
| MediaHost 崩溃/进程退出 | 上位机 receiver 进程崩溃、退出或长时间卡死，NA20 发送 STREAM/RPC 失败或检测到 session timeout。 | 这是 receiver/session lost；NA20 停止向旧 session 发送 STREAM，释放旧 stream context。MediaHost 重启后必须新建 AXTP session；若 source 仍可用，可由 NA20 重新主动 open，也可由 MediaHost 主动 pull。 |
| 业务流程图反馈 | User 的操作更接近 NT10/源端侧，MediaHost 更接近 NA20/上位机侧；插入 NT10 触发上游推流，拔出 NT10 触发上游停止。 | 本文把 upstream source ready/receiving 与 NA20->MediaHost downstream stream open 解耦；open/close 通过对端 receiver/transmitter 角色表达收发职责，不使用抽象 `direction` 字段作为主语义。 |

### 2.2 Requirement Notes

- NA20 是投屏接收端，同时是 Wi-Fi AP 端点和 USB HID 设备。
- NT10 是投屏发射端，作为 Wi-Fi STA 连接 NA20，向 NA20 推送 H.264 + AAC 音视频流。
- 虽然 `workspace/business/device-streaming.md` 第 3 步只显式写到 audio 数据，评审已确认 MediaHost MVP 同时从 NA20 接收视频和音频。
- NA20 不在本流程中本地播放；它作为媒体桥，将收到的音视频经 USB HID 给上位机。
- NA20 到 MediaHost 的视频 H.264 使用 Annex-B，SPS/PPS 随关键帧发送。
- NA20 到 MediaHost 的音频采用 AAC 透传；不采用 NA20 解码 PCM 后再给 MediaHost 作为本场景 MVP。
- 音视频 STREAM 16B `cursor` 统一使用 NT10 源端单调时钟域下的采集时间戳 `sourceCaptureTimestampUs`；不得使用 UTC/wall clock、编码完成时间、发送时间或 NA20 转发时间覆盖该语义。
- 音频 MVP 使用固定 packetization 推导 PTS：`audioPtsMode=derivedFromSeq`、`timebase=48000`、`sampleRate=48000`、`samplesPerPacket=1024`、`firstMediaSeqId=0`、`audioPtsBase=0`、`packetizationMode=fixedSamplesPerPacket`、`cursorUnit=sourceCaptureTimestampUs`。
- 视频 MVP 使用采集时间作为 PTS：`videoPtsMode=sameAsCursor`、`timebase=1000000`、`cursorUnit=sourceCaptureTimestampUs`、`packetizationMode=completeFrame`。
- NT10 需要产生源端 clock report，NA20 可以将其重新封装为 AXTP `stream.clockReport` 发给 MediaHost；MediaHost 使用 report 建立 NT10 媒体时间线、NT10 单调时钟、NA20 中转时钟和本地 monotonic arrival time 的映射。
- MediaHost 不得直接用本地当前时间减去 `cursor` 判断设备是否发包超时；传输健康看本地 arrival interval、seq gap、jitter buffer 和 `stream.clockReport` 拟合后的 drift/offset。
- 主流程由用户插入 NT10 到源端 PC 触发；NT10 自动向 NA20 开始推流，不要求 MediaHost 先下发“开始投屏”命令。
- NA20 检测到 NT10 视频源或音频源 ready/receiving 后，可分别向 MediaHost 发起对应 `openStream`；也可只发出 source available/receiving 事件，可能只有视频或只有音频进入可拉取状态。
- MediaHost 也可以在任一 source 仍 available/receiving 时主动向 NA20 发起 `video.openStream` / `audio.openStream` 拉取该 source 的下游 receiver stream。
- NA20 主动 `openStream` 被 MediaHost 拒绝后，不得发送该 streamId 的 STREAM；若 upstream source 仍 available/receiving，NA20 可以发出分媒体 source available/receiving 事件，通知 MediaHost 后续主动 open。
- `openStream` / `closeStream` 的收发职责不使用 `direction=deviceToHost` 这类抽象方向字段表达，而是通过对端媒体角色表达；NA20 发给 MediaHost 时，对端 MediaHost 是 receiver，MediaHost 发给 NA20 时，对端 NA20 是 transmitter/producer。
- 已建立的 stream 支持双向 close：MediaHost 可因用户停止、资源不足或只想关闭单路媒体而主动 close；NA20 可因 NT10 断开或 source 停止而主动 close。
- MediaHost 发起的 receiver close 只关闭 Host 接收侧和 NA20->MediaHost 的 AXTP 下行 stream；它不等价于 `video.stopStreamSource` / `audio.stopStreamSource`，也不默认要求 NA20 断开或重建 NT10->NA20 的无线 source。
- MediaHost 关闭窗口后，NA20 可以继续接收 NT10 上游媒体，并按设备资源策略丢弃、保留最新关键帧/音频时间基准或保持轻量缓存；再次打开窗口时优先复用仍活跃的 upstream source 重建下行 stream。
- close 必须幂等并以 terminal state 收敛；如果双方同时 close，同一个 streamId 只能进入一次 `closed` / `failed` 终态，重复 close 返回 already closed 或等价 closed 结果。
- NT10 拔出或停止发流后，NA20 可以发起对应 `closeStream`，也可以将 stopped 合并进对应 stream 的 `failed` / `closed` 状态事件，并立即关闭对应 streamId；MediaHost 释放播放器资源。
- NA20 被直接拔出、重启、掉电，或 MediaHost 崩溃/进程退出时，对端通常已经不可通信；这类异常硬断流不要求补发 `closeStream`，由仍存活的一端根据 AXTP session/transport lost 本地终止所有 stream。
- stream 生命周期绑定 AXTP session；session lost 后，该 session 下所有 `streamId` 和 stream context 立即失效，重连后必须重新建 session 并重新 `openStream`。
- MediaHost 卡死但 USB 连接未立即断开时，NA20 可通过 heartbeat、发送超时、流控窗口长期不前进或 STREAM 写失败判定 receiver timeout，并停止向旧 session 发送媒体。
- 为避免旧 close 或旧 STREAM 数据影响新建流，媒体 streamId 在同一 AXTP session 内不应快速复用；若必须复用，需要额外 stream instance 标识或 castSessionId 参与校验。
- NT10 开始/停止无线推流可做成可选 AXTP source proxy control，但不作为主流程或 MVP 前置条件。

### 2.3 Device / System State Observations

| State | Meaning | Protocol relevance |
|---|---|---|
| pairing completed | NA20/NT10 已完成 AP/Wi-Fi 配对。 | 前置条件；详见 `workspace/flows/cast-rxtx-pairing.md`。 |
| NA20 AXTP session ready | MediaHost 与 NA20 已建立 Standard Framed AXTP session。 | generated；RPC + STREAM 前置条件。 |
| wireless_cast upstream source available/receiving | NA20 已检测到 NT10 无线投屏源，且可能正在接收上游媒体。 | draft query/event；source state 可驱动 NA20 producer-open、MediaHost receiver-pull，以及 NA20 open 被拒后的 fallback notification。 |
| video source available/receiving | NA20 已收到 NT10 的 H.264 视频输入，具备建立 NA20->MediaHost 下游视频 STREAM 的条件。 | draft amendment；NA20 可发起 `video.openStream(peerRole=receiver)`，也可发出视频 source available/receiving 事件；MediaHost 可在该状态下 `video.openStream(peerRole=transmitter)` 主动拉取。 |
| audio source available/receiving | NA20 已收到 NT10 的 AAC 音频输入，具备建立 NA20->MediaHost 下游音频 STREAM 的条件。 | draft amendment；NA20 可发起 `audio.openStream(peerRole=receiver)`，也可发出音频 source available/receiving 事件；MediaHost 可在该状态下 `audio.openStream(peerRole=transmitter)` 主动拉取。 |
| producer open rejected, source retained | NA20 主动 `openStream` 被 MediaHost 拒绝，但上游 source 仍 available/receiving。 | draft amendment；NA20 不发送 STREAM，并可发出对应 mediaKind 的 source available/receiving 事件，让 MediaHost 后续主动 pull。 |
| video downstream opening / streaming / closed | NA20->MediaHost 视频下游 stream 正在准备、发送或停止。 | draft query/event；`video.openStream`, `video.closeStream`, `video.streamStateChanged`。 |
| audio downstream opening / streaming / closed | NA20->MediaHost 音频下游 stream 正在准备、发送或停止。 | draft query/event；`audio.openStream`, `audio.closeStream`, `audio.streamStateChanged`。 |
| sync group ready | 视频和音频使用同一 `syncGroupId` / `castSessionId`。 | draft metadata；MediaHost 绑定播放器时间线。 |
| video seq gap | MediaHost 检测到视频 STREAM seq gap 或解码失败。 | draft action；`video.requestKeyFrame`。 |
| audio seq gap | MediaHost 检测到音频缺包。 | local-only playback handling；实时音频优先静音补偿或丢弃旧 chunk。 |
| video/audio source stopped | NT10 断开、拔出或无线投屏停止，NA20 不再收到上游媒体。 | draft；可合并到 `video.streamStateChanged` / `audio.streamStateChanged` 的 `failed` 或 `closed`，NA20 立即关闭 stream。 |
| receiver requested close | MediaHost 主动停止接收某一路或全部媒体。 | draft amendment；MediaHost 发起 `video.closeStream` / `audio.closeStream`，NA20 停止对应 AXTP downstream producer 并关闭 stream；NT10->NA20 upstream source 保持 active，除非另有 source stop/control。 |
| receiver inactive, source retained | MediaHost 关闭播放窗口或暂无接收兴趣，但 NT10 仍在向 NA20 推流。 | runtime / draft lifecycle；NA20 不向 Host 发送 STREAM，可保留 upstream source 和必要最新媒体状态，等待 Host 重新打开窗口后重建下行 stream。 |
| simultaneous close | MediaHost 和 NA20 几乎同时关闭同一 stream。 | draft amendment；close 幂等，终态单调，重复 close 收敛到已关闭状态。 |
| AXTP session/transport lost | NA20 被拔出、重启、掉电，或 USB HID / Standard Framed session 断开。 | generated + runtime；不经业务 `closeStream`，仍存活的一端本地将该 session 下全部 stream 置为 terminal state。 |
| receiver lost / timeout | MediaHost 崩溃、退出、卡死或长时间不消费 STREAM。 | runtime / draft policy；NA20 停止向旧 session 发送数据，释放 stream context，可保留 NT10 上游输入直到新 Host session 建立或设备策略停止上游。 |

### 2.4 Receiver Phase Projection

HID/NA20 不新增独立 `cast.streaming` method domain，但 MediaHost/Launcher 可以把 source state、stream state 和播放器状态聚合为统一 `receiverPhase`，与 AirPlay / UxPlay 的接收端阶段对齐。

| receiverPhase | HID/NA20 观测条件 | 说明 |
|---|---|---|
| `idle` | 没有 active upstream source，也没有 active downstream stream。 | NA20 可用但未在接收投屏。 |
| `incoming` | NT10 插入后，NA20 检测到 `wireless_cast` source `available` / `receiving`。 | 表示收到投屏意图或源已经出现。 |
| `authenticating` | 通常不出现。 | HID/NA20 主流程无用户 PIN；可在聚合摘要中表达 `authRequired=false`。 |
| `streamStarting` | `video.openStream` / `audio.openStream` 已 accepted，任一路处于 `opening`。 | 已建下游 stream context，但还不能判定用户可见播放。 |
| `streaming` | video/audio stream 进入 `streaming`，并开始收到对应 STREAM chunk。 | 表示数据面流动；不等价于播放器已经显示画面。 |
| `rendering` | MediaHost player 完成首帧渲染或开始音频播放。 | AV MVP 可要求视频和音频都 ready 后再展示整体成功。 |
| `interrupted` | receiver inactive 但 source retained，或短暂缺包/背压/receiver timeout 仍可恢复。 | 与 source stop 区分；可恢复时不应直接进入终态。 |
| `stopping` | MediaHost 或 NA20 发起 `closeStream`，或 source stopped 正在关闭 active stream。 | 用于展示正在收敛，不复用旧 streamId。 |
| `ended` | user stop、receiver close 或 source stop 后所有相关 stream 正常关闭。 | 回到等待新 source / open 的稳定状态。 |
| `failed` | open failed、source failed、session lost、hard unplug 或不可恢复媒体错误。 | 硬断流不等待业务 `closeStream`。 |

### 2.5 Actors And Boundaries

| Actor / boundary | Responsibility | Protocol boundary |
|---|---|---|
| User | 插入 NA20、插入/拔出 NT10，触发源端投屏开始或停止。 | 人工动作；不是 AXTP method。 |
| Source PC | 为 NT10 供电并提供源端视频/音频输入环境。 | non-protocol；不直接参与 AXTP wire。 |
| NT10 cast transmitter | 插入源端 PC 后接入 NA20，并自动无线推流 H.264 + AAC；拔出后停止发流。 | NT10 到 NA20 的无线协议不进入 AXTP wire。 |
| NA20 AXTP endpoint / media bridge | 接收 NT10 无线投屏，维护 video/audio source available/receiving 状态；可主动发起 `openStream` 请求 MediaHost 接收，也可在 open 被拒或策略要求时发出 source state 事件；接受 MediaHost 基于 available source 的主动 `openStream` 拉取；已建立下游 stream 后发送 STREAM 数据面；MediaHost 关闭接收时停止 Host 下行 stream 但可保持 NT10 upstream source；source 停止或 session lost 时关闭/清理对应 context。 | AXTP device endpoint；device-initiated business RPC 需要与 core RPC 方向语义对齐，同时 feature role policy 要允许 Host receiver-pull。 |
| Upper-host MediaHost | 发现 NA20、建立 AXTP session、接收并可拒绝 NA20 发起的分媒体 `openStream`；缓存 source available/receiving 状态，并在窗口打开或 receiver service ready 时主动 `openStream` 拉取；按对端媒体角色准备 receiver pipeline，也可主动发起 `closeStream` 停止接收；NA20/session lost 时本地释放 decoder、buffer 和 stream context。 | AXTP host endpoint；既可能作为 openStream responder，也可能作为 receiver-pull requester。 |
| MediaHost player | 解码 H.264/AAC、jitter buffer、A/V sync、渲染和播放。 | local-only；使用 stream metadata，不定义 AXTP method。 |

## 3. Assumptions And Non-Goals

| Type | Item | Status |
|---|---|---|
| Assumption | NA20/NT10 配对流程已经完成，NT10 已可连接 NA20 AP；配对详见 `workspace/flows/cast-rxtx-pairing.md`。 | `[REVIEW-DRAFT]` |
| Assumption | MediaHost 至少与 NA20 建立一条 AXTP-over-USB-HID 会话；主流程不要求 MediaHost 直接控制 NT10 开始推流。 | `[REVIEW-DRAFT]` |
| Assumption | NA20 将音频和视频拆成两个独立 `streamId`，通过同一 AXTP session 多路复用。 | `[REVIEW-DRAFT]` |
| Assumption | MediaHost 能接收 NA20 发起的业务 RPC request，或协议层补充 Host-side receiver service 语义；core spec 已按双向 requester 语义澄清后，generated 输出仍需后续同步。 | `[REVIEW-DRAFT]` |
| Decision | upstream source lifecycle 与 downstream stream lifecycle 解耦：NT10 插入只让 NA20 的 source 进入 available/receiving；NA20 producer-open 与 MediaHost receiver-pull 都只是建立 NA20->MediaHost 下游 stream，不隐式停止或重建 NT10->NA20 upstream source。 | `[REVIEW-OK]` |
| Decision | source available/receiving 可通过分媒体 source state query/event 表达；NA20 主动 `openStream` 被拒后，若 source 仍 available/receiving，可再发 source available/receiving event 提醒 MediaHost 后续主动拉取。 | `[REVIEW-OK]` |
| Decision | `openStream` / `closeStream` 使用对端媒体角色表达职责，例如 NA20 发给 MediaHost 时声明对端是 receiver；不把 `direction` 作为主语义字段。 | `[REVIEW-OK]` |
| Decision | 已建立的 video/audio stream 支持双向 close；MediaHost 可提前关闭接收，NA20 也可因 source 停止关闭。 | `[REVIEW-OK]` |
| Decision | MediaHost 主动 `closeStream` 只关闭 Host 接收侧和 NA20->MediaHost 下行 stream，不默认停止或重建 NT10->NA20 upstream source；Host 重新打开窗口时可基于 NA20 保留的 source 重新建下行 stream。 | `[REVIEW-OK]` |
| Decision | `closeStream` 必须幂等；双方同时 close 或重复 close 时，结果必须收敛到同一个 terminal state。 | `[REVIEW-OK]` |
| Decision | NT10 停止发流后的 stopped 语义可以与对应 `video.streamStateChanged` / `audio.streamStateChanged` 的 `failed` 或 `closed` 终态合并。 | `[REVIEW-OK]` |
| Decision | NT10 断连后，NA20 不保留 streamId 等待快速重连，应立即关闭相关 video/audio stream；重连后重新走 ready/open 流程。 | `[REVIEW-OK]` |
| Decision | NA20 被拔出、重启、掉电，或 MediaHost 崩溃/退出时，不要求补发业务 `closeStream`；按 AXTP session/transport hard-disconnect 处理，仍存活的一端本地终止该 session 下所有 stream。 | `[REVIEW-OK]` |
| Decision | session lost 后旧 `streamId`、旧 stream context 和未完成 close/open response 全部作废；重连恢复必须新建 AXTP session 并重新 open audio/video stream。 | `[REVIEW-OK]` |
| Decision | `openStream` 是新 session 上的建流请求，不是旧 stream 恢复；被请求端可拒绝，producer accepted 前不得发送 STREAM；任一端 `closeStream` 可取消 opening/streaming；同一 session 内同一 source/mediaKind 只能有一个 active downstream stream instance。 | `[REVIEW-OK]` |
| Decision | 运行时可把 HID/NA20 source state、stream state 和播放器首帧聚合为 `receiverPhase`；该聚合只服务 UI/重连校准，不新增独立 AXTP method domain。 | `[REVIEW-OK]` |
| Assumption | 为避免旧 close/STREAM 影响新流，同一 AXTP session 内媒体 streamId 不应快速复用；若复用必须有 stream instance 或 castSessionId 校验。 | `[REVIEW-DRAFT]` |
| Assumption | 实时投屏低延迟优先，视频丢包优先请求关键帧，音频丢包优先丢弃过旧 chunk，不默认重传历史媒体数据。 | `[REVIEW-DRAFT]` |
| Decision | MediaHost MVP 同时从 NA20 接收视频和音频；不按 audio-only 范围设计。 | `[REVIEW-OK]` |
| Decision | NA20 到 MediaHost 的音频格式采用 AAC 透传方案；AAC 具体封装如 ADTS/LATM/raw AAC 由 `audio.stream` 草案继续确认。 | `[REVIEW-OK]` |
| Decision | NA20 到 MediaHost 的视频 H.264 使用 Annex-B，SPS/PPS 随关键帧发送。 | `[REVIEW-OK]` |
| Decision | STREAM `cursor` 使用 NT10 源端 `sourceCaptureTimestampUs`；NA20 可记录接收/转发时钟用于中转诊断，但不得覆盖源端 cursor。 | `[REVIEW-OK]` |
| Decision | 音频 PTS 在 MVP 中由 `seqId` 按固定 1024 samples 推导，不在每个 AAC STREAM data 前重复携带 `mediaPts`；如果未来允许变长包、静音抑制、合包/拆包或 NA20 重打包，必须切换到 explicit audio mediaPts。 | `[REVIEW-OK]` |
| Decision | 视频 PTS 在 MVP 中等同于 `cursor`，`videoPtsMode=sameAsCursor` 且 `timebase=1000000`；每个 STREAM 包承载完整视频帧，媒体层不再重复定义 frame offset。 | `[REVIEW-OK]` |
| Decision | 播放同步主时间轴使用 NT10 源媒体时间线；NA20 接收/转发时钟用于 relay delay、queue 和 jitter 诊断；MediaHost 通过 `stream.clockReport` 做长期 drift/offset 估计。 | `[REVIEW-OK]` |
| Decision | 当前 MVP 的时钟校准入口为 `stream.flowControl` 下的 `stream.clockReport`，作为通用 STREAM runtime 诊断事件，latest-wins，不阻塞媒体数据。 | `[REVIEW-OK]` |
| Decision | NT10 开始/停止无线推流可以做成可选 AXTP source proxy control；NA20 与 NT10 之间实现看设备，主流程仍由 NT10 插入/拔出触发。 | `[REVIEW-OK]` |
| Non-goal | 不设计 NT10 到 NA20 的私有 Wi-Fi 媒体协议、无线丢包恢复或 Wi-Fi 加密细节。 | `[REVIEW-OK]` |
| Non-goal | 不设计设备升级、AP/Wi-Fi 配对、UI 文案或上位机播放器内部渲染实现。 | `[REVIEW-OK]` |
| Non-goal | 不把 H.264/AAC 大数据塞进普通 RPC response；媒体数据必须走 STREAM 数据面。 | `[REVIEW-OK]` |
| Non-goal | 不新增独立 `cast.streaming` method domain；整体投屏阶段由 MediaHost/Launcher 聚合为 `receiverPhase`。 | `[REVIEW-OK]` |

## 4. Protocol Coverage

| Need | Coverage state | AXTP protocol | Evidence | Next action |
|---|---|---|---|---|
| MediaHost 与 NA20 建立 USB HID 会话 | generated | `AXTP-USB-HID`, Standard Framed transport | `contract/generated/protocol.md`, `contract/registry/core/protocol_meta.yaml` | 可按 generated/core 实现。 |
| 通过 USB HID 承载连续音视频数据 | generated | `PayloadType=STREAM`, 16B header: `streamId`, `seqId`, `cursor` | `contract/generated/protocol.md`, `specs/20-core.md` | 可作为数据面基础。 |
| STREAM 基础错误和缺包观测 | generated | `STREAM_NOT_FOUND`, `STREAM_SEQ_INVALID`, `STREAM_CHUNK_MISSING`, `STREAM_OFFSET_INVALID` | `contract/generated/protocol.md` | 可复用 generated errors。 |
| AXTP session/transport lost 后清理 stream context | generated + local-only | AXTP transport/session lifecycle + MediaHost/NA20 runtime cleanup | `contract/generated/protocol.md`, `specs/20-core.md` | session lost 时不要求业务 `closeStream`；所有旧 stream context 本地失效，重连后重新 open。 |
| NA20 producer 主动请求 Host 接收 H.264 视频 | draft amendment | device-initiated `video.openStream` with peer media role receiver | `workspace/protocol/video/video.stream.md` | Stage 20/30 需补充 openStream 发起方可为 producer，并声明对端 MediaHost 是 receiver。 |
| NA20 producer 主动请求 Host 接收 AAC 音频 | draft amendment | device-initiated `audio.openStream` with peer media role receiver | `workspace/protocol/audio/audio.stream.md` | Stage 20/30 需补充 openStream 发起方可为 producer，并声明对端 MediaHost 是 receiver。 |
| MediaHost receiver 主动拉取 available/receiving source | draft amendment | host-initiated `video.openStream` / `audio.openStream` with peer media role transmitter | `workspace/protocol/video/video.stream.md`, `workspace/protocol/audio/audio.stream.md` | Stage 20/30 需补充 openStream 也允许 receiver 发起；请求声明对端 NA20 是 transmitter/producer，成功后仍只建立 NA20->MediaHost downstream stream。 |
| NA20 主动 open 被拒后的 source available/receiving 通知 | draft amendment | per-media source state event/query, e.g. video source event and audio equivalent | `workspace/protocol/video/video.stream.md`, `workspace/protocol/audio/audio.stream.md` | Stage 20/30 需明确 rejected open 后不得发送 STREAM；若 source 仍 available/receiving，可发事件提示 Host 后续主动 pull。 |
| 任一端关闭已建立 stream | draft amendment | bidirectional `video.closeStream` / `audio.closeStream`, or terminal state event | `workspace/protocol/video/video.stream.md`, `workspace/protocol/audio/audio.stream.md` | MediaHost 可提前关闭，NA20 可因 source stopped 关闭；close 必须幂等并收敛到 terminal state。 |
| Host 关闭接收后保留 NT10 upstream source | draft amendment / runtime policy | receiver inactive state, retained source reopen, host-initiated `openStream` pull or NA20 re-offer | `workspace/protocol/video/video.stream.md`, `workspace/protocol/audio/audio.stream.md` | Stage 20/30 需明确 receiver close 不等价于 source stop；重新打开窗口时可复用 active upstream source 建立新的 downstream stream。 |
| AXTP RPC request 方向支持 NA20 主动调用 MediaHost | partial / generated sync gap | bidirectional business RPC request or Host-side receiver service | `specs/20-core.md`, `contract/generated/protocol.md` | Core spec 已澄清 Identified 后双方可作为 requester；generated 输出和协议草案仍需同步 requester/responder 与 role policy。 |
| 查询或订阅 NA20 是否有投屏输入源和媒体桥能力 | draft | `video.stream` source `wireless_cast`, `audio.stream` source `wireless_cast_audio`, optional source state event | `workspace/protocol/video/video.stream.md`, `workspace/protocol/audio/audio.stream.md` | 可作为诊断/预检能力，也可作为 MediaHost 主动拉取和 rejected producer-open fallback 的状态依据。 |
| 打开 H.264 视频到 MediaHost | draft | `video.getStreamCapabilities`, `video.openStream`, `video.closeStream`, `video.streamStateChanged` | `workspace/protocol/video/video.stream.md` | 采纳 `video.stream`，固化 `wireless_cast` source、对端 receiver 角色和同步字段。 |
| 视频完整帧 envelope、minimal flags 和丢包重同步 | draft amendment | minimal video payload flags such as keyFrame/config/discontinuity, `video.requestKeyFrame`, `media.video` profile | `workspace/protocol/video/video.stream.md` | `packetizationMode=completeFrame` 下不增加媒体层 frame offset；H.264 Annex-B、SPS/PPS 随关键帧需采纳固化。 |
| 打开 AAC 透传音频到 MediaHost | draft | `audio.getStreamCapabilities`, `audio.openStream`, `audio.closeStream`, `audio.streamStateChanged` | `workspace/protocol/audio/audio.stream.md` | 采纳前确认 AAC `transportFormat` 和对端 receiver 角色。 |
| 音频 AAC packet envelope、minimal flags 和丢包统计 | draft amendment | minimal audio payload flags such as config/discontinuity, `media.audio` / `realtime_audio` profile | `workspace/protocol/audio/audio.stream.md`, `workspace/protocol/stream/stream.flowControl.md` | `fixedSamplesPerPacket` 下不在每包重复 `mediaPts` / duration / sampleCount；变长、静音抑制或重打包时再切 explicit audio `mediaPts`。 |
| 音频固定包 PTS 推导参数 | generated draft | `audio.openStream` / `AudioOpenStreamResult` fields: `audioPtsMode=derivedFromSeq`, `timebase=48000`, `sampleRate=48000`, `samplesPerPacket=1024`, `firstMediaSeqId=0`, `audioPtsBase=0`, `packetizationMode=fixedSamplesPerPacket`, `cursorUnit=sourceCaptureTimestampUs` | `contract/generated/protocol.md`, `workspace/protocol/audio/audio.stream.md` | 已固化为 stream context metadata；该模式下每个音频 STREAM packet 消耗固定 1024 samples，NA20 不得重打包。 |
| 视频 PTS 与 cursor 同源参数 | generated draft | `video.openStream` / `VideoOpenStreamResult` fields: `videoPtsMode=sameAsCursor`, `timebase=1000000`, `cursorUnit=sourceCaptureTimestampUs`, `packetizationMode=completeFrame` | `contract/generated/protocol.md`, `workspace/protocol/video/video.stream.md` | 已固化为 stream context metadata；该模式下视频 media PTS 等同于 STREAM `cursor`，且每个 STREAM packet 是完整视频帧。 |
| 音视频同步和时钟校准 | generated draft | `stream.clockReport` event carrying NT10 media PTS anchors plus NA20 relay timestamps | `contract/generated/protocol.md`, `workspace/protocol/stream/stream.flowControl.md` | 当前 MVP 将 clock report 放在 `stream.flowControl`；它是 runtime 诊断事件，不改变 STREAM 16B header，也不替代 ACK/window/pause/resume。 |
| MediaHost 本地解码、jitter buffer、A/V sync 和播放 | local-only | MediaHost player/runtime | business implementation | 不进入协议；测试验收需覆盖。 |
| NT10 到 NA20 无线推流 | non-protocol | device-specific wireless cast | `workspace/business/device-streaming.md` | 不进入 AXTP wire。 |

Coverage 取值：

| Coverage | Meaning |
|---|---|
| generated | 已进入 `contract/generated/**` 或 protocol IR，可作为实现合同视图。 |
| adopted | 已写入 registry YAML，但当前 flow 未直接引用 generated 输出。 |
| draft | 已有 `workspace/protocol/**` 草案，但尚未 adopted/generated。 |
| missing | 没有合适的 adopted/generated/draft 协议覆盖。 |
| local-only | App/UI/runtime 本地逻辑，不需要 AXTP 协议。 |
| non-protocol | 产品规则、人工流程、运营策略或文档说明，不进入协议。 |

## 5. End-To-End Sequence

```mermaid
sequenceDiagram
    participant User
    participant SourcePC as "Source PC"
    participant NT10 as "NT10 Cast Transmitter"
    participant NA20 as "NA20 AXTP Endpoint / Media Bridge"
    participant MediaHost as "Upper Host MediaHost"
    participant Player as "MediaHost Player"

    User->>MediaHost: Insert NA20 / open receiver app
    MediaHost->>NA20: Open AXTP-USB-HID session
    NA20-->>MediaHost: Session accepted / identified

    Note over MediaHost,NT10: Pairing is already completed by cast-rxtx-pairing flow.
    User->>SourcePC: Insert NT10
    SourcePC-->>NT10: Power and source media available
    NT10->>NA20: Auto-start wireless cast(H.264 Annex-B + AAC passthrough)
    NA20-->>NA20: Mark video/audio source available or receiving
    MediaHost-->>MediaHost: receiverPhase=incoming, authRequired=false
    Note over NA20,MediaHost: Opening only creates NA20->MediaHost downstream streams; it does not stop or rebuild NT10->NA20 upstream media.

    alt Producer-open accepted
        NA20->>MediaHost: Draft video.openStream(peerRole=receiver, source=wireless_cast, codec=h264, videoPtsMode=sameAsCursor)
        MediaHost-->>NA20: accepted video stream context / receiver ready
        NA20->>MediaHost: Draft audio.openStream(peerRole=receiver, source=wireless_cast_audio, codec=aac, audioPtsMode=derivedFromSeq)
        MediaHost-->>NA20: accepted audio stream context / receiver ready
    else Producer-open rejected, receiver pulls later
        NA20->>MediaHost: Draft video/audio.openStream(peerRole=receiver, source=wireless_cast or wireless_cast_audio)
        MediaHost-->>NA20: rejected(receiver not ready or policy)
        NA20-->>MediaHost: Draft per-media source available/receiving event(no streamId)
        MediaHost->>NA20: Draft video.openStream(peerRole=transmitter, source=wireless_cast, videoPtsMode=sameAsCursor)
        NA20-->>MediaHost: accepted video stream context
        MediaHost->>NA20: Draft audio.openStream(peerRole=transmitter, source=wireless_cast_audio, audioPtsMode=derivedFromSeq)
        NA20-->>MediaHost: accepted audio stream context
    else Receiver pulls from available source first
        NA20-->>MediaHost: Draft per-media source available/receiving event
        MediaHost->>NA20: Draft video.openStream(peerRole=transmitter, source=wireless_cast, videoPtsMode=sameAsCursor)
        NA20-->>MediaHost: accepted video stream context
        MediaHost->>NA20: Draft audio.openStream(peerRole=transmitter, source=wireless_cast_audio, audioPtsMode=derivedFromSeq)
        NA20-->>MediaHost: accepted audio stream context
    end

    NA20-->>MediaHost: Draft video.streamStateChanged(streaming)
    NA20-->>MediaHost: Draft audio.streamStateChanged(streaming)
    MediaHost-->>MediaHost: receiverPhase=streaming
    MediaHost->>Player: Create decoders and A/V sync group

    loop realtime media
        NT10-->>NA20: Source media packet(cursor=sourceCaptureTimestampUs)
        NA20-->>MediaHost: STREAM(videoStreamId, seqId, cursor=sourceCaptureTimestampUs, H.264 frame)
        NA20-->>MediaHost: STREAM(audioStreamId, seqId, cursor=sourceCaptureTimestampUs, AAC packet)
        MediaHost->>Player: Decode, jitter-buffer, sync, render/play
        Player-->>MediaHost: first frame rendered / audio playback started
        MediaHost-->>MediaHost: receiverPhase=rendering
    end

    loop clock report interval
        NT10-->>NA20: source clock report(media PTS anchors, nt10 monotonic clock)
        NA20-->>MediaHost: Generated draft stream.clockReport(source=NT10 anchors, relay=NA20 receive/send time)
        MediaHost-->>MediaHost: Record local receive time and fit NT10/NA20/Nearcast drift
    end

    alt MediaHost stops receiving first
        MediaHost->>NA20: Draft audio.closeStream(peerRole=transmitter, streamId, reason=receiver_closed)
        NA20-->>MediaHost: audio stream closed / already closed
        MediaHost->>NA20: Draft video.closeStream(peerRole=transmitter, streamId, reason=receiver_closed)
        NA20-->>MediaHost: video stream closed / already closed
        MediaHost->>Player: Release decoders and buffers
        Note over NT10,NA20: NT10->NA20 wireless cast may remain active; only NA20->MediaHost downstream streams are closed.
        opt MediaHost window opens again while source is retained
            MediaHost->>NA20: Draft video.openStream(peerRole=transmitter, source=wireless_cast)
            NA20-->>MediaHost: accepted video stream context
            MediaHost->>NA20: Draft audio.openStream(peerRole=transmitter, source=wireless_cast_audio)
            NA20-->>MediaHost: accepted audio stream context
            NA20-->>MediaHost: STREAM(new video/audio streamIds from retained source)
            Note over NT10,NA20: Upstream source was retained; no wireless recast setup is implied.
        end
    else NT10/source stops first
        User->>SourcePC: Unplug NT10 / stop source
        NT10--xNA20: Wireless media stops
        NA20->>MediaHost: Draft audio.closeStream(peerRole=receiver, streamId, reason=source_disconnected)
        MediaHost-->>NA20: audio receiver closed / already closed
        NA20->>MediaHost: Draft video.closeStream(peerRole=receiver, streamId, reason=source_disconnected)
        MediaHost-->>NA20: video receiver closed / already closed
        MediaHost->>Player: Release decoders and buffers
    else AXTP session or transport hard-disconnects
        NA20--xMediaHost: USB HID / AXTP session lost in either direction
        MediaHost->>Player: If still running, fail all stream contexts and release decoders
        NA20->>NA20: If still running, stop STREAM to old session and release contexts
        Note over NA20,MediaHost: No closeStream is required or guaranteed; reconnect creates a new session. If source remains available, NA20 may producer-open or MediaHost may pull.
    end
```

### 5.1 Simplified Stream Timing Contract

本流程采用最简时钟方案，保持 STREAM 16B header 不变：

```text
streamId = NA20->MediaHost 下游 stream id
seqId    = NA20->MediaHost 当前 stream 的包序号
cursor   = NT10 sourceCaptureTimestampUs，源端单调时钟域采集时间，单位 us
```

音频 stream context 在 `audio.openStream` / result 中确认：

```json
{
  "audioPtsMode": "derivedFromSeq",
  "timebase": 48000,
  "sampleRate": 48000,
  "samplesPerPacket": 1024,
  "firstMediaSeqId": 0,
  "audioPtsBase": 0,
  "packetizationMode": "fixedSamplesPerPacket",
  "cursorUnit": "sourceCaptureTimestampUs"
}
```

该模式下 MediaHost 使用 `extendedSeqId` 推导音频 PTS：

```text
audioPts = audioPtsBase + (extendedSeqId - firstMediaSeqId) * samplesPerPacket
audioTimeSec = audioPts / timebase
```

视频 stream context 在 `video.openStream` / result 中确认：

```json
{
  "videoPtsMode": "sameAsCursor",
  "timebase": 1000000,
  "cursorUnit": "sourceCaptureTimestampUs",
  "packetizationMode": "completeFrame"
}
```

该模式下视频 PTS 等同于 `cursor`，MediaHost 使用：

```text
videoPts = cursor
videoTimeSec = videoPts / 1000000
```

约束：

- NT10 必须从源端采集链路取得 `sourceCaptureTimestampUs`；不得使用 wall clock、编码完成时间或发送时间。
- NA20 转发时不得覆盖 NT10 的 `cursor`，也不得在 `audioPtsMode=derivedFromSeq` 的 stream 中重新合包、拆包或插入不消耗 samples 的媒体包。
- 每个音频 STREAM packet 固定消耗 1024 samples；如果未来该约束不成立，必须改为显式携带音频 `mediaPts`。
- 每个视频 STREAM packet 承载完整视频帧；AXTP frame fragmentation 可在 STREAM parser 前完成重组，但媒体层不拆成多个 STREAM seqId。
- `stream.clockReport` 周期性把 NT10 源端媒体 PTS、NT10 单调时钟、NA20 中转时钟和 MediaHost 本地接收时间关联起来，用于漂移估计和诊断；它不替代 STREAM `seqId` / `cursor`。

## 6. Interaction Steps

| Step | Actor | Action | Capability / precondition | Protocol call/event | Payload fields | Result / state change | Coverage | Error / fallback |
|---:|---|---|---|---|---|---|---|---|
| 1 | MediaHost | 建立到 NA20 的控制和数据会话。 | NA20 connected, Standard Framed supported。 | `AXTP-USB-HID` session | transport/session fields | MediaHost 可与 NA20 交互 RPC，并接收 STREAM。 | generated | HID 打开失败则提示 NA20 未连接或被占用。 |
| 2 | MediaHost | 确认 NA20/NT10 配对已完成，并准备接收 NA20 发起的 Host-side stream request 或后续主动拉取 source。 | Pairing flow done, host receiver service ready。 | local-only / previous flow result | pair record, receiver role, source availability cache | 进入投屏媒体等待阶段。 | local-only | 未配对时跳转 `cast-rxtx-pairing` flow。 |
| 3 | User / Source PC | 用户将 NT10 插入源端 PC。 | NT10 已与 NA20 配对，源端 PC 供电。 | non-protocol | source power, source media availability | NT10 具备向 NA20 发起无线投屏的条件。 | non-protocol | NT10 未供电或未配对时 NA20 不会产生 ready 事件。 |
| 4 | NT10 | 自动向 NA20 发起无线投屏。 | NT10 connected to NA20 AP。 | device-specific wireless cast | H.264 Annex-B video, AAC passthrough audio | NA20 开始接收上游音视频。 | non-protocol | 无线连接失败由设备侧重连或提示处理，不进入 AXTP wire。 |
| 5 | NA20 | 检测视频 source available/receiving，并选择主动请求 Host 接收或发出 source state。 | 收到 NT10 H.264 输入。 | Draft `video.openStream` from NA20, or per-media source available/receiving event | source, codec=h264, stream profile, peer media role=receiver when producer-open, `videoPtsMode=sameAsCursor`, `timebase=1000000`, `cursorUnit=sourceCaptureTimestampUs`, `packetizationMode=completeFrame` | 若 producer-open 被接受，MediaHost 创建 H.264 decoder/buffer；若只发 source event，MediaHost 缓存可拉取状态。 | draft amendment | 若 MediaHost 拒绝或不支持 receiver service，NA20 不发送 STREAM；source 仍可用时可继续通知 Host 后续 pull。 |
| 6 | NA20 | 检测音频 source available/receiving，并选择主动请求 Host 接收或发出 source state。 | 收到 NT10 AAC 输入。 | Draft `audio.openStream` from NA20, or per-media source available/receiving event | source, codec=aac, transportFormat, sampleRate=48000, peer media role=receiver when producer-open, `audioPtsMode=derivedFromSeq`, `samplesPerPacket=1024`, `firstMediaSeqId=0`, `audioPtsBase=0`, `packetizationMode=fixedSamplesPerPacket`, `cursorUnit=sourceCaptureTimestampUs` | 若 producer-open 被接受，MediaHost 创建 AAC decoder/playback pipeline；若只发 source event，MediaHost 缓存可拉取状态。 | draft amendment | 可能只有音频或只有视频进入 available/receiving；AV MVP 成功仍需要两路都成功建流。 |
| 7 | MediaHost | 响应 NA20 producer-open。 | open request valid, resources available。 | Draft openStream response | accept/reject, stream context confirmation, limits, timing metadata | 接受时双方确认 streamId、codec、syncGroup、clock、音频 PTS 推导参数和视频 PTS 模式；拒绝时不创建下游 stream。 | draft amendment | 拒绝后 NA20 不发送该 streamId 的 STREAM；若 source 仍 available/receiving，可发 source event 提醒 Host 主动拉取。 |
| 8 | MediaHost | 在 source available/receiving 时主动拉取视频或音频。 | Host receiver service ready, source state known or queried。 | Draft `video.openStream` / `audio.openStream` from MediaHost | source, mediaKind, peer media role=transmitter, requested profile/format, sync metadata, timing mode requirements | NA20 知道自己是 transmitter/producer，接受后创建新的 NA20->MediaHost downstream stream context。 | draft amendment | source 已停止则返回 unavailable；资源不足则 Host 保持 source cache 并等待下一次 state event 或用户重试。 |
| 9 | NA20 | 视频进入 streaming 状态。 | Video stream accepted and producer bound。 | Draft `video.streamStateChanged` | streamId, state, codec, resolution, syncGroupId | MediaHost 开始接收 H.264 STREAM；聚合 `receiverPhase=streaming`。 | draft | 启动失败需暴露 media stream start failure。 |
| 10 | NA20 | 音频进入 streaming 状态。 | Audio stream accepted and producer bound。 | Draft `audio.streamStateChanged` | streamId, state, codec=aac, sampleRate, channels, syncGroupId | MediaHost 开始接收 AAC STREAM；聚合 `receiverPhase=streaming`。 | draft | 启动失败需区分 source 不可用、codec 不支持、音频设备忙。 |
| 11 | NA20 | 发送视频数据。 | Video stream accepted/opened。 | `PayloadType=STREAM` | streamId, seqId, cursor=NT10 `sourceCaptureTimestampUs`, H.264 complete frame, minimal video flags such as keyFrame/config/discontinuity | MediaHost 按 seqId 检测丢包，按 cursor 作为 videoPts 放入视频时间线。 | generated + draft | Core STREAM 和 `videoPtsMode=sameAsCursor` metadata 已生成；flags/profile binding 仍按草案说明实现。 |
| 12 | NA20 | 发送音频数据。 | Audio stream accepted/opened。 | `PayloadType=STREAM` | streamId, seqId, cursor=NT10 `sourceCaptureTimestampUs`, AAC packet, minimal audio flags such as config/discontinuity | MediaHost 使用 extended seqId 推导 audioPts，并按 cursor 记录源采集时间和 jitter。 | generated + draft | Core STREAM 和 `audioPtsMode=derivedFromSeq` metadata 已生成；minimal flags/profile binding 仍按草案说明实现。 |
| 13 | MediaHost / Player | 音频与视频同步播放。 | video/audio share syncGroupId and clock semantics。 | local-only using stream metadata | syncGroupId, audio derived pts, video cursor pts, clock report samples | Player 做 A/V sync、缓冲控制和漂移修正；首帧/播放开始后聚合 `receiverPhase=rendering`。 | local-only | 缺少源媒体时间戳或 clock mapping 时需草案补字段。 |
| 13a | NT10 / NA20 / MediaHost | 周期性上报和转发 clock report。 | Media streaming active。 | Generated draft `stream.clockReport` emitted by NA20 to MediaHost, sourced from NT10 report | reportSeq, syncGroupId, sourceDeviceId, sourceClockDomain, audio/video anchors, NT10 monotonic time, NA20 receivedAt/sentAt | MediaHost 记录本地 receive monotonic time，拟合 NT10 media timeline -> NT10 clock -> NA20 relay -> MediaHost clock。 | generated draft | 若 report 丢失，latest-wins；不阻塞媒体数据，下一次 report 继续校准。 |
| 14 | MediaHost | 提前停止接收某一路或全部媒体。 | Streaming active, user stop or receiver resource policy。 | Draft `audio.closeStream` / `video.closeStream` | streamId, peer media role=transmitter, reason=receiver_closed/user_stop/not_needed | NA20 关闭对应 NA20->MediaHost downstream streamId，停止向 Host 发送 STREAM；NT10->NA20 upstream source 可继续 active。 | draft amendment | close 必须幂等；若 NA20 已关闭，返回 already closed 或等价 closed；不得把 receiver close 当成 source stop。 |
| 15 | MediaHost / NA20 | 用户重新打开投屏窗口，从保留的 upstream source 重新接收。 | NT10->NA20 upstream source retained, Host receiver service ready。 | Draft `video.openStream` / `audio.openStream` from MediaHost or NA20 re-offer | source, peer media role=transmitter or receiver by initiator, new stream context, sync metadata | 建立新的 NA20->MediaHost downstream streamId；无需重建 NT10->NA20 无线推流。 | draft amendment | 若 upstream source 已超时或停止，则 NA20 返回 source unavailable 或等待新的 NT10 source ready。 |
| 16 | User / Source PC | 用户拔出 NT10 或源端停止。 | Streaming active, receiver inactive, or source receiving。 | non-protocol | source power/media lost | NT10 不再向 NA20 发流；NA20 retained source 失效。 | non-protocol | NA20 不保留 streamId 等待快速重连；重连后重新 open。 |
| 17 | NA20 | 检测 video/audio source stopped 并主动关闭相关 stream。 | 上游无线媒体停止。 | Draft device-initiated `audio.closeStream` / `video.closeStream`, or terminal state event | streamId, peer media role=receiver, reason=source_disconnected, last cursor/seq, castSessionId | 若 Host downstream stream 仍 active，则对应 streamId 终止；若 receiver inactive，则仅清理 retained source 状态。 | draft amendment | stopped 不要求独立 sourceStopped；可合并到 close/terminal state。 |
| 18 | MediaHost / Player | 根据 close request 或 stream 终态释放播放器资源并回到等待 open。 | video/audio closed or failed observed。 | local-only | stream contexts, syncGroupId | 清理 decoder、buffer 和 UI 状态；若只是 receiver close，NA20 可继续保留 upstream source。 | local-only | 若 NT10 快速重插且 source 再次 available，可由 NA20 re-offer 或 Host pull。 |
| 19 | MediaHost / NA20 | 同一 stream 发生重复 close 或双方同时 close。 | stream is closing/closed。 | Draft closeStream response or terminal state | streamId, final state, reason, close origin | 双方收敛到同一 terminal state。 | draft amendment | 不得重新打开旧 streamId；旧 STREAM 数据按 unknown/closed stream 丢弃。 |
| 20 | MediaHost / Player | 检测 NA20 被拔出、重启、掉电或 USB HID session 断开。 | Streaming active or opening。 | AXTP session/transport lost | session id, active stream contexts, last observed seq/cursor | MediaHost 本地将该 session 下所有 video/audio stream 标记为 failed/closed，并释放 decoder、buffer、UI 播放状态。 | generated + local-only | 不等待 `closeStream`；NA20 重连后必须新建 session 并重新 open。 |
| 21 | NA20 | 检测 MediaHost 崩溃、进程退出、session lost 或 receiver timeout。 | Streaming active or opening。 | AXTP session/transport lost / runtime timeout | session id, stream contexts, last sent seq/cursor | NA20 停止向旧 session 发送 STREAM，释放旧 downstream stream context；可按设备策略继续接收或丢弃 NT10 上游媒体。 | generated + runtime policy | MediaHost 恢复后不能复用旧 stream；若 source 仍可用，由 NA20 重新发起 open 或接受 Host pull。 |
| 22 | MediaHost / NA20 | 硬断流后恢复连接。 | Device reconnected or receiver app restarted。 | New AXTP session, then draft openStream if source available | new session id, new stream context, castSessionId if reused semantically | 建立全新的 stream context；旧 streamId、旧 close/open response、旧 STREAM packet 全部无效。 | generated + draft amendment | 迟到旧包按 closed/unknown session 丢弃；不得把旧 session 状态迁移到新 session。 |

## 7. State Changes And Events

| State change | Trigger | Event needed | Payload | Client handling | Coverage |
|---|---|---|---|---|---|
| wireless_cast source available/receiving | NT10 接入 NA20 并开始上游推流 | Optional source state query/event | source id, media kinds, codecs, state, session/cast id | MediaHost 缓存 source 状态；NA20 可 producer-open，MediaHost 也可 receiver-pull。 | draft |
| video source available/receiving | NA20 收到 NT10 H.264 输入 | Draft `video.openStream` from NA20, or video source available/receiving event | source, mediaKind=video, codec, state, castSessionId, receiver timestamp | MediaHost 可接受 NA20 producer-open，也可基于 source event 稍后主动拉取视频。 | draft amendment |
| audio source available/receiving | NA20 收到 NT10 AAC 输入 | Draft `audio.openStream` from NA20, or audio source available/receiving event | source, mediaKind=audio, codec, transportFormat if known, state, castSessionId | MediaHost 可接受 NA20 producer-open，也可基于 source event 稍后主动拉取音频。 | draft amendment |
| producer open rejected, source still available | MediaHost 拒绝 NA20 主动 open，但 NT10 上游仍在推流 | Per-media source available/receiving event | source id, mediaKind, codecs, state, last rejection reason if exposed | MediaHost 不创建 stream；等 receiver service ready 后可主动 `openStream` 拉取。 | draft amendment |
| receiver pulls source | MediaHost 窗口打开、receiver service 恢复或用户重新选择 source | Draft `video.openStream` / `audio.openStream` from MediaHost | source id, mediaKind, peer media role=transmitter, requested profile/format | NA20 作为 transmitter/producer 接受后创建新的 downstream streamId。 | draft amendment |
| video opening / streaming / closed | producer-open 或 receiver-pull accepted 后，NA20 发送或关闭视频流 | Draft `video.streamStateChanged` | streamId, state, codec, syncGroupId, reason | 创建/释放 video decoder。 | draft |
| audio opening / streaming / closed | producer-open 或 receiver-pull accepted 后，NA20 发送或关闭音频流 | Draft `audio.streamStateChanged` | streamId, state, codec, syncGroupId, reason | 创建/释放 audio decoder。 | draft |
| source clock report available | NT10 周期上报媒体 PTS 与源端单调时钟锚点，NA20 收到并准备转发 | Generated draft `stream.clockReport` from NA20 to MediaHost | reportSeq, syncGroupId, source device id, NT10 audio/video anchors, NA20 receivedAt/sentAt | MediaHost 记录本地 receive time，更新 drift/offset 估计；report latest-wins，不阻塞媒体。 | generated draft |
| receiverPhase streaming / rendering | stream 进入 `streaming`，随后播放器首帧或音频播放开始 | local aggregated state | receiverPhase, source id, active stream ids, firstFrame/audioStarted | UI 先展示正在接收，再展示已开始播放；重连后可由 source/stream/player state 重建。 | local-only |
| receiver closes video | MediaHost 用户停止、切换 source 或资源不足 | Draft `video.closeStream` from MediaHost | streamId, peer media role=transmitter, reason=receiver_closed/user_stop/not_needed | NA20 停止该 Host downstream video producer 并关闭 streamId；不默认停止 NT10 upstream video source。 | draft amendment |
| receiver closes audio | MediaHost 用户停止、切换 source 或资源不足 | Draft `audio.closeStream` from MediaHost | streamId, peer media role=transmitter, reason=receiver_closed/user_stop/not_needed | NA20 停止该 Host downstream audio producer 并关闭 streamId；不默认停止 NT10 upstream audio source。 | draft amendment |
| receiver reopens retained source | MediaHost 播放窗口重新打开，且 NA20 仍保留 NT10 upstream source | Draft `video.openStream` / `audio.openStream` from MediaHost or NA20 re-offer | source id, mediaKind, peer media role, new stream context, syncGroupId/castSessionId | 基于 retained source 建立新的 downstream streamId。 | draft amendment |
| video source stopped | NT10 拔出、断开或停止发送视频 | Draft device-initiated `video.closeStream` or terminal state | streamId, reason=source_disconnected/producer_stopped, last cursor/seq, castSessionId | MediaHost 释放播放器视频资源；重连后等待新的 source available、NA20 producer-open 或 Host pull。 | draft amendment |
| audio source stopped | NT10 拔出、断开或停止发送音频 | Draft device-initiated `audio.closeStream` or terminal state | streamId, reason=source_disconnected/producer_stopped, last cursor/seq, castSessionId | MediaHost 释放播放器音频资源；重连后等待新的 source available、NA20 producer-open 或 Host pull。 | draft amendment |
| simultaneous close | MediaHost 和 NA20 近同时关闭同一 stream | Draft closeStream response or terminal state | streamId, close origin, final state, final cursor/seq | close 幂等，双方最终都是 closed/failed，不产生第二个有效终态。 | draft amendment |
| AXTP session/transport lost | NA20 被拔出、重启、掉电，或 MediaHost 崩溃/退出导致 session 断开 | no business event required | session id, active stream contexts, last observed cursor/seq if available | 仍存活的一端本地终止该 session 下所有 stream，不等待 `closeStream`。 | generated + local-only |
| receiver timeout | MediaHost 卡死、长时间不消费或 STREAM 发送失败 | no business event required; optional runtime diagnostic | session id, streamId list, timeout reason, last sent cursor/seq | NA20 停止向旧 session 发送数据，释放 stream context；重连后重新 open。 | runtime policy |
| video seq gap / decode error | MediaHost 检测缺 chunk 或 H.264 解码失败 | no event required; request action needed | streamId, seq gap, frame id | 调用 draft `video.requestKeyFrame`。 | draft |
| audio seq gap | MediaHost 检测缺 chunk | no protocol event | streamId, seq gap | 静音补偿或丢弃过期 chunk。 | local-only |
| stats update | NA20 周期统计或 MediaHost 轮询 | Draft stream stats event/query | bitrate, dropped frames/chunks, jitter | 更新诊断 UI。 | draft |

## 8. Protocol Details

### 8.1 Adopted / Generated Protocols

| Method/Event/Profile | Purpose in this flow | Source |
|---|---|---|
| `AXTP-USB-HID` | MediaHost 通过 USB HID 与 NA20 建立 AXTP Standard Framed 会话。 | `contract/generated/protocol.md`, `contract/registry/core/protocol_meta.yaml` |
| `PayloadType=STREAM` | 承载视频和音频连续数据。 | `contract/generated/protocol.md`, `specs/20-core.md` |
| STREAM 16B header | 每个媒体 chunk 使用 `streamId`, `seqId`, `cursor`；业务含义由 stream context/profile 解释。 | `specs/20-core.md` |
| STREAM common errors | 缺包、无效 streamId、seq/cursor 问题的基础错误可复用。 | `contract/generated/protocol.md` |

### 8.2 Draft Or Missing Protocol Gaps

| Gap | Candidate domain.feature | Candidate method/event/schema | Routed skill | Review question |
|---|---|---|---|---|
| `video.stream` lifecycle 语义仍需补齐。 | `video.stream` | generated `video.getStreamCapabilities`, `video.openStream`, `video.closeStream`, `video.requestKeyFrame`, state/stats events, `videoPtsMode=sameAsCursor` timing metadata; draft source proxy/source-state semantics | `tooling/skills/40-amend-adopted-protocol/SKILL.md` after review | `[REVIEW-DRAFT]` 需补充 producer-initiated open、receiver-initiated pull open、source available/receiving event、bidirectional close、对端 receiver/transmitter 角色、close 幂等竞态、`wireless_cast` source、H.264 Annex-B、SPS/PPS 随关键帧和同步字段。 |
| 投屏音频实时流 lifecycle 语义仍需补齐。 | `audio.stream` | generated `audio.getStreamCapabilities`, `audio.openStream`, `audio.closeStream`, `audio.getStreamState`, `audio.streamStateChanged`, `audioPtsMode=derivedFromSeq` timing metadata; draft minimal audio flags/source-state semantics | `tooling/skills/40-amend-adopted-protocol/SKILL.md` after review | `[REVIEW-ASK]` 需补充 producer-initiated open、receiver-initiated pull open、source available/receiving event、bidirectional close、对端 receiver/transmitter 角色和 close 幂等竞态；AAC `transportFormat` 仍需确认。 |
| NA20 producer 需要主动向 MediaHost 发起分媒体 openStream。 | `video.stream` / `audio.stream` | `video.openStream`, `audio.openStream` with peer media role receiver | `tooling/skills/20-draft-business-protocol/SKILL.md` if current drafts lack this semantic | `[REVIEW-OK]` producer-initiated openStream 表达“请对端接收”；可能只有一路 open，accepted 前不得发送 STREAM。 |
| MediaHost receiver 需要在 source available/receiving 时主动拉取。 | `video.stream` / `audio.stream` | `video.openStream`, `audio.openStream` with peer media role transmitter | `tooling/skills/20-draft-business-protocol/SKILL.md` if current drafts lack this semantic | `[REVIEW-OK]` receiver-initiated pull 表达“请对端作为 transmitter 发送”；只建立 NA20->MediaHost downstream stream，不重建 upstream source。 |
| NA20 producer-open 被拒后需要通知 Host 后续可拉取。 | `video.stream` / `audio.stream` | per-media source available/receiving event/query | `tooling/skills/20-draft-business-protocol/SKILL.md` if current drafts lack source state event | `[REVIEW-OK]` rejected open 后不得发送 STREAM；source 仍可用时可发事件让 Host 主动 `openStream`。 |
| 已建立 stream 的 close 语义双向可控。 | `video.stream` / `audio.stream` | `video.closeStream`, `audio.closeStream`, or terminal state with reason | `tooling/skills/30-adopt-protocol-draft/SKILL.md` after review | `[REVIEW-OK]` MediaHost 可提前 close 下行接收，NA20 可 source stopped close；重复 close / 同时 close 必须幂等。 |
| receiver close 与 source stop 需要解耦。 | `video.stream` / `audio.stream` | receiver inactive / retained source lifecycle, reopen downstream stream from retained source | `tooling/skills/20-draft-business-protocol/SKILL.md` if current drafts lack lifecycle rule | `[REVIEW-OK]` MediaHost 结束投屏不默认断开 NT10->NA20；重新打开窗口时可复用 retained source。 |
| streamId 复用和旧包竞态需要约束。 | `video.stream` / `audio.stream` / runtime | stream instance guard or no-fast-reuse policy | `tooling/skills/20-draft-business-protocol/SKILL.md` if drafts lack lifecycle rule | `[REVIEW-DRAFT]` close 后的旧 STREAM 或旧 close 不得影响新 stream。 |
| 异常硬断流需要明确 lifecycle rule。 | `video.stream` / `audio.stream` / runtime profile | session-lost stream cleanup rule, terminal reason taxonomy such as session_lost / peer_disconnected / receiver_timeout | `tooling/skills/20-draft-business-protocol/SKILL.md` if drafts lack lifecycle rule | `[REVIEW-OK]` hard-disconnect 不要求业务 closeStream；旧 session 下所有 stream 立即失效。 |
| AXTP RPC request 方向需要支持设备调用 Host。 | core RPC / runtime profile | bidirectional request/response or explicit Host-side receiver service | `tooling/skills/50-generate-axtp-protocol/SKILL.md` after core/spec adoption, plus Stage 20 for feature role policy | `[REVIEW-DRAFT]` 本 flow 依赖 NA20 能向 MediaHost 发起业务 RPC request；generated 输出仍需同步。 |
| 音视频同步字段需要采纳。 | `video.stream` / `audio.stream` | `syncGroupId`, `castSessionId`, `clockDomain`, `receiverClockDomain`, `cursorUnit=sourceCaptureTimestampUs`, fixed packetization timing fields | `tooling/skills/30-adopt-protocol-draft/SKILL.md` after review | `[REVIEW-DRAFT]` 时间戳来源包含 NT10 源媒体采集时钟和 NA20 接收/转发时钟；`timestampBaseUs` / `firstPtsUs` 候选应被当前简化参数替代或映射。 |
| NT10 source clock report 需要透传到 MediaHost。 | `stream.flowControl` | generated draft `stream.clockReport` event from NA20 containing NT10 source anchors, NA20 relay timestamps, stream ids and syncGroupId | `contract/generated/protocol.md` | `[REVIEW-OK]` 当前 MVP 放在 `stream.flowControl`；该事件为 diagnostic latest-wins sample，不改变媒体包格式。 |
| NT10 开始/停止无线推流为可选 source proxy control。 | `video.stream` optional source control | `video.startStreamSource`, `video.stopStreamSource`, `video.getStreamSourceState`, `video.streamSourceStateChanged` | `tooling/skills/20-draft-business-protocol/SKILL.md` only if MVP/P1 需要修改草案 | `[REVIEW-OK]` 主流程不依赖该控制；可通过 NA20 控制，但 NA20 与 NT10 之间实现看设备。 |
| 整体投屏会话状态不新建独立 method domain。 | none | MediaHost/Launcher 聚合 source、stream 和 player state 为 `receiverPhase`；`castSessionId` 仅作为透明关联字段。 | Runtime/App implementation | `[REVIEW-OK]` 不治理独立 `cast.streaming`。 |

### 8.3 Candidate Feature Boundaries

| Feature | Responsibility | Boundary notes |
|---|---|---|
| `video.stream` | NA20 到 MediaHost 的视频业务流控制面、视频状态事件、视频 payload envelope 和关键帧重同步。 | 不定义 NT10 到 NA20 的无线协议；不定义音频数据。 |
| `audio.stream` | NA20 到 MediaHost 的 AAC 透传实时音频业务流控制面、音频状态事件和音频 payload envelope。 | 不要求 NA20 PCM fallback；不负责文件化录制。 |
| `stream.flowControl` | 公共 ACK/window/pause/resume/abort、低带宽流控、统计策略和 `stream.clockReport` 诊断事件。 | 普通业务 App 不需要直接调用 ACK/window；MediaHost runtime/SDK 使用 `stream.clockReport` 建立源媒体时间线、设备单调时钟、中转时钟和本地接收时钟之间的锚点。 |
| MediaHost player/runtime | 本地解码、jitter buffer、A/V sync、渲染播放和 UI 展示。 | local-only，不新增 AXTP method。 |

## 9. Test / Conformance Notes

| Case | Given | When | Then | Protocol evidence |
|---|---|---|---|---|
| producer-open accepted path | 配对完成，MediaHost 与 NA20 session ready | 用户将 NT10 插入源端 PC，NT10 自动向 NA20 推流 | NA20 分别向 MediaHost 发起 video/audio openStream，MediaHost 作为 receiver 接受后接收 STREAM | Draft producer-initiated `video.openStream`, draft `audio.openStream`, generated STREAM |
| producer-open rejected then source event | NA20 主动 open，MediaHost receiver service 未 ready 或策略拒绝 | MediaHost 拒绝 open，且 NT10 上游 source 仍 available/receiving | NA20 不发送 STREAM，发出分媒体 source available/receiving event；MediaHost ready 后主动 openStream 拉取 | Draft source available/receiving event, draft receiver-initiated openStream |
| receiver-pull available source | MediaHost 已知或查询到 source available/receiving | 用户打开投屏窗口或 receiver service 恢复 | MediaHost 主动调用 video/audio openStream，声明对端 NA20 是 transmitter；成功后接收 NA20->MediaHost downstream STREAM | Draft receiver-initiated `video.openStream`, draft `audio.openStream`, generated STREAM |
| single-media available | 只检测到视频或只检测到音频 source available/receiving | NA20 只 open 或只通知对应一路 source | MediaHost 只接收或拉取已成功建流的 stream；AV MVP 不判定整体成功 | Draft per-media openStream / source state event |
| happy streaming | MediaHost 已接受两路 openStream | NA20 发送 H.264 + AAC chunk | 播放器同步播放视频和音频 | Draft `video.streamStateChanged`, draft `audio.streamStateChanged`, generated STREAM |
| audio derived pts | Audio stream accepted with `audioPtsMode=derivedFromSeq` | NA20 发送 seqId 连续的 AAC packets | MediaHost 以 `audioPtsBase + (extendedSeqId - firstMediaSeqId) * samplesPerPacket` 推导音频 PTS，不要求每包携带 audio mediaPts | Generated `audio.openStream` timing metadata + generated STREAM seqId |
| video same-as-cursor pts | Video stream accepted with `videoPtsMode=sameAsCursor` | NA20 发送 H.264 complete frame STREAM packets | MediaHost 使用 STREAM `cursor` 作为 video PTS，timebase 为 1000000 | Generated `video.openStream` timing metadata + generated STREAM cursor |
| clock report relay | NT10 正在向 NA20 推流，NA20 与 MediaHost stream active | NT10 周期发送 source clock report，NA20 转发 `stream.clockReport` | MediaHost 记录本地 receive time，估计 NT10 media clock drift、NA20 relay delay 和端到端延迟趋势；丢失单个 report 不阻塞媒体 | Generated draft `stream.clockReport` in `stream.flowControl` |
| unsupported audio | NA20 不支持 `audio.stream` 或 AAC 透传 | MediaHost 查询音频能力 | AV MVP 不应判定成功，除非产品定义 video-only 降级 | Draft `audio.getStreamCapabilities` |
| unsupported video | NA20 producer-open 或 MediaHost receiver-pull 请求了不支持的 codec/resolution | 被请求端校验 open request | open 被拒绝或返回 typed error；NA20 不发送该 streamId 的 STREAM 数据 | Draft `video.getStreamCapabilities`, draft `video.openStream` |
| host early close | 音视频 stream active | MediaHost 用户停止播放或只关闭其中一路 | MediaHost 发起 closeStream，NA20 停止对应 downstream producer，不再发送该 streamId 数据，但 NT10->NA20 upstream source 继续 active | Draft bidirectional `audio.closeStream`, draft `video.closeStream` |
| host reopen retained source | MediaHost 已关闭下行 stream，但 NT10->NA20 upstream source 仍 active | 用户重新打开投屏窗口 | MediaHost 和 NA20 在 retained source 上建立新的 video/audio downstream streamId，无需重建 NT10->NA20 无线推流 | Draft openStream with retained source lifecycle |
| source stop close | 音视频 stream active | 用户拔出 NT10 或源端停止发流 | NA20 立即发起 closeStream 或 terminal state，MediaHost 释放播放器 | Draft device-initiated `audio.closeStream`, draft `video.closeStream`, or stream terminal state |
| simultaneous close | 音视频 stream active | MediaHost 提前 close，同时 NA20 检测 source disconnected | 双方 close 幂等，最终只有一个 terminal state；重复 close 返回 already closed 或等价 closed | Draft closeStream idempotency |
| NA20 hard unplug | 音视频 stream active | 用户直接拔出 NA20 或 NA20 重启/掉电 | MediaHost 检测 session/transport lost，本地释放所有 stream；不等待 `closeStream` | Generated AXTP transport/session lifecycle + local runtime cleanup |
| MediaHost crash | 音视频 stream active | MediaHost 进程崩溃、退出或 receiver 长时间无响应 | NA20 检测 session lost、STREAM 写失败或 timeout，停止旧 session STREAM 并释放 stream context | Generated AXTP transport/session lifecycle + runtime timeout policy |
| reconnect after hard-disconnect | 旧 session 已 lost，NA20 或 MediaHost 恢复 | 双方重新连接 | 建立新 AXTP session；若 source 仍可用，NA20 可重新 producer-open 或接受 Host receiver-pull；旧 streamId 和旧包不得复用 | Generated session lifecycle + draft openStream |
| no-stream-before-open | MediaHost session ready，但下游 stream 尚未被任一 openStream 接受 | NA20 准备发送媒体 | NA20 不得发送该 streamId 的 STREAM 数据；MediaHost 对未知 streamId 必须丢弃并记录 | Draft openStream + generated STREAM context rule |
| old-packet-after-close | stream 已 closed/failed | 旧 STREAM packet 或迟到 close 到达 | 接收端丢弃旧 packet，重复 close 收敛为 closed，不影响后续新 stream | Generated STREAM context rule + draft lifecycle rule |
| video seq gap | MediaHost 检测 video `seqId` 缺失 | MediaHost 请求关键帧 | 下一关键帧后恢复画面 | Draft `video.requestKeyFrame` |
| audio seq gap | MediaHost 检测 audio `seqId` 缺失 | MediaHost 继续播放音频 | 静音补偿或丢弃过期 chunk，不要求 NA20 重传旧音频 | Generated STREAM + local runtime handling |
| backpressure | MediaHost 处理过慢 | NA20 继续实时发送 | 视频丢旧帧、音频丢旧 chunk，保持低延迟并上报统计 | Draft stream stats / flow policy |

## 10. Acceptance Gates

- MediaHost 能通过 `AXTP-USB-HID` 与 NA20 建立 Standard Framed session，并接收 `PayloadType=STREAM`。
- 用户将 NT10 插入源端 PC 后，NT10 自动向 NA20 发起无线投屏；主流程不要求 MediaHost 先发开始投屏命令。
- HID/NA20 主流程不要求用户密码鉴权；统一 `receiverPhase` 中的 `authenticating` 通常跳过，并以 `authRequired=false` 或等价摘要表达。
- NT10 插入只触发 NA20 上游 source 进入 available/receiving；建流必须显式通过 `video.openStream` / `audio.openStream` 完成。
- MediaHost/Launcher 必须能从 source available/receiving、stream opening/streaming 和播放器首帧状态聚合 `incoming`、`streamStarting`、`streaming`、`rendering`。
- `receiverPhase=streaming` 只表示 STREAM 数据开始流动；`receiverPhase=rendering` 必须等首帧渲染或音频播放开始后才进入。
- `openStream` 必须同时支持 producer-initiated open 和 receiver-initiated pull：NA20 主动请求时声明对端 MediaHost 是 receiver，MediaHost 主动拉取时声明对端 NA20 是 transmitter/producer。
- 两种 open 方式都只建立 NA20->MediaHost downstream stream，不得隐式停止、断开或重建 NT10->NA20 upstream source。
- MediaHost 拒绝 NA20 producer-open 后，NA20 不得发送该 streamId 的 STREAM；若 source 仍 available/receiving，NA20 可发出 source available/receiving event，让 MediaHost 后续主动拉取。
- 每一路成功建流前，NA20 与 MediaHost 必须确认明确的 `streamId`、profile、cursorUnit、codec/format、状态和限制。
- 音频建流必须确认 `audioPtsMode=derivedFromSeq`、`timebase=48000`、`sampleRate=48000`、`samplesPerPacket=1024`、`firstMediaSeqId=0`、`audioPtsBase=0`、`packetizationMode=fixedSamplesPerPacket` 和 `cursorUnit=sourceCaptureTimestampUs`。
- 视频建流必须确认 `videoPtsMode=sameAsCursor`、`timebase=1000000`、`cursorUnit=sourceCaptureTimestampUs` 和 `packetizationMode=completeFrame`。
- MediaHost MVP 同时接受视频和音频 stream；单路成功不能直接判定 AV 投屏成功。
- 已建立 stream 的 `closeStream` 必须双向可发起；MediaHost 可提前关闭接收，NA20 可因 source stopped 关闭 producer。
- MediaHost 结束投屏或关闭窗口时，`closeStream(reason=receiver_closed/user_stop/not_needed)` 只关闭 NA20->MediaHost downstream stream，不得默认触发 NT10->NA20 source stop 或无线重建。
- MediaHost 重新打开投屏窗口时，若 NA20 仍保留 `wireless_cast` upstream source，双方必须能以新的 downstream stream context 恢复接收；旧 streamId 不复用，NT10->NA20 无线推流不重建。
- `closeStream` 必须幂等；重复 close、交叉 close 或 close 与 terminal state 同时到达时，双方必须收敛到同一 `closed` / `failed` 终态。
- 视频 H.264 chunk 能被 MediaHost 依据 `seqId`、`cursor=sourceCaptureTimestampUs`、minimal flags 和 keyframe 策略解码；在 `packetizationMode=completeFrame` 下不依赖媒体层 frame offset。
- 音频 AAC 透传 chunk 能被 MediaHost 依据 `seqId` 推导 audio PTS，并依据 `cursor=sourceCaptureTimestampUs` 记录源端采集时间和 jitter。
- 音频和视频具备共同 `syncGroupId/castSessionId` 与统一时钟语义，MediaHost 可做 A/V sync。
- NA20 必须周期向 MediaHost 发出或转发 `stream.clockReport`，报告 NT10 source media PTS anchors、NT10 monotonic clock、NA20 receive/send timestamp；MediaHost 必须记录本地 monotonic receive time 并用于 drift/offset 估计。
- NT10 拔出或停止发流后，NA20 必须立即关闭相关 streamId，并通过 device-initiated `video.closeStream` / `audio.closeStream` 或对应 stream terminal state 携带 source disconnected reason；MediaHost 释放播放器资源。
- NA20 被拔出、重启、掉电或 AXTP session/transport lost 时，MediaHost 必须本地终止该 session 下所有 stream，释放播放器资源，不等待业务 `closeStream`。
- MediaHost 崩溃、进程退出、卡死或 receiver timeout 时，NA20 必须停止向旧 session 发送 STREAM 并释放旧 stream context；重连后重新建 session 和 stream。
- close 后 NA20 不得继续发送该 streamId 的 STREAM 数据；MediaHost 收到 closed/unknown streamId 数据必须丢弃并记录。
- session lost 后旧 `streamId`、旧 stream context、旧 open/close response 和迟到 STREAM packet 全部无效；新连接不得继承旧 session 的流状态。
- 同一 AXTP session 内媒体 streamId 不应快速复用；若协议允许复用，必须有 stream instance 或 castSessionId 校验避免旧包误伤新流。
- 丢包、背压、NT10 断连、source 不可用、codec 不支持等错误路径都有状态或错误码可观测。
- 本流程的简化 timing metadata 和 `stream.clockReport` 已进入 registry/generated；后续新增协议事实仍必须通过 Stage 20/30/40/50 工作流进入正式生成路径。

## 11. Open Questions

| Question | Impact | Owner | Status | Next action |
|---|---|---|---|---|
| AAC 透传的具体 `transportFormat` 是 ADTS、LATM、raw AAC，还是多种都支持？ | protocol / firmware | TBD | REVIEW-ASK | Stage 20/30 固化 `audio.stream` schema。 |
| `sourceCaptureTimestampUs` 的 NT10 时钟源使用 `CLOCK_MONOTONIC_RAW`、硬件 capture timestamp，还是设备平台等价 monotonic clock？ | firmware / runtime | TBD | REVIEW-ASK | 固件确认可用时钟源；协议只要求同一 stream 内单调、同源、不可被 wall clock/NTP 调整影响。 |
