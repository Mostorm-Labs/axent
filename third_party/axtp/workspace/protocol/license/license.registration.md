---
status: generated
contract: true
generated: true
domain: license
feature: license.registration
registry: contract/registry/domains/license/domain.yaml
lastReviewed: 2026-11-19
---

# license.registration

> Adoption note (2026-11-18): 已采纳写入 `contract/registry/domains/license/domain.yaml`（domain `0x19`）并于同日完成 Stage 50 生成刷新——IR 与 generated/mcp 产物已含本域全部事实（本草案 `status: generated`）。后续语义变更 MUST 走 `amend-adopted-protocol`。

**Registry item 分配**（本 feature 部分）：

| Registry item | New assignment | Binding |
|---|---|---|
| domain `license` | high_byte `0x19` | `contract/registry/core/domain_registry.yaml` |
| method `license.registerIdentity` | id `0x1901`, bitOffset `0`, since `1.0.0` | `contract/registry/domains/license/domain.yaml` |
| capability `license.registration` | id `0x1901` | 同上 |
| schema `RegisterIdentityResult` | fieldIds `0x01`–`0x03` | 同上 |
| schema `LicenseRegistrationCapabilities` | fieldId `0x01` | 同上 |
| vendor error `LICENSE_IDENTITY_INVALID` | id `0x7001`（domain: vendor） | `contract/registry/error/error_code.yaml` |

## 0. 速读结论

| 项目 | 内容 |
|---|---|
| 这个能力做什么 | 设备经云反连通道识别成功后向平台注册设备主体（幂等），触发平台按默认策略自动签发授权凭证（异步受理）。 |
| 当前状态 | generated（已写入 registry 并刷新到 generated 产物） |
| 是否可直接实现 | 是。正式实现合同以 `contract/protocol/axtp.protocol.yaml` / `contract/generated/**` 为准。 |
| 主要交互 | RPC（设备→云调用；零自定义事件） |
| 是否使用 STREAM | 否 |
| Registry readiness | ready（registry + generated 均已刷新，validate/test 全绿） |
| Conformance | needed |
| 主要未决问题 | 无（OQ-1 helloSender 属 axtp 维护事项，不在本域范围）。 |

## 1. 功能说明

设备（接收桥 + 设备侧身份模块）经 `AXTP-WS-CLOUD-REVERSE` 通道反连、会话识别成功（`device.getInfo` 双态自报：未注册设备自报 identity_value〔128 位小写 hex P-256 公钥〕/ 已注册设备自报平台分配 deviceId）后，主动调用本方法完成设备主体注册。平台从连接上下文采纳自报值登记主体并（首次）**分配 deviceId**，按唯一默认策略自动签发授权凭证（异步完成，凭证经 `license.deliverCredential` 推送或后续 `license.reconcile` 取得）。注册与工作空间归属解耦、与授权签发为独立记录的事实。

公共 RPC envelope、错误约定与 schema 展开遵循 [Protocol Draft Conventions](../draft-conventions.md)。业务场景与协议覆盖分析见 [license-entitlement-lifecycle flow](../../flows/license-entitlement-lifecycle.md)；接口素材来源：上游业务规格（设备权益授权系统，平台侧特性规格）的设备通道方法契约。

> 方向术语：本文以「设备→云」描述调用方向。cloud-reverse 通道中设备为逻辑服务端（`contract/protocol/axtp.protocol.yaml:164-169`），设备→云调用为逻辑服务端发起的反向 Request；SDK 实装角色模型存在待裁决矛盾（flow OQ-1），方向建模见 §11。

## 2. 能力边界

| 类型 | 内容 |
|---|---|
| 包含 | 设备主体注册（幂等、采纳连接身份）、注册状态返回（REGISTERED / EXISTING）、未收敛标志（credentialPending）、触发自动签发的受理语义。 |
| 不包含 | 凭证下发与回执（归 `license.credentialDelivery`）；对账与版本探测（归 `license.reconciliation`）；自动签发链路本身、吊销/恢复/换绑（平台内部与管理面操作，非协议面）。 |
| 数据面 | 所有操作均通过 RPC method 完成；本 feature 不定义 STREAM。 |

