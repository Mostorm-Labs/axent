# Method 候选规划表

> 本附录为非规范性内容。
> 本附录保留历史规划表和候选 registry 条目。
> Runtime 实现不得将本附录视为实现合同。
> 已采纳条目必须以 registry YAML 和 generated protocol 产物为准。

来源文档： `../02-Methods-Registry.md`

## 正式 MethodId 注册表

版本：v1.1
状态：MethodId 注册表与当前生成集合（精简版）
适用范围：AXTP RPC MethodId 分配、Domain 分段、已采纳方法集合、老协议适配

---
### 1. MethodId 的位置

`methodId` 只出现在：

```text
PayloadType = RPC
rpcOp = REQUEST / RESPONSE
```

典型二进制 RPC 结构：

```text
AXTP Frame Header
  payloadType = RPC

RPC Payload
  rpcEncoding = JSON / CBOR / MSGPACK / JSON_BINARY
  rpcOp       = REQUEST / RESPONSE / EVENT / BATCH
  requestId
  methodOrEventId
  statusCode
  bodyEncoding
  body
```

其中：

```text
REQUEST / RESPONSE:
  methodOrEventId = methodId

EVENT:
  methodOrEventId = eventId
```

---

### 2. MethodId 基本规则

- `methodId` 使用 `uint16`，线上编码为 Big-Endian / network byte order
- 同一 Registry 中 `methodId` 不得重复，`methodName` 不得重复
- 进入 `stable` 状态后不得改变语义；语义变化时新增方法（如 `audio.setAlgorithmConfigV2`），不修改旧方法
- 方法名格式：`domain.verbFeatureNoun`，feature 归属遵循 `specs/2-registry/01-Naming-and-Taxonomy.md`
- 推荐动词：`get / set / list / open / close / start / stop / begin / end / verify / apply / abort / resume / subscribe / unsubscribe`
- 配置、状态、动作、流、导出必须使用不同模板；`stream.*` 不定义常规业务建流方法

---

### 3. MethodId Domain 分段

| 范围 | Domain | 说明 | MVP |
|---:|---|---|---:|
| `0x0000-0x00FF` | reserved | 保留 | 否 |
| `0x0100-0x01FF` | `device.*` | 设备基础信息与生命周期 | 是 |
| `0x0200-0x02FF` | `capability.*` | 能力查询与能力协商 | 是 |
| `0x0300-0x03FF` | `system.*` | 系统控制：重启、时间、重置、功耗 | 是 |
| `0x0400-0x04FF` | `firmware.*` | 固件更新 / 固件升级控制面 | 是 |
| `0x0500-0x05FF` | `stream.*` | STREAM 控制面 | 是 |
| `0x0600-0x06FF` | `display.*` | 显示控制：亮度、分辨率、旋转、布局、输入源 | 是 |
| `0x0700-0x07FF` | `camera.*` | 摄像头：变焦、追踪、镜像、帧率、图像参数 | 可选 |
| `0x0800-0x08FF` | `video.*` | 视频编码与输出控制 | 可选 |
| `0x0900-0x09FF` | `audio.*` | 音频控制 | 可选 |
| `0x0A00-0x0AFF` | `input.*` | 输入、KVM、HID Raw 控制 | P1 |
| `0x0B00-0x0BFF` | `output.*` | 输出源、输出路由与输出布局 | P2 |
| `0x0C00-0x0CFF` | `room.*` | 会议室 / 协作空间业务 | P2 |
| `0x0D00-0x0DFF` | `signage.*` | 数字标牌业务 | P2 |
| `0x0E00-0x0EFF` | `network.*` | 网络配置 | P2 |
| `0x0F00-0x0FFF` | `storage.*` | 存储管理 | P2 |
| `0x1000-0x10FF` | `file.*` | 文件传输控制面 | P1 |
| `0x1100-0x11FF` | `log.*` | 日志控制面 | P1 |
| `0x1200-0x12FF` | `diagnostic.*` | 诊断、产测、链路测试 | P1 |
| `0x1300-0x13FF` | `sensor.*` | 传感器控制 | P2 |
| `0x1400-0x14FF` | `auth.*` | 认证与访问控制 | P2 |
| `0x1500-0x15FF` | `privacy.*` | 隐私遮挡与隐私状态 | P2 |
| `0x1600-0x16FF` | `software.*` | 软件对象运行配置 | P2 |
| `0x7000-0x7FFF` | `vendor.*` | 厂商私有方法 | 按需 |
| `0x8000-0xFFFF` | reserved | 保留，不用于 MethodId/EventId | 否 |

