# Event 候选规划表

> 本附录为非规范性内容。
> 本附录保留历史规划表和候选 registry 条目。
> Runtime 实现不得将本附录视为实现合同。
> 已采纳条目必须以 registry YAML 和 generated protocol 产物为准。

来源文档： `../03-Events-Registry.md`

## 正式 EventId 注册表

版本：v1.1
状态：MVP EventId 注册表（精简版）
适用范围：AXTP RPC EventId 分配、Domain 分段、MVP 事件集合、老协议事件适配

---
### 1. EventId 在协议中的位置

事件只属于 RPC 层：

```text
AXTP Frame Header
  payloadType = RPC
    ↓
RPC Payload
  rpcOp = EVENT
  eventId = xxx
  bodyEncoding = NONE / TLV8 / TLV16
  body = event data
```

EventId 不应出现在 Frame Header、Control Payload Header 或 Stream Payload Header 中。

---

### 2. 事件设计原则

- 事件名使用 feature 模板，表达变化、进度、结果或上报（`network.wifiStateChanged`，不是 `network.changeWifiState`）
- 事件不替代 Response：Response 回答请求是否成功，Event 通知后续状态变化
- 接收端必须允许忽略未知事件
- 每个事件必须绑定事件数据 Schema，不允许只定义事件名而不定义数据结构
- 事件订阅使用域级掩码（`eventMasks`），由 RPC `IDENTIFY / REIDENTIFY` 声明；每个事件在其 Domain 内分配唯一 `bitOffset`（0-255）；MVP 阶段设备可采用全量广播模式

---

### 2.1 bitOffset 分配规则

每个事件在其 Domain 内分配一个唯一的、自增的 `bitOffset`（0 到 255）。`bitOffset` 与 EventId 低字节无关，独立分配，由 Registry 管理。

`eventMasks` 中的 DomainId 等于 EventId 的高字节，并与同 domain 的 MethodId 高字节一致（如 `display.*` 方法和事件均使用 `0x06xx`，DomainId = `0x06`）。

设备端判定某事件是否被订阅：

```cpp
bool isEventSubscribed(const uint8_t* bitmask, uint8_t maskLen, uint8_t bitOffset) {
    uint8_t byteIndex = bitOffset / 8;
    uint8_t bitIndex  = bitOffset % 8;
    if (byteIndex >= maskLen) return false;
    return (bitmask[byteIndex] & (1 << bitIndex)) != 0;
}
```

---

### 3. EventId 分段规划

EventId 使用 `uint16`，按与 MethodId 相同的 domain 分段分配。Event 与 Method 通过 RPC `rpcOp` 区分，不通过高位区间区分；同一 domain 的 EventId、MethodId 和域级掩码 DomainId 必须使用同一个高字节。

| 范围 | Domain | 说明 |
|---:| --- |---|
| `0x0000-0x00FF` | reserved | 保留 |
| `0x0100-0x01FF` | `device.*` | 设备基础事件 |
| `0x0200-0x02FF` | `capability.*` | 能力变化事件 |
| `0x0300-0x03FF` | `system.*` | 系统状态事件 |
| `0x0400-0x04FF` | `firmware.*` | 固件更新 / 固件升级事件 |
| `0x0500-0x05FF` | `stream.*` | 流状态事件 |
| `0x0600-0x06FF` | `display.*` | 显示类事件 |
| `0x0700-0x07FF` | `camera.*` | 摄像头事件 |
| `0x0800-0x08FF` | `video.*` | 视频控制面事件 |
| `0x0900-0x09FF` | `audio.*` | 音频控制面事件 |
| `0x0A00-0x0AFF` | `input.*` | 输入 / KVM 事件 |
| `0x0B00-0x0BFF` | `output.*` | 输出源 / 路由 / 布局事件 |
| `0x0C00-0x0CFF` | `room.*` | 会议室 / 协作空间事件 |
| `0x0D00-0x0DFF` | `signage.*` | 数字标牌事件 |
| `0x0E00-0x0EFF` | `network.*` | 网络事件 |
| `0x0F00-0x0FFF` | `storage.*` | 存储事件 |
| `0x1000-0x10FF` | `file.*` | 文件传输事件 |
| `0x1100-0x11FF` | `log.*` | 日志事件 |
| `0x1200-0x12FF` | `diagnostic.*` | 诊断 / 产测事件 |
| `0x1300-0x13FF` | `sensor.*` | 传感器事件 |
| `0x1400-0x14FF` | `auth.*` | 认证事件 |
| `0x1500-0x15FF` | `privacy.*` | 隐私事件 |
| `0x7000-0x7FFF` | `vendor.*` | 厂商私有事件 |
| `0x8000-0xFFFF` | reserved | 保留 |

