# Profile 候选规划表

> 本附录为非规范性内容。
> 本附录保留历史规划表和候选 registry 条目。
> Runtime 实现不得将本附录视为实现合同。
> 已采纳条目必须以 registry YAML 和 generated protocol 产物为准。

来源文档： `../05-Profiles-Registry.md`

## MVP 最小实现注册表

版本：v1.1
状态：MVP / 实现合同（精简版）
适用范围：AXTP v1 第一阶段落地最小注册表集合

---

### 1. MVP 设计原则

#### 1.1 只实现闭环，不追求全集

MVP 必须覆盖：建连、基础 RPC 请求/响应、基础事件承载、心跳保活、优雅关闭、STREAM 数据面、错误处理和生成产物一致性。ACK/NACK 严格重传、业务能力查询、业务事件和 legacy CmdValue 兼容必须由已采纳草案或后续 profile 显式加入。

MVP 暂不覆盖：完整音视频能力、完整文件系统、完整日志系统、完整诊断系统、完整输入/KVM、完整网络配置、完整权限系统、完整安全加密、完整压缩、完整多路复用调度、复杂 Batch RPC、复杂对象 Schema 反射。

#### 1.2 PayloadType 只保留三种

AXTP MVP 固定实现三种 PayloadType：`CONTROL / RPC / STREAM`。

STREAM 数据包不携带 `streamProfile`。具体业务 Stream Profile 必须由已采纳业务建流方法协商并绑定到 `streamId`；通用 stream 建流方法不作为 AXTP v1 主接口，业务流必须由业务域创建。

#### 1.3 Registry / Schema 是单一事实源

MVP 文档中的表格用于阅读和审查，最终实现以 `contract/registry/**/*.yaml` 与 `contract/registry/domains/**/*.yaml` 为单一事实源。`contract/protocol/axtp.protocol.yaml` 与 `contract/generated/*` 均为 Generator 输出。

#### 1.4 老协议优先兼容，不推翻重写

```text
旧 CmdValue -> AXTP methodId
旧 Payload -> AXTP TLV8/TLV16 body，或由 legacy adapter 在协议边界转换
旧 status -> AXTP errorCode
旧设备能力表 -> AXTP capability
旧固件更新数据块 -> 后续 AXTP STREAM 固件更新 profile
```

对于复杂旧结构，Phase 1 不新增原始字节透传 bodyEncoding；必须在 runtime legacy adapter 中转成已采纳 schema，或等待后续 profile 明确透传策略。

---

### 2. MVP 实现总览

| 类别 | MVP 必须实现 | 延后实现 |
| --- |---| --- |
| PayloadType | CONTROL / RPC / STREAM | 新增 PayloadType |
| Frame Profile | 当前 v1 Core 使用 STANDARD_FRAME 或 WebSocket Unframed JSON；HID-64/BLE/UART Compact 进入降级文档 | 新增低带宽 Frame Profile |
| Control | OPEN / ACCEPT / HEARTBEAT / HEARTBEAT_ACK / CLOSE / CLOSE_ACK | ACK / NACK / RESUME / SESSION_RESET / WINDOW_UPDATE 高级策略 |
| RPC Encoding | JSON 必须覆盖当前正式路径；JSON_BINARY 是 Standard Framed 优化路径 | CBOR / MessagePack |
| RPC Op | HELLO / IDENTIFY / IDENTIFIED / EVENT / REQUEST / REQUEST_RESPONSE | BATCH |
| Stream Profile | audio/video 媒体流数据面 | firmware/file/log/kvm/sensor 数据面 |
| Type System | uint / bool / enum / bitmap / string / bytes | nested object 高级约束 |
| TLV | short TLV 必须，extended length 建议 | packed array 高级优化 |
| Registry | Method/Event/Error/Capability | 完整业务全集 |

---

### 3. MVP PayloadType 注册表

| ID | 名称 | Parser | MVP |
|---:| --- |---| --- |
| `0x01` | `CONTROL` | `ControlParser` | 必须 |
| `0x02` | `RPC` | `RpcParser` | 必须 |
| `0x03` | `STREAM` | `StreamParser` | 必须 |

---

### 4. MVP Frame Profile

#### 4.1 Standard Profile（必须）

```text
+--------+--------+---------+-------------+
| Magic  | Ver/PT | Length  | Src/Dst/Msg |
+--------+--------+---------+-------------+
| Payload...                       |
+----------------------------------+
| CRC16                            |
+----------------------------------+
```

