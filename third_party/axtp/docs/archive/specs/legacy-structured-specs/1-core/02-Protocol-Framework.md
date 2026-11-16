# 1-core/02《AXTP 协议框架》

> 状态： 规范性架构说明
> 规范版本： 1.0.0-rc1
> 变更策略： v1.0.0 前仅允许澄清性修改
> 范围：AXTP Core 的设计原则、分层架构和职责边界
> 权威边界：本文定义 AXTP 各层之间的关系；具体 wire format、字段和状态机由后续 core specs 维护。

版本：v1.0.0-rc1
状态：AXTP v1 Core 冻结候选
适用范围：AXTP v1 Core 设计原则、分层架构与职责边界

---

## 1. 文档目的

本文定义 AXTP v1 Core 的设计原则和分层架构。它不重复后续文档中的字段表、完整状态机或 registry 当前清单。

权威边界：

| 内容 | 权威文档 |
|---|---|
| Frame Header、PayloadType、CRC、fragment | [`03-Frame-and-Payload.md`](03-Frame-and-Payload.md) |
| Transport Profile 行为 | [`04-Transport-Profiles.md`](04-Transport-Profiles.md) |
| CONTROL payload 与链路状态 | [`05-Control-Session.md`](05-Control-Session.md) |
| RPC envelope、Hello / Identify / Identified、Request / Response / Event | [`06-RPC-Session.md`](06-RPC-Session.md) |
| STREAM 16B Header 与数据面生命周期 | [`07-Stream-Data-Plane.md`](07-Stream-Data-Plane.md) |
| 低带宽降级 profile | [`08-Low-Bandwidth-Degradation.md`](08-Low-Bandwidth-Degradation.md)，补充性 / future |

## 2. 设计原则

### 2.1 Transport Profile 决定线上 envelope

AXTP 不在运行时协商 Frame Header 形态。是否使用 Standard Frame 由 Transport Profile 固定决定：

| Transport Profile | 线上 envelope |
|---|---|
| `AXTP-USB-HID` | Standard Framed |
| `AXTP-TCP` | Standard Framed |
| `AXTP-WS-JSON` | WebSocket Unframed JSON |
| `AXTP-WS-CLOUD-REVERSE` | WebSocket Unframed JSON |

同一个 AXTP Session 内不切换 Frame 形态。

### 2.2 PayloadType 是 parser 选择器，不是业务类型

AXTP v1 Core 只定义三类顶层 PayloadType：

| PayloadType | 职责 |
|---|---|
| CONTROL | 协议运行时控制 |
| RPC | 业务控制面 |
| STREAM | 连续数据面 |

PayloadType 不表达 `VIDEO`、`AUDIO`、`FIRMWARE_UPDATE`、`FILE` 等业务语义。业务类型必须通过 method / event / schema / profile / stream context 表达。

### 2.3 CONTROL、RPC 与 STREAM 职责分离

| Payload | 负责 | 不负责 |
|---|---|---|
| CONTROL | OPEN / ACCEPT、心跳、关闭、协议运行时控制 | 业务 method、业务 event、连续数据 |
| RPC | Hello / Identify / Identified、Request / Response / Event、业务控制命令 | 大块连续数据传输 |
| STREAM | 已建立 stream context 后的数据块 | stream 业务语义的声明与协商 |

STREAM 的业务含义来自 RPC 建立的 stream context，而不是 STREAM Header。

### 2.4 Registry 是业务合同

具体 method / event / error / capability / schema / profile 的当前事实来自：

```text
contract/registry/**/*.yaml
contract/registry/domains/**/*.yaml
```

Generator 将这些事实聚合为：

```text
contract/protocol/axtp.protocol.yaml
contract/generated/protocol.md
contract/generated/protocol.json
```

Specs 定义规则和边界；generated 输出定义当前清单。草案、legacy 证据和 roadmap 不直接成为 runtime 实现合同。

### 2.5 扩展必须保持 core 边界