---

### 4. Method 条目字段

```yaml
id: 0x0902
name: audio.setAlgorithmConfig
kind: method
status: stable
domain: audio
description: Partially update audio algorithm configuration.
rpc:
  request: true
  response: true
  event: false
  supportedEncodings:
    - JSON
    - JSON_BINARY
  supportedBodyEncodings:
    - TLV8
schema:
  params: AudioSetAlgorithmConfigRequest
  result: AudioSetAlgorithmConfigResponse
errors:
  - SUCCESS
  - INVALID_ARGUMENT
  - NOT_SUPPORTED
  - BUSY
events:
  - audio.algorithmConfigChanged
capability:
  required:
    - audio.algorithm
legacy:
  cmdValue: null
  source: null
  payloadMapping: null
version:
  since: 1.0.0
  deprecated: null
mvp: true
priority: P0
```

| 字段 | 必填 | 说明 |
|---|---:|---|
| `id` | 是 | `uint16` 方法编号 |
| `name` | 是 | 方法名 |
| `kind` | 是 | 固定为 `method` |
| `status` | 是 | `draft / mvp / stable / deprecated / reserved` |
| `domain` | 是 | 所属业务域 |
| `description` | 是 | 方法说明 |
| `rpc.supportedEncodings` | 是 | 支持的 RPC body 编码 |
| `schema.params` | 是 | 请求参数 Schema（registry YAML 中对应 `requestSchema`） |
| `schema.result` | 是 | 响应结果 Schema（registry YAML 中对应 `responseSchema`） |
| `errors` | 是 | 可能返回的错误码 |
| `events` | 否 | 可能触发的事件 |
| `capability.required` | 否 | 调用前需要满足的能力 |
| `legacy` | 否 | 老协议映射 |
| `mvp` | 是 | 是否属于 MVP |
| `priority` | 是 | `P0 / P1 / P2` |

---

### 5. 当前生成方法集合

当前生成方法表以 `contract/registry/domains/<domain>/domain.yaml` 和按需存在的 `contract/registry/method/method_registry.yaml` 为事实源。当前业务方法只包含已采纳的 `audio.algorithm`；其他方法即使出现在规划表或草案中，也不得视为当前实现合同。

| methodId | methodName | Domain | 优先级 | 说明 |
|---:|---|---|---|---|
| `0x090D` | `audio.getAlgorithmCapabilities` | audio | P0 | 查询音频算法字段、范围、默认值和更新策略 |
| `0x0901` | `audio.getAlgorithmConfig` | audio | P0 | 查询当前音频算法配置 |
| `0x0902` | `audio.setAlgorithmConfig` | audio | P0 | 部分更新音频算法配置 |
| `0x090E` | `audio.resetAlgorithmConfig` | audio | P0 | 恢复全部、指定算法或指定字段默认值 |

说明：事件本身使用 EventId，通过 `rpcOp = EVENT` 承载。事件订阅集合由 RPC `IDENTIFY / REIDENTIFY` 的 `eventMasks` 字段声明和更新。无业务即无协议；未采纳草案不得进入本表或 generated 产物。

---

### 6. 完整 MethodId 规划表

以下表格是正式 MethodId 规划表，用于保留编号空间和约束可采纳能力；当前实现状态以 `contract/registry/domains/<domain>/domain.yaml`、按需存在的 `contract/registry/method/method_registry.yaml` 及生成产物为准。

#### 6.1 device.*

| methodId | methodName | 状态 | MVP | 说明 |
|---:|---|---|---:|---|
| `0x0101` | `device.getInfo` | draft | 否 | 待 `device.info` 草案采纳后才可生成 |
| `0x0102` | `device.getIdentity` | draft | 否 | 获取设备身份字段 |
| `0x0103` | `device.getState` | draft | 否 | 获取设备状态 |
| `0x0104` | `device.getInventory` | draft | 否 | 获取设备库存/子模块信息 |
| `0x0105` | `device.setIndicatorConfig` | draft | 否 | 设置指示灯/蜂鸣提示配置 |
| `0x0106` | `device.getIndicatorConfig` | draft | 否 | 获取指示灯/蜂鸣提示配置 |
| `0x0107` | `device.getPowerState` | draft | 否 | 获取电源状态 |
| `0x0108` | `device.setPowerConfig` | draft | 否 | 设置设备电源配置 |
| `0x0109` | `device.getChildDeviceState` | draft | 否 | 获取子设备状态 |

