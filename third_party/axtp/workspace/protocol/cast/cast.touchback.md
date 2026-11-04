---
status: generated
contract: true
generated: true
domain: cast
feature: cast.touchback
registry: ../../../../contract/registry/domains/cast/domain.yaml
lastReviewed: 2026-11-03
---

# cast.touchback

## 0. 速读结论

`cast.touchback` 是产品控制面；连续输入属于 `input.stream`。控制面按 target
独立维护接收端开关、Launcher 请求、模式、stream epoch、分辨率偏好和几何
fence。AirPlay 不声明此 capability。

| Method/event | 用途 |
|---|---|
| `cast.getTouchbackState` (`0x161C`) | 查询一路完整状态 |
| `cast.setTouchbackEnabled` (`0x161D`) | 设置 controller gate |
| `cast.setTouchbackMode` (`0x161E`) | 设置 touch/mouse 与 preferred/required |
| `cast.setTouchbackResolutionPreference` (`0x161F`) | 设置坐标/源质量偏好 |
| `cast.touchbackChanged` (`0x1615`) | 状态和 stream lifecycle 事件 |
| `cast.getControlCapabilities` (`0x1615`) | 返回 presentation/touchback sections |
| capability `cast.touchback` (`0x160F`) | 声明协议路径和模式 |

## 1. Per-target identity and state

`CastTouchbackTarget` 的 fencing 字段是 `sessionId`、`sessionGeneration`、
`slotId`、`bindingGeneration`、`lifecycleGeneration`。状态至少包含：

```text
receiverOptIn       // receiver local UI gate, default false, not persisted
controllerRequested // Launcher/Tauri gate
effectiveEnabled    // both gates + target/capability/protocol checks
requestedMode       // touch | mouse
negotiatedMode      // touch | mouse | none
resolutionPreference// registry enum: inherit | prefer1080p
streamId, epoch
canvasRevision, geometryRevision
canvasToken, geometryToken
revision, diagnosticReason
```

产品实现可在本地扩展 `modeState`、`resolutionState` 和 `viewport`，但向 AXTP
返回的字段必须遵守 generated schema。`accepted` 只表示 owner lane 接受了
写请求，不表示 `effectiveEnabled` 或 stream 已 active。

## 2. Gate semantics

```text
effectiveEnabled = receiverOptIn
                && controllerRequested
                && targetIsPresented
                && touchbackCapabilitySupported
                && protocol != airplay
```

本地 UI 和 Launcher 请求进入同一个 target owner lane。任一 gate 从 true 变为
false 时，owner 先 fence 新事件、释放本地状态、发送幂等 reset、关闭旧 stream，
再发布 `cast.touchbackChanged`。重新打开必须以新的 epoch 建流。

## 3. Mode negotiation

- 默认 `requestedMode=touch`；sender 应声明 `supportedModes=[touch, mouse]`。
- `preferred` 允许在对端明确声明不支持 preferred 时选择另一支持模式。
- `required` 只接受指定模式；不支持返回 `NOT_SUPPORTED`，不能静默降级。
- 运行中切换执行 reset → close → open；切换期间不发送普通 batch。
- `touch` 传输原始 contact 生命周期；`mouse` 传输接收端已经转换的按钮、拖动、
  滚轮和 modifiers。长按右键/拖选文本的阈值和 slop 归 NearCast policy。

## 4. Resolution and geometry

当前 registry 的 `CastTouchbackResolutionRequest` 使用 `inherit | prefer1080p`：

- `inherit` 表示保持 sender 当前编码分辨率，只使用已应用 canvas/viewport 计算
  normalized 坐标；
- `prefer1080p` 表示向 WebCast sender 请求 1080p 源质量，sender 可以报告
  `degraded` 或 `unsupported`；
- TouchBatch v1 的实际 x/y 始终是 0..65535 normalized，偏好不改变 packet
  layout，也不能靠接收端放大画布伪造 1080p。

接收端必须在 state/event 中报告当前 preference 和诊断状态，并明确 sender
拒绝/降级的结果。

每个 stream 绑定当时的 canvas/geometry revision 和 token；画布改变、旋转、
镜像、fit、target 复用或 runtime 重启都必须使旧 token 失效。

## 5. Control examples

### Enable one WebCast route

```text
1. getTouchbackState(target=A)
2. setTouchbackEnabled(target=A, enabled=true, commandId=C1)
3. wait touchbackChanged(target=A)
4. receiver local UI sets receiverOptIn=true
5. receiver opens input.openStream(target=A, streamKind=touchback)
6. receiver sends binary batches to the bound sender only after active state is observable
```

### Disable or route change

```text
1. fence target A
2. send reset(epoch E)
3. input.closeStream(streamId=S, epoch=E, reason=requested|route_changed)
4. release buttons/contacts locally and at NT10 when applicable
5. publish touchbackChanged with negotiatedMode=none or closed state
```

## 6. Capability and AirPlay policy

`cast.touchback` capability lists supported paths (`webcast`, later `hid`) and
supported modes, and says whether per-target/control-plane operation is supported.
AirPlay receivers omit this capability entirely and must never open `input.stream`
for touchback. A sender seeing no capability must keep ordinary AirPlay video/audio
working and report only the unsupported touchback operation.

## 7. Errors and diagnostics

Use registered common errors (`NOT_SUPPORTED`, `PERMISSION_DENIED`, `INVALID_ARGUMENT`,
`INVALID_STATE`, `BUSY`, `NOT_FOUND`, `UNAVAILABLE`). Put stale target/geometry,
injection failure, mode negotiation and generation details in bounded diagnostic
fields/events; do not invent a private error ID in NearCast, NA20 or NT10.

`cast.touchbackChanged` is the reconnect/event-loss recovery signal together with
`cast.getStatus(include=touchback)`. It must include the complete target and current
stream/epoch/token values so a sender can detect a stale cached route.

## 8. Conformance checklist

- independent two-route state and slot reuse fencing;
- local gate/controller gate combinations;
- AirPlay capability omission;
- touch preferred and mouse required negotiation;
- reset-before-close on disable, mode switch, geometry change and disconnect;
- accepted versus active/unavailable distinction;
- stale target/token and standard error mapping;
- event replay through status snapshot;
- sender/receiver restart and first-use fingerprint trust.
