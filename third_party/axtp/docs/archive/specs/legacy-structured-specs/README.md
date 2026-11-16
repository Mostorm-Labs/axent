# AXTP 规范文档

> 状态： 正式规范入口
> 范围：`specs` 导航、权威边界与 runtime 阅读路径
> 权威边界：本文说明 AXTP 正式设计规则的位置；不定义新的 wire 字段、registry 条目或 generated 协议事实。

`specs/` 是 AXTP 的正式设计规范主干，回答“AXTP 的规则是什么”。它不是业务草案区，不是 legacy 迁移手册，也不是 generated 协议清单。

## 1. 先读哪几篇

如果只想先看懂 AXTP 怎么连、线上怎么传，按下面顺序读：

```text
0-principles/01  术语表：统一 CONTROL / RPC / STREAM / Session / Registry 等术语
0-principles/02  合同边界：实现合同等级、冲突处理和连接状态边界
0-principles/03  分类原则：domain / feature 归属判断
1-core/01  总览：AXTP 一页总览
1-core/02  协议框架：设计原则、分层架构、职责边界
1-core/03  Frame 与 Payload：Standard Framed Binary 的 frame/payload 边界
1-core/04  Transport Profile：Standard Framed Binary 与 WebSocket Unframed JSON 如何承载 AXTP
1-core/05  CONTROL 会话：OPEN / ACCEPT / reject outcome / HEARTBEAT / CLOSE
1-core/06  RPC 会话：Hello / Identify / Identified、sid/op/d、Request/Response/Event
1-core/07  STREAM 数据面：16B STREAM Header、streamId / seqId / cursor、payload 透传
1-core/08  低带宽降级：HID-64 / BLE / UART 等 profile-specific 降级边界
```

两条正式生产路径必须分开阅读：

| 路径 | 适用传输 | 线上结构 | 能力 |
|---|---|---|---|
| Standard Framed | `AXTP-USB-HID`、`AXTP-TCP` | `Standard Frame Header(12B) + Payload(N) + CRC16(2B)` | CONTROL / RPC / STREAM |
| WebSocket Unframed JSON | `AXTP-WS-JSON`、`AXTP-WS-CLOUD-REVERSE` | `WebSocket message payload = JSON { sid, op, d }` | RPC-only |

WebSocket Unframed JSON 没有 CONTROL、STREAM、CRC16、Standard Frame Header，也不使用 JSON_BINARY RPC 15B Header。

## 2. 规范地图

