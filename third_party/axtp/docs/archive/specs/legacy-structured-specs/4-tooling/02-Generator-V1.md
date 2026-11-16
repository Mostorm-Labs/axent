# 4-tooling/02《AXTP Generator v1 规范》

> 状态： AXTP v1 规范性 tooling 合同
> 范围：generator 输入、输出、校验阶段、generated artifact 规则、CLI 和 CI 要求
> 权威边界：本文定义 generator 行为；不定义协议 wire 语义。

## 文档目的

本文档定义 AXTP Generator v1 的实现合同：从 registry YAML 读取 source model，生成 Protocol IR、generated reference、machine-readable JSON、runtime/tooling metadata，并对 source 和输出进行验证。

## 范围

本文档覆盖 generator 输入、输出、阶段、失败条件、CLI、CI 和不可手写 generated 规则。本文档不覆盖具体 method/event/error/capability 大表，不承载 Codex skill 操作流程，也不规定 runtime 内部实现。

## 规范规则

1. Generator MUST 只从 registry YAML 和 generator 自身模板/代码读取协议事实。
2. Generator MUST NOT 从 `contract/generated/**`、`workspace/protocol/**`、appendix 或 runtime generated files 反推 source facts。
3. `contract/protocol/axtp.protocol.yaml`、`contract/generated/**`、runtime generated outputs 和 `contract/mcp/*.generated.json` MUST 被视为 generated artifacts，不得手写修改。
4. Generator MUST 先 validate source model，再 build Protocol IR，再 validate Protocol IR，再 emit generated artifacts。
5. Generator 输出 MUST 尽可能确定性排序，避免无意义 diff。
6. Generator MUST 在引用缺失、id 冲突、fieldId 冲突、重复 name、非法 status、非法 profile 引用、core/domain 重复事实时失败。
7. Generator MUST 保留 deprecated/reserved 条目的 generated enum/metadata，以支持兼容。
8. Generator MUST NOT 改变协议语义；语义变更必须先修改 source YAML/spec 并通过 review。

## Registry / Schema / Tooling 模型

生成管线：

```text
contract/registry/**/*.yaml
  -> validate-sources
  -> build-protocol
  -> contract/protocol/axtp.protocol.yaml
  -> validate-protocol
  -> emit-protocol
  -> contract/generated/protocol.md
  -> contract/generated/protocol.json
  -> optional runtime/tooling generated outputs
```

输入：

| 输入 | 用途 |
|---|---|
| `contract/registry/core/**` | core enum/opcode/encoding 事实 |
| `contract/registry/error/**` | error 事实 |
| `contract/registry/schema/**` | shared schema 事实 |
| `contract/registry/capability/**` | shared capability/profile 事实 |
| `contract/registry/domains/**` | business domain 事实 |
| `contract/registry/version.yaml` | version 元数据 |

输出：

| 输出 | 类型 | 手写 |
|---|---|---|
| `contract/protocol/axtp.protocol.yaml` | Protocol IR | MUST NOT |
| `contract/generated/protocol.md` | 人可读 generated reference | MUST NOT |
| `contract/generated/protocol.json` | 机器可读 generated reference | MUST NOT |
| `contract/mcp/*.generated.json` | tooling generated 数据 | MUST NOT |
| runtime generated headers/types | runtime 实现视图 | MUST NOT |

## 校验规则

Generator MUST 至少提供以下失败条件：

1. source YAML 解析失败；
2. source model shape 无效；
3. id/name/bitOffset/fieldId 在不允许的 scope 中重复；
4. schema/error/event/method/capability/profile 引用缺失；
5. status transition 或 stable id 复用违规；
6. 不支持的 type，或非法 enum/bitmap/range/default；
7. transport/profile 约束违规；
8. Protocol IR 缺少必需顶层 section；
9. generated docs/json 与 Protocol IR 事实不匹配。

## 兼容规则

- 仅修改 Generator template SHOULD NOT 改变 Protocol IR 语义事实。
- Source fact 变更 MUST 产生可见的 Protocol IR/generated diff。
- Breaking source change 在合并前 MUST 要求 version/release review。
- 除非 source YAML 在治理流程下明确 deprecated，Generator v1 MUST 保留旧 stable generated identifier。

## 实现要求

- CLI SHOULD 至少暴露 `validate`、`validate-sources`、`build-protocol`、`validate-protocol`、`emit-protocol`、`generate`、`generate-registry` 或等价命令。
- PR 触及 registry、影响 source contract 的 specs、generator code 或 generated artifact 时，CI SHOULD 运行 generator validation。
- Generator error SHOULD 包含 source file path 和 logical object name。
- Generated 文件在支持时 SHOULD 包含 generated 标记注释。
- Maintainer MUST 将 generated diff 作为 implementation contract diff 审查，而不是当作手写 prose 审查。

## 示例

当前 package scripts 暴露了预期流程：

```bash
cd tooling/generators
pnpm validate
pnpm validate:sources
pnpm build:protocol
pnpm validate:protocol
pnpm emit:protocol
pnpm generate
```

对于 registry 变更，`pnpm generate` SHOULD 同时刷新 Protocol IR 和 generated reference。

## 非目标 / 未来

Codex skill workflow 属于 `tooling/skills/**`，不是 generator 的规范性行为。更丰富的 runtime codegen、docs UX 改进、compatibility dashboard 和 generated source manifest 等 P1/P2 roadmap 项，在实现并采纳前应保持为 future/tooling planning。