#### 6.2 capability.*

| methodId | methodName | 状态 | MVP | 说明 |
|---:|---|---|---:|---|
| `0x0201` | `capability.getSupportedMethods` | draft | 否 | 如需全局能力发现，必须先完成 capability 草案采纳 |
| `0x0202` | `capability.getRegistry` | draft | 否 | 获取完整能力注册表摘要 |
| `0x0203` | `capability.getDomainRegistry` | draft | 否 | 获取指定 domain 的能力摘要 |
| `0x0204` | `capability.hasMethod` | draft | 否 | 查询是否支持某方法 |
| `0x0205` | `capability.getLimits` | draft | 否 | 获取传输与资源限制 |
| `0x0206` | `capability.negotiate` | draft | 否 | 业务能力协商 |

协议能力优先通过 CONTROL OPEN / ACCEPT 协商；业务能力通过已采纳业务查询方法或后续采纳的 `capability.*` RPC 查询。

#### 6.3 system.*

| methodId | methodName | 状态 | MVP | 说明 |
|---:|---|---|---:|---|
| `0x0301` | `system.reboot` | draft | 否 | 重启设备 |
| `0x0302` | `system.getTimeConfig` | draft | 否 | 获取系统时间与时区配置 |
| `0x0303` | `system.setTimeConfig` | draft | 否 | 设置系统时间与时区配置 |
| `0x0304` | `system.reset` | draft | 否 | 执行系统级重置 |
| `0x0305` | `system.getLifecycleState` | draft | 否 | 获取生命周期状态 |
| `0x0306` | `system.startInitialization` | draft | 否 | 启动初始化流程 |
| `0x0307` | `system.getLicenseState` | draft | 否 | 获取系统级 license 状态 |

#### 6.4 firmware.*

固件升级候选方案采用 RPC 控制面 + STREAM 固件更新数据面；正式方法必须等待 `firmware.update` 草案采纳。

| methodId | methodName | 状态 | MVP | 说明 |
|---:|---|---|---:|---|
| `0x0401` | `firmware.getUpdateCapabilities` | draft | 否 | 获取固件更新能力 |
| `0x0402` | `firmware.beginUpdate` | draft | 否 | 开始固件更新 |
| `0x0403` | `firmware.commitUpdateBatch` | draft | 否 | 提交固件更新数据批次 |
| `0x0404` | `firmware.verifyUpdatePackage` | draft | 否 | 校验固件更新包 |
| `0x0405` | `firmware.installUpdate` | draft | 否 | 安装固件更新 |
| `0x0406` | `firmware.cancelUpdate` | draft | 否 | 取消固件更新 |
| `0x0407` | `firmware.rollbackUpdate` | draft | 否 | 回滚固件更新 |
| `0x0408` | `firmware.getUpdateState` | draft | 否 | 获取固件更新状态 |
| `0x0409` | `firmware.getUpdateTransferState` | draft | 否 | 获取固件更新传输状态 |
| `0x040A` | `firmware.confirmUpdate` | draft | 否 | 确认固件更新结果 |

#### 6.5 stream.*

`stream.*` 只定义公共流控和数据面运行时能力，不按文件、固件、媒体、日志等业务类型建流。

| methodId | methodName | 状态 | MVP | 说明 |
|---:|---|---|---:|---|
| `0x0501` | `stream.getFlowControlState` | draft | 否 | 获取公共流控状态 |
| `0x0502` | `stream.updateWindow` | draft | 否 | 更新流控窗口 |
| `0x0503` | `stream.pause` | draft | 否 | 暂停指定 streamId 的数据面 |
| `0x0504` | `stream.resume` | draft | 否 | 恢复指定 streamId 的数据面 |
| `0x0505` | `stream.getStats` | draft | 否 | 获取 STREAM 统计 |

#### 6.6 display.*

