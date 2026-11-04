# AXTP 核心协议

本文是 AXTP 传输路径、Standard Framed Binary、CONTROL、RPC、STREAM 和低带宽边界的规范性 runtime 合同。

## 核心模型

AXTP 用两条生产路径暴露同一套业务注册表：

| 路径 | Transport 示例 | Wire 形态 | 必需能力 |
|---|---|---|---|
| Standard Framed | `AXTP-TCP`、`AXTP-USB-HID` | `Standard Frame Header(12B) + Payload(N) + CRC16(2B)` | CONTROL / RPC / STREAM |
| WebSocket Unframed JSON | `AXTP-WS-JSON`、`AXTP-WS-CLOUD-REVERSE` | WebSocket message payload 是 JSON `{ sid, op, m?, d }` | 仅 RPC |

PayloadType 只选择解析器：

| PayloadType | 值 | 解析器 |
|---|---:|---|
| CONTROL | `0x01` | 链路运行时控制。 |
| RPC | `0x02` | Session、request/response 和 event 控制面。 |
| STREAM | `0x03` | 连续数据面。 |

PayloadType MUST NOT 编码 video、audio、firmware、file、domain.feature、method、event 或 capability 等业务类型。

## Standard Frame 标准帧

Standard Framed Binary runtime MUST 实现：

```text
Standard Frame Header(12B) + Payload(N) + CRC16(2B)
```

Header 布局：

| 字段 | 偏移 | 大小 | 规则 |
|---|---:|---:|---|
| `Magic[0]` | 0 | 1B | MUST 为 `0x41` (`A`)。 |
| `Magic[1]` | 1 | 1B | MUST 为 `0x58` (`X`)。 |
| `Version` | 2 | 1B | 当前值为 `0x01`。 |
| `PayloadType` | 3 | 1B | CONTROL=`0x01`，RPC=`0x02`，STREAM=`0x03`。 |
| `PayloadLength` | 4 | 2B | 仅 payload 字节数，不包含 header 和 CRC。 |
| `SourceId` | 6 | 1B | 当前 framed link 内的发送方 logical node；不是全局 Endpoint ID。 |
| `DestinationId` | 7 | 1B | 当前 framed link 内的接收方 logical node；不是全局 Endpoint ID。 |
| `MessageId` | 8 | 2B | 用于分片和调试的 frame/message 关联。 |
| `FrameIndex` | 10 | 1B | 分片序号，从 0 开始。 |
| `FrameCount` | 11 | 1B | 分片总数；未分片 message 使用 1。 |
| `CRC16` | Footer | 2B | 对 header + payload 计算 CRC16-CCITT-FALSE。 |

所有 AXTP 多字节 wire integer MUST 使用 Big-Endian / network byte order。

Frame parser MUST 在分发前校验 magic、version、PayloadType、`PayloadLength + 14 <= maxFrameSize`、`FrameCount >= 1`、`FrameIndex < FrameCount`、CRC 和完整 payload 可用性。

分片属于 Frame layer。Request/Response 匹配使用 RPC request id，不使用 `MessageId`。STREAM 排序使用 `seqId`，不使用 `MessageId`。

Frame `SourceId` / `DestinationId` 只描述当前 Standard Framed link 的逐跳 node address。对象编码 RPC 的 `m.src` / `m.dst` 描述跨 relay 的逻辑 Endpoint address。Relay 转发 RPC 时 MAY 改变 Frame `SourceId` / `DestinationId`，但 MUST 按本文 RPC 规则处理端到端 `m.src` / `m.dst`。

## Transport Profile 传输配置

Transport profile 固定外层 envelope。AXTP 不在运行时协商 Standard Frame 形态。