---

### 4. 当前生成 EventId 注册表

EventId 表以 `contract/registry/domains/<domain>/domain.yaml` 和按需存在的 `contract/registry/event/event_registry.yaml` 为事实源。当前 generated 只包含下列已采纳事件；其他事件即使出现在后续规划表中，也不得视为当前实现合同。

| eventId | eventName | Domain | bitOffset | 状态 | 说明 |
| ---: | --- | --- | ---: | --- | --- |
| `0x0901` | `audio.algorithmConfigChanged` | audio | 0 | stable | 音频算法配置变化 |

说明：本表使用 domain-feature 治理后的主名称，旧事件名只在 legacy、deprecated 或 migration 说明中出现。

---

### 5. 完整 EventId 规划

以下表格是正式 EventId 规划表，用于保留编号空间和约束可采纳事件；当前实现状态以 `contract/registry/domains/<domain>/domain.yaml`、按需存在的 `contract/registry/event/event_registry.yaml` 及生成产物为准。

#### 5.1 device 事件

| eventId | eventName | 状态 | 说明 |
|---:| --- |---| --- |
| `0x0101` | `device.stateChanged` | draft | 设备状态变化 |
| `0x0102` | `device.powerStateChanged` | draft | 电源状态变化 |
| `0x0103` | `device.indicatorConfigChanged` | draft | 指示灯/蜂鸣配置变化 |
| `0x0104` | `device.inventoryChanged` | draft | 设备库存/子模块信息变化 |
| `0x0105` | `device.childDeviceStateChanged` | draft | 子设备状态变化 |

#### 5.2 reserved / session 说明

`session` 与 `event` 不作为业务 EventId 域。事件订阅集合属于 RPC 会话状态，由 `IDENTIFY / REIDENTIFY.eventSubscriptions` 管理；变更失败必须通过 RPC status 返回，不额外分配 `event.subscribed / event.unsubscribed` 事件。

`CONTROL` 的 `OPEN / CLOSE / RESUME` 是协议运行时信令，不应注册为 EventId。

#### 5.3 capability 事件

| eventId | eventName | 状态 | 说明 |
|---:| --- |---| --- |
| `0x0201` | `capability.registryChanged` | draft | 能力注册表变化 |
| `0x0202` | `capability.methodStateChanged` | draft | 支持的方法集合变化 |
| `0x0203` | `capability.limitStateChanged` | draft | 设备限制参数变化 |

#### 5.4 system 事件

| eventId | eventName | 状态 | 说明 |
|---:| --- |---| --- |
| `0x0301` | `system.timeConfigChanged` | draft | 系统时间配置变化 |
| `0x0302` | `system.lifecycleStateChanged` | draft | 生命周期状态变化 |
| `0x0303` | `system.resetStateChanged` | draft | 重置流程状态变化 |
| `0x0304` | `system.initializationStateChanged` | draft | 初始化状态变化 |
| `0x0305` | `system.licenseStateChanged` | draft | 系统级 license 状态变化 |

#### 5.5 firmware 事件

