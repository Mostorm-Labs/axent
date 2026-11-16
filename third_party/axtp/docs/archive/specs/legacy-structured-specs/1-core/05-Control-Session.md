# 1-core/05《AXTP CONTROL 会话规范》

> 状态： 规范性 runtime 实现规范
> 规范版本： 1.0.0-rc1
> 变更策略： v1.0.0 前允许冻结候选收敛；影响 wire/必填字段语义的修改需遵循版本治理
> 本文的规范范围：`PayloadType=CONTROL` payload 结构、CONTROL opcode registry、TLV 字段、framed link 启动、心跳、关闭和 CONTROL 校验。
> 本文不定义：业务 capability discovery、RPC method 调用、事件订阅、STREAM 数据 payload、methodId/eventId registry 或 WebSocket JSON 启动流程。
> Runtime 实现状态：Standard Framed profile 必需实现；ACK/NACK、RESUME、SESSION_RESET、WINDOW_UPDATE、PING/PONG、GOAWAY 和 VENDOR 属于可选/未来能力。

版本：v1.0.0-rc1
状态：AXTP v1 Core 冻结候选
适用范围：`PayloadType = CONTROL` 的 Payload 结构、Opcode、TLV 字段、会话建连、心跳、关闭，以及预留的 ACK/NACK、恢复、流控
前置文档：[`02-Protocol-Framework.md`](02-Protocol-Framework.md)、[`03-Frame-and-Payload.md`](03-Frame-and-Payload.md)、[`04-Transport-Profiles.md`](04-Transport-Profiles.md)
后续文档：[`06-RPC-Session.md`](06-RPC-Session.md)、[`07-Stream-Data-Plane.md`](07-Stream-Data-Plane.md)

---

## 文档目的

本文定义 Standard Framed profiles 中的 CONTROL payload。CONTROL 只建立和管理 AXTP Framed Link Context，包括 OPEN / ACCEPT、心跳和关闭。CONTROL 不承载业务 capability，也不承载业务 method、event 或连续数据。

CONTROL 在线形态：

```text
Standard Frame Header(payloadType=CONTROL)
  + Control Payload = opcode(1) + controlId(2) + statusCode(2) + TLV body(N)
  + CRC16(2)
```

## 范围

本文覆盖：

- CONTROL 5B 固定头；
- Control opcode registry；
- Control statusCode；
- TLV 编码和字段表；
- OPEN / ACCEPT / reject outcome / HEARTBEAT / CLOSE 生命周期；
- optional/future ACK/NACK、RESUME、SESSION_RESET、WINDOW_UPDATE、PING/PONG、GOAWAY、VENDOR 边界；
- runtime parser 和安全验证要求。

本文不覆盖：

- WebSocket Unframed JSON；
- RPC Hello / Identify / Identified 的字段行为；
- capability.getAll 或任何业务能力发现 method；
- STREAM payload 数据；
- low-bandwidth Compact outer frame。

## v1 必需实现

### CONTROL Payload Header

所有 CONTROL payload MUST 使用以下 5B 固定 header：

| 字段 | 偏移 | 长度 | 类型 | 规则 |
|---|---:|---:|---|---|
| `opcode` | 0 | 1B | uint8 | CONTROL 操作 |
| `controlId` | 1 | 2B | uint16 | Request/response 关联 id，Big-Endian / network byte order |
| `statusCode` | 3 | 2B | uint16 | `0x0000 = SUCCESS`；非零值使用 ErrorCode Registry；Big-Endian / network byte order |
| `body` | 5 | N | bytes | TLV body；长度为 `Frame.payloadLength - 5` |

如果 body 为空，`Frame.payloadLength` MUST 等于 5。

### 必需 Opcode 集合

Runtime MUST 实现：

| opcode | 名称 | 方向 | 必需行为 |
|---:|---|---|---|
| `0x01` | `OPEN` | Physical Client -> Physical Server | 启动 framed link 协商 |
| `0x02` | `ACCEPT` | Physical Server -> Physical Client | 返回协商结果；非零 `statusCode` 表示拒绝 |
| `0x04` | `HEARTBEAT` | 双向 | link keepalive |
| `0x05` | `HEARTBEAT_ACK` | 双向 | heartbeat 响应 |
| `0x0A` | `CLOSE` | 双向 | 请求优雅关闭 control/session |
| `0x0B` | `CLOSE_ACK` | 双向 | close 确认 |

