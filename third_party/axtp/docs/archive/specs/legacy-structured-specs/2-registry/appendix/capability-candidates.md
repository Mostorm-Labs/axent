# Capability 候选规划表

> 本附录为非规范性内容。
> 本附录保留历史规划表和候选 registry 条目。
> Runtime 实现不得将本附录视为实现合同。
> 已采纳条目必须以 registry YAML 和 generated protocol 产物为准。

来源文档： `../../3-codec/02-Capability-Types.md`

## 正式 CapabilityId 与 Capability 注册表

版本：v1.1
状态：Capability Registry 规范（精简版）
适用范围：AXTP Capability 分类、CapabilityId 编号、条目结构、查询方法、当前生成集合、老协议适配

---

### 1. Capability 定位

Capability 是对设备能力、协议能力、传输能力和业务功能的声明，不是业务命令本身。业务能力主名采用 `domain.feature`，feature 是能力块，不是字段名。

两类能力发现机制：

| 机制 | 职责 |
| --- |---|
| `CONTROL OPEN / ACCEPT` | 协议运行时参数协商（maxFrameSize、supportedPayloadTypes、supportedRpcEncodings、heartbeatIntervalMs、ackMode、windowSize，以及 profile-specific mtu） |
| `RPC capability.*` | 业务能力查询（display brightness、firmware.update、video stream、methodId 支持、eventId 支持、文件类型） |

规则：
- 影响 Frame/Control/RPC/Stream Parser 工作方式的能力必须在 CONTROL OPEN 阶段协商
- 业务能力不得放进 CONTROL OPEN，必须通过 RPC 查询
- MethodId 存在不代表设备必须支持；设备是否支持必须通过 Capability 判断
- StreamProfile 存在不代表设备支持该流；必须通过 `stream.profile` 或业务域 feature capability 判断
- 字段级限制进入 feature capability schema 或 `getFeatureCapabilities` 响应，不再作为独立 capability 主名

---

### 2. Capability 分类

| 类别 | 说明 | 发现方式 |
| --- |---| --- |
| protocol capability | 协议版本、PayloadType、Frame Profile | CONTROL OPEN + RPC |
| transport capability | 最大 Frame、profile-specific MTU、窗口、ACK 模式 | CONTROL OPEN + RPC |
| rpc capability | RPC 编码、bodyEncoding、methodId 支持 | CONTROL OPEN + RPC |
| stream capability | Stream Profile ID、窗口、断点续传、CRC | RPC |
| business capability | display、firmware、file、video 等业务能力 | RPC |
| vendor capability | 厂商私有能力 | RPC |

---

### 3. CapabilityId 编号规划

CapabilityId 使用 `uint16`，按 `specs/4-tooling/01-YAML-Mapping.md`§9 的 Domain Registry 高字节分段：

```text
0x0000-0x00FF  协议 / 通用能力
0x0100-0x01FF  设备基础能力
0x0200-0x02FF  能力查询与协商能力
0x0300-0x03FF  系统能力
0x0400-0x04FF  固件 / 固件更新能力
0x0500-0x05FF  STREAM 能力
0x0600-0x06FF  显示能力
0x0700-0x07FF  摄像头能力
0x0800-0x08FF  视频能力
0x0900-0x09FF  音频能力
0x0A00-0x0AFF  输入 / KVM 能力
0x0B00-0x0BFF  输出能力
0x0C00-0x0CFF  会议室 / 协作空间能力
0x0D00-0x0DFF  数字标牌能力
0x0E00-0x0EFF  网络能力
0x0F00-0x0FFF  存储能力
0x1000-0x10FF  文件能力
0x1100-0x11FF  日志能力
0x1200-0x12FF  诊断能力
0x1300-0x13FF  传感器能力
0x1400-0x14FF  认证能力
0x1500-0x15FF  隐私能力
0x1600-0x16FF  软件对象能力
0x7000-0x7FFF  厂商私有能力
0x8000-0xFFFF  保留
```

---

### 4. Capability 条目结构

