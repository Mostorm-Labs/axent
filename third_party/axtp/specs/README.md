# AXTP 规范文本

`specs/` 是手写的 AXTP 正式标准文本。它解释 `contract/` 中机器合同背后的规则；这里不放生成的注册表、候选表、legacy 证据或工作流手册。

## 阅读方式

| 读者 | 推荐阅读 |
|---|---|
| Runtime / SDK 实现者 | 先读 [20-core.md](20-core.md)，需要编码细节时再读 [40-codec.md](40-codec.md)。 |
| 注册表 / 生成器维护者 | 读 [30-registry.md](30-registry.md)、[40-codec.md](40-codec.md)、[50-tooling.md](50-tooling.md)。 |
| 产品 / 协议评审 | 读 [30-registry.md](30-registry.md)，再看 `docs/product/domain-status.md`。 |
| 发布负责人 | 读 [10-contract.md](10-contract.md)、[50-tooling.md](50-tooling.md)、`release/README.md`。 |
| 对术语不确定的人 | 读 [00-glossary.md](00-glossary.md)。 |

## 标准文本

| 文件 | 合同内容 |
|---|---|
| [00-glossary.md](00-glossary.md) | 共享术语。 |
| [10-contract.md](10-contract.md) | 事实源顺序、冲突处理和非合同材料边界。 |
| [20-core.md](20-core.md) | Frame、transport profile、CONTROL、RPC、STREAM 和低带宽边界。 |
| [30-registry.md](30-registry.md) | `domain.feature` 分类，以及 method、event、error、profile 注册规则。 |
| [40-codec.md](40-codec.md) | 类型、schema、capability 模型、TLV 和字段编号规则。 |
| [45-cast-control.md](45-cast-control.md) | 可选多路投屏控制 v1：精确目标、原子快照、事件组、操作恢复和兼容保证。 |
| [46-cast-settings.md](46-cast-settings.md) | 可选呈现配置 v1：单字段 patch、生效/保存部分结果、独立设置流和双屏障快照。 |
| [47-cast-webcast-backend.md](47-cast-webcast-backend.md) | 可选 WebCast 云端/本地信令状态、独立事件流、重连快照及隐私边界。 |
| [50-tooling.md](50-tooling.md) | Registry YAML 到 Protocol IR、生成器行为、版本和发布绑定。 |

## 权威顺序

实现时按照以下顺序判断合同事实：

1. 已发布的 spec artifact，或精确的 `spec/vMAJOR.MINOR.PATCH` tag。
2. `contract/protocol/axtp.protocol.yaml`。
3. `contract/generated/protocol.md` 和 `contract/generated/protocol.json`。
4. `conformance/**`。
5. `specs/**`。

当前 method、event、error、capability、profile 等事实位于 `contract/registry/**` 和生成产物中。候选规划位于 `workspace/registry-planning/**`，不是 runtime 合同。

## 关键词

| 关键词 | 含义 |
|---|---|
| MUST | 对相关 AXTP profile 来说是强制要求。 |
| SHOULD | 强烈建议；偏离时需要明确工程理由。 |
| MAY | 可选行为，或 profile 专属行为。 |
| RESERVED / FUTURE | 不是 v1 必需行为；不能作为 required profile claim 实现。 |