## 3. 方法 Methods

### 3.0 方法速览

| Method | 调用类型 | 用途 | Params Schema | Result Schema | 是否触发事件 | 状态 |
|---|---|---|---|---|---|---|
| `license.registerIdentity` | command | 设备主体注册（幂等）+ 触发自动签发受理 | `Empty`（候选，见 §11） | `RegisterIdentityResult` | 否 | draft |

### 3.1 `license.registerIdentity`

**用途**：识别成功后由设备调用，平台采纳连接上下文中的自报值（双态：未注册 = identity_value / 已注册 = 平台分配 deviceId）登记设备主体；新主体返回 REGISTERED 并由平台分配 deviceId、受理自动签发，已注册主体幂等返回 EXISTING（恒返回同一 deviceId）。

| 项 | 内容 |
|---|---|
| 调用类型 | command |
| Params Schema | `Empty`（已定案复用；空对象，演进预留——见 3.1.6 规则） |
| Result Schema | `RegisterIdentityResult` |
| 是否触发事件 | 否（零自定义事件，见 §4） |
| 幂等性 / 异步性 | 同步受理、幂等（identity_value 或 deviceId 命中恒返回同一 deviceId）；自动签发异步完成，凭证勿在此同步等待 |
| 常见错误 | `INVALID_ARGUMENT`（采纳校验失败的通用占位）、`LICENSE_IDENTITY_INVALID`（见 §8） |

#### 3.1.1 请求参数 Params：`Empty`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| （无业务字段） | — | — | — | — | 空对象。自报值恒取连接上下文（识别绑定的双态自报身份），请求不携带、也无法覆盖。已定案：复用已注册 `Empty` schema（§11 decided）。 |

#### 3.1.2 返回结果 Result：`RegisterIdentityResult`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `deviceId` | string | yes | `^axldev_[0-9a-f]{32}$`（40 字符） | none | **平台注册时分配**的设备主体标识（`axldev_`+32hex，注册响应返回）；设备 MUST 本地持久化该引用并在后续连接经 `identity.deviceId` 自报（非权威引用，权威身份 = identity_value 公钥）。 |
| `status` | string enum | yes | `REGISTERED` / `EXISTING` | none | 新注册（identity_value 首次采纳、平台分配 deviceId）/ 幂等命中（identity_value 或 deviceId 命中恒返回同一 deviceId，不产生重复主体）。 |
| `credentialPending` | bool | yes | true / false | none | 台账存在未收敛（下发指针未收敛、有授权无凭证、清权未产出、聚合漂移四类口径）时为 true——凭证经推送或后续对账取得；EXISTING 且已收敛时为 false。 |

#### 3.1.3 d block 示例

request:

```json
{
  "id": 101,
  "method": "license.registerIdentity",
  "params": {}
}
```

success:

```json
{
  "id": 101,
  "status": {
    "ok": true,
    "code": 0
  },
  "result": {
    "deviceId": "axldev_0123456789abcdef0123456789abcdef",
    "status": "REGISTERED",
    "credentialPending": true
  }
}
```

error（采纳格式校验失败——终态错误示意）:

```json
{
  "id": 101,
  "status": {
    "ok": false,
    "code": 10,
    "details": {
      "candidateError": "LICENSE_IDENTITY_INVALID"
    }
  }
}
```

读法：`code: 10` 为最近通用码 `INVALID_ARGUMENT` 占位；正式错误 `LICENSE_IDENTITY_INVALID` = **0x7001**（vendor 段，已采纳，见 §8）。双态正则均不匹配（identity_value `^[0-9a-f]{128}$` / deviceId `^axldev_[0-9a-f]{32}$`）为**终态错误**——设备侧应视为致命错误、重建本地身份后重试，而非可重试暂态错误。

#### 3.1.4 可能触发的事件