```yaml
- id: 0x0001
  name: protocol.payload.control
  domain: protocol
  domainId: 0x00        # protocol/common capability 使用 0x00；业务 capability 使用第 9 章 Domain Registry
  bitOffset: 0          # 该 capability 在 Domain 内的掩码位偏移
  status: mvp
  type: bool
  description: Device supports CONTROL payload.
  discovery:
    controlHello: true
    rpc: true
  relatedMethods: []
  since: 1.0.0
```

| 字段 | 说明 |
| --- | --- |
| `id` | capabilityId |
| `name` | capability 名称 |
| `domain` | 所属域 |
| `domainId` | capability 查询响应中的 DomainId（1B）；业务 capability 与 MethodId 高字节对齐，协议/通用 capability 使用 `0x00` |
| `bitOffset` | 该 capability 在 Domain 内的掩码位偏移（0-255），由 Registry 自增分配 |
| `status` | draft / mvp / stable / deprecated / reserved |
| `type` | bool / uint / enum / bitmap / object / array |
| `description` | 描述 |
| `values` | 枚举或 bitmap 值 |
| `range` | 数值范围 |
| `discovery` | 是否可通过 OPEN 或 RPC 查询 |
| `relatedMethods` | 相关 RPC 方法 |
| `relatedEvents` | 相关事件 |
| `relatedStreams` | 相关 Stream 类型 |
| `legacyMapping` | 老协议适配关系 |
| `since` | 引入版本 |
| `deprecatedSince` | 废弃版本 |

---

### 4.1 可选 method bitmap 响应格式：域级方法掩码

AXTP v1 Core 不强制生成全局 supported-methods 查询方法。如果后续已采纳的 capability 协议需要返回当前设备、当前固件、当前会话、当前鉴权状态下支持的 methodId 集合，必须使用本节的域级方法掩码格式。

该 Binary 响应使用域级二进制掩码链（Domain Mask Packets Chain）：

```text
Domain Block = [DomainId: 1B] + [MaskLen: 1B] + [MethodBitmask: N B (Big-Endian / network byte order)]
```

完整 `capability.getRegistry` 响应属于 v2/P1 Capability Model，不作为 v1 Core 必选项。

### 4.2 capability.getRegistry 响应格式：域级掩码（v2/P1）

`capability.getRegistry` 的 Binary 响应使用**域级二进制掩码链（Domain Mask Packets Chain）**，而非 JSON 键值对列表。

**格式**：

```text
Domain Block = [DomainId: 1B] + [MaskLen: 1B] + [Bitmask: N B (Big-Endian / network byte order)]
```

- `DomainId`：与 MethodId 高字节对齐（如 `display.*` 的 DomainId = `0x06`）
- `MaskLen`：Bitmask 字节数，高水位截断（只发到最高有效字节）
- `Bitmask`：该域的能力掩码，Bit N 对应 `bitOffset=N` 的 capability

**高水位截断规则**：如果某域只用到 bitOffset=3，`MaskLen` 必须为 1，不得发送多余字节。

**MVP 设备示例**（支持 protocol 域 Bits 0-2、display 域 Bit 0、firmware 域 Bits 0-1）：

```text
Binary: 00 01 07  06 01 01  04 01 03
        |________|  |________|  |________|
         domain=0x00  domain=0x06  domain=0x04
```

**JSON 编码（WebSocket/HTTP Debug）**：

```json
{
  "result": {
    "capabilityMasks": "000107060101 040103"
  }
}
```

**DomainId 与 MethodId 范围对应关系**：

| DomainId | MethodId 范围 | 域名 |
| --- | --- | --- |
| `0x01` | `0x0100-0x01FF` | `device.*` |
| `0x02` | `0x0200-0x02FF` | `capability.*` |
| `0x03` | `0x0300-0x03FF` | `system.*` |
| `0x04` | `0x0400-0x04FF` | `firmware.*` |
| `0x05` | `0x0500-0x05FF` | `stream.*` |
| `0x06` | `0x0600-0x06FF` | `display.*` |
| `0x07` | `0x0700-0x07FF` | `camera.*` |
| `0x08` | `0x0800-0x08FF` | `video.*` |
| `0x09` | `0x0900-0x09FF` | `audio.*` |
| `0x0A` | `0x0A00-0x0AFF` | `input.*` |
| `0x0B` | `0x0B00-0x0BFF` | `output.*` |
| `0x0C` | `0x0C00-0x0CFF` | `room.*` |
| `0x0D` | `0x0D00-0x0DFF` | `signage.*` |
| `0x0E` | `0x0E00-0x0EFF` | `network.*` |
| `0x0F` | `0x0F00-0x0FFF` | `storage.*` |
| `0x10` | `0x1000-0x10FF` | `file.*` |
| `0x11` | `0x1100-0x11FF` | `log.*` |
| `0x12` | `0x1200-0x12FF` | `diagnostic.*` |
| `0x13` | `0x1300-0x13FF` | `sensor.*` |
| `0x14` | `0x1400-0x14FF` | `auth.*` |
| `0x15` | `0x1500-0x15FF` | `privacy.*` |
| `0x16` | `0x1600-0x16FF` | `software.*` |

