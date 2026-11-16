# AXTP 产品路线图

本页是产品和架构规划材料，不是 runtime 实现合同。Runtime、SDK、CLI、mock server 和 adapter 仓库必须绑定 spec tag、release artifact、`contract/protocol/axtp.protocol.yaml`、`contract/generated/**`、`specs/**` 和 `conformance/**`。

## 当前状态

当前 generated / adopted 覆盖情况以 [Product Domain Status](domain-status.md) 为准。只有当 source YAML 已进入 `contract/registry/domains/<domain>/domain.yaml`、生成产物已刷新、并且 validation 通过时，该 domain 才算 runtime-contract ready。

仓库前台刻意保持窄入口：

- 产品和架构看 [Product Guide](../guides/product.md) 与 [Domain Status](domain-status.md)。
- Runtime 和 SDK 团队看 generated protocol、specs、release artifact 与 conformance。
- 协议维护者只有在修改协议事实时，才进入 business inputs、flows、drafts、registry 和 generator。

## 规划主题

| 主题 | 方向 | 合同边界 |
|---|---|---|
| Runtime 互操作 | 围绕已发布 spec tag 和 conformance profile 保持 C++、TypeScript、mock-server 等实现一致。 | 在 runtime / tool 仓库实现，不在本仓库实现。 |
| Standard Framed transport | 可用时优先使用 AXTP-TCP + Standard Frame 作为跨 runtime 互操作基准。 | 行为必须由 conformance 和 mock-server interop 验证。 |
| Generated business domains | 只有经过产品和协议评审的 domain 草案，才能进入 `contract/registry/domains/**`。 | Draft 不是实现合同。 |
| Conformance expansion | 当 generated protocol 行为变化时，同步增加 profile / case 覆盖。 | 测试只验证 runtime 声明的 support level。 |
| Legacy migration | 旧协议 evidence 和 adapter planning 保持为后台材料。 | Legacy evidence 不覆盖 generated AXTP facts。 |

## 近期优先级

| 优先级 | 目标 | 责任入口 |
|---|---|---|
| P0 | 保持 release artifact、generated protocol 和 conformance validation 绿色。 | [发布文档](../../release/README.md)、[测试指南](../guides/testing.md) |
| P0 | 使用 Node mock-server 和 runtime conformance 验证 active runtime 的 TCP / framed 行为。 | Runtime 仓库 + conformance profile |
| P1 | 将高置信度草案从 `workspace/protocol/**` 推进到 registry YAML 和 generated outputs。 | [协议维护指南](../guides/protocol-maintainer.md) |
| P1 | 按 domain 澄清产品状态和采纳优先级。 | [Product Domain Status](domain-status.md) |
| P2 | 只有当 legacy adapter planning 支撑明确迁移目标时才继续推进。 | [Legacy 迁移指南](../guides/legacy-migration.md) |

## 规则

- Roadmap milestone 是规划标签，不是 spec version。
- Runtime 团队绑定 `spec/vMAJOR.MINOR.PATCH`、明确 commit 或 release artifact。
- 未采纳草案、flow plans、audits 和 legacy evidence 不能作为 runtime 实现合同。
- Tool 和 runtime 交付发生在外部仓库；只有本仓库显式生成的产物才留在这里。