| 分组 | 文档 | 状态 | 职责 |
|---|---|---|---|
| Principles | [`0-principles/01-Glossary.md`](0-principles/01-Glossary.md) | 规范性术语 | 全局统一术语表 |
| Principles | [`0-principles/02-Contract-Boundaries.md`](0-principles/02-Contract-Boundaries.md) | 规范性治理原则 | 合同等级、冲突处理、连接状态机和事实源边界 |
| Principles | [`0-principles/03-Domain-Feature-Classification.md`](0-principles/03-Domain-Feature-Classification.md) | 规范性分类原则 | 业务能力选择 domain、feature 和候选 method/event 归属的判断规则 |
| Core | [`1-core/01-Overview.md`](1-core/01-Overview.md) | 总览 | AXTP 一页总览 |
| Core | [`1-core/02-Protocol-Framework.md`](1-core/02-Protocol-Framework.md) | 规范性架构说明 | 设计原则、分层架构、职责边界 |
| Core | [`1-core/03-Frame-and-Payload.md`](1-core/03-Frame-and-Payload.md) | Standard Framed 必需 runtime 规范 | Standard Framed Binary 的 frame/payload 边界、PayloadType、fragment、CRC |
| Core | [`1-core/04-Transport-Profiles.md`](1-core/04-Transport-Profiles.md) | 所支持 profile 的必需 runtime 规范 | transport profile、物理/逻辑角色、framed/unframed 启动流程 |
| Core | [`1-core/05-Control-Session.md`](1-core/05-Control-Session.md) | Standard Framed CONTROL 必需 runtime 规范 | OPEN / ACCEPT / reject outcome / HEARTBEAT / CLOSE；ACK/NACK/RESUME/PING/PONG 为 Optional/Future |
| Core | [`1-core/06-RPC-Session.md`](1-core/06-RPC-Session.md) | 所有 profile 的必需 runtime 规范 | RPC envelope、Hello / Identify / Identified、Request / Response / Event、eventMasks、JSON_BINARY |
| Core | [`1-core/07-Stream-Data-Plane.md`](1-core/07-Stream-Data-Plane.md) | 支持 STREAM 时的必需 runtime 规范 | STREAM 16B header、Stream Context、seq/cursor、payload 透传；可靠重传/断点续传为 Optional/Future |
| Core | [`1-core/08-Low-Bandwidth-Degradation.md`](1-core/08-Low-Bandwidth-Degradation.md) | 可选/未来的 profile-specific 规范 | HID-64 / BLE / UART / Compact 降级边界，不反向修改 v1 Core Standard Frame |
| Registry | [`2-registry/01-Naming-and-Taxonomy.md`](2-registry/01-Naming-and-Taxonomy.md) | 规范性治理规则 | `domain.feature`、method/event/capability 命名与分类规则 |
| Registry | [`2-registry/02-Methods-Registry.md`](2-registry/02-Methods-Registry.md) | 规范性 registry 规则 | Method registry 准入、ID/bitOffset 稳定性、schema/error/event 绑定 |
| Registry | [`2-registry/03-Events-Registry.md`](2-registry/03-Events-Registry.md) | 规范性 registry 规则 | Event registry 准入、eventId/bitOffset、eventMasks 与 payload schema |
| Registry | [`2-registry/04-Errors-Registry.md`](2-registry/04-Errors-Registry.md) | 规范性 registry 规则 | ErrorCode 准入、范围、CONTROL/RPC/STREAM 映射和稳定性 |
| Registry | [`2-registry/05-Profiles-Registry.md`](2-registry/05-Profiles-Registry.md) | 规范性 registry 规则 | Implementation profile 准入、runtime 支持声明和 conformance 关系 |
| Registry Appendix | [`2-registry/appendix/`](2-registry/appendix/) | 非规范历史/候选表 | Method/Event/Error/Profile/Capability 历史规划表与候选表；runtime 不得从这里实现 |
| Codec | [`3-codec/01-Type-System.md`](3-codec/01-Type-System.md) | 规范性 codec 规则 | 基础类型、nullable 边界、array/object/enum/bytes/number 规则 |
| Codec | [`3-codec/02-Capability-Types.md`](3-codec/02-Capability-Types.md) | 规范性模型规则 | Schema Model 与 Capability Model；能力发现不混入 CONTROL 建链 |
| Codec | [`3-codec/03-TLV-Encoding.md`](3-codec/03-TLV-Encoding.md) | 规范性 codec 规则 | TLV field envelope、基础类型/object/array 编码、unknown field、canonical encoding |
| Codec | [`3-codec/04-Schema-Numbering.md`](3-codec/04-Schema-Numbering.md) | 规范性 codec 规则 | schema-local fieldId 分配、reserved/deprecated、兼容新增和禁止复用 |
| Tooling | [`4-tooling/01-YAML-Mapping.md`](4-tooling/01-YAML-Mapping.md) | Tooling 合同 | Source registry YAML 到 Protocol IR 的映射、业务模块准入和 validation |
| Tooling | [`4-tooling/02-Generator-V1.md`](4-tooling/02-Generator-V1.md) | Tooling 合同 | Generator v1 输入、输出、不可手写 generated、失败条件、CLI/CI |
| Tooling | [`4-tooling/03-Versioning.md`](4-tooling/03-Versioning.md) | 版本治理合同 | spec tag、registry version、breaking change、release artifact 和 runtime 绑定 |

