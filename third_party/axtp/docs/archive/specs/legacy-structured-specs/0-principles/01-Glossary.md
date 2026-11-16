# 00《AXTP 术语表》

> 状态：AXTP v1 Core 冻结候选
> 规范版本： 1.0.0-rc1
> 变更策略： v1.0.0 前仅允许澄清性修改
> 范围：共享术语、领域语言和跨 runtime 对齐

版本：v1.0.0-rc1
状态：AXTP v1 Core 冻结候选
适用范围：AXTP 全仓库术语、跨语言 runtime 沟通、规范文档第一次引用专有名词时的链接目标
后续文档：`specs/1-core/01-Overview.md`、`specs/1-core/02-Protocol-Framework.md`、`specs/1-core/03-Frame-and-Payload.md`

---

## 0. 文档目的

本文是 AXTP 的全局统一术语表。它回答“这个词在 AXTP 里到底是什么意思”，用于避免 C / C++ / Flutter / Python / Web / firmware / cloud / test 团队对同一术语产生不同理解。

规范文档第一次使用 AXTP 专有名词时，应该链接到本文对应条目。本文只定义术语边界；具体 wire format、字段布局、ID 分配和采纳状态仍以对应 specs、registry YAML 与 generated 输出为准。

引用示例：

```markdown
PayloadType -> ../0-principles/01-Glossary.md#payloadtype
RPC Session -> ../0-principles/01-Glossary.md#rpc-session
Domain -> ../0-principles/01-Glossary.md#domain
```

从 `specs/1-core/**` 等子目录引用本文时，使用 `../0-principles/01-Glossary.md#anchor`；从 `specs/README.md` 同目录引用时，使用 `0-principles/01-Glossary.md#anchor`。

---

## 1. 核心分层术语

### AXTP

**AXTP**（Auditoryworks Transport Protocol）是面向智能硬件、音视频设备和多端 SDK 的统一通信协议规范。它覆盖连接建立、会话协商、RPC 请求/响应、事件、STREAM 连续数据和低带宽链路适配。

不要把 AXTP 理解为某一个语言 runtime、某一种 TCP 协议实现或某一个 WebSocket JSON API。AXTP 是跨 transport、跨 runtime 的协议标准。

### Transport

**Transport** 是承载 AXTP 字节或 JSON 消息的底层连接方式，例如 WebSocket、TCP、USB HID、BLE、UART 或 mock transport。

Transport 负责连接和字节传输，不理解 AXTP 的 `methodId`、`eventId`、`streamId` 或业务语义。

### Transport Profile

**Transport Profile** 定义某一种 transport 如何承载 AXTP，例如是否使用 Frame Header、是否支持 CONTROL、是否支持 STREAM、MTU 和编码约束是什么。

示例：`AXTP-WS-JSON`、`AXTP-WS-CLOUD-REVERSE`、`AXTP-TCP`、`AXTP-USB-HID`。

### Control Plane

**Control Plane** 是协议控制面，负责连接建立、会话协商、RPC 请求/响应、事件订阅、能力发现和业务控制命令。

AXTP 中 CONTROL 和 RPC 都属于控制面，但职责不同：CONTROL 是协议运行时控制，RPC 是业务控制。

### Data Plane

**Data Plane** 是数据面，负责搬运连续数据，例如音视频帧、文件块、OTA 数据块或日志数据块。

AXTP 中 P0 数据面由 `PayloadType=STREAM` 承载。不要把大块连续数据塞进普通 RPC Response 或 Event。

### Layer / L1 / L2

**Layer** 是协议分层。AXTP 文档中常用：

| 层级 | 名称 | 职责 |
|---|---|---|
| L1 | Frame Layer | Frame Header、PayloadType、payloadLength、fragment、CRC。 |
| L2 | Payload Layer | CONTROL / RPC / STREAM 的内部 payload 结构。 |
| Registry Layer | Registry | method/event/error/schema/capability/profile 的机器事实。 |
| Business Layer | Business | 具体设备业务语义，例如 audio、video、system、network。 |

L1 只决定外层 frame 如何解析；L2 决定 payload 内部如何解析。不要把 L2 的业务语义写进 L1 Frame Header。

---

## 2. Frame 与 Payload 术语

### Frame

**Frame** 是 Standard Framed transport 上的 AXTP 外层传输单元。

标准结构：

```text
Standard Frame Header(12B) + Payload(N) + CRC16(2B)
```

WebSocket Unframed JSON 路径没有 Frame Header，也没有 CRC16。

### Frame Header

**Frame Header** 是 Standard Frame 的 12B 固定头，包含 magic、version、payloadLength、PayloadType、messageId、fragment 等 L1 字段。

Frame Header 不承载业务类型。`VIDEO`、`FIRMWARE_UPDATE`、`FILE` 等不能成为 Frame Header 字段或 PayloadType。

