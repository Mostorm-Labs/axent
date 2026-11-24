---
status: generated
contract: true
generated: true
domain: license
feature: license.reconciliation
registry: contract/registry/domains/license/domain.yaml
lastReviewed: 2026-11-19
---

# license.reconciliation

> Adoption note (2026-11-18): 已采纳写入 `contract/registry/domains/license/domain.yaml`（domain `0x19`）并于同日完成 Stage 50 生成刷新——IR 与 generated/mcp 产物已含本域全部事实（本草案 `status: generated`）。后续语义变更 MUST 走 `amend-adopted-protocol`。

**Registry item 分配**（本 feature 部分）：

| Registry item | New assignment | Binding |
|---|---|---|
| method `license.reconcile` | id `0x1903`, bitOffset `2`, since `1.0.0` | `contract/registry/domains/license/domain.yaml` |
| method `license.resolveCredential` | id `0x1905`, bitOffset `4`, since `1.0.0` | 同上 |
| capability `license.reconciliation` | id `0x1903` | 同上 |
| schema `ReconcileParams` | fieldIds `0x01`–`0x02` | 同上 |
| schema `ReconcileResult` | fieldIds `0x01`–`0x04` | 同上 |
| schema `ResolveCredentialParams` | fieldId `0x01` | 同上 |
| schema `ResolveCredentialResult` | fieldIds `0x01`–`0x02` | 同上 |
| schema `LicenseCredentialInfo` | fieldIds `0x01`–`0x05` | 同上 |
| schema `LicenseVersionTriple` | fieldIds `0x01`–`0x03` | 同上 |
| schema `LicenseReconciliationCapabilities` | fieldId `0x01` | 同上 |
| vendor error `LICENSE_DEVICE_NOT_FOUND` | id `0x7002`（domain: vendor） | `contract/registry/error/error_code.yaml` |

## 0. 速读结论

| 项目 | 内容 |
|---|---|
| 这个能力做什么 | 设备上线/重连对账（拉通道，双维度版本比对、三应答形态）与队列 worker 下发前版本纯探测（永不携带凭证字节）。 |
| 当前状态 | generated（已写入 registry 并刷新到 generated 产物） |
| 是否可直接实现 | 是。正式实现合同以 `contract/protocol/axtp.protocol.yaml` / `contract/generated/**` 为准。 |
| 主要交互 | RPC（设备→云对账 + 云→设备探测；零自定义事件） |
| 是否使用 STREAM | 否 |
| Registry readiness | ready（registry + generated 均已刷新，validate/test 全绿） |
| Conformance | needed |
| 主要未决问题 | 无。 |

## 1. 功能说明

下发是尽力而为的（超时 ≠ 失败），对账保证最终一致：设备上线/重连时必须发起对账，平台比对设备当前凭证版本与台账最新版本、设备已装策略行集版本与台账最新行集两个维度，返回台账最新凭证（或补签受理）。队列 worker 在下发前经 `license.resolveCredential` 纯探测设备持有版本，避免向已收敛设备重复下发大载荷。

公共 RPC envelope、错误约定与 schema 展开遵循 [Protocol Draft Conventions](../draft-conventions.md)。业务场景与协议覆盖分析见 [license-entitlement-lifecycle flow](../../flows/license-entitlement-lifecycle.md)；接口素材来源：上游业务规格（设备权益授权系统，平台侧特性规格）的设备通道方法契约。对账为权威、回执为受采信条件约束的加速信号；台账未收敛集路由与 sweep 兜底为平台内部行为，不入协议。

> 方向术语：本文以「设备→云 / 云→设备」描述调用方向。cloud-reverse 通道中设备为逻辑服务端（`contract/protocol/axtp.protocol.yaml:164-169`）——`resolveCredential` 为逻辑客户端（云）的正向调用、`reconcile` 为设备发起的反向 Request；SDK 实装角色模型存在待裁决矛盾（flow OQ-1），方向建模见 §11。

## 2. 能力边界

| 类型 | 内容 |
|---|---|
| 包含 | 对账拉取（三应答形态）、双维度版本比对语义、版本三元组上报、纯探测（永不携带凭证字节）。 |
| 不包含 | 凭证下发与回执（归 `license.credentialDelivery`）；注册（归 `license.registration`）；补签/清权/修订的签发链执行、未收敛集路由、sweep 调度（平台内部）；凭证格式合同（axlic 冻结面）。 |
| 数据面 | 所有操作均通过 RPC method 完成；本 feature 不定义 STREAM。 |