| Event | 触发条件 | Payload Schema | 客户端处理建议 |
|---|---|---|---|
| （无） | 本方法不触发任何自定义事件 | — | 未收敛信号由 `credentialPending` 表达；凭证到达经 `license.deliverCredential` 调用。 |

#### 3.1.5 错误

| 错误 | 场景 | 返回建议 |
|---|---|---|
| `INVALID_ARGUMENT`（占位） | 自报值采纳校验失败：双态正则（identity_value `^[0-9a-f]{128}$` / deviceId `^axldev_[0-9a-f]{32}$`）均不匹配，或 identity_value 已映射其他主体（占用）。 | `LICENSE_IDENTITY_INVALID`（0x7001）；双态正则均不匹配为终态语义（设备重建本地身份，勿原样重试）。 |
| `UNAVAILABLE` | 平台签发后端暂不可用等瞬态。 | 设备可稍后重试或经对账补签。 |

#### 3.1.6 规则

- 识别成功是注册前置：连接识别由会话层 `device.getInfo` 完成（双态自报、识别层仅校验非空），身份未就绪的连接被断开重试属连接层正确行为，不构成本方法错误。
- responder MUST 忽略 params 全部字段（含未来扩展字段）——自报值恒以连接上下文为准，params 为纯演进预留，防参数层覆盖连接身份。
- 采纳校验为双态正则：匹配 identity_value `^[0-9a-f]{128}$` 且无既有映射 → 创建主体并由平台分配 deviceId（`axldev_`+32hex，响应返回）；匹配 deviceId `^axldev_[0-9a-f]{32}$` 或 identity_value 命中既有映射 → 幂等 EXISTING 恒返回**同一** deviceId（identity 重解析恒回同一主体；本地注册引用丢失的设备自报 identity_value 重解析即恢复）。
- 注册幂等仅约束主体唯一（一 identity_value / deviceId 至多映射一个设备主体）与首个自动签发授权的幂等发放键；主体已存在但零有效授权且权威态非吊销时，EXISTING 命中可重触发自动签发（平台侧幂等防线，重放无害）。
- 权威态已吊销（REVOKED）的主体重连注册：EXISTING 幂等命中，不恢复任何授权、不重触发自动签发；授权恢复仅经管理面操作。
- 注册不携带、也不推断工作空间归属；license 域零工作空间信息。

## 4. 事件 Events

本 feature 不定义任何自定义事件。状态与未收敛信号全部经 Request/Response 表达；凭证到达经 `license.deliverCredential` 云→设备调用承载。

设计约束（来自 flow §7）：licensing 域零自定义事件——SDK broker 对带点号的未注册事件名静默丢弃，事件通道不可依赖。

## 5. Capability

Capability name（候选）: `license.registration`。`[REVIEW-DRAFT]`

本方法为云侧实现（设备→云调用），capability 的设备侧语义为「设备完成识别后会主动发起注册」。OQ-5 已裁决（2026-11-18）：按业务域登记方法、不加「实现方」标注位；capability 维持设备侧声明，云侧实现方法不设 capability（见 §11）。

| 能力字段 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `capability` | string | yes | fixed `license.registration` | none | capability 名称。 |
| `deviceIdFormat` | string | yes | 双态格式声明：identity_value `^[0-9a-f]{128}$` / deviceId `^axldev_[0-9a-f]{32}$` | none | 设备自报值的双态格式声明（采纳校验口径：未注册自报 identity_value、已注册自报平台分配 deviceId）。 |

## 6. 字段 / Schemas

简单 feature：字段表已在方法小节直接展开，本章仅保留索引。

| Schema | 用途 | 定义位置 |
|---|---|---|
| `Empty` | 注册请求空对象（候选复用已注册 Empty schema） | §3.1.1 |
| `RegisterIdentityResult` | 注册应答（deviceId / status / credentialPending） | §3.1.2 |

## 7. 交互流程示例 Flow Examples

