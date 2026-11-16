# 1-core/03《AXTP Frame 与 Payload 规范》

> 状态： 规范性 runtime 实现规范
> 规范版本： 1.0.0-rc1
> 变更策略： v1.0.0 前仅允许澄清性修改
> 本文的规范范围：Standard Framed Binary 的 frame 边界、Frame Header 字段、PayloadType 分发、分片和 CRC。
> 本文不定义：RPC method、业务 schema、CONTROL opcode 语义、RPC op 语义、STREAM Stream Context、registry 条目或低带宽 Compact Frame 语义。
> Runtime 实现状态：`AXTP-USB-HID` 和 `AXTP-TCP` 必需实现；WebSocket Unframed JSON 不使用本文定义的 Standard Frame。

版本：v1.0.0-rc1
状态：AXTP v1 Core 冻结候选
适用范围：AXTP v1 Core 的 Standard Framed Binary 模式
前置文档：[`02-Protocol-Framework.md`](02-Protocol-Framework.md)
后续文档：[`04-Transport-Profiles.md`](04-Transport-Profiles.md)、[`05-Control-Session.md`](05-Control-Session.md)、[`06-RPC-Session.md`](06-RPC-Session.md)、[`07-Stream-Data-Plane.md`](07-Stream-Data-Plane.md)、[`08-Low-Bandwidth-Degradation.md`](08-Low-Bandwidth-Degradation.md)

---

## 文档目的

本文定义 AXTP Standard Framed Binary 的 L1 frame / payload 边界。Frame Header 只识别 PayloadType，并把 payload 交给 CONTROL、RPC 或 STREAM parser；它不理解 domain、feature、method、event、schema 或 stream profile。

## 范围

本文覆盖：

- Standard Frame Header 固定 12B；
- PayloadType 三分类；
- PayloadLength 语义；
- MessageId / FrameIndex / FrameCount 分片边界；
- CRC16-CCITT-FALSE footer；
- Frame parser 的验证和分发要求。

本文不覆盖：

- WebSocket Unframed JSON 的 `sid/op/d` envelope；
- CONTROL Payload 内部 opcode/TLV；
- RPC Payload 的 op、sid、requestId、methodId/eventId；
- STREAM 16B Header 的 streamId/seqId/cursor 语义；
- Compact / HID-64 / BLE / UART 降级 frame。

## v1 必需实现

Standard Framed Binary runtime MUST 实现以下线上结构：

```text
Standard Frame Header(12B) + Payload(N) + CRC16(2B)
```

Header 布局：

```text
Offset  0: Magic[0](1)  Magic[1](1)  Version(1)  PayloadType(1)
Offset  4: PayloadLength(2)  SourceId(1)  DestinationId(1)
Offset  8: MessageId(2)  FrameIndex(1)  FrameCount(1)
Offset 12: Payload starts
Footer:    CRC16(2)，位于 Payload 末尾
```

| 字段 | 偏移 | 长度 | 类型 | 规则 |
|---|---:|---:|---|---|
| `Magic[0]` | 0 | 1B | uint8 | MUST 等于 `0x41` (`A`) |
| `Magic[1]` | 1 | 1B | uint8 | MUST 等于 `0x58` (`X`) |
| `Version` | 2 | 1B | uint8 | 当前值为 `0x01` |
| `PayloadType` | 3 | 1B | enum | CONTROL=`0x01` / RPC=`0x02` / STREAM=`0x03` |
| `PayloadLength` | 4 | 2B | uint16 | Payload 字节长度，不包含 Header 和 CRC |
| `SourceId` | 6 | 1B | uint8 | 发送方逻辑节点 |
| `DestinationId` | 7 | 1B | uint8 | 接收方逻辑节点 |
| `MessageId` | 8 | 2B | uint16 | 用于 frame/message 关联的逻辑 message id |
| `FrameIndex` | 10 | 1B | uint8 | 分片序号，从 0 开始 |
| `FrameCount` | 11 | 1B | uint8 | 分片总数；未分片 message 使用 1 |
| `CRC16` | Footer | 2B | uint16 | 覆盖 Header(12B) + Payload 的 CRC16-CCITT-FALSE |

所有多字节整数 MUST 使用 Big-Endian，即 network byte order 编码。

PayloadType registry：

| ID | 名称 | 解析器 |
|---:|---|---|
| `0x01` | `CONTROL` | ControlParser |
| `0x02` | `RPC` | RpcParser |
| `0x03` | `STREAM` | StreamParser |

PayloadType MUST 只选择下一级 parser。它 MUST NOT 编码 `VIDEO`、`AUDIO`、`FIRMWARE_UPDATE`、`FILE`、domain.feature、method、event 或 capability 语义。

## v1 可选 / Profile 特定

分片是 Standard Frame 的一部分；当逻辑 message 超过协商 `maxFrameSize`，或超过 profile-specific transport MTU 时 MAY 使用。

Profile 特定行为：

- TCP byte-stream 实现 SHOULD 先扫描 `Magic = 0x41 0x58`，再解析 header 剩余部分。
- 具有 packet 边界的 transport MAY 每个 packet 接收一个完整 frame，但仍然 MUST 校验 12B header 和 CRC。
- 通过 `SourceId` 和 `DestinationId` 路由属于 profile/product 特定行为；点对点设备 MAY 使用固定值。