**C++ 解析（O(1) 安全寻址）**：

```cpp
bool isCapabilitySupported(const uint8_t* bitmask, uint8_t maskLen, uint8_t bitOffset) {
    uint8_t byteIndex = bitOffset / 8;
    uint8_t bitIndex  = bitOffset % 8;
    if (byteIndex >= maskLen) return false;
    return (bitmask[byteIndex] & (1 << bitIndex)) != 0;
}
```

---

### 5. 协议 / 通用能力注册表

`0x0000-0x00FF` 只用于协议级、通用或历史保留 capability，不作为业务域。最大 Frame、profile-specific MTU、RPC 编码、窗口等运行时参数优先通过 `CONTROL OPEN / ACCEPT` 协商；如果后续需要以 CapabilityId 暴露，也必须继续分配在 `0x00xx`。

| capabilityId | name | 类型 | 状态 | 说明 |
|---:| --- |---| --- |---|
| `0x0001` | `protocol.payload.control` | bool | mvp | 支持 CONTROL payload |
| `0x0002` | `protocol.payload.rpc` | bool | mvp | 支持 RPC payload |
| `0x0003` | `protocol.payload.stream` | bool | mvp | 支持 STREAM payload |
| `0x0009` | `protocol.reservedRequestIdWidth` | - | reserved | 历史 requestId 宽度协商位；v1 固定为 uint32 |

---

### 6. 设备基础能力注册表

| capabilityId | name | 类型 | 状态 | 说明 |
|---:| --- |---| --- |---|
| `0x0101` | `device.info` | bool | mvp | 设备基础信息 |
| `0x0102` | `device.identity` | object | draft | 设备身份字段 |
| `0x0103` | `device.state` | object | draft | 设备状态 |
| `0x0104` | `device.power` | object | draft | 电源能力 |
| `0x0105` | `device.indicator` | object | draft | 指示灯/蜂鸣能力 |
| `0x0106` | `device.inventory` | object | draft | 设备库存/模块能力 |
| `0x0107` | `device.childDevice` | object | draft | 子设备能力 |

---

### 7. 能力查询能力注册表

当前没有已采纳的 capability 查询 method。后续若采纳 `capability.getRegistry / capability.getDomainRegistry / capability.hasMethod / capability.getLimits / capability.negotiate` 等能力，应继续分配在 `0x02xx`。

---

### 8. 系统能力注册表

当前 v1 Core 暂无必须实现的 `system.*` capability。后续重启、时间同步、恢复出厂、功耗策略等系统能力统一分配在 `0x03xx`。

---

### 9. 固件 / 固件更新能力注册表

| capabilityId | name | 类型 | 状态 | 说明 |
|---:| --- |---| --- |---|
| `0x0401` | `firmware.update` | object | mvp | 支持基于 STREAM 的固件更新 |
| `0x0402` | `firmware.info` | object | draft | 固件信息能力 |
| `0x0403` | `firmware.updatePolicy` | object | draft | 固件更新策略能力 |

#### 9.1 固件升级校验算法 bitmap

| Bit | 算法 |
|---:| --- |
| 0 | `CRC32` |
| 1 | `MD5` |
| 2 | `SHA1` |
| 3 | `SHA256` |
| 4 | `SIGNATURE` |

MVP 推荐：`CRC32 + SHA256`

---

### 10. STREAM 能力注册表

