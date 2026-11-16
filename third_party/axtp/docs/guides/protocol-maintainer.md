# 协议维护指南

协议维护者负责把自然语言需求推进到可生成、可测试、可发布的协议事实。

主链路：

```text
business -> flows -> protocol draft -> registry -> generated -> conformance -> release
```

核心规则：草案可以讨论，contract/registry/specs 才是源，Protocol IR/generated 是输出，conformance 用来验收 runtime。不要手写 generated 产物。

## 心智模型

| 类型 | 路径 | 含义 |
|---|---|---|
| 评审输入 | `workspace/business/`、`workspace/flows/`、`workspace/protocol/`、`workspace/legacy-migration/` | 用于讨论、规划和评审，还不是 runtime 合同。 |
| 事实源 | `specs/`、`contract/registry/`、`contract/registry/domains/` | 定义当前协议的人读规则和机器可读事实。 |
| 生成合同 | `contract/protocol/axtp.protocol.yaml`、`contract/generated/`、`contract/test-vectors/` | 供工具、测试、release artifact 和 runtime 仓库消费。 |

完整合同等级和冲突处理规则见 [Contract Boundaries](../../specs/10-contract.md)。

## 标准动作

1. 判断输入是业务 PRD、场景流程、协议草案、采纳、修订、生成还是发布。
2. 需求还只是产品、架构、客户、legacy 或 UI 意图时，先形成 `workspace/business/<topic>.md`。
3. 端到端交互再形成 `workspace/flows/<scenario>.md`。
4. 新增或修改业务能力先形成 `workspace/protocol/<domain>/<domain.feature>.md`。
5. 草案评审通过后，反向确认 specs 和 registry YAML。
6. 从 YAML 运行 Generator，刷新 Protocol IR、generated reference、test vectors 或 snapshots。
7. 行为合同变化时，同步 conformance 或说明为什么不需要。
8. release 时发布 spec tag，并让 runtime 仓库更新 spec lock。

## Skill 路由

| 阶段 | Skill | 什么时候使用 | 典型输出 |
|---:|---|---|---|
| 00 | `business-intake` | 需求还停留在产品、架构、客户、legacy 或 UI 意图，需要先沉淀业务 PRD。 | `workspace/business/<topic>.md`。 |
| 10 | `plan-protocol-flow` | 有场景、UI flow、用户 story 或端到端交互。 | `workspace/flows/<scenario>.md`。 |
| 20 | `draft-business-protocol` | business intake 或 flow 已识别出明确协议缺口。 | `workspace/protocol/<domain>/<domain.feature>.md` 草案。 |
| 30 | `adopt-protocol-draft` | 草案已评审，需要成为正式协议事实。 | 更新 specs 和 registry YAML。 |
| 40 | `amend-adopted-protocol` | 已采纳 / 已生成协议需要语义修正、废弃、改名、收窄或扩展。 | 更新 proposal、specs、YAML 和 generated artifacts。 |
| 50 | `generate-axtp-protocol` | YAML 事实已准备好，需要刷新或验证输出。 | Protocol IR、generated reference、snapshots、test vectors。 |
| 60 | `release-axtp-spec` | 已验证 spec 需要发布为 `spec/vX.Y.Z`。 | Spec tag、release artifact、runtime dispatch。 |
| 99 | `axtp-protocol-workflow` | 不确定请求处在哪个生命周期阶段，或请求横跨多个阶段。 | 路由到 00/10/20/30/40/50/60 或 runtime 实现。 |

Skill 定义见 `tooling/skills/README.md`。

## 常见请求模板

业务 intake：

```text
为 <产品/场景/能力> 起草 AXTP 业务需求输入。
用户目标：<谁要完成什么>。
设备/App/云/SDK 边界：<各自负责什么>。
约束：<权限、延迟、持久化、离线、legacy、兼容性>。
请判断后续是否需要 flow 或 protocol draft。
```

规划 flow：

```text
为 <scenario> 规划 AXTP 协议流程。
参与角色：<device, app, cloud, runtime, user>。
必要行为：<steps>。
边界情况：<errors, retry, timeout, low bandwidth, permissions>。
```

起草协议：

```text
为 <domain.feature> 起草 AXTP 协议。
业务目标：<feature 做什么>。
输入输出：<关键字段>。
事件：<状态变化或通知>。
Legacy references：<旧命令、旧文档或旧行为，如有>。
兼容性约束：<stable fields, old runtime behavior, low bandwidth>。
```

采纳草案：

```text
采纳 workspace/protocol/<domain>/<domain.feature>.md 中已评审的草案。
确认 naming、schema、method、event、error、capability、profile 和 legacy mapping。
然后更新 registry YAML 并重新生成。
```

修订已采纳协议：

```text
修订已采纳的 <domain.feature> 协议。
变更：<需要改什么>。
原因：<bug, compatibility issue, product change, legacy correction>。
兼容性等级：<draft, experimental, mvp, stable>。
偏好的处理方式：<deprecate, add replacement, rename before release, version>。
```

## 采纳前检查

| 检查项 | 说明 |
|---|---|
| 业务来源 | 能追溯到 business、flow、legacy evidence 或 review 结论。 |
| Domain/feature | 符合 naming 和分类规则。 |
| Review 标记 | 没有未解决的 `[REVIEW-ASK]`、`[REVIEW-FIX]`、`[REVIEW-BLOCKER]`。 |
| Schema | 字段命名、类型、可选性、默认值和兼容性清楚。 |
| Error | 错误码引用正式 registry 或明确新增需求。 |
| Capability | capability ID 使用 `domain.feature`。 |
| Conformance | 行为变化是否需要新增或调整 case 已有结论。 |

## 生成和发布

本地信心检查：

```bash
pnpm --dir tooling/generators build
pnpm --dir tooling/generators test
pnpm --dir tooling/generators validate:sources
pnpm --dir tooling/generators generate
pnpm --dir tooling/generators validate:protocol
tooling/scripts/validate-conformance.sh
git diff --check
```

release 路径会构建 spec artifact，将版本标记为 `spec/vMAJOR.MINOR.PATCH`，并可触发 runtime upgrade workflows。发布规则见 [release docs](../../release/README.md)。

如果只是文档变更，通常链接检查和针对性 validation 就够；如果文档影响 generated facts、conformance、release artifacts 或 workflow scripts，就跑完整链路。
