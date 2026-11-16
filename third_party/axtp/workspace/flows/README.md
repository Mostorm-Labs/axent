# AXTP 协议交互流程

> Backend workspace: do not start here from the main docs. Use the role guides first, and open this directory only when a scenario needs interaction mapping or protocol coverage analysis.

`workspace/flows/` 存放场景级协议交互方案，是 Stage 10 `plan-protocol-flow` 的默认输出目录。

如果同名或相近主题已经有 `workspace/business/**`，business 文档只作为 PRD 源头；交互步骤、actor 责任、协议覆盖、缺口和下一步路由由 flow 文档维护。

这类文档把产品 story、UI 原型、旧协议线索和 AXTP 协议工作连接起来，回答：

- 用户或系统要完成什么目标。
- 哪些 actor 参与，例如用户、App、云端、设备、固件服务、SDK 或工具。
- 每一步需要哪些 method、event、capability、profile 或 stream。
- 哪些协议事实已经 adopted/generated，可以直接复用。
- 哪些能力缺失，需要转入 Stage 20 `draft-business-protocol`。
- 哪些已采纳协议需要语义变更，需要转入 Stage 40 `amend-adopted-protocol`。
- 哪些只是 UI-only 或业务编排行为，不应该进入协议草案。

`workspace/flows/**` 不是最终协议事实源。稳定实现合同仍然来自：

```text
contract/registry/**/*.yaml + contract/registry/domains/**/*.yaml
  -> contract/protocol/axtp.protocol.yaml
  -> contract/generated/**
```

## 什么时候用 Stage 10

当输入不是单个明确协议方法，而是一个端到端需求时，先使用 `tooling/skills/10-plan-protocol-flow/SKILL.md`：

| 输入 | Stage 10 做什么 | 后续 |
|---|---|---|
| UI 原型，例如音频算法强度滑条和恢复默认按钮 | 识别屏幕控件、读写流程、默认值、错误路径和已有协议覆盖 | 写 `workspace/flows/<scenario>.md`，缺口转 20/40 |
| 用户 story，例如“用户在 App 中切换输出布局” | 拆 actor、步骤、请求、响应、事件和状态同步 | 判断复用已有协议还是新增草案 |
| 旧协议迁移场景 | 把旧命令映射到 story 步骤，区分兼容保留和 AXTP 新增能力 | 缺口转 `workspace/protocol/**` 草案 |
| 跨端业务流程 | 标出 App、后台、设备、SDK、测试各自消费的协议事实 | 给研发和测试形成共同流程图 |

## 工作流

```text
workspace/business/<requirement>.md 或业务场景 / UI 原型 / 用户 story
  -> Stage 10 plan-protocol-flow
  -> workspace/flows/<scenario>.md
  -> Stage 20 draft-business-protocol 或 Stage 40 amend-adopted-protocol
  -> Stage 30 adopt-protocol-draft
  -> Stage 50 generate-axtp-protocol
```

## Active Flow Plans

| Flow 名称 | 文件 | 状态 | Next action |
|---|---|---|---|
| Audio Algorithm Level Control | [audio-algorithm-level-control.md](audio-algorithm-level-control.md) | 已完成 / 复用 adopted | 使用 generated `audio.algorithm`；后续语义变化走 amendment。 |
| Cast Receiver UxPlay | [cast-receiver-uxplay.md](cast-receiver-uxplay.md) | 已进入 protocol draft | 继续确认投屏会话、PIN、窗口和音频控制对应的 protocol drafts。 |
| Cast RX/TX Pairing | [cast-rxtx-pairing.md](cast-rxtx-pairing.md) | 已进入 protocol draft | 采纳或修订 `network.ap` / `network.wifi` 草案。 |
| Device Firmware Update | [device-firmware-update.md](device-firmware-update.md) | 已进入 protocol draft | 采纳 `firmware.update` / `firmware.info`，并确认 STREAM P0 边界。 |
| Device Information And System Runtime State | [device-system-info.md](device-system-info.md) | 已进入 protocol draft | 采纳 `device.info`、`device.childDevice` 和 `system.*` 草案。 |
| NearHub Launcher Digital Signage Device Management | [signage-device-management.md](signage-device-management.md) | 已进入 protocol draft | 继续确认 signage、network、firmware、log 等 legacy 映射。 |
| NA20/NT10 Device Streaming Audio And Video | [device-streaming-audio-video.md](device-streaming-audio-video.md) | 已进入 protocol draft | 采纳 `video.stream` / `audio.stream` 和媒体 STREAM profile。 |