`stream.*` 只表达公共流控、Profile 摘要和数据面参数，不表达文件、固件、媒体或日志等业务分类。

| capabilityId | name | 类型 | 状态 | 说明 |
|---:| --- |---| --- |---|
| `0x0501` | `stream.profile` | object | mvp | 支持的 Stream Profile 摘要 |
| `0x0502` | `stream.flowControl` | object | mvp | 公共流控能力 |

#### 10.1 stream.profile

Stream Profile 是具体可建流协议档案，存在于 Registry/Capability/Stream Context 中，不存在于 STREAM L2 Header。

| profileId | Profile |
|---:| --- |
| `0x0401` | `firmware.update` |
| `0x1002` | `file.transfer` |
| `0x1101` | `log.stream` |
| `0x0801` | `video.stream` |
| `0x0902` | `audio.recording` |
| `0x0A01` | `input.hid` |
| `0x1301` | `sensor.sample` |
| `0x8001` | `legacy.tunnel` |

#### 10.2 stream.reliableModes bitmap

| Bit | 模式 | 说明 |
|---:| --- |---|
| 0 | `BEST_EFFORT` | 不保证可靠 |
| 1 | `STOP_AND_WAIT` | 停等确认 |
| 2 | `SLIDING_WINDOW` | 滑动窗口 |

---

### 11. 显示能力注册表

| capabilityId | name | 类型 | 状态 | 说明 |
|---:| --- |---| --- |---|
| `0x0601` | `display.brightness` | object | mvp | 亮度能力，包含 min/max/step/autoMode 等字段 |
| `0x0602` | `display.color` | object | draft | 色彩能力 |
| `0x0603` | `display.backlight` | object | draft | 背光能力 |
| `0x0604` | `display.power` | object | draft | 显示电源能力 |
| `0x0605` | `display.input` | object | draft | 显示输入状态能力 |
| `0x0606` | `display.output` | object | draft | 显示输出状态能力 |

迁移说明：旧 `display.brightnessMin / display.brightnessMax / display.brightnessStep` 收敛为 `display.brightness` capability schema 字段，不再作为主 capability。

---

### 12. 摄像头能力注册表

当前 v1 Core 暂无必须实现的 `camera.*` capability。后续变焦、帧率、图像参数等摄像头能力统一分配在 `0x07xx`。

---

### 13. 视频能力注册表

| capabilityId | name | 类型 | 状态 | 说明 |
|---:| --- |---| --- |---|
| `0x0801` | `video.framing` | object | draft | 取景/构图能力 |
| `0x0802` | `video.outputTransform` | object | draft | 输出变换能力 |
| `0x0803` | `video.pip` | object | draft | 画中画能力 |
| `0x0804` | `video.encoder` | object | draft | 编码能力，codec 是字段 |
| `0x0805` | `video.osd` | object | draft | OSD 能力 |
| `0x0806` | `video.overlay` | object | draft | 叠加层能力 |
| `0x0807` | `video.layout` | object | draft | 视频布局能力 |
| `0x0808` | `video.scene` | object | draft | 视频场景能力 |
| `0x0809` | `video.recording` | object | draft | 视频录制能力 |
| `0x080A` | `video.stream` | object | draft | 视频业务流能力 |
| `0x080B` | `video.rtsp` | object | draft | RTSP 服务能力 |
| `0x080C` | `video.ndi` | object | draft | NDI 服务能力 |

---

### 14. 音频能力注册表

| capabilityId | name | 类型 | 状态 | 说明 |
|---:| --- |---| --- |---|
| `0x0901` | `audio.algorithm` | object | draft | 音频算法能力 |
| `0x0902` | `audio.eq` | object | draft | EQ 能力 |
| `0x0903` | `audio.volume` | object | draft | 音量能力 |
| `0x0904` | `audio.mixer` | object | draft | 混音能力 |
| `0x0905` | `audio.routing` | object | draft | 音频路由能力 |
| `0x0906` | `audio.input` | object | draft | 音频输入能力 |
| `0x0907` | `audio.output` | object | draft | 音频输出能力 |
| `0x0908` | `audio.recording` | object | draft | 音频录制能力 |
| `0x0909` | `audio.playback` | object | draft | 音频播放能力 |
| `0x090A` | `audio.uac` | object | draft | UAC 能力 |
| `0x090B` | `audio.dante` | object | draft | Dante 能力 |