## 3. 角色阅读路径

| 读者 | 推荐路径 |
|---|---|
| 首次了解 AXTP | `0-principles/01-Glossary.md` -> `0-principles/02-Contract-Boundaries.md` -> `1-core/01-Overview.md` -> `1-core/02-Protocol-Framework.md` |
| Standard Framed runtime 实现者 | `0-principles/01` -> `0-principles/02` -> `1-core/01` -> `1-core/02` -> `1-core/03` -> `1-core/04` -> `1-core/05` -> `1-core/06` -> `1-core/07` |
| WebSocket JSON runtime 实现者 | `0-principles/01` -> `0-principles/02` -> `1-core/01` -> `1-core/02` -> `1-core/04` -> `1-core/06` |
| Registry / generator 维护者 | `0-principles/01` -> `0-principles/02` -> `2-registry/*` -> `3-codec/*` -> `4-tooling/*` |
| 业务协议评审者 | `0-principles/01` -> `0-principles/02` -> `0-principles/03` -> `2-registry/01` -> `2-registry/02-05` -> `3-codec/02` -> `4-tooling/01` |
| 低带宽 profile 评审 | `0-principles/01` -> `1-core/02` -> `1-core/03` -> `1-core/04` -> `1-core/08` |

## 4. 权威边界

同一件协议事实只应在一个权威位置讲完整：

| 内容类型 | 权威位置 |
|---|---|
| 术语含义 | `specs/0-principles/01-Glossary.md` |
| 合同等级、冲突处理和事实源边界 | `specs/0-principles/02-Contract-Boundaries.md` |
| domain / feature 分类原则 | `specs/0-principles/03-Domain-Feature-Classification.md` |
| wire format、session、transport、codec 规则 | `specs/1-core/**`、`specs/3-codec/**` |
| registry 元模型、命名、ID 分配规则 | `specs/2-registry/**` |
| 历史规划表和候选 registry 表 | `specs/2-registry/appendix/**`，非规范 |
| generator 和 Protocol IR 映射规则 | `specs/4-tooling/**` |
| 具体当前 method/event/error/capability/profile 清单 | `contract/registry/**/*.yaml`、`contract/registry/domains/**/*.yaml` 与 `contract/generated/**` |
| 业务需求和 flow | `workspace/business/**`、`workspace/flows/**` |
| 协议草案 | `workspace/protocol/**` |
| legacy 证据、迁移计划、adapter 方案 | `workspace/legacy-migration/**` |
| runtime 验收要求 | `conformance/**` |
| Codex / agent 工作流 | `tooling/skills/**` |

如果 specs 中的当前清单与 YAML/generated 输出冲突，以 YAML/generated 为实现事实源，并回修 specs 中的描述或链接。

## 5. 非规范内容规则

`specs` 可以保留必要的设计背景，但以下内容不应作为主干规范来阅读：

- legacy 迁移细节：应在 `workspace/legacy-migration` 讲完整；
- roadmap、P1/P2/Future 规划：应标记为 future、appendix 或转到 roadmap/release 文档；
- 长业务示例和端到端教程：应在 `docs/guides` 或 `workspace/flows`；
- generated 当前大表：应由 `contract/generated` 输出；
- Method/Event/Error/Profile/Capability 候选规划表：只能在 `2-registry/appendix/**` 非规范保存；
- agent / skill 操作流程：应在 `tooling/skills`。

规范正文只保留实现者必须遵守的规则、字段语义、状态机、校验规则和兼容边界。

## 6. 规范关键字

Runtime 规范中的英文关键字按以下含义使用：

| 关键字 | 含义 |
|---|---|
| MUST | runtime 必须遵守；违反即不满足对应 profile 的实现合同 |
| SHOULD | 强建议；只有在有明确工程理由时才可偏离 |
| MAY | profile 或实现可选能力 |
| RESERVED / FUTURE | 保留或未来能力；不得作为 v1 必选实现要求 |
