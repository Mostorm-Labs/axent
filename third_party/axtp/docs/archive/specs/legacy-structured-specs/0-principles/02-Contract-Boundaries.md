# AXTP 连接建立边界与协议事实源边界

本文是 AXTP 主仓库的合同等级、冲突处理和连接状态机边界说明。

它覆盖两件事：

1. Runtime / SDK、协议维护者和测试负责人应该按什么顺序判断“哪份材料是合同”。
2. AXTP Phase 1 从 Standard Framed `CONTROL OPEN / ACCEPT` 到 RPC Hello / Identify / Identified，再到 `APP_READY` 的连接状态机边界。

如果要找具体操作指南，请看 [Protocol Maintainer Guide](../../../../guides/protocol-maintainer.md)。如果要找 lifecycle skill，请看 `../../tooling/skills/README.md`。

## 0. 合同等级与冲突处理

### 0.1 Runtime implementation contract

Runtime / SDK / mock server 实现者按以下顺序读取实现合同：

```text
1. Released protocol artifact, when available
2. contract/protocol/axtp.protocol.yaml
3. contract/generated/protocol.md and contract/generated/protocol.json
4. conformance/**
5. specs/**
```

说明：

- Released protocol artifact 是 release 后最适合 runtime 绑定的快照。
- `contract/protocol/axtp.protocol.yaml` 是 generated Protocol IR，适合工具、runtime、mock server 消费。
- `contract/generated/protocol.md` 和 `contract/generated/protocol.json` 是当前 generated reference。
- `conformance/**` 是 runtime 行为验收输入。
- `specs/**` 是人工维护的正式规范，用来解释 wire、session、registry、codec、tooling 和 versioning 规则。

Runtime MUST NOT implement draft-only `workspace/protocol/**` entries unless explicitly building a mock/prototype.

### 0.2 Protocol maintenance sources

协议维护者修改协议事实时，按以下顺序判断源头：

```text
1. specs/**
2. contract/registry/**/*.yaml
3. contract/registry/domains/**/*.yaml
4. tooling/generators/**
5. generated outputs after regeneration
```

说明：

- `specs/**` 维护规则和治理约束。
- `contract/registry/**/*.yaml` 与 `contract/registry/domains/**/*.yaml` 是手写机器事实源。
- `tooling/generators/**` 定义如何从事实源生成 Protocol IR、generated reference、snapshots 和测试向量。
- `contract/protocol/axtp.protocol.yaml` 与 `contract/generated/**` 是输出，不是手写输入。

### 0.3 非合同材料

以下材料不是 runtime 实现合同：

```text
workspace/business/**
workspace/flows/**
workspace/protocol/** before adoption
workspace/legacy-migration/**
docs/product/roadmap.md
```

它们分别用于业务输入、场景流程、协议草案、迁移证据和规划。它们可以指导评审，但不能跳过采纳与生成直接进入 runtime。

### 0.4 冲突处理规则

| 冲突 | 判断方式 | 处理 |
|---|---|---|
| generated 与 draft 冲突 | `contract/generated/**` 或 Protocol IR 与 `workspace/protocol/**` 不一致。 | runtime 相信 generated；维护者回到草案或采纳流程修正。 |
| generated 与 specs/YAML 冲突 | generated 输出不能反映 specs 或 registry YAML。 | 维护者修正 specs、YAML 或 Generator 后重新生成。 |
| conformance 与 specs/generated 冲突 | case 要求和正式合同不一致。 | 修 `conformance/**` 或 specs/YAML，不让 runtime 绕过。 |
| roadmap 与 generated 冲突 | roadmap 规划了未 generated 能力。 | runtime 不实现 roadmap；产品/架构推动 flow、draft、采纳和生成。 |
| legacy mapping 与 generated 冲突 | 迁移候选无法映射当前 generated 事实。 | 标记 blocked 或 adapter-private，进入迁移评审。 |

一句话规则：

```text
草案可以讨论；
contract/registry/specs 才能成为源；
Protocol IR/generated 是输出；
conformance 用来验收 runtime。
```

## 1. 两个 session 不要混用

| 概念 | 所在层 | 由谁产生 | Phase 1 用途 |
|---|---|---|---|
| Link Context `sessionId` | CONTROL | Physical Server 可在 ACCEPT 中返回 | 可保存到连接上下文，用于日志、追踪和未来 RESUME；不参与业务路由。 |
| RPC `sid` | RPC | Logical Server 在 Identified 中返回 | 业务 session ID；所有 Request / Response / Event 都必须携带。 |

