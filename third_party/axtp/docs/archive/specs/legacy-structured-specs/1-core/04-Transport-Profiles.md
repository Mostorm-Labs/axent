# 1-core/04《AXTP Transport Profile 规范》

> 状态： 规范性 runtime 实现规范
> 规范版本： 1.0.0-rc1
> 变更策略： v1.0.0 前仅允许澄清性修改
> 本文的规范范围：AXTP 如何运行在不同 transport profile 上，以及启动方向、物理/逻辑角色、framed/unframed 生产路径。
> 本文不定义：Standard Frame 字段、CONTROL opcode/TLV 字段、RPC op 字段、STREAM Header 字段、registry 条目或业务 method。
> Runtime 实现状态：支持对应生产 profile 时必需实现；低带宽 profile 属于可选/未来能力。

版本：v1.0.0-rc1
状态：AXTP v1 Core 冻结候选
适用范围：AXTP 当前正式连接形态
前置文档：[`02-Protocol-Framework.md`](02-Protocol-Framework.md)、[`03-Frame-and-Payload.md`](03-Frame-and-Payload.md)
后续文档：[`05-Control-Session.md`](05-Control-Session.md)、[`06-RPC-Session.md`](06-RPC-Session.md)、[`07-Stream-Data-Plane.md`](07-Stream-Data-Plane.md)、[`08-Low-Bandwidth-Degradation.md`](08-Low-Bandwidth-Degradation.md)

---

## 文档目的

本文定义 AXTP runtime 如何选择和执行 transport profile。它只描述 transport 如何承载 AXTP，不重复 frame 字段、RPC 字段或 STREAM 字段定义。

## 范围

本文覆盖：

- Standard Framed Binary 与 WebSocket Unframed JSON 两条生产路径；
- USB HID、TCP、WebSocket JSON、Cloud Reverse 的角色和启动流程；
- Physical Client/Server 与 Logical Client/Server 的区别；
- HID-64、BLE、UART 等低带宽 profile 的边界。

本文不覆盖：

- 12B Standard Frame Header 的字段布局；
- 5B CONTROL Payload Header；
- RPC `sid/op/d` 字段规则；
- STREAM 16B Header；
- Compact Frame 的完整 wire format。

## v1 必需实现

AXTP v1 Core 定义两条生产路径：

```text
Standard Framed Binary
  Transport: AXTP-USB-HID / AXTP-TCP
  Frame:     STANDARD_FRAME
  Payload:   CONTROL / RPC / STREAM
  RPC:       JSON / CBOR / MSGPACK / JSON_BINARY

WebSocket Unframed JSON
  Transport: AXTP-WS-JSON / AXTP-WS-CLOUD-REVERSE
  Frame:     none
  Payload:   RPC JSON envelope only
  RPC:       JSON
```

当前 transport profile：

| Transport Profile | 路径 | Frame Profile | RPC Encoding | CONTROL | STREAM | 典型用途 |
|---|---|---|---|---:|---:|---|
| `AXTP-USB-HID` | Standard Framed Binary | `STANDARD_FRAME` | JSON / CBOR / MSGPACK / JSON_BINARY | 是 | 是 | USB HID High Speed 或大 report 设备 |
| `AXTP-TCP` | Standard Framed Binary | `STANDARD_FRAME` | JSON / CBOR / MSGPACK / JSON_BINARY | 是 | 是 | PC/App 到设备的直接连接 |
| `AXTP-WS-JSON` | WebSocket Unframed JSON | 无 | JSON | 否 | 否 | Browser、cloud、轻量 RPC |
| `AXTP-WS-CLOUD-REVERSE` | WebSocket Unframed JSON | 无 | JSON | 否 | 否 | 设备主动连接 cloud |

Runtime MUST NOT 在同一个 transport connection 内混用 Standard Framed Binary 和 WebSocket Unframed JSON。

## v1 可选 / Profile 特定

Profile 特定行为：

