---
status: business-input
contract: false
generated: false
domain: cast
feature: cast.touchback
registry: ""
lastReviewed: 2026-11-03
---

# NearCast Touchback and Presentation Canvas

## 背景

NearCast 的 WebCast 投屏已经能够把发送端画面显示在接收端，但用户仍需
手动查找投屏码，而且接收端无法把画面上的点按、拖动和滚轮动作传回发送端。
后续 HID 投屏还需要把同一套输入语义从 NearCast 传到 NA20，再由 NA20 原样
透传到 NT10，最后在 NT10 上转换为 Windows HID 输入。

本需求同时覆盖两类控制：

1. presentation canvas：为每一路投屏提供稳定的画布和几何版本，让发送端的
   坐标在布局变化时不会落到另一条投屏；
2. touchback：为每一路投屏独立开关反向控制，并协商 `touch` 或 `mouse` 输入
   语义。

设备发现只用于找到候选接收端和展示投屏码。mDNS 不授予投屏或反控权限；首次
   连接仍需用户确认设备指纹，之后固定信任该设备。

## 用户目标

- Launcher/Tauri sender 在局域网内发现 NearCast 接收端后，直接选择目标并让
  接收端悬浮窗显示 WebCast code。
- 用户能够在接收端本地打开或关闭 touchback；Launcher 也能够只控制选中的
  投屏路。
- 发送端优先使用 `touch`；用户或控制面明确选择时可以使用 `mouse`。长按、
  拖选文本等产品手势由 NearCast 接收端在 `mouse` 模式下转换为按钮语义。
- AirPlay 投屏不声明 `cast.touchback`，也不发送 touchback 数据。
- WebCast 和后续 HID 使用相同的 AXTP `input.stream` 语义；只有承载适配器
  不同，NA20 不解释事件，NT10 才负责 HID 注入。

## 范围

- 接收端每路 target 的双门控、模式协商、画布/几何版本和安全释放。
- WebCast WebRTC control DataChannel 上的二进制 TouchBatch。
- NA20 的 AXTP HID relay 和 NT10 的 Touch-to-HID 终点职责。
- WebCast 首版的单路、双路、重连、画布变化和 AirPlay 排除验收。

## 非目标

- 不在 AXTP Core 中实现长按右键、拖选文本等产品手势。
- 不通过提权绕过 Windows UAC 或安全桌面。
- 不让 NA20 修改坐标、按钮、phase 或自行识别手势。
- 不为 AirPlay 增加点触通道。
- 不把 mDNS、投屏码或设备信任写入 `input.stream` 数据帧。

## 参与方和成功条件

| 参与方 | 责任 | 成功条件 |
|---|---|---|
| Tauri sender/Launcher | 发现、指纹确认、选择 target、控制门控、协商 stream、接收 touchback 并执行 Windows 输入注入 | 只接受已认证且绑定的 target；能区分 accepted、active 和 unavailable |
| NearCast receiver | 产品策略、逐路 owner、canvas/geometry、本地输入采集、WebCast adapter、reset/release | 触控数据只发往对应 source；任一路关闭或失步都不会影响其他路，并且不会产生卡住的按钮 |
| Axent/AXTP | 注册控制面和通用 input stream 事实 | 两种承载使用同一 method/schema/stream 语义 |
| NA20 | AXTP HID relay | 顺序透传 envelope，断链时释放，不改写输入含义 |
| NT10 | Touch-to-HID endpoint | 校验并映射坐标，生成 HID 报告，错误时安全释放 |

成功是一次完整的端到端行为：控制面确认目标和能力，stream 在正确 epoch
中打开，接收端发出的 down/move/up 或 mouse button/wheel 事件在正确的发送端
设备上生效，
关闭、断链、失步和布局变化都能收敛到 released 状态。

## 关键约束

- 每个 presentation target 独立维护 `receiverOptIn`、`controllerRequested`、
  `effectiveEnabled`、streamId、epoch、canvasToken 和 geometryToken。
- `effectiveEnabled` 只有在本地开关、控制端请求、目标仍存活、能力支持且
  协议不是 AirPlay 时才为 true。
- `touch` 是默认首选；`mouse` 必须通过控制面显式要求或在 open 协商中被
  明确声明支持。切换模式必须 reset、close、open 并建立新 epoch。
- Touch stream 使用标准 AXTP 16 字节大端 STREAM header；不创建第二套
  WebCast 专用 stream header。
- transport 使用 ordered + reliable；不发送逐帧应用 ACK。sequence、epoch、
  token、CRC 和 reset 提供失步检测及安全恢复。
- `down`、`up`、`cancel` 必须保持顺序；`move` 可以合并。队列溢出时丢弃
  move 并 reset，不能静默丢失释放事件。

## 场景

1. 发现并绑定：sender 发现 NearCast 服务，用户确认指纹，接收端返回 target
   和能力；sender 选择一路并请求显示 code。
2. 开启 touchback：接收端打开本地开关，sender 对同一 target 设置控制面
   gate，双方状态分别显示，只有双门都开才 open stream。
3. WebCast 输入：sender 通过 control DataChannel 发送 TouchBatch；receiver
   根据 stream 和 geometry fence 接受、转换或丢弃。
4. HID relay：同一 TouchBatch 语义进入 NA20，再透传至 NT10；NT10 映射到
   本地 HID 坐标和按钮。
5. 安全停止：用户关闭、失焦、重连、画布重建、目标复用、CRC/sequence 错误
   都触发 reset/release，并关闭旧 epoch。

## 下一步

- 交互步骤见 `workspace/flows/nearcast-touchback-na20-nt10.md`。
- 控制面和画布见 `workspace/protocol/cast/cast.presentation.md` 与
  `workspace/protocol/cast/cast.touchback.md`。
- 数据面见 `workspace/protocol/input/input.stream.md`。
- AXTP 机器事实仍以 `contract/registry/**` 和生成产物为准。
