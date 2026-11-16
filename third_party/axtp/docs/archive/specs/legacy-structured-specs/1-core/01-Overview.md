# 1-core/01《AXTP 总览》

> 状态：总览
> 范围：AXTP Core 的一页式导读
> 权威边界：本文提供协议整体速览；详细规则由 `02-Protocol-Framework.md` 到 `07-Stream-Data-Plane.md` 维护。

版本：v2.1
状态：总览
适用范围：AXTP 协议体系一页总览

---

## 1. AXTP 是什么

AXTP（Auditoryworks Transport Protocol）是一套面向设备、runtime、SDK 和测试工具的统一通信协议规范。它把同一套业务 method / event / error / schema 放到两条正式连接路径上：

| 路径 | 典型传输 | 线上结构 | 能力 |
|---|---|---|---|
| Standard Framed | `AXTP-USB-HID`、`AXTP-TCP` | `Standard Frame Header(12B) + Payload(N) + CRC16(2B)` | CONTROL / RPC / STREAM |
| WebSocket Unframed JSON | `AXTP-WS-JSON`、`AXTP-WS-CLOUD-REVERSE` | `WebSocket message payload = JSON { sid, op, d }` | RPC-only |

Standard Framed 路径先建立 CONTROL 链路上下文，再建立 RPC Session，并可通过 STREAM 承载连续数据。WebSocket Unframed JSON 路径没有 Frame Header、CONTROL、STREAM 和 CRC16，只承载 JSON RPC Envelope。

## 2. AXTP 分成哪几层

AXTP 的核心分层如下，详细职责边界见 [`02-Protocol-Framework.md`](02-Protocol-Framework.md)。

```text
Business Layer    device / system / audio / video / firmware / ...
Registry Layer    method / event / error / capability / schema / profile
Payload Layer     CONTROL(0x01) / RPC(0x02) / STREAM(0x03)
Frame Layer       Standard Frame Header / length / fragment / CRC
Transport Layer   USB HID / TCP / WebSocket
```

关键原则只有一个：底层只解决传输和解析边界，业务语义必须进入 Registry 和 Business Layer。Frame Header 与 PayloadType 不表达 `VIDEO`、`FIRMWARE_UPDATE`、`FILE` 这类业务类型。

## 3. 三类 Payload

| PayloadType | 值 | 职责 | 详见 |
|---|---:|---|---|
| CONTROL | `0x01` | 协议运行时控制，例如 OPEN / ACCEPT / HEARTBEAT / CLOSE | [`05-Control-Session.md`](05-Control-Session.md) |
| RPC | `0x02` | 业务控制面，例如 Hello / Identify / Request / Response / Event | [`06-RPC-Session.md`](06-RPC-Session.md) |
| STREAM | `0x03` | 连续数据面，以 `streamId` 绑定业务 stream context | [`07-Stream-Data-Plane.md`](07-Stream-Data-Plane.md) |

PayloadType 只选择一级 parser，不表达具体业务。

## 4. 连接过程概览

Standard Framed 路径：

```text
Transport connected
CONTROL OPEN
CONTROL ACCEPT
RPC Hello
RPC Identify(randomSeed)
RPC Identified
RPC Request / Response / Event
STREAM data，在业务 method 已建立 stream context 后使用
```

WebSocket Unframed JSON 路径：

```text
WebSocket connected
RPC Hello
RPC Identify(randomSeed)
RPC Identified
RPC Request / Response / Event
```

完整 transport 行为见 [`04-Transport-Profiles.md`](04-Transport-Profiles.md)。RPC session 字段和状态机以 [`06-RPC-Session.md`](06-RPC-Session.md) 为唯一权威来源。

## 5. 事实源模型

AXTP 使用 registry-first 的事实源模型：

```text
contract/registry/**/*.yaml + contract/registry/domains/**/*.yaml
        -> contract/protocol/axtp.protocol.yaml
        -> contract/generated/protocol.md / protocol.json
        -> runtime metadata / test inputs
```

边界如下：

| 位置 | 职责 |
|---|---|
| `specs/**` | 人工维护的正式设计规范 |
| `contract/registry/**/*.yaml`、`contract/registry/domains/**/*.yaml` | 机器可读协议事实源 |
| `contract/protocol/axtp.protocol.yaml` | Generator 输出的 Protocol IR |
| `contract/generated/**` | Generator 输出的协议参考 |
| `workspace/protocol/**` | 评审中的协议草案 |

如果当前 method / event / error / capability 清单与 specs 叙述冲突，以 registry YAML 和 generated 输出为实现合同，并回修 specs。

## 6. AXTP 解决什么问题

- 多 transport 共享同一套业务 registry。
- Frame Header 保持稳定，不被业务类型污染。
- CONTROL / RPC / STREAM 分离协议运行时控制、业务控制面和连续数据面。
- JSON、CBOR、MSGPACK、JSON_BINARY 是同一套 RPC 语义的不同编码路径。
- runtime、generator、conformance 可以围绕同一份机器事实源协同。

## 7. 不在本页讲完整的内容

本页只是一页总览。以下内容由其他文档作为权威来源：

| 内容 | 权威文档 |
|---|---|
| 设计原则和分层架构 | [`02-Protocol-Framework.md`](02-Protocol-Framework.md) |
| Standard Frame 线上格式 | [`03-Frame-and-Payload.md`](03-Frame-and-Payload.md) |
| Transport profile 差异 | [`04-Transport-Profiles.md`](04-Transport-Profiles.md) |
| CONTROL 会话 | [`05-Control-Session.md`](05-Control-Session.md) |
| RPC 会话 | [`06-RPC-Session.md`](06-RPC-Session.md) |
| STREAM 数据面 | [`07-Stream-Data-Plane.md`](07-Stream-Data-Plane.md) |
| 低带宽降级 | [`08-Low-Bandwidth-Degradation.md`](08-Low-Bandwidth-Degradation.md)，补充性 / future profile |

Legacy 迁移、roadmap、长业务示例和 generated 大表不属于本页的规范正文。
