---
status: generated
contract: true
generated: true
domain: device
feature: device.enrollment
registry: ../../../../contract/registry/domains/device/domain.yaml
lastReviewed: 2026-06-16
---

# AXTP device.enrollment 协议草案

版本：v0.10（已采纳）

归属域：`device`（DomainId `0x01`）

Capability ID：`device.enrollment`（capability `0x0102`）

适用范围：未入管设备通过 pairing code 完成注册纳管、纳管状态查询和变更、纳管状态变化事件。

---

## 协议审核标记（人工复核）

完整的审核标记（含 `[REVIEW-DRAFT]` / `[REVIEW-RESOLVED]` / `[REVIEW-ASK]` 条目、审核结论与后续动作）见 **附录 A**，本文以附录 A 为唯一权威源。开篇要点：

- 本文已采纳（见下方「采纳记录 (Adoption)」），machine 事实源为 `registry/domains/device/domain.yaml`（DomainId `0x01`）；YAML 与 generated 产物不一致时以 registry YAML 为准。
- 域命名采用 `device.enrollment`（比 `device.binding` 更准确描述"未入管设备成为后台管理对象"的语义），方法名为 `device.getPairingCode`。
- Legacy 命令 `GetBindCode` / `GetBindConfig` / `SetBindConfig` / `OnBindState` 的逐字段映射见 **Section 9**；binding → device.enrollment 的 generated legacy 文件更新本次不落地，留待 legacy-migration 专项处理（见 9.6 / 采纳记录 A.5）。

---

## 采纳记录 (Adoption)

本文于 2026-06-17 经 `adopt-protocol-draft` skill（`tooling/skills/30-adopt-protocol-draft`）采纳。machine 事实源为 `registry/domains/device/domain.yaml` 与 `registry/error/error_code.yaml`；本草案为正式提案，YAML 与 generated 产物不一致时以 registry YAML 为准。

### A.1 已分配正式 ID

DomainId `0x01` = `device`（device 域已存在并被 `device.getInfo` 使用，generator `domainByHighByte` 无需补充）。

| 条目 | ID | bitOffset | 备注 |
|---|---|---:|---|
| method `device.getPairingCode` | `0x0102` | 1 | request `DeviceGetPairingCodeParams` / response `DevicePairingCodeInfo` |
| method `device.getEnrollmentState` | `0x0103` | 2 | request `DeviceGetEnrollmentStateParams` / response `DeviceEnrollmentInfo` |
| method `device.setEnrollmentState` | `0x0104` | 3 | request `DeviceSetEnrollmentStateParams` / response `DeviceSetEnrollmentStateResult`；触发 event |
| event `device.enrollmentStateChanged` | `0x0102` | 0 | payload `DeviceEnrollmentStateChangedEvent`；trigger `device.setEnrollmentState`（device 域首个 event） |
| capability `device.enrollment` | `0x0102` | — | type `object`；schema `DeviceEnrollmentCapability` |

低字节计数惯例（与 `software.config` / `software.updatePolicy` / `signage.playlist` 一致）：method / event / capability 各自独立从 `0x01` 起编号，三者可同号共存（不同命名空间）。method bitOffset 续编 `device.getInfo`（0）之后为 1/2/3；device 域原无 event，event bitOffset 从 0 起；均满足 generator `assertDomainBitOffsets` 从 0 起连续要求。

### A.2 Schema 名映射（草案 → registry）

本草案 §6 schema 名在 registry 中统一加 `Device` 前缀（与 `device.info` 的 `DeviceInfo` / `DeviceIdentity` 惯例一致，保证 schema 名全局唯一）：

| 草案 schema 名（§6） | registry schema 名 | 说明 |
|---|---|---|
| `EnrollmentInfo`（§6.1） | `DeviceEnrollmentInfo` | getEnrollmentState result / SetEnrollmentStateResult.state / event.state 共用 |
| `EnrollmentEndpointSummary`（§6.2） | `DeviceEnrollmentEndpointSummary` | 嵌套于 DeviceEnrollmentInfo 与 DeviceSetEnrollmentStateParams |
| `GetPairingCodeParams`（§6.3） | `DeviceGetPairingCodeParams` | |
| `PairingCodeInfo`（§6.4） | `DevicePairingCodeInfo` | `code` pattern `^[A-HJ-NP-Z2-9]{6,8}` 写入 description |
| `GetEnrollmentStateParams`（§6.5） | `DeviceGetEnrollmentStateParams` | |
| `SetEnrollmentStateParams`（§6.6） | `DeviceSetEnrollmentStateParams` | 校验规则（enrolled 需 endpoint+reason 等）写入 description |
| `SetEnrollmentStateResult`（§6.7） | `DeviceSetEnrollmentStateResult` | |
| `EnrollmentStateChangedEvent`（§6.8） | `DeviceEnrollmentStateChangedEvent` | |
| 草案 §5 capability 字段 | `DeviceEnrollmentCapability` | `supportsPairingCode` 等 7 字段 |

枚举值统一用 `candidate values include ...` 开放措辞（对齐 `device.info` 标杆），不写死枚举集合；`maxActivePairingCodes` / `pairingCodeLength` 用 `default` 表达默认值。

### A.3 错误码决策

method 复用 common mvp 码：

| Error | 复用 common 码 | 数值 | 用于 |
|---|---|---|---|
| `NOT_SUPPORTED` | ✓ mvp | `0x0003` | getPairingCode / getEnrollmentState / setEnrollmentState |
| `INVALID_ARGUMENT` | ✓ mvp | `0x000A` | setEnrollmentState（字段非法） |
| `INVALID_STATE` | ✓ mvp | `0x0004` | setEnrollmentState（状态机转换违规） |
| `PERMISSION_DENIED` | ✓ mvp | `0x0009` | getPairingCode / setEnrollmentState |
| `INTERNAL_ERROR` | ✓ mvp | `0x000E` | getPairingCode（服务端无法生成 code） |

新增 device 域业务错误码（pairing code 过期/已用是 enrollment 独有、客户端需精确区分的语义；连号追加在 device 段已占用区末尾 `0x0109` 之后）：

| Error | 数值 | retryable | 说明 |
|---|---|---|---|
| `ENROLLMENT_CODE_EXPIRED` | `0x010A` | true | pairing code 已过期 |
| `ENROLLMENT_CODE_ALREADY_USED` | `0x010B` | false | pairing code 已使用 |

### A.4 限定采纳范围

本次为**局部采纳（scoped）**，核心事实（3 method / 1 event / 8 schema / 状态机 / capability 主体）全部采纳；`[REVIEW-ASK]` 事实用安全默认值写入 YAML，标注 `[REVIEW-ADOPTED-SCOPED]`，确认后走 `amend-adopted-protocol`（Stage 40）：

- ✅ **采纳**：3 个 method（`getPairingCode` / `getEnrollmentState` / `setEnrollmentState`）、1 个 event（`enrollmentStateChanged`）、8 个 schema、§3.4 状态机转换表、capability 主体字段。
- ⏸ **scoped 默认值**：`DeviceEnrollmentEndpointSummary.displayName` `max_length: 128`（与 `device.info` 的 `product.displayName` 对齐，待产品确认）。
- ⏸ **scoped 默认值**：`workspaceId` / `endpointId` 作为 `required: false` 摘要字段写入，隐私暴露策略待产品/安全确认。
- ⏸ **scoped 保留**：`enrolledAt`、`triggerMethod` / `triggerId`、capability 的 `endpointTypes` / `supportedPurposes` / `maxActivePairingCodes` / `pairingCodeLength` 字段以开放措辞写入，待产品确认 P0 范围与具体上限。
- ⏸ **scoped 保留**：`desiredState: "pending"` 方向维持 Server → Device（服务端通知设备 code 已提交）；设备是否可自行转入 pending 待确认。
- ⏸ **保留**：是否需要独立 `device.revokeEnrollment` 方法——当前用 `setEnrollmentState(desiredState: "unmanaged")` 替代解绑；如需独立权限或流程则另建方法。
- ⏸ **保留**：pairing code 生成方向（Device → Server 拉取，服务端 handler 生成）缓存策略待确认；`getPairingCode` 命名偏好（pairing code vs enrollment code）待产品最终确认。

### A.5 后续约束

- 上述 scoped 项确认后走 `tooling/skills/40-amend-adopted-protocol`（Stage 40）正式变更，不再重跑 Stage 30。
- generated 产物（`protocol/axtp.protocol.yaml`、`docs/generated/*`、`tooling/mcp/*`）由 `tooling/skills/50-generate-axtp-protocol`（Stage 50）重跑生成；本次不手改任何 generated 文件。
- legacy mapping（binding → device.enrollment，§9.6）本次不落地到 `registry/legacy/legacy_mapping.yaml`，与 `software.config` / `software.updatePolicy` / `signage.playlist` 三个 sibling 采纳一致；generated legacy 文件更新留待 legacy-migration 专项处理。

---

**v0.10 变更说明（采纳）：**
经 `adopt-protocol-draft` skill 采纳，machine 事实源落地为 `registry/domains/device/domain.yaml`（追加到已存在的 device domain，DomainId `0x01`）与 `registry/error/error_code.yaml`。本草案冻结为正式提案，标题区补 DomainId/capabilityId，并新增「采纳记录 (Adoption)」节记录已分配正式 ID、Schema 名映射、错误码决策、限定采纳范围与后续约束。
本次为**局部采纳（scoped）**：核心事实（3 method / 1 event / 8 schema / §3.4 状态机 / capability 主体）全部采纳；`[REVIEW-ASK]` 事实用安全默认值写入 YAML 并标 `[REVIEW-ADOPTED-SCOPED]`，确认后走 `amend-adopted-protocol`。具体 scoped 默认值：`DeviceEnrollmentEndpointSummary.displayName` `max_length: 128`（对齐 `device.info` `product.displayName`）；`workspaceId` / `endpointId` 为 `required: false` 摘要字段。
ID 分配（低字节计数惯例，与 `software.config` / `signage.playlist` 一致）：method `0x0102`/`0x0103`/`0x0104`、event `0x0102`、capability `0x0102`；method bitOffset 续编 device.getInfo 之后为 1/2/3，device 域首个 event bitOffset 从 0 起。
Schema 名加 `Device` 前缀（`EnrollmentInfo` → `DeviceEnrollmentInfo` 等），与 `device.info` 的 `Device` 前缀惯例一致，保证全局唯一。错误码：4 个 method 复用 common mvp 码；新增 2 个 device 业务码 `ENROLLMENT_CODE_EXPIRED`(`0x010A`) / `ENROLLMENT_CODE_ALREADY_USED`(`0x010B`)，连号追加在 device 段已占用区末尾。
本次不新增 legacy mapping 到 `registry/legacy/`（与三个 sibling 采纳一致），binding → device.enrollment 的 generated 文件更新留待 legacy-migration 专项；不改 MVP profile；不改 §3.4 状态机、§6 schema 业务语义、§7 flow 示例、§9 legacy 映射记录。generated 产物由 Stage 50 重生成。

**v0.9 变更说明：**
对齐 20-draft-business-protocol skill 与 `protocol-draft-template.md` 的 JSON 示例约定（业务语义、schema、状态机、错误码、legacy 映射均不变）：
(1) 在 §0 速读结论后新增 `## JSON 示例约定` 节（RPC envelope 速查 + op=6/7/8 表，声明示例默认 `APP_READY`、后续只展示 RPC `d` block、禁止 JSON-RPC 2.0 外层格式）。
(2) 将原集中式第 7 节 12 个 JSON 示例迁移到各 method/event 小节：每个 method 补齐 Request / Success Response / Error Response `d block` 内联示例 + 错误表 + `规则`；event 补齐 Event `d block` 示例 + 客户端处理建议 + `规则`。method/event 子标题改为带编号风格（`3.x.1`…`4.1.1`…），示例标题统一标注 `op=7` / `op=8` / `op=6`。
(3) 第 7 节改为 `## 7. 交互流程示例 Flow Examples`，只保留端到端 flow（首次注册 / 解绑 / 重连后状态校准）。
(4) 本次不改 schema 定义（6.1–6.8）、状态机（3.4）、错误码（§8 数值已核实全部正确）、legacy 映射（9.0–9.6）、附录 A–D；JSON 示例的业务内容（字段值、错误码、placeholder）原样迁移，只改位置、标题与「d block」称谓。

