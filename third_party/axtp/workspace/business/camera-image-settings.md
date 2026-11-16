# Camera Image Settings 需求

## 背景

摄像设备需要提供图像质量和成像链路参数配置，用于适配会议室光照、肤色还原、抗频闪、逆光、不同传感器和不同产品默认风格。用户希望在 App 或上位机中调整 ISO、曝光、白平衡、亮度、对比度、饱和度、锐度、WDR、防频闪等配置，并能恢复默认、监听变化和处理不支持项。

本需求用于整理成像参数类业务，不直接定义协议 schema。后续 flow 会把基础图像参数、曝光参数和白平衡参数分别映射到 `camera.image`、`camera.exposure`、`camera.whiteBalance`。

## 用户目标

用户、App 或上位机希望完成以下目标：

- 查询摄像头支持哪些图像参数、每个参数的范围、默认值、单位和是否自动控制。
- 获取当前图像配置。
- 设置亮度、对比度、饱和度、锐度、色调、图像风格或视角类基础参数。
- 设置曝光模式、ISO / gain、shutter、EV、WDR、防频闪或 power-line frequency。
- 设置白平衡模式、色温、RGB gain 或手动白平衡参数。
- 一键恢复默认图像配置。
- 设备因场景、profile、恢复默认或本地操作改变配置时，App 能刷新状态。

## 范围

本需求包含：

- `camera.image`：亮度、对比度、饱和度、锐度、色调、图像风格、视角等基础图像参数。
- `camera.exposure`：auto/manual exposure、ISO/gain、shutter、EV、WDR、防频闪、power-line frequency。
- `camera.whiteBalance`：auto/manual white balance、色温、RGB gain、白平衡锁定。
- 获取能力、获取配置、设置配置、恢复默认和配置变化同步。
- legacy Camera 配置中的字段拆分线索。

## 非目标

本需求不包含：

- PTZ、zoom、focus 或 autofocus。
- framing、gallery、speaker tracking 或区域追踪。
- 视频编码、码率、分辨率、帧率和 STREAM 数据。
- 传感器校准、工厂调试和产测。
- ISP 内部算法实现或厂商私有调参文件传输。

## 场景

- 用户打开图像设置页，App 查询支持的 image / exposure / white balance 参数和当前值。
- 用户调整亮度、对比度、饱和度、锐度，App 做本地范围校验后提交。
- 用户从自动曝光切换到手动曝光，并设置 ISO / gain、shutter 或 EV。
- 用户开启或关闭 WDR，改善逆光场景。
- 用户设置防频闪为 50Hz、60Hz 或 auto。
- 用户从自动白平衡切换到手动白平衡，并设置色温或 RGB gain。
- 用户恢复默认图像参数，设备返回最终生效配置。
- 固件 profile、场景模式或其他客户端改变图像配置，App 收到变化事件后刷新页面。
- 某些参数只在特定模式下可写，例如手动曝光下才允许写 shutter，手动白平衡下才允许写色温。

## 约束

- 图像参数应按 capability 暴露范围、默认值、单位和模式依赖，不应写死统一范围。
- 设置操作应支持部分更新，但需要说明设备是否原子应用。
- 自动模式和手动字段存在依赖关系，非法组合应返回明确错误。
- 某些图像参数可能需要重启摄像头 pipeline 才生效，flow 中需区分 immediate / pending restart。
- [REVIEW-ASK] ISO、gain、shutter、EV 的单位和枚举需要由传感器/固件确认。
- [REVIEW-ASK] VM33 Camera 配置中的 WhiteBalance / Exposure 字段需要拆到对应能力，不能全部保留在 `camera.image`。

## 旧协议线索

- `CommonSetImageStyle` / `CommonGetImageStyle` 候选映射到 `camera.setImageConfig` / `camera.getImageConfig`。
- `CommonSetSightAngle` / `CommonGetSightAngle` 候选映射到 `camera.image`，但需确认视角是否应归 lens / framing。
- `CommonSetPowerLineFreq` / `CommonGetPowerLineFreq` 候选映射到 `camera.exposure`。
- `CommonSetWdrState` / `CommonGetWdrState` 候选映射到 `camera.exposure`。
- VM33 Camera 配置中出现 `WhiteBalance.Mode`、`WhiteBalance.Temperature`、`Exposure.Mode`、`Exposure.ExposureValue` 等字段，需要拆到 `camera.whiteBalance` 和 `camera.exposure`。
- 旧分类中 `Config.MultiGet:Camera` / `Config.MultiGet:camera` 暂归 `camera.image`，但已经标记需要继续拆分。

## 开放问题

- [REVIEW-ASK] 基础图像参数的 MVP 字段集合是什么：brightness、contrast、saturation、sharpness、hue、style、sightAngle 是否都需要？
- [REVIEW-ASK] ISO 与 gain 是否使用同一字段，还是分别建模？
- [REVIEW-ASK] shutter 单位使用微秒、分母式曝光时间，还是设备枚举？
- [REVIEW-ASK] 白平衡手动模式使用 color temperature，RGB gain，还是二者都支持？
- [REVIEW-ASK] WDR 属于曝光能力还是 image enhancement 子能力？当前建议归 `camera.exposure`。

## 下一步

- 交互和协议覆盖由 `workspace/flows/camera-image-settings.md` 承接。
- Flow 识别出的协议缺口再进入 `camera.image`、`camera.exposure`、`camera.whiteBalance` 或相关 protocol 草案。