---

### 15. 输入 / KVM 能力注册表

| capabilityId | name | 类型 | 状态 | 说明 |
|---:| --- |---| --- |---|
| `0x0A01` | `input.key` | object | draft | 按键能力 |
| `0x0A02` | `input.hid` | object | draft | HID 能力 |
| `0x0A03` | `input.source` | object | draft | 输入源能力 |
| `0x0A04` | `input.kvm` | object | draft | KVM 能力 |
| `0x0A05` | `input.gpio` | object | draft | GPIO 能力 |

---

### 16. 输出能力注册表

| capabilityId | name | 类型 | 状态 | 说明 |
|---:| --- |---| --- |---|
| `0x0B01` | `output.source` | object | draft | 输出源能力 |
| `0x0B02` | `output.routing` | object | draft | 输出路由能力 |
| `0x0B03` | `output.layout` | object | draft | 输出布局能力 |

---

### 17. 会议室 / 协作空间能力注册表

| capabilityId | name | 类型 | 状态 | 说明 |
|---:| --- |---| --- |---|
| `0x0C01` | `room.info` | object | draft | 会议室信息能力 |
| `0x0C02` | `room.schedule` | object | draft | 日程能力 |
| `0x0C03` | `room.source` | object | draft | 会议室输入源能力 |
| `0x0C04` | `room.layout` | object | draft | 会议室布局能力 |
| `0x0C05` | `room.participant` | object | draft | 参会者能力 |

---

### 18. 数字标牌能力注册表

| capabilityId | name | 类型 | 状态 | 说明 |
|---:| --- |---| --- |---|
| `0x0D01` | `signage.media` | object | draft | 标牌媒体能力 |
| `0x0D02` | `signage.playlist` | object | draft | 播放列表能力 |
| `0x0D03` | `signage.schedule` | object | draft | 播放计划能力 |
| `0x0D04` | `signage.playback` | object | draft | 播放能力 |
| `0x0D05` | `signage.osd` | object | draft | OSD 能力 |

---

### 19. 网络能力注册表

| capabilityId | name | 类型 | 状态 | 说明 |
|---:| --- |---| --- |---|
| `0x0E01` | `network.interface` | object | draft | 网络接口能力 |
| `0x0E02` | `network.ip` | object | draft | IP 配置能力 |
| `0x0E03` | `network.wifi` | object | draft | Wi-Fi 能力 |
| `0x0E04` | `network.ap` | object | draft | AP 能力 |
| `0x0E05` | `network.bluetooth` | object | draft | Bluetooth 能力 |
| `0x0E06` | `network.serviceEndpoint` | object | draft | 服务端点发现能力 |

---

### 20. 存储能力注册表

| capabilityId | name | 类型 | 状态 | 说明 |
|---:| --- |---| --- |---|
| `0x0F01` | `storage.sdCard` | object | draft | SD 卡能力 |
| `0x0F02` | `storage.disk` | object | draft | 磁盘能力 |
| `0x0F03` | `storage.volume` | object | draft | 卷能力 |
| `0x0F04` | `storage.media` | object | draft | 媒体资源能力 |
| `0x0F05` | `storage.recording` | object | draft | 录制资源能力 |
| `0x0F06` | `storage.index` | object | draft | 索引能力 |

---

### 21. 文件能力注册表

| capabilityId | name | 类型 | 状态 | 说明 |
|---:| --- |---| --- |---|
| `0x1001` | `file.transfer` | object | draft | 文件传输能力 |
| `0x1002` | `file.storage` | object | draft | 文件存储能力 |

---

### 22. 日志能力注册表

| capabilityId | name | 类型 | 状态 | 说明 |
|---:| --- |---| --- |---|
| `0x1101` | `log.stream` | object | draft | 实时日志流能力 |
| `0x1102` | `log.export` | object | draft | 日志导出能力 |
| `0x1103` | `log.files` | object | draft | 日志文件列表与元信息能力 |

---

### 23. 诊断能力注册表