**v0.8 变更说明：**
(1) 格式修正：变更说明版本顺序修正——v0.2/v0.3 原顺序颠倒（v0.2 排在 v0.3 前），调整为严格倒序（v0.3 在前），与 signage.playlist / software.config / software.updatePolicy 等标杆草案一致。
(2) 同步 frontmatter `lastReviewed` 与标题版本号。
(3) §7.3 读法补充 pending→enrolled 两步流程交叉引用（指向 §7.3c / §3.4 / §6.6 / §7.5a），增强可读性，不改语义。
(4) 本次不改 schema（§6.1–6.8）、状态机转换表（§3.4）、错误码（§8）、方法/事件定义。

**v0.7 变更说明：**
(1) 结构去重与 flow 对齐，不改业务语义：将文件开头的"协议审核标记"简表收敛为指引段，完整审核标记统一以**附录 A** 为唯一权威源。
(2) 删除 Section 10 的 9 项"采纳检查清单"子节（与附录 D 重复且不同步），完整采纳检查清单统一以**附录 D** 为唯一权威源。
(3) Section 3.4 状态机末尾新增 flow 阶段3 序列图省略 `pending` 中间状态的交叉引用，消解 flow 序列图与状态机在 pending 路径上的观感差异。
(4) 本次不改 schema（6.1–6.8）、状态机转换表（3.4）、错误码（Section 8）、JSON 示例（Section 7）。

**v0.6 变更说明：**
(1) 按 20-draft-business-protocol skill 要求补齐附录 A–D，对齐成熟草案（`signage.playlist`）结构标杆。
(2) 新增附录 A 协议审核标记：整合文件开头简表与 Section 12 的 REVIEW 项为正式附录；明确记录方法命名采用 `getPairingCode`（非 `getEnrollmentCode`）的决策，消解 flow Open Question 命名歧义。
(3) 新增附录 B 协议决策：记录域/命名、控制面/数据面/WebSocket、6 状态枚举与状态机、解绑跨域副作用不隐式清除、保留 `expiresInSeconds`、时间戳类型变更、`type=room` 时 `profileId` 必填等关键决策。
(4) 新增附录 C Registry 草案输入：给出 capability / methods / event / 候选 errors 的 YAML 候选片段，所有 numeric ID 为 `TBD after adoption`；说明 generated 当前 `binding` 域占用的 `0x1301`–`0x1303` 待 Stage 30 采纳时随 domain 改名一并处理。
(5) 新增附录 D 采纳检查清单：规范化 Section 10 已有清单并补充附录就绪自检项。
(6) 本次不改 schema 定义（6.1–6.8）、状态机（3.4）、错误码（Section 8 已核实正确）、JSON 示例（7.1–7.5a）。

**v0.5 变更说明：**
(1) 新增 Section 6.0 Schema 层级速览，展示 schema 分类（请求 Params / 响应 Results / 事件 Payload / 共享对象）和阅读规则。
(2) 新增校验规则块：6.2 `EnrollmentEndpointSummary`（`type = "room"` 时 `profileId` 必填）、6.4 `PairingCodeInfo`（`code` 格式 regex、`expiresInSeconds > 0`、双时间戳规则）、6.6 `SetEnrollmentStateParams`（条件必填升级为正式校验规则 + 状态转换校验）。
(3) 修正 Section 8 候选 Errors 表：补齐 `INTERNAL_ERROR` (common 0x000E)；添加候选业务错误域范围脚注（device 域 0x0100–0x01FF）。
(4) 更新 JSON 示例 7.2、7.3、7.4：补齐 `profileId` 和 `enrolledAt` 字段，对齐 schema 定义。
(5) 新增 JSON 示例：7.1c `INTERNAL_ERROR` 错误、7.3c `pairing_available → pending` 中间转换、7.5a `INVALID_STATE` 状态转换违规。
(6) Section 9 Legacy 映射添加 Adapter 转换示例：9.1 `GetBindCode` 响应方向（`expiresAt` 类型转换）、9.3 `SetBindConfig` 请求方向（`bound` → `desiredState` + `reason` + `endpoint` 合成）。
(7) Section 3.4 状态机 `pairing_available → pending` 备注增加实现灵活性说明；6.8 `EnrollmentStateChangedEvent.triggerMethod` 语义澄清（`"server_sync"` 含义、`"getPairingCode"` 间接关联）。
(8) Section 12 新增 `EnrollmentEndpointSummary.displayName` 最大长度确认项。

**v0.4 变更说明：**
(1) 修正 Section 8 候选 Errors 中 `INVALID_STATE` 和 `PERMISSION_DENIED` 的错误码：`INVALID_STATE` 从错误的 `0x000E` 更正为 `0x0004`；`PERMISSION_DENIED` 从错误的 `0x0105` 更正为 `0x0009`（`0x000E` 实际为 `INTERNAL_ERROR`，`0x0105` 实际为 `DEVICE_OVER_TEMPERATURE`）。
(2) 修正 JSON 示例 7.5 中 PERMISSION_DENIED 错误的 `"code": 261`（`0x0105`）为 `"code": 9`（`0x0009`）。
(3) Section 6 Schemas 重组：新增 6.3–6.8 子节，将方法/事件小节内联定义的 `GetPairingCodeParams`、`PairingCodeInfo`、`GetEnrollmentStateParams`、`SetEnrollmentStateParams`、`SetEnrollmentStateResult`、`EnrollmentStateChangedEvent` 提升为 Section 6 独立 schema 定义；方法/事件小节保留内联字段表并增加 Section 6 交叉引用。

**v0.3 变更说明：**
(1) Section 9 "Legacy 待映射" 从 4 行简表扩展为 9.0-9.6 子节，包含逐命令字段映射表（GetBindCode / GetBindConfig / SetBindConfig / OnBindState）、adapter 转换说明和 generated legacy 文件交叉引用。
(2) `PairingCodeInfo.expiresAt` 字段说明增加 legacy 类型变更注释（Unix timestamp integer → RFC 3339 string）。
(3) 审核标记表 `legacy 映射` 行从 `[REVIEW-ASK]` 更新为 `[REVIEW-RESOLVED]`。

**v0.2 变更说明：**
(1) 将 `EnrollmentState` schema 重命名为 `EnrollmentInfo`，消除 schema 名与 `state` 字段枚举的命名冲突。
(2) 新增 3.4 enrollment 状态机小节，包含状态定义和有效转换表。
(3) 补齐 `GetPairingCodeParams.purpose` 语义、`PairingCodeInfo.code` 格式约束、`expiresAt`/`expiresInSeconds` 优先级规则。
(4) 补齐 `SetEnrollmentStateParams` 条件必填规则（endpoint/message/reason 各 desiredState 下的要求）。
(5) 补齐 `EnrollmentInfo` 字段约束（deviceId 范围、workspaceId 隐私说明、新增 enrolledAt 审计字段、updatedAt 标准化为 RFC 3339）。
(6) `EnrollmentStateChangedEvent` 新增 triggerMethod/triggerId 关联字段；`previousState` 明确为枚举值而非完整对象。
(7) Capability 表新增 supportedPurposes/maxActivePairingCodes/pairingCodeLength；endpointTypes 解除 `[REVIEW-ASK]`。
(8) JSON 示例新增 5 个缺失场景：refresh=true、NOT_SUPPORTED 错误、解绑、注册失败、includeEndpoint=false。

---

## 0. 速读结论

| 项目 | 内容 |
|---|---|
| 这个能力做什么 | 让未入管设备通过 pairing code / enrollment state 成为后台可管理对象。 |
| 当前状态 | draft（已采纳局部 / scoped，见「采纳记录 (Adoption)」） |
| 是否可直接实现 | 否（草案为提案，未生成）；machine 事实源为 `registry/domains/device/domain.yaml`，generated 产物待 Stage 50 重生成。 |
| 主要交互 | RPC + EVENT |
| 是否使用 STREAM | 否 |
| Registry readiness | adopted（scoped）— YAML 已写入 |
| Conformance | needed（待 Stage 50 generated 后补 conformance cases） |
| 主要未决问题 | pairing code 生成方向、endpoint 关联字段、解绑语义和跨域副作用仍需确认。 |

---

## JSON 示例约定

本文中的 JSON 示例默认 RPC Session 已进入 `APP_READY`，`sid` 已由 Server 分配。Hello、Identify、Identified 属于 RPC Session 规范，不在每篇业务 feature 草案中重复。

示例使用 AXTP RPC JSON envelope。除本节的 envelope 速查外，后续 method/event/flow 示例默认只展示 RPC `d` 数据块，并在小节标题中标明对应 `op`：

```json
{ "sid": "12345678", "op": 7, "d": {} }
```

| op | 名称 | 用途 |
|---:|---|---|
| `6` | Event | 设备向客户端推送事件。 |
| `7` | Request | 客户端调用业务 method。 |
| `8` | RequestResponse | 设备返回业务 method 结果或错误。 |

本文中的 `sid="12345678"`、`id`（取 `1`–`9` 等示例值）、`intent=1` 均为示例值。正式 methodId、eventId、fieldId、errorCode、intent bit 由 registry 采纳后分配。

业务草案不得使用 JSON-RPC 2.0 外层格式作为 AXTP wire 示例；不要在 AXTP 示例中写 `jsonrpc`、JSON-RPC 外层 `id/method/params`，或把 JSON-RPC envelope 当作 AXTP envelope。

---

## 1. 功能说明

`device.enrollment` 用于设备注册、pairing code 获取、纳管状态查询和变更。它描述"未入管设备成为后台管理对象"的过程，不属于认证会话（`auth.*`），也不属于 room 业务域（`room.*`）。

本文落实 `workspace/flows/signage-device-management.md` 中对 legacy `GetBindCode` / `GetBindConfig` / `SetBindConfig` / `OnBindState` 的最终定域。当前 generated 协议未包含这些方法或事件；本文所有 method、event、schema 均为候选，正式数值为 `TBD after adoption`。

**关键证据：** legacy device-sdk 实测中 `GetBindCode` 响应包含 `expiresInSeconds: 1800`（测试 `src/sdk.spec.ts` 中 `createServerSdk register wires command/event handlers` 断言失败暴露）。草案必须保留 `expiresInSeconds` 字段。

---

## 2. 能力边界

| 类型 | 内容 |
|---|---|
| 包含 | pairing code 获取和刷新、过期时间、纳管状态查询、纳管状态设置、纳管状态变化事件。 |
| 包含 | enrollment 后关联的 endpoint 摘要（endpointId、type、displayName）。 |
| 不包含 | 登录、token、session 鉴权；这些属于 `auth.*`。 |
| 不包含 | room 名称、room profile 修改；enrollment 完成后的 room endpoint 信息属于 `room.info`。 |
| 不包含 | 设备静态身份、硬件型号、软件版本；这些属于 `device.info` 或 `software.*`。 |
| 数据面 | 不使用 STREAM；所有交互通过 RPC method/event。 |

---

## 3. 方法

### 3.0 方法速览