`display.*` 只保留显示设备自身能力；输出源、路由和布局归 `output.*`。

| methodId | methodName | 状态 | MVP | 说明 |
|---:|---|---|---:|---|
| `0x0601` | `display.getBrightness` | draft | 否 | 待 display 草案采纳后才可生成 |
| `0x0602` | `display.setBrightness` | draft | 否 | 待 display 草案采纳后才可生成 |
| `0x0603` | `display.getBrightnessCapabilities` | draft | 否 | 获取亮度范围、步进和模式能力 |
| `0x0604` | `display.getBrightnessConfig` | draft | 否 | 获取亮度配置 |
| `0x0605` | `display.setBrightnessConfig` | draft | 否 | 设置亮度配置 |
| `0x0606` | `display.resetBrightnessConfig` | draft | 否 | 重置亮度配置 |
| `0x0607` | `display.getPowerState` | draft | 否 | 获取显示电源状态 |
| `0x0608` | `display.setPowerConfig` | draft | 否 | 设置显示电源配置 |

#### 6.7 camera.*

`camera.*` 定义摄像头控制面。视频帧数据由业务域建流方法创建并通过 STREAM 承载。

| methodId | methodName | 状态 | MVP | 说明 |
|---:|---|---|---:|---|
| `0x0701` | `camera.getImageConfig` | draft | 否 | 获取图像参数配置 |
| `0x0702` | `camera.setImageConfig` | draft | 否 | 设置图像参数配置 |
| `0x0703` | `camera.getExposureConfig` | draft | 否 | 获取曝光配置 |
| `0x0704` | `camera.setExposureConfig` | draft | 否 | 设置曝光配置 |
| `0x0705` | `camera.getWhiteBalanceConfig` | draft | 否 | 获取白平衡配置 |
| `0x0706` | `camera.setWhiteBalanceConfig` | draft | 否 | 设置白平衡配置 |
| `0x0707` | `camera.getFocusState` | draft | 否 | 获取对焦状态 |
| `0x0708` | `camera.triggerAutoFocus` | draft | 否 | 触发自动对焦 |
| `0x0709` | `camera.getZoomState` | draft | 否 | 获取变焦状态 |
| `0x070A` | `camera.setZoomConfig` | draft | 否 | 设置变焦配置 |
| `0x070B` | `camera.getPtzState` | draft | 否 | 获取 PTZ 状态 |
| `0x070C` | `camera.setPtzConfig` | draft | 否 | 设置 PTZ 配置 |

#### 6.8 video.*

`video.*` 定义视频 framing、编码、叠加、场景和视频业务流。codec 是参数或 schema 字段，不作为 feature。

| methodId | methodName | 状态 | MVP | 说明 |
|---:|---|---|---:|---|
| `0x0801` | `video.getFramingConfig` | draft | 否 | 获取取景/构图配置 |
| `0x0802` | `video.setFramingConfig` | draft | 否 | 设置取景/构图配置 |
| `0x0803` | `video.getEncoderConfig` | draft | 否 | 获取编码配置 |
| `0x0804` | `video.setEncoderConfig` | draft | 否 | 设置编码配置 |
| `0x0805` | `video.getOutputTransformConfig` | draft | 否 | 获取输出变换配置 |
| `0x0806` | `video.setOutputTransformConfig` | draft | 否 | 设置输出变换配置 |
| `0x0807` | `video.getLayoutConfig` | draft | 否 | 获取视频布局配置 |
| `0x0808` | `video.setLayoutConfig` | draft | 否 | 设置视频布局配置 |
| `0x0809` | `video.getSceneConfig` | draft | 否 | 获取视频场景配置 |
| `0x080A` | `video.setSceneConfig` | draft | 否 | 设置视频场景配置 |
| `0x080B` | `video.openStream` | draft | 否 | 创建视频业务流 |
| `0x080C` | `video.closeStream` | draft | 否 | 关闭视频业务流 |
| `0x080D` | `video.getStreamState` | draft | 否 | 获取视频流状态 |
| `0x080E` | `video.getRtspConfig` | draft | 否 | 获取 RTSP 服务配置 |
| `0x080F` | `video.setRtspConfig` | draft | 否 | 设置 RTSP 服务配置 |
| `0x0810` | `video.getNdiConfig` | draft | 否 | 获取 NDI 服务配置 |
| `0x0811` | `video.setNdiConfig` | draft | 否 | 设置 NDI 服务配置 |

