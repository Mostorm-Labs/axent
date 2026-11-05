---
status: business-input
contract: false
generated: false
domain: cast
feature: cast.touchback
registry: ""
lastReviewed: 2026-11-05
---

# NearCast Typed Input 与呈现控制：业务需求 R2

## 1. 背景、范围与协议入口

TX 将采集画面传给 NearCast RX，用户希望在对应画面直接操作 TX。输入从 RX
返回当前画面的输入终点；不能根据一个固定 Touch/Mouse 配置猜测事件语义。
产品继续支持稳定画布、目标授权和逐路开关；输入事件类型与 TX 输出策略分开。

本轮重写旧方向，完整协议见 [input.stream R2](../protocol/input/input.stream.md)，
产品控制见 [cast.touchback](../protocol/cast/cast.touchback.md)，逐步执行见
[WebCast/HID flow](../flows/nearcast-touchback-na20-nt10.md)。本文为用户确认的业务输入，
详细字段、帧承载与状态规则已同步到正式 specs/registry；实现仍须独立验收。

协议设计包含 Touch 和 Mouse；初版 RX 生产采集只交付真实 Touch。扩展 Mouse
采集后，同一允许两类型的输入流可以交错收发，无需反复协商整流模式。

## 2. 用户目标和成功定义

| 用户动作 | 应看到的行为 | 成功证据 |
|---|---|---|
| 发现并选择 RX | 展示可连接候选、现场投屏码和首次信任提示 | 正确设备与媒体来源关联；发现不自动授权 |
| 对某一路开启反控 | 双门控满足后准备输入，显示等待/生效/失败原因 | 当前目标的 open 成功且本机能力/坐标就绪 |
| 单指或多指操作画面 | 一次采样的多指一起发给对应 TX | 真正 Touch 应用事件，contact 生命周期不串路 |
| 后续换用鼠标 | 发 Mouse 事件，活动 Touch 不被隐式取消 | 两类状态独立；不猜 buttons=0 的含义 |
| 关闭/断链/失效 | 停止新输入，释放本 owner 状态 | 已释放或明确 cleanup blocked，不假报成功 |
| 平台不支持原生类型 | 明确拒绝该能力，媒体继续 | 可见原因，无隐式 Mouse/Touch 回退 |

injectionCount、RPC accepted、DataChannel open 都不是应用实际消费输入的证明。
验收同时保留两端合同指纹/提交、本机提交结果和目标应用观察记录。

## 3. 产品对象与归属

每路 target 使用 sessionId/sessionGeneration/slotId/bindingGeneration/
lifecycleGeneration，避免 slot 复用把旧事件送到另一来源。RX 画布、布局和物理
输入设备身份保持本地；TX 保存自己验证过的授权来源和本机坐标映射。

同一认证 session 中 input streamId 不复用。影响输入坐标语义的 TX 来源变化
先使旧流失效，清理后新开流。删除 wire epoch/geometryToken 不等于删除本地
generation、不可变绑定、旧任务隔离和关闭责任。

| 所有者 | 责任 |
|---|---|
| RX target owner | 本地授权、控制端 intent、目标、采样去重、画布映射和统一 gate |
| TX 输入引擎 | 验证来源/类型/序号、坐标映射、串行队列、native 状态及释放 |
| Runtime/Axent | 标准 wire/session 机制及不泄漏 Runtime 细节的产品合同 |
| NA20 | 按通用 session/stream 路由中继，不能识别/改写输入记录 |
| NT10 | 实际 native/HID 能力、输入校验、报告生成和本地失效清理 |

初版每个 native 输入域只允许一个控制 owner；其它路仍可投屏，竞争反控返回 BUSY。
未来多 owner 必须另行解决全局鼠标、按钮、修饰键和接触池的所有权。

## 4. 统一开关与授权

receiverOptIn 是默认关闭且不持久化的现场许可，controllerRequested 是控制端
intent；两者进入同一个 owner lane。AirPlay 不声明本反控能力，也不打开输入流。
同一路暂不可用要报告原因，不影响其视频/音频或其它路。