| Method | 调用类型 | 用途 | Params Schema | Result Schema | 是否触发事件 | 状态 |
|---|---|---|---|---|---|---|
| `device.getPairingCode` (`0x0102`) | query / action | 获取或刷新设备纳管 pairing code。 | `GetPairingCodeParams` | `PairingCodeInfo` | 否 | adopted(scoped) |
| `device.getEnrollmentState` (`0x0103`) | query | 查询当前纳管状态。 | `GetEnrollmentStateParams` | `EnrollmentInfo` | 否 | adopted(scoped) |
| `device.setEnrollmentState` (`0x0104`) | command | 设置纳管状态（绑定成功、解绑等）。 | `SetEnrollmentStateParams` | `SetEnrollmentStateResult` | 是，状态变化后触发 `device.enrollmentStateChanged`。 | adopted(scoped) |

> 已采纳（局部）。methodId / bitOffset 已分配，见「采纳记录 A.1」；registry schema 名加 `Device` 前缀（`GetPairingCodeParams` → `DeviceGetPairingCodeParams` 等），见 A.2。

### 3.1 `device.getPairingCode`

| 项 | 内容 |
|---|---|
| 目的 | 返回用于现场纳管或后台认领的 pairing code。 |
| 调用类型 | query / action（Device → Server） |
| Params Schema | `GetPairingCodeParams` |
| Result Schema | `PairingCodeInfo` |
| 事件触发 | 不直接触发事件；pairing code 被使用或过期导致状态变化时由 `device.enrollmentStateChanged` 表达。 |
| 幂等 / 异步 | `refresh=false` 时返回当前有效 code；`refresh=true` 生成新 code 并使旧 code 失效。 |
| 常见错误 | `NOT_SUPPORTED`, `PERMISSION_DENIED`, `INTERNAL_ERROR` |

#### 3.1.1 请求参数 Params：`GetPairingCodeParams`

字段见 6.3。

| 字段 | 类型 | 必填 | 范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `refresh` | boolean | no | `true` / `false` | `false` | 是否强制刷新 pairing code。 |
| `purpose` | string | no | `"initial_enrollment"`, `"re_enrollment"`, `"service_repair"` | `"initial_enrollment"` | code 使用场景。`"initial_enrollment"`：首次注册新设备；`"re_enrollment"`：重新注册（如工作空间迁移）；`"service_repair"`：维修配对，可能有不同 TTL 或权限。`[REVIEW-DRAFT]` 不支持的 purpose 返回 `NOT_SUPPORTED`。 |

#### 3.1.2 返回结果 Result：`PairingCodeInfo`

字段见 6.4。

| 字段 | 类型 | 必填 | 范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `code` | string | yes | 6-8 位大写字母数字，排除易混淆字符（0/O, 1/I/L） | none | 可展示或输入的 pairing code。 |
| `expiresAt` | string | no | RFC 3339 timestamp | omitted | 绝对过期时间。Legacy `GetBindCode` 响应中此字段为 Unix timestamp integer（例 `1234567890`）；AXTP 改为 RFC 3339 string。Adapter 需做类型转换。详见 9.1。 |
| `expiresInSeconds` | uint32 | no | `> 0` | omitted | 相对过期秒数。legacy device-sdk 实测值为 `1800`，不可省略。 |
| `state` | string | no | `"available"`, `"expired"`, `"used"`, `"disabled"` | `"available"` | code 当前状态。转换路径：`available` → `used`（code 被云端消费）、`available` → `expired`（TTL 到期）、`available` → `disabled`（服务端撤销）。终态：`used`、`expired`、`disabled`。 |

> **时间戳优先级**：当 `expiresAt` 和 `expiresInSeconds` 同时存在时，`expiresAt` 为权威时间；`expiresInSeconds` 保留用于 legacy device-sdk 兼容（实测值 1800）。客户端应优先使用 `expiresAt`。

#### 3.1.3 d block 示例

request:

```json
{
  "id": 1,
  "method": "device.getPairingCode",
  "params": {
    "refresh": false,
    "purpose": "initial_enrollment"
  }
}
```

读法：`refresh` 默认 `false`，返回当前有效 code；置 `true` 时服务端生成新 code 并使旧 code 失效（见 §3.1.3 success 读法）。`purpose` 默认 `"initial_enrollment"`，设备首次纳管最常见场景。

success:

```json
{
  "id": 1,
  "status": {
    "ok": true,
    "code": 0
  },
  "result": {
    "code": "ABC123",
    "expiresAt": "2026-06-11T10:30:00Z",
    "expiresInSeconds": 1800,
    "state": "available"
  }
}
```

读法：`expiresInSeconds` 来自 legacy device-sdk 实测证据（`src/sdk.spec.ts` 断言差异暴露），草案必须保留。设备展示 `code` 和倒计时；用户在云端管理系统输入此 code。`refresh=true` 时返回新 code（如 `XYZ789`）和新的过期时间，旧 code 失效。

#### 3.1.4 可能触发的事件

| Event | 触发条件 | Payload Schema | 客户端处理建议 |
|---|---|---|---|
| 无 | query / action method 不应因获取 code 本身触发状态变化事件。 | none | pairing code 被使用或过期导致状态变化时由 `device.enrollmentStateChanged` 表达（见 §4.1）。 |

#### 3.1.5 错误

| Error | 类别 | 说明 |
|---|---|---|
| `NOT_SUPPORTED` | common | 设备或服务端不支持 pairing code。客户端应隐藏注册码入口或引导用户使用其他纳管方式。 |
| `PERMISSION_DENIED` | common | 无权获取 pairing code。 |
| `INTERNAL_ERROR` | common | 服务端无法生成 code。客户端应提示用户稍后重试。 |

#### 3.1.6 规则

- Request MUST 使用 `op=7`。
- Success / Error Response MUST 使用 `op=8`，并回显 Request 的 `d.id`。
- 失败响应 MUST NOT 携带业务 `result`。
- query / action method SHOULD NOT 因获取 code 本身触发状态变化事件；状态变化由后续 `device.setEnrollmentState` 或服务端同步驱动。
- 草案阶段不得分配正式 methodId、bitOffset 或 fieldId。


### 3.2 `device.getEnrollmentState`

| 项 | 内容 |
|---|---|
| 目的 | 查询设备是否已纳管，以及纳管后关联的后台对象摘要。 |
| 调用类型 | query（双向） |
| Params Schema | `GetEnrollmentStateParams` |
| Result Schema | `EnrollmentInfo` |
| 事件触发 | 无 |
| 幂等性 | 是 |
| 常见错误 | `NOT_SUPPORTED` |

#### 3.2.1 请求参数 Params：`GetEnrollmentStateParams`

字段见 6.5。

| 字段 | 类型 | 必填 | 范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `includeEndpoint` | boolean | no | `true` / `false` | `true` | 是否返回 enrollment 后的 endpoint 摘要。默认 `true` 因为最常见调用方（云端管理后台）需要 endpoint 信息；轻量轮询或已缓存 endpoint 时设为 `false`。 |

#### 3.2.2 返回结果 Result：`EnrollmentInfo`

返回 `EnrollmentInfo`：`state`（纳管状态枚举 `unmanaged`/`pairing_available`/`pending`/`enrolled`/`failed`/`unenrolling`）+ `deviceId`/`workspaceId`/`endpoint`/`enrolledAt`（`enrolled`/`unenrolling` 时填充）+ `updatedAt`/`message`。完整 schema 详见 §6.1。

#### 3.2.3 d block 示例

request:

```json
{
  "id": 2,
  "method": "device.getEnrollmentState",
  "params": {
    "includeEndpoint": true
  }
}
```

读法：`includeEndpoint` 默认 `true`，云端管理后台需要 endpoint 信息；轻量轮询或已缓存 endpoint 时设为 `false`。

success:

已纳管时（含 endpoint）：

```json
{
  "id": 2,
  "status": {
    "ok": true,
    "code": 0
  },
  "result": {
    "state": "enrolled",
    "deviceId": "device-7f3a2e1b",
    "workspaceId": "ws-2c8e1a90",
    "endpoint": {
      "endpointId": "room-9f4a2e1c",
      "type": "room",
      "displayName": "Boardroom A",
      "profileId": "<PROFILE_ID>"
    },
    "enrolledAt": "2026-06-10T08:30:00Z",
    "updatedAt": "2026-06-11T10:02:00Z"
  }
}
```

未纳管时：

```json
{
  "id": 2,
  "status": {
    "ok": true,
    "code": 0
  },
  "result": {
    "state": "unmanaged",
    "deviceId": "device-7f3a2e1b"
  }
}
```

读法：`state` 为 `enrolled` / `unenrolling` 时填充 `workspaceId` / `endpoint` / `enrolledAt`；`unmanaged` 等中间状态省略这些字段。`includeEndpoint=false` 时省略 `endpoint` 字段，适用于已缓存 endpoint 的轻量轮询场景。

#### 3.2.4 可能触发的事件

| Event | 触发条件 | Payload Schema | 客户端处理建议 |
|---|---|---|---|
| 无 | query method 不应因查询触发状态变化事件。 | none | 无需处理。 |

#### 3.2.5 错误

| Error | 类别 | 说明 |
|---|---|---|
| `NOT_SUPPORTED` | common | 设备不支持纳管状态查询。客户端应回退到默认假设（如 `unmanaged`）或隐藏纳管相关 UI。 |

#### 3.2.6 规则

- Request MUST 使用 `op=7`。
- Success / Error Response MUST 使用 `op=8`，并回显 Request 的 `d.id`。
- 失败响应 MUST NOT 携带业务 `result`。
- query method MUST NOT 因查询本身触发状态变化事件。
- 草案阶段不得分配正式 methodId、bitOffset 或 fieldId。


### 3.3 `device.setEnrollmentState`

