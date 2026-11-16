# Video Framing Modes 需求

## 背景

会议摄像设备需要支持多种图像输出和构图模式，用来适配会议室、多人协作、发言人突出、区域追踪和 gallery 展示等场景。现有设备和旧协议中已经出现全景模式、智能自动取景、区域追踪、gallery、speaker tracking 等能力，但这些能力的业务边界、模式枚举、互斥关系和配置项还需要先在业务层整理清楚。

本需求只描述用户和产品期望，不作为协议实现合同。后续 flow 会把场景拆成 AXTP 交互步骤，协议草案仍由 `workspace/protocol/video/video.framing.md` 等文档承接。

## 用户目标

用户、App 或上位机希望在设备的视频设置页中完成以下目标：

- 查看当前设备支持哪些构图模式。
- 在全景、自动取景、区域追踪、gallery、无上边条 gallery、speaker tracking 等模式之间切换。
- 按模式配置必要参数，例如追踪目标区域、PTZ 承载方式、gallery 显示样式、speaker tracking 延迟或策略。
- 设备本地策略、遥控器、物理按键或旧协议改变模式后，App 能同步看到最新状态。
- 模式不可用、被隐私遮挡或被其他视频功能占用时，App 能给出明确反馈。

## 范围

本需求包含：

- 构图模式管理：全景模式、auto framing 智能模式、区域追踪、gallery、无上边条 gallery、speaker tracking。
- 区域追踪的承载方式：over physical PTZ 与 over electronic PTZ。
- 模式级配置：tracking 区域、策略、启停、默认模式、模式切换后的运行状态。
- 设备主动变化同步：本地按键、设备策略、恢复默认、旧协议 adapter 或自动策略改变后的状态刷新。
- legacy 线索归类到 `video.framing`，不在本阶段固化旧 payload。

## 非目标

本需求不包含：

- 视频编码、分辨率、码率、帧率编码参数；这些属于 `video.encoder` 或 `video.stream`。
- 输出画面布局、拼接、PIP、多窗口路由；这些更偏 `video.layout` / `output.layout`。
- PTZ 机械控制、云台 preset、镜头 zoom/focus；这些属于 `camera.ptz`、`camera.zoom`、`camera.focus`。
- 视频帧或预览数据传输；连续媒体数据必须走 `video.stream` / STREAM。
- UI 视觉样式和具体文案。

## 场景

- 用户打开视频构图设置页，App 查询设备支持的 framing modes 和每种模式的可配置项。
- 用户从全景模式切换到 auto framing，设备根据人数和画面内容自动裁切。
- 用户选择区域追踪 over physical PTZ，App 设置追踪区域，设备通过物理 pan/tilt/zoom 追踪目标。
- 用户选择区域追踪 over electronic PTZ，设备在传感器画面内做电子裁切追踪，不驱动物理云台。
- 用户切换到 gallery 模式，设备输出多人分格画面。
- 用户切换到无上边条 gallery 模式，设备输出无顶部信息条的 gallery 画面。
- 用户启用 speaker tracking，设备根据说话人或音频定位切换构图目标。
- 设备本地按键、遥控器或旧协议改变构图模式，App 收到变化后刷新页面。
- 当前模式被隐私遮挡、低带宽、摄像头占用或不支持的硬件状态限制，App 展示不可用原因。

## 约束

- framing 是视频业务控制面，不是 AXTP core Frame Header。
- 模式切换应有明确互斥策略，同一时刻只能有一个主 framing mode 生效，除非产品确认组合模式。
- 区域追踪 over physical PTZ 可能与 `camera.ptz` 状态互相影响，需要在 flow 中标出边界。
- 区域追踪 over electronic PTZ 可能影响输出裁切，但不应直接改变物理 PTZ 坐标。
- speaker tracking 依赖音频定位或说话人检测，算法是否可用应通过能力或状态表达。
- gallery 与 `video.layout` 的边界需要确认：gallery 是构图算法模式，还是通用布局模板。
- [REVIEW-ASK] 各模式的正式英文枚举值、默认模式、切换延迟、是否持久化和恢复默认策略需要产品与固件确认。

## 旧协议线索

- `CommonSetVideoMode` / `CommonGetVideoMode` 候选映射到 `video.setFramingMode` / `video.getFramingMode`。
- `CommonSetVideoTrackMode` / `CommonGetVideoTrackMode` 候选映射到 framing mode 查询与设置。
- `CommonSetSpeakerTrackDelay` / `CommonGetSpeakerTrackDelay` 与 speaker tracking 配置有关。
- `CommonSetRegionTracking` / `CommonGetRegionTracking` 与区域追踪有关。
- `CommonSetHorTrackingStrategy` / `CommonGetHorTrackingStrategy`、`CommonSetVerTrackingStrategy` / `CommonGetVerTrackingStrategy` 与追踪策略有关。
- `CommonSetStartupPosition` / `CommonGetStartupPosition` 与默认构图或启动位置有关。
- VM33 `Config.MultiSet:Video` 中 `Video.mode=auto-framing` 属于 framing 线索。

## 开放问题

- [REVIEW-ASK] 全景、auto framing、区域追踪、gallery、无上边条 gallery、speaker tracking 的正式模式枚举和 UI 显示名是什么？
- [REVIEW-ASK] 区域追踪 over physical PTZ 与 over electronic PTZ 是否是同一 mode 的 `ptzBinding` 参数，还是两个独立 mode？
- [REVIEW-ASK] gallery 与无上边条 gallery 是否只差 UI overlay，还是算法/布局完全不同？
- [REVIEW-ASK] speaker tracking 是否依赖独立音频算法能力，设备不可用时应返回什么原因？
- [REVIEW-ASK] 模式切换是否需要异步 apply 状态，例如 `applying`、`active`、`failed`？

## 下一步

- 交互和协议覆盖由 `workspace/flows/video-framing-modes.md` 承接。
- Flow 识别出的协议缺口再进入 `video.framing` 或相关 protocol 草案。