### Payload

**Payload** 是 Frame Header 后面的 L2 数据块。它由 `PayloadType` 决定交给哪个 parser：

| PayloadType | 解析器 |
|---|---|
| CONTROL | ControlParser |
| RPC | RpcParser |
| STREAM | StreamParser |

### PayloadType

**PayloadType** 是 Frame Header 中的一级 parser 选择器。AXTP v1 Core 只定义三类：

| PayloadType | 名称 | 说明 |
|---:|---|---|
| `0x01` | CONTROL | 协议运行时控制。 |
| `0x02` | RPC | 业务控制面消息。 |
| `0x03` | STREAM | 连续数据面消息。 |

PayloadType 不表达具体业务类型。不要新增 `VIDEO`、`AUDIO`、`OTA`、`FILE` 等业务 PayloadType。

### Fragment

**Fragment** 是 L1 Frame 分片机制，用于把一个过大的 Frame Message 拆成多个 Frame 传输。

Fragment 与 STREAM 业务分块不同：接收端必须先完成 L1 Frame 重组，再解析 L2 Payload。

### CRC16

**CRC16** 是 Standard Framed 路径上的帧校验值，覆盖 Header + Payload。它用于发现传输错误，不表达业务成功或失败。

---

## 3. CONTROL / RPC / STREAM

### CONTROL

**CONTROL** 是 `PayloadType=CONTROL` 的协议运行时控制 payload，负责 OPEN / ACCEPT、HEARTBEAT、CLOSE，以及后续预留的 ACK / NACK / RESUME / flow-control。

CONTROL 不承载业务 method。业务命令必须通过 RPC。

### RPC

**RPC** 是 `PayloadType=RPC` 的业务控制 payload，承载 Hello / Identify / Identified / Request / Response / Event / Batch 等 op。

RPC 可以跑在 WebSocket Unframed JSON 上，也可以作为 Standard Frame 的 payload。

### STREAM

**STREAM** 是 `PayloadType=STREAM` 的连续数据 payload，使用固定 16B STREAM Header：

```text
streamId:uint32 + seqId:uint32 + cursor:uint64
```

STREAM 本身只携带流 ID、顺序和游标；具体业务类型通过 RPC 建流返回的 stream context / profile 绑定，不写进 STREAM Header。

### Control Payload

**Control Payload** 是 CONTROL 的 L2 数据结构：

```text
opcode(1) + controlId(2) + statusCode(2) + TLV body(N)
```

### RPC Envelope

**RPC Envelope** 是 JSON / CBOR / MSGPACK 路径统一使用的对象结构：

```json
{ "sid": "12345678", "op": 7, "d": { } }
```

`sid` 表示 RPC Session，`op` 表示 RPC 操作码，`d` 是 op 相关的数据块。

### STREAM Header

**STREAM Header** 是 STREAM 的 L2 固定头。它不包含 codec、fileType、firmware version、domain 或 event 信息。

---

## 4. Session 与 ID 术语

### Session

**Session** 是上下文关系的泛称。AXTP 中不要单独使用 “Session” 表达具体对象，必须说明是哪一种 session。

常见 session：

| 名称 | 所属层 | 含义 |
|---|---|---|
| Transport connection | Transport | TCP/WebSocket/USB HID 等底层连接。 |
| Framed Link Context | CONTROL / L1-L2 | Standard Framed OPEN / ACCEPT 后形成的链路上下文。 |
| RPC Session | RPC | Hello / Identify / Identified 后形成的应用层业务 session。 |
| Stream Context | STREAM / Business | RPC 建流后形成的连续数据流上下文。 |

不要把 RPC Session 等同于 TCP 连接，也不要把它理解为用户登录态。

### Framed Link Context

**Framed Link Context** 是 Standard Framed transport 在 CONTROL OPEN / ACCEPT 后建立的 AXTP 链路上下文，保存 negotiated payload types、rpc encodings、maxFrameSize、可选 profile MTU、heartbeatInterval、ackMode 等运行时参数。

WebSocket Unframed JSON 没有 CONTROL OPEN / ACCEPT，因此没有 Standard Framed Link Context。

### RPC Session

**RPC Session** 是应用层业务 session，由 Logical Server 通过 Identified 分配 `sid` 后建立。

RPC Session 用于路由 Request / Response / Event、恢复断线会话和区分同一连接上的逻辑业务上下文。

### Stream Context

**Stream Context** 是通过业务 RPC 建流后形成的数据流上下文，至少包含 `streamId`、stream profile、方向、媒体/文件/OTA 等业务参数和生命周期状态。

### sid

**sid** 是 RPC Session ID。规范类型是 `uint32`，JSON 表达为固定 8 位十六进制字符串，例如 `"12345678"`；JSON_BINARY 中表达为 Big-Endian / network byte order `uint32`。

