# 4-tooling/03《AXTP 版本与兼容性规范》

> 状态： AXTP v1 规范性版本规范
> 范围：spec version、registry version、generated artifact 版本锁定、兼容性分类、breaking change review 和 release 绑定
> 权威边界：本文定义 AXTP 版本规则；legacy adapter 迁移细节为非规范参考。

## 文档目的

本文档定义 AXTP 的版本号、release artifact、兼容性和 breaking change 规则。它确保 specs、registry YAML、Protocol IR、generated artifacts、runtime 仓库和 conformance 使用同一个可追溯版本快照。

## 范围

本文档覆盖 spec version、registry version、schema/wire version、spec tag、release artifact、runtime 绑定、兼容变更、breaking change、deprecated/reserved 和 generated artifact version lock。本文档不承载 legacy adapter 的完整迁移设计；legacy 迁移细节应进入 `workspace/legacy-migration/**`。

## 规范规则

1. Runtime 仓库 MUST 绑定 spec tag、明确 commit 或 release artifact；不得依赖浮动 main。
2. `contract/registry/version.yaml` MUST 记录当前 spec、registry、schema、wire 等版本元数据。
3. `contract/protocol/axtp.protocol.yaml` 和 `contract/generated/**` MUST 与同一 source registry 版本生成。
4. stable methodId、eventId、errorCode、capabilityId、profile name 和 fieldId MUST NOT 复用。
5. 不兼容 wire format 变更 MUST 提升对应 wire/header/profile version，并经过 release review。
6. 兼容新增 SHOULD 优先使用 optional fields、新 method/event/error/capability 或新 profile，不得修改 stable 语义。
7. Reserved 字段、ID 或范围 MUST NOT 被业务草案临时占用。
8. Deprecated 条目 MUST 继续保留 generated 表达，直到 release policy 明确移除策略。
9. Protocol IR schema 的 breaking change MUST 同步 generator、generated docs、runtime consumers 和 conformance。

## Registry / Schema / Tooling 模型

版本层级：

| 版本 | 用途 |
|---|---|
| Spec tag | 例如 `spec/v0.0.x`，runtime 可绑定的发布快照 |
| Roadmap milestone | 例如 `v0.1`、`v0.2`、`v1.0`，只表示规划阶段 |
| `spec.version` | AXTP 规范版本 |
| `registry_version` | registry source model 版本 |
| `schema_version` | schema/model 版本 |
| `wire_version` | frame/session/codec wire compatibility 版本 |
| generated artifact version | 由 source registry 和 generator 共同决定 |

推荐 release chain：

```text
contract/registry/version.yaml
  -> contract/protocol/axtp.protocol.yaml
  -> contract/generated/protocol.md/json
  -> release artifact or spec tag
  -> runtime AXTP_SPEC.lock.yaml / pinned commit
  -> conformance result
```

## 校验规则

Release 或 CI SHOULD 校验：

1. registry version 与 Protocol IR metadata 一致；
2. generated docs/json 与 Protocol IR facts 一致；
3. stable ID/name/fieldId 没有被复用；
4. deprecated/reserved 条目仍在 generated artifacts 中可见；
5. runtime lock file 或 release metadata 指向明确 spec tag/commit；
6. conformance scope 与 declared profile 支持一致；
7. breaking change 已有版本 bump、migration note 和 release review。

## 兼容规则

兼容变更通常包括：

- 新增 optional schema field；
- 新增 method/event/error/capability/profile；
- 新增 enum/bitmap 值且接收端有 unknown 策略；
- 新增 generated 文档章节，不改变 Protocol IR facts。

不兼容变更通常包括：

- 改变 stable methodId、eventId、errorCode、capabilityId 或 fieldId；
- 删除 stable required field；
- 改变 stable 字段类型、单位、范围或 required 语义；
- 改变 frame/RPC/STREAM header wire layout；
- 改变 Hello/Identify/Identified 必选字段语义；
- 给 existing profile 增加新的 required capability，导致旧 runtime 无法通过。

## 实现要求

- Maintainers MUST 在合并 contract/registry/source 变更时检查 version impact。
- Runtime MUST 在文档或 lock file 中声明绑定的 spec tag/commit 和 supported profiles。
- SDK/generated consumers SHOULD 暴露 protocol/spec version，便于兼容性校验。
- Conformance SHOULD 记录被测 runtime 的 spec tag/commit、profile、transport 和 generated artifact version。

## 示例

兼容新增：

```text
AudioAlgorithmConfig 增加 optional fieldId=0x05
-> registry schema update
-> generated docs/json update
-> conformance optional-field case
-> no wire/header version bump
```

不兼容变更：

```text
把 stable fieldId=0x02 从 uint16 改成 string
-> breaking change
-> 需要新字段或新 schema/version，不得静默替换
```

## 非目标 / 未来

Legacy protocol sniffing、legacy CmdValue 到 AXTP method 的迁移、legacy status code 映射、legacy adapter C++ skeleton 和迁移阶段计划均为非规范迁移内容，应在 `workspace/legacy-migration/**` 中维护。本文只规定这些迁移不得污染 AXTP Core wire format、registry 稳定性和 generated implementation contract。