v1 没有独立的 `REJECT` opcode。被拒绝的 OPEN MUST 表达为带有 `statusCode != SUCCESS` 的 `ACCEPT`，通常使用 `CONTROL_OPEN_REJECTED` 或 `CONTROL_NEGOTIATION_FAILED`。

### 必需 TLV 字段

Runtime MUST 解析并校验：

| TLV type | 名称 | 类型 | 必需用途 |
|---:|---|---|---|
| `0x04` | `maxFrameSize` | uint16/uint32 | OPEN / ACCEPT |
| `0x07` | `supportedPayloadTypes` | bitmap | OPEN / ACCEPT |
| `0x08` | `supportedRpcEncodings` | bitmap | OPEN |
| `0x0A` | `heartbeatIntervalMs` | uint16/uint32 | OPEN / ACCEPT |
| `0x0B` | `ackMode` | uint8 | OPEN / ACCEPT；Phase 1 默认 `NONE` |
| `0x10` | `reasonCode` | uint16 | CLOSE / CLOSE_ACK 可选 body |
| `0x1E` | `selectedRpcEncoding` | uint8 | ACCEPT |

`sessionId(0x01)` MAY 被解析并保存用于 tracing/future resume，但 MUST NOT 用作 RPC/STREAM 业务 session routing。

### CONTROL 版本字段职责

AXTP v1 不要求在 CONTROL OPEN / ACCEPT 中使用独立 `protocolVersion` 协商。版本边界按层分工：

| 字段 | 所在层 | 实际作用 | CONTROL 结论 |
|---|---|---|---|
| Frame Header `Version` | Frame | 保护 12B Standard Frame header 布局、CRC 覆盖范围和 PayloadType 编码 | 必需；不支持时拒绝或关闭 |
| RPC Hello `axtpVersion` | RPC Session | 表达 AXTP spec/release 快照兼容性 | 作为 AXTP v1 主兼容判断 |
| CONTROL `protocolVersion` | CONTROL TLV | 早期草案中的 link/control 版本字段 | Deprecated/Transition；新实现 SHOULD 省略 |

规则：

- 新实现 SHOULD 省略 CONTROL `protocolVersion(0x02)`。
- Runtime MUST NOT 因为 OPEN/ACCEPT 缺少 `protocolVersion` 而拒绝 otherwise valid 的 v1 CONTROL 握手。
- 为兼容早期 rc1 草案，接收端 MAY 接受 `protocolVersion=1`。
- 接收端在未声明未来扩展规范时 MUST 拒绝非 `1` 的 `protocolVersion`，避免形成与 Frame Header `Version` 和 RPC `axtpVersion` 并行的第三套兼容状态。
- 对 CONTROL header、TLV 编码、opcode 含义或必填字段语义的不兼容变更，MUST 走版本治理，并优先通过新的 Frame Header `Version`、新的 profile 或新 TLV/opcode 表达。

### CONTROL 尺寸字段职责

AXTP v1 的 CONTROL 握手只需要一个必填尺寸字段：`maxFrameSize`。它描述接收端愿意接受的单个 Standard Frame 线上总字节数上限，包含 12B Frame Header、Payload 和 2B CRC。`Frame.payloadLength` 的有效上限可由 `maxFrameSize - 14` 推导。

| 字段 | v1 状态 | 实际作用 | 结论 |
|---|---|---|---|
| `maxFrameSize` | Required | 限制单个 Standard Frame 总字节数和接收端内存分配；用于 Frame parser 校验 `PayloadLength + 14 <= maxFrameSize` | 保留 |
| `maxPayloadSize` | Deprecated/Reserved | 与 `maxFrameSize - 14` 重复，且不同 payload 还有 CONTROL/RPC/STREAM 自己的内部 header | 新实现 MUST NOT 发送；接收端 MAY 忽略 |
| `mtu` | Profile-specific Optional | 表达 transport packet/report 的建议承载单元，例如小 MTU profile 的分片策略输入 | 不作为 v1 Core 必填；需要时由 transport/profile 约束 |

规则：