`sid=""` 表示尚未分配 RPC Session，常见于 Hello / Identify 新建 session 阶段。Identified 后 Request / Response / Event 必须携带非 0 `sid`。

`sid` 不是任意 token、UUID、用户登录态或 TCP connection id。

### requestId

**requestId** 是 RPC 请求 ID，在 JSON d-block 中写作 `d.id`。它用于匹配 Request 和 RequestResponse。

`requestId` 不用于 Frame 分片，不用于 STREAM 顺序，也不用于事件订阅。

### messageId

**messageId** 是 L1 Frame Header 字段，用于标识完整 Frame Message 及其分片/ACK/NACK 相关上下文。

`messageId` 不匹配 RPC Response；RPC Response 必须通过 `requestId` 匹配。

### streamId

**streamId** 是 STREAM 数据流 ID，由 RPC 建流方法返回，后续 STREAM Packet 通过它路由到对应 Stream Context。

`streamId` 不等同于 sid。一个 RPC Session 下可以创建多个 streamId。

### seqId

**seqId** 是 STREAM 内的包序号，用于流内顺序、丢包统计或后续重传/恢复。

### cursor

**cursor** 是 STREAM 内的位置或时间游标，语义由 stream profile 定义。它可以表示 byte offset、sample cursor、frame timestamp 等。

### op

**op** 是 RPC Operation Code，例如 Hello、Identify、Identified、Event、Request、RequestResponse。

`op` 决定 RPC Envelope 中 `d` 的结构，不等同于 methodId。

---

## 5. Registry 与业务建模术语

### Registry

**Registry** 是 AXTP 的机器事实源集合，定义 domain、method、event、error、schema、capability、profile 等稳定事实。

Runtime 实现应消费 registry 生成的 Protocol IR / generated reference，不应从草案文档里手写并行协议常量。

### Domain

**Domain** 是业务或协议能力的大类，例如 `audio`、`video`、`system`、`network`、`firmware`、`capability`。

Domain 通常对应 methodId/eventId/errorCode 的高字节分段，也对应业务团队和文档组织边界。

### Feature

**Feature** 是 domain 下的一块能力边界，例如 `audio.algorithm`、`system.lifecycle`、`network.wifi`。

Feature 不是字段名，不应用 `Config`、`State`、`Scan` 这类过泛词直接命名。

### Capability

**Capability** 是设备在当前固件、会话和鉴权状态下声明支持的一组能力。AXTP 通常使用 `domain.feature` 作为 capability ID。

Capability 描述“支持什么”，Method 描述“怎么调用”，Event 描述“怎么通知变化”。

### Method

**Method** 是客户端通过 RPC Request 调用的业务操作，例如 `audio.getAlgorithmConfig`。

Method 有稳定 method name 和 methodId。methodId 出现在 binary RPC 中，不出现在 Frame Header 中。

### Event

**Event** 是 Logical Server 通过 RPC Event 推送给 Logical Client 的低频状态或变化通知，例如 `audio.algorithmConfigChanged`。

Event 通过 `PayloadType=RPC` 且 `rpcOp=EVENT` 承载，不通过 STREAM 承载。

### ErrorCode

**ErrorCode** 是统一错误码，供 RPC Response、CONTROL statusCode/NACK 和 STREAM 错误映射使用。

不要在单个 feature 文档里发明局部错误码；应复用或采纳到 ErrorCode Registry。

### Schema

**Schema** 定义 method params/result、event data、capability、profile 等结构化对象的字段类型、fieldId、必填性和兼容规则。

### Profile

**Profile** 是某类运行时能力或数据流的档案，例如 transport profile、stream profile、implementation profile。

Profile 不是 runtime 包版本，也不是具体设备型号。

### Protocol IR

**Protocol IR** 是 Generator 聚合 registry 后输出的机器可读协议模型，当前路径为 `contract/protocol/axtp.protocol.yaml`。

Protocol IR 是生成产物，不应手写修改。

### Generated Reference

**Generated Reference** 是 Generator 输出的人读/机读协议参考，例如 `contract/generated/protocol.md` 和 `contract/generated/protocol.json`。

Generated reference 是实现合同的一部分，但应由 registry / generator 生成，不应手写。

---

## 6. 编码与数据格式术语

### JSON

**JSON** 是默认 RPC 对象编码，直接承载 `{sid, op, d}` envelope。WebSocket JSON 路径使用 JSON。

### CBOR

**CBOR** 是后续紧凑对象编码之一，仍编码同一个 `{sid, op, d}` 语义对象。

### MSGPACK

**MSGPACK** 是后续紧凑对象编码之一，仍编码同一个 `{sid, op, d}` 语义对象。

### JSON_BINARY

**JSON_BINARY** 是 AXTP RPC 的固定二进制 envelope 编码，使用 15B RPC header 表达 `sid/op/requestId/methodOrEventId/statusCode/bodyEncoding`，后接 body。

