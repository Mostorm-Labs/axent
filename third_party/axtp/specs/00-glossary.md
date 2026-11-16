# AXTP 术语表

本文定义 AXTP 的共享词汇。它不定义字段布局、注册表事实、生成产物或发布状态。

## 分层

| 术语 | 含义 |
|---|---|
| AXTP | Auditoryworks Transport Protocol，由 runtime、SDK、工具链、mock server 和 conformance 仓库消费的协议标准。 |
| Transport | 承载 AXTP 字节流或 JSON 消息的传输载体，例如 TCP、USB HID、WebSocket、BLE、UART 或 mock transport。 |
| Transport Profile | 固定某种 transport 如何承载 AXTP 的规则，包括是否使用 Standard Framed Binary 或 WebSocket Unframed JSON。 |
| Frame Layer | Standard Frame 标准帧解析层，处理 magic、version、payload length、PayloadType、分片和 CRC。 |
| Payload Layer | 根据 PayloadType 分发后的 CONTROL、RPC 或 STREAM 解析层。 |
| Registry Layer | method、event、error、schema、capability 和 profile 的机器事实层。 |
| Business Layer | 设备或产品语义层，例如 `audio`、`video`、`network`、`firmware` 或 `room`。 |

## Payload 层

| 术语 | 含义 |
|---|---|
| PayloadType | 顶层解析器选择值：CONTROL=`0x01`，RPC=`0x02`，STREAM=`0x03`。它不是业务类型。 |
| CONTROL | Standard Framed 链路控制，用于 OPEN / ACCEPT、heartbeat 和 close。 |
| RPC | 业务控制面，用于 Hello / Identify / Identified、Request / Response 和 Event。 |
| RPC Message Metadata | JSON / CBOR / MSGPACK RPC envelope 中可选的 `m` object；用于不属于具体 `op` 业务数据的消息级元信息。v1 首批字段为 `src` 和 `dst`。 |
| STREAM | 为已经建立的 stream context 承载连续数据的数据面。 |
| Control Plane | 管理 session 和业务命令的 CONTROL 加 RPC 行为。 |
| Data Plane | 承载媒体、firmware、file、log 或 sensor chunk 等连续数据的 STREAM 行为。 |

## 上下文与 ID

| 术语 | 含义 |
|---|---|
| Transport connection | 底层 socket、WebSocket、HID connection 或等价 transport handle。 |
| Framed Link Context | 通过 CONTROL OPEN / ACCEPT 建立的 Standard Framed 上下文。 |
| RPC Session | 通过 RPC Hello / Identify / Identified 建立的应用 session。 |
| Stream Context | 发送 STREAM 数据前，由已采纳 RPC method 或 profile 创建的单 stream 元数据。 |
| Endpoint | 可以被 AXTP 逻辑寻址并独立接收或产生 RPC 的实体。Endpoint 可以是设备、Agent、应用实例、Cloud service、Room、Software service 或其他逻辑资源，不等同于物理连接。 |
| Endpoint Key (`endpointKey`) | 用于确定性生成 AXTP-native `endpointId` 的稳定 identity input。相同逻辑 Endpoint 的 `endpointKey` MUST 始终保持字节级一致；不同逻辑 Endpoint MUST NOT 共用同一个 `endpointKey`。 |
| Endpoint ID (`endpointId`) | Endpoint 的稳定 opaque string identity。在同一寻址/管理域内 MUST 唯一；AXTP-native sender 根据稳定 `endpointKey` 确定性生成，同一 `endpointKey` 每次得到相同 `endpointId`。它不是 `sid`、Frame `SourceId/DestinationId`、IP、USB path 或当前 parent Agent。 |
| Endpoint Provider | 当前节点内部负责到达或执行某个 Endpoint 的 provider，例如本地业务实现、协议 adapter、child AXTP session 或下级 Agent session。Provider 是本地路由事实，不是 RPC wire 路径。 |
| Endpoint Projection | Agent/relay 将下级 provider 中可寻址 Endpoint 平铺到自己的可见 Endpoint 集合，使上游只使用最终 `endpointId`，而无需知道多级 Agent 拓扑。 |
| `sessionId` | 可选的 CONTROL link identifier，用于 trace 或未来 resume；不是业务 session id。 |
| `sid` | Identify 成功后由 Logical Server 分配的 RPC Session ID。 |
| `requestId` | RPC request/response 关联 id。 |
| `messageId` | Standard Frame 分片、重组和调试 id。 |
| `streamId` | STREAM context id。 |
| `seqId` | STREAM packet sequence id。 |
| `cursor` | STREAM 位置或时间游标，单位由 stream context 定义。 |

## 注册表术语

| 术语 | 含义 |
|---|---|
| Domain | 稳定的业务或协议分类，例如 `audio`、`video`、`device`、`system`、`network` 或 `capability`。 |
| Feature | domain 内可评审的能力块，例如 `audio.algorithm` 或 `network.wifi`。 |
| Method | 拥有稳定名称和可选 binary methodId 的 RPC 业务操作。 |
| Event | 表示状态、进度、结果或报告语义的 RPC 异步通知。 |
| ErrorCode | RPC status、CONTROL status 和 STREAM/profile error mapping 共用的数值错误码。 |
| Schema | method params/result、event payload、capability 或 profile data 的结构化对象定义。 |
| Capability | 设备声明的某个 domain.feature 的可用性或限制。 |
| Profile | 对 transport、method、event、error、type 和 capability 的命名实现要求集合。 |
| Protocol IR | 生成的机器可读模型，位于 `contract/protocol/axtp.protocol.yaml`。 |
| Generated Reference | `contract/generated/**` 下生成的人读和机器读参考。 |

## 角色

| 术语 | 含义 |
|---|---|
| Physical Client | 主动建立底层 transport connection 的一侧。 |
| Physical Server | 接受底层 transport connection 的一侧。 |
| Logical Client | 向 Logical Server identify，并主要消费其 method/event/stream 的一侧。 |
| Logical Server | 发送 Hello、分配 `sid`，并暴露 method/event/stream 的一侧。 |
| Relay Endpoint | 能根据 `m.dst` 把对象编码 RPC 转交给本地或下级 Endpoint Provider 的 Endpoint。Relay 不向 wire 暴露 next hop、hop list 或完整 Agent path。 |

物理方向和逻辑方向可以不同。例如设备主动连接云端时可以是 Physical Client，但仍然作为 Logical Server 暴露能力。

## 编码

| 术语 | 含义 |
|---|---|
| JSON | `{ sid, op, m?, d }` 的默认 RPC 对象编码；`m` optional，缺失时保持既有 `{ sid, op, d }` 行为。 |
| CBOR | 同一 RPC 语义的可选紧凑对象编码。 |
| MSGPACK | 同一 RPC 语义的可选紧凑对象编码。 |
| JSON_BINARY | AXTP RPC 固定二进制 envelope 编码；不是通用 binary JSON 格式。当前 15B fixed header 不承载对象编码 `m`。 |
| TLV8 | `fieldId:uint8 + length:uint8 + value` body 编码。 |
| TLV16 | 更长的 TLV body 编码，除非 profile 要求，否则属于 optional/future。 |