- `maxFrameSize` MUST 大于等于 `19`，以容纳最小 CONTROL payload：12B Frame Header + 5B CONTROL header + 2B CRC。
- `maxFrameSize` MUST NOT 超过本地接收缓冲、重组缓冲或安全策略允许值。
- `maxPayloadSize` MUST NOT 作为独立协商结果参与 v1 Core 校验；若收到该 TLV，接收端 MAY 校验 length 后忽略，或在 strict mode 下拒绝。
- `mtu` 只描述 transport 层单次 packet/report/write 的建议大小；Frame 层 MUST NOT 把 `mtu` 当作业务 payload 大小。
- 对 TCP 等 byte-stream transport，runtime SHOULD 省略 `mtu`，并只使用 `maxFrameSize` 作为 AXTP frame 上限。
- 对 USB HID、BLE、UART 或其他 packet-bound profile，profile MAY 规定 `mtu`，并 MAY 要求发送端使单个 Standard Frame 或低带宽 outer frame 不超过该值。

## v1 可选 / Profile 特定

v1 中可选或 profile 特定的内容：

默认握手只要求 OPEN / ACCEPT。`READY` 是可选/保留的第三步标记，不得作为 v1 默认握手的必需条件。

| 项目 | 状态 | 规则 |
|---|---|---|
| `READY(0x03)` | 可选/保留 | MAY 忽略；MUST NOT 要求作为 handshake 第三步 |
| `ACK(0x06)` / `NACK(0x07)` | 可选/未来 | 为可靠 frame/message/stream/control 确认保留 |
| `sessionId(0x01)` TLV | 可选 | 仅用于 trace/future resume |
| `protocolVersion(0x02)` | Deprecated/Transition | 早期 rc1 兼容接收；新实现 SHOULD 省略 |
| Control payload fragmentation | 可选 | SHOULD 避免；parser 可拒绝过大的 control payload |
| Extended TLV length `0xFF` | parser 需能识别 | Runtime MAY 将不支持的 extended length 作为 malformed 拒绝 |
| `maxPayloadSize(0x05)` | Deprecated/Reserved | 与 `maxFrameSize - 14` 重复；新实现 MUST NOT 发送 |
| `mtu(0x06)` | Profile-specific Optional | 仅在 transport/profile 需要声明 packet/report 单元时使用 |
| `windowSize(0x0C)` | P1 | 可选 flow-control 字段 |

## 保留 / 未来

Opcode registry：

| opcode | 名称 | 方向 | Body | 状态 | 含义 |
|---:|---|---|---|---|---|
| `0x00` | `RESERVED` | - | 否 | Reserved | MUST NOT 发送 |
| `0x01` | `OPEN` | Client -> Server | 是 | Required | 启动 framed link 协商 |
| `0x02` | `ACCEPT` | Server -> Client | 是 | Required | 返回协商结果 |
| `0x03` | `READY` | Client -> Server | 可选 | Reserved | 可选三步协商标记 |
| `0x04` | `HEARTBEAT` | 双向 | 可选 | Required | keepalive |
| `0x05` | `HEARTBEAT_ACK` | 双向 | 可选 | Required | keepalive 响应 |
| `0x06` | `ACK` | 双向 | 是 | Future | 确认 frame/message/stream chunk/control |
| `0x07` | `NACK` | 双向 | 是 | Future | 负确认或重传请求 |
| `0x08` | `RESUME` | Client -> Peer | 是 | P1 | session resume |
| `0x09` | `RESUME_ACK` | Peer -> Client | 是 | P1 | resume 响应 |
| `0x0A` | `CLOSE` | 双向 | 可选 | Required | close 请求 |
| `0x0B` | `CLOSE_ACK` | 双向 | 可选 | Required | close 响应 |
| `0x0C` | `SESSION_RESET` | 双向 | 是 | P1 | 强制 reset |
| `0x0D` | `WINDOW_UPDATE` | 双向 | 是 | P1 | 接收方窗口更新 |
| `0x0E` | `PING` | 双向 | 可选 | P1 | RTT/link 探测 |
| `0x0F` | `PONG` | 双向 | 可选 | P1 | RTT/link 探测响应 |
| `0x10` | `GOAWAY` | 双向 | 可选 | P2 | 关闭前拒绝新 message |
| `0x11-0x6F` | `RESERVED` | - | - | Future | 标准扩展保留 |
| `0x70-0x7E` | `EXPERIMENTAL` | - | 可选 | Experimental | 实验用途 |
| `0x7F` | `VENDOR` | 双向 | 是 | Vendor | 厂商私有扩展 |
| `0x80-0xFF` | `RESERVED` | - | - | Reserved | MUST NOT 发送 |