#### 6.9 audio.*

`audio.*` 定义音频算法、EQ、音量、路由、录制和播放。音频业务流由 audio 域创建。

| methodId | methodName | 状态 | MVP | 说明 |
|---:|---|---|---:|---|
| `0x0901` | `audio.getAlgorithmConfig` | stable | 否 | 获取音频算法配置 |
| `0x0902` | `audio.setAlgorithmConfig` | stable | 否 | 设置音频算法配置 |
| `0x0903` | `audio.getEqConfig` | draft | 否 | 获取 EQ 配置 |
| `0x0904` | `audio.setEqConfig` | draft | 否 | 设置 EQ 配置 |
| `0x0905` | `audio.getVolumeState` | draft | 否 | 获取音量状态 |
| `0x0906` | `audio.setVolumeConfig` | draft | 否 | 设置音量配置 |
| `0x0907` | `audio.getRoutingConfig` | draft | 否 | 获取音频路由配置 |
| `0x0908` | `audio.setRoutingConfig` | draft | 否 | 设置音频路由配置 |
| `0x0909` | `audio.openRecordingStream` | draft | 否 | 创建音频录制流 |
| `0x090A` | `audio.closeRecordingStream` | draft | 否 | 关闭音频录制流 |
| `0x090B` | `audio.getPlaybackState` | draft | 否 | 获取播放状态 |
| `0x090C` | `audio.setPlaybackConfig` | draft | 否 | 设置播放配置 |
| `0x090D` | `audio.getAlgorithmCapabilities` | stable | 否 | 获取音频算法参数能力 |
| `0x090E` | `audio.resetAlgorithmConfig` | stable | 否 | 重置音频算法配置 |

#### 6.10 input.*

高频输入数据走 STREAM；低频输入控制走 RPC。

| methodId | methodName | 状态 | MVP | 说明 |
|---:|---|---|---:|---|
| `0x0A01` | `input.getKeyConfig` | draft | 否 | 获取按键配置 |
| `0x0A02` | `input.setKeyConfig` | draft | 否 | 设置按键配置 |
| `0x0A03` | `input.getHidConfig` | draft | 否 | 获取 HID 配置 |
| `0x0A04` | `input.setHidConfig` | draft | 否 | 设置 HID 配置 |
| `0x0A05` | `input.getSourceState` | draft | 否 | 获取输入源状态 |
| `0x0A06` | `input.setSourceConfig` | draft | 否 | 设置输入源配置 |
| `0x0A07` | `input.openKvm` | draft | 否 | 开启 KVM |
| `0x0A08` | `input.closeKvm` | draft | 否 | 关闭 KVM |
| `0x0A09` | `input.getGpioState` | draft | 否 | 获取 GPIO 状态 |
| `0x0A0A` | `input.setGpioConfig` | draft | 否 | 设置 GPIO 配置 |

#### 6.11 output.*

| methodId | methodName | 状态 | MVP | 说明 |
|---:|---|---|---:|---|
| `0x0B01` | `output.getSourceConfig` | draft | 否 | 获取输出源配置 |
| `0x0B02` | `output.setSourceConfig` | draft | 否 | 设置输出源配置 |
| `0x0B03` | `output.getRoutingConfig` | draft | 否 | 获取输出路由配置 |
| `0x0B04` | `output.setRoutingConfig` | draft | 否 | 设置输出路由配置 |
| `0x0B05` | `output.getLayoutConfig` | draft | 否 | 获取输出布局配置 |
| `0x0B06` | `output.setLayoutConfig` | draft | 否 | 设置输出布局配置 |

#### 6.12 room.*

| methodId | methodName | 状态 | MVP | 说明 |
|---:|---|---|---:|---|
| `0x0C01` | `room.getInfo` | draft | 否 | 获取会议室信息 |
| `0x0C02` | `room.getScheduleConfig` | draft | 否 | 获取日程配置 |
| `0x0C03` | `room.setScheduleConfig` | draft | 否 | 设置日程配置 |
| `0x0C04` | `room.getSourceConfig` | draft | 否 | 获取会议室输入源配置 |
| `0x0C05` | `room.setSourceConfig` | draft | 否 | 设置会议室输入源配置 |
| `0x0C06` | `room.getLayoutConfig` | draft | 否 | 获取会议室布局配置 |
| `0x0C07` | `room.setLayoutConfig` | draft | 否 | 设置会议室布局配置 |
| `0x0C08` | `room.getParticipantState` | draft | 否 | 获取参会者状态 |