```text
设备本地生成身份密钥对（对外仅 identity_value = 128hex P-256 公钥）
→ 反连 + 会话握手 + device.getInfo 识别（双态自报，会话层，generated 事实）
→ 设备调用 license.registerIdentity（空 params，自报值取连接上下文）
← REGISTERED（含平台分配 deviceId，设备持久化）/ EXISTING + credentialPending
（自动签发异步完成 → 凭证经 license.deliverCredential 推送，见 license.credentialDelivery 草案）
```

## 8. 错误

| 错误 | 适用场景 | 说明 |
|---|---|---|
| `LICENSE_IDENTITY_INVALID` | 自报值采纳校验失败：双态正则（identity_value `^[0-9a-f]{128}$` / deviceId `^axldev_[0-9a-f]{32}$`）均不匹配（终态——设备重建本地身份后重试，非可重试暂态），或 identity_value 已映射其他主体（占用同码）。 | **已采纳：0x7001**（domain: vendor，`contract/registry/error/error_code.yaml`，2026-11-18；2026-11-19 修正为双态正则 + 占用语义；specs/30-registry.md vendor 段使用规则已同步登记）。 |

其余上游候选码不进本提案（处置口径，另两份草案 §8 同引）：

| 候选码（上游提案） | 处置 | 理由 |
|---|---|---|
| `LICENSE_NOT_ENTITLED`（0x7004） | 不注册 | 正常流不出现（无有效授权时不会到达方法语义层）；如需表达可由共享 `NOT_FOUND` / `PERMISSION_DENIED` 承载。 |
| `LICENSE_RATE_LIMITED`（0x7005） | 不注册（码位可随域提案保留） | 上游定案 D2S responder 不限流（千级设备并发上线为合法场景，重放幂等无危害）；限流启用时再提案。 |
| `LICENSE_INTERNAL`（0x7006） | 不注册 | 复用共享 `INTERNAL_ERROR`（0x000E）。 |

## 9. Legacy 映射

暂无（legacy SN 桥接体系已废除，见 flow 速读结论）。

## 10. 测试重点与采纳风险

| 类型 | 要点 |
|---|---|
| registry readiness | adopted + amended——已写入 `contract/registry/domains/license/domain.yaml`（0x1901）与 `error_code.yaml`（0x7001），domain 0x19 已注册；2026-11-19 amend（deviceId 平台分配/双态）已刷新 generated。 |
| conformance focus | 幂等（重复注册不产生重复主体、identity 重解析恒回同一主体）、params 全字段忽略（连接身份不可被参数覆盖）、吊销主体重注册不恢复授权。 |
| happy path | 首次注册（自报 identity_value）返回 REGISTERED + 平台分配 deviceId + credentialPending=true；重连再注册（自报 deviceId 或 identity_value）返回 EXISTING 同一 deviceId。 |
| boundary case | 非 axlic 客户端自报任意非空串：识别成功但注册恒拒（双态正则均不匹配，终态 0x7001）——「识别成功 × 采纳永拒」组合失效模式的契约出口。 |
| error case | 格式不符终态拒绝；占用拒绝；瞬态不可用可重试。 |
| compatibility | 仅 JSON 编码（cloud-reverse 通道）；TLV 为演进。 |

## 11. 待确认问题