| eventId | eventName | 状态 | 说明 |
|---:| --- |---| --- |
| `0x0401` | `firmware.infoChanged` | draft | 固件信息变化 |
| `0x0402` | `firmware.updateProgressReported` | draft | 固件更新进度上报，待 `firmware.update` 草案采纳 |
| `0x0403` | `firmware.updateStateChanged` | draft | 固件更新状态变化，待 `firmware.update` 草案采纳 |
| `0x0404` | `firmware.updateResultReported` | draft | 固件更新结果上报，待 `firmware.update` 草案采纳 |
| `0x0405` | `firmware.updatePolicyChanged` | draft | 固件更新策略变化 |

#### 5.6 stream 事件

| eventId | eventName | 状态 | 说明 |
|---:| --- |---| --- |
| `0x0501` | `stream.flowControlStateChanged` | draft | 公共流控状态变化 |
| `0x0502` | `stream.windowUpdated` | draft | 流控窗口变化 |
| `0x0503` | `stream.statsReported` | draft | STREAM 统计上报 |
| `0x0504` | `stream.errorReported` | draft | STREAM 数据面错误上报 |

#### 5.7 display 事件

| eventId | eventName | 状态 | 说明 |
|---:| --- |---| --- |
| `0x0601` | `display.powerStateChanged` | draft | 显示电源状态变化 |
| `0x0602` | `display.colorConfigChanged` | draft | 色彩配置变化 |
| `0x0603` | `display.backlightConfigChanged` | draft | 背光配置变化 |
| `0x0604` | `display.inputStateChanged` | draft | 显示输入状态变化 |
| `0x0605` | `display.outputStateChanged` | draft | 显示输出状态变化 |
| `0x0607` | `display.brightnessChanged` | draft | 亮度值变化，待 display 草案采纳 |
| `0x0608` | `display.brightnessConfigChanged` | draft | 亮度配置变化 |

#### 5.8 camera 事件

摄像头控制面事件归入 `camera.*`；视频帧数据本身仍必须通过 STREAM 承载。

| eventId | eventName | 状态 | 说明 |
|---:| --- |---| --- |
| `0x0701` | `camera.imageConfigChanged` | draft | 图像配置变化 |
| `0x0702` | `camera.exposureConfigChanged` | draft | 曝光配置变化 |
| `0x0703` | `camera.whiteBalanceConfigChanged` | draft | 白平衡配置变化 |
| `0x0704` | `camera.focusStateChanged` | draft | 对焦状态变化 |
| `0x0705` | `camera.zoomStateChanged` | draft | 变焦状态变化 |
| `0x0706` | `camera.ptzStateChanged` | draft | PTZ 状态变化 |
| `0x0707` | `camera.calibrationStateChanged` | draft | 校准状态变化 |

#### 5.9 video 事件

视频帧数据本身不通过 Event 承载，应通过 `PayloadType = STREAM` 承载。

| eventId | eventName | 状态 | 说明 |
|---:| --- |---| --- |
| `0x0801` | `video.framingConfigChanged` | draft | 视频构图配置变化 |
| `0x0802` | `video.outputTransformConfigChanged` | draft | 输出变换配置变化 |
| `0x0803` | `video.encoderConfigChanged` | draft | 编码配置变化 |
| `0x0804` | `video.layoutConfigChanged` | draft | 视频布局配置变化 |
| `0x0805` | `video.sceneConfigChanged` | draft | 视频场景配置变化 |
| `0x0806` | `video.streamStateChanged` | draft | 视频业务流状态变化 |
| `0x0807` | `video.rtspConfigChanged` | draft | RTSP 配置变化 |
| `0x0808` | `video.ndiConfigChanged` | draft | NDI 配置变化 |

#### 5.10 audio 事件

