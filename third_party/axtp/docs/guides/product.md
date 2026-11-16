# 产品 / 架构指南

产品 / 架构负责人主要看三件事：AXTP 现在覆盖哪些能力、哪些仍是缺口、能力边界怎么划分。不要从 wire format、YAML 或 runtime 实现代码开始。

Roadmap 和 draft 都不是 runtime 合同。它们用于规划和评审；只有 generated + conformance 后，runtime 才能正式跟进。

## 最短路径

| 顺序 | 文档 | 读它的目的 |
|---:|---|---|
| 1 | [产品 Domain 状态](../product/domain-status.md) | 查看 domain 生成状态、优先级和下一步动作。 |
| 2 | [产品路线图](../product/roadmap.md) | 了解阶段规划；记住它不是 runtime 合同。 |
| 3 | [Domain/Feature 分类规则](../../specs/30-registry.md) | 判断需求归属和能力边界。 |
| 4 | `workspace/business/` | 查看或补充业务需求输入。 |
| 5 | `workspace/flows/` | 查看场景流程和协议覆盖。 |

## 评审时看什么

| 问题 | 检查点 |
|---|---|
| 这个需求属于哪个 domain？ | 按 domain/feature 分类规则判断逻辑空间、物理设备、系统运行态还是功能能力。 |
| 是否已有 generated 能力？ | 看 [generated protocol](../../contract/generated/protocol.md) 和 [产品 Domain 状态](../product/domain-status.md)。 |
| 是否需要新 flow？ | 跨多个角色、多条消息或有异常路径时，优先写 `workspace/flows/**`。 |
| 是否需要新草案？ | generated 不覆盖，且 flow 识别出协议缺口时，进入 `workspace/protocol/**`。 |
| 是否影响 runtime release？ | 只有 generated + conformance 后，runtime 才能排正式实现。 |

## 不要从哪里开始

| 不建议入口 | 原因 |
|---|---|
| `contract/protocol/axtp.protocol.yaml` | 这是机器 IR，不适合作为产品第一入口。 |
| `contract/generated/protocol.json` | 适合工具读取，不适合判断业务优先级。 |
| 单个 `workspace/protocol/**` 草案 | 容易只看到局部，不知道 domain 整体状态。 |
| runtime 仓库实现代码 | runtime 是消费方，不代表协议规划全貌。 |

## 输出物

| 输出物 | 什么时候写 |
|---|---|
| 业务输入 | 需求还在表达目标、约束、用户场景和开放问题。 |
| 交互 Flow | 已经能描述多个角色之间的端到端交互。 |
| 协议草案反馈 | 草案已有，但 domain、字段、状态、事件或 legacy 语义需要确认。 |
| Roadmap 更新 | 影响阶段优先级、批次顺序或 release 计划。 |

## 完成定义

一次产品/架构评审完成时，应留下：

| 项 | 说明 |
|---|---|
| 业务目标 | 这个能力解决什么用户或系统问题。 |
| Domain 边界 | 为什么属于这个 domain/feature，而不是相邻 domain。 |
| 交互路径 | 是否需要 flow，是否涉及事件、重试、断线或计划任务。 |
| 采纳优先级 | 属于 P0/P1/P2 还是 deferred。 |
| 未决问题 | 需要设备、legacy、测试或 runtime 确认的事项。 |