| 项 | 内容 |
|---|---|
| 目的 | 设置纳管状态（绑定成功、解绑、清除失败状态或同步服务端认领结果）。 |
| 调用类型 | command（Server → Device 为主） |
| Params Schema | `SetEnrollmentStateParams` |
| Result Schema | `SetEnrollmentStateResult` |
| 事件触发 | 状态实际变化后触发 `device.enrollmentStateChanged`。 |
| 幂等 / 异步 | 对同一 `desiredState` + `reason` 可幂等；解绑可能异步。 |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_ARGUMENT`, `INVALID_STATE`, `PERMISSION_DENIED` |

#### 3.3.1 请求参数 Params：`SetEnrollmentStateParams`

字段见 6.6。

| 字段 | 类型 | 必填 | 范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `desiredState` | string | yes | `"enrolled"`, `"unmanaged"`, `"failed"`, `"pending"` | none | 目标状态。 |
| `reason` | string | no | `"pairing_code_used"`, `"server_claimed"`, `"user_unenrolled"`, `"admin_reset"`, `"unknown"` | `"unknown"` | 状态变化原因。 |
| `endpoint` | `EnrollmentEndpointSummary` | no | see schema | omitted | enrollment 成功后关联的 endpoint 摘要。 |
| `message` | string | no | human-readable | omitted | 失败、解绑或修复说明。 |

> **校验规则**：见 6.6。

#### 3.3.2 返回结果 Result：`SetEnrollmentStateResult`

字段见 6.7。

| 字段 | 类型 | 必填 | 范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `state` | `EnrollmentInfo` | yes | see schema | none | 操作后的纳管状态。 |
| `disconnectExpected` | boolean | no | `true` / `false` | `false` | 解绑或重置是否预期导致连接变化。`true` 仅在 `desiredState = "unmanaged"` 且设备需要关闭管理 session 时。其余转换均为 `false`。 |

#### 3.3.3 d block 示例

request:

```json
{
  "id": 3,
  "method": "device.setEnrollmentState",
  "params": {
    "desiredState": "enrolled",
    "reason": "pairing_code_used",
    "endpoint": {
      "endpointId": "room-9f4a2e1c",
      "type": "room",
      "displayName": "Boardroom A",
      "profileId": "<PROFILE_ID>"
    }
  }
}
```

读法：Server → Device 方向设置 `enrolled` 状态，同时下发 endpoint 信息。设备保存后后续 room endpoint 操作可进入 `room.info`。本示例假设设备已处于 `pending` 状态（先经 §3.3.3 success 读法中的 `pairing_available → pending` 转换）；状态机要求 `enrolled` 仅从 `pending` 转入（见 §3.4 状态机与 §6.6 校验规则），从 `unmanaged` / `pairing_available` 直接设 `enrolled` 会返回 `INVALID_STATE`（见 §3.3.3 error 读法）。

success:

```json
{
  "id": 3,
  "status": {
    "ok": true,
    "code": 0
  },
  "result": {
    "state": {
      "state": "enrolled",
      "deviceId": "device-7f3a2e1b",
      "workspaceId": "ws-2c8e1a90",
      "endpoint": {
        "endpointId": "room-9f4a2e1c",
        "type": "room",
        "displayName": "Boardroom A",
        "profileId": "<PROFILE_ID>"
      },
      "enrolledAt": "2026-06-11T10:02:00Z"
    },
    "disconnectExpected": false
  }
}
```

读法：`result.state` 为操作后的完整 `EnrollmentInfo`。不同 `desiredState` 的差异：
- `desiredState: "pending"`：设备进入 `pending`，表示 pairing code 已在云端提交、注册进行中（`pairing_available → pending` 中间转换，见 §3.4）。
- `desiredState: "unmanaged"`：触发解绑，设备进入 `unenrolling` 异步清理，`disconnectExpected` 可能为 `true`（见 Flow 7.2）。
- `desiredState: "failed"`：服务端通知注册失败，`state.message` 必填，用于展示失败原因。

error:

```json
{
  "id": 4,
  "status": {
    "ok": false,
    "code": 9,
    "msg": "Permission denied.",
    "details": {
      "candidateError": "ENROLLMENT_PERMISSION_DENIED"
    }
  }
}
```

读法：`PERMISSION_DENIED`（0x0009），无权执行解绑 / 纳管操作；候选业务错误 `ENROLLMENT_PERMISSION_DENIED` 落点 `0x0100–0x01FF`，编号 `TBD after adoption`，故 JSON 示例用最近通用码 `9` 并在 `details.candidateError` 放候选名。状态转换违规（如从 `unmanaged` 直接设 `enrolled`）返回 `INVALID_STATE`（0x0004 / `code: 4`），客户端应引导用户先获取 pairing code 经由 `pending` 转入。

#### 3.3.4 可能触发的事件

| Event | 触发条件 | Payload Schema | 客户端处理建议 |
|---|---|---|---|
| `device.enrollmentStateChanged` | 状态实际变化时触发。 | `EnrollmentStateChangedEvent` | 更新纳管页面、设备列表和后续 room endpoint 操作门禁；事件丢失时调用 `device.getEnrollmentState` 校准（完整事件定义见 §4.1）。 |

```json
{
  "event": "device.enrollmentStateChanged",
  "intent": 1,
  "data": {
    "previousState": "pending",
    "reason": "pairing_code_used",
    "state": {
      "state": "enrolled",
      "deviceId": "device-7f3a2e1b",
      "workspaceId": "ws-2c8e1a90",
      "endpoint": {
        "endpointId": "room-9f4a2e1c",
        "type": "room",
        "displayName": "Boardroom A",
        "profileId": "<PROFILE_ID>"
      },
      "enrolledAt": "2026-06-11T10:02:00Z",
      "updatedAt": "2026-06-11T10:02:00Z"
    }
  }
}
```

#### 3.3.5 错误

| Error | 类别 | 说明 |
|---|---|---|
| `NOT_SUPPORTED` | common | 设备不支持状态变更。 |
| `INVALID_ARGUMENT` | common | desiredState 或 endpoint 字段非法。 |
| `INVALID_STATE` | common | 当前状态不允许变更（如未纳管设备请求解绑、跳过 `pending` 直接设 `enrolled`）。 |
| `PERMISSION_DENIED` | common | 无权执行此操作。 |

#### 3.3.6 规则

- Request MUST 使用 `op=7`。
- Success / Error Response MUST 使用 `op=8`，并回显 Request 的 `d.id`。
- 失败响应 MUST NOT 携带业务 `result`。
- 状态实际变化后 SHOULD 触发 `device.enrollmentStateChanged`（op=6）；状态未变化时 MAY 成功返回且 MAY 不触发事件。
- 状态转换 MUST 符合 §3.4 状态机表；违反转换路径返回 `INVALID_STATE`。
- `desiredState: "enrolled"` 时 `endpoint` 必填且 `reason` 必填；`type = "room"` 时 `endpoint.profileId` 必填（见 §6.2 / §6.6 校验规则）。
- 草案阶段不得分配正式 methodId、bitOffset 或 fieldId。


### 3.4 Enrollment 状态机

#### 状态枚举定义

| 状态 | 含义 | 设备行为 |
|---|---|---|
| `unmanaged` | 未纳管，无 pairing code | 正常运行，不接收云端管理指令。 |
| `pairing_available` | 已获取 pairing code，等待用户输入 | 展示 pairing code 和倒计时；等待云端确认。 |
| `pending` | pairing code 已在云端提交，注册进行中 | 等待服务端确认或拒绝。 |
| `enrolled` | 注册成功，已绑定工作空间 | 完整管理功能可用；可接收云端配置下发。 |
| `failed` | 注册失败 | 展示错误信息；可重试或重置。 |
| `unenrolling` | 解绑进行中（异步清理） | 清理本地纳管数据；可能触发连接断开。 |

#### 有效状态转换

| From | To | 触发方法 / 原因 | 必填字段 | 备注 |
|---|---|---|---|---|
| `unmanaged` | `pairing_available` | `device.getPairingCode` 完成 | — | 设备首次获取 pairing code。 |
| `pairing_available` | `pending` | `device.setEnrollmentState(desiredState: "pending")` 或 pairing code 在云端提交 | — | 用户在云端输入 code；服务端通知设备。此转换可能由服务端内部触发而不通知设备（服务端内部直接跳到 `enrolled`），视具体实现而定。`[REVIEW-DRAFT]` |
| `pairing_available` | `unmanaged` | pairing code 过期未使用 | — | code TTL 到期；设备可重新获取。 |
| `pending` | `enrolled` | `device.setEnrollmentState(desiredState: "enrolled", reason: "pairing_code_used"/"server_claimed")` | `endpoint` 必填 | 注册成功；下发 endpoint 信息。 |
| `pending` | `failed` | `device.setEnrollmentState(desiredState: "failed")` | `message` 必填 | 注册失败（如工作空间配额不足）。 |
| `enrolled` | `unenrolling` | `device.setEnrollmentState(desiredState: "unmanaged", reason: "user_unenrolled"/"admin_reset")` | `reason` 推荐 | 发起解绑；`disconnectExpected` 可能为 `true`。 |
| `unenrolling` | `unmanaged` | 设备清理完成 | — | 异步完成；由设备内部触发。 |
| `failed` | `pending` | `device.setEnrollmentState(desiredState: "pending")` | — | 重试注册。 |
| `failed` | `unmanaged` | `device.setEnrollmentState(desiredState: "unmanaged")` | — | 放弃注册；重置设备。 |
| `enrolled` | `failed` | 服务端侧注册失效（如工作空间删除） | `message` 必填 | 罕见：服务端撤销或数据损坏。 |

> 无效转换（如 `unmanaged` → `enrolled` 跳过 `pending`）应返回 `INVALID_STATE`。

> **与 flow 对齐说明**：`workspace/flows/signage-device-management.md` Section 5 阶段3 序列图为可读性省略了 `pairing_available → pending` 中间状态，直接展示 `device.getPairingCode → device.setEnrollmentState(desiredState: "enrolled")`。实际状态转换以本节状态机为准；`pairing_available → pending` 转换可由服务端内部触发而不通知设备（服务端内部直接跳到 `enrolled`），见 §3.3（`desiredState: "pending"` 转换）与 Flow 7.1。

---

## 4. 事件

### 4.0 事件速览

| Event | 触发条件 | Payload Schema | 客户端处理建议 | 状态 |
|---|---|---|---|---|
| `device.enrollmentStateChanged` (`0x0102`) | pairing code 被使用、纳管成功、解绑、纳管失败或状态被服务端同步。 | `EnrollmentStateChangedEvent` | 更新纳管页面、设备列表和后续 room endpoint 操作门禁。 | adopted(scoped) |

> 已采纳（局部）。eventId `0x0102` / bitOffset `0`，device 域首个 event；payload `DeviceEnrollmentStateChangedEvent`（registry 名），见「采纳记录 A.1 / A.2」。

### 4.1 `device.enrollmentStateChanged`

**触发条件**：

- pairing code 被使用（注册成功）。
- 纳管成功 / 失败（服务端确认或拒绝）。
- 解绑（`desiredState: "unmanaged"` 触发 `unenrolling`）。
- 服务端同步状态（如 code 过期回退、服务端撤销注册）。

| 项 | 内容 |
|---|---|
| Payload Schema | `EnrollmentStateChangedEvent` |
| 客户端处理建议 | 状态变为 `enrolled` 且 `endpoint.type=room` 时，后续 room endpoint 操作才可进入 `room.info`。事件丢失时调用 `device.getEnrollmentState` 校准。 |

#### 4.1.1 Payload：`EnrollmentStateChangedEvent`

字段见 6.8。

| 字段 | 类型 | 必填 | 范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `state` | `EnrollmentInfo` | yes | see schema | none | 变化后的纳管状态。 |
| `previousState` | string | no | state enum value（字符串），非完整对象 | omitted | 变化前状态枚举值。 |
| `reason` | string | no | `"pairing_code_used"`, `"server_claimed"`, `"user_unenrolled"`, `"admin_reset"`, `"unknown"` | `"unknown"` | 变化原因。 |
| `triggerMethod` | string | no | `"setEnrollmentState"`, `"getPairingCode"`, `"server_sync"` | omitted | 触发此状态变化的操作类型。`"setEnrollmentState"` 为显式调用触发；`"server_sync"` 为服务端内部操作触发（如 code 过期导致状态回退、服务端撤销注册等）。`"getPairingCode"` 不直接触发此事件（见 3.1），保留用于 code 过期后状态回退的间接关联场景。`[REVIEW-DRAFT]` |
| `triggerId` | string | no | RPC request id | omitted | 触发操作的 RPC 请求 `id`。`triggerMethod` 为 `"server_sync"` 时此字段 omitted。`[REVIEW-DRAFT]` |

#### 4.1.2 d block 示例

```json
{
  "event": "device.enrollmentStateChanged",
  "intent": 1,
  "data": {
    "previousState": "pending",
    "reason": "pairing_code_used",
    "state": {
      "state": "enrolled",
      "deviceId": "device-7f3a2e1b",
      "workspaceId": "ws-2c8e1a90",
      "endpoint": {
        "endpointId": "room-9f4a2e1c",
        "type": "room",
        "displayName": "Boardroom A",
        "profileId": "<PROFILE_ID>"
      },
      "enrolledAt": "2026-06-11T10:02:00Z",
      "updatedAt": "2026-06-11T10:02:00Z"
    }
  }
}
```

读法：`previousState` 为变化前状态枚举值（非完整对象）；`state` 为变化后的完整 `EnrollmentInfo`。示例省略可选的 `triggerMethod` / `triggerId`；当状态变化由显式 RPC 调用驱动时，`triggerMethod` 为 `"setEnrollmentState"` 且 `triggerId` 匹配该请求 `id`；服务端内部驱动时 `triggerMethod` 为 `"server_sync"` 且 `triggerId` omitted。

#### 4.1.3 客户端处理建议

| 场景 | 建议 |
|---|---|
| `state` 变为 `enrolled` 且 `endpoint.type=room` | 后续 room endpoint 操作可进入 `room.info`；更新设备列表纳管状态 UI。 |
| `state` 变为 `unenrolling` | 预期可能断连（`disconnectExpected`）；UI 提示解绑进行中。 |
| `state` 变为 `failed` | 展示 `reason` / `message`；允许用户重试或重置。 |
| event 丢失或重连 | 重连后主动调用 `device.getEnrollmentState` 校准。 |
| 多端同时控制 | 以事件 `state` 为权威；冲突时以 `device.getEnrollmentState` 校准。 |

#### 4.1.4 规则

- Event MUST 使用 `op=6`。
- Event MUST NOT 携带 `d.id`。
- Event payload MUST 放在 `d.data` 中。
- `previousState` 为枚举值字符串，非完整 `EnrollmentInfo` 对象。
- `triggerMethod` 为 `"server_sync"` 时 MUST 省略 `triggerId`。
- 草案阶段不得分配正式 eventId 或 eventMasks bitOffset。


---

## 5. Capability

Capability name: `device.enrollment`（capability `0x0102`，schema `DeviceEnrollmentCapability`）。

| 字段 | 类型 | 必填 | 范围 / 枚举 | 说明 |
|---|---|---:|---|---|
| `supportsPairingCode` | boolean | no | `true` / `false` | 是否支持 pairing code。 |
| `pairingCodeTtlSeconds` | uint32 | no | `> 0` | 默认 code TTL（秒）。 |
| `supportsUnenroll` | boolean | no | `true` / `false` | 是否支持远程解绑或清除纳管状态。 |
| `endpointTypes` | string[] | no | `"room"`, `"device"`, `"asset"` | enrollment 后可能生成的 endpoint 类型。P0 仅需 `"room"`。`[REVIEW-DRAFT]` |
| `supportedPurposes` | string[] | no | `"initial_enrollment"`, `"re_enrollment"`, `"service_repair"` | 支持的 pairing code purpose 列表。省略表示仅支持 `"initial_enrollment"`。`[REVIEW-DRAFT]` |
| `maxActivePairingCodes` | uint32 | no | ≥ 1 | 同时有效的 pairing code 最大数量。默认 `1`。`[REVIEW-DRAFT]` |
| `pairingCodeLength` | uint32 | no | 6-8 | Pairing code 字符长度。默认 `6`。`[REVIEW-DRAFT]` |

---

## 6. Schemas

### 6.0 Schema 层级速览

```text
请求 Params
  GetPairingCodeParams              ← device.getPairingCode
  GetEnrollmentStateParams          ← device.getEnrollmentState
  SetEnrollmentStateParams          ← device.setEnrollmentState