| eventId | eventName | 状态 | 说明 |
|---:| --- |---| --- |
| `0x0901` | `audio.algorithmConfigChanged` | stable | 音频算法配置变化 |
| `0x0902` | `audio.eqConfigChanged` | draft | EQ 配置变化 |
| `0x0903` | `audio.volumeStateChanged` | draft | 音量状态变化 |
| `0x0904` | `audio.routingConfigChanged` | draft | 音频路由配置变化 |
| `0x0905` | `audio.recordingStreamStateChanged` | draft | 音频录制流状态变化 |
| `0x0906` | `audio.playbackStateChanged` | draft | 播放状态变化 |

#### 5.11 input / KVM 事件

| eventId | eventName | 状态 | 说明 |
|---:| --- |---| --- |
| `0x0A01` | `input.keyConfigChanged` | draft | 按键配置变化 |
| `0x0A02` | `input.hidConfigChanged` | draft | HID 配置变化 |
| `0x0A03` | `input.sourceStateChanged` | draft | 输入源状态变化 |
| `0x0A04` | `input.kvmStateChanged` | draft | KVM 状态变化 |
| `0x0A05` | `input.gpioStateChanged` | draft | GPIO 状态变化 |

#### 5.12 output 事件

| eventId | eventName | 状态 | 说明 |
|---:| --- |---| --- |
| `0x0B01` | `output.sourceChanged` | draft | 输出源变化 |
| `0x0B02` | `output.routingChanged` | draft | 输出路由变化 |
| `0x0B03` | `output.layoutChanged` | draft | 输出布局变化 |

#### 5.13 room 事件

| eventId | eventName | 状态 | 说明 |
|---:| --- |---| --- |
| `0x0C01` | `room.infoChanged` | draft | 会议室信息变化 |
| `0x0C02` | `room.scheduleChanged` | draft | 日程变化 |
| `0x0C03` | `room.sourceChanged` | draft | 会议室输入源变化 |
| `0x0C04` | `room.layoutChanged` | draft | 会议室布局变化 |
| `0x0C05` | `room.participantChanged` | draft | 参会者变化 |

#### 5.14 signage 事件

| eventId | eventName | 状态 | 说明 |
|---:| --- |---| --- |
| `0x0D01` | `signage.mediaChanged` | draft | 标牌媒体变化 |
| `0x0D02` | `signage.playlistChanged` | draft | 播放列表变化 |
| `0x0D03` | `signage.scheduleChanged` | draft | 播放计划变化 |
| `0x0D04` | `signage.playbackStateChanged` | draft | 播放状态变化 |
| `0x0D05` | `signage.osdChanged` | draft | OSD 配置变化 |

#### 5.15 network 事件

| eventId | eventName | 状态 | 说明 |
|---:| --- |---| --- |
| `0x0E01` | `network.interfaceStateChanged` | draft | 网络接口状态变化 |
| `0x0E02` | `network.ipConfigChanged` | draft | IP 配置变化 |
| `0x0E03` | `network.wifiConfigChanged` | draft | Wi-Fi 配置变化 |
| `0x0E04` | `network.wifiStateChanged` | draft | Wi-Fi 状态变化 |
| `0x0E05` | `network.wifiScanResultReported` | draft | Wi-Fi 扫描结果上报 |
| `0x0E06` | `network.apConfigChanged` | draft | AP 配置变化 |
| `0x0E07` | `network.apStateChanged` | draft | AP 状态变化 |
| `0x0E08` | `network.apClientChanged` | draft | AP 客户端变化 |
| `0x0E09` | `network.serviceEndpointStateChanged` | draft | 服务端点状态变化 |

#### 5.16 storage 事件

| eventId | eventName | 状态 | 说明 |
|---:| --- |---| --- |
| `0x0F01` | `storage.sdCardStateChanged` | draft | SD 卡状态变化 |
| `0x0F02` | `storage.diskStateChanged` | draft | 磁盘状态变化 |
| `0x0F03` | `storage.volumeStateChanged` | draft | 卷状态变化 |
| `0x0F04` | `storage.mediaChanged` | draft | 媒体资源变化 |
| `0x0F05` | `storage.recordingChanged` | draft | 录制资源变化 |
| `0x0F06` | `storage.indexStateChanged` | draft | 存储索引状态变化 |