| 字段 | 长度 | 说明 |
| --- |---:| --- |
| `Magic` | 2B | 固定 `AX` |
| `Version/PayloadType` | 2B | Header 版本与 PayloadType |
| `PayloadLength` | 2B | Payload 字节数 |
| `SourceId/DestinationId/MessageId/FrameInfo` | 6B | 路由与分片字段 |
| `CRC16` | 2B | Footer，覆盖 Header(12B) + Payload |

Standard Profile 是 AXTP v1 Core 的正式二进制帧路径，适用于 AXTP-USB-HID 与 AXTP-TCP。

#### 4.2 低带宽降级

Compact / HID-64 / BLE / UART 不作为当前 MVP 必选实现，进入 `specs/1-core/08-Low-Bandwidth-Degradation.md`。

---

### 5. MVP Control Opcode 注册表

#### 5.1 MVP 必须实现

| Opcode | Name | Direction | Body | 说明 |
| ---: | --- | --- | --- | --- |
| `0x01` | `OPEN` | Client → Device | TLV | 建立协议会话，声明协议能力 |
| `0x02` | `ACCEPT` | Device → Client | TLV | 返回协商结果 |
| `0x04` | `HEARTBEAT` | Both | Optional TLV | 保活 |
| `0x05` | `HEARTBEAT_ACK` | Both | Optional TLV | 保活响应 |
| `0x06` | `ACK` | Both | TLV | 确认 Frame / Message / Stream Chunk |
| `0x07` | `NACK` | Both | TLV | 否认 Frame / Message / Stream Chunk |
| `0x0A` | `CLOSE` | Both | Optional TLV | 主动关闭会话 |
| `0x0B` | `CLOSE_ACK` | Both | Optional TLV | 关闭确认 |

#### 5.2 MVP 可延后

| Opcode | Name | 延后原因 |
| ---: | --- | --- |
| `0x03` | `READY` | 三步协商预留，当前版本不实现；收到时必须忽略 |
| `0x08` | `RESUME` | 断点恢复可先由业务层 firmware.update resume 覆盖 |
| `0x09` | `RESUME_ACK` | 同上 |
| `0x0C` | `SESSION_RESET` | 可先使用 CLOSE + OPEN 重建 |
| `0x0D` | `WINDOW_UPDATE` | 第一版可使用固定窗口 |
| `0x0E` | `PING` | HEARTBEAT 已能覆盖基本保活 |
| `0x0F` | `PONG` | 同上 |
| `0x10` | `GOAWAY` | P2 |

#### 5.3 Control TLV MVP 字段

| fieldId | Name | Type | 用途 |
|---:| --- |---| --- |
| `0x01` | `sessionId` | `uint32` | 会话 ID |
| `0x02` | `protocolVersion` | `uint8` | Deprecated/Transition；v1 新实现省略 |
| `0x03` | `reserved` | - | 历史 `headerProfile`，v1 不得使用 |
| `0x04` | `maxFrameSize` | `uint16` | 最大 Standard Frame 总字节数 |
| `0x05` | `maxPayloadSize` | `uint16` | Deprecated/Reserved；由 `maxFrameSize - 14` 推导 |
| `0x06` | `mtu` | `uint16` | Profile-specific Optional 传输 MTU |
| `0x07` | `supportedPayloadTypes` | `bitmap32` | 支持的 PayloadType |
| `0x08` | `supportedRpcEncodings` | `bitmap32` | 支持的 RPC Encoding |
| `0x09` | `reserved` | - | 历史 `supportedStreamProfiles`，新实现不得使用 |
| `0x0A` | `heartbeatIntervalMs` | `uint16` | 心跳间隔 |
| `0x0B` | `ackMode` | `uint8` | ACK 模式 |
| `0x0C` | `windowSize` | `uint16` | 固定窗口 |
| `0x10` | `reasonCode` | `uint16` | 关闭或错误原因 |
| `0x11` | `messageId` | `uint16` | 被确认的 Message |
| `0x12` | `frameIndex` | `uint8` | 被确认的 Frame |
| `0x13` | `frameCount` | `uint8` | 总分片数 |
| `0x15` | `streamId` | `uint32` | 被确认的 Stream |
| `0x16` | `seqId` | `uint32` | 被确认的 Stream Chunk |
| `0x20` | `targetType` | `uint8` | ACK/NACK 目标类型 |

#### 5.4 ACK targetType

| Value | Name | 说明 |
|---:| --- |---|
| `0x01` | `FRAME` | 确认某个 Frame 分片 |
| `0x02` | `MESSAGE` | 确认完整 Message |
| `0x03` | `STREAM_CHUNK` | 确认某个 Stream Chunk |
| `0x04` | `CONTROL` | 确认某个 Control 消息 |

