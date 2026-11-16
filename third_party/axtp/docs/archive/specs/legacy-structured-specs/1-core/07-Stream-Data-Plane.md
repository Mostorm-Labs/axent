# 1-core/07《AXTP STREAM 数据面规范》

> 状态： 规范性 runtime 实现规范
> 规范版本： 1.0.0-rc1
> 变更策略： v1.0.0 前仅允许澄清性修改
> 本文的规范范围：`PayloadType=STREAM` payload 结构、16B STREAM Header、streamId/seqId/cursor 语义、Stream Context 查找和基础数据面 parser 行为。
> 本文不定义：建立 stream 的业务 method、firmware/file/log schema、CONTROL ACK/NACK 协议、可靠重传、resume 协议、codec metadata 或业务 payload 格式。
> Runtime 实现状态：支持 STREAM 的 Standard Framed runtime 必需实现；可靠性、resume、高级流控和低带宽 STREAM 属于可选/未来或 profile 特定能力。

版本：v1.0.0-rc1
状态：AXTP v1 Core 冻结候选
适用范围：`PayloadType = STREAM` 的 Payload 结构、Stream Context、基础顺序检测、多路复用与 profile 边界
前置文档：[`02-Protocol-Framework.md`](02-Protocol-Framework.md)、[`03-Frame-and-Payload.md`](03-Frame-and-Payload.md)、[`05-Control-Session.md`](05-Control-Session.md)、[`06-RPC-Session.md`](06-RPC-Session.md)

---

## 文档目的

STREAM 是 AXTP 的连续数据面。它只传输已经建立的 stream 的数据块，不声明业务类型、不协商 codec、不携带固件/文件/日志元数据。

STREAM 在线形态：

```text
Standard Frame Header(payloadType=STREAM)
  + STREAM Payload = streamId(4) + seqId(4) + cursor(8) + data(N)
  + CRC16(2)
```

业务语义来自本地 Stream Context。Stream Context 通常由已采纳 RPC method 建立；CONTROL CLOSE 或 transport loss 会关闭所属 session 的 stream 资源。

## 范围

本文覆盖：

- 16B STREAM Header；
- `streamId`、`seqId`、`cursor` 语义；
- STREAM payload 透传边界；
- Stream Context 生命周期和资源回收；
- Frame 分片与 Stream 分块边界；
- 基础 parser 校验；
- optional/future reliability、resume、flow-control 边界。

本文不覆盖：

- 具体 `stream.open` 或 domain-specific open/close method；
- 业务 profile schema；
- firmware/file/log transfer 业务规则；
- CONTROL ACK/NACK 详细重传规则；
- legacy RawStream 映射。

## v1 必需实现

STREAM 只存在于 Standard Framed profile。WebSocket Unframed JSON 是 RPC-only，MUST NOT 承载 STREAM。

STREAM Header 固定为 16B：

```text
+------------------+
| streamId:uint32  |  4B
+------------------+
| seqId:uint32     |  4B
+------------------+
| cursor:uint64    |  8B
+------------------+
| data...          |  N
```

| 字段 | 类型 | 长度 | 规则 |
|---|---|---:|---|
| `streamId` | uint32 | 4B | stream setup 期间分配的 stream channel id |
| `seqId` | uint32 | 4B | stream 内 packet 序号；从 0 开始并自然回绕 |
| `cursor` | uint64 | 8B | 通用位置/时间 cursor；含义来自 Stream Context |
| `data` | bytes | N | 不透明业务 payload |

所有多字节字段 MUST 使用 Big-Endian，即 network byte order。`Frame.payloadLength` MUST 等于 `16 + dataLength`。

`streamId` 范围：

| 范围 | 含义 |
|---|---|
| `0x00000000` | 保留；MUST NOT 使用 |
| `0x00000001-0x7FFFFFFF` | 标准动态 streamId |
| `0x80000000-0xEFFFFFFF` | 扩展保留 |
| `0xF0000000-0xFFFFFFFF` | Vendor/Debug 保留 |

基础 runtime MUST 实现：

- 解析 16B STREAM Header；
- 拒绝 `streamId = 0`；
- 通过 `streamId` 查找 Stream Context；
- 将 `data` 作为不透明 bytes 交给 profile handler；
- 检测 duplicate/missing/out-of-order `seqId`；
- 解析 `cursor`，但不假设其业务含义；
- 区分 Frame fragmentation 与 Stream data chunk；
- 所属 session 关闭时清理 stream 资源。

