# Cast Receiver UxPlay Protocol Interaction Flow

> Status: flow design
> Scope: Windows Launcher AirPlay receiver, Cast Receiver AXTP Adapter, UxPlay backend, Media / Render Core
> Source inputs: `workspace/business/cast-receiver-uxplay.md`, `workspace/legacy-migration/evidence/WEBSOCKET_PROTOCOL.md`, `contract/generated/protocol.md`, `workspace/protocol/auth/*.md`, `workspace/protocol/cast/*.md`
> Protocol lifecycle: Stage 10 `plan-protocol-flow`

本文只描述外部控制端、Launcher、UxPlay backend 和 AirPlay source 之间的控制面交互。正式合同仍以 `contract/registry/**`、`contract/protocol/axtp.protocol.yaml` 和 `contract/generated/**` 为准。

## 0. 速读结论

| 项目 | 内容 |
|---|---|
| Flow 目标 | 外部控制端通过 AXTP WS-JSON 管理 AirPlay 名称、会话、PIN、音频、窗口、backend 重启和本地渲染流控。 |
| 当前覆盖 | core RPC 已 generated；`cast.session`、`cast.pinCode`、`cast.audio`、`cast.window`、`cast.backend`、`cast.flowControl`、`cast.status` 已有 draft。 |
| 主要缺口 | LAN auth 入口策略、AirPlay 名称 schema、`stopSession` 幂等性、无窗口控制行为、backend 重启事件顺序。NT10 source 编码参数另有 Nearcast/NA20 分支。 |
| 边界 | AirPlay / mDNS / RAOP / 媒体帧不进入 AXTP cast 控制协议；UxPlay backend 内部控制口只是 adapter 实现。 |

## 1. 已确认决策

| Decision | Status |
|---|---|
| 对外控制口由 Cast Receiver AXTP Adapter 承载，不直接暴露 UxPlay backend 内部控制服务。 | `[REVIEW-OK]` |
| 第一版外部控制面使用 `AXTP-WS-JSON`；WebSocket 建立后由服务端发送 `Hello`，客户端 `Identify`。 | `[REVIEW-DRAFT]` |
| 外部控制口允许 LAN 访问；是否需要 token / HMAC 和 Origin 白名单由 auth 草案收敛。 | `[REVIEW-DRAFT]` |
| `cast.*` 普通查询和控制能力先按朴素可调用状态面设计，不拆分 per-method 权限 scope。 | `[REVIEW-OK]` |
| AirPlay 显示名称归入 `cast.session`。 | `[REVIEW-OK]` |
| 投屏音频播放默认关闭，只影响接收端本地播放。 | `[REVIEW-OK]` |
| PIN 保护默认开启；默认 PIN 自动生成，授权 response / event 可携带明文 PIN。 | `[REVIEW-OK]` |
| Backend 重启只重启 UxPlay backend；活动会话会被强制结束。 | `[REVIEW-OK]` |
| `receiverPhase` 作为跨 AirPlay/UxPlay 与 HID/NA20 的接收端统一阶段；`sessionState` 或媒体 source/stream state 继续保留为协议细节状态。 | `[REVIEW-OK]` |
| `cast.setRenderFps` 只控制本地渲染 fps，不要求 source 降低输入 fps。 | `[REVIEW-OK]` |
| `cast.setVideoStreamParams` 只在 Nearcast/NA20/NT10 source capability 声明支持时生效；UxPlay/AirPlay source 不支持时返回 `NOT_SUPPORTED`，不关闭控制 session。 | `[REVIEW-OK]` |
| `fps=0` 表示不限速；目标 fps 高于输入时按输入 fps 运行；默认 `dropMode=drop-late`。 | `[REVIEW-OK]` |
| 投屏窗口 `mode=normal` 表示退出全屏、取消置顶，并恢复进入全屏 / 置顶前的窗口尺寸和位置。 | `[REVIEW-OK]` |

## 2. Protocol Coverage