| 问题 | 影响 | 当前建议 | 状态 |
|---|---|---|---|
| 专属错误码落位：vendor 段（上游提案 0x7001–0x7006）还是复用共享错误码？ | registry | **已裁决：vendor 段**——specs/30-registry.md vendor 段使用规则已登记；0x7001 已采纳 | decided（2026-11-18 用户裁决）`[REVIEW-OK]` |
| 空 params 复用已注册 `Empty` schema，还是定义 feature 专属空 params（演进预留语义入 schema）？ | schema | **已裁决：复用 `Empty`**——「忽略全部字段」由方法规则层承载（§3.1.6） | decided（2026-11-18）`[REVIEW-OK]` |
| 云侧实现方法的 capability 表达（设备声明注册意愿 vs 云声明受理能力）与双向 RPC 的 registry 建模 | registry / capability | **已裁决：按业务域登记方法，不加「实现方」标注位**——capability 维持设备侧声明（本 feature 的 `license.registration` 为设备侧行为声明），云侧实现方法不设 capability；随 flow OQ-5 关闭 | decided（2026-11-18）`[REVIEW-OK]` |
| 与既有 `workspace/protocol/system/system.license.md` 草案的命名冲突处置（泛型配置 CRUD 语义 vs 设备授权链语义） | registry / 命名治理 | **已裁决：新 license 域定案（Stage 30 采纳）时将旧草案标记 superseded**；本轮仅记录决定 | decided（执行时点 = Stage 30）`[REVIEW-OK]` |
| cloud-reverse 角色模型矛盾（协议 IR helloSender=Device vs SDK 实装）对方向描述的影响 | spec 一致性 | 不阻塞本草案；以 flow OQ-1 裁决为准，必要时 `amend-adopted-protocol` | open（axtp 维护事项） |
| 方法命名与上游契约一致性（`license.registerIdentity`） | 跨仓对齐 | 保持同名对齐（上游平台已按此实现） | decided（2026-11-18 用户确认）`[REVIEW-OK]` |

## 修订记录

| 日期 | 变更 | 理由 / 来源 | 兼容性决策 |
|---|---|---|---|
| 2026-11-18 | 初版采纳（0x1901 / capability 0x1901 / vendor 0x7001），随 v0.19.0 生成发布。 | Stage 30 采纳。 | — |
| 2026-11-19 | deviceId 产生方修正：设备本地产生（`dev_`+32hex、36 字符）→ **平台注册时分配**（`^axldev_[0-9a-f]{32}$`、40 字符、设备 MUST 持久化、非权威引用）；设备对外身份 = identity_value（`^[0-9a-f]{128}$` P-256 公钥）；device.getInfo 双态自报；0x7001 改双态正则 + identity_value 占用语义；`deviceIdFormat` 改双态格式声明（max_length 32→64）。 | 同步上游设备身份定案（flow OQ-7）。 | draft 状态下 breaking correction——id/fieldId/bitOffset/name/status/since 全部保持；YAML 仅改 max_length 与 description；generated 产物经 Stage 50 重新生成。 |

## 附录 C. Registry 草案输入（已采纳定稿）

已采纳（2026-11-18）——以下为实际分配值，与 `contract/registry/domains/license/domain.yaml` 一致。

```yaml
# contract/registry/domains/license/domain.yaml（节选——本 feature 部分，实际分配值）
methods:
  - id: 0x1901
    name: license.registerIdentity
    domain: license
    status: draft
    bitOffset: 0
    since: 1.0.0
    description: Register the device subject from the connection identity (idempotent) and accept automatic entitlement issuance.
    rpc_op: request_response
    request_schema: Empty            # 复用已注册 Empty（§11 decided）
    response_schema: RegisterIdentityResult
    recommended_encoding: [json]     # cloud-reverse 通道仅 JSON；TLV 为演进
    errors: [SUCCESS, INVALID_ARGUMENT, UNAVAILABLE, LICENSE_IDENTITY_INVALID]   # 0x7001（domain: vendor）
capabilities:
  - id: 0x1901
    name: license.registration
    domain: license
    status: draft
    since: 1.0.0
    description: 设备注册流程支持声明（识别成功后主动发起注册）。
    type: object
    schema: LicenseRegistrationCapabilities
types:
  RegisterIdentityResult:
    kind: object
    fields:
      - id: 0x01
        name: deviceId
        type: string
        required: true
        max_length: 40
      - id: 0x02
        name: status
        type: enum
        enum: [REGISTERED, EXISTING]
        required: true
      - id: 0x03
        name: credentialPending
        type: bool
        required: true
  LicenseRegistrationCapabilities:
    kind: object
    fields:
      - id: 0x01
        name: deviceIdFormat
        type: string
        required: true
        max_length: 64
```