| Profile | Envelope | 说明 |
|---|---|---|
| `AXTP-TCP` | TCP 字节流上的 Standard Framed。 | 接收方 SHOULD 扫描 magic、解析 12B header、读取 payload 和 CRC，然后分发。 |
| `AXTP-USB-HID` | report/packet 上的 Standard Framed。 | Packet 边界不能替代 header/CRC 校验。 |
| `AXTP-WS-JSON` | WebSocket Unframed JSON。 | 仅 RPC；没有 CONTROL、STREAM、CRC、Standard Frame Header 或 JSON_BINARY fixed header。 |
| `AXTP-WS-CLOUD-REVERSE` | WebSocket Unframed JSON。 | 同样是仅 RPC 的 wire shape；物理连接方向可以不同于逻辑角色。 |

Standard Framed 启动顺序：

```text
Transport connected
CONTROL OPEN
CONTROL ACCEPT
RPC Hello
RPC Identify(randomSeed)
RPC Identified
APP_READY
```

WebSocket JSON 启动顺序：

```text
WebSocket connected
RPC Hello
RPC Identify(randomSeed)
RPC Identified
APP_READY
```

Runtime gate 状态：

| 状态 | 允许的流量 | 拒绝的流量 |
|---|---|---|
| `LINK_CONNECTED` | Standard Framed：只允许 CONTROL OPEN。WebSocket JSON：可以开始 RPC Hello。 | 业务 RPC 和 STREAM。 |
| `FRAMING_READY` | RPC Hello / Identify / Identified。 | 业务 Request、Event 和 STREAM。 |
| `APP_READY` | Request / RequestResponse / Event；如果 profile 和业务 Stream Context 允许，也可以 STREAM。 | 未知 method、未授权 method、无效 sid、未知 streamId。 |
| `CLOSING` | CLOSE_ACK 和本地清理。 | 新业务 RPC 或新 STREAM context。 |

Request-before-identified MUST 按 profile policy 拒绝或关闭 session；它 MUST NOT 被当作业务流量处理。

## CONTROL

CONTROL 只存在于 Standard Framed profile。WebSocket Unframed JSON MUST NOT 发送或要求 CONTROL。

CONTROL payload 为：

```text
opcode:uint8 + controlId:uint16 + statusCode:uint16 + TLV body
```

`controlId` 和 `statusCode` 使用 Big-Endian / network byte order。`statusCode=0x0000` 表示 SUCCESS；非零值使用 ErrorCode registry。

必需 CONTROL opcode：

| Opcode | 名称 | 必需行为 |
|---:|---|---|
| `0x01` | `OPEN` | Physical Client 发起 framed link 协商。 |
| `0x02` | `ACCEPT` | Physical Server 接受或拒绝 OPEN。 |
| `0x04` | `HEARTBEAT` | 保活。 |
| `0x05` | `HEARTBEAT_ACK` | 保活响应。 |
| `0x0A` | `CLOSE` | 优雅关闭请求。 |
| `0x0B` | `CLOSE_ACK` | 优雅关闭响应。 |

不存在 `REJECT` opcode。被拒绝的 OPEN 是一个带非零 `statusCode` 的 `ACCEPT`。

对 OPEN、HEARTBEAT 和 CLOSE 的 response MUST 回显 request `controlId`。如果接收方无法解析 CONTROL payload length、TLV length、opcode 或必需协商字段，在还能 framing 出合法 response 时，SHOULD 返回最接近的 CONTROL error；否则 MAY 关闭 transport。

必需 OPEN / ACCEPT TLV：

| TLV | 名称 | 规则 |
|---:|---|---|
| `0x04` | `maxFrameSize` | 必需；总 frame size，包含 12B header 和 2B CRC。 |
| `0x07` | `supportedPayloadTypes` | 必需 bitmap。 |
| `0x08` | `supportedRpcEncodings` | OPEN 中必需。 |
| `0x0A` | `heartbeatIntervalMs` | 必需。 |
| `0x0B` | `ackMode` | 必需；Phase 1 默认值为 `NONE`。 |
| `0x1E` | `selectedRpcEncoding` | 成功 ACCEPT 中必需。 |

`sessionId(0x01)` MAY 为 trace 或未来 resume 而解析，但 MUST NOT 路由 RPC 或 STREAM 业务状态。