响应 Results
  PairingCodeInfo                   ← device.getPairingCode result
  EnrollmentInfo                    ← device.getEnrollmentState result / SetEnrollmentStateResult.state
  SetEnrollmentStateResult          ← device.setEnrollmentState result

事件 Payload
  EnrollmentStateChangedEvent       ← device.enrollmentStateChanged

共享对象
  EnrollmentEndpointSummary         ← nested in EnrollmentInfo and SetEnrollmentStateParams
```

阅读规则：

- `EnrollmentInfo` 是核心状态快照，同时用于 `device.getEnrollmentState` 的 Result、`SetEnrollmentStateResult.state` 和 `EnrollmentStateChangedEvent.state`。
- `EnrollmentEndpointSummary` 为条件出现对象：`SetEnrollmentStateParams` 中 `desiredState: "enrolled"` 时必填；`EnrollmentInfo` 中仅 `state` 为 `enrolled` 或 `unenrolling` 时填充，且受 `GetEnrollmentStateParams.includeEndpoint` 控制。
- `PairingCodeInfo.state`（`available` / `expired` / `used` / `disabled`）描述 pairing code 自身生命周期，与 enrollment 状态机（Section 3.4）独立。
- 每个方法/事件小节保留内联字段表并标注 Section 6 交叉引用，schema 定义以 Section 6 为准。

### 6.1 `EnrollmentInfo`

| 字段 | 类型 | 必填 | 范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `state` | string | yes | `"unmanaged"`, `"pairing_available"`, `"pending"`, `"enrolled"`, `"failed"`, `"unenrolling"` | none | 当前纳管状态。状态定义和有效转换见 3.4。 |
| `deviceId` | string | no | opaque id | omitted | 服务端分配的设备标识符，跨 session 稳定。 |
| `workspaceId` | string | no | opaque id | omitted | 已纳管工作空间 ID。仅在 `state` 为 `enrolled` 或 `unenrolling` 时填充。隐私敏感字段。 |
| `endpoint` | `EnrollmentEndpointSummary` | no | see schema | omitted | 纳管后生成或绑定的后台 endpoint。仅在 `state` 为 `enrolled` 或 `unenrolling` 时填充。 |
| `enrolledAt` | string | no | RFC 3339 datetime | omitted | 注册完成时间（状态变为 `enrolled` 的时间）。仅在 `state` 为 `enrolled` 或 `unenrolling` 时存在。可用于审计。 `[REVIEW-DRAFT]` |
| `updatedAt` | string | no | RFC 3339 datetime | omitted | 状态最近更新时间。 |
| `message` | string | no | human-readable | omitted | `state` 为 `failed` 或 `pending` 时填充；`unenrolling` 时可选。 |

### 6.2 `EnrollmentEndpointSummary`

| 字段 | 类型 | 必填 | 范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `endpointId` | string | yes | opaque id | none | 后台 endpoint ID。 |
| `type` | string | yes | `"room"`, `"device"`, `"asset"` | none | endpoint 类型。`room.setName` 只允许 `type=room`。 |
| `displayName` | string | no | max length TBD | omitted | endpoint 显示名。 |
| `profileId` | string | no | opaque id | omitted | room profile 或业务 profile ID。当 `type = "room"` 时必填。 |
| `workspaceId` | string | no | opaque id | omitted | endpoint 所属工作空间 ID。与父级 `EnrollmentInfo.workspaceId` 重复时以此处为准。`[REVIEW-DRAFT]` |

> **校验规则**：
>
> - `type = "room"` 时 `profileId` 必填；其他 type 允许 omitted。
> - `displayName` non-empty 且 max length TBD `[REVIEW-ASK]`；前后空格应 trimmed。

### 6.3 `GetPairingCodeParams`

| 字段 | 类型 | 必填 | 范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `refresh` | boolean | no | `true` / `false` | `false` | 是否强制刷新 pairing code。 |
| `purpose` | string | no | `"initial_enrollment"`, `"re_enrollment"`, `"service_repair"` | `"initial_enrollment"` | code 使用场景。`"initial_enrollment"`：首次注册新设备；`"re_enrollment"`：重新注册（如工作空间迁移）；`"service_repair"`：维修配对，可能有不同 TTL 或权限。`[REVIEW-DRAFT]` 不支持的 purpose 返回 `NOT_SUPPORTED`。 |

### 6.4 `PairingCodeInfo`

| 字段 | 类型 | 必填 | 范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `code` | string | yes | 6-8 位大写字母数字，排除易混淆字符（0/O, 1/I/L） | none | 可展示或输入的 pairing code。 |
| `expiresAt` | string | no | RFC 3339 timestamp | omitted | 绝对过期时间。Legacy `GetBindCode` 响应中此字段为 Unix timestamp integer（例 `1234567890`）；AXTP 改为 RFC 3339 string。Adapter 需做类型转换。详见 9.1。 |
| `expiresInSeconds` | uint32 | no | `> 0` | omitted | 相对过期秒数。legacy device-sdk 实测值为 `1800`，不可省略。 |
| `state` | string | no | `"available"`, `"expired"`, `"used"`, `"disabled"` | `"available"` | code 当前状态。转换路径：`available` → `used`（code 被云端消费）、`available` → `expired`（TTL 到期）、`available` → `disabled`（服务端撤销）。终态：`used`、`expired`、`disabled`。 |

> **时间戳优先级**：当 `expiresAt` 和 `expiresInSeconds` 同时存在时，`expiresAt` 为权威时间；`expiresInSeconds` 保留用于 legacy device-sdk 兼容（实测值 1800）。客户端应优先使用 `expiresAt`。

> **校验规则**：
>
> - `code` 必须匹配 pattern `[A-HJ-NP-Z2-9]{6,8}`（6-8 位大写字母数字，排除易混淆字符 0/O、1/I/L）。
> - `expiresInSeconds` 存在时必须 `> 0`。
> - `expiresAt` 与 `expiresInSeconds` 可同时存在；同时存在时 `expiresAt` 为权威时间。服务端 SHOULD 同时返回两者以兼容 legacy device-sdk。

### 6.5 `GetEnrollmentStateParams`

| 字段 | 类型 | 必填 | 范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `includeEndpoint` | boolean | no | `true` / `false` | `true` | 是否返回 enrollment 后的 endpoint 摘要。默认 `true` 因为最常见调用方（云端管理后台）需要 endpoint 信息；轻量轮询或已缓存 endpoint 时设为 `false`。 |

### 6.6 `SetEnrollmentStateParams`

| 字段 | 类型 | 必填 | 范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `desiredState` | string | yes | `"enrolled"`, `"unmanaged"`, `"failed"`, `"pending"` | none | 目标状态。 |
| `reason` | string | no | `"pairing_code_used"`, `"server_claimed"`, `"user_unenrolled"`, `"admin_reset"`, `"unknown"` | `"unknown"` | 状态变化原因。 |
| `endpoint` | `EnrollmentEndpointSummary` | no | see schema | omitted | enrollment 成功后关联的 endpoint 摘要。 |
| `message` | string | no | human-readable | omitted | 失败、解绑或修复说明。 |

> **校验规则**：
>
> - `desiredState: "enrolled"` — 仅从 `pending` 转入；`endpoint` 必填且须为合法 `EnrollmentEndpointSummary`；`reason` 必填（`"pairing_code_used"` 或 `"server_claimed"`）。
> - `desiredState: "unmanaged"` — 从 `enrolled`、`failed` 或 `pairing_available` 转入；`reason` 推荐填写。
> - `desiredState: "failed"` — 仅从 `pending` 转入；`message` 必填。
> - `desiredState: "pending"` — 从 `failed`（重试）或 `pairing_available`（code 已提交）转入。
> - `endpoint` 当 `endpoint.type = "room"` 时，`endpoint.profileId` 必填（见 6.2 校验规则）。
> - 状态转换必须符合 Section 3.4 状态机表；违反转换路径（如从 `unmanaged` 直接设 `"enrolled"`）返回 `INVALID_STATE`。

### 6.7 `SetEnrollmentStateResult`

| 字段 | 类型 | 必填 | 范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `state` | `EnrollmentInfo` | yes | see schema | none | 操作后的纳管状态。 |
| `disconnectExpected` | boolean | no | `true` / `false` | `false` | 解绑或重置是否预期导致连接变化。`true` 仅在 `desiredState = "unmanaged"` 且设备需要关闭管理 session 时。其余转换均为 `false`。 |

### 6.8 `EnrollmentStateChangedEvent`

| 字段 | 类型 | 必填 | 范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `state` | `EnrollmentInfo` | yes | see schema | none | 变化后的纳管状态。 |
| `previousState` | string | no | state enum value（字符串），非完整对象 | omitted | 变化前状态枚举值。 |
| `reason` | string | no | `"pairing_code_used"`, `"server_claimed"`, `"user_unenrolled"`, `"admin_reset"`, `"unknown"` | `"unknown"` | 变化原因。 |
| `triggerMethod` | string | no | `"setEnrollmentState"`, `"getPairingCode"`, `"server_sync"` | omitted | 触发此状态变化的操作类型。`"setEnrollmentState"` 为显式调用触发；`"server_sync"` 为服务端内部操作触发（如 code 过期导致状态回退、服务端撤销注册等）。`"getPairingCode"` 不直接触发此事件（见 3.1），保留用于 code 过期后状态回退的间接关联场景。`[REVIEW-DRAFT]` |
| `triggerId` | string | no | RPC request id | omitted | 触发操作的 RPC 请求 `id`。`triggerMethod` 为 `"server_sync"` 时此字段 omitted。`[REVIEW-DRAFT]` |

---

## 7. 交互流程示例 Flow Examples

本章只展示多个 method/event 组成的端到端业务流程。单个 method 的 Request / Success Response / Error Response 示例见第 3 章；单个 event 的 Event 示例见第 4 章。每个 flow 引用 §3/§4 的 `d` block，点明调用顺序与状态变化。

### 7.1 场景：首次注册（unmanaged → enrolled）

设备首次纳管的完整流程，对应 `workspace/flows/signage-device-management.md` 阶段 3。

#### Step 1. device.getPairingCode：请求 (op=7)

设备向云端请求注册码（Device → Server），状态 `unmanaged` → `pairing_available`：

```json
{
  "id": 1,
  "method": "device.getPairingCode",
  "params": {
    "refresh": false,
    "purpose": "initial_enrollment"
  }
}
```

设备收到 code 后展示给用户（完整成功响应见 §3.1.3）。

#### Step 2. 用户在云端管理系统输入注册码

非协议步骤：用户读取设备展示的 code，在云端管理后台提交。服务端验证 code 后开始处理注册。

#### Step 3. device.setEnrollmentState(pending)：请求 (op=7)

服务端通知设备 code 已提交、注册进行中（Server → Device）。状态 `pairing_available` → `pending`：

```json
{
  "id": 8,
  "method": "device.setEnrollmentState",
  "params": {
    "desiredState": "pending"
  }
}
```

> 此中间转换可能由服务端内部触发而不通知设备（服务端内部直接从 `pairing_available` 跳到 `enrolled`），视实现而定（`[REVIEW-DRAFT]`，见 §3.4）。

#### Step 4. device.setEnrollmentState(enrolled)：请求 (op=7)

注册成功，服务端下发 endpoint（Server → Device）。状态 `pending` → `enrolled`：

```json
{
  "id": 3,
  "method": "device.setEnrollmentState",
  "params": {
    "desiredState": "enrolled",
    "reason": "pairing_code_used",
    "endpoint": {
      "endpointId": "room-9f4a2e1c",
      "type": "room",
      "displayName": "Boardroom A",
      "profileId": "<PROFILE_ID>"
    }
  }
}
```

完整成功响应见 §3.3.3。

#### Step 5. device.enrollmentStateChanged：事件 (op=6)

状态变化触发事件：

```json
{
  "event": "device.enrollmentStateChanged",
  "intent": 1,
  "data": {
    "previousState": "pending",
    "reason": "pairing_code_used",
    "state": {
      "state": "enrolled",
      "deviceId": "device-7f3a2e1b",
      "workspaceId": "ws-2c8e1a90",
      "endpoint": {
        "endpointId": "room-9f4a2e1c",
        "type": "room",
        "displayName": "Boardroom A",
        "profileId": "<PROFILE_ID>"
      },
      "enrolledAt": "2026-06-11T10:02:00Z",
      "updatedAt": "2026-06-11T10:02:00Z"
    }
  }
}
```

读法：客户端收到事件后更新纳管状态 UI；`state` 为 `enrolled` 且 `endpoint.type=room` 时后续 room endpoint 操作可进入 `room.info`。客户端也可在 Step 4 响应后直接用 `result.state` 更新，事件作为异步确认。跳过 `pending` 直接从 `unmanaged` 设 `enrolled` 会返回 `INVALID_STATE`（见 §3.3.3 error 读法）。

### 7.2 场景：解绑（enrolled → unmanaged）

管理员将设备从工作空间解绑，对应"设备重置 / 工作空间迁移"场景。

#### Step 1. device.setEnrollmentState(unmanaged)：请求 (op=7)

```json
{
  "id": 5,
  "method": "device.setEnrollmentState",
  "params": {
    "desiredState": "unmanaged",
    "reason": "admin_reset",
    "message": "Device reassigned to different workspace."
  }
}
```

#### Step 2. 成功响应 (op=8)

```json
{
  "id": 5,
  "status": {
    "ok": true,
    "code": 0
  },
  "result": {
    "state": {
      "state": "unenrolling",
      "deviceId": "device-7f3a2e1b",
      "message": "Device reassigned to different workspace.",
      "updatedAt": "2026-06-11T12:00:00Z"
    },
    "disconnectExpected": true
  }
}
```

#### Step 3. device.enrollmentStateChanged：事件 (op=6)

设备异步清理完成后状态由 `unenrolling` → `unmanaged`，触发事件（`previousState: "unenrolling"`、`state.state: "unmanaged"`）。

读法：`disconnectExpected: true` 表示设备完成清理后可能断开当前管理连接，客户端应预期重连。解绑**不隐式清除**播放列表 / 网络配置 / 软件配置（跨域副作用），需显式调用对应域（`signage.playlist` / `network.*` / `software.config`）。权限不足时返回 `PERMISSION_DENIED`（code 9，见 §3.3.3）。

### 7.3 场景：重连后状态校准

事件丢失或设备重连后，客户端通过 query 校准当前纳管状态。

#### Step 1. device.getEnrollmentState：请求 (op=7)

```json
{
  "id": 2,
  "method": "device.getEnrollmentState",
  "params": {
    "includeEndpoint": true
  }
}
```

#### Step 2. 成功响应 (op=8)

已纳管设备返回完整状态（未纳管返回 `state: "unmanaged"`，见 §3.2.3）：

```json
{
  "id": 2,
  "status": {
    "ok": true,
    "code": 0
  },
  "result": {
    "state": "enrolled",
    "deviceId": "device-7f3a2e1b",
    "workspaceId": "ws-2c8e1a90",
    "endpoint": {
      "endpointId": "room-9f4a2e1c",
      "type": "room",
      "displayName": "Boardroom A",
      "profileId": "<PROFILE_ID>"
    },
    "enrolledAt": "2026-06-10T08:30:00Z",
    "updatedAt": "2026-06-11T10:02:00Z"
  }
}
```

读法：客户端重连或 `device.enrollmentStateChanged` 事件丢失时，主动调用 `device.getEnrollmentState` 以 `state` 为权威校准本地缓存与 UI。轻量轮询可设 `includeEndpoint: false` 省略 endpoint（见 §3.2.3 读法）。

---

## 8. 候选 Errors

| Error | 复用 / 候选 | 说明 | Review |
|---|---|---|---|
| `NOT_SUPPORTED` | common (0x0003) | 设备或服务端不支持 pairing code 或状态变更。 | — |
| `INVALID_ARGUMENT` | common (0x000A) | desiredState 或 endpoint 字段非法。 | adopted |
| `INVALID_STATE` | common (0x0004) | 当前状态不允许变更（如未纳管设备请求解绑）。 | adopted |
| `PERMISSION_DENIED` | common (0x0009) | 无权生成 code、纳管或解绑。 | adopted |
| `INTERNAL_ERROR` | common (0x000E) | 服务端内部错误（如无法生成 pairing code）。 | adopted |
| `ENROLLMENT_CODE_EXPIRED` | device (0x010A) | pairing code 已过期。 | adopted |
| `ENROLLMENT_CODE_ALREADY_USED` | device (0x010B) | pairing code 已使用。 | adopted |

> 通用错误码数值取自 `registry/error/error_code.yaml`。业务域错误 `ENROLLMENT_CODE_EXPIRED` (`0x010A`) / `ENROLLMENT_CODE_ALREADY_USED` (`0x010B`) 已新增写入设备域区段 `0x0100–0x01FF`，连号追加在已占用区末尾 `0x0109` 之后（见「采纳记录 A.3」）。

---

## 9. Legacy 待映射

### 9.0 Legacy 映射总览

| Legacy entry | 证据源 / 状态 | Direction | AXTP target | 说明 |
|---|---|---|---|---|
| `GetBindCode` | 通用管理命令 · 已研发 | Device -> Server | `device.getPairingCode` | 逐字段映射见 9.1。 |
| `GetBindConfig` | 通用管理命令 · 已研发 | Server <-> Device | `device.getEnrollmentState` | 逐字段映射见 9.2。 |
| `SetBindConfig` | 通用管理命令 · 已研发 | Server -> Device | `device.setEnrollmentState` | 逐字段映射见 9.3。 |
| `OnBindState` | 通用管理命令 · 未研发 | Device -> Server | `device.enrollmentStateChanged` | 逐字段映射见 9.4。 |

> **证据源文件**：`workspace/legacy-migration/evidence/NearHub-Launcher数字标牌设备管理通用管理命令.md`（主要）和 `workspace/legacy-migration/evidence/NearHub-Launcher设备管理命令.md`（TypeScript 类型定义）。

### 9.1 `GetBindCode` → `device.getPairingCode`

Legacy Device → Server，请求无参数，返回 `{ code, expiresAt, expiresInSeconds }`。

**请求参数映射**：

| Legacy 字段 | 类型 | AXTP 字段 | 类型 | 转换说明 |
|---|---|---|---|---|
| *(none)* | — | `refresh` | boolean | AXTP 新增。Legacy 无此参数，Adapter 不传（默认 `false`）。 |
| *(none)* | — | `purpose` | string | AXTP 新增。Legacy 无此参数，Adapter 不传（默认 `"initial_enrollment"`）。 |

**响应结果映射**：

| Legacy 字段 | 类型 | AXTP 字段 | 类型 | 转换说明 |
|---|---|---|---|---|
| `code` | string | `code` | string | 直传。AXTP 增加 format 约束（6-8 位大写字母数字，排除易混淆字符）。 |
| `expiresAt` | number (Unix timestamp) | `expiresAt` | string (RFC 3339) | **类型变更**。Adapter 必须将 Unix timestamp integer 转换为 RFC 3339 string。例：`1234567890` → `"2009-02-13T23:31:30Z"`。 |
| `expiresInSeconds` | number | `expiresInSeconds` | uint32 | 直传。Legacy 实测值 `1800`，不可省略。`[REVIEW-DRAFT]` TypeScript 类型定义 (`contract.ts`) 中未声明此字段，但 legacy 实测响应包含；以实测为准。 |
| *(none)* | — | `state` | string | AXTP 新增。Legacy 无对应。Adapter 从 legacy 响应推断：code 有效时为 `"available"`。 |

> **Adapter 说明**：Adapter 需处理 `expiresAt` 的类型转换（integer → RFC 3339 string）。`state` 字段无 legacy 对应，从 legacy 响应推断默认值 `"available"`。

**Adapter 转换示例**（GetBindCode 响应方向）：

Legacy 响应：
```json
{ "code": "ABC123", "expiresAt": 1234567890, "expiresInSeconds": 1800 }
```

AXTP 转换结果：
```json
{ "code": "ABC123", "expiresAt": "2009-02-13T23:31:30Z", "expiresInSeconds": 1800, "state": "available" }
```

转换步骤：`expiresAt` 从 Unix timestamp integer 转为 RFC 3339 string；`expiresInSeconds` 直传；`state` 从响应有效性推断为 `"available"`。

### 9.2 `GetBindConfig` → `device.getEnrollmentState`

Legacy Server ↔ Device，请求无参数，返回 `{ bound: boolean }`。

**请求参数映射**：

| Legacy 字段 | 类型 | AXTP 字段 | 类型 | 转换说明 |
|---|---|---|---|---|
| *(none)* | — | `includeEndpoint` | boolean | AXTP 新增。Legacy 无此参数，Adapter 不传（默认 `true`）。 |

**响应结果映射**：

| Legacy 字段 | 类型 | AXTP 字段 | 类型 | 转换说明 |
|---|---|---|---|---|
| `bound` | boolean | `state` | string (enum) | **语义丰富化**。`bound: true` → `"enrolled"`；`bound: false` → `"unmanaged"`。Legacy 二值映射到 AXTP 6-state enum 的两个基础值。 |
| *(none)* | — | `deviceId` | string | AXTP 新增。服务端分配的跨 session 稳定标识符。 |
| *(none)* | — | `workspaceId` | string | AXTP 新增。已纳管工作空间 ID。隐私敏感。 |
| *(none)* | — | `endpoint` | `EnrollmentEndpointSummary` | AXTP 新增。纳管后绑定或生成的 endpoint 摘要。`includeEndpoint: true` 时填充。 |
| *(none)* | — | `enrolledAt` | string (RFC 3339) | AXTP 新增审计字段。注册完成时间。 |
| *(none)* | — | `updatedAt` | string (RFC 3339) | AXTP 新增。状态最近更新时间。 |
| *(none)* | — | `message` | string | AXTP 新增。`failed` 或 `pending` 时填充。 |

> **Adapter 说明**：`bound: true/false` 是 AXTP 6-state enum 的无损子集映射。正向转换明确（`true` → `"enrolled"`，`false` → `"unmanaged"`），但中间状态（`pairing_available`、`pending`、`failed`、`unenrolling`）在 legacy 接口无法表达。AXTP adapter 可缓存或查询额外状态来填充这些值。

### 9.3 `SetBindConfig` → `device.setEnrollmentState`

Legacy Server → Device，请求 `{ bound: boolean }`，返回 `{ ok: true }`。

**请求参数映射**：

| Legacy 字段 | 类型 | AXTP 字段 | 类型 | 转换说明 |
|---|---|---|---|---|
| `bound` | boolean | `desiredState` | string | **方向变更**。`bound: true` → `desiredState: "enrolled"`；`bound: false` → `desiredState: "unmanaged"`。Legacy 是简单 boolean toggle，AXTP 是状态机条件转换。 |
| *(none)* | — | `reason` | string | AXTP 条件必填（`desiredState: "enrolled"` 时必填）。Adapter 需提供默认 reason（推荐 `"server_claimed"`）。 |
| *(none)* | — | `endpoint` | `EnrollmentEndpointSummary` | AXTP 条件必填（`desiredState: "enrolled"` 时必填）。Adapter 需从服务端上下文获取 endpoint 信息。 |
| *(none)* | — | `message` | string | AXTP 可选。Adapter 不传。 |

**响应结果映射**：

| Legacy 字段 | 类型 | AXTP 字段 | 类型 | 转换说明 |
|---|---|---|---|---|
| `ok` | boolean | `state` | `EnrollmentInfo` | **结构变更**。Legacy 返回 `{ ok: true }`；AXTP 返回标准 success status + 完整 `EnrollmentInfo` 对象。 |
| *(none)* | — | `disconnectExpected` | boolean | AXTP 新增。仅 `desiredState: "unmanaged"` 时可能为 `true`。 |

> **Adapter 说明**：`bound: true` 时 adapter 需合成 `reason`（推荐 `"server_claimed"`）和 `endpoint` 对象（从服务端数据库/工作空间信息获取）。`bound: false` 时 adapter 需合成 `reason`（推荐 `"admin_reset"` 或 `"user_unenrolled"`）。Legacy `{ ok: true }` 映射到 AXTP 标准 success status（`status: { ok: true, code: 0 }`）；typed `SetEnrollmentStateResult` 需 adapter 从服务端状态构造。

**Adapter 转换示例**（SetBindConfig 请求方向）：

Legacy 请求：
```json
{ "bound": true }
```

AXTP 转换结果：
```json
{
  "desiredState": "enrolled",
  "reason": "server_claimed",
  "endpoint": {
    "endpointId": "room-9f4a2e1c",
    "type": "room",
    "displayName": "Boardroom A",
    "profileId": "<PROFILE_ID>"
  }
}
```

转换步骤：`bound: true` → `desiredState: "enrolled"`；`reason` 从服务端上下文合成（推荐 `"server_claimed"`）；`endpoint` 从服务端工作空间/房间信息获取。Adapter 需要访问服务端数据库来填充 `endpoint` 对象。

### 9.4 `OnBindState` → `device.enrollmentStateChanged`

Legacy Device → Server（**未研发**），事件 data `{ status, code, message }`。AXTP 事件为新设计。

**事件 payload 映射**：

| Legacy 字段 | 类型 | AXTP 字段 | 类型 | 转换说明 |
|---|---|---|---|---|
| `status` | `"success"` / `"failed"` | `state`（via `EnrollmentInfo`） | string (enum) | **间接映射**。`status: "success"` → `state: "enrolled"`；`status: "failed"` → `state: "failed"`。Event payload 从 flat 字段变为嵌套 `EnrollmentInfo` 结构。 |
| `code` | string | *(无直接映射)* | — | Legacy 携带 pairing code。AXTP event 不直接包含 code（code 状态通过 `PairingCodeInfo.state` 追踪）。 |
| `message` | string | `reason` / `state.message` | string | AXTP `reason` 使用结构化枚举值（如 `"pairing_code_used"`）；自由文本 `message` 移入 `EnrollmentInfo.message`。 |
| *(none)* | — | `previousState` | string | AXTP 新增。变化前状态枚举值。 |
| *(none)* | — | `triggerMethod` | string | AXTP 新增。触发状态变化的操作类型。 |
| *(none)* | — | `triggerId` | string | AXTP 新增。触发操作的 RPC 请求 `id`。 |

> **Adapter 说明**：Legacy 事件未研发（未发货），AXTP 事件 `device.enrollmentStateChanged` 为全新设计。无需向后兼容 adapter。generated legacy 映射文件中 `OnBindState` 指向 `setbindconfig.bindState` / `events.bindState`，采纳时应更新为 `device.enrollmentStateChanged`（见 9.6）。

### 9.5 Adapter 通用转换模式

以下转换模式在多个 legacy 命令中共享：

| 模式 | 说明 | 涉及命令 |
|---|---|---|
| **Boolean → enum** | Legacy `bound: true/false` 映射到 AXTP `state` 枚举。正向明确（`true` → `"enrolled"`，`false` → `"unmanaged"`）。反向丢失中间状态。 | `GetBindConfig`，`SetBindConfig` |
| **Timestamp 类型转换** | Legacy `expiresAt` 为 Unix timestamp integer（例 `1234567890`）。AXTP `expiresAt` / `updatedAt` / `enrolledAt` 为 RFC 3339 string（例 `"2009-02-13T23:31:30Z"`）。Adapter 必须在请求/响应双方向做类型转换。 | `GetBindCode` |
| **`{ ok: true }` 响应模式** | Legacy Set 指令统一返回 `{ "ok": true }`。AXTP 使用标准 success status（`status: { ok: true, code: 0 }`）加 typed result 对象。Adapter 将 `ok: true` 映射为 AXTP success status，并从服务端状态合成 typed result。 | `SetBindConfig` |
| **必填字段合成** | `SetBindConfig` → `device.setEnrollmentState` 要求 adapter 提供 `reason` 和 `endpoint` 字段，legacy 无对应。这些字段需从服务端上下文（数据库、工作空间信息）获取。 | `SetBindConfig` |

### 9.6 Generated Legacy 文件交叉引用

以下 generated 文件当前使用旧 domain `binding`（而非 `device.enrollment`），采纳阶段需同步更新。

**`workspace/legacy-migration/generated/legacy-to-axtp-map.generated.yaml`** 当前映射：

| Legacy entry | 当前 target method | 应更新为 | Status |
|---|---|---|---|
| `GetBindCode` | `binding.getCode` | `device.getPairingCode` | compat → 需更新 |
| `GetBindConfig` | `binding.getConfig` | `device.getEnrollmentState` | draft → 需更新 |
| `SetBindConfig` | `binding.setConfig` | `device.setEnrollmentState` | draft → 需更新 |
| `OnBindState` | `setbindconfig.bindState` / `events.bindState` | `device.enrollmentStateChanged` | 需更新 |

**`workspace/legacy-migration/generated/registry-patches.generated.yaml`** 当前条目：

| 当前 ID | 当前名称 | 当前 domain | 应更新为 |
|---|---|---|---|
| Method `0x1301` | `binding.getCode` | `binding` | `device.getPairingCode`（domain: `device`） |
| Method `0x1302` | `binding.getConfig` | `binding` | `device.getEnrollmentState`（domain: `device`） |
| Method `0x1303` | `binding.setConfig` | `binding` | `device.setEnrollmentState`（domain: `device`） |
| Capability `0x1301` | `binding.code` | `binding` | `device.enrollment` |
| Capability `0x1302` | `binding.config` | `binding` | 合并入 `device.enrollment` |

> Schema 名称（`BindingGetCodeRequest` / `BindingGetCodeResponse` / `BindingGetConfigRequest` / `BindingGetConfigResponse` / `BindingSetConfigRequest` / `BindingSetConfigResponse`）和 Adapter 名称（`GetbindcodeAdapter` / `GetbindconfigAdapter` / `SetbindconfigAdapter`）也需在采纳阶段更新为与 AXTP schema 命名一致。
>
> **注意**：以上 generated 文件不在本草案阶段修改。更新发生在 `adopt-protocol-draft`（Stage 30）阶段，当 registry YAML 写入并 Generator 重跑后自动生效。本 section 9 的映射信息作为采纳阶段的输入参考。

---

## 10. Registry / Conformance Status

| 项 | 状态 |
|---|---|
| Registry YAML | written（局部采纳 / scoped，见「采纳记录」） |
| Generated docs | not generated（待 Stage 50 `generate-axtp-protocol` 重生成） |
| Method / event IDs | 已分配：method `0x0102`/`0x0103`/`0x0104`、event `0x0102`、capability `0x0102` |
| Conformance | 需覆盖 pairing code TTL、状态查询、状态变更事件、解绑权限、legacy `expiresInSeconds`（待 generated 后补 conformance cases）。 |

> 完整采纳检查清单（12 项，含 `getPairingCode` 命名偏好、`state` 枚举 P0 范围、`workspaceId` / `endpointId` 隐私策略、`binding` → `device.enrollment` generated 文件更新方案等）见 **附录 D**，本文以附录 D 为唯一权威源。

---

## 11. Test Notes

- `device.getPairingCode` 返回 `code`、`expiresAt`、`expiresInSeconds`。`expiresInSeconds` 不可省略。
- `device.getEnrollmentState` 未纳管时返回 `state: "unmanaged"`；已纳管时返回 `state: "enrolled"` + endpoint。
- `device.setEnrollmentState(desiredState: "enrolled")` 后触发 `device.enrollmentStateChanged`。
- 未 enrolled 时调用需要 room endpoint 的协议应被拒绝或隐藏。
- 解绑权限不足返回 `PERMISSION_DENIED`。
- `refresh=true` 应使旧 code 失效并返回新 code。
- **状态转换校验**：`setEnrollmentState(desiredState: "enrolled")` 从 `unmanaged` 直接调用应返回 `INVALID_STATE`（必须经由 `pending`）。
- **`enrolledAt` 存在条件**：仅在 `state` 为 `enrolled` 或 `unenrolling` 时返回；其他状态不包含此字段。
- **`refresh=true` 使旧 code 失效**：连续两次 `getPairingCode(refresh: true)` 应返回不同 code，第一次的 code 不再可用。
- **`triggerId` 关联**：事件 `triggerId` 应匹配触发操作的 RPC 请求 `id`。
- **`disconnectExpected` 条件**：仅 `desiredState: "unmanaged"` 时可能为 `true`；其他转换均为 `false`。
- **Conformance case**：采纳后应为每个 method 和 state transition 创建至少一个 conformance case。

---

## 12. 待确认问题

| Issue | Impact | Current recommendation | Status |
|---|---|---|---|
| pairing code 是设备主动拉取，还是服务端预生成后由设备展示？ | 方法方向和缓存策略 | 保留 Device → Server current evidence，同时允许服务端 handler 生成。 | `[REVIEW-ASK]` |
| `workspaceId` / `endpointId` 是否都能暴露给设备？ | schema / privacy | 先作为 optional 摘要字段，采纳前与产品和安全确认。 | `[REVIEW-ASK]` |
| 解绑是否清除本地播放列表、网络配置或软件配置？ | 跨域副作用 | 不在 `device.enrollment` 内隐式清除；需要显式调用对应域。 | `[REVIEW-DRAFT]` |
| 状态枚举首批值 | schema / conformance | v0.2 已包含全部 6 个枚举值和状态机转换规则；是否全部纳入 P0 仍需产品和架构确认。 | `[REVIEW-DRAFT]` |
| `device.enrollment` 是否需要 `device.revokeEnrollment` 方法？ | method 完整性 | 当前用 `setEnrollmentState(desiredState: "unmanaged")` 替代；如果需要独立权限或流程则另建方法。 | `[REVIEW-ASK]` |
| `desiredState: "pending"` 方向确认 | 方法方向 | 当前设计为 Server → Device（服务端通知设备 code 已提交）；设备是否可自行转入 pending？ | `[REVIEW-ASK]` |
| `EnrollmentEndpointSummary.displayName` 最大长度 | schema | 6.2 校验规则标注 `max length TBD`；影响客户端 UI 布局和设备存储。采纳前需产品确认。 | `[REVIEW-ASK]` |

---

## 附录 A. 协议审核标记

| 标记 | 条目 | 审核结论 | 后续动作 |
|---|---|---|---|
| `[REVIEW-ADOPTED]` | `device.enrollment` capability | 本文已于 2026-06-17 经 Stage 30 局部采纳，machine 事实源落地为 `registry/domains/device/domain.yaml` + `registry/error/error_code.yaml`；替代已删除的 `device.binding` 草案。 | scoped 待确认项走 `amend-adopted-protocol`（见「采纳记录 A.4 / A.5」）。 |
| `[REVIEW-RESOLVED]` | 域命名 `device.enrollment` | `device.enrollment` 比 `device.binding` 更准确描述"未入管设备成为后台管理对象"的语义。 | — |
| `[REVIEW-RESOLVED]` | 方法命名 `getPairingCode` | 草案方法名为 `device.getPairingCode`（非 `device.getEnrollmentCode`），消解 flow 第 296 行 Open Question 的命名歧义。 | 待产品最终确认"pairing code" vs "enrollment code"命名偏好；采纳前统一。 |
| `[REVIEW-RESOLVED]` | Legacy 映射 | 9.0–9.6 已补齐 4 个 legacy 命令详细字段映射、adapter 转换示例和 generated 文件交叉引用。domain 从 `binding` 改为 `device.enrollment` 已在 9.6 标注。 | Stage 30 采纳时同步更新 `legacy-to-axtp-map.generated.yaml` 和 `registry-patches.generated.yaml` 中的 domain 名称（`binding` → `device.enrollment`）。 |
| `[REVIEW-ASK]` | pairing code 生成方向 | 方法方向已确认 Device → Server（设备主动拉取），同时允许服务端 handler 生成。缓存策略待确认。 | scoped 采纳，默认值见「采纳记录 A.4」；缓存策略确认后走 amend。 |
| `[REVIEW-ASK]` | `workspaceId` / `endpointId` 隐私 | 这两个 ID 是否都能暴露给设备尚未确认，当前作为 optional 摘要字段。 | scoped 采纳（`required: false` 写入），确认后走 amend，见「采纳记录 A.4」。 |
| `[REVIEW-ASK]` | `EnrollmentEndpointSummary.displayName` 最大长度 | 6.2 校验规则标注 `max length TBD`；影响客户端 UI 布局和设备存储。 | scoped 采纳（`max_length: 128`，对齐 `device.info` `product.displayName`），见「采纳记录 A.4」。 |
| `[REVIEW-ASK]` | `desiredState: "pending"` 方向 | 当前设计为 Server → Device（服务端通知设备 code 已提交）。设备是否可自行转入 pending 尚未确认。 | scoped 采纳（维持 Server → Device），确认后走 amend，见「采纳记录 A.4」。 |
| `[REVIEW-ASK]` | 是否需要独立 `device.revokeEnrollment` 方法 | 当前用 `setEnrollmentState(desiredState: "unmanaged")` 替代解绑。 | scoped 采纳（暂不另建方法），见「采纳记录 A.4」。 |

## 附录 B. 协议决策

| 决策点 | 结论 | 理由 |
|---|---|---|
| 新增/修改/复用 | Modify | 扩展现有 v0.x 草案；feature 边界正确，仅按 skill 规范补齐附录结构。 |
| 域与命名 | `device.enrollment`（替代已删 `device.binding`） | `enrollment` 比 `binding` 更准确描述"未入管设备成为后台管理对象"的语义。 |
| 方法命名 | `device.getPairingCode`（非 `getEnrollmentCode`） | "pairing code" 是现场纳管场景的通用术语；待产品最终确认命名偏好。 |
| 控制面 | RPC method/event | 业务控制不进入 Frame Header；`PayloadType` 不编码业务语义。 |
| 数据面 | None（不使用 STREAM） | 所有交互通过 RPC method/event 完成，无连续数据传输。 |
| WebSocket | RPC-only | `AXTP-WS-CLOUD-REVERSE` / `AXTP-WS-JSON` 不承载 STREAM。 |
| 状态枚举 | `EnrollmentInfo.state` 6 值（`unmanaged`/`pairing_available`/`pending`/`enrolled`/`failed`/`unenrolling`）+ Section 3.4 状态机 | 覆盖 legacy `bound`(bool) 之外的全部中间状态；无效转换返回 `INVALID_STATE`。 |
| `PairingCodeInfo` 状态 | `available`/`expired`/`used`/`disabled` | 描述 pairing code 自身生命周期，与 enrollment 状态机独立。 |
| 解绑跨域副作用 | 不在 `device.enrollment` 内隐式清除播放列表/网络/软件配置 | 需显式调用对应域（`signage.playlist` / `network.*` / `software.config`）；避免隐式破坏。 |
| `expiresInSeconds` 保留 | 必填保留，legacy 实测值 `1800` | legacy device-sdk 实测响应包含此字段（`src/sdk.spec.ts` 断言差异暴露），不可省略。 |
| 时间戳类型 | Unix timestamp integer → RFC 3339 string（`expiresAt`/`updatedAt`/`enrolledAt`） | 对齐 AXTP 时间戳规范；Adapter 在请求/响应双方向做类型转换。 |
| `expiresAt` 与 `expiresInSeconds` 优先级 | 同时存在时 `expiresAt` 为权威 | `expiresInSeconds` 保留用于 legacy device-sdk 兼容。 |
| endpoint 校验 | `type = "room"` 时 `endpoint.profileId` 必填 | `room.setName` 等操作要求 room profile；`room.setName` 只允许 `type=room`。 |
| 文档结构 | v0.6 补附录 A–D | 对齐 20-draft-business-protocol skill 规范与 `signage.playlist` / `device.info` 等成熟草案结构。 |

## 附录 C. Registry 草案输入

采纳本文后，`registry/domains/device/domain.yaml` 至少应包含（所有 numeric ID 为 `TBD after adoption`，不在此分配）：

```yaml
capabilities:
  - name: device.enrollment
    status: draft

