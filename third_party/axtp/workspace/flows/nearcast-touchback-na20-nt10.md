---
status: flow-planned
contract: false
generated: false
domain: cast
feature: cast.touchback
registry: ""
lastReviewed: 2026-11-03
---

# NearCast Touchback: WebCast and NA20/NT10 Relay Flow

> 本文是 Stage 10 flow，不是机器可读协议事实。method、event、schema、ID 和
> 字段的正式来源是 `contract/registry/**` 及其生成产物。

## 1. Actors and transport boundary

```text
Tauri sender / Launcher  ◄──── authenticated RPC/control + touchback data ────  NearCast receiver
        ▲                              WebCast DataChannel                         │
        │                                                                         │ AXTP input.stream
        │                                                                         ▼
        └──────── Windows input injection ◄────────────── NA20 relay ◄────────── NT10 Touch-to-HID
```

WebCast 的 DataChannel 只是 AXTP `input.stream` 的承载适配器。HID 路径不经过
DataChannel，但复用相同的 `streamKind=touchback`、`touch-batch-v1`、epoch、
sequence、token 和 reset 语义。音频和视频仍使用各自 stream；关闭 touch stream
不能重启媒体 stream。

## 2. Discovery and trust

1. NearCast receiver 在局域网发布统一的 NearCast mDNS 服务，TXT 记录提供
   runtime instance、版本、控制端点和能力摘要。不要再以 `_webcast._tcp`
   作为第二套协议服务；WebCast 是 NearCast 的 capability/protocol 字段。
2. Tauri sender 扫描服务并展示候选列表。候选只代表可连接，不代表授权。
3. 首次 HTTPS/control 连接展示设备指纹；用户确认后 sender 固定信任该指纹。
4. sender 获取 `cast.getControlCapabilities` 和 `cast.getStatus`，只选择含有
   `cast.presentation`/`cast.touchback` 且协议不是 AirPlay 的 target。
5. sender 发送“显示投屏码”产品消息；receiver 在本地悬浮窗显示 code。投屏码
   仍是现场确认步骤，不被 mDNS 或 touch stream 替代。

## 3. WebCast first-use sequence

```text
Sender                  Receiver                 AXTP/adapter
  |  discover + trust       |                       |
  |----------------------->|                       |
  | getControlCapabilities  |                       |
  |----------------------->|                       |
  |<-----------------------| cast.presentation/touchback
  | select target           |                       |
  | setPresentationSettings |                       |
  |----------------------->| validate/CAS/rebuild  |
  |<-----------------------| presentationSettingsChanged
  | setTouchbackEnabled     |                       |
  |----------------------->| controllerRequested   |
  |<-----------------------| touchbackChanged      |
  | local receiverOptIn=on  |                       |
  |                         | effectiveEnabled     |
  |                         | input.openStream     |
  |------------------------>| streamId, epoch, mode |
  |<------------------------|                       |
  |                         | binary TouchBatch    |
  |<------------------------| validate/fence       |
  | reset/close or disconnect| release + close       |
  |<------------------------|                       |
```

`accepted=true` 只表示请求进入 owner lane；receiver 必须等
`effectiveEnabled=true` 和 `modeState=active`（在产品状态层）后才发送 batch。
没有 active stream 时，任何触控输入都应被 receiver 本地吞掉或显示不可用状态。

## 4. Control-plane state transitions

### 4.1 Per-target gates

```text
inactive
  ├─ receiverOptIn=false                 ──► inactive
  ├─ controllerRequested=false           ──► inactive
  └─ both true + capability + WebCast    ──► negotiating
                                             ├─ open accepted ──► active
                                             └─ NOT_SUPPORTED / denied ──► unavailable
```

每个 target 的完整 identity 是 `sessionId + sessionGeneration + slotId +
bindingGeneration + lifecycleGeneration`。slot 下标、投屏码、IP、MAC、窗口句柄
都不能单独作为 fencing key。目标 generation 不匹配时返回标准错误并在诊断中
注明 stale target，不能把请求应用到新 slot。

### 4.2 Mode switching

`touch` 是默认 preferred。`mouse` 被显式要求时是 required；对端不支持就
进入 unavailable，不得静默回退。运行中切换严格执行：

```text
switching
→ fence new events
→ send ordered/idempotent reset
→ release receiver-side contacts/buttons
→ input.closeStream(streamKind=touchback)
→ input.openStream(new preferred/required mode)
→ active(new mode) 或 unavailable
```

### 4.3 Canvas/geometry change

```text
settings patch accepted
→ transition=rebuilding / waitingKeyFrame
→ fence input and reset
→ close old epoch
→ atomically apply new canvas after safe keyframe
→ increment canvasRevision and geometryRevision
→ open new epoch with new canvasToken/geometryToken
```

旧画布的最后一帧可以继续显示；重建失败保留旧 applied canvas，并用新 epoch
重新绑定旧 token。改变一条 target 的 viewport 不得改变另一条 target。

## 5. WebCast data-plane sequence

1. 控制面 owner（通常是 receiver 在双门开启后）发起或确认
   `input.openStream`，请求带 `streamKind=touchback`、`format=touch-batch-v1`、
   `preferredMode`/`supportedModes`、target generations、canvas/geometry
   revisions and tokens，以及 sender 的 `commandId`。