新实现 SHOULD 省略 deprecated CONTROL `protocolVersion(0x02)`，且 MUST NOT 因为一个有效 v1 handshake 缺少它而拒绝握手。`maxPayloadSize(0x05)` 已 deprecated/reserved；使用 `maxFrameSize`。

READY is optional / 可选。默认握手只要求 OPEN / ACCEPT。Runtime MUST NOT 把 READY 当作第三个默认握手步骤。

Phase 1 不要求 runtime 实现 ACK/NACK。ACK、NACK、RESUME、SESSION_RESET、WINDOW_UPDATE、PING、PONG、GOAWAY 和 VENDOR 属于 future/profile-specific 行为，除非已发布 profile 明确要求。

最小 CONTROL 示例，按 payload level 展示：

```text
OPEN:
01 00 01 00 00
04 02 10 00        # maxFrameSize = 4096
07 01 07           # CONTROL + RPC + STREAM
08 01 09           # JSON + JSON_BINARY supported
0a 02 03 e8        # heartbeatIntervalMs = 1000
0b 01 00           # ackMode = NONE

ACCEPT:
02 00 01 00 00
04 02 10 00        # maxFrameSize = 4096
07 01 07           # CONTROL + RPC + STREAM
1e 01 01           # selectedRpcEncoding = JSON
0a 02 03 e8        # heartbeatIntervalMs = 1000
0b 01 00           # ackMode = NONE
```

Standard Frame Header、payload length 和 CRC 会包裹这个 CONTROL payload。启动流程摘要见 `docs/guides/core-protocol-flow.md`。

## RPC

RPC 是业务控制面。它运行在 Standard Framed `PayloadType=RPC` 中，或直接作为 WebSocket Unframed JSON。

JSON/CBOR/MSGPACK RPC envelope：

```json
{ "sid": "12345678", "op": 7, "m": { "src": "ep-app-001", "dst": "ep-camera-001" }, "d": {} }
```

`m` 是 optional。既有消息仍可保持：

```json
{ "sid": "12345678", "op": 7, "d": {} }
```

| 字段 | 规则 |
|---|---|
| `sid` | 分配前为空字符串；Identified 后携带 Logical Server 分配的 session 字符串。AXTP-native 生成端使用 8 位 hex；接收端按 opaque string 兼容。 |
| `op` | uint8 操作码。 |
| `m` | Optional message metadata object。v1 首批标准字段为 `src`、`dst`；未知且 structurally valid 的 optional metadata field 按 codec forward-compatibility 规则处理。 |
| `d` | op-specific object；允许 empty object。 |

### Endpoint identity

Endpoint 是可以被 AXTP 逻辑寻址并独立接收或产生 RPC 的实体。Endpoint 可以是设备、Agent、应用实例、Cloud service、Room、Software service 或其他逻辑资源。

`endpointId` 是 Endpoint 的稳定 opaque string identity。它：

- MUST 是非空 string，并 SHOULD 不超过 128 UTF-8 bytes；
- MUST 在所属寻址/管理域中唯一；
- MUST 对同一个逻辑 Endpoint 稳定：transport 重连、RPC `sid` 变化和进程/设备重启不得导致新的 `endpointId`；
- MUST NOT 从当前 `sid`、request id、IP address、TCP/WebSocket connection id、USB path、BLE connection handle 或当前 parent Agent 直接推导；
- MUST NOT 被接收方按前缀解析来决定 Endpoint type；类型和拓扑属于 discovery/registry/business data。

AXTP-native Endpoint 使用一个稳定的 `endpointKey` 确定性生成 `endpointId`。规则固定为：

```text
canonicalInput = UTF-8("AXTP-ENDPOINT-v1|" + endpointKey)
digest         = SHA-256(canonicalInput)
endpointId     = "ep_" + lowerHex(digest[0..15])
```

也就是取 SHA-256 的前 16 bytes，以 32 个小写 hex 字符表示。AXTP-native sender 生成的 canonical `endpointId` 形态为：

```text
ep_<32 lowercase hex chars>
```

确定性规则：