---
### 6. MVP RPC 注册表

#### 6.1 RPC Encoding

| ID | Name | MVP | 说明 |
|---:| --- |---| --- |
| `0x01` | `JSON` | 必须 | WebSocket Unframed JSON 与 framed JSON |
| `0x02` | `CBOR` | 延后 | 复杂对象 |
| `0x03` | `MSGPACK` | 延后 | 高级动态编码 |
| `0x04` | `JSON_BINARY` | 建议 | AXTP-USB-HID / AXTP-TCP；bodyEncoding 推荐 TLV8 |

`JSON_BINARY+TLV8` 不是独立 wire enum，只是 `rpcEncoding=JSON_BINARY + bodyEncoding=TLV8` 的推荐组合名称。

#### 6.2 RPC Body Encoding

| ID | Name | MVP | 说明 |
|---:| --- |---| --- |
| `0x00` | `NONE` | 必须 | 无 body |
| `0x01` | `TLV8` | 必须 | 1B fieldId + 1B length 的结构化参数、结果和事件数据 |
| `0x02` | `TLV16` | 延后 | 扩展长度 TLV |

#### 6.3 RPC Op

| rpcOp | Name | MVP |
|---:| --- |---|
| `0x00` | `HELLO` | 必须 |
| `0x02` | `IDENTIFY` | 必须 |
| `0x03` | `IDENTIFIED` | 必须 |
| `0x06` | `EVENT` | 必须 |
| `0x07` | `REQUEST` | 必须 |
| `0x08` | `REQUEST_RESPONSE` | 必须 |
| `0x09` | `REQUEST_BATCH` | 延后 |
| `0x0A` | `REQUEST_BATCH_RESPONSE` | 延后 |

---

### 7. Profile Method 要求

#### 7.1 AXTP-MVP Required Methods

本节应与 `contract/registry/capability/mvp_profile.yaml` 保持一致；如存在差异，以 YAML/generated 为当前实现事实源，并应回修本表。当前 AXTP-MVP / AXTP-MVP-HID 不强制任何业务 method；业务 method 只有在已采纳草案写入 YAML 并由 Generator 生成后，才可被产品或 domain profile 引用。

| Profile | Required Methods | 说明 |
|---|---|---|
| `AXTP-MVP` | none | Core 只固定传输、帧、CONTROL/RPC/STREAM 和错误语义 |
| `AXTP-MVP-HID` | none | 继承 `AXTP-MVP`，限制 transport 为 `AXTP-USB-HID` |

#### 7.2 当前生成业务方法

当前 generated registry 中的业务方法来自已采纳 `audio.algorithm` 草案，但不自动成为 AXTP-MVP profile requiredMethods：

| methodId | methodName | Domain | Status | 说明 |
|---:|---|---|---|---|
| `0x090D` | `audio.getAlgorithmCapabilities` | `audio` | stable | 查询音频算法字段、范围、默认值和更新策略 |
| `0x0901` | `audio.getAlgorithmConfig` | `audio` | stable | 查询当前音频算法配置 |
| `0x0902` | `audio.setAlgorithmConfig` | `audio` | stable | 部分更新音频算法配置 |
| `0x090E` | `audio.resetAlgorithmConfig` | `audio` | stable | 恢复全部、指定算法或指定字段默认值 |

---

### 8. Profile Event 要求

本节应与 `contract/registry/capability/mvp_profile.yaml` 保持一致；如存在差异，以 YAML/generated 为当前实现事实源，并应回修本表。当前 AXTP-MVP / AXTP-MVP-HID 不强制任何业务 event。

`bitOffset` 为该事件在其 Domain 内的掩码位偏移，用于 `eventMasks` 域级订阅（见 `specs/4-tooling/01-YAML-Mapping.md`§23）。

| eventId | eventName | Domain | domainId | bitOffset | Status | 说明 |
| ---: | --- | --- | ---: | ---: | --- | --- |
| `0x0901` | `audio.algorithmConfigChanged` | `audio` | `0x09` | 0 | stable | 音频算法配置变化通知 |

---

### 9. MVP ErrorCode 注册表

本节应与 `contract/registry/error/error_code.yaml` 及 `contract/registry/domains/<domain>/domain.yaml` 保持一致；如存在差异，以 YAML/generated 为当前实现事实源，并应回修本表。更完整的错误码规划见 `specs/2-registry/04-Errors-Registry.md`，未进入 YAML 的错误名不得作为当前实现的 wire 合同。

