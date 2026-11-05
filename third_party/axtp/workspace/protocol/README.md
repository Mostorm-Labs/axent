# AXTP Protocol Draft Intake

> Backend workspace: do not start here from the main docs. `workspace/protocol/**` contains draft protocol proposals for maintainers, not the runtime implementation contract.

`workspace/protocol/` 是业务协议方案输入与评审区，不是最终协议事实源。

这里的文档用于沉淀产品/架构师提出的业务流程、候选 method/event/schema/error/capability/profile、旧协议线索和评审结论。只有被评审采纳、反向确认到 `specs/30-registry.md` 与 `specs/40-codec.md`，并写入 `contract/registry/` 或 `contract/registry/domains/<domain>/domain.yaml` 的内容，才进入正式生成路径。

注意：`workspace/protocol/` 是草案目录；根目录 `contract/protocol/axtp.protocol.yaml` 是 Generator 输出的 Protocol IR。二者名字相近，但不能互相替代。

```text
workspace/protocol/ = human-written protocol drafts
contract/protocol/axtp.protocol.yaml = generated Protocol IR
```

草案的公共写法只维护在 [Protocol Draft Conventions](draft-conventions.md)。单篇草案只写 feature-specific 的方法、事件、字段、错误候选、legacy 映射和待确认问题，不重复 JSON envelope、错误占位、合同边界和 flow 示例规则。

## Typed Input R2 当前入口

Touch/Mouse 输入合同已按 R2 修订，业务/流程和协议正文同步维护：
[business](../business/nearcast-touchback.md)、[flow](../flows/nearcast-touchback-na20-nt10.md)、
[input.stream](input/input.stream.md)、[cast.touchback](cast/cast.touchback.md)、
[正式规范](../../specs/48-input-stream.md)。旧模式正文从 Git e986dcf 追溯；当前实现
依据机器事实与精确生成合同，不依据历史草案或未发布版本名称推测能力。

## 权威边界

| 路径 | 角色 | 能否作为实现合同 |
|---|---|---|
| `workspace/protocol/<domain>/<domain.feature>.md` | 业务草案、评审记录、采纳/修订依据 | 否，除非已采纳且 generated 已刷新 |
| `contract/registry/**/*.yaml`、`contract/registry/domains/**/*.yaml` | 已采纳机器事实源 | 是，Generator 输入 |
| `contract/protocol/axtp.protocol.yaml` | 聚合后的 Protocol IR | 是，但只读，不手写 |
| `contract/generated/protocol.md` / `.json` | 研发、测试、工具消费的当前协议参考 | 是，Generator 输出 |

## 草案 Frontmatter

后续所有 `workspace/protocol/<domain>/<domain.feature>.md` 草案都应在文件头部补充以下 frontmatter，方便 review、迁移、生成状态和合同边界自动化检查：

```yaml
---
status: draft | review-ok | generated | deprecated
contract: false | true
generated: false | true
domain: <domain>
feature: <domain.feature>
registry: <optional registry path>
lastReviewed: YYYY-MM-DD
---
```

字段含义：

| 字段 | 含义 |
|---|---|
| `status` | 草案当前生命周期状态；`generated` 只在 registry 存在且 generated 已通过时使用。 |
| `contract` | 是否可作为 runtime 实现合同；draft/review-ok 通常为 `false`。 |
| `generated` | 当前内容是否已进入 generated 输出。 |
| `domain` | 所属 domain。 |
| `feature` | 所属 `domain.feature`。 |
| `registry` | 已采纳时指向对应 registry YAML；未采纳可为空字符串。 |
| `lastReviewed` | 最近一次人工确认状态的日期。 |

判断 `generated` 时不要猜：以 `contract/registry/domains/<domain>/domain.yaml`、相关 registry YAML 和 `contract/generated/protocol.md` 为准。无法确认时使用 `status: draft`、`contract: false`、`generated: false`。

## 生成路径

```text
用户没有明确阶段 / 请求横跨多个阶段
        ↓
Codex skill: Stage 99 axtp-protocol-workflow
        ↓ lifecycle routing
产品想法、客户诉求、架构目标、UI 意图或旧协议线索
        ↓
Codex skill: Stage 00 business-intake
        ↓ business requirement brief
workspace/business/<topic>.md
        ↓ scenario / UI / actor / failure path needs mapping
Codex skill: Stage 10 plan-protocol-flow
        ↓ protocol coverage and gap list
workspace/flows/<scenario>.md
        ↓ concrete protocol gap
domain.feature 语义、method/event/schema/error/capability/profile 候选
        ↓
Codex skill: Stage 20 draft-business-protocol
        ↓ reviewable protocol draft
workspace/protocol/<domain>/<domain.feature>.md 协议草案
        ↓ internal review / confirmation
Codex skill: Stage 30 adopt-protocol-draft
        ↓ reverse-confirm Registry and Capability Types specs, plus Profiles Registry when profiles/MVP change
contract/registry/**/*.yaml + contract/registry/domains/**/*.yaml
        ↓
Codex skill: Stage 50 generate-axtp-protocol
        ↓
contract/protocol/axtp.protocol.yaml
        ↓
contract/generated/*、tooling/*、runtime 子仓库 generated 产物
        ↓ post-adoption semantic correction / field removal / deprecation / extension
Codex skill: Stage 40 amend-adopted-protocol
        ↓ update adopted proposal + specs/YAML
contract/registry/**/*.yaml + contract/registry/domains/**/*.yaml
        ↓
Codex skill: Stage 50 generate-axtp-protocol
        ↓
refreshed contract/protocol/axtp.protocol.yaml + generated artifacts
```