#### 6.13 signage.*

| methodId | methodName | 状态 | MVP | 说明 |
|---:|---|---|---:|---|
| `0x0D01` | `signage.listMedia` | draft | 否 | 列出标牌媒体 |
| `0x0D02` | `signage.getPlaylistConfig` | draft | 否 | 获取播放列表配置 |
| `0x0D03` | `signage.setPlaylistConfig` | draft | 否 | 设置播放列表配置 |
| `0x0D04` | `signage.getScheduleConfig` | draft | 否 | 获取播放计划配置 |
| `0x0D05` | `signage.setScheduleConfig` | draft | 否 | 设置播放计划配置 |
| `0x0D06` | `signage.getPlaybackState` | draft | 否 | 获取播放状态 |
| `0x0D07` | `signage.startPlayback` | draft | 否 | 开始播放 |
| `0x0D08` | `signage.stopPlayback` | draft | 否 | 停止播放 |
| `0x0D09` | `signage.getOsdConfig` | draft | 否 | 获取 OSD 配置 |
| `0x0D0A` | `signage.setOsdConfig` | draft | 否 | 设置 OSD 配置 |

#### 6.14 network.*

`network.*` 覆盖接口、IP、Wi-Fi、AP、Bluetooth 与服务端点发现；RTSP/NDI/Dante 等服务配置归各自业务域。

| methodId | methodName | 状态 | MVP | 说明 |
|---:|---|---|---:|---|
| `0x0E01` | `network.getInterfaceState` | draft | 否 | 获取网络接口状态 |
| `0x0E02` | `network.getIpConfig` | draft | 否 | 获取 IP 配置 |
| `0x0E03` | `network.setIpConfig` | draft | 否 | 设置 IP 配置 |
| `0x0E04` | `network.getWifiConfig` | draft | 否 | 获取 Wi-Fi 配置 |
| `0x0E05` | `network.setWifiConfig` | draft | 否 | 设置 Wi-Fi 配置 |
| `0x0E06` | `network.scanWifi` | draft | 否 | 扫描 Wi-Fi |
| `0x0E07` | `network.connectWifi` | draft | 否 | 连接 Wi-Fi |
| `0x0E08` | `network.disconnectWifi` | draft | 否 | 断开 Wi-Fi |
| `0x0E09` | `network.getWifiState` | draft | 否 | 获取 Wi-Fi 状态 |
| `0x0E0A` | `network.getApConfig` | draft | 否 | 获取 AP 配置 |
| `0x0E0B` | `network.setApConfig` | draft | 否 | 设置 AP 配置 |
| `0x0E0C` | `network.startAp` | draft | 否 | 启动 AP |
| `0x0E0D` | `network.stopAp` | draft | 否 | 停止 AP |
| `0x0E0E` | `network.getApState` | draft | 否 | 获取 AP 状态 |
| `0x0E0F` | `network.getServiceEndpointState` | draft | 否 | 获取服务端点状态 |

#### 6.15 storage.*

| methodId | methodName | 状态 | MVP | 说明 |
|---:|---|---|---:|---|
| `0x0F01` | `storage.getSdCardState` | draft | 否 | 获取 SD 卡状态 |
| `0x0F02` | `storage.formatSdCard` | draft | 否 | 格式化 SD 卡 |
| `0x0F03` | `storage.getDiskState` | draft | 否 | 获取磁盘状态 |
| `0x0F04` | `storage.getVolumeState` | draft | 否 | 获取卷状态 |
| `0x0F05` | `storage.listMedia` | draft | 否 | 列出媒体资源 |
| `0x0F06` | `storage.listRecordings` | draft | 否 | 列出录制资源 |
| `0x0F07` | `storage.rebuildIndex` | draft | 否 | 重建存储索引 |

#### 6.16 file.*

文件数据块必须走 STREAM；文件业务流由 `file.*` 方法创建。

