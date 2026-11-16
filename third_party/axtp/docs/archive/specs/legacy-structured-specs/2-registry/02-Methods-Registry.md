# 2-registry/02《AXTP Method Registry 规范》

> 状态： AXTP v1 规范性 registry 规范
> 范围：`methods:` 条目、methodId 分配、request/response schema 绑定和 method 准入规则
> 权威边界：当前已采纳 method 事实来自 registry YAML、Protocol IR 和 generated artifacts。

## 文档目的

本文档定义 AXTP method registry 的准入、命名、编号、稳定性和生成规则。Method 是 RPC 层的业务调用入口；线上 JSON 使用 method name，二进制 RPC 使用 `methodId:uint16`，二者 MUST 指向同一 registry 事实。

## 范围

本文档覆盖 `contract/registry/domains/<domain>/domain.yaml` 与 core method registry 中 `methods:` 条目的要求，包括 method name、methodId、bitOffset、request schema、response schema、error、event 和 capability 引用规则。本文档不维护完整 MethodId 大表；历史和候选规划表保存在非规范附录：[appendix/method-candidates.md](appendix/method-candidates.md)。Runtime MUST NOT 从该 appendix 实现协议。

## 规范规则

1. Method MUST 通过 `PayloadType=RPC` 承载；methodId MUST NOT 出现在 Frame Header、CONTROL payload 或 STREAM payload header 中。
2. Method name MUST 全局唯一，并采用 `domain.actionObject` 形式；name 前缀 MUST 与条目的 `domain` 一致。
3. `methodId` MUST 是全局唯一的 `uint16`。stable methodId MUST NOT 改变语义或复用。
4. `bitOffset` MUST 在同一 `domain` 内唯一，并用于 domain-scoped method bitmap 或 generated metadata。stable bitOffset MUST NOT 复用。
5. 每个 method MUST 绑定 request schema 和 response schema；空请求或空响应 MUST 使用已注册的 empty schema 约定，不得省略 schema 关系。
6. 每个 method MUST 声明可能返回的 errors，且每个 error MUST 能解析到 error registry。
7. 如果 method 成功后可能触发事件，events 引用 MUST 指向已注册 event。
8. Method MAY 关联 capability，但 capability 只描述设备能力或可用性，不改变 methodId 或 RPC wire format。
9. 新增业务 method 默认进入 `contract/registry/domains/<domain>/domain.yaml`。只有 Core/shared 采纳后才进入核心 method registry；同一 method 不得在 domain YAML 与 core registry 重复定义。
10. 连续数据 MUST 通过 STREAM 承载；method 只能用于协商、控制、查询、开始/停止或返回有限大小结果。

## Registry / Schema / Tooling 模型

最小 method 条目模型：

```yaml
methods:
  - id: 0x0901
    name: audio.getAlgorithmConfig
    domain: audio
    status: stable
    bitOffset: 1
    since: 1.0.0
    request_schema: AudioGetAlgorithmConfigRequest
    response_schema: AudioAlgorithmConfig
    errors:
      - SUCCESS
      - NOT_SUPPORTED
      - INVALID_ARGUMENT
```

| 字段 | 要求 | 说明 |
|---|---|---|
| `id` | MUST | `uint16` methodId，二进制 RPC 使用 |
| `name` | MUST | JSON/RPC method name，全局唯一 |
| `domain` | MUST | 业务 domain，必须与 name 前缀一致 |
| `status` | MUST | 生命周期状态 |
| `bitOffset` | 已采纳 bitmap 中 SHOULD/MUST | domain 内 method bitmap 位号；采用 bitmap 能力发现时为 MUST |
| `since` | MUST | 首次进入 registry 的 spec/registry 版本 |
| `request_schema` | MUST | 请求 schema 引用 |
| `response_schema` | MUST | 响应 schema 引用 |
| `errors` | MUST | 可能返回的 error name 列表 |
| `events` | MAY | 调用后可能触发的 event name 列表 |
| `capabilities` | MAY | 关联的 `domain.feature` capability |
| `legacy` | MAY | legacy 映射元数据，不参与 wire format |

## 校验规则

Generator MUST 至少校验：

1. `id` 在所有 method 中唯一，并处于允许的 methodId 范围；
2. `name` 全局唯一，且 `domain` 与 name 前缀一致；
3. 同一 domain 内 `bitOffset` 唯一；
4. `request_schema`、`response_schema`、`errors[]`、`events[]`、`capabilities[]` 引用存在；
5. stable/deprecated/reserved methodId 没有被新条目复用；
6. method 所属 domain/feature 已通过 naming taxonomy review；
7. Protocol IR 与 generated docs 中的 method facts 与 source YAML 一致。

## 兼容规则

- 新增 method 通常是兼容变更；修改 stable methodId、name、request/response schema 含义或错误语义通常是 breaking change。
- 废弃 method MUST 保留 id 和 generated enum，状态标记为 `deprecated`，不得把 id 分配给新 method。
- 旧协议映射只能作为 adapter 元数据或迁移输入；runtime 的 AXTP 实现合同以 registry YAML、Protocol IR 和 generated artifacts 为准。
- 可选 method bitmap 的第 N bit MUST 对应该 domain 内 `bitOffset=N` 的 method。

## 实现要求

- Runtime MUST 使用绑定版本的 generated registry 或 Protocol IR 解析 methodId/name，不得维护手写第二套 method 表。
- Runtime 遇到未知 methodId/name SHOULD 返回已注册的 method-not-found 或 not-supported 错误，而不是猜测业务语义。
- SDK SHOULD 从 generated artifacts 生成强类型 request/response API。
- Conformance tests SHOULD 覆盖 method id/name 映射、schema validation、错误返回和关联事件。

## 示例

```text
audio.getAlgorithmConfig     -> RPC request/response
video.openStream             -> RPC 协商 streamId，后续数据走 STREAM
file.readChunk               -> 不推荐；大块连续数据应走 STREAM
```

## 非目标 / 未来

本文档不列出完整 MethodId 规划表。历史 planning/current 表已移至 [appendix/method-candidates.md](appendix/method-candidates.md)，仅用于审计和迁移参考。正式实现 MUST 以 `contract/registry/**/*.yaml`、`contract/protocol/axtp.protocol.yaml` 和 `contract/generated/**` 为准。