| capabilityId | name | 类型 | 状态 | 说明 |
|---:| --- |---| --- |---|
| `0x1201` | `diagnostic.selfTest` | object | draft | 自检能力 |
| `0x1202` | `diagnostic.networkTest` | object | draft | 网络测试能力 |
| `0x1203` | `diagnostic.audioTest` | object | draft | 音频测试能力 |
| `0x1204` | `diagnostic.videoTest` | object | draft | 视频测试能力 |
| `0x1205` | `diagnostic.storageTest` | object | draft | 存储测试能力 |
| `0x1206` | `diagnostic.inputTest` | object | draft | 输入测试能力 |
| `0x1207` | `diagnostic.kvmTest` | object | draft | KVM 测试能力 |
| `0x1208` | `diagnostic.calibration` | object | draft | 校准能力 |
| `0x1209` | `diagnostic.manufacturing` | object | draft | 产测能力 |
| `0x120A` | `diagnostic.report` | object | draft | 诊断报告能力 |

---

### 24. 传感器能力注册表

| capabilityId | name | 类型 | 状态 | 说明 |
|---:| --- |---| --- |---|
| `0x1301` | `sensor.state` | object | draft | 传感器状态能力 |
| `0x1302` | `sensor.sample` | object | draft | 传感器采样能力 |

---

### 25. 认证能力注册表

| capabilityId | name | 类型 | 状态 | 说明 |
|---:| --- |---| --- |---|
| `0x1401` | `auth.session` | object | draft | 认证会话能力 |
| `0x1402` | `auth.permission` | object | draft | 权限能力 |
| `0x1403` | `auth.token` | object | draft | token 能力 |

---

### 26. 隐私能力注册表

| capabilityId | name | 类型 | 状态 | 说明 |
|---:| --- |---| --- |---|
| `0x1501` | `privacy.cover` | object | draft | 隐私盖能力 |
| `0x1502` | `privacy.mode` | object | draft | 隐私模式能力 |
| `0x1503` | `privacy.state` | object | draft | 隐私状态能力 |

---

### 26b. 软件对象能力注册表

| capabilityId | name | 类型 | 状态 | 说明 |
|---:| --- |---| --- |---|
| `0x1601` | `software.config` | object | draft | 软件对象运行配置能力（读取、设置、恢复默认、配置变化通知） |
| `0x1602` | `software.updatePolicy` | object | draft | 软件对象自动更新策略能力（读取、设置、恢复默认、策略变化通知） |

---

### 27. Vendor Capability

厂商私有能力范围：`0x7000-0x7FFF`

规则：
- 不得覆盖标准 capabilityId
- 必须带 vendorId
- 必须带 capability name
- 必须带 type
- 必须声明是否影响互操作

```yaml
- id: 0x7001
  name: vendor.aw.customLedMatrix
  domain: vendor
  vendorId: 0x1234
  status: draft
  type: object
  description: Vendor-specific LED matrix capability.
  interoperable: false
```

---

### 28. Capability 查询方法

后续建议能力查询：

| methodId | methodName | 说明 |
|---:| --- |---|
| `0x0202` | `capability.getRegistry` | 获取完整能力摘要 |
| `0x0203` | `capability.getDomainRegistry` | 获取指定 domain 能力 |
| `0x0204` | `capability.hasMethod` | 判断 methodId 是否支持 |
| `0x0205` | `capability.getLimits` | 获取协议和传输限制 |
| `0x0206` | `capability.negotiate` | 业务能力协商 |

---

### 29. capability.getRegistry 返回结构（v2/P1）

#### 29.1 JSON 表达

```json
{
  "protocol": {
    "payloadTypes": ["CONTROL", "RPC", "STREAM"],
    "frameProfile": "STANDARD_FRAME",
    "frameVersion": 1,
    "frameCrcProfiles": { "STANDARD": "CRC16" },
    "sessionResume": true
  },
  "transport": {
    "type": "USB_HID",
    "maxFrameSize": 1024,
    "mtu": 1024,
    "ackMode": "MESSAGE_ACK",
    "windowSize": 1
  },
  "rpc": {
    "encodings": ["JSON_BINARY"],
    "bodyEncodings": ["TLV8"],
    "event": true
  },
  "stream": {
    "profile": ["firmware.update"],
    "maxChunkSize": 48,
    "resume": true,
    "chunkCrc32": true
  },
  "business": {
    "display": { "brightness": { "supported": true, "min": 0, "max": 100, "step": 1 } },
    "firmware": { "supported": true, "imageTypes": ["MCU_FIRMWARE"], "verify": ["CRC32", "SHA256"], "resume": true }
  }
}
```