启用步骤是：双方授权 → 当前目标/来源绑定 → 承载 APP_READY → 软件能力交集
→ native/坐标准备 → open 成功 → 当前 RX 请求仍有效 → effective=true。
任一 gate 关闭立即停止新采集/入队，清理异步执行；effective=false 不证明已释放。
旧 pending open 晚成功不能重新激活；它只能清理其旧 generation 的资源。

RX 不再提供 Touch/Mouse 或 preferred/required 的输入模式开关。TX 当前仅开放
native 输出；未来显式转换为有损的指针兼容策略，不能许诺所有输入都无损转换。
切换输出策略也先停止、释放并隔离旧队列，再接受新生命周期。

## 5. 采样、设备和手势规则

Touch 每条 7B，data 为 1B inputType 加 7×N。同一次采样的多指同包，共享 cursor；
本轮采用完整采样集合，包含静止活动指和本次 Up/Cancel，终止指在下一采样移除。
每包共享一个采样时间；缺失活动 ID 不是隐式 Up，而是不合法的采样。

物理 pointer ID 到逻辑 contactId 的映射由 RX 维护，Down 到 Up/Cancel 期间不变。
触点绑定最初命中的目标；跨画面拖动不能中途换 target。采集开启前已按住的
手指不补造远端 Down；只接纳开启后的新生命周期。触摸产生的兼容 Mouse 事件
必须去重，Pen 不隐式当作初版 Touch。

设备移除或捕获丢失时显式 Cancel 其接触；其它手指仍在完整采样集合。无新采样
不发包，不能因静止或另一类型到达自动 Up。Mouse 本版由 RX 合成一只逻辑鼠标，
按钮/滚动/修饰键有独立生命周期；不将 Touch Cancel 扩散到 Mouse。

## 6. 呈现、几何和质量

RX 负责去 letterbox、按内容归一化；TX 按自己保存的采集/输入区域恢复本机位置。
不能拿客户端自报窗口句柄/屏幕尺寸作为已验证几何。初版先验证主屏全屏；副屏、
跨 DPI、窗口/区域、旋转等需独立映射证据才能扩大支持。

RX 纯布局缩放在坐标含义不变时本地处理；受影响触点明确取消。TX 来源、区域、
方向/单位或授权改变时先清理旧流，再应用新绑定并开新 ID。接受重建 intent
不等于新几何已应用；失败不能让旧队列在新区域执行。

inherit/prefer1080p 是独立源质量偏好。TX 可拒绝或降级，RX 必须展示实际结果；
放大画布不等于编码升级。质量和几何参数不能重新引入整流 Touch/Mouse 模式。

## 7. 承载与异常责任

WebCast input 必须补齐完整 Standard Frame，当前裸 STREAM 不能自动获得 CRC。
媒体继续走既有 RTP/音视频链路，反控关闭不得重启媒体。使用 AXTP-WEBRTC-DC 复用 control DC 的
binary Standard Frames，产品 text 消息仍按原有分流；完整 profile/session 见输入规范。

HID 路径复用同一 input data，逐跳 frame 可以按通用规则重建，但语义不变。
NA20 不能把“已转发”当作 NT10 已释放；终点断线需自行释放。本版中继保持端到端逻辑 context；独立 RPC session 的桥接不在本版支持范围，
不能在 Touch parser 内临时翻译 stream ID。

未知/未允许类型、跳序、溢出、几何失效或 native 部分失败使当前输入停止并清理。
只记录可确认提交前缀，不重放输入；释放不明保留 blocked，不能新初始化绕过。
控制重试幂等且有预算；接收、校验、提交、应用消费的指标分别记录。

## 8. 非目标与验收边界

- 不增加 AirPlay 输入、不通过提权绕过安全桌面、不改媒体重试和产品隐私策略。
- 首版不交付 Mouse 采集、强制转换、通用键盘/文本/Pen、相对鼠标或多逻辑鼠标。
- 不将静态样例、单元测试或历史 Mouse/Scroll 实测冒充新 Touch 真机验收。
- 业务/协议全文按 R2 同步采纳；旧当前正文撤下，历史由 Git e986dcf 追溯。

首版验收覆盖单/多指、同采样终态、两路目标隔离、双方开关、关闭/重开、断线、
槽复用、权限/几何失效、清理失败、AirPlay 排除和媒体持续。Mouse 协议的独立
decoder/conformance 与后续真实采集验收分开记录。