## v1 可选 / Profile 特定

以下内容属于 profile 特定：

| 能力 | 状态 | 规则 |
|---|---|---|
| 具体 stream open/close method | 业务/profile 特定 | MUST 来自已采纳 RPC method 或产品 profile |
| cursor unit | Profile-specific | `timestampUs`、`byteOffset`、`sampleIndex` 或其他 profile-defined unit |
| `maxDataSize` / chunk size | Profile-specific | 通常由 RPC stream setup 协商 |
| media best-effort delivery | Profile-specific | MAY 使用 `ackMode=none` |
| firmware/file reliable delivery | Future/profile-specific | 采纳后 MAY 使用 stop-and-wait 或 sliding-window |
| concurrent streams | 基于 streamId 隔离的必需基础支持；scheduling/QoS 可选 | Runtime SHOULD 按 streamId 隔离状态 |

Cursor 示例：

| 场景 | cursorUnit | cursor 含义 |
|---|---|---|
| media/log/HID | `timestampUs` | 微秒时间戳 |
| firmware/file | `byteOffset` | 字节偏移 |
| sensor/audio | `sampleIndex` | 采样序号 |

这些 cursor unit 是 profile metadata。它们 MUST NOT 向 STREAM Header 添加字段。

## 保留 / 未来

以下内容属于 RESERVED/FUTURE，MUST NOT 作为 v1 基础 STREAM conformance 的必需项：

- 用于 stream chunk 的 CONTROL ACK/NACK；
- stop-and-wait、sliding-window 或 selective-repeat retransmission；
- CONTROL WINDOW_UPDATE 流控；
- 基于 RESUME / RESUME_ACK 的 stream recovery；
- object-level firmware/file 校验流程；
- HID-64/BLE/UART 上的 low-bandwidth STREAM；
- 向 16B STREAM Header 添加 codec、fileType、firmware version、offset、timestamp、flags 或 domain 字段。

Reliable transfer 和 resume MAY 由未来 profile-specific spec 添加，但它们 MUST 保持 16B STREAM Header 不变。

## 规范规则

- STREAM 在 Standard Framed profile 中 MUST 使用 `PayloadType=STREAM`。
- Stream Context 存在前 MUST NOT 发送 STREAM。
- 数据传输前，Stream Context MUST 由已采纳 RPC method 或 profile-specific control flow 建立。
- CONTROL CLOSE、transport loss 或 session teardown MUST 释放该 session 的 stream 资源。
- STREAM Header MUST 保持为 16B：4B `streamId`、4B `seqId`、8B `cursor`。
- STREAM Header MUST NOT 承载 business type、codec、fileType、firmware parameter、profileId、domain、event 或 capability data。
- 通用 StreamParser MUST 将 STREAM `data` 视为不透明数据。
- Receiver MUST 完成 L1 Frame reassembly 后再解析 STREAM Header。
- `messageId`、`streamId`、`seqId` 和 RPC `requestId` MUST NOT 混用。
- stream profile metadata MAY 出现在 Protocol Definition 或 RPC response 中，但 MUST NOT 写入 STREAM Header。

## 状态机 / 生命周期

基础 Stream Context 生命周期：

```text
CREATED
  -> OPENING
  -> OPEN
  -> ACTIVE
  -> DRAINING
  -> CLOSED
```

典型启动流程：

```text
已采纳 RPC open/begin method
  -> RPC response 返回 streamId 和 Stream Context metadata
  -> sender 使用该 streamId 发送 STREAM frames
  -> 已采纳 RPC close/stop/end method，或 CONTROL/session close
  -> runtime 释放 Stream Context
```

状态处理：

| Receiver 状态 | 收到的 STREAM | 必需处理 |
|---|---|---|
| `OPENING` | data 提前到达 | MAY 有界缓存少量数据，或丢弃并本地计数 |
| `OPEN` / `ACTIVE` | 有效 data | 交给 profile handler |
| `DRAINING` | 新 data | 丢弃；MAY 通过已采纳业务机制暴露本地 diagnostic/event |
| `CLOSED` | 任意 data | 丢弃并释放相关 buffer |
| 任意 | unknown streamId | 丢弃并记录 `STREAM_NOT_FOUND`；未来 reliable profile MAY NACK |

资源清理：