| Profile family | 状态 | 说明 |
|---|---|---|
| `AXTP-USB-HID` | 支持 USB HID 时必需 | 在产品 profile 选择的 HID report/bulk carrier 上使用 Standard Frame。 |
| `AXTP-TCP` | 支持 TCP 时必需 | 在 byte stream 上使用 Standard Frame；parser SHOULD 通过 Magic `AX` 重新同步。 |
| `AXTP-WS-JSON` | 支持 WebSocket JSON 时必需 | 直接使用 JSON RPC envelope；仅 RPC。 |
| `AXTP-WS-CLOUD-REVERSE` | 支持 cloud reverse 时必需 | 设备是 Physical Client，但仍保持 Logical Server。 |
| HID-64 / BLE / UART | Optional/Future | profile-specific 低带宽降级；见 [`08-Low-Bandwidth-Degradation.md`](08-Low-Bandwidth-Degradation.md)。 |

WebSocket Unframed JSON 是生产可用的 RPC-only 路径，不是仅用于调试的路径。它 MUST NOT 承载 CONTROL、STREAM、CRC16、Standard Frame Header 或 JSON_BINARY RPC fixed header。

## 保留 / 未来

以下内容属于 RESERVED/FUTURE，MUST NOT 成为 v1 Core runtime conformance 的必需项：

- 将 Compact Frame 作为默认 transport；
- 将 HID-64 / BLE / UART 作为必需 profile；
- runtime 在 Standard Frame 与 Compact Frame 之间切换；
- WebSocket Unframed JSON 承载 STREAM payload；
- WebSocket Unframed JSON 上的 CONTROL ACK/NACK/RESUME。

低带宽 profile 只有在作为独立 transport/profile 被选择时，MAY 改变外层 frame header、MTU、fragmentation 和 retry policy。它们 MUST NOT 改变 MethodId、EventId、ErrorCode、PayloadType、CONTROL/RPC/STREAM payload header 或 STREAM 16B Header。

## 规范规则

- Transport Profile MUST 在 AXTP session 启动前选定。
- Transport Profile MUST 决定是否存在 frame header。
- Standard Framed profile MUST 在 RPC Hello / Identify / Identified 前运行 CONTROL OPEN / ACCEPT。
- WebSocket Unframed JSON profile MUST 在 WebSocket connection 建立后以 RPC Hello / Identify / Identified 启动。
- WebSocket Unframed JSON MUST 是 RPC-only。
- Hello MUST 由 Logical Server 发送，不一定由 Physical Server 发送。
- 在 Standard Framed profile 中，CONTROL OPEN 遵循物理连接方向。
- RPC Identify / Identified 完成后，业务 Request 可由任一端作为 requester 发起；method 的可调用端由对应 domain.feature、capability 或 role policy 约束，不由 Physical Client/Server 方向单独决定。
- cloud reverse 模式下设备 MAY 是 Physical Client；如果它是 Logical Server，则仍然 MUST 发送 Hello。
- transport 实现 MUST NOT 仅根据 transport profile 推断业务 capability；业务 capability 由 contract/registry/RPC 表达。

## 状态机 / 生命周期

### Standard Framed Binary

```text
Transport connected
  -> Physical Client 发送 CONTROL OPEN
  -> Physical Server 发送 CONTROL ACCEPT 或失败 ACCEPT
  -> FRAMING_READY
  -> Logical Server sends RPC Hello
  -> Logical Client sends RPC Identify(randomSeed)
  -> Logical Server sends RPC Identified
  -> APP_READY
```

### WebSocket Unframed JSON

```text
WebSocket connected
  -> Logical Server 发送 RPC Hello
  -> Logical Client 发送 RPC Identify(randomSeed)
  -> Logical Server 发送 RPC Identified
  -> APP_READY
```

### Cloud Reverse

```text
Device 连接到 Cloud WebSocket endpoint
  -> Device 是 Physical Client
  -> Cloud 是 Physical Server
  -> Device 仍然是 Logical Server
  -> Device 发送 RPC Hello
```

角色模型：

