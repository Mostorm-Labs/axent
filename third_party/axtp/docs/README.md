# AXTP 文档阅读路线

`docs/` 是仓库的公开阅读入口。默认先从角色指南进入；维护者专用输入放在根目录 `workspace/`，不属于新人默认浏览路径。

## 前台入口

| 角色 | 入口 |
|---|---|
| 产品 / 架构 | [guides/product.md](guides/product.md) |
| Runtime / SDK 研发 | [guides/runtime.md](guides/runtime.md) |
| 测试 / Conformance | [guides/testing.md](guides/testing.md) |
| 协议维护 | [guides/protocol-maintainer.md](guides/protocol-maintainer.md) |
| Release owner | [../release/README.md](../release/README.md) |

Release owner 是前台角色；`release/` 是发布操作区，用来管理 spec tag、artifact、changelog 和 runtime update flow。它不是 runtime 行为合同源。

## 实现合同

| 合同类型 | 读取位置 | Runtime 可直接实现 |
|---|---|---:|
| 生成合同 | [../contract/generated/protocol.md](../contract/generated/protocol.md), [../contract/generated/protocol.json](../contract/generated/protocol.json), `../contract/protocol/axtp.protocol.yaml` | 是 |
| 人读正式规范 | [../specs/](../specs/README.md) | 是，但必须与 generated / YAML 对齐 |
| Registry 事实 | `../contract/registry/**`, `../contract/registry/domains/**` | 是 |
| Conformance | [../conformance/](../conformance/README.md) | 是，作为行为验收面 |
| Release 状态 | [../release/](../release/README.md) | 是，用于版本绑定 |

## 后台工作区

不要默认浏览这些目录。只有当角色指南或维护任务明确指向它们时再进入。
默认 release artifact 只包含 `release/` 下的发布操作文档；其他 workspace 目录是仓库内维护输入，不进入默认发布包。

| 目录 | 用途 | Runtime 合同 |
|---|---|---:|
| `workspace/` | 维护者工作区：business 输入、flow、协议草案、legacy 迁移。 | 否 |
| [../release/](../release/README.md) | 发布操作文档：tag、artifact、changelog、spec lock、runtime update flow。 | 否，除版本绑定外 |
| `docs/archive/` | 历史审计、旧 spec 形态和过期决策背景。 | 否 |
| `tooling/skills/` | Agent lifecycle skills 和流程定义。 | 否 |
| `../workspace/registry-planning/candidates/` | 历史 / 候选 registry 表。 | 否 |

## 生成文件

从文档编辑角度看，生成输出是只读文件。如果生成内容不对，应修 source YAML、specs 或 generator，然后重新跑生成链。

| 生成路径 | 来源 |
|---|---|
| `../contract/protocol/axtp.protocol.yaml` | `../contract/registry/**`, `../contract/registry/domains/**` |
| `../contract/generated/**` | Generator 从 Protocol IR 和 registry sources 生成 |
| `../contract/mcp/**` | Generator 输出 |
| `../contract/test-vectors/**` | Generator 或 test-vector tooling 输出 |