| methodId | methodName | 状态 | MVP | 说明 |
|---:|---|---|---:|---|
| `0x1001` | `file.beginTransfer` | draft | 否 | 开始文件传输 |
| `0x1002` | `file.endTransfer` | draft | 否 | 结束文件传输 |
| `0x1003` | `file.cancelTransfer` | draft | 否 | 取消文件传输 |
| `0x1004` | `file.resumeTransfer` | draft | 否 | 恢复文件传输 |
| `0x1005` | `file.getTransferState` | draft | 否 | 获取文件传输状态 |
| `0x1006` | `file.listStorage` | draft | 否 | 列出文件存储位置 |
| `0x1007` | `file.getStorageState` | draft | 否 | 获取文件存储状态 |

#### 6.17 log.*

实时日志流归 `log.stream`，日志导出任务归 `log.export`。

| methodId | methodName | 状态 | MVP | 说明 |
|---:|---|---|---:|---|
| `0x1101` | `log.openStream` | draft | 否 | 创建实时日志流 |
| `0x1102` | `log.closeStream` | draft | 否 | 关闭实时日志流 |
| `0x1103` | `log.getStreamState` | draft | 否 | 获取实时日志流状态 |
| `0x1104` | `log.createExport` | draft | 否 | 创建日志导出任务 |
| `0x1105` | `log.getExportState` | draft | 否 | 获取日志导出状态 |
| `0x1106` | `log.cancelExport` | draft | 否 | 取消日志导出 |
| `0x1107` | `log.listFiles` | draft | 否 | 列出日志文件 |
| `0x1108` | `log.deleteFile` | draft | 否 | 删除日志文件 |

#### 6.18 diagnostic.*

| methodId | methodName | 状态 | MVP | 说明 |
|---:|---|---|---:|---|
| `0x1201` | `diagnostic.runSelfTest` | draft | 否 | 执行设备自检 |
| `0x1202` | `diagnostic.runNetworkTest` | draft | 否 | 执行网络测试 |
| `0x1203` | `diagnostic.runAudioTest` | draft | 否 | 执行音频测试 |
| `0x1204` | `diagnostic.runVideoTest` | draft | 否 | 执行视频测试 |
| `0x1205` | `diagnostic.runStorageTest` | draft | 否 | 执行存储测试 |
| `0x1206` | `diagnostic.runInputTest` | draft | 否 | 执行输入测试 |
| `0x1207` | `diagnostic.startCalibration` | draft | 否 | 启动校准流程 |
| `0x1208` | `diagnostic.createReportExport` | draft | 否 | 创建诊断报告导出任务 |
| `0x1209` | `diagnostic.getReportExportState` | draft | 否 | 获取诊断报告导出状态 |

#### 6.19 sensor.*

| methodId | methodName | 状态 | MVP | 说明 |
|---:|---|---|---:|---|
| `0x1301` | `sensor.getInfo` | draft | 否 | 获取传感器信息 |
| `0x1302` | `sensor.getState` | draft | 否 | 获取传感器状态 |
| `0x1303` | `sensor.openSampleStream` | draft | 否 | 打开采样流 |
| `0x1304` | `sensor.closeSampleStream` | draft | 否 | 关闭采样流 |

#### 6.20 auth.*

| methodId | methodName | 状态 | MVP | 说明 |
|---:|---|---|---:|---|
| `0x1401` | `auth.createSession` | draft | 否 | 创建认证会话 |
| `0x1402` | `auth.closeSession` | draft | 否 | 关闭认证会话 |
| `0x1403` | `auth.refreshToken` | draft | 否 | 刷新访问令牌 |
| `0x1404` | `auth.getPermissionState` | draft | 否 | 查询权限状态 |

#### 6.21 privacy.*

隐私遮挡属于通用业务域；具体云台、镜头盖、麦克风静音联动可以在 `camera.*`、`audio.*` 或 `vendor.*` 中扩展。

| methodId | methodName | 状态 | MVP | 说明 |
|---:|---|---|---:|---|
| `0x1501` | `privacy.getCoverState` | draft | 否 | 获取隐私盖状态 |
| `0x1502` | `privacy.setCoverConfig` | draft | 否 | 设置隐私盖配置 |
| `0x1503` | `privacy.getModeConfig` | draft | 否 | 获取隐私模式配置 |
| `0x1504` | `privacy.setModeConfig` | draft | 否 | 设置隐私模式配置 |
| `0x1505` | `privacy.getState` | draft | 否 | 获取隐私状态 |