| Need | Coverage | AXTP candidate | Remaining question |
|---|---|---|---|
| WebSocket RPC 通道 | generated | `Hello`, `Identify`, `Identified`, `Request`, `RequestResponse`, `Event` | 用新 RPC session 替代 legacy `HelloAck`。 |
| LAN auth 入口策略 | draft | `Identify.d.authentication`, `auth.session`, `auth.token` | 确认是否需要 token / HMAC 和 Origin 白名单。 |
| AirPlay 名称 | draft | `cast.getAirPlayName`, `cast.setAirPlayName` | 名称长度、字符集、立即生效和失败恢复。 |
| 投屏会话查询与停止 | draft | `cast.getSession`, `cast.stopSession`, `cast.session*` | 状态枚举和无活动会话时的幂等性。 |
| 快照状态 | draft | `cast.getStatus` | 当前快照、敏感字段裁剪；不提供 `cast.statusChanged` 持续聚合事件。 |
| PIN / 密码保护 | draft | `cast.getPinCodeConfig`, `cast.setPinCodeConfig`, `cast.setPinCode`, `cast.pinCode*` | PIN 格式、明文可见性和日志脱敏验证。 |
| 投屏音频 | draft | `cast.getAudio`, `cast.setAudio`, `cast.setMuted`, `cast.audioChanged` | `enabled` / `muted` 关系和 `setMuted` 是否独立保留。 |
| 投屏窗口 | draft | `cast.getWindowState`, `cast.setWindowState`, `cast.windowChanged` | 无活动窗口时拒绝还是记住预设；是否需要 public show / hide。 |
| Backend 状态和重启 | draft | `cast.getBackendStatus`, `cast.restartBackend`, `cast.backendChanged` | session / backend 事件顺序。 |
| 本地渲染流控 | draft | `cast.getFlowControlState`, `cast.setRenderFps`, `cast.setFlowPolicy`, `cast.flowControlChanged` | 当前无业务阻断项；采纳时补 conformance。 |
| NT10 source 编码参数 | draft | `cast.setVideoStreamParams` + `video.closeStream(reason=encodingReconfigure)` + `audio.closeStream` + paired `video.openStream` / `audio.openStream` with fresh `syncGroupId` | 仅 Nearcast/NA20/NT10 source；活动音视频完整 close -> open，生成新的 audio/video `streamId` 和同步组；UxPlay/AirPlay 返回 `NOT_SUPPORTED`。 |
| Launcher 启动与 UxPlay 内部控制口 | local / non-protocol | app orchestration, backend adapter | 不进入 AXTP 公共协议。 |

### 2.1 Receiver Phase Model

`receiverPhase` 是给 UI、重连校准和跨协议对齐使用的低频阶段；它不替代 AirPlay 的 `sessionState`，也不要求 HID/NA20 新增独立 `cast.streaming` method domain。

| receiverPhase | AirPlay / UxPlay 映射 | HID/NA20 映射 |
|---|---|---|
| `idle` | receiver ready，但没有活动 source / session。 | 没有 active upstream source 或 downstream stream。 |
| `incoming` | UxPlay 收到 AirPlay source 连接或投屏意图。 | NA20 检测到 NT10 source available / receiving。 |
| `authenticating` | 等待 PIN、校验密码或 AirPlay 认证握手。 | 通常跳过；摘要中可标记 `authRequired=false`。 |
| `streamStarting` | 鉴权通过，mirror / media session 正在协商或 renderer 正在准备。 | `video.openStream` / `audio.openStream` accepted，stream 处于 `opening`。 |
| `streaming` | UxPlay 已收到媒体数据，或 backend 已确认 mirror 数据开始流动。 | video/audio downstream stream 进入 `streaming` 且 STREAM 数据开始到达。 |
| `rendering` | Render Core 已渲染首帧或开始本地音频播放；用户可见投屏开始。 | MediaHost player 完成首帧渲染或音频播放；AV MVP 可要求视频和音频都 ready。 |
| `interrupted` | source 暂停、短暂断流、窗口/renderer 可恢复异常。 | receiver inactive 且 source retained，或短暂背压/缺包后仍可恢复。 |
| `stopping` | 外部 stop、source 关闭或 backend restart 正在收敛。 | `closeStream` 发起，或 source stopped 正在关闭 active stream。 |
| `ended` | 正常结束，receiver 回到可接收下一次投屏。 | source stop / user stop 后 stream 正常关闭。 |
| `failed` | 鉴权、协商、backend 或 renderer 异常失败。 | source failed、open failed、session lost 或不可恢复媒体错误。 |