## 3. 方法 Methods

### 3.0 方法速览

| Method | 调用类型 | 用途 | Params Schema | Result Schema | 是否触发事件 | 状态 |
|---|---|---|---|---|---|---|
| `license.reconcile` | query | 设备上线/重连对账（拉通道） | `ReconcileParams` | `ReconcileResult` | 否 | draft |
| `license.resolveCredential` | query | 下发前版本纯探测（队列 worker） | `ResolveCredentialParams` | `ResolveCredentialResult` | 否 | draft |

### 3.1 `license.reconcile`

**用途**：设备上报当前持有版本三元组，平台比对台账后以三种形态之一应答：已最新 / 携带台账最新凭证 / 补签受理（异步重走签发链）。

| 项 | 内容 |
|---|---|
| 调用类型 | query |
| Params Schema | `ReconcileParams` |
| Result Schema | `ReconcileResult` |
| 是否触发事件 | 否（零自定义事件，见 §4） |
| 幂等性 / 异步性 | 同步应答；补签受理异步完成（幂等键防重），产物经 `license.deliverCredential` 推送或后续对账取得 |
| 常见错误 | `INVALID_ARGUMENT`；候选 `LICENSE_DEVICE_NOT_FOUND`（见 §8） |

#### 3.1.1 请求参数 Params：`ReconcileParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `deviceId` | string | yes | `^axldev_[0-9a-f]{32}$` | none | = 连接寻址键当前自报值（已注册形态，平台分配）；平台回退寻址时该字段填连接当前自报值。 |
| `version` | `LicenseVersionTriple` | no | 见 §6.3 | 省略 | 设备当前持有版本三元组（修订号/代次/策略版本）——**无凭证时整体省略**（复用共享 schema，§11 decided）。 |

#### 3.1.2 返回结果 Result：`ReconcileResult`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `upToDate` | bool | yes | true / false | none | 设备已最新（指针自洽且聚合一致）时 true。零授权链、清权未产出、聚合漂移形态 MUST NOT 为 true。 |
| `policyVersion` | uint32 | no | ≥ 0 | 省略 | 台账当前策略行集版本（携带凭证时与 credential 同级返回，设备本地持久化）。 |
| `credential` | `LicenseCredentialInfo` | no | 见 §6.2 | 省略 | 台账最新凭证（upToDate=false 且已有工件时）。 |
| `reissueAccepted` | bool | no | true / false | 省略 | 补签受理形态标志（应签而未签/清权未产出/聚合漂移——无凭证字节可返回时），与 `credential` 互斥。 |

三应答形态（互斥）。**建模已定案**：字段缺席 + 互斥规则（§11 decided，2026-11-18），不引入显式变体/discriminator。

| 形态 | 字段组合 | 语义 |
|---|---|---|
| ① 已最新 | `upToDate=true` | 设备保持现状。 |
| ② 携带最新凭证 | `upToDate=false` + `credential` + `policyVersion` | 设备安装最新版（单调不回退、不重复）。 |
| ③ 补签受理 | `upToDate=false` + `reissueAccepted=true` | 异步重走签发链（自动签发/清权/修订），产物经推送或后续对账取得。 |

#### 3.1.3 d block 示例

request:

```json
{
  "id": 301,
  "method": "license.reconcile",
  "params": {
    "deviceId": "axldev_0123456789abcdef0123456789abcdef",
    "version": {
      "authorityRevision": 3,
      "credentialGeneration": 7,
      "policyVersion": 1
    }
  }
}
```

success:

```json
{
  "id": 301,
  "status": {
    "ok": true,
    "code": 0
  },
  "result": {
    "upToDate": false,
    "policyVersion": 2,
    "credential": {
      "credentialId": "3f9d2c68-7b4e-4f2a-9c1d-5e8a7b6c4d21",
      "authorityRevision": 4,
      "credentialGeneration": 8,
      "reason": "REVISE",
      "cborBase64": "o2JheExpY2Vuc2UuY3JlZGVudGlhbAE…（截断示意）"
    }
  }
}
```