`sessionId` 解决“这条 framed link 是哪条链路”的问题。`sid` 解决“这次业务会话是谁”的问题。设备直连时二者看起来可能一一对应，但网关、多客户端、云反连、断线恢复和调试追踪场景会让二者自然分离。

## 2. Phase 1 控制面只做三件事

CONTROL 在 Phase 1 只负责：

| 能力 | 消息 | 目的 |
|---|---|---|
| 建连协商 | `OPEN / ACCEPT` | 建立 Standard Framed link context，协商 MTU、Frame 大小、RPC encoding 和心跳间隔。 |
| 保活 | `HEARTBEAT / HEARTBEAT_ACK` | 发现半开连接、静默断链和 transport 空闲超时。 |
| 优雅关闭 | `CLOSE / CLOSE_ACK` | 让对端停止新业务、清理上下文并关闭 transport。 |

`ACK / NACK` 保留给后续可靠传输、低带宽链路、固件传输或 STREAM profile。普通 RPC 的业务结果由 `requestId` 和 `RequestResponse.status` 表达，不需要每个 Frame 再做严格确认。

## 3. RPC encoding 与 bodyEncoding 的边界

`PayloadType=RPC` 的第一个字节永远是 `rpcEncoding`：

| rpcEncoding | 值 | 含义 |
|---|---:|---|
| JSON | `0x01` | UTF-8 JSON 编码的 `{ sid, op, d }` envelope。 |
| CBOR | `0x02` | 后续 CBOR 编码的 `{ sid, op, d }` envelope。 |
| MSGPACK | `0x03` | 后续 MessagePack 编码的 `{ sid, op, d }` envelope。 |
| JSON_BINARY | `0x04` | 固定 15B 二进制 envelope，再由 `bodyEncoding` 解释 body。 |

`bodyEncoding` 只在 `JSON_BINARY` 下有意义。Phase 1 只保留 `NONE / TLV8 / TLV16`，其中 `TLV8` 是最小二进制 body 实现。

## 4. STREAM 是 Phase 1 的 audio/video 数据面

Phase 1 先实现控制面、业务 RPC 闭环，以及 Standard Framed 的 STREAM 数据面。P0 重点是 audio/video 媒体流：通过 `video.stream` / `audio.stream` 等 RPC 方法打开或关闭流，通过 `PayloadType=STREAM` 承载实际媒体数据。

固件、文件、日志等也会复用同一套 STREAM Header 和 Stream Context，但这些业务 profile 可以后续增量采纳。ACK/NACK 严格重传不是 Phase 1 前置条件；P0 STREAM 可先依赖有序 `seqId/cursor`、CRC 和业务播放器/解码器容错。

## 5. Domain-feature-method 的使用方式

业务协议不要从 transport 代码开始设计。正确路径是：

```text
业务需求
  -> workspace/flows/<scenario>.md
  -> workspace/protocol/<domain>/<domain.feature>.md
  -> contract/registry/domains/<domain>/domain.yaml
  -> contract/protocol/axtp.protocol.yaml
  -> contract/generated/*
  -> runtime / conformance
```

`domain.feature` 决定业务归属，`method/event/schema/error/capability` 决定机器可读事实。runtime 只消费 generated 产物和 conformance，不自行发明协议事实。

## 6. 连接状态机与合同边界的关系

连接状态机只定义 runtime 何时可以从 `LINK_CONNECTED` 进入 `FRAMING_READY`、`RPC_READY` 和 `APP_READY`。它不定义业务 domain、method、event、schema 或 capability。

当连接规则与业务草案同时出现时：

| 问题 | 看哪里 |
|---|---|
| WebSocket JSON 是否必须等待 Server Hello 后才能 Identify | `specs/1-core/06-RPC-Session.md` |
| Standard Framed 是否必须先 CONTROL OPEN / ACCEPT | `specs/1-core/05-Control-Session.md` |
| 某个业务 method 是否存在 | `contract/protocol/axtp.protocol.yaml` 或 `contract/generated/protocol.md` |
| 某个业务 method 是否还只是草案 | `workspace/protocol/<domain>/<domain.feature>.md` 和 [Product Domain Status](../../../../product/domain-status.md) |
| runtime 应该跑哪些验收 case | `conformance/**` 和 runtime 支持等级声明 |

因此，runtime 不从草案补齐连接状态，也不从连接状态机推断业务协议事实。