## 3. End-To-End Sequence

```mermaid
sequenceDiagram
    participant UI as "Launcher / App UI"
    participant Client as "External Control Client"
    participant AXTP as "Cast Receiver AXTP Adapter"
    participant Backend as "UxPlay Backend Adapter"
    participant Render as "Media / Render Core"
    participant UxPlay as "UxPlay AirPlay Service"
    participant Source as "iOS/macOS Source"

    UI->>AXTP: Start receiver control surface
    AXTP->>Backend: Start UxPlay backend
    Backend->>UxPlay: Launch AirPlay service
    Backend-->>AXTP: ready / failed
    AXTP-->>Client: cast.backendChanged

    Client->>AXTP: Open WebSocket
    AXTP-->>Client: Hello
    Client->>AXTP: Identify(auth, subscriptions)
    AXTP-->>Client: Identified(sid)
    Client->>AXTP: cast.getStatus
    AXTP-->>Client: session / receiverPhase / pin / audio / window / backend / flow summary

    opt Rename receiver
        Client->>AXTP: cast.setAirPlayName
        AXTP->>Backend: Republish mDNS / Bonjour name
        AXTP-->>Client: result + status/session event
    end

    Source->>UxPlay: Discover and start AirPlay connection
    UxPlay-->>Backend: incoming connection
    Backend-->>AXTP: incoming / pin required / auth result
    AXTP-->>Client: cast.sessionIncoming / cast.pinCodeRequired / cast.sessionStateChanged(receiverPhase=incoming/authenticating/streamStarting)

    UxPlay-->>Backend: mirror data flowing
    Backend-->>AXTP: session streaming
    AXTP-->>Client: cast.sessionStateChanged(receiverPhase=streaming)
    Render-->>AXTP: first frame rendered
    AXTP-->>Client: cast.sessionStarted(receiverPhase=rendering) + cast.windowChanged
    UI-->>UI: Show cast window

    opt Local controls
        Client->>AXTP: cast.setAudio / cast.setWindowState / cast.setRenderFps
        AXTP->>Render: Apply local audio / window / fps policy
        AXTP-->>Client: result + changed event
    end

    alt Client stops casting
        Client->>AXTP: cast.stopSession
        AXTP->>Backend: Stop current session
        AXTP-->>Client: cast.sessionStopped
    else Backend restart
        Client->>AXTP: cast.restartBackend
        AXTP-->>Client: cast.backendChanged(restarting)
        AXTP->>Backend: Stop active session if any, restart backend
        AXTP-->>Client: cast.sessionStopped(backendRestart)
        Backend-->>AXTP: ready / failed
        AXTP-->>Client: cast.backendChanged
    end
```

### 3.1 Nearcast → NA20 → NT10 音视频编码参数重配置分支

该分支不经过 UxPlay/AirPlay backend：Nearcast 通过 WS 控制面接收 `cast.setVideoStreamParams`，由 NA20 对 NT10 source 的编码目标执行完整的音视频 downstream stream replacement。参数也可以在 `video.openStream` 中直接给出；一次 open 的显式值优先于当前 session 配置，再回退到 source/profile 默认值。

