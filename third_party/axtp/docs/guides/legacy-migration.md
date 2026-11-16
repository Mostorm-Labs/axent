# Legacy 迁移指南

Legacy 迁移负责人负责把旧协议证据、分类结果和迁移计划追踪到 AXTP 的 domain / feature / method / event 候选。

核心规则：

```text
legacy mapping 在采纳前不是新协议正式合同。
```

旧协议可以解释为什么需要某个能力，但不能直接决定新协议命名和 runtime 行为。

## 最短路径

| 顺序 | 文档 | 读它的目的 |
|---:|---|---|
| 1 | `workspace/legacy-migration/README.md` | 了解 legacy 工作区目录和非合同边界。 |
| 2 | `workspace/legacy-migration/MIGRATION_DASHBOARD.md` | 按来源查看迁移总览、阻塞项和 next step。 |
| 3 | `workspace/legacy-migration/classification/README.md` | 了解分类输出和使用方式。 |
| 4 | `workspace/legacy-migration/plans/README.md` | 查看迁移计划入口。 |
| 5 | [Domain/Feature 分类规则](../../specs/30-registry.md) | 按新协议语义判断 domain / feature。 |

## 判断步骤

1. 找到旧协议 evidence，保留来源、命令、状态码、payload 和行为描述。
2. 查看 classification 是否已有 domain / feature 候选。
3. 用 domain/feature 分类规则校准语义边界。
4. 判断目标能力是否已经 generated。
5. 如果已经 generated，补充或校准 legacyRefs 和 adapter 计划。
6. 如果还未 generated，进入 `workspace/flows/**` 或 `workspace/protocol/**`。
7. 阻塞项写到 migration dashboard，不假装已经映射完成。

## 不要从哪里开始

| 不建议入口 | 原因 |
|---|---|
| 直接改 `contract/registry/**` | 旧命令必须先完成分类、草案评审和采纳。 |
| 只看旧命令名 | 旧 namespace 不一定对应新 AXTP domain。 |
| 只看 generated mapping | generated migration 产物是候选输出，仍需人工评审。 |
| runtime adapter 代码 | adapter 只能实现已确认映射，不能定义新协议事实。 |

## Dashboard 状态

| 状态 | 什么时候使用 |
|---|---|
| `TBD` | 还没有可靠统计或映射结论。 |
| `blocked` | 缺少 evidence、字段语义、设备行为或产品决策。 |
| `mapped to draft` | 已能指向 `workspace/protocol/**`，但未采纳。 |
| `mapped to generated` | 已能指向 generated 协议事实。 |
| `adapter needed` | 旧协议还需要兼容层、gateway 或数据转换。 |

## 完成定义

一个 legacy 能力迁移完成时，至少应该有：

| 项 | 说明 |
|---|---|
| Evidence | 能追溯旧命令、字段、状态码或行为。 |
| AXTP target | 指向 generated method/event/schema 或明确 deferred。 |
| Adapter plan | 说明旧协议如何转换到 AXTP，或为什么不迁移。 |
| Test plan | 说明如何用 conformance、fixture 或 adapter 测试验证。 |