1. 相同 `endpointKey` MUST 每次生成完全相同的 `endpointId`；
2. 不同逻辑 Endpoint MUST NOT 共用同一个 `endpointKey`；
3. 一个 Endpoint 一旦选择 `endpointKey`，在该 Endpoint 生命周期内 MUST NOT 因重启、重连、换 IP、换 USB port 或更换 parent Agent 而改变；
4. receiver MUST 把 `endpointId` 当作 opaque string。为了兼容 legacy/external sender，receiver MUST NOT 要求收到的 `endpointId` 一定符合 `ep_<32hex>` canonical sender form，只要该值是合法的非空 Endpoint ID。

`endpointKey` 本身不在 wire 上发送，只作为本地稳定身份输入。推荐来源保持简单：

| Endpoint | `endpointKey` 建议来源 |
|---|---|
| App | `app:<productNamespace>:<persistentInstallationId>`；`persistentInstallationId` 首次安装生成一次并持久化。 |
| Agent | `agent:<productNamespace>:<persistentInstallationId>`；首次安装生成一次并持久化。 |
| Cloud / Service / Room / Software | `service:<namespace>:<stableResourceId>`；使用管理系统已经稳定存在的资源 ID。 |
| AXTP-native Device | 优先使用设备自身持久 `deviceUuid` 或稳定 public-key identity。 |
| Legacy / Child Device | 使用协议能够稳定取得的设备 identity，例如 `deviceUuid`，其次 stable public key，再其次 `vendor + product + serialNumber` 的 canonical identity。 |

对于软件 Endpoint，首次创建一个随机 UUID 作为 `persistentInstallationId` 是允许的；关键不是每次重新生成 UUID，而是**生成一次、持久化、以后始终用同一个值作为 `endpointKey` 的组成部分**。删除该持久 identity 并重新安装/重新 provision 表示创建了新的逻辑 Endpoint，因此 MAY 得到新的 `endpointId`。

对于物理 Device，`endpointKey` MUST 来自设备本身的稳定 identity，而不是当前 Agent。这样同一个 Device 从 Agent A 移到 Agent B，只要两边取得相同的稳定 device identity，就会确定性得到相同 `endpointId`。产品/profile SHOULD 固定 identity evidence 的选择优先级，避免同一设备因为后续出现新的 identity field 而切换 `endpointKey`。

无法取得或持久化稳定 `endpointKey` 的对象不能保证稳定 Endpoint identity。此类对象 SHOULD 只作为 provider-local `childId` / 临时资源存在，直到完成稳定 identity 绑定后再向上游投影为可寻址 Endpoint。Identity fingerprint MAY 用于构造或查找 `endpointKey`，但 fingerprint 本身不是 wire `endpointId`。

### RPC message metadata

v1 对 `m` 正式定义两个字段：

| 字段 | 类型 | 语义 |
|---|---|---|
| `m.src` | string | 原始逻辑来源 Endpoint ID。Relay 转发时保持原来源；它不是当前 socket/frame sender。 |
| `m.dst` | string | 当前逻辑消息唯一的最终目标 Endpoint ID。它 MUST 是单个 string，MUST NOT 是 array。 |

`m.src` / `m.dst` 是对象编码 RPC 的逻辑地址，不改变 `sid` 的 session scope，也不改变 Standard Frame `SourceId` / `DestinationId` 的逐跳含义。

第一跳发送方 MAY 省略 `m.src`；接入 relay/server MAY 根据已认证 RPC Session 确定来源并在可信转发时补充 `m.src`。接收方 MUST NOT 把未经认证 peer 自报的 `m.src` 单独作为授权依据。

Request 中 `m.dst` 缺失时，目标默认为当前 RPC Session 的 Logical Server，行为与既有 `{ sid, op, d }` RPC 完全一致。`m.dst` 存在时：