PING/PONG 因 opcode 值已保留而列出，但它们不是 v1 必需项。HEARTBEAT/HEARTBEAT_ACK 仍是必需的 keepalive 机制。

ACK/NACK、RESUME、SESSION_RESET、WINDOW_UPDATE、GOAWAY 和 VENDOR MUST NOT 用于声明 v1 必需 conformance。

Phase 1 不要求 runtime 实现 ACK/NACK；可靠重传、选择性确认和严格分片确认只能由未来或 profile-specific reliability profile 启用。

## 规范规则

- CONTROL 只存在于 Standard Framed profile。
- WebSocket Unframed JSON MUST NOT 发送或要求 CONTROL。
- Physical Client MUST 发起 CONTROL OPEN。
- Physical Server MUST 裁决 OPEN，并在可以安全响应时返回成功或非零拒绝状态的 ACCEPT。
- AXTP Link Session 仍由 Physical Client 发起 OPEN，并由 Physical Server 返回 ACCEPT 成功或非零拒绝结果；该物理建连方向不限制 APP_READY 后 RPC 业务 Request 的发起方。
- OPEN / ACCEPT MUST NOT 协商 Header Profile；Frame Profile 由 Transport Profile 固定。
- CONTROL MUST NOT 承载业务 capability discovery。
- CONTROL MUST NOT 承载业务 method 参数或 event。
- `capability.getAll` 等业务 capability discovery 属于 RPC 和已采纳业务协议，不属于 CONTROL。
- `statusCode` MUST 使用 ErrorCode Registry 值；`0x0000` 表示 SUCCESS。
- Unknown TLV field 在 length 合法时 MUST 被跳过。
- 新实现 MUST NOT 生成 deprecated/reserved TLV field。
- ACK/NACK 是 CONTROL payload，不是 Frame Header field。
- 除非未来/profile-specific reliability profile 明确启用，否则 Phase 1 MUST NOT 要求 Frame ACK/NACK 语义。

## 状态机 / 生命周期

Standard Framed 生命周期：

```text
DISCONNECTED
  -> transport connected
LINK_CONNECTED
  -> Physical Client 发送 CONTROL OPEN
  -> Physical Server 返回成功的 CONTROL ACCEPT
FRAMING_READY
  -> RPC Hello / Identify / Identified
APP_READY
  -> 空闲时 HEARTBEAT / HEARTBEAT_ACK
  -> CLOSE / CLOSE_ACK 或 transport lost
DISCONNECTED
```

拒绝结果：

```text
LINK_CONNECTED
  -> CONTROL OPEN
  -> CONTROL ACCEPT 携带 statusCode != SUCCESS
  -> 被拒绝；client 可调整参数重试或关闭 transport
```

CLOSE 生命周期：

```text
ANY_OPEN_STATE
  -> CLOSE(controlId=N, optional reasonCode)
  -> peer 回复 CLOSE_ACK(controlId=N, statusCode=SUCCESS)
  -> 释放 framed link context
  -> 关闭或回到 transport-specific disconnected state
```

PING/PONG 未来生命周期：

```text
APP_READY
  -> optional PING(controlId=N, timestamp/nonce)
  -> optional PONG(controlId=N, statusCode=SUCCESS)
```

该 PING/PONG 生命周期属于 RESERVED/FUTURE，不替代 v1 必需 runtime 中的 HEARTBEAT。

各状态允许的操作：

| 状态 | 允许 | 拒绝 |
|---|---|---|
| `LINK_CONNECTED` | CONTROL OPEN | RPC / STREAM / 非 OPEN CONTROL |
| `FRAMING_READY` | RPC Hello / Identify / Identified、HEARTBEAT、CLOSE | APP_READY 前的业务 RPC / STREAM |
| `APP_READY` | CONTROL heartbeat/close 和已采纳 RPC/STREAM | invalid opcode、malformed payload |

## 校验规则

Control parser MUST 校验：

