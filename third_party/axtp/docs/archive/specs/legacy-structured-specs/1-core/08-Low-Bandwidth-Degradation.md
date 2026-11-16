# 1-core/08《AXTP 低带宽降级规范》

> 状态： 补充性 / profile 特定规范
> 规范版本： 0.2.0
> 变更策略： 在被稳定 transport profile 采纳前保持实验状态
> 本文的规范范围：显式选择低带宽 profile 时的约束。
> 本文不定义：v1 Core Standard Frame、默认 TCP/USB-HID 行为、WebSocket JSON 行为、新 PayloadType 值、CONTROL/RPC/STREAM payload 语义或 registry 业务 API。
> Runtime 实现状态：可选/未来能力；v1 Core runtime conformance 不要求实现。

版本：0.2.0
状态：补充规范
适用范围：HID-64、BLE、UART 或小 MTU 链路的降级 profile 设计

---

## 文档目的

本文定义低带宽、小 MTU 或资源受限 transport 如何在独立 profile 中降级承载 AXTP。降级只允许改变外层承载策略，例如 outer frame header、MTU、fragmentation、retry policy；不得改变 AXTP CONTROL / RPC / STREAM 的 Payload 语义。

```text
MAY change:     outer frame header / MTU / fragmentation / retry policy
MUST NOT change: PayloadType / CONTROL 5B / RPC Binary 15B / STREAM 16B
                 methodId / eventId / errorCode / business registry
```

## 范围

本文覆盖：

- HID-64 / BLE / UART / small-MTU profile 选择；
- Compact Frame 候选结构；
- frame size 与 fragmentation 约束；
- 低带宽 encoding 约束；
- fallback 与兼容性要求。

本文不覆盖：

- Standard Framed Binary 12B Header；
- WebSocket Unframed JSON；
- 具体业务 method/event/schema；
- 低带宽 profile 的 stable registry 条目；
- generated conformance case。

## v1 必需实现

AXTP v1 Core 不要求实现任何低带宽降级 profile。

v1 Core 必需路径仍然是：

| 路径 | 必需行为 |
|---|---|
| Standard Framed Binary | `AXTP-USB-HID` / `AXTP-TCP` 使用 Standard Frame |
| WebSocket Unframed JSON | `AXTP-WS-JSON` / `AXTP-WS-CLOUD-REVERSE` 只使用 JSON RPC envelope |

runtime 即使不实现 Compact Frame、HID-64、BLE 或 UART 降级，也 MAY 完全符合 v1 Core。

## v1 可选 / Profile 特定

低带宽 profile MAY 在 AXTP session 启动前被选择并引入：

| Transport/profile family | 状态 | 约束 |
|---|---|---|
| HID-64 | Optional/Future | MUST 计算 report size 和 STREAM 16B header 开销 |
| BLE GATT | Optional/Future | MUST 协商 MTU；默认 ATT MTU 通常太小，不适合 STREAM |
| UART | Optional/Future | MUST 提供显式 frame boundary，例如 COBS/SLIP/length-prefix |
| Compact Frame | Experimental/Future | MUST 由独立 profile/entrypoint 选择 |

选择点 MAY 包括：

| Transport | 选择点 |
|---|---|
| USB HID | 独立 interface、report descriptor、endpoint 或 device descriptor |
| BLE GATT | 独立 service UUID / characteristic set / profile UUID |
| UART | 独立 port configuration、startup adapter 或 out-of-band configuration |
| TCP | 独立 port、ALPN、URL path 或 connection discovery |
| WebSocket | SHOULD 继续使用 Unframed JSON 或明确声明的 binary subprotocol |

## 保留 / 未来

以下内容 MUST 保持为 future/profile-specific：

- stable Compact Frame registry 条目；
- Compact CRC8 polynomial 和 test vector；
- HID-64/BLE/UART conformance manifest；
- 低带宽 stop-and-wait 或 sliding-window reliability 要求；
- runtime 在 Standard Frame 与 Compact Frame 之间切换；
- 将 Compact Frame 变成 v1 Core 默认行为。

低带宽 profile MUST NOT 反向修改 [`03-Frame-and-Payload.md`](03-Frame-and-Payload.md)。

## 规范规则

- 低带宽 profile MUST 在 AXTP session 启动前选定。
- runtime MUST NOT 在同一个 AXTP Session 内切换 Frame Profile。
- OPEN / ACCEPT MUST NOT 将既有 Standard connection 转换成 Compact connection。
- 低带宽 profile MUST 保留 PayloadType 值：CONTROL=`0x01`、RPC=`0x02`、STREAM=`0x03`。
- 低带宽 profile MUST 保留 CONTROL 5B Payload Header。
- 低带宽 profile MUST 保留 JSON_BINARY 15B fixed header。
- 低带宽 profile MUST 保留 STREAM 16B Header。
- 低带宽 profile MUST 保留 MethodId、EventId、ErrorCode、schema、capability 和 profile registry 语义。
- 低带宽 profile MUST NOT 创建重复业务 API。
- 如果超过 encoded Compact 限制，runtime MUST 让 encoding 失败或 fallback 到兼容 profile；MUST NOT 截断值。