扩展 AXTP 时应优先使用以下机制：

- 新增 method / event / schema / error / capability / profile；
- 新增 transport profile；
- 新增 CONTROL TLV 或保留范围内的 CONTROL opcode；
- 新增 stream profile。

扩展不得重新定义已有 Frame Header 字段、PayloadType 语义、RPC envelope 字段或 STREAM Header 字段。

## 3. 分层架构

```text
+--------------------------------------------------+
| Business Layer                                   |
| device / system / audio / video / firmware / ... |
+--------------------------------------------------+
| Registry Layer                                   |
| Method / Event / Error / Capability / Schema     |
+--------------------------------------------------+
| Payload Layer                                    |
| CONTROL / RPC / STREAM                           |
+--------------------------------------------------+
| Frame Layer                                      |
| Frame Profile / PayloadType / Fragment / CRC     |
+--------------------------------------------------+
| Transport Layer                                  |
| USB HID / TCP / WebSocket                        |
+--------------------------------------------------+
```

| 层级 | 职责 | 不应承担 |
|---|---|---|
| Transport | 建立连接、承载字节或 WebSocket message、处理 transport 级 MTU | 解释 methodId、eventId、streamId 或业务字段 |
| Frame | 定义 Standard Frame 边界、长度、PayloadType、fragment、CRC | 放入业务类型或业务状态 |
| Payload | 把 payload 分发给 CONTROL、RPC 或 STREAM parser | 定义业务 registry |
| Registry | 定义 method / event / error / capability / schema / profile 的机器事实 | 改变 Frame wire format |
| Business | 实现具体设备语义 | 绕过 registry 直接修改底层 wire contract |

## 4. Runtime 上下文

AXTP 中不要把所有上下文都称为同一个 "Session"。实现时应区分四类上下文：

| 上下文 | 创建方式 | 适用范围 |
|---|---|---|
| Transport connection | 底层 transport connect | Standard Framed 和 WebSocket Unframed JSON |
| Framed Link Context | CONTROL OPEN / ACCEPT | 仅 Standard Framed |
| RPC Session | RPC Hello / Identify / Identified | Standard Framed 和 WebSocket Unframed JSON |
| Stream Context | 已采纳业务 method 建立 stream 后 | Standard Framed STREAM |

WebSocket Unframed JSON 没有 Framed Link Context，也没有 STREAM Context。

## 5. 生产路径

### 5.1 Standard Framed

Standard Framed 路径的职责分布：

```text
Transport connected
  -> CONTROL 建立 Framed Link Context
  -> RPC 建立 RPC Session
  -> RPC 承载业务控制
  -> Stream Context 存在后，STREAM 承载连续数据
```

完整 wire format 和状态机分别由 `03`、`04`、`05`、`06`、`07` 定义。

### 5.2 WebSocket Unframed JSON

WebSocket Unframed JSON 路径的职责分布：

```text
WebSocket connected
  -> RPC Hello / Identify / Identified 建立 RPC Session
  -> RPC 承载 Request / Response / Event
```

该路径是正式 RPC-only 通道。它不承载 CONTROL、STREAM、CRC16、Standard Frame Header，也不使用 JSON_BINARY RPC fixed header。

## 6. 非规范材料边界

为保持 `specs` 作为正式规范主干，以下内容不属于本文的规范正文：

| 内容 | 归属 |
|---|---|
| legacy protocol mapping、CmdValue 迁移、adapter 设计 | `workspace/legacy-migration/**` |
| roadmap、P1/P2/Future 计划 | `docs/product/roadmap.md`、`release/**` 或明确标记的 appendix |
| 长业务示例、端到端教程 | `docs/guides/**`、`workspace/flows/**` |
| 当前完整 method/event/error/capability/profile 清单 | `contract/generated/**` |
| Codex / agent 操作流程 | `tooling/skills/**` |

本文可以引用这些材料，但不把它们作为 AXTP Core 的 wire 或 runtime 语义来源。
