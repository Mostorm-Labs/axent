# 3-codec/04《AXTP Schema 字段编号规范》

> 状态： AXTP v1 规范性 codec 规范
> 范围：schema-local fieldId 分配、reserved/deprecated 字段、兼容性和校验规则
> 权威边界：本文只定义 fieldId 治理；不定义 TLV 字节 envelope 或业务 schema 内容。

## 文档目的

本文档定义 AXTP schema 字段编号 `fieldId` 的分配、复用、废弃、保留和兼容规则。TLV 线上不携带字段名，因此同一个 schema 内的 fieldId 一旦发布就成为兼容性合同。

## 范围

本文档覆盖 method params/result、event payload、capability object 和 STREAM context schema 的 schema-local fieldId 规则。CONTROL 固定字段或 frame/RPC/STREAM header 字段由 core specs 定义，不通过本文分配。

## 规范规则

1. `fieldId` 是 schema-local 编号；不同 schema MAY 复用相同 fieldId。
2. `fieldId=0x00` MUST 保留为 invalid，不得使用。
3. 普通业务字段 SHOULD 从 `0x01` 开始按自然顺序分配。
4. 同一 schema 内 fieldId MUST 唯一。
5. stable 字段的 fieldId MUST NOT 复用；字段废弃后 MUST 保留占位。
6. 新增字段 SHOULD 默认为 optional，以保持向后兼容。
7. stable required 字段 MUST NOT 删除；如需替代，新增字段并废弃旧字段。
8. reserved 字段 MUST NOT 被临时复用。
9. vendor 字段 MUST 限定作用域，不能影响标准字段解释。
10. fieldId 只解释当前 schema 的 TLV body；不得建立跨 schema 公共固定字段表。

## Registry / Schema / Tooling 模型

推荐范围：

| 范围 | 名称 | 用途 |
|---:|---|---|
| `0x00` | `INVALID` | 禁用，保留 |
| `0x01-0x5F` | `SCHEMA_FIELDS` | 当前 schema 普通字段 |
| `0x60-0x7E` | `EXTENSION_FIELDS` | 当前 schema 扩展字段 |
| `0x7F` | `VENDOR_DATA` | 厂商私有数据或 escape 字段 |
| `0x80-0xBF` | `RESERVED_EXTENDED` | 未来扩展字段编号机制 |
| `0xC0-0xEF` | `VENDOR_FIELDS` | 厂商私有字段 |
| `0xF0-0xFF` | `SYSTEM_RESERVED` | 系统保留，不建议业务使用 |

Schema 示例：

```yaml
types:
  ExampleLevelConfig:
    kind: object
    fields:
      - id: 0x01
        name: level
        type: uint8
        required: true
      - id: 0x02
        name: applyDelayMs
        type: uint16
        required: false
        default: 0
```

## 校验规则

Generator MUST 至少校验：

1. 同一 schema 内 fieldId 唯一；
2. `0x00` 和系统保留范围没有被普通字段使用；
3. deprecated/reserved 字段没有被新字段复用；
4. required/default/type/range 组合合法；
5. field name 使用 lowerCamelCase，schema name 使用 PascalCase；
6. fieldId 变更在 compatibility diff 中被标记为 breaking 或需要 review。

## 兼容规则

- 新增 optional 字段通常兼容。
- 新增 required 字段通常不兼容。
- 删除 required 字段、复用 deprecated fieldId、改变 stable 字段类型或单位通常不兼容。
- 重命名字段 MAY 兼容，前提是 fieldId、type、语义和 generated mapping 保持稳定；但 SDK API 仍需版本治理。
- Legacy mapping MAY 记录旧字段位置，但不能改变 AXTP fieldId 稳定性规则。

## 实现要求

- Runtime decoder MUST 依赖 fieldId，不得依赖字段顺序。
- Runtime MUST 跳过未知且 length 合法的 optional field。
- SDK SHOULD 保留 deprecated 字段的读取能力，并对新发送路径隐藏或标记 deprecated。
- Conformance SHOULD 覆盖新增 optional、缺失 required、重复字段、reserved 字段出现、unknown field 跳过和 fieldId 复用拒绝。

## 示例

兼容演进：

```yaml
fields:
  - id: 0x01
    name: level
    type: uint8
    required: true
  - id: 0x02
    name: applyDelayMs
    type: uint16
    required: false
    deprecated: true
  - id: 0x03
    name: applyPolicy
    type: enum
    required: false
```

`0x02` 被废弃后仍保留占位，新字段使用 `0x03`。

## 非目标 / 未来

本文档不定义 schema registry 的完整大表，也不定义 legacy 固定结构到 TLV 的完整迁移方案。legacy 字段映射应进入 `workspace/legacy-migration` 或 registry 条目的非 wire 元数据。