补签受理形态 success（无凭证字节）:

```json
{
  "id": 301,
  "status": {
    "ok": true,
    "code": 0
  },
  "result": {
    "upToDate": false,
    "reissueAccepted": true
  }
}
```

读法：三种形态互斥；形态③ 不携带凭证字节，与「已最新」明确区分——设备收到后等待推送或再次对账，勿轮询加压。

#### 3.1.4 可能触发的事件

| Event | 触发条件 | Payload Schema | 客户端处理建议 |
|---|---|---|---|
| （无） | 本方法不触发任何自定义事件 | — | 补签产物经 `license.deliverCredential` 推送到达。 |

#### 3.1.5 错误

| 错误 | 场景 | 返回建议 |
|---|---|---|
| `NOT_FOUND`（占位） | deviceId 无法定位主体。 | 候选 `LICENSE_DEVICE_NOT_FOUND`；正式码 `TBD after adoption`。 |
| `INVALID_ARGUMENT` | 版本三元组部分省略（要么齐备、要么整体省略）等字段非法。 | 返回字段路径。 |

#### 3.1.6 规则

- 对账为权威：比对凭证版本 (authorityRevision, credentialGeneration) 与策略行集版本（policyVersion）两个维度；离线设备上线后经对账收敛，不丢失、不重复、不取旧版。
- 漂移形态（凭证版本已最新但行集版本落后，或平台检测到快照授权集漂移）MUST NOT 返回旧凭证捎带新行集版本（将造成设备侧行集版本与权益集不一致）——同以补签受理形态触发重签。
- 已注册但授权链未建立、权威态已吊销且清权未产出、签发最终失败等「应签而未签」形态 MUST NOT 应答 upToDate=true，同以补签受理形态受理。
- 已放弃送达（粘性终态）的链，设备主动对账仍可触发清权补签；台账观测态不因晚到收敛翻回（平台侧语义）。
- 补签受理幂等（幂等键防重）；设备重放对账无副作用。

### 3.2 `license.resolveCredential`

**用途**：平台队列 worker 在下发前探测设备当前持有版本（纯探测），比对后决定是否真正下发——避免向已收敛设备重复下发大载荷。

| 项 | 内容 |
|---|---|
| 调用类型 | query |
| Params Schema | `ResolveCredentialParams` |
| Result Schema | `ResolveCredentialResult` |
| 是否触发事件 | 否（零自定义事件，见 §4） |
| 幂等性 / 异步性 | 同步应答、纯探测、无副作用 |
| 常见错误 | `INVALID_ARGUMENT`；候选 `LICENSE_DEVICE_NOT_FOUND`（见 §8） |

#### 3.2.1 请求参数 Params：`ResolveCredentialParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `deviceId` | string | yes | `^axldev_[0-9a-f]{32}$` | none | = 连接寻址键当前自报值（已注册形态，平台分配）；平台回退寻址（经 identity 映射）时该字段填连接当前自报值——与 `reportReceipt` / `reconcile` 口径一致。 |

#### 3.2.2 返回结果 Result：`ResolveCredentialResult`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `hasCredential` | bool | yes | true / false | none | 设备是否持有凭证。 |
| `version` | `LicenseVersionTriple` | no | 见 §6.3 | 省略 | hasCredential=true 时必填——与 `ReconcileParams.version` 同构复用（§11 decided）。 |

#### 3.2.3 d block 示例

request:

```json
{
  "id": 302,
  "method": "license.resolveCredential",
  "params": {
    "deviceId": "axldev_0123456789abcdef0123456789abcdef"
  }
}
```

success:

```json
{
  "id": 302,
  "status": {
    "ok": true,
    "code": 0
  },
  "result": {
    "hasCredential": true,
    "version": {
      "authorityRevision": 3,
      "credentialGeneration": 7,
      "policyVersion": 1
    }
  }
}
```

读法：本方法**永不返回凭证字节**——探测与下发的载荷语义分离；平台比对结果决定是否走 `license.deliverCredential`。

#### 3.2.4 可能触发的事件

| Event | 触发条件 | Payload Schema | 客户端处理建议 |
|---|---|---|---|
| （无） | 本方法不触发任何自定义事件 | — | — |

#### 3.2.5 错误