```mermaid
sequenceDiagram
    participant WS as "Nearcast WS 控制面"
    participant NA20 as "NA20 video.stream"
    participant NT10 as "NT10 encoder/source"
    participant Audio as "Audio stream"

    WS->>NA20: cast.setVideoStreamParams(frameRate, bitrateKbps)
    NA20-->>WS: accepted=true, state=pending, reconfigureId
    alt 已有 active audio/video stream pair
        NA20->>NA20: video.closeStream(oldVideoStreamId, reason=encodingReconfigure)
        NA20->>NA20: audio.closeStream(oldAudioStreamId, reason omitted)
        NA20-->>WS: both close states converge to terminal closed
        NA20->>NA20: wait for both terminal closed (opening barrier)
        NA20->>NA20: retire old video/audio streamIds and old syncGroupId
    else session idle / no active audio/video pair
        NA20->>NA20: no close; there are no active streamIds or syncGroupId to retain
    end

    NA20->>NA20: allocate fresh syncGroupId
    par replacement video
        NA20->>NA20: video.openStream(peerRole=transmitter, new frameRate/bitrateKbps, original video context, syncGroupId=new)
    and replacement audio
        NA20->>NA20: audio.openStream(peerRole=transmitter, original AAC/adts/media.audio/timing context, syncGroupId=new)
    end

    alt both replacement opens succeed
        NA20-->>WS: both open results (newVideoStreamId, newAudioStreamId)
        NA20->>NT10: confirm new encoder target effective
        NT10-->>NA20: frameRate/bitrateKbps effective
        NA20-->>WS: cast.flowControlChanged(sourceVideo.state=streaming, reason=videoStreamReconfigure)
    else any replacement open fails
        NA20-->>WS: partial open failure (one or zero replacement streamIds)
        NA20->>NA20: close any partial replacement stream(s)
        NA20->>NA20: retire partial replacement IDs/group
        NA20->>NA20: allocate another fresh rollback syncGroupId
        par rollback video
            NA20->>NA20: video.openStream(peerRole=transmitter, old encoder target, original video context, syncGroupId=rollback)
        and rollback audio
            NA20->>NA20: audio.openStream(peerRole=transmitter, original AAC/adts/media.audio/timing context, syncGroupId=rollback)
        end
        alt both rollback opens succeed
            NA20-->>WS: state=rolledBack; both rollback streamIds are fresh
        else rollback incomplete or fails
            NA20->>NA20: close any partial rollback stream(s)
            NA20-->>WS: state=failed; no complete active audio/video pair
        end
    end
```

规则：