1. 若 `m.dst` 标识当前 Endpoint，则本地处理；
2. 若当前 Endpoint 是 relay 且其 Endpoint Provider Table 能解析 `m.dst`，则 relay MAY 将 RPC 转交给本地 adapter、child session 或下级 Agent；
3. relay MUST NOT 要求调用方提供 `nextHop`、完整 Agent path、`routeId`、`ttl` 或 `hops`；
4. 已知但当前不可达的目标 SHOULD 使用 `UNAVAILABLE`；未知目标 SHOULD 使用 `NOT_FOUND`，除非业务 method 已注册更具体错误；
5. relay 的内部 provider/parent-child topology 是本地实现事实，不是 RPC envelope contract。

这种行为称为 Endpoint Relay。多级 Agent SHOULD 使用 Endpoint Projection：每一层向上游暴露最终可寻址 Endpoint，并在本地维护 `endpointId -> provider` 映射，使上游无需了解真实 Agent 层级。

RequestResponse 逻辑上返回给 Request 的来源。Relay 若在转发 Request 时保留/补充了 `m.src` 和 `m.dst`，则向上游转发对应 RequestResponse 时 SHOULD 使用：

```text
response.m.src = request.m.dst
response.m.dst = request.m.src
```

如果某一侧没有使用 metadata，Response MAY 继续省略 `m`，并依赖当前 `sid` 和 request id 完成现有逐 session response matching。

Event 的 `m.src` SHOULD 标识真正产生事件的 Endpoint，而不是中间 Agent/Cloud relay。Event 的 `m.dst` MAY 缺失：缺失表示接收范围由当前 session subscription / event relay policy 决定。Relay MAY 将一个逻辑 Event fanout 到多个 outbound sessions；每个 wire message 仍是单目标或无显式目标的 Event，Relay MUST NOT 把 `m.dst` 改为数组，并 MUST NOT 因 fanout 改写原始 `m.src`。需要显式定向单个消费者时，Event MAY 携带单个 `m.dst`。

多目标 Request 不由 `m.dst` 表达。需要对多个 Endpoint 执行相同操作时，调用方 SHOULD 发送多个独立 Request；如果多个资源构成一个可独立寻址的逻辑整体，可以把该整体建模为一个 Endpoint；需要 batch/atomic/partial-result 语义时，业务 domain SHOULD 定义 batch method，并在 `d.params` 中携带 targets。这样每个 AXTP Request 仍只有一个负责执行的 `m.dst` 和一个 RequestResponse 生命周期。

AXTP v1 不定义 `route`、`routeId`、`nextHop`、`ttl`、`hops`、`trace` 或 `deadline` metadata。后续对象编码可以通过 optional `m` fields 独立扩展，但不得改变本节单目标 `m.dst` 的基础语义。

AXTP-native `sid` 生成使用非零 `uint32`，在 JSON / CBOR / MSGPACK envelope 中渲染为固定 8 位十六进制字符串，例如 `"00000003"` 或 `"12345678"`。对象编码接收端 MUST NOT 要求收到的 `sid` 一定是 8 位 hex；Identified 后应把 Logical Server 分配的非空字符串按 opaque value 保存，并在后续 Request / Response / Event 中精确携带。JSON_BINARY fixed header 中仍使用 4B Big-Endian / network byte order `uint32`，未分配前为 `0`。

必需 RPC op：

| op | 名称 | 必需行为 |
|---:|---|---|
| `0` | `Hello` | Logical Server 公告可选 AXTP version diagnostics 和 auth requirements。 |
| `2` | `Identify` | Logical Client 发送 identity、`randomSeed`、auth 和 event subscription intent。 |
| `3` | `Identified` | Logical Server 分配 `sid`；session 进入 app-ready。 |
| `6` | `Event` | 低频 event 投递。 |
| `7` | `Request` | Identified 后的业务 method request。 |
| `8` | `RequestResponse` | 业务 result 或 error。 |

RequestResponse 的 `d.status` 在 JSON / CBOR / MSGPACK envelope 中 MUST 是 object，且 MUST 携带 `status.ok` 和 `status.code`。成功响应 MUST 使用 `status.ok=true`、`status.code=0`；失败响应 MUST 使用 `status.ok=false` 和非零 `status.code`，且 MUST NOT 携带业务 `result`。JSON_BINARY fixed header 中的 `statusCode:uint16` 与 `status.code` 语义一致，但 JSON envelope MUST NOT 使用数字形式的 `status` 简写。