| 错误 | 场景 | 返回建议 |
|---|---|---|
| `NOT_FOUND`（占位） | deviceId 无法定位主体。 | 候选 `LICENSE_DEVICE_NOT_FOUND`；正式码 `TBD after adoption`。 |
| `INVALID_ARGUMENT` | 字段非法。 | 返回字段路径。 |

#### 3.2.6 规则

- 纯探测：result 为设备持有版本上报，本方法 MUST NOT 携带凭证字节。
- 版本三元组与 `license.reconcile` params 复用同一共享 schema `LicenseVersionTriple`（§11 decided，2026-11-18）。

## 4. 事件 Events

本 feature 不定义任何自定义事件。

设计约束（来自 flow §7）：licensing 域零自定义事件——SDK broker 对带点号的未注册事件名静默丢弃，事件通道不可依赖。

## 5. Capability

Capability name（候选）: `license.reconciliation`。`[REVIEW-DRAFT]`

`resolveCredential` 为设备侧实现（云→设备调用），capability 面向设备侧声明版本持久化与上报能力。OQ-5 已裁决（2026-11-18）：按业务域登记方法、不加「实现方」标注位；capability 维持设备侧声明，`reconcile` 等云侧实现方法不设 capability（见 §11）。

| 能力字段 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `capability` | string | yes | fixed `license.reconciliation` | none | capability 名称。 |
| `persistentPolicyVersion` | bool | yes | true / false | none | 设备是否本地持久化 policyVersion 供对账/探测上报。 |

## 6. 字段 / Schemas

### 6.1 Schema 层级速览

方法字段表已在方法小节直接展开；本章集中展开跨方法/跨草案复用对象 `LicenseCredentialInfo` 与 `LicenseVersionTriple`。

| Schema | 用途 | 定义位置 |
|---|---|---|
| `ReconcileParams` | 对账请求（deviceId + 可选 version 三元组） | §3.1.1 |
| `ReconcileResult` | 对账三形态应答（字段缺席 + 互斥，已定案） | §3.1.2 |
| `ResolveCredentialParams` | 探测请求（deviceId） | §3.2.1 |
| `ResolveCredentialResult` | 探测应答（hasCredential + version 三元组） | §3.2.2 |
| `LicenseCredentialInfo` | 台账最新凭证对象（reconcile result 内嵌；**被 `license.credentialDelivery` 复用**，§11 decided） | §6.2 |
| `LicenseVersionTriple` | 版本三元组（`ReconcileParams.version` 与 `ResolveCredentialResult.version` 复用，§11 decided） | §6.3 |

### 6.2 请求与响应 Schemas：`LicenseCredentialInfo`（跨草案共享）

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `credentialId` | string | yes | uuid 格式 | none | 凭证工件标识。 |
| `authorityRevision` | uint32 | yes | ≥ 0 | none | 凭证修订号。 |
| `credentialGeneration` | uint32 | yes | ≥ 0 | none | 凭证代次。 |
| `reason` | string enum | yes | `ISSUE` / `REVISE` / `REVOKE` | none | 签发语义（REVOKE = 空权益集清权凭证）。 |
| `cborBase64` | string | yes | Base64（RFC 4648）；工件 ≤ 64 KiB → 编码后 ≤ 87384 字符（含填充） | none | 不透明凭证字节（axlic 冻结 CBOR V1 工件）。 |

### 6.3 共享对象：`LicenseVersionTriple`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `authorityRevision` | uint32 | yes | ≥ 0 | none | 凭证修订号。 |
| `credentialGeneration` | uint32 | yes | ≥ 0 | none | 凭证代次。 |
| `policyVersion` | uint32 | yes | ≥ 0 | none | 设备已安装策略行集版本（本地持久化值）。 |

复用范围（§11 decided，2026-11-18）：`ReconcileParams.version`（设备上报）与 `ResolveCredentialResult.version`（探测应答）同构复用；三字段要么齐备、要么随 `version` 整体省略。

## 7. 交互流程示例 Flow Examples