- `Frame.payloadLength >= 5`;
- opcode 已知，或可根据 registry status 安全忽略；
- response-like message 的 `controlId` 关联关系；
- `statusCode` 按 ErrorCode Registry 值解释；
- TLV length 不超过 payload 边界；
- extended length `0xFF` 被处理或作为 malformed 拒绝；
- strict mode 下非 repeated TLV field 不重复出现；
- OPEN 只出现在合法启动状态；
- ACCEPT response 对应一个 pending OPEN；
- 协商出的 `maxFrameSize`、`supportedPayloadTypes`、`supportedRpcEncodings`、`heartbeatIntervalMs` 和 `ackMode` 被本地接受；
- 如果出现 `protocolVersion`，其值符合过渡兼容规则；
- 如果 profile 使用 `mtu`，协商出的 `mtu` 被本地 transport 接受；
- unknown TLV 只有在 length 校验成功后才跳过。

协商失败映射：

| 失败场景 | 推荐 statusCode | Runtime 动作 |
|---|---|---|
| Header Version 不支持 | `FRAME_VERSION_UNSUPPORTED` | 返回非零 status 的 ACCEPT，然后关闭 |
| PayloadType 无交集 | `CONTROL_NEGOTIATION_FAILED` | 返回非零 status 的 ACCEPT |
| rpcEncoding 无交集 | `CONTROL_NEGOTIATION_FAILED` | 返回非零 status 的 ACCEPT |
| Profile 要求的 MTU 过小 | `CONTROL_NEGOTIATION_FAILED` | 返回非零 status 的 ACCEPT |
| Malformed OPEN | `CONTROL_PAYLOAD_INVALID` 或不响应直接关闭 | 不进入 FRAMING_READY |
| OPEN 被策略拒绝 | `CONTROL_OPEN_REJECTED` | 返回非零 status 的 ACCEPT |

MVP 需要识别的 Control status code：

| statusCode | 名称 |
|---:|---|
| `0x0000` | `SUCCESS` |
| `0x0012` | `FRAME_VERSION_UNSUPPORTED` |
| `0x0016` | `FRAME_CRC_ERROR` |
| `0x0018` | `FRAME_FRAGMENT_MISSING` |
| `0x0021` | `CONTROL_OPCODE_INVALID` |
| `0x0022` | `CONTROL_PAYLOAD_INVALID` |
| `0x0023` | `RESERVED_CONTROL_BODY_ENCODING_UNSUPPORTED` |
| `0x0024` | `CONTROL_OPEN_REQUIRED` |
| `0x0025` | `CONTROL_OPEN_REJECTED` |
| `0x0026` | `RESERVED_CONTROL_PROFILE_UNSUPPORTED` |
| `0x0027` | `CONTROL_NEGOTIATION_FAILED` |
| `0x0028` | `CONTROL_SESSION_INVALID` |
| `0x002A` | `CONTROL_RESUME_FAILED` |
| `0x002C` | `CONTROL_WINDOW_EXCEEDED` |

新实现 MUST NOT 生成历史保留错误 `RESERVED_CONTROL_BODY_ENCODING_UNSUPPORTED` 或 `RESERVED_CONTROL_PROFILE_UNSUPPORTED`，但 SHOULD 识别它们用于诊断。

## Runtime 实现要求

TLV 编码：

```text
short TLV: type(1B) + length(1B) + value(N)
extended:  type(1B) + length(0xFF) + extendedLength(uint16) + value(N)
```

通用 TLV registry：

