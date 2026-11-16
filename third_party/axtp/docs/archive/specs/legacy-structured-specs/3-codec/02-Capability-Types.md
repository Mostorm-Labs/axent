# 3-codec/02《AXTP Schema 与 Capability 模型规范》

> 状态： AXTP v1 规范性 schema/capability 模型规范
> 范围：schema source 条目、capability 声明、capability discovery 边界和 registry 准入规则
> 权威边界：当前已采纳 schema/capability 事实来自 registry YAML、Protocol IR 和 generated artifacts。

## 文档目的

本文档定义 AXTP schema model 与 capability model 的边界。Schema 描述 RPC params/result、event payload 和 capability object 的数据结构；Capability 描述设备在当前固件、配置、会话和鉴权状态下可用的能力。

## 范围

本文档覆盖 `schemas:`/`types:` 源条目、schema 字段属性、capability 命名和能力声明边界。本文档不定义 CONTROL 建链字段，也不维护完整 CapabilityId 大表；历史和候选 capability 规划表保存在非规范附录：[../2-registry/appendix/capability-candidates.md](../2-registry/appendix/capability-candidates.md)。Runtime MUST NOT 从该 appendix 实现协议。

## 规范规则

1. Schema MUST 描述 params、result、event payload 或 capability object 的结构；schema 本身不分配 methodId/eventId。
2. Object schema 字段 MUST 有 schema-local fieldId；fieldId 兼容规则由 [04-Schema-Numbering.md](04-Schema-Numbering.md) 定义。
3. Schema 字段类型 MUST 来自 [01-Type-System.md](01-Type-System.md) 或引用已注册 schema。
4. Capability MUST 使用 `domain.feature` 命名，除非它是已采纳的 protocol/core capability。
5. Capability 描述业务能力或实现能力，不改变 Frame Header、PayloadType、CONTROL session、RPC op 或 STREAM header。
6. 影响 parser、frame、transport、payload 支持的运行时参数 MUST 在 CONTROL 或 transport profile 中表达；业务能力 MUST 通过 RPC capability method 或 generated registry 表达。
7. AXTP v1 Core 不强制内置完整业务能力发现 method。任何 capability discovery method 都必须由已采纳协议明确注册。
8. MethodId 存在不代表设备当前支持该 method；当前可用性必须由 capability、profile、鉴权或 generated registry 约束共同判断。

## Registry / Schema / Tooling 模型

### Schema 模型

最小 schema 条目模型：

```yaml
types:
  DeviceInfo:
    kind: object
    fields:
      - id: 0x01
        name: deviceId
        type: string
        required: true
```

| 字段 | 要求 | 说明 |
|---|---|---|
| `kind` | MUST | `object`、`enum`、`bitmap`、`alias`、`bytes` 等 |
| `fields[].id` | object 必需 | schema-local fieldId |
| `fields[].name` | object 必需 | 字段名，lowerCamelCase |
| `fields[].type` | MUST | 基础类型或 schema 引用 |
| `fields[].required` | MUST | 是否必填 |
| `fields[].default` | MAY | 缺省值 |
| `fields[].deprecated` | MAY | 废弃标记 |

Schema admission MUST 满足：params/result/event payload 可 schema 化，字段类型明确，错误模型明确，能够生成 Protocol IR、generated docs 和 conformance case。

### Capability 模型

最小 capability 条目模型：

```yaml
capabilities:
  - id: 0x0002
    name: protocol.payload.rpc
    domain: protocol
    status: mvp
    type: bool
    description: Device supports RPC payload.
```

| 字段 | 要求 | 说明 |
|---|---|---|
| `id` | 分配时 MUST | capabilityId，稳定后不得复用 |
| `name` | MUST | capability name，通常为 `domain.feature` |
| `domain` | MUST | 归属 domain |
| `status` | MUST | 生命周期状态 |
| `type` | SHOULD | bool、object、enum 或 capability schema 引用 |
| `description` | SHOULD | 能力说明 |

## 校验规则

Generator MUST 至少校验：

1. schema/type name 全局唯一；
2. object fieldId 在 schema 内唯一，且没有复用 deprecated/reserved 字段；
3. field type 引用存在；
4. required/default/range/enum 组合合法；
5. capability id 和 name 唯一；
6. capability name 符合 `domain.feature` 分类规则；
7. method/event/profile 对 schema 和 capability 的引用存在；
8. Protocol IR 与 generated docs 中的 schema/capability facts 与 source YAML 一致。

## 兼容规则

- 新增 optional schema 字段通常兼容；新增 required 字段通常不兼容。
- 废弃字段或 capability MUST 保留编号/名称占位，不得复用。
- 改变 capability 的语义、粒度或可用性含义可能是 breaking change。
- capability discovery 响应 MAY 随设备状态变化，但响应 schema 的 stable 字段语义不得改变。

## 实现要求

- Runtime MUST 使用 generated schema/codec 验证 params/result/event payload。
- Runtime MUST NOT 在 CONTROL OPEN/ACCEPT 中塞入业务 capability 树。
- SDK SHOULD 从 generated artifacts 暴露 capability metadata，但不得把 appendix 候选表当实现合同。
- Conformance SHOULD 覆盖 schema validation、capability availability、unknown capability 和 deprecated capability 行为。

## 示例

```text
audio.algorithm             # 业务能力
protocol.payload.rpc        # 协议 payload 支持能力
capability.registry         # 已采纳能力发现能力时才可出现
```

业务配置能力常见方法组合：

```text
get<Feature>Capabilities
get<Feature>Config
set<Feature>Config
reset<Feature>Config
<feature>ConfigChanged
```

## 非目标 / 未来

完整 `capability.getAll`、能力树反射、复杂事件/流/profile 协商属于 future 或独立草案，不是 AXTP v1 Core 必选能力。若未来需要拆分文件，建议将 Schema Model 独立为 `3-codec/02-Schema-Model.md`，Capability Model 独立为 `2-registry` 或 `3-codec` 下的 capability spec；本阶段只在文档内部明确边界。