```text
设备离线期间台账两次演进（吊销 + 策略发布）
→ 设备上线，调用 license.reconcile（版本三元组：rev 3 / gen 7 / policy 1）
← 形态②：upToDate=false + credential(REVISE, rev 4 / gen 8) + policyVersion 2
→ 设备验签安装 → license.reportReceipt(INSTALLED)（见 license.credentialDelivery 草案）

签发最终失败的设备上线对账：
→ license.reconcile（无凭证，三元组省略）
← 形态③：upToDate=false + reissueAccepted=true（无凭证字节）
→ 补签产物经 license.deliverCredential 推送到达
```

## 8. 错误

| 错误 | 适用场景 | 说明 |
|---|---|---|
| `LICENSE_DEVICE_NOT_FOUND` | deviceId 无法定位主体。 | **已采纳：0x7002**（domain: vendor，`contract/registry/error/error_code.yaml`，2026-11-18）。 |

上游候选码 0x7004–0x7006（NOT_ENTITLED / RATE_LIMITED / INTERNAL）不进本提案，处置口径见 `license.registration` §8。

## 9. Legacy 映射

暂无。

## 10. 测试重点与采纳风险

| 类型 | 要点 |
|---|---|
| registry readiness | adopted + amended——已写入 `contract/registry/domains/license/domain.yaml`（0x1903/0x1905）与 `error_code.yaml`（0x7002）；2026-11-19 amend（deviceId 口径）已刷新 generated。 |
| conformance focus | 三应答形态互斥、漂移形态不返回旧凭证捎带新行集版本、应签而未签形态不答 upToDate=true、探测永不携带凭证字节、版本三元组整体省略规则。 |
| happy path | 离线演进后上线对账取最新凭证；无凭证设备对账受理补签。 |
| boundary case | 无凭证（三元组省略）；放弃送达链主动对账仍触发清权补签。 |
| error case | deviceId 不存在（候选 `LICENSE_DEVICE_NOT_FOUND`）；三元组部分省略拒绝。 |
| compatibility | 仅 JSON 编码；`policyVersion` 语义与 `license.credentialDelivery` 下发捎带一致（同源通道侧元数据）。 |

## 11. 待确认问题

| 问题 | 影响 | 当前建议 | 状态 |
|---|---|---|---|
| 三应答形态的协议层建模：字段缺席表达 vs 显式 result 变体（discriminator）？ | schema | **已裁决：字段缺席 + 互斥规则**（§3.1.2 已标注）；TLV 演进时再评估变体 | decided（2026-11-18 用户裁决）`[REVIEW-OK]` |
| `LicenseCredentialInfo` 是否抽共享 schema 供 `license.credentialDelivery` 复用？ | schema | **已裁决：抽共享**——下发 params 内嵌 credential 对象与本草案 §6.2 同构（credentialDelivery §3.1.1 已按此更新） | decided（2026-11-18）`[REVIEW-OK]` |
| 专属错误码落位（vendor 段 vs 共享码） | registry | **已裁决：vendor 段**（上游提案 0x7001–0x7006）；specs/30-registry.md vendor 段规则已登记，0x7002 已采纳 | decided（2026-11-18）`[REVIEW-OK]` |
| 云侧实现方法（reconcile）的 capability 表达与双向 RPC registry 建模 | registry / capability | **已裁决：按业务域登记方法，不加「实现方」标注位**——capability 维持设备侧声明（本 feature capability 描述设备版本持久化上报能力）；随 flow OQ-5 关闭 | decided（2026-11-18）`[REVIEW-OK]` |
| 探测应答版本三元组与 reconcile params 是否复用同一 schema | schema | **已裁决：复用**——抽共享 `LicenseVersionTriple`（§6.3 已定义，两处引用） | decided（2026-11-18）`[REVIEW-OK]` |
| 与 `system.license` 草案命名冲突（同 license.registration §11） | registry | **已裁决：新域定案时标记 superseded**（Stage 30 执行） | decided（执行时点 = Stage 30）`[REVIEW-OK]` |

## 修订记录

| 日期 | 变更 | 理由 / 来源 | 兼容性决策 |
|---|---|---|---|
| 2026-11-18 | 初版采纳（0x1903 / 0x1905 / capability 0x1903 / vendor 0x7002），随 v0.19.0 生成发布。 | Stage 30 采纳。 | — |
| 2026-11-19 | `ReconcileParams.deviceId` / `ResolveCredentialParams.deviceId` 格式改 `^axldev_[0-9a-f]{32}$`（40 字符，max_length 36→40），语义 = 连接寻址键当前自报值（已注册形态，平台分配）；补平台回退寻址时该字段填连接当前自报值的口径。 | 同步上游设备身份定案（flow OQ-7）。 | draft 状态下 breaking correction——id/fieldId/bitOffset/name/status/since 全部保持；YAML 仅改 max_length 与 description；generated 产物经 Stage 50 重新生成。 |