## Skill 分工

完整 skill 索引见 `tooling/skills/README.md`。编号目录用于阶段排序，skill 名称仍使用语义化触发名。

| 阶段 | 触发输入 | Skill 做什么 | 允许修改 | 输出 |
|---|---|---|---|---|
| Stage 99 总控路由 | 用户不确定应该业务 intake、flow、草案、采纳、修订、生成、发布还是 runtime 实现 | `axtp-protocol-workflow` 判断生命周期阶段并路由到正确 skill | 按被路由阶段决定 | 明确下一步 workflow |
| Stage 00 业务 | 产品想法、客户诉求、架构目标、UI 意图、legacy 线索或还没形成交互流程的 PRD 输入 | `business-intake` 沉淀业务目标、范围、约束、开放问题和下一阶段建议 | `workspace/business/**` | 业务需求 brief |
| Stage 10 流程 | 业务场景、用户 story、UI 原型、端到端交互 | `plan-protocol-flow` 遍历 story 步骤，查询 adopted/generated/draft 协议覆盖，输出协议交互方案和缺口 | `workspace/flows/**` | 场景流程文档，带 sequence、步骤表、协议覆盖和下一步 skill |
| Stage 20 草案 | flow 已识别出明确协议缺口，或用户已给出具体 method/event/schema/error/capability/profile 语义 | `draft-business-protocol` 遍历 `workspace/protocol/**` 和 legacy 线索，判断复用、修改或新增 domain.feature 草案 | `workspace/protocol/**` 草案和待确认问题 | 可评审协议草案，带候选接口、字段、legacyRefs 和 `[REVIEW-*]` 标记 |
| Stage 30 采纳 | 内部评审确认后的草案 | `adopt-protocol-draft` 读取草案、specs 和现有 YAML，拒绝未确认 `[REVIEW-*]`，反向确认 Registry/Codec specs，固定草案状态，写入 YAML | `workspace/protocol/**`、`specs/30-registry.md`、`specs/40-codec.md`、`contract/registry/**`、`contract/registry/domains/**` | formal proposal + YAML 机器事实源 |
| Stage 40 修订 | 已采纳或已生成的协议事实需要语义修正、字段删除、字段废弃、重命名或扩展 | `amend-adopted-protocol` 读取 adopted proposal、specs、YAML 和 generated 现状，判断兼容性，记录 amendment，修正 YAML 并重新生成 | `workspace/protocol/**`、`specs/30-registry.md`、`specs/40-codec.md`、`contract/registry/**`、`contract/registry/domains/**`、Generator 生成产物 | amended proposal + 更新后的 YAML/生成物 |
| Stage 50 生成 | YAML 事实源已更新，需要刷新正式产物 | `generate-axtp-protocol` 从 YAML 运行 Generator pipeline 并验证输出 | 生成产物 | `contract/protocol/axtp.protocol.yaml`、`contract/generated/*`、tooling/runtime generated 产物 |

草案阶段不得写 registry YAML，不得直接生成最终协议；采纳阶段不得采纳 `[REVIEW-ASK]` 或 `[REVIEW-BLOCKER]` 标记的事实；修订阶段不得绕过 adopted proposal 和 YAML 直接改 generated；生成阶段不得从 Markdown 推断新协议事实，只从 YAML 生成。

如果输入还只是产品目标、客户诉求、架构意图、legacy 线索或 PRD 片段，不要直接进入 flow 或协议草案；先使用 `tooling/skills/00-business-intake/SKILL.md` 输出 `workspace/business/<topic>.md`。如果输入已经是端到端场景、UI 原型或用户 story，不要直接进入协议草案；先使用 `tooling/skills/10-plan-protocol-flow/SKILL.md` 输出 `workspace/flows/<scenario>.md`，把协议步骤、已有覆盖、协议缺口和 UI-only 行为分清楚。

采纳阶段也不应该靠人照着 Markdown 手填 YAML；应使用 `tooling/skills/30-adopt-protocol-draft/SKILL.md` 固化草案到 specs/YAML 的转译、编号、冲突检查和源级验证流程。

