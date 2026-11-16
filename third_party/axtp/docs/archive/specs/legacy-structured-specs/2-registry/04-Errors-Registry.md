# 2-registry/04《AXTP Error Registry 规范》

> 状态： AXTP v1 规范性 registry 规范
> 范围：error registry 条目、errorCode 分配、RPC/CONTROL/STREAM 错误映射和准入规则
> 权威边界：当前已采纳 error 事实来自 registry YAML、Protocol IR 和 generated artifacts。

## 文档目的

本文档定义 AXTP error registry 的准入、编号、稳定性和映射规则。ErrorCode 是 CONTROL、RPC、STREAM 和业务错误模型共用的机器可判断错误编号空间。

## 范围

本文档覆盖 error 条目结构、errorCode 分段、RPC Response、CONTROL status、STREAM 错误原因与 error registry 的映射，以及 errorCode 稳定性、废弃和兼容规则。本文档不维护完整 ErrorCode 大表；历史和候选规划表保存在非规范附录：[appendix/error-candidates.md](appendix/error-candidates.md)。Runtime MUST NOT 从该 appendix 实现协议。

## 规范规则

1. `0x0000` MUST 表示成功语义；失败 MUST 使用非零 errorCode。
2. errorCode MUST 是全局唯一的 `uint16`。stable errorCode MUST NOT 改变语义或复用。
3. Error name MUST 全局唯一，SHOULD 使用全大写枚举风格。
4. RPC 业务失败 SHOULD 通过 RPC Response 表达，不得用 Frame Header 或 CONTROL NACK 替代。
5. CONTROL 协商失败、frame 解析失败、transport 限制失败 SHOULD 使用 common/frame/control/stream 类错误。
6. STREAM 数据面错误 MAY 通过 stream-specific status、CONTROL 错误或业务协商定义返回，但 reason MUST 引用 error registry 中的稳定 errorCode。
7. Legacy Adapter 错误映射 MUST 保持在 legacy 范围或 legacy mapping 中，不得改变 AXTP Core 错误语义。
8. 新增业务专属 error 默认进入 `contract/registry/domains/<domain>/domain.yaml`；跨 domain、Core/MVP 或协议层错误进入核心 error registry。

## Registry / Schema / Tooling 模型

最小 error 条目模型：

```yaml
errors:
  - id: 0x000A
    name: INVALID_ARGUMENT
    domain: common
    status: mvp
    description: Argument is invalid.
    retryable: false
```

| 字段 | 要求 | 说明 |
|---|---|---|
| `id` | MUST | `uint16` errorCode；部分文档或 IR 中可称为 `code` |
| `name` | MUST | error enum name，全局唯一 |
| `domain` / `category` | MUST | 错误归属，用于分段、文档和生成 |
| `status` | MUST | 生命周期状态 |
| `description` / `message` | SHOULD | 默认英文描述 |
| `retryable` | SHOULD | 客户端是否可重试 |
| `severity` | MAY | 日志或诊断严重程度 |

保留分段沿用当前 registry 规划：

| 范围 | 归属 |
|---:|---|
| `0x0000-0x00FF` | common / frame / control / rpc |
| `0x0100-0x05FF` | device / capability / system / firmware / stream |
| `0x0600-0x15FF` | 业务 domains |
| `0x7000-0x7EFF` | vendor |
| `0x7F00-0x7FFF` | legacy adapter |

## 校验规则

Generator MUST 至少校验：

1. error numeric id 全局唯一，且处于允许范围；
2. error name 全局唯一；
3. stable/deprecated/reserved errorCode 没有被新条目复用；
4. method `errors[]` 引用存在；
5. Protocol IR、generated docs 和 source YAML 中的 error facts 一致；
6. legacy/vendor error 不进入 common/core 范围。

## 兼容规则

- 新增 error 通常是兼容变更；改变 stable errorCode 的语义、名称或 retry 语义可能是 breaking change。
- deprecated error MUST 继续生成 enum 和文档，直到对应兼容窗口结束；编号不得复用。
- reserved code MUST NOT 被业务草案临时占用。
- 细粒度错误信息 SHOULD 放入 RPC status details、event payload、diagnostic payload 或 vendor detail 字段，不得通过新增大量重复 errorCode 表达。

## 实现要求

- Runtime MUST 使用 generated error enum/lookup，不得在各语言 runtime 中维护手写第二套 error 表。
- Runtime 返回错误时 SHOULD 同时携带机器可判断 code 和人可读 message。
- SDK SHOULD 将 unknown errorCode 映射为未知错误类型并保留原始 code。
- Conformance tests SHOULD 覆盖成功 code、常见 RPC error、frame/control error 和 unknown error 保留行为。

## 示例

```text
RPC invalid params       -> INVALID_ARGUMENT or RPC_PARAM_INVALID
Unsupported method       -> NOT_SUPPORTED or RPC_METHOD_NOT_FOUND
Frame CRC failure        -> FRAME_CRC_ERROR
Legacy adapter failure   -> legacy range errorCode plus legacy detail
```

## 非目标 / 未来

本文档不列出完整 ErrorCode 规划表。历史 planning/current 表已移至 [appendix/error-candidates.md](appendix/error-candidates.md)，仅用于审计和迁移参考。正式实现 MUST 以 `contract/registry/**/*.yaml`、`contract/protocol/axtp.protocol.yaml` 和 `contract/generated/**` 为准。