## 附录 C. Registry 草案输入（已采纳定稿）

已采纳（2026-11-18）——以下为实际分配值，与 `contract/registry/domains/license/domain.yaml` 一致。

```yaml
# contract/registry/domains/license/domain.yaml（节选——本 feature 部分，实际分配值）
methods:
  - id: 0x1903
    name: license.reconcile
    domain: license
    status: draft
    bitOffset: 2
    since: 1.0.0
    description: Pull-channel reconciliation on device online/reconnect; compares credential and policy-rowset versions and answers in one of three forms.
    rpc_op: request_response
    request_schema: ReconcileParams
    response_schema: ReconcileResult
    recommended_encoding: [json]
    errors: [SUCCESS, INVALID_ARGUMENT, NOT_FOUND, LICENSE_DEVICE_NOT_FOUND]   # 0x7002（domain: vendor）
  - id: 0x1905
    name: license.resolveCredential
    domain: license
    status: draft
    bitOffset: 4
    since: 1.0.0
    description: Pure pre-delivery version probe; returns the device-held version triple and never carries credential bytes.
    rpc_op: request_response
    request_schema: ResolveCredentialParams
    response_schema: ResolveCredentialResult
    recommended_encoding: [json]
    errors: [SUCCESS, INVALID_ARGUMENT, NOT_FOUND, LICENSE_DEVICE_NOT_FOUND]
capabilities:
  - id: 0x1903
    name: license.reconciliation
    domain: license
    status: draft
    since: 1.0.0
    description: 设备对账与版本持久化上报能力声明。
    type: object
    schema: LicenseReconciliationCapabilities
types:
  ReconcileParams:
    kind: object
    fields:
      - id: 0x01
        name: deviceId
        type: string
        required: true
        max_length: 40
      - id: 0x02
        name: version
        type: LicenseVersionTriple    # 共享 schema（域内单文件定义，§11 decided）
        required: false
  ReconcileResult:
    kind: object
    fields:
      - id: 0x01
        name: upToDate
        type: bool
        required: true
      - id: 0x02
        name: policyVersion
        type: uint32
        required: false
      - id: 0x03
        name: credential
        type: LicenseCredentialInfo
        required: false
      - id: 0x04
        name: reissueAccepted
        type: bool
        required: false
  ResolveCredentialParams:
    kind: object
    fields:
      - id: 0x01
        name: deviceId
        type: string
        required: true
        max_length: 40
  ResolveCredentialResult:
    kind: object
    fields:
      - id: 0x01
        name: hasCredential
        type: bool
        required: true
      - id: 0x02
        name: version
        type: LicenseVersionTriple
        required: false
  LicenseCredentialInfo:
    kind: object
    description: Shared credential object reused by license.reconcile result and license.credentialDelivery params.
    fields:
      - id: 0x01
        name: credentialId
        type: string
        required: true
        max_length: 36
      - id: 0x02
        name: authorityRevision
        type: uint32
        required: true
      - id: 0x03
        name: credentialGeneration
        type: uint32
        required: true
      - id: 0x04
        name: reason
        type: enum
        enum: [ISSUE, REVISE, REVOKE]
        required: true
      - id: 0x05
        name: cborBase64
        type: string
        required: true
        max_length: 87384         # 64KiB 工件 Base64（含填充）上限（2026-11-18 已确认）
  LicenseVersionTriple:
    kind: object
    description: Shared version triple reused by ReconcileParams.version and ResolveCredentialResult.version.
    fields:
      - id: 0x01
        name: authorityRevision
        type: uint32
        required: true
      - id: 0x02
        name: credentialGeneration
        type: uint32
        required: true
      - id: 0x03
        name: policyVersion
        type: uint32
        required: true
  LicenseReconciliationCapabilities:
    kind: object
    fields:
      - id: 0x01
        name: persistentPolicyVersion
        type: bool
        required: true
```