最小 RPC JSON 序列：

```json
{ "sid": "", "op": 0, "d": { "axtpVersion": "1.0.0" } }
{ "sid": "", "op": 2, "d": { "randomSeed": 305419896, "eventMasks": "090101" } }
{ "sid": "12345678", "op": 3, "d": { "accepted": true } }
{ "sid": "12345678", "op": 7, "d": { "id": 1, "method": "audio.getAlgorithmConfig", "params": {} } }
{ "sid": "12345678", "op": 7, "m": { "src": "ep-app-001", "dst": "ep-audio-001" }, "d": { "id": 2, "method": "audio.getAlgorithmConfig", "params": {} } }
{ "sid": "12345678", "op": 8, "m": { "src": "ep-audio-001", "dst": "ep-app-001" }, "d": { "id": 2, "status": { "ok": true, "code": 0 }, "result": {} } }
{ "sid": "12345678", "op": 6, "m": { "src": "ep-audio-001" }, "d": { "event": "audio.algorithmConfigChanged", "data": { "reason": "user_request", "applyState": "applied" } } }
```

Event 的业务 payload 不重复携带 `sid`。发送方 MUST 在 RPC envelope 或 JSON_BINARY fixed header 中携带当前 RPC session 的 `sid`；接收方按 envelope/header 校验、寻址和鉴权后，再把 `d.data` 作为 event payload 交给业务处理。

在 Standard Framed JSON RPC 中，RPC payload 是 `rpcEncoding(1B) + JSON bytes`；当 `selectedRpcEncoding=JSON` 时，`rpcEncoding=0x01`。在 WebSocket Unframed JSON 中，WebSocket message payload 正好就是 JSON object。

Hello 中的 `axtpVersion` 是 optional advisory diagnostics metadata。它不协商 feature，也不是 AXTP compatibility authority。无论该字段缺失、不是合法 SemVer，还是 major、minor 或 patch 不同，接收方 MUST NOT 因此拒绝或延迟 `Hello -> Identify -> Identified`、改变 retry/reconnect 策略，或影响不相关 RPC 的可用性。实现 MAY 把观测值分类并记录到日志、telemetry 或 diagnostics，但分类结果 MUST NOT 成为 session admission gate。

`protocolVersion`、`rpcVersion` 和 `negotiatedRpcVersion` 是 deprecated compatibility inputs；新发送方 SHOULD 省略它们。接收方 MAY 为兼容性读取它们，但 MUST NOT 把它们变成 session admission gate。

RPC `Hello.axtpVersion` 与 Standard Frame Header 的 `Version` 是不同层次的字段。Frame Header `Version` 是 hard wire parsing boundary；如果接收方无法安全解析该 frame layout，MUST 使用 `FRAME_VERSION_UNSUPPORTED` 拒绝该 frame。这个 frame parsing rule 不得被推导为对 RPC `axtpVersion` 的拒绝规则。

Identify MUST 包含 `randomSeed:uint32`。Logical Server MUST 在生成 `sid` 时把 `randomSeed` 与本地状态混合；它 MUST NOT 直接把 `randomSeed` 当作 `sid`。`randomSeed` 不是认证 secret。

生成新 `sid` 时，AXTP-native Logical Server MUST 避免 `0` 和当前仍有效 RPC Session 的 `sid` 冲突，并 SHOULD 使用大写 8 位 hex 作为 JSON canonical sender form。对象编码 receiver MUST 接受非空字符串形式的已分配 `sid`，包括非 8 位 hex 的 legacy / external 值；它 MUST 按精确字符串匹配 session，不得把 `sid` 当作认证 secret 或用户 token。

Identified 之后，如果 method 的 domain.feature、capability 或 role policy 允许，双方 MAY 发起 RPC Request。这不改变 Hello / Identify / Identified 的逻辑角色，也不改变 CONTROL 的物理角色。