## 状态机 / 生命周期

Profile 选择生命周期：

```text
选择 low-bandwidth Transport/Profile entrypoint
  -> 解析被选择的 outer L1 profile，例如 COMPACT_FRAME
  -> CONTROL OPEN
  -> CONTROL ACCEPT or rejection
  -> RPC Hello / Identify / Identified
  -> APP_READY
```

Fallback 生命周期：

```text
尝试 low-bandwidth profile
  -> peer/profile 不支持
  -> 安全关闭或拒绝 connection
  -> 可用时 fallback 到 Standard Framed 或 WebSocket Unframed JSON
```

兼容规则：

```text
旧 v1 Core runtime 只需要 Standard Framed 和 WebSocket JSON。
Compact 或 BLE/UART profiles 使用独立 entrypoints，MUST NOT 出现在同一个 wire stream 内。
```

## 校验规则

低带宽 encoder 发送前 MUST 校验 profile 限制：

- payload length 符合所选 outer frame profile；
- message id 符合所选 wire width；
- frame index 和 frame count 符合所选 wire width；
- 扣除 outer header 和 L2 payload header 后剩余 data capacity 为正；
- STREAM data capacity 计算了 16B STREAM Header；
- CONTROL/RPC/STREAM payload header 保持不变；
- 所选 profile 标记 stable 前已定义 CRC/checksum configuration。

Compact Frame 候选校验：

| 字段 | 限制 |
|---|---:|
| `PayloadLength` | `<= 0xFF` |
| `MessageId` | Compact wire 上 `<= 0xFF` |
| `frameIndex` | `<= 14` |
| `frameCount` | `1..15` |

如果超过任何限制，runtime MUST 让 encoding 失败或选择其他 profile。Runtime MUST NOT 回绕或截断这些值。

## Runtime 实现要求

### Compact Frame 候选

Compact Frame 是实验性/未来低带宽候选方案：

```text
Offset 0: VT(1)
Offset 1: PayloadLength(1)
Offset 2: MessageId(1)
Offset 3: FrameInfo(1)
Offset 4: Payload starts
Footer:   CRC8(1)
```

| 字段 | 长度 | 规则 |
|---|---:|---|
| `VT` | 1B | 高 4 bit = Compact Header Version；低 4 bit = PayloadType |
| `PayloadLength` | 1B | Payload 字节数，不包含 Compact Header 和 CRC8 |
| `MessageId` | 1B | Compact wire 的 message id |
| `FrameInfo` | 1B | 高 4 bit = frameIndex；低 4 bit = frameCount |
| `CRC8` | 1B | 覆盖 Compact Header(4B) + Payload |

推荐候选编码：

```text
VT.version = 1
VT.payloadType = CONTROL(1) / RPC(2) / STREAM(3)
FrameInfo.frameIndex = 0..14
FrameInfo.frameCount = 1..15
```

Compact Frame 只替换外层 L1 Frame Header：

| PayloadType | Compact 下的 Payload |
|---|---|
| CONTROL | 相同 5B Control Payload Header + TLV body |
| RPC | 相同 JSON envelope 或 15B JSON_BINARY Header + body |
| STREAM | 相同 16B STREAM Header + data |

Compact profile 标记为 stable 前，MUST 明确 CRC8 polynomial、init value 和 test vectors。

### HID-64 / BLE / UART 容量

实现 SHOULD 显式计算可用 data capacity：

```text
usablePayload = transportPacketSize
  - transportSpecificOverhead
  - lowBandwidthFrameHeader
  - checksumFooter
  - payloadSpecificHeader
```

对于 STREAM，`payloadSpecificHeader` 包含 16B STREAM Header。

## 示例

HID-64 容量示例：

```text
ReportId: 1B, optional
Compact Header: 4B
CRC8 Footer: 1B
Frame Payload capacity: 58B  // 64B report with 1B ReportId
STREAM data capacity: 42B    // 58B - 16B STREAM Header
```

决策指引：

| 场景 | 建议 |
|---|---|
| USB HID high-speed 或 TCP | 使用 Standard Framed |
| Browser/cloud RPC | 使用 WebSocket Unframed JSON |
| BLE/UART/HID-64 small MTU | 仅在需要时使用独立低带宽 profile |
| video/firmware/file 连续数据 | 优先使用 Standard Framed，除非低带宽 profile 能证明容量/可靠性足够 |

## 非目标

- 本文不使 Compact Frame 成为 stable。
- 本文不要求 v1 Core 实现 HID-64、BLE 或 UART。
- 本文不修改 Standard Framed Binary。
- 本文不新增 PayloadType 值。
- 本文不定义新的业务 method、event、error、schema、capability 或 stream profile。
- 本文不定义 generated artifact 或 conformance manifest。