| 角色 | 含义 | 职责 |
|---|---|---|
| Physical Client | transport connection 发起方 | 连接 transport；在 Standard Framed 中发送 CONTROL OPEN |
| Physical Server | transport 监听/接受方 | 接受连接；在 Standard Framed 中返回 CONTROL ACCEPT |
| Logical Client | RPC session 识别发起方 | 发送 Identify/Reidentify；APP_READY 后可按 feature/capability/role policy 发起允许的业务 Request |
| Logical Server | RPC session 提供方 | 发送 Hello、校验 Identify、分配 sid、提供 methods/events/streams；APP_READY 后也可按 feature/capability/role policy 发起允许的业务 Request |

默认角色映射：

| Transport Profile | Physical Client | Physical Server | Logical Client | Logical Server | Hello 发送方 |
|---|---|---|---|---|---|
| `AXTP-USB-HID` | Host / App | USB HID Device | Host / App | Device | Device |
| `AXTP-TCP` | App / PC | Device | App / PC | Device | Device |
| `AXTP-WS-JSON` | App / Cloud | Device / Gateway | App / Cloud | Device | Device |
| `AXTP-WS-CLOUD-REVERSE` | Device | Cloud | Cloud | Device | Device |

## 校验规则

Runtime MUST 校验：

- 所选 Transport Profile 被本地 runtime 和 connection entrypoint 同时支持；
- Standard Framed profile 在解析 CONTROL/RPC/STREAM 前收到有效 Standard Frame；
- WebSocket JSON message 在进入 RPC session 处理前是有效 JSON RPC envelope；
- WebSocket Unframed JSON message 不包含 Standard Frame bytes；
- Standard Framed 启动时先收到 CONTROL OPEN，再处理 RPC/STREAM payload；
- WebSocket Unframed JSON 不接收 CONTROL 或 STREAM payload；
- 逻辑角色方向被一致应用，尤其是 cloud reverse。

## Runtime 实现要求

- 实现 `AXTP-USB-HID` 或 `AXTP-TCP` 的 runtime MUST 实现 Standard Frame parsing 和 CONTROL/RPC startup。
- 实现 `AXTP-WS-JSON` 或 `AXTP-WS-CLOUD-REVERSE` 的 runtime MUST 实现 RPC JSON envelope startup，且 MUST NOT 要求 CONTROL。
- Standard Framed runtime SHOULD 支持 RPC JSON 用于诊断，并 MAY 根据 profile negotiation 支持 JSON_BINARY 高吞吐路径。
- WebSocket JSON runtime MUST 等待 Server Hello 后再发送 Identify。
- Runtime code SHOULD 将物理 transport role 与逻辑 AXTP role 作为独立状态变量维护。
- 低带宽 runtime MUST 声明独立 profile/entrypoint，且 MUST NOT 静默把 Standard Framed connection 解释为 Compact。

## 示例

Standard Framed 启动：

```text
PC -> PS: CONTROL OPEN
PS -> PC: CONTROL ACCEPT
LS -> LC: RPC Hello
LC -> LS: RPC Identify(randomSeed)
LS -> LC: RPC Identified
```

WebSocket JSON 启动：

```text
LS -> LC: { "sid": "", "op": 0, "d": { "axtpVersion": "1.0.0-rc1" } }
LC -> LS: { "sid": "", "op": 2, "d": { "randomSeed": 305419896 } }
LS -> LC: { "sid": "12345678", "op": 3, "d": {} }
```

Cloud reverse 角色提醒：

```text
Device 向 Cloud 打开 WebSocket。
Device 仍然发送 Hello，因为 Device 是 Logical Server。
```

## 非目标

- 本文不定义任何 frame 字段布局。
- 本文不定义 CONTROL OPEN/ACCEPT TLV 字段。
- 本文不定义启动顺序之外的 RPC op 字段行为。
- 本文不定义业务 method 可用性。
- 本文不把 BLE/UART/HID-64 作为 v1 Core 必需行为。