#### 29.2 TLV 表达

v2/P1 推荐返回摘要（复杂能力通过 `capability.getDomainRegistry` 查询）：

```text
capability.getRegistry.result
  protocol.payload.control / protocol.payload.rpc / protocol.payload.stream
  stream.profile
  firmware.update / display.brightness
```

---

### 30. capability.getDomainRegistry 参数

参数：

| 字段 | fieldId | 类型 | 说明 |
| --- |---:| --- |---|
| domain | `0x01` | enum/string | 能力域 |

支持 domain：`protocol / device / capability / system / firmware / stream / display / camera / video / audio / input / output / room / signage / network / storage / file / log / diagnostic / sensor / auth / privacy / vendor`

---

### 31. capability.hasMethod 参数与结果

参数：

| 字段 | fieldId | 类型 | 说明 |
| --- |---:| --- |---|
| methodId | `0x01` | uint16 | 方法 ID |

结果：

| 字段 | fieldId | 类型 | 说明 |
| --- |---:| --- |---|
| supported | `0x01` | bool | 是否支持 |
| reasonCode | `0x02` | uint16 | 不支持原因 |
| minFirmwareVersion | `0x03` | string | 最低固件版本 |

---

### 32. Capability 与 Method / Event / Stream 的关系

- Method Registry 表示协议定义了哪些方法；Capability 表示当前设备实际支持哪些方法
- Event Registry 表示协议定义了哪些事件；Capability 表示当前设备是否会上报该事件
- StreamProfile 存在不代表当前设备支持该 Stream；必须通过 `stream.profile` 或业务域 feature capability 判断

---

### 33. 老协议适配

老协议能力（设备信息命令、能力矩阵命令、Feature bitmap、CmdValue 支持表）必须统一转换为 Capability Registry，不得继续让上层直接判断旧字段。

| 老协议能力 | AXTP capability | 说明 |
| --- |---| --- |
| `CmdValue 0xC0021 exists` | `video.supported = true` | 支持视频模式设置 |
| `AlphaUpgradeInfo exists` | `firmware.update = true` | 支持基于 STREAM 的固件更新 |
| `FeatureBitmap.bit0` | `display.brightness = true` | 支持亮度 |
| `FeatureBitmap.bit1` | `firmware.update.resumeSupported = true` | 支持续传字段 |

legacyMapping 字段：

```yaml
- id: 0x0401
  name: firmware.update
  domain: firmware
  type: bool
  status: mvp
  legacyMapping:
    source: AXDP
    rule: "AlphaUpgradeInfo command exists"
    legacyCmdValues:
      - 0xA0001
```

---

### 34. 当前生成 Capability 集合

Capability 集合以 `contract/registry/capability/capability_registry.yaml` 与 `contract/registry/domains/<domain>/domain.yaml` 为事实源。Capability 采用 `domain.feature` 能力块表达；字段级限制进入 feature capability schema。下列集合是当前生成合同。

| capabilityId | capabilityName | Type | 说明 |
|---:| --- |---| --- |
| `0x0001` | `protocol.payload.control` | bool | 支持 CONTROL payload |
| `0x0002` | `protocol.payload.rpc` | bool | 支持 RPC payload |
| `0x0003` | `protocol.payload.stream` | bool | 支持 STREAM payload |
| `0x0009` | `protocol.reservedRequestIdWidth` | reserved | 历史 requestId 位宽协商占位；AXTP v1 固定 requestId 为 uint32 |
| `0x0901` | `audio.algorithm` | object | 支持音频算法能力查询、配置、重置和变化通知 |

---

### 35. Generator v1 校验规则

```text
CapabilityId 唯一性 / name 唯一性 / ID 范围合法性
status 合法性 / type 合法性
deprecated Capability 不得复用 ID
legacyMapping 指向合法 Capability
```