---

### 7. 老协议适配 MethodId intake

老协议适配不要求保持原始 `CmdValue` 数值，但必须保留原有业务语义。映射必须记录在 `contract/registry/legacy/legacy_mapping.yaml` 中。

如果老协议 `CmdValue` 超过 `uint16`，不应直接作为 `methodId` 使用，应映射为 AXTP 新 methodId，并在 `legacy.cmdValue` 中记录原值。

当前没有 `contract/registry/legacy/legacy_mapping.yaml`，表示没有已采纳 legacy method mapping。下表只作为 intake 示例和历史分类线索，不参与 generated 协议：

| 老协议来源 | legacy CmdValue | 旧命令语义 | 候选 AXTP method | 采纳状态 | 说明 |
|---|---:|---|---|---|---|
| AXDP_HID | `0xB0002` | BetaDeviceInfo | `device.getInfo` | 未采纳 | 待 device 草案采纳后才可登记 |
| AXDP_HID | `0xC0021` | CommonSetVideoMode | `video.setFramingConfig` | 未采纳 | 待 video.framing 草案采纳后才可登记 |
| AXDP_HID | `0xA0001` | AlphaUpgradeInfo | `firmware.getUpdateCapabilities` | 未采纳 | 待 firmware.update 草案采纳后才可登记 |
| AXDP_HID | `0xB0042` | BetaBrightnessSet | `display.setBrightness` | 未采纳 | 待 display 草案采纳后才可登记 |

未批准或缺少具体 CmdValue 的旧协议命令不得写入本表；应先按 Naming and Taxonomy spec 的 domain-feature 规则完成分类，再在 `contract/registry/legacy/legacy_mapping.yaml` 中登记确定的映射。

---

### 8. Method 与 Schema 的关系

每个 Method 必须引用 `Params Schema` 和 `Result Schema`。即使没有参数或返回数据，也必须显式声明 `EmptyParams` / `EmptyResult`。

---

### 9. Method 与 ErrorCode 的关系

每个 Method 必须声明可能返回的错误码。当前 adopted 业务方法至少应覆盖通用成功、参数、支持性和运行态错误：

```text
SUCCESS / INVALID_ARGUMENT / RPC_METHOD_NOT_SUPPORTED / BUSY / INTERNAL_ERROR
```

固件升级类方法还应包含：

```text
FW_IMAGE_INVALID / FW_VERIFY_FAILED / FW_VERSION_UNSUPPORTED
FW_STORAGE_NOT_ENOUGH / FW_ROLLBACK_FAILED
```

Stream 类方法还应包含：

```text
STREAM_NOT_FOUND / STREAM_WINDOW_FULL / STREAM_TIMEOUT
```

---

### 10. Method 与 Capability 的关系

Method 是否可调用，应结合 Capability Registry 判断。调用流程：

```text
1. App 加载当前产品 generated registry，确认目标 method 已生成。
2. 如业务协议提供运行时 capability/config 查询，先调用该查询方法。
3. 检查相关 capability 是否满足。
4. 发送 RPC Request。
5. 若设备不支持，返回 RPC_METHOD_NOT_SUPPORTED、NOT_SUPPORTED 或对应业务错误。
```

Method 的 `capability.required` 必须引用 `domain.feature` 能力块。例如当前 `audio.getAlgorithmConfig`、`audio.setAlgorithmConfig`、`audio.resetAlgorithmConfig` 均归属 `audio.algorithm`。

---

### 11. Method 与 Event 的关系

如果 Method 会触发事件，应在条目中声明（如 `audio.setAlgorithmConfig` 触发 `audio.algorithmConfigChanged`）。事件本身不使用 MethodId，而使用 EventId，通过 `rpcOp = EVENT` 承载。

---

### 12. Generator v1 校验规则

Generator 必须执行以下校验：

```text
methodId 不重复 / methodName 不重复 / methodId 范围与 domain 匹配
status 合法 / mvp 条目必须 priority = P0 或 P1
params schema 必须存在 / result schema 必须存在
errorCode 必须存在 / event 引用必须存在 / capability 引用必须存在
legacy mapping 的 axtpMethodId 必须存在
```

校验失败时，应停止生成。
