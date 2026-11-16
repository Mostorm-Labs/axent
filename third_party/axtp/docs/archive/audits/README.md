# AXTP Audits

`docs/archive/audits/` 保留历史审计、盘点和重构建议。它们回答“当时为什么这么判断”，不回答“现在应该从哪里开始”。

当前入口以 [docs/README.md](../../README.md)、[guides/](../../guides/runtime.md)、[specs/](../../../specs/README.md)、[generated](../../../contract/generated/protocol.md) 和 [conformance](../../../conformance/README.md) 为准。审计文件可能引用旧目录、旧计划或已完成的迁移动作，阅读时必须先看文件顶部日期、scope 和约束。

## 使用边界

| 能做什么 | 不能做什么 |
|---|---|
| 追溯某次文档、spec、registry、codec、tooling 或业务域盘点结论。 | 作为当前 runtime 实现合同。 |
| 查找历史风险、待办和设计取舍。 | 作为当前文档导航入口。 |
| 为新一轮治理计划提供背景材料。 | 覆盖 `specs/**`、`contract/registry/**`、`contract/generated/**` 或 `conformance/**` 的当前事实。 |

## 保留策略

- 保留：有日期、有 scope、有结论或能解释历史决策的审计。
- 可删除或归档：没有日期、没有结论、只重复当前指南的临时笔记。
- 如果审计结论已经转成当前治理规则，应在当前文档中维护规则，在审计中只保留历史记录。