| errorCode | Name | Layer | Retryable | 说明 |
|---:| --- |---| --- |---|
| `0x0000` | `SUCCESS` | common | false | 成功 |
| `0x0001` | `UNKNOWN_ERROR` | common | false | 未知错误 |
| `0x0005` | `BUSY` | common | true | 设备或资源忙 |
| `0x0012` | `FRAME_VERSION_UNSUPPORTED` | frame | false | Frame Version 不支持 |
| `0x0016` | `FRAME_CRC_ERROR` | frame | true | Frame CRC 错误 |
| `0x0018` | `FRAME_FRAGMENT_MISSING` | frame | true | 缺失 Frame 分片 |
| `0x0021` | `CONTROL_OPCODE_INVALID` | control | false | Control Opcode 非法 |
| `0x0022` | `CONTROL_PAYLOAD_INVALID` | control | false | Control Payload 非法 |
| `0x0024` | `CONTROL_OPEN_REQUIRED` | control | false | 会话尚未完成 OPEN |
| `0x0025` | `CONTROL_OPEN_REJECTED` | control | false | OPEN 被拒绝 |
| `0x0026` | `RESERVED_CONTROL_PROFILE_UNSUPPORTED` | control | false | 历史 Header Profile 协商错误，v1 新实现不得产生 |
| `0x0027` | `CONTROL_NEGOTIATION_FAILED` | control | false | 协商失败 |
| `0x0028` | `CONTROL_SESSION_INVALID` | control | false | SessionId 无效 |
| `0x002A` | `CONTROL_RESUME_FAILED` | control | false | 会话恢复失败 |
| `0x002C` | `CONTROL_WINDOW_EXCEEDED` | control | true | 超出流控窗口 |
| `0x0031` | `RPC_ENCODING_UNSUPPORTED` | rpc | false | RPC 编码不支持 |
| `0x0036` | `RPC_METHOD_NOT_FOUND` | rpc | false | methodId 或 method name 不支持 |
| `0x003B` | `RPC_PARAM_INVALID` | rpc | false | RPC 参数非法 |
| `0x0501` | `STREAM_NOT_FOUND` | stream | false | Stream Context 不存在 |
| `0x0502` | `STREAM_TIMEOUT` | stream | true | Stream 超时 |
| `0x0503` | `STREAM_CRC_ERROR` | stream | true | Chunk CRC 错误 |
| `0x040B` | `FW_VERIFY_FAILED` | firmware | false | 固件校验失败 |

---

### 10. 当前 Capability 注册表

本节应与 `contract/registry/capability/capability_registry.yaml` 和 `contract/registry/domains/**` 保持一致；如存在差异，以 YAML/generated 为当前实现事实源，并应回修本表。Profile 不使用 `requiredCapabilities` 字段，capability 只作为 registry 事实和业务方法关联。

`bitOffset` 为该 capability 在其 Domain 内的掩码位偏移，用于 `capabilityMasks` 域级响应（见 `specs/4-tooling/01-YAML-Mapping.md`§23）。

| capabilityId | capabilityName | domainId | bitOffset | Type | Status |
| ---: | --- | ---: | ---: | --- | --- |
| `0x0001` | `protocol.payload.control` | `0x00` | 0 | `bool` | stable |
| `0x0002` | `protocol.payload.rpc` | `0x00` | 1 | `bool` | stable |
| `0x0003` | `protocol.payload.stream` | `0x00` | 2 | `bool` | stable |
| `0x0901` | `audio.algorithm` | `0x09` | 0 | `object` | stable |

---

### 11. MVP Stream 注册表

#### 11.1 Stream Profile

Stream Profile 是可建流协议档案，不是 STREAM 数据包字段。Profile 由 RPC 建流方法协商并绑定到 `streamId`。

| profileId | Name | Domain | MVP | 默认 cursorUnit | 默认可靠性 |
|---:| --- |---| --- |---| --- |
| `0x0401` | `firmware.update` | firmware | 必须 | `byteOffset` | reliable |
| `0x1002` | `file.transfer` | file | 延后 | `byteOffset` | reliable |
| `0x1101` | `log.stream` | log | 延后 | `timestampUs` | best_effort |
| `0x0801` | `video.stream` | video | 延后 | `timestampUs` | best_effort |
| `0x0902` | `audio.recording` | audio | 延后 | `timestampUs` | best_effort |

#### 11.2 固件升级 Stream MVP Metadata