APP_READY 后，JSON / CBOR / MSGPACK envelope 中缺失、非字符串、空字符串或未知 session 的 `sid` MUST 被拒绝；JSON_BINARY 中 zero 或未知 session 的 `sid` MUST 被拒绝。

Standard Framed RPC MUST 在 payload 前添加 `rpcEncoding`。JSON (`0x01`) 是 Phase 1 互操作必需编码。高吞吐或嵌入式 Standard Framed profile SHOULD 实现 JSON_BINARY (`0x04`)。

JSON_BINARY fixed header 为 15B：

```text
rpcEncoding(1) + rpcOp(1) + sid(4) + requestId(4)
  + methodOrEventId(2) + statusCode(2) + bodyEncoding(1)
  + body(N)
```

JSON_BINARY 多字节字段使用 Big-Endian / network byte order。Event 使用 requestId `0`。`bodyEncoding` 的值为 NONE=`0x00`、TLV8=`0x01`、TLV16=`0x02`。

JSON_BINARY 15B fixed header 在本次 Endpoint Relay 扩展中保持不变，不承载 `m.src` / `m.dst`。需要 Endpoint Relay 的 peer MUST 使用支持 `m` 的对象编码 RPC profile；不得在现有 JSON_BINARY v1 header 中插入可变 metadata。

Request/Response matching MUST 使用 RPC request id。Unknown method MUST 返回 RPC error，例如 `RPC_METHOD_NOT_FOUND`；CONTROL MUST NOT 处理业务 method error。

`eventMasks` 编码 domain-scoped event subscription。每个 entry 为 `domainId:uint8 + maskLen:uint8 + bitmask(maskLen)`；bit 0 映射到该 domain 中 registry `bitOffset=0` 的 event。空 mask 或缺失 mask 表示不订阅 event，除非 profile 另有规定。

## STREAM

STREAM 只存在于 Standard Framed profile。WebSocket Unframed JSON 是仅 RPC profile，MUST NOT 承载 STREAM。

STREAM Payload 为：

```text
STREAM Header(16B) + data(N)
```

STREAM Header 布局：

| 字段 | 类型 | 大小 | 规则 |
|---|---|---:|---|
| `streamId` | uint32 | 4B | 非零 stream context id。 |
| `seqId` | uint32 | 4B | stream 内的 packet sequence id。 |
| `cursor` | uint64 | 8B | 由 Stream Context 解释的位置或时间游标。 |

16B STREAM Header 字段 `streamId:uint32`、`seqId:uint32` 和 `cursor:uint64` 使用 Big-Endian / network byte order。`Frame.payloadLength` MUST 等于 `16 + dataLength`。

STREAM Header MUST NOT 携带 codec、file type、firmware metadata、offset field、timestamp field、flag、domain、event 或 capability。业务含义来自已采纳 RPC method 或 profile 创建的 Stream Context。

StreamParser MUST 校验 `payloadLength >= 16`、`streamId != 0`、已知 Stream Context、data size limit 和 profile-specific `seqId` 行为。它 MUST 把 `data` 视为 opaque bytes，并分发给 profile handler。

CONTROL CLOSE、transport loss 或 session teardown MUST 释放该 session 的 Stream Context。

可靠重传、resume、window update、chunk-level CRC 和 object-level verification 是 future/profile-specific 行为，且 MUST 保持 16B STREAM Header 不变。

## 低带宽边界

Compact/HID-64/BLE/UART framing 是低带宽降级路径，不是 AXTP v1 Core 要求。

低带宽 profile MUST 保留：

- PayloadType 值：CONTROL=`0x01`，RPC=`0x02`，STREAM=`0x03`。
- CONTROL 5B payload header。
- 使用 JSON_BINARY 时的 JSON_BINARY 15B fixed header。
- STREAM 16B Header。
- MethodId、EventId、ErrorCode、schema、capability 和 profile registry 语义。

低带宽 profile MUST NOT 为普通 Standard Framed runtime 重新定义 Standard Frame、PayloadType、RPC envelope 或 STREAM Header 语义。
