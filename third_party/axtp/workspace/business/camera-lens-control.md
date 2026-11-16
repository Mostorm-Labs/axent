# Camera Lens Control 需求

## 背景

摄像设备需要提供镜头和取景方向控制能力，包括物理 PTZ、电子 PTZ、zoom、autofocus、手动 focus 等。用户希望通过 App、上位机或会议控制面完成视角调整、放大缩小、自动对焦、手动对焦和预置位恢复，同时设备也可能因为 framing 算法、遥控器或本地按键改变镜头状态。

本需求用于把 PTZ / zoom / focus 这组相邻能力先作为业务需求讲清楚。后续 flow 再决定哪些步骤走 `camera.ptz`、`camera.zoom`、`camera.focus`，以及它们和 `video.framing` 的联动边界。

## 用户目标

用户、App 或上位机希望完成以下目标：

- 查询摄像头是否支持物理 PTZ、电子 PTZ、zoom、autofocus、手动对焦和 preset。
- 调整 pan / tilt / zoom 方向、绝对位置、相对步进或速度。
- 选择 optical zoom、digital zoom 或设备默认 zoom 路径。
- 启动一次自动对焦，设置连续自动对焦或切换到手动对焦。
- 设置或查询对焦位置、对焦区域、点选对焦目标。
- 保存、调用、更新或删除摄像头预置位。
- 当 framing 算法、物理按键、遥控器或旧协议改变镜头状态时，App 能同步状态。

## 范围

本需求包含：

- `camera.ptz`：物理云台 pan / tilt、PTZ preset、home / reset、限位和移动状态。
- `camera.zoom`：光学变焦、数字变焦、zoom ratio / position、zoom speed、zoom region。
- `camera.focus`：focus mode、autofocus、continuous autofocus、manual focus position、focus region、focus state。
- physical PTZ 与 electronic PTZ 的业务边界描述。
- 用户主动操作和算法联动操作的冲突处理。
- legacy 线索归类和待确认项。

## 非目标

本需求不包含：

- framing mode 的算法策略；这些属于 `video.framing`。
- ISO、曝光、白平衡、亮度、对比度等图像质量参数；这些属于 `camera.image` / `camera.exposure` / `camera.whiteBalance`。
- 视频流数据传输、编码或录制。
- 工厂校准、AF calibration 或产测流程；这些属于 `camera.calibration` / `diagnostic.*`。
- UI 动画、拖拽手势实现和遥控器按键映射细节。

## 场景

- 用户打开摄像头控制页，App 查询 PTZ / zoom / focus 能力和当前状态。
- 用户点击方向键，摄像头按指定方向和速度开始移动，松开后停止。
- 用户拖动 zoom slider，设备执行光学或数字变焦，并返回最终倍率或位置。
- 用户点击自动对焦，设备执行一次 AF，完成后上报 focused 或 failed。
- 用户选择点选对焦或区域对焦，设备按归一化坐标设置对焦目标。
- 用户切换到手动对焦，并设置绝对 focus position。
- 用户保存当前 PTZ/zoom/focus 为 preset，之后一键恢复。
- framing 区域追踪 over physical PTZ 接管镜头移动时，手动 PTZ 操作被拒绝或临时暂停算法。
- 设备被隐私遮挡、摄像头未打开、校准中或被另一个控制端占用时，App 收到明确错误或不可用状态。

## 约束

- 物理 PTZ 会改变摄像头机械位置；电子 PTZ 通常只改变裁切区域，不能假装物理位置已改变。
- `camera.ptz` 中的 zoom 只表示 PTZ 三元状态中的 z 轴历史兼容字段；独立 zoom 配置和倍率应归 `camera.zoom`。
- zoom/focus 的数值范围高度依赖设备，应通过 capability 暴露，不应假设统一为 `0..100`。
- 手动 PTZ、区域追踪、autoframing 和 speaker tracking 可能互斥，需要定义优先级和抢占策略。
- 自动对焦通常是异步动作，调用成功不代表已经对焦完成。
- [REVIEW-ASK] preset 是否包含 focus、zoom、framing mode 和 exposure 等其他状态，需要产品确认。

## 旧协议线索

- `CommonSetPanTiltZoom` / `CommonGetPanTiltZoom` 当前分类到 `camera.zoom`，但名称含 pan/tilt/zoom，需确认是否应拆到 `camera.ptz` + `camera.zoom`。
- `CommonGetPositionNumberJson` / `CommonSetPositionNumberJson` 候选归 `camera.ptz` preset，但需确认 position number 是 PTZ preset 还是房间座位编号。
- VM33 `Focus.ManualZoom`、`Focus.SetZoomSpeedMode`、`Focus.GetZoomSpeedMode`、`Focus.DigitalZoom`、`Focus.OpticsZoom`、`Focus.GetZoomInfo` 候选归 `camera.zoom`。
- AXDP `CommonSetAutoFocusState` / `CommonGetAutoFocusState`、`CommonSetManualFocusPosition` / `CommonGetManualFocusPosition` 候选归 `camera.focus`。
- VM33 `Focus.SetMode`、`Focus.GetMode`、`Focus.Manual`、`Focus.SetFocus`、`Focus.SetFocusRegion` 候选归 `camera.focus`。

## 开放问题

- [REVIEW-ASK] 设备是否同时支持 physical PTZ 与 electronic PTZ？二者能力如何命名和展示？
- [REVIEW-ASK] `CommonSetPanTiltZoom` 的 pan/tilt/zoom payload 是否足够拆成 PTZ 与 zoom 两类正式语义？
- [REVIEW-ASK] preset 是否是纯 PTZ preset，还是 lens scene preset？
- [REVIEW-ASK] 自动对焦触发后是否必须产生事件，还是客户端轮询 `getFocusState`？
- [REVIEW-ASK] 多控制端同时操作时，冲突策略是拒绝、抢占、排队还是 last-write-wins？

## 下一步

- 交互和协议覆盖由 `workspace/flows/camera-lens-control.md` 承接。
- Flow 识别出的协议缺口再进入 `camera.ptz`、`camera.zoom`、`camera.focus` 或相关 protocol 草案。