#### 5.17 file 事件

| eventId | eventName | 状态 | 说明 |
|---:| --- |---| --- |
| `0x1001` | `file.transferStateChanged` | draft | 文件传输状态变化 |
| `0x1002` | `file.transferProgressReported` | draft | 文件传输进度上报 |
| `0x1003` | `file.storageStateChanged` | draft | 文件存储状态变化 |

#### 5.18 log 事件

| eventId | eventName | 状态 | 说明 |
|---:| --- |---| --- |
| `0x1101` | `log.streamStateChanged` | draft | 实时日志流状态变化 |
| `0x1102` | `log.exportStateChanged` | draft | 日志导出状态变化 |
| `0x1103` | `log.exportProgressReported` | draft | 日志导出进度上报 |
| `0x1104` | `log.filesChanged` | draft | 日志文件列表变化 |

#### 5.19 diagnostic 事件

| eventId | eventName | 状态 | 说明 |
|---:| --- |---| --- |
| `0x1201` | `diagnostic.selfTestStateChanged` | draft | 自检状态变化 |
| `0x1202` | `diagnostic.selfTestProgressReported` | draft | 自检进度上报 |
| `0x1203` | `diagnostic.networkTestStateChanged` | draft | 网络测试状态变化 |
| `0x1204` | `diagnostic.audioTestStateChanged` | draft | 音频测试状态变化 |
| `0x1205` | `diagnostic.videoTestStateChanged` | draft | 视频测试状态变化 |
| `0x1206` | `diagnostic.reportExportStateChanged` | draft | 诊断报告导出状态变化 |
| `0x1207` | `diagnostic.reportExportProgressReported` | draft | 诊断报告导出进度上报 |

#### 5.20 sensor 事件

| eventId | eventName | 状态 | 说明 |
|---:| --- |---| --- |
| `0x1301` | `sensor.stateChanged` | draft | 传感器状态变化 |
| `0x1302` | `sensor.sampleStreamStateChanged` | draft | 采样流状态变化 |
| `0x1303` | `sensor.sampleReported` | draft | 低频采样上报 |

#### 5.21 auth 事件

| eventId | eventName | 状态 | 说明 |
|---:| --- |---| --- |
| `0x1401` | `auth.sessionStateChanged` | draft | 认证会话状态变化 |
| `0x1402` | `auth.permissionStateChanged` | draft | 权限状态变化 |
| `0x1403` | `auth.tokenStateChanged` | draft | token 状态变化 |

#### 5.22 privacy 事件

| eventId | eventName | 状态 | 说明 |
|---:| --- |---| --- |
| `0x1501` | `privacy.coverStateChanged` | draft | 隐私盖状态变化 |
| `0x1502` | `privacy.modeConfigChanged` | draft | 隐私模式配置变化 |
| `0x1503` | `privacy.stateChanged` | draft | 隐私状态变化 |

---

### 6. Event Schema 规范

每个事件必须绑定一个事件数据 Schema，推荐使用 TLV Schema。

事件数据 Schema 的 `fieldId` 是 schema-local 语义，不设置跨事件的公共固定字段。每个事件自己的字段从 `0x01` 开始连续分配；如果某个事件需要 `timestamp / reasonCode / errorCode / message` 等字段，也在该事件 Schema 内按普通字段编号声明。

#### 6.1 severity 枚举

| 值 | 名称 | 说明 |
|---:| --- |---|
| `0x00` | `INFO` | 普通信息 |
| `0x01` | `WARNING` | 警告 |
| `0x02` | `ERROR` | 错误 |
| `0x03` | `CRITICAL` | 严重错误 |

---

### 7. 当前生成事件 Schema