`JSON_BINARY` 是规范中的 canonical name；不要写成 “Binary JSON” 作为正式枚举名。

### Binary JSON

**Binary JSON** 是容易产生歧义的口语化说法。AXTP 正式术语是 `JSON_BINARY`，表示 RPC encoding，不表示把 JSON 文本压缩成某种通用二进制 JSON 格式。

### TLV

**TLV**（Type-Length-Value）是二进制字段编码方式。AXTP 中常见 `TLV8` / `TLV16` 用于 JSON_BINARY body。

TLV 不是新的 PayloadType，也不是新的 RPC encoding。

### TLV8

**TLV8** 是 `fieldId:uint8 + length:uint8 + value` 的短字段编码。Phase 1 JSON_BINARY body 必须实现 `NONE` 和 `TLV8`。

### TLV16

**TLV16** 是支持更长字段长度的 TLV 扩展形式，属于后续扩展。

### rpcEncoding

**rpcEncoding** 表示 RPC payload 使用哪种编码，例如 JSON、CBOR、MSGPACK、JSON_BINARY。

rpcEncoding 不等同于 PayloadType；RPC payload 一定已经处在 `PayloadType=RPC` 内。

### bodyEncoding

**bodyEncoding** 是 JSON_BINARY RPC Header 中的字段，表示 body 如何编码，例如 NONE、TLV8、TLV16。

bodyEncoding 只在 `rpcEncoding=JSON_BINARY` 时有意义。

---

## 7. 角色与方向术语

### Physical Client

**Physical Client** 是底层 transport 主动发起连接的一端。

它不一定是业务上的 client。例如设备主动连接云端时，设备是 Physical Client，但仍可能是 Logical Server。

### Physical Server

**Physical Server** 是底层 transport 被连接的一端。

### Logical Client

**Logical Client** 是调用 method、订阅 event、消费 stream 的一端，通常是 App、SDK、云端控制台或测试工具。

### Logical Server

**Logical Server** 是暴露 methods/events/streams 的一端，通常是设备、mock server 或设备侧 gateway。

RPC Hello 永远由 Logical Server 发送，不按 TCP accept/connect 判断。

---

## 8. 文档治理术语

### Business Input

**Business Input** 是 `workspace/business/**` 下的产品需求、PRD、现场反馈或用户场景。它是评审输入，不是 runtime 实现合同。

### Flow

**Flow** 是 `workspace/flows/**` 下的场景级交互流，用于描述多角色、多消息的业务流程。它是草案输入，不是最终协议事实源。

### Protocol Draft

**Protocol Draft** 是 `workspace/protocol/**` 下的 domain.feature 草案，用于评审候选 method/event/schema/error/capability/profile。

未采纳 draft 不得作为 runtime 正式实现合同。

### Adopted

**Adopted** 表示草案已被评审采纳，并写入 `contract/registry/**` 或 `contract/registry/domains/**`，随后由 Generator 生成 Protocol IR 和 generated reference。

### Spec Tag

**Spec Tag** 是可发布、可锁定的 AXTP spec 版本 tag，格式为 `spec/vMAJOR.MINOR.PATCH`。

Runtime release 应绑定 spec tag、明确 commit 或 release artifact，不应依赖浮动 main。

### Roadmap Milestone

**Roadmap Milestone** 是规划阶段或功能完成度标识，例如 `v0.1`、`v0.2`、`v1.0`。它不是 runtime 可绑定的 spec release 版本。

---

## 9. 常见误解速查

| 容易混淆 | 正确理解 |
|---|---|
| Session = TCP 连接 | 错。请区分 Transport connection、Framed Link Context、RPC Session、Stream Context。 |
| sid = 用户登录 token | 错。sid 是 RPC Session ID，JSON 中是固定 8 位 hex string。 |
| PayloadType 可以加 VIDEO / OTA / FILE | 错。PayloadType 只允许 CONTROL / RPC / STREAM；业务类型进入 contract/registry/profile。 |
| Event 可以承载视频帧 | 错。Event 是低频状态通知；连续数据走 STREAM。 |
| STREAM Header 里写 codec/fileType | 错。STREAM Header 只含 streamId/seqId/cursor；业务参数由 RPC 建流上下文绑定。 |
| JSON_BINARY = 二进制 JSON 标准格式 | 错。JSON_BINARY 是 AXTP RPC fixed binary envelope 的 canonical name。 |
| methodId 可以复用旧 CmdValue | 错。旧 CmdValue 必须通过 legacy mapping 映射到 AXTP MethodId。 |
| workspace/protocol 草案就是实现合同 | 错。采纳并 generated 后才是 runtime 可依赖合同。 |
| generated 文档可以手写修 | 错。应修改 contract/registry/spec/generator 后重新生成。 |
