# AXTP Business Requirements

> Backend workspace: do not start here from the main docs. Use the role guides first, and open this directory only when maintaining product requirements or tracing source intent.

`workspace/business/` 用于存放 AXTP 工作中最上游、最原始的业务需求概要。

这里保存还没有被拆成协议交互流程或协议草案的需求输入。业务需求可以来自产品规划、客户场景、UI 草图、集成诉求、现场反馈，或仍需要产品语义解释的旧协议行为。

本目录不是协议合同。文档应尽量保留需求意图、业务背景、用户目标、约束和开放问题，让后续 `workspace/flows/**` 与 `workspace/protocol/**` 可以追溯到原始业务来源。

业务文档只回答“为什么要做、给谁用、成功是什么、边界和疑问是什么”。不要在这里维护协议覆盖表、method/event/schema 细节、wire 示例或端到端 sequence；这些分别属于 `workspace/flows/**` 和 `workspace/protocol/**`。

## 边界

| 路径 | 角色 | 能否作为实现合同 |
|---|---|---:|
| `workspace/business/<requirement>.md` | 原始业务需求、背景、目标、约束、开放问题 | 否 |
| `workspace/flows/<scenario>.md` | 从业务需求拆出的场景级协议交互方案 | 否 |
| `workspace/protocol/<domain>/<domain.feature>.md` | domain.feature 协议草案和评审输入 | 否，除非已采纳并生成 |
| `contract/registry/**/*.yaml`、`contract/registry/domains/**/*.yaml` | 已采纳的机器可读协议事实 | 是 |
| `contract/protocol/axtp.protocol.yaml`、`contract/generated/**` | 生成后的协议参考 | 是 |

## 工作流

```text
产品想法 / 客户诉求 / 现场反馈 / UI 草图 / 旧协议需求
  -> workspace/business/<requirement>.md
  -> Stage 10 plan-protocol-flow
  -> workspace/flows/<scenario>.md
  -> workspace/protocol/<domain>/<domain.feature>.md
  -> contract/registry/**/*.yaml + contract/registry/domains/**/*.yaml
  -> contract/protocol/axtp.protocol.yaml + contract/generated/**
```

如果一份需求已经描述了端到端交互，应使用 `tooling/skills/10-plan-protocol-flow/SKILL.md` 拆成 `workspace/flows/<scenario>.md`。如果它已经明确指出某个缺失或需要修订的协议能力，应继续创建或修订 `workspace/protocol/<domain>/<domain.feature>.md`。

## 建议模板

```markdown
# <业务需求名称>

## 背景

说明这个需求为什么存在、由谁提出、解决当前哪个产品或集成缺口。

## 用户目标

说明用户、操作员、设备、App 或集成方想完成什么。

## 范围

本需求包含什么。

## 非目标

本需求不解决什么。

## 场景

- 场景 1
- 场景 2

## 约束

- 产品、设备、固件、网络、兼容性、时延、安全、隐私或部署约束。

## 旧协议线索

- 关联旧协议文件、命令、日志、SDK 行为或历史文档。

## 开放问题

- [REVIEW-ASK] 需要产品、架构、固件、App、后台或旧协议行为确认的问题。

## 下一步

- 推荐下一阶段：none / `plan-protocol-flow` / `draft-business-protocol` / `amend-adopted-protocol` / runtime implementation / legacy migration。
- 如果需要端到端交互和协议覆盖分析，交给 `workspace/flows/<scenario>.md`。
- 如果 flow 已识别出明确协议缺口，再进入 `workspace/protocol/<domain>/<domain.feature>.md` 草案。
```

## 命名

文件名优先使用简短的 kebab-case，并尽量来自需求或场景名称，例如：

- `audio-algorithm-level-control.md`
- `display-layout-control.md`
- `firmware-ota-policy.md`

当一份业务需求会拆到多个协议领域时，保留原始 business 文档不拆散，在“下一步”里链接到多个 flow 或 protocol 文档。