#### 7.1 audio.algorithmConfigChanged

字段以 `contract/registry/domains/audio/domain.yaml` 中的 `AudioAlgorithmConfigChangedEvent` 为准；当前只包含已采纳 `audio.algorithm` 草案要求的配置变化信息。

| fieldId | 字段名 | 类型 | 必填 | 说明 |
|---:| --- |---| --- |---|
| `0x01` | `reason` | enum | 是 | 变化原因，如 user_request、reset_to_default、factory_reset、profile_changed、device_policy |
| `0x02` | `applyState` | enum | 是 | 应用状态，值为 applied 或 pending_restart |
| `0x03` | `requiresAudioRestart` | bool | 是 | 是否需要重启音频链路或重建音频 pipeline |
| `0x04` | `config` | AudioAlgorithmConfig | 是 | 变化或受影响的算法配置值 |
| `0x05` | `changedFields` | bytes | 否 | JSON 字符串数组，记录变化字段路径，如 `noiseSuppression.level` |

---

### 8. Event 与 Method 的关系

| Method | 可能触发的 Event |
| --- |---|
| `audio.setAlgorithmConfig` | `audio.algorithmConfigChanged` |
| `audio.resetAlgorithmConfig` | `audio.algorithmConfigChanged` |
| `video.openStream` | `video.streamStateChanged` |
| `log.openStream` | `log.streamStateChanged` |
| `file.beginTransfer` | `file.transferStateChanged` / `file.transferProgressReported` |
| `firmware.beginUpdate` | `firmware.updateStateChanged` / `firmware.updateProgressReported` |
| `firmware.verifyUpdatePackage` | `firmware.updateStateChanged` / `firmware.updateResultReported` |
| `firmware.installUpdate` | `firmware.updateStateChanged` / `firmware.updateResultReported` |

Method Registry 中通过 `events` 字段声明关联事件。未采纳的规划 method/event 不能作为当前生成合同。

---

### 9. Event 与 Capability 的关系

设备是否支持某个事件，应通过已采纳业务 capability 或配置查询方法表达。v1 Core 不强制内置事件能力发现 method。

---

### 10. Event 与 ErrorCode 的关系

事件可以携带 `errorCode`，但事件本身不等于错误。错误码必须来自 ErrorCode Registry。

---

### 11. Event 与 Control 的关系

Control 层的 ACK/NACK、OPEN、HEARTBEAT 不应作为 Event 上报。CONTROL 是协议运行时信令，EVENT 是业务层异步通知，二者可以相关但不能混用。

---

### 12. 老协议事件适配规则

旧协议中常见状态上报迁移为 AXTP Event：

| 旧协议语义 | AXTP Event |
| --- |---|
| 设备状态变化 | `device.stateChanged` |
| 亮度变化上报 | 待 display 草案采纳后映射 |
| 固件更新进度/状态/结果上报 | 待 `firmware.update` 草案采纳后映射 |
| 视频流开启/关闭通知 | `video.streamStateChanged` |
| 日志流开启/关闭通知 | `log.streamStateChanged` |
| 流异常通知 | 业务域 `*.streamStateChanged` 或公共 `stream.errorReported` |

旧规划中的 `stream.opened / stream.closed / stream.error` 仅作为迁移别名保留；新主事件必须归属业务域或公共 `stream.flowControl`。

如果老协议中存在独立事件 CmdValue，可以保留其值作为 `legacyId`，但不建议直接作为 AXTP EventId。

如果旧协议没有事件只能轮询状态，应在对应业务草案中同时设计查询 method 和事件上报；未采纳前不得写入 generated 协议。

---

### 13. Generator v1 校验规则

Generator 必须执行以下校验：

```text
eventId 不重复 / eventName 不重复 / eventId 范围与 domain 匹配
status 合法 / schema 引用必须存在 / capability 引用必须存在
legacy mapping 的 axtpEventId 必须存在
```
