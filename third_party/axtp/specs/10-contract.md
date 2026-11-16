# AXTP 合同边界

本文定义事实源顺序和冲突处理规则。它不定义新的 wire 字段、注册表条目、生成协议事实或发布流程。

## 实现合同

Runtime、SDK、mock server 和测试工具按照以下顺序消费 AXTP 合同：

1. 已发布的 spec artifact，或精确的 `spec/vMAJOR.MINOR.PATCH` tag。
2. `contract/protocol/axtp.protocol.yaml`。
3. `contract/generated/protocol.md` 和 `contract/generated/protocol.json`。
4. `conformance/**`。
5. `specs/**`。

`specs/**` 解释设计规则和兼容边界。当前可调用 method、event、schema、error、capability、profile、id 和生成元数据来自 `contract/registry/**`、Protocol IR 和生成产物。

Runtime MUST NOT 实现仅存在于 `workspace/protocol/**` 的草案条目，除非它明确是在构建 mock 或 prototype。

## 维护来源

协议维护者通过以下来源修改协议事实：

```text
specs/**                         # 标准规则
contract/registry/**/*.yaml       # 手写机器事实
tooling/generators/**             # 确定性的 source-to-contract 工具
contract/protocol/**              # 生成的 Protocol IR
contract/generated/**             # 生成参考
conformance/**                    # 行为验收输入
```

生成产物不是手写输入。如果生成产物错误，应修正 specs、YAML 或 generator 行为，然后重新生成。

## 非合同材料

以下路径属于评审输入、规划、归档或工作流材料：

```text
workspace/business/**
workspace/flows/**
workspace/protocol/** before adoption
workspace/registry-planning/**
workspace/legacy-migration/**
docs/product/roadmap.md
docs/archive/**
tooling/skills/**
```

它们可以辅助评审，但不能绕过采纳、注册表更新、生成和 conformance。

## 冲突规则

| 冲突 | 以谁为准 | 处理动作 |
|---|---|---|
| 生成产物 vs draft | 生成产物 | 修正 draft 或采纳路径。 |
| 生成产物 vs specs/YAML | YAML/spec/generator source | 修正 source 或 generator，然后重新生成。 |
| Conformance vs specs/generated | 正式合同 | 修正 conformance 或合同；不要让 runtime 绕过矛盾。 |
| Roadmap vs generated contract | 生成合同 | Roadmap 必须先进入 business、flow、draft 和 registry，才能实现。 |
| Legacy mapping vs generated contract | 生成合同 | 标记为 blocked、adapter-private，或进入 migration review。 |

## 会话边界

AXTP 明确区分 link、RPC 和 stream 上下文：

| 上下文 | 创建方式 | 用途 |
|---|---|---|
| Transport connection | Transport connect/accept | 原始承载。 |
| Framed Link Context | CONTROL OPEN / ACCEPT | Standard Framed runtime link。 |
| RPC Session | Hello / Identify / Identified | 通过 `sid` 路由的业务 session。 |
| Stream Context | 已采纳的 RPC method 或 profile | STREAM payload 的解释上下文。 |

CONTROL 的 `sessionId` 不是 `sid`。`sid` 是业务 RPC Session ID。`messageId` 不是 `requestId`。`streamId` 不是 `requestId`。

## 业务采纳路径

业务协议事实只能单向流动：

```text
workspace/business or workspace/flows
  -> workspace/protocol/<domain>/<domain.feature>.md
  -> contract/registry/domains/<domain>/domain.yaml
  -> contract/protocol/axtp.protocol.yaml
  -> contract/generated/**
  -> conformance 与 runtime 实现
```

`domain.feature` 决定业务归属。`method`、`event`、`schema`、`error`、`capability` 和 `profile` 决定机器可读事实。