已采纳协议的语义变更不应回到普通草案流程，也不应直接手改生成物；应使用 `tooling/skills/40-amend-adopted-protocol/SKILL.md` 记录修订依据、判断兼容性、修正 adopted proposal/specs/YAML，并重新运行 Generator。

## 使用规则

- 新增业务必须先按 `specs/30-registry.md`确定 `domain.feature`。
- 业务 method、event、error、capability、schema、profile 的稳定事实必须写入 YAML。
- `workspace/protocol/<domain>/<domain.feature>.md` 中的 method/event wire name 可以作为评审输入；采纳前不得视为当前协议合同。
- 未进入 migration approved 状态的旧协议材料，应先在本目录或交互式 skill 中完成 domain-feature 分类和待确认问题整理。
- 不得从本目录直接生成 `contract/protocol/axtp.protocol.yaml`；必须经过评审确认、Registry/Capability Types/Profiles specs 反向确认、registry YAML，再由 `generate-axtp-protocol` 生成。
- 研发只根据采纳后的 generated 产物开发和上架 feature，不依赖未采纳草案。
- 已采纳协议如果要删除字段、收窄枚举/范围、改名、废弃或新增字段，必须先判断当前事实是 `draft/experimental` 还是 `mvp/stable`；draft 可按确认事实修正，stable/MVP 默认走 deprecate 或版本化替代。

## 采纳检查

采纳一份业务文档前必须确认：

- capability ID 使用 `domain.feature`，不使用字段级 `Config / State / Scan / Connection` 作为 feature。
- method/event 命名符合配置型、状态型、动作型、流型或导出型模板。
- 新增 ID、`bitOffset` 和 schema fieldId 不与现有 YAML 冲突。
- `specs/30-registry.md` 与 `specs/40-codec.md` 已完成反向确认。
- 旧协议适配只登记确定的 legacy CmdValue、状态码和 payload 映射；未知项保留为待确认问题。
- 运行 `generate-axtp-protocol` 后，`contract/protocol/axtp.protocol.yaml` 与 `contract/generated/*` 能完整反映采纳结果。

## 协议审核标记

`workspace/protocol/<domain>/<domain.feature>.md` 直接使用以下标记进行人工审核：

- `[REVIEW-DRAFT]`：草案已归类，但业务语义、字段或 legacy 映射仍在整理中。
- `[REVIEW-OK]`：命名、归属和接口方向符合 Naming/YAML mapping specs，可进入人工确认或 registry 草案。
- `[REVIEW-FIX]`：进入 registry 前必须修正文档、方法清单、错误策略、schema 或生成路径描述。
- `[REVIEW-ASK]`：需要产品、设备实现或 legacy 行为确认后才能写入 `legacyRefs` 或 YAML。
- `[REVIEW-BLOCKER]`：当前文档定位会误导新协议生成，必须先重写或拆分。

草案文件的 review 表格应放在文档前部，并尽量说明“当前判断”和“进入 registry 前需要做什么”。采纳阶段不得把 `[REVIEW-ASK]`、`[REVIEW-FIX]` 或 `[REVIEW-BLOCKER]` 项直接写入正式 YAML。

## 状态说明

| 状态 | 含义 | 可作为 runtime 合同 |
|---|---|---:|
| `business-input` | 仍在 `workspace/business/**`，只是需求、背景或开放问题。 | 否 |
| `flow-planned` | 已有 `workspace/flows/**` 场景流程，识别了已有覆盖和协议缺口。 | 否 |
| `draft` | 已有 `workspace/protocol/<domain>/<domain.feature>.md` 草案，但还有 DRAFT/ASK/FIX 项。 | 否 |
| `review-ok` | 草案关键事实已通过人工 review，可进入采纳流程。 | 否，需先写入 YAML 并生成 |
| `generated` | `contract/registry/domains/<domain>/domain.yaml` 或相关 registry YAML 已存在，并且 Generator / validation 通过。 | 是 |
| `deprecated` | 已采纳事实被标记废弃，保留兼容但不建议新实现使用。 | 只按 deprecation 规则使用 |

草案版本、review 状态和 spec tag 不是同一个概念。草案可以多次修订但不形成 runtime 可绑定版本；runtime 只能绑定 `spec/vMAJOR.MINOR.PATCH` tag、明确 commit 或 release artifact。

## 草案临时 ID

草案文档可以为了 mock/testing 联调临时使用 `0xF000-0xFEFF` 范围的 methodId、eventId 或 errorCode 占位，但必须满足：

- 只用于草案、mock server、demo 或本地联调。
- 不得写入正式 `contract/registry/**` 或 `contract/registry/domains/**`。
- 采纳时必须按 Domain Registry 高字节和对应 specs 重新分配正式 ID。
- generated 产物中不得出现 `0xF000-0xFEFF` 的正式业务 ID。

产品 / 架构负责人查看当前 domain 覆盖、生成状态和采纳优先级时，请使用 [Product Domain Status](../../docs/product/domain-status.md)。