2. `InputOpenStreamResult` 返回独立 `streamId`、`epoch`、`acceptedMode`、
   tokens、limits 和 revision；双方都缓存同一 stream binding。
3. receiver → sender 的 DataChannel 每条 binary message 为：标准 16B AXTP STREAM header + 28B
   TouchBatch header + 16B records。不要在 DataChannel 另加 JSON envelope。
4. sender 校验 `streamId`、epoch、sequence、tokens、长度、kind、phase 和
   CRC，并在对应 Windows source 上执行输入。正常 batch 只执行数据处理，不回 ACK。
5. `down/up/cancel` 依序进入 owner；连续 move 可按 contact 合并。应用队列
   超限时清除 move、释放 active 状态、发送 reset 并重建 epoch。
6. receiver 在 DataChannel close、target 变化、失焦、UI 隐藏或控制面关闭时
   发送 reset/close；sender 即使没收到 close 也必须通过 timeout/disconnect
   本地释放 Windows pressed state。

## 6. NA20/NT10 relay sequence

```text
NearCast receiver / input producer    NA20 relay                 NT10 endpoint
          | input.openStream             |                           |
          |----------------------------->| validate target/session   |
          |                              |--------------------------->|
          | TouchBatch(STREAM)           | opaque forward            |
          |=============================>|===========================>|
          |                              |                           | validate
          |                              |                           | map normalized x/y
          |                              |                           | emit HID report
          | input.close/reset            | forward in order          |
          |----------------------------->|--------------------------->|
          |                              |                           | release
```

### NA20 MUST

- 保留并按顺序转发 streamId、seqId/cursor、epoch、canvasToken、geometryToken、
  phase、buttons、wheel、reset 和 close 语义；
- 对 session/stream/epoch 不匹配、transport error、队列溢出和断链停止转发，
  报告诊断，并确保 NT10 收到 release/reset；
- 不改变坐标，不把 `touch` 转成 `mouse`，不识别长按，不生成新的 relay
  capability，不重排 down/up/cancel；
- 不因 touch stream close 关闭音频或视频 stream。

### NT10 MUST

- 验证 stream/epoch/sequence/generation、长度、CRC 和 token；
- 根据已协商 canvas/geometry 把 normalized `x/y` 映射到本地触摸/桌面分辨率；
- 在 `touch` 模式生成 contact HID，在 `mouse` 模式生成 primary/secondary/
  middle、drag、wheel 和 modifiers；
- 对 reset、timeout、disconnect、sequence gap、generation change 和 HID 注入
  失败发送 release/cancel，禁止保留按下状态；
- 暴露 accepted sequence、reset/gap count、last epoch 和 HID injection error
  诊断。

## 7. Error and recovery matrix

| 条件 | Receiver/NA20 行为 | Sender/NT10 可观察结果 |
|---|---|---|
| AirPlay target | 不广告 `cast.touchback`，不 open | `NOT_SUPPORTED`/能力缺失 |
| stale target/generation | 拒绝，不落到新 slot | 标记 target 过期，重新查询 status |
| old canvas/geometry token | 丢弃 batch，计数，不重放 | `STALE_GEOMETRY` 诊断详情或等价标准错误 |
| seq gap/CRC/invalid length | 停止普通输入，release，reset，重建 epoch | `modeState=unavailable` 直到新 open |
| queue overflow | 丢弃过期 move，保留安全释放路径 | reset/reopen |
| DataChannel/session close | 本地释放并关闭 stream | 需要重新建立控制面和 stream |
| NA20 relay loss | stop forwarding and release | NT10 释放 HID；媒体 stream 独立处理 |
| Windows injection/UAC failure | 不伪装成已生效；上报诊断并 release | `effectiveEnabled` 与 `injectionError` 分开 |

## 8. Protocol coverage

| Flow step | AXTP fact | Source of truth |
|---|---|---|
| capability discovery | `cast.getControlCapabilities`, `cast.presentation`, `cast.touchback`, `input.stream` | generated registry |
| canvas query/update | `cast.getPresentationSettings`, `cast.setPresentationSettings`, `cast.presentationSettingsChanged` | `cast.presentation.md` + registry |
| gate/mode/resolution | `cast.getTouchbackState`, `cast.setTouchbackEnabled`, `cast.setTouchbackMode`, `cast.setTouchbackResolutionPreference`, `cast.touchbackChanged` | `cast.touchback.md` + registry |
| stream open/close | `input.openStream`, `input.closeStream` | `input.stream.md` + registry |
| continuous data | AXTP STREAM + `touch-batch-v1` | `input.stream.md` |
| reset/release | ordered stream control frame and close semantics | `input.stream.md` |

## 9. Acceptance matrix

- 单路与双路同时投屏，关闭一路不影响另一路；
- receiverOptIn/controllerRequested 任一关闭时无数据注入；
- touch preferred、mouse preferred、mouse required 不支持时的明确失败；
- 画布固定/自适应、旋转/镜像/fit、旧 token 拒绝和重建无黑屏；
- down/move/up、长按右键、长按拖选、wheel、cancel 和失焦释放；
- DataChannel close、seq gap、CRC、未知 version、队列溢出和重连；
- AirPlay 无 touchback capability；
- NA20 字节/顺序透传，NT10 坐标/按钮/wheel/HID release；
- 生成 registry、AXTP boundary check、Windows target/test、`git diff --check` 和
  clean recursive checkout。
