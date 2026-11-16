# AXTP

AXTP 主仓库是 Auditoryworks Transport Protocol 的 spec 合同中心。它负责维护协议事实、生成参考、发布包和 conformance 验收输入，供各 runtime / SDK 仓库绑定和实现；它不是 runtime 实现仓库。

## 先看哪里

| 角色 | 第一入口 | 用来做什么 |
|---|---|---|
| 产品 / 架构 | [产品指南](docs/guides/product.md) | 看能力状态、domain 边界、路线图和采纳优先级。 |
| Runtime / SDK 研发 | [Runtime / SDK 指南](docs/guides/runtime.md) | 绑定 spec 版本，使用 generated protocol，对接 mock server / runtime 实现。 |
| 测试 / Conformance | [测试指南](docs/guides/testing.md) | 按 profile 和 level 验收 runtime、SDK、mock-server 行为。 |
| 协议维护者 | [协议维护指南](docs/guides/protocol-maintainer.md) | 把需求从 business input、flow、draft 推到 registry、generated 和 release。 |
| Release owner | [发布文档](release/README.md) | 发布 spec tag、release artifact，并触发 runtime 更新流程。 |

完整阅读路线见 [docs/README.md](docs/README.md)。不要把整个仓库当新人目录从头翻；默认只看角色入口和当前合同。

## 当前实现合同

Runtime 和 SDK 工作必须绑定到明确的 spec tag、commit 或 release artifact。当前可实现合同包括：

| 合同 | 路径 |
|---|---|
| Protocol IR | [contract/protocol/axtp.protocol.yaml](contract/protocol/axtp.protocol.yaml) |
| Generated 参考 | [contract/generated/protocol.md](contract/generated/protocol.md), [contract/generated/protocol.json](contract/generated/protocol.json) |
| 人读正式规范 | [specs/](specs/README.md) |
| Conformance 验收输入 | [conformance/](conformance/README.md) |
| 发布治理 | [release/](release/README.md) |

Registry 事实源位于 `contract/registry/**` 和 `contract/registry/domains/**`。Generated artifact 只能由工具生成，不能手写修改。

## 硬规则

- 不要从 `workspace/business/**`、`workspace/flows/**` 或未采纳的 `workspace/protocol/**` 草案实现 runtime 行为。
- 不要手写修改 `contract/protocol/axtp.protocol.yaml`、`contract/generated/**`、`contract/mcp/**` 或 `contract/test-vectors/**`。
- 不要绕过协议评审，把新的业务语义直接写进 registry YAML。
- Conformance 是 runtime、SDK 和 mock-server 行为验收面。