| fieldId | Name | Type | 说明 |
|---:| --- |---| --- |
| `0x01` | `transferId` | `uint32` | 传输 ID |
| `0x02` | `imageType` | `uint8` | 镜像类型 |
| `0x03` | `offset` | `uint32` | 当前偏移 |
| `0x04` | `chunkIndex` | `uint32` | 当前块序号 |
| `0x05` | `chunkSize` | `uint16` | 当前块大小 |
| `0x06` | `chunkCrc32` | `uint32` | 当前块 CRC32 |
| `0x07` | `totalSize` | `uint32` | 总大小 |
| `0x08` | `totalHash` | `bytes` | 完整镜像 Hash，可选 |

#### 11.3 imageType MVP

| imageType | Name | MVP |
|---:| --- |---|
| `0x01` | `MCU_FIRMWARE` | 必须 |
| `0x02` | `LINUX_FIRMWARE` | 建议 |
| `0x03` | `RESOURCE_PACKAGE` | 建议 |
| `0x7F` | `VENDOR_IMAGE` | 延后 |

---

### 12. MVP Type System 子集

| Type | MVP | 用途 |
| --- |---| --- |
| `uint8` | 必须 | enum、flags、小数值 |
| `uint16` | 必须 | length、status、size |
| `uint32` | 必须 | id、offset、crc32 |
| `uint64` | 建议 | timestamp、大文件 offset |
| `bool` | 必须 | capability、状态 |
| `enum` | 必须 | PayloadType、imageType |
| `bitmap32` | 必须 | 能力集合 |
| `string` | 必须 | 设备名、版本号 |
| `bytes` | 必须 | Hash、legacy payload |
| `array` | 建议 | method/event/capability 列表 |
| `object` | 延后 | 复杂嵌套结构 |

---

### 13. MVP TLV 子集

MVP TLV 必须支持：

```text
fieldId:uint8 / length:uint8 / value:N
```

建议同时支持扩展长度：

```text
length = 0xFF / extendedLength:uint16 / value:N
```

解码器必须满足：支持未知字段跳过、字段顺序无关、required 字段校验、重复字段报错、deprecated 字段忽略、bytes passthrough。

编码器必须满足：按 fieldId 升序输出、不输出 default 值（除非 schema 要求）、不输出 unknown 字段、不输出 deprecated 字段。

---

### 14. 当前已采纳业务 Schema 列表

本节只列已经进入手写事实源并可由 Generator 生成的业务 schema。草案、迁移 intake、历史 MVP 示例不得补进本清单；新增 schema 必须先完成业务草案评审，并写入 `contract/registry/domains/<domain>/domain.yaml` 或相关 registry YAML。

#### 14.1 Audio Algorithm

```text
AudioGetAlgorithmCapabilitiesRequest / AudioGetAlgorithmCapabilitiesResponse
AudioGetAlgorithmConfigRequest / AudioAlgorithmConfig
AudioSetAlgorithmConfigRequest / AudioSetAlgorithmConfigResponse
AudioResetAlgorithmConfigRequest
AudioAlgorithmConfigChangedEvent
```

#### 14.2 Capability

```text
当前没有已采纳的业务 capability RPC schema。
```

#### 14.3 Device / Display / Firmware / Stream

```text
这些 domain 目前只有草案或规划项；采纳前不得生成业务 method/event/schema。
```

---

### 15. 老协议兼容 MVP

最小兼容目标：旧 CmdValue 可以映射到 AXTP methodId；旧 Payload 由 legacy adapter 转成 AXTP TLV8/TLV16 body；旧返回码可以映射到 AXTP errorCode；旧设备能力可以映射到 AXTP capability；旧固件更新流程后续映射到 firmware.* + STREAM 固件更新 profile。

每个从旧协议迁移的方法都应声明：

```yaml
legacyMapping:
  protocol: AXDP_HID
  cmdValue: 0xC0021
  oldName: CommonSetVideoMode
  bodyEncoding: TLV8
  migration: keep_cmd_value_as_method_id
```

旧协议方法适配示例。下表是历史迁移证据示例，不代表当前 registry 已采纳；只有具备旧命令、旧状态码、payload 映射和目标 AXTP 方法证据后，才允许写入 `contract/registry/legacy/legacy_mapping.yaml`。

| oldName | oldCmdValue | AXTP methodName | AXTP methodId | MVP |
| --- |---:| --- |---:| --- |
| `BetaDeviceInfo` | `0xB0002` | 待 `device.info` 采纳后确认 | 待分配或保留旧值 | 未采纳 |
| `AlphaUpgradeInfo` | `0xA0001` | 待 `firmware.update` 采纳后确认 | 待分配或保留旧值 | 未采纳 |

AXTP `methodId` 固定为 uint16，旧 CmdValue 可能超过 uint16，不得直接作为 methodId。旧 CmdValue 必须保存在 legacyMapping 中，并唯一映射到 AXTP MethodId。