## 保留 / 未来

以下变化属于 RESERVED/FUTURE，MUST NOT 被视为 v1 runtime 必需行为：

- 改变 12B Standard Frame Header 布局；
- 重新定义已有 PayloadType 值；
- 使用 PayloadType 表达业务分类；
- 在同一个 AXTP Session 内切换 Frame Profile；
- 在 Standard Framed transport 上用 Compact Frame 替代 Standard Frame。

低带宽 Compact Frame 属于 profile 特定能力，由 [`08-Low-Bandwidth-Degradation.md`](08-Low-Bandwidth-Degradation.md) 覆盖；它不修改本文的 Standard Frame 定义。

## 规范规则

- Standard Framed receiver MUST 先解析恰好 12B 的 Frame Header，再读取 Payload。
- `PayloadLength` MUST 只解释为 Payload 字节数。
- CRC16 MUST 覆盖 Header(12B) + Payload，且 MUST NOT 覆盖 CRC footer 本身。
- receiver MUST 只根据 `PayloadType` 将 Payload 分发给 CONTROL、RPC 或 STREAM。
- receiver 在 Frame 层 MUST NOT 检查 method、event、domain、feature、capability、codec、file type、firmware metadata 或 stream profile。
- 同一逻辑 message 的所有 fragment MUST 使用相同 `MessageId`。
- `FrameIndex` MUST 从 0 开始，并标识 fragment 在 `FrameCount` 中的位置。
- Request/Response 匹配 MUST 使用 RPC `requestId`，而不是 Frame `MessageId`。
- STREAM 排序 MUST 使用 STREAM `seqId`，而不是 Frame `MessageId`。
- 新增 MethodId、EventId、ErrorCode、Stream Profile、schema field 或 TLV field MUST NOT 要求改变 Frame Header `Version`。
- 对 Header 字段顺序、长度、含义、CRC 规则或 PayloadType 编码的不兼容变更 MUST 要求新的 Header `Version`。

## 状态机 / 生命周期

byte-stream parser SHOULD 遵循以下生命周期：

```text
WAIT_MAGIC
  -> READ_HEADER
  -> READ_PAYLOAD
  -> READ_CRC
  -> VALIDATE
  -> DISPATCH_PAYLOAD
```

分片重组生命周期：

```text
single frame：
  FrameCount = 1, FrameIndex = 0
  -> validate CRC
  -> dispatch Payload

fragmented message：
  collect frames with same MessageId
  -> require FrameIndex range 0..FrameCount-1
  -> reassemble complete Payload
  -> dispatch reassembled Payload
```

如果 runtime 因 profile 特定原因选择在完整重组前分发单个 frame，该行为 MUST NOT 改变 CONTROL/RPC/STREAM payload 语义。

## 校验规则

Standard Frame parser MUST 校验：

- Magic bytes 等于 `0x41 0x58`；
- Header `Version` 受支持；
- `PayloadType` 是 `0x01`、`0x02`、`0x03` 之一；
- `PayloadLength + 14` 不超过本地和协商出的 `maxFrameSize`；
- `FrameCount >= 1`;
- `FrameIndex < FrameCount`;
- CRC16 与 Header(12B) + Payload 匹配；
- 分发前已收到完整 payload bytes。

CRC 失败时，Phase 1 runtime MAY 丢弃该 frame 并记录错误。如果 runtime 无法安全重新同步或连续失败，SHOULD 关闭 transport。通过 CONTROL NACK 做可靠重传属于可选/未来能力，MUST NOT 成为 v1 frame parsing 的必需条件。

## Runtime 实现要求

- 实现 MUST 保持 Frame parsing 与业务 registry lookup 解耦。
- 实现 MUST 限制因 `PayloadLength`、`FrameCount` 和 `MessageId` 重组状态分配的内存。
- 实现 SHOULD 对最大 payload size、最大在途分片 message 数和重新同步次数设置本地限制。
- 实现 MUST 完成 L1 frame 校验后，才能把字节交给 CONTROL/RPC/STREAM parser。
- 实现 MUST 将 WebSocket Unframed JSON 视为独立 transport profile，而不是 Standard Frame payload。

## 示例

最小未分片 Standard Frame 结构：

```text
41 58                // Magic "AX"
01                   // Version
02                   // PayloadType = RPC
00 05                // PayloadLength = 5
01                   // SourceId
02                   // DestinationId
00 2A                // MessageId = 42
00                   // FrameIndex = 0
01                   // FrameCount = 1
... 5 payload bytes ...
.. ..                // CRC16 over Header + Payload
```

Parser 分发边界：

```text
PayloadType=CONTROL -> ControlParser parses opcode/controlId/statusCode/TLV
PayloadType=RPC     -> RpcParser parses rpcEncoding and RPC envelope/header
PayloadType=STREAM  -> StreamParser parses streamId/seqId/cursor/data
```

## 非目标

- 本文不定义任何 RPC method、event、error、capability、schema 或 profile。
- 本文不定义 CONTROL 业务 capability discovery。
- 本文不定义 JSON、CBOR、MSGPACK、JSON_BINARY body schema。
- 本文不定义 stream open/close 业务 method。
- 本文不把 Compact / BLE / UART 降级作为 v1 Core 必需行为。