- 活动音视频 stream pair 变更时，必须把它作为一个 `pending` -> `closing` -> `opening` -> `streaming/applied` 事务：对旧 video 调用 `video.closeStream(reason=encodingReconfigure)`，并对旧 audio 调用 `audio.closeStream`（audio 的 `reason` 可以省略）。调用顺序不作规范性要求，但必须等待 audio 和 video 都达到 terminal `closed` 后才能进入 opening。
- 两侧 terminal close 后，旧 video/audio `streamId` 和旧 `syncGroupId` 全部退役；不得在 replacement 中复用。分配一个全新的 `syncGroupId`，并以同一新组分别打开 video 和 audio；两次 open 都必须返回新的 `streamId`。
- 新 `video.openStream` 使用新的 `frameRate` / `bitrateKbps`，并保持原有 video `source`、codec/format、stream/profile、cursor、PTS、packetization 和时钟等上下文；配对的 `audio.openStream` 保持原有 source、AAC codec、`adts` transport、`media.audio` profile 以及采样、packetization、PTS 和时钟等 timing context。两者使用同一个新 `syncGroupId`。
- 只有收到两个 replacement `streamId`、两次 open 都成功且 NT10 已确认新的 `frameRate` / `bitrateKbps` 实际生效，Nearcast/NA20 才能将 `sourceVideo.state` 标为 `streaming` 并发送 `cast.flowControlChanged(reason=videoStreamReconfigure)`。若初始 `cast.setVideoStreamParams` response 已返回 `state=pending`，这只是单次异步 response，不暗示稍后还会有第二个 response；客户端通过该 event 加 `cast.getFlowControlState`（或 `cast.getStatus`）查询最终状态。`state=applied` 只描述在事务可同步完成时返回的 `cast.setVideoStreamParams` result。该 event 的 `state` 字段仍是 `CastFlowControlState` 对象，不承载 `applied` 枚举。cast result 的 `previousStreamId` / `activeStreamId` 仍只表示 video stream ID；audio ID 通过 `audio.closeStream` / `audio.openStream` 响应或 `audio.streamStateChanged` 观察。
- session idle、没有 active audio/video pair 时省略 close，但仍分配一个新的共享 `syncGroupId`，直接打开完整的 video/audio pair；只有两个 open 成功且 NT10 参数实际生效，且请求可同步完成时，才在 `cast.setVideoStreamParams` result 报告 `applied`。
- 任一 replacement open 失败时，已经成功打开的 partial stream 属于不完整音视频 pair，必须先关闭并清理其 ID/group。随后恢复旧 encoder/audio 语义，以另一套全新的 video/audio `streamId` 和全新的 `syncGroupId` 重开完整 pair；不得复用任何退役标识。两侧都恢复成功才报告 `rolledBack`；若回退任一侧失败，关闭所有 partial rollback stream，报告 `failed`，此时没有 complete active audio/video pair。
- 若 session 只有 audio 或 video 一侧的 partial media state，则该状态不是 active cast pair；必须先清理并归一化 partial stream，再开始重配置，不能按 video-only active path 执行。
- 没有 active audio/video pair 时，只要求 `cast.flowControl.supportsVideoStreamParams` 和所选 source encoder capability；满足后可直接打开完整 video/audio pair，不要求 `supportsActiveVideoReconfigure`。
- 已有 active audio/video pair 时，除上述能力外还必须声明 `supportsActiveVideoReconfigure`；任一能力缺失时 Nearcast 返回 `NOT_SUPPORTED`。此错误不终止 WS/AXTP session，后续普通 RPC 仍可调用。
- UxPlay/AirPlay 只提供接收端本地渲染路径，不具备 NT10 encoder 控制能力；对这些 source 调用该方法一律返回 `NOT_SUPPORTED`，不得尝试重启 UxPlay 或改变 `cast.setRenderFps` 的语义。

## 4. Review Checks

- 密码保护关闭时，source 可直接进入投屏；密码保护开启时，等待密码、鉴权成功、鉴权失败不得误报为 `receiverPhase=streaming` 或 `receiverPhase=rendering`。
- `receiverPhase=streaming` 表示媒体数据开始流动；`receiverPhase=rendering` 必须等到 Render Core 首帧或音频播放开始。
- 控制端初次连接、重连或事件丢失后优先用 `cast.getStatus` 校准；持续变化由 feature-specific events 承担，必要时再调用 feature-specific get method。
- Backend 重启需要固定 `cast.sessionStopped` 与 `cast.backendChanged` 的顺序和用户可理解原因。
- 本地 fps cap 生效后，`inputFps` 和 `renderFps` 不能混淆，队列和端到端延迟不得持续增长。
- `cast.setVideoStreamParams` 的 `NOT_SUPPORTED` 只表示 capability/source 不支持；不得把 UxPlay/AirPlay 的本地 render fps 当作 NT10 encoder 参数，也不得因该错误关闭 WS/AXTP session。
- LAN 控制口如启用 auth，应在入口层完成身份校验；`cast.*` 内部普通操作不再按 scope 拆分。

## 5. Open Questions

| Question | Impact | Owner | Status |
|---|---|---|---|
| LAN 控制口是否需要 token / HMAC 和 Origin 白名单？ | security / product | auth | REVIEW-ASK |
| AirPlay 名称长度、字符集、立即生效策略和失败恢复如何定义？ | protocol / backend | cast.session | REVIEW-ASK |
| `stopSession` 在无活动会话时应返回成功，还是返回 no active session？ | protocol / app | cast.session | REVIEW-ASK |
| Backend 重启强制结束会话时，停止原因和 backend 状态如何展示给用户？ | product / backend | cast.backend / cast.session | REVIEW-ASK |