- session 关闭时，Runtime MUST 关闭该 session 的所有 Stream Context。
- Runtime SHOULD 执行 `streamIdleTimeoutMs` 并关闭不活跃 stream。
- Runtime SHOULD 限制 `maxOpenStreams`、`maxDataSize` 和 per-stream buffering。

推荐默认值：

| 参数 | 推荐值 |
|---|---:|
| `maxOpenStreams` | 2 |
| `streamIdleTimeoutMs` | 30000 |
| `streamOpeningTimeoutMs` | 5000 |

## 校验规则

StreamParser MUST 校验：

- `Frame.payloadLength >= 16`;
- payload bytes 包含完整 16B STREAM Header；
- `streamId != 0`;
- streamId 映射到已存在的 Stream Context；
- data length 不超过 Stream Context/profile 限制；
- `seqId` duplicate/missing/out-of-order 行为按 profile 处理；
- `cursor` 按 uint64 Big-Endian / network byte order 解析，且只由 Stream Context/profile 解释；
- STREAM parsing 前已经完成 L1 Frame CRC 和 reassembly。

基础错误处理：

| 错误 | 必需基础处理 |
|---|---|
| `payloadLength < 16` | 丢弃；记录 `STREAM_PAYLOAD_INVALID` |
| `streamId = 0` | 丢弃；记录 `STREAM_ID_INVALID` |
| unknown streamId | 丢弃；记录 `STREAM_NOT_FOUND` |
| duplicate `seqId` | 丢弃或幂等处理 |
| missing `seqId` | 记录丢失；base best-effort profile 继续 |
| data 超过 `maxDataSize` | 丢弃并 MAY 关闭 stream |

未来 reliable profile MAY 将这些条件转换为 CONTROL NACK 或 profile-specific recovery。

## Runtime 实现要求

Parser 流程：

```text
parse AXTP Frame Header
  -> if payloadType != STREAM: dispatch other parser
  -> ensure payloadLength >= 16
  -> read streamId / seqId / cursor
  -> data = payload[16:]
  -> lookup Stream Context by streamId
  -> validate seqId/cursor/data size according to profile
  -> dispatch opaque data to profile handler
```

Frame fragmentation 与 Stream chunking：

| 概念 | 层级 | 规则 |
|---|---|---|
| Frame fragmentation | L1 Frame | STREAM parsing 前先 reassemble |
| Stream chunk | L2 STREAM | 一个带 streamId/seqId/cursor/data 的 STREAM payload |

Runtime MUST 保持这些层级分离。

完整性边界：

- Frame CRC 校验单个 AXTP frame。
- md5/sha256/crc32 等 object-level verification 属于已采纳业务/profile method。
- Chunk-level CRC 或 retransmission 属于可选/未来 reliable profile。

## 示例

最小 STREAM packet payload：

```text
streamId=1, seqId=0, cursor=0, data=AA BB CC DD

00 00 00 01              // streamId
00 00 00 00              // seqId
00 00 00 00 00 00 00 00  // cursor
AA BB CC DD              // opaque data
```

对象传输 chunk 示例：

```text
streamId=33, seqId=1, cursor=512, data=object[512..1023]

00 00 00 21              // streamId=33
00 00 00 01              // seqId=1
00 00 00 00 00 00 02 00  // cursor=512
[512 bytes opaque data]
```

ID 边界：

| 字段 | 层级 | 用途 |
|---|---|---|
| `messageId` | Frame | fragment/reassembly/debug |
| `requestId` | RPC | Request/Response 匹配 |
| `streamId` | STREAM | Stream context 查找 |
| `seqId` | STREAM | stream 内 packet 顺序 |
| `cursor` | STREAM | 根据 stream context 表达位置/时间 |

## 非目标

- 本文不定义通用 `stream.open` 或任何具体 domain open/close method。
- 本文不定义 firmware update、file transfer、log streaming、KVM/HID、sensor、audio 或 video 业务 schema。
- 本文不要求 reliable retransmission 或 resume 作为 v1 基础行为。
- 本文不定义 legacy RawStream 映射。
- 本文不为 codec、business type、file type、timestamp、offset、checksum 或 flags 向 STREAM Header 添加字段。

非规范说明：`streamType`、`timestamp`、`flags`、`dataLength`、旧 firmware `offset` 或 chunk CRC 等 legacy stream 字段，应通过 RPC Stream Context、profile metadata、payload data 或 legacy migration adapter 映射，而不是修改 16B STREAM Header。