methods:
  - name: device.getPairingCode
    id: TBD after adoption
    bitOffset: TBD after adoption
    rpc_op: request_response
    requestSchema: GetPairingCodeParams
    responseSchema: PairingCodeInfo
    capabilities:
      - device.enrollment
    errors:
      - NOT_SUPPORTED
      - PERMISSION_DENIED
      - INTERNAL_ERROR
  - name: device.getEnrollmentState
    id: TBD after adoption
    bitOffset: TBD after adoption
    rpc_op: request_response
    requestSchema: GetEnrollmentStateParams
    responseSchema: EnrollmentInfo
    capabilities:
      - device.enrollment
    errors:
      - NOT_SUPPORTED
  - name: device.setEnrollmentState
    id: TBD after adoption
    bitOffset: TBD after adoption
    rpc_op: request_response
    requestSchema: SetEnrollmentStateParams
    responseSchema: SetEnrollmentStateResult
    capabilities:
      - device.enrollment
    events:
      - device.enrollmentStateChanged
    errors:
      - NOT_SUPPORTED
      - INVALID_ARGUMENT
      - INVALID_STATE
      - PERMISSION_DENIED

events:
  - name: device.enrollmentStateChanged
    id: TBD after adoption
    schema: EnrollmentStateChangedEvent
    capabilities:
      - device.enrollment