| TLV | 名称 | 类型 | 状态 |
|---:|---|---|---|
| `0x01` | `sessionId` | uint32 | Optional |
| `0x02` | `protocolVersion` | uint8 | Deprecated/Transition |
| `0x03` | `reserved` | - | 历史 headerProfile 保留 |
| `0x04` | `maxFrameSize` | uint16/uint32 | Required |
| `0x05` | `maxPayloadSize` | uint16 | Deprecated/Reserved |
| `0x06` | `mtu` | uint16 | Profile-specific Optional |
| `0x07` | `supportedPayloadTypes` | bitmap | Required |
| `0x08` | `supportedRpcEncodings` | bitmap | Required |
| `0x09` | `reserved` | - | 历史字段保留 |
| `0x0A` | `heartbeatIntervalMs` | uint16/uint32 | Required |
| `0x0B` | `ackMode` | uint8 | Required |
| `0x0C` | `windowSize` | uint16 | P1 |
| `0x0D` | `compression` | bitmap | P2 |
| `0x0E` | `encryption` | bitmap | P2 |
| `0x0F` | `timestamp` | uint64 | P1 |
| `0x10` | `reasonCode` | uint16 | 出现 close reason 时必需 |
| `0x11` | `messageId` | uint16 | Future ACK/NACK |
| `0x12` | `frameIndex` | uint8 | Future ACK/NACK |
| `0x13` | `frameCount` | uint8 | P1 |
| `0x14` | `missingRanges` | bytes | P1 |
| `0x15` | `streamId` | uint32 | P1 |
| `0x16` | `seqId` | uint32 | P1 |
| `0x17` | `offset` | uint64 | P1 |
| `0x18` | `resumeToken` | bytes | P1 |
| `0x19` | `errorDetail` | string/bytes | P1 |
| `0x1A` | `controlFlags` | uint8 bitmap | Deprecated |
| `0x1B` | `statusCode` | uint16 | Deprecated |
| `0x1C` | `nonce` | bytes | P1 |
| `0x1D` | `rttMs` | uint16/uint32 | P2 |
| `0x1E` | `selectedRpcEncoding` | uint8 | ACCEPT 中必需 |
| `0x1F` | `reserved` | - | 历史字段保留 |
| `0x20` | `targetType` | uint8 | Future ACK/NACK |
| `0x21` | `targetOpcode` | uint8 | P1 |
| `0x22` | `targetControlId` | uint16/uint8 | P1 |
| `0x23` | `retryAfterMs` | uint16/uint32 | P1 |
| `0x24` | `receivedCount` | uint16/uint32 | P2 |
| `0x25` | `expectedCount` | uint16/uint32 | P2 |
| `0x70-0x7E` | `experimental` | bytes | Experimental |
| `0x7F` | `vendorData` | bytes | Vendor |

PayloadType bitmap：

| bit | PayloadType |
|---:|---|
| 0 | CONTROL |
| 1 | RPC |
| 2 | STREAM |

RPC encoding bitmap：

| bit | rpcEncoding |
|---:|---|
| 0 | JSON |
| 1 | CBOR |
| 2 | MSGPACK |
| 3 | JSON_BINARY |
| 4-31 | RESERVED |

Ack mode：

| ackMode | 名称 | 状态 |
|---:|---|---|
| `0x00` | `NONE` | 必需默认值 |
| `0x01` | `FRAME_ACK` | Future |
| `0x02` | `MESSAGE_ACK` | Future |
| `0x03` | `STREAM_CHUNK_ACK` | Future |
| `0x04` | `SELECTIVE_ACK` | Future |
| `0x05` | `RESERVED` | 历史保留；新实现 MUST NOT 使用 |

Runtime limit SHOULD 包含：

```text
maxControlPayloadSize <= 512B
maxTlvCount <= 64
maxErrorDetailSize <= 128B
minHeartbeatIntervalMs >= 500
maxHeartbeatIntervalMs <= 60000
```

## 示例

最小 OPEN TLV body：

```text
04 02 10 00     // maxFrameSize = 4096
07 01 07        // supportedPayloadTypes = CONTROL/RPC/STREAM
08 01 09        // supportedRpcEncodings = JSON + JSON_BINARY
0A 02 03 E8     // heartbeatIntervalMs = 1000
0B 01 00        // ackMode = NONE
```

最小 ACCEPT 成功 TLV body：

```text
04 02 10 00     // maxFrameSize = 4096
07 01 07        // payloadTypes = CONTROL/RPC/STREAM
1E 01 01        // selectedRpcEncoding = JSON
0A 02 03 E8     // heartbeatIntervalMs = 1000
0B 01 00        // ackMode = NONE
```

OPEN 拒绝：

```text
opcode=ACCEPT(0x02)
controlId=<same as OPEN>
statusCode=CONTROL_NEGOTIATION_FAILED or CONTROL_OPEN_REJECTED
body MAY include supported parameters or errorDetail
```

## 非目标

- CONTROL 不提供业务 capability discovery。
- CONTROL 不替代 RPC Identify 或 RPC authentication。
- CONTROL 不承载 method 参数、event payload 或 stream data。
- READY、ACK/NACK、RESUME、SESSION_RESET、WINDOW_UPDATE、PING/PONG、GOAWAY 和 VENDOR 不是 v1 必需实现项。
- CONTROL 不定义 WebSocket Unframed JSON 启动流程。
