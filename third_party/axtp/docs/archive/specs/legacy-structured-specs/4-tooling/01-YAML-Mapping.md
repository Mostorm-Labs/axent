# 4-tooling/01《AXTP YAML 到 Protocol IR 映射规范》

> 状态： AXTP v1 规范性 tooling 合同
> 范围：source registry YAML 加载、归一化、校验和 Protocol IR 映射
> 权威边界：本文定义 registry YAML 如何成为 Protocol IR；不完整定义 registry ID 或 generated 输出格式。

## 文档目的

本文档定义 AXTP source registry YAML 到 `contract/protocol/axtp.protocol.yaml` Protocol IR 的映射规则。它的职责是保证业务模块从 flow/protocol draft 进入 registry 后，能够被 generator 稳定地归一化、校验、生成文档、生成 SDK/runtime metadata，并进入 conformance。

## 范围

本文档覆盖：

- `contract/registry/core/**`、`contract/registry/error/**`、`contract/registry/schema/**`、`contract/registry/capability/**`、`contract/registry/domains/**` 的输入关系；
- source YAML 到 Protocol IR 的字段归一化；
- method/event/schema/error/capability/profile 的引用校验；
- 业务模块进入正式协议的准入标准。

本文档不覆盖完整 Method/Event/Error/Profile/Capability 大表，也不定义 legacy adapter 迁移细节。legacy 迁移材料应进入 `workspace/legacy-migration/**`。

## 规范规则

1. Registry YAML 是 generator 的人工维护 source model；`contract/protocol/axtp.protocol.yaml` 是 generated Protocol IR，MUST NOT 手写修改。
2. `contract/generated/**` 是 generated implementation view，MUST NOT 手写修改。
3. Generator MUST 从 source YAML 聚合出单一 Protocol IR；不得从 `workspace/protocol`、appendix 或 generated markdown 反推协议事实。
4. Domain YAML 中的业务条目与 core registry 条目在进入 Protocol IR 后同属正式机器事实，但 profile/status 决定其是否为 Core/MVP required。
5. 同一 method/event/error/schema/capability/profile 不得在 core registry 和 domain YAML 重复定义。
6. Source YAML 中的 id、name、domain、schema、error、capability、profile 引用 MUST 在生成前完成一致性校验。
7. Protocol IR MUST 保留足够信息用于生成 human-readable docs、machine-readable JSON、SDK/runtime metadata 和 conformance 输入。
8. business draft 只有满足准入标准并写入 registry YAML 后，才成为可生成协议事实。

## Registry / Schema / Tooling 模型

三段式输入输出关系：

```text
contract/registry/**/*.yaml
  -> validate-sources
  -> contract/protocol/axtp.protocol.yaml
  -> validate-protocol
  -> contract/generated/protocol.md
  -> contract/generated/protocol.json
  -> runtime/tooling generated outputs
```

Source model 分层：

| 输入 | 职责 |
|---|---|
| `contract/registry/core/*.yaml` | PayloadType、RPC op、encoding、CONTROL opcode 等 core 枚举 |
| `contract/registry/error/error_code.yaml` | Core/shared error registry |
| `contract/registry/schema/*.yaml` | Core/shared schema |
| `contract/registry/capability/*.yaml` | Core/shared capability 与 profile |
| `contract/registry/domains/<domain>/domain.yaml` | 业务 domain 的 method/event/type/error/capability/profile |
| `contract/registry/version.yaml` | spec/contract/registry/schema/wire version 元数据 |

业务模块进入正式协议的准入标准：

1. 有明确 `domain.feature`；
2. 有 method/event/schema/capability 定义，且名称符合 taxonomy；
3. params/result/event payload 可 schema 化；
4. 错误模型明确，method `errors[]` 可解析；
5. ID 分配符合 registry 规则，stable 后不复用；
6. 能生成 Protocol IR；
7. 能生成 `contract/generated/**`；
8. 能写 conformance case 或说明无需新增 conformance 的理由。

## 校验规则

`validate-sources` MUST 至少执行：

1. YAML 可解析，schema 形状符合 generator source model；
2. id/name 全局唯一性检查；
3. domain 前缀、`domain.feature`、method/event/capability 命名检查；
4. request/response/event schema 引用存在；
5. error/capability/profile 引用存在；
6. fieldId、bitOffset、status、since 的基础一致性检查；
7. core registry 与 domain YAML 无重复事实；
8. generated target 不需要手写修改。

`validate-protocol` MUST 校验 Protocol IR 与 source model、core specs 和 generated 输出的关键事实一致。

## 兼容规则

- Source YAML 中 stable id/name/fieldId 变更 MUST 被 compatibility diff 标记，并走 breaking change review。
- Draft/experimental 条目 MAY 调整，但进入 stable 前必须完成 ID、schema、error、conformance review。
- Protocol IR 的结构演进 MUST 保持 generator 和 runtime consumers 可识别版本；不兼容 IR 变更需要 release note 和 version bump。
- legacy mapping MAY 作为 source 元数据保留，但不得改变 AXTP 正式 wire format。

## 实现要求

- Generator MUST 确保输出确定性：同一输入产生 byte-for-byte 等价的 generated artifacts，除非模板版本变化。
- CI SHOULD 在 registry 或 generator 修改后运行 source validation、Protocol IR build、Protocol IR validation 和 generated diff 检查。
- Runtime MUST 绑定 spec tag、明确 commit 或 release artifact，不得依赖浮动 main 的 Protocol IR。
- Protocol maintainer 在合并 registry 修改时 MUST 同步 generated artifacts 和 conformance，或在审查记录中说明无需同步。

## 示例

```text
新增 audio.algorithm method:
  workspace/protocol/audio/audio.algorithm.md review-ok
  -> contract/registry/domains/audio/domain.yaml 增加 method/type/event/error/capability
  -> tooling/generators validate-sources
  -> build contract/protocol/axtp.protocol.yaml
  -> emit contract/generated/protocol.md/json
  -> add or update conformance case
```

常用命令由 generator package 提供：

```bash
cd tooling/generators
pnpm validate:sources
pnpm build:protocol
pnpm validate:protocol
pnpm emit:protocol
```

## 非目标 / 未来

本文档不作为 registry governance 的唯一入口；命名和 ID 稳定性规则分别在 `2-registry/**` 与 `3-codec/**` 中定义。Codex skill 工作流应放在 `tooling/skills/**`。legacy adapter 的分类、映射和测试向量应放在 `workspace/legacy-migration/**`。