```

候选 device 域 errors（落点 `0x0100–0x01FF`，本次已连号分配 `0x010A` / `0x010B`）：

| Error | 说明 | Review |
|---|---|---|
| `ENROLLMENT_CODE_EXPIRED` (`0x010A`) | pairing code 已过期。 | adopted |
| `ENROLLMENT_CODE_ALREADY_USED` (`0x010B`) | pairing code 已使用。 | adopted |

> **ID 说明（已采纳）**：本次 Stage 30 **不复用** legacy `binding` 域的 `0x1301`–`0x1303`（binding 属 DomainId `0x13`，与 `device`（`0x01`）不同域；且 binding 域 registry YAML 尚未创建，仅存在于 generated 候选补丁中）。`device.enrollment` 按 device 域低字节计数惯例分配 method `0x0102`/`0x0103`/`0x0104`、event/capability `0x0102`，见「采纳记录 A.1」。binding → device.enrollment 的 generated 文件更新留待 legacy-migration 专项处理（见 A.5）。

## 附录 D. 采纳检查清单

- [~] 01 已确认 domain.feature 粒度和 method/event 命名（`getPairingCode` 命名偏好 scoped，见 A.4）。
- [~] 02 `EnrollmentInfo` schema 与 `state` 枚举 6 值已采纳；P0 范围 scoped，见 A.4。
- [x] 03 Section 3.4 状态机转换表覆盖所有已知 legacy 场景。
- [x] 04 methodId（`0x0102`/`0x0103`/`0x0104`）、bitOffset（1/2/3）、request/response schema 已分配（A.1）。
- [x] 05 eventId（`0x0102`）、bitOffset（0）、`DeviceEnrollmentStateChangedEvent` schema 已分配（A.1）。
- [x] 06 errorCode 已分配：common 码复用 + `ENROLLMENT_CODE_EXPIRED`(`0x010A`) / `ENROLLMENT_CODE_ALREADY_USED`(`0x010B`)（A.3）。
- [~] 07 capability 字段以开放措辞写入；枚举值/默认值 scoped，见 A.4。
- [x] 08 schema fieldId（从 `0x01` 起）、capabilityId（`0x0102`）已分配。
- [~] 09 `displayName` `max_length: 128`（对齐 `device.info`），scoped 待产品确认，见 A.4。
- [~] 10 `workspaceId` / `endpointId` 作为 `required: false` 摘要字段写入；隐私暴露策略 scoped，见 A.4。
- [ ] 11 `binding` → `device.enrollment` 的 generated legacy 文件（9.6）更新——本次不落地，留待 legacy-migration 专项（A.5）。
- [ ] 12 Generator 完整生成 `protocol/axtp.protocol.yaml` 和 `docs/generated/*`——待 Stage 50 重跑（A.5）。

> `[x]` 已落实；`[~]` scoped 采纳（默认值已写入，确认后走 amend）；`[ ]` 留待后续阶段（Stage 50 / legacy-migration 专项）。
