# Launcher AirPlay Receiver Control 需求

## 背景

Windows Launcher 集成基于 UxPlay 改造的 AirPlay 接收端。Launcher 启动后自动拉起接收服务，iOS / macOS 发射端通过 mDNS 发现 receiver 并发起投屏。

本文只记录投屏接收端控制面的业务事实和未决问题；交互路径见 `workspace/flows/cast-receiver-uxplay.md`，候选协议见 `workspace/protocol/cast/*.md`。

## 目标与范围

| 能力 | 业务要求 |
|---|---|
| Receiver / backend 状态 | UI 和外部控制端能看到 receiver、UxPlay backend 的 ready / exited / failed / restarting 状态。 |
| AirPlay 名称 | 支持读取和设置 AirPlay 显示名称；归入 `cast.session`，设置后需要重新发布 mDNS / Bonjour 名称。 |
| 投屏会话 | 支持查询当前会话、接收生命周期事件，并允许外部停止当前投屏。 |
| 接收端阶段 | 暴露统一 `receiverPhase`，用于把 AirPlay / UxPlay 和 HID/NA20 投屏都投影到固定接收阶段。 |
| PIN / 密码保护 | 支持密码保护开关、当前有效 PIN、等待输入、鉴权失败和 PIN 变化通知。 |
| 投屏音频 | 控制接收端本地是否播放投屏音频，并支持静音；默认不播放。 |
| 投屏窗口 | 控制投屏窗口置顶、全屏、还原、隐藏，并上报窗口状态变化。 |
| 本地流控 | 控制接收端本地目标渲染 fps、队列上限、late frame 阈值和丢帧策略。 |
| 快照状态 | 需要独立 `cast.getStatus`，用于一次性校准 receiverPhase / session / PIN / audio / window / backend / flowControl 摘要；不提供持续聚合状态事件。 |

## 边界

- 对外 AXTP 控制口由 Cast Receiver AXTP Adapter 承载，不直接暴露 UxPlay backend 内部控制服务。
- 外部控制口允许 LAN 访问；是否需要 token / HMAC 和 Origin 白名单由 auth 草案确认。
- `cast.*` 只覆盖接收端控制面，不标准化 AirPlay、mDNS、RAOP、H.264、AAC 或媒体帧传输。
- UxPlay 只是 backend type 或 adapter evidence，不进入公共 method 名。
- `cast.flowControl` 只控制接收端本地渲染策略，不替代通用 `stream.flowControl`。
- business 文档不分配 methodId、eventId、errorCode 或 fieldId；正式合同仍以 `contract/registry/**`、`contract/protocol/axtp.protocol.yaml` 和 `contract/generated/**` 为准。

## 业务规则

- 投屏音频播放默认关闭；打开或静音只影响接收端本地播放，不要求改变 AirPlay 媒体协商。
- 密码保护默认开启；默认 PIN 由 NearCast 或 UxPlay 内部自动生成，并允许外部控制端覆盖。
- 密码保护关闭时，发射端必须可以无密码投屏。
- 授权 response / event 可以携带明文 PIN；日志、诊断和错误摘要必须脱敏。
- `cast.*` 普通查询和控制能力先按朴素可调用状态面设计，不在本业务中拆分 per-method 权限 scope。
- 明文 PIN 可以通过授权控制通道返回；协议仍必须标明可见性，并保证日志、诊断和错误摘要脱敏。
- `receiverPhase` 是 UI、重连校准和跨协议对齐使用的接收端阶段，取值为 `idle`、`incoming`、`authenticating`、`streamStarting`、`streaming`、`rendering`、`interrupted`、`stopping`、`ended`、`failed`。
- AirPlay / UxPlay 可以把密码等待、鉴权、mirror start 和首帧渲染映射到 `receiverPhase`；HID/NA20 没有密码鉴权时应跳过 `authenticating`，以 `authRequired=false` 或等价摘要表达。
- `streaming` 只表示接收端已经有媒体数据或 downstream stream 在流动；`rendering` 必须等到首帧渲染或音频播放实际开始后才进入。
- 外部主动停止、发射端结束、backend 异常和 backend 重启都需要产生可理解的会话状态变化和原因。
- Backend 重启只重启 UxPlay backend，不等同于退出 Launcher 或 receiver runtime 重启；如存在活动投屏，会强制结束当前会话。
- 投屏窗口 `normal` 表示退出全屏、取消置顶，并恢复进入全屏 / 置顶前的窗口尺寸和位置。
- 没有活动投屏窗口时，窗口控制是返回 invalid state 还是保存预设状态，仍需在 flow / protocol 中确认。
- 本地流控不要求发射端降低输入 fps；`inputFps` 反映上游输入，`renderFps` 反映本地渲染。
- `fps=0` 表示不限速；目标 fps 高于当前输入 fps 时，接收端按输入 fps 运行。
- 默认丢帧策略为 `drop-late`；视频队列必须有上限，未渲染帧不得无限排队。
- 外部控制端不直接请求关键帧；关键帧请求由接收端内部根据丢帧、解码和 fps 变化自动触发。
- overlay / diagnostics 进入 `cast.flowControl`，但不得变成高频媒体事件。

## 开放问题

- [REVIEW-ASK] LAN 控制口是否需要 token / HMAC 和 Origin 白名单，如何绑定到 auth 草案？
- [REVIEW-ASK] AirPlay 名称长度、字符集、立即生效策略和失败恢复如何定义？
- [REVIEW-ASK] `stopSession` 在无活动会话时应返回幂等成功，还是返回 no active session？

## 参考

- `workspace/flows/cast-receiver-uxplay.md`
- `workspace/protocol/cast/*.md`
- `workspace/legacy-migration/evidence/WEBSOCKET_PROTOCOL.md`
