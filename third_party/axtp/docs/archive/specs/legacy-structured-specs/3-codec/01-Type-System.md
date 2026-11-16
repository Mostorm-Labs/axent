# 3-codec/01《AXTP Type System 规范》

> 状态： AXTP v1 规范性 codec 规范
> 范围：scalar、string、bytes、enum、bitmap、array、object、optional/default 以及 JSON/TLV 类型映射规则
> 权威边界：本文定义 AXTP 数据类型；不定义 fieldId 分配或 TLV envelope 字节。

## 文档目的

本文档定义 AXTP registry schema 可使用的数据类型，以及这些类型在 JSON 表达、TLV 表达和 generated SDK 中的共同语义。它为 RPC params/result、event payload、capability object 和 CONTROL/RPC 内部 schema 提供一致类型基础。

## 范围

本文档覆盖基础数值、bool、string、bytes、enum、bitmap、array、object、optional/default、nullable 边界和类型约束。TLV 字段 envelope 由 [03-TLV-Encoding.md](03-TLV-Encoding.md) 定义；fieldId 分配由 [04-Schema-Numbering.md](04-Schema-Numbering.md) 定义；capability 模型由 [02-Capability-Types.md](02-Capability-Types.md) 定义。

## 规范规则

1. AXTP v1 多字节整数 MUST 使用 Big-Endian，即 network byte order。
2. `uint8`、`uint16`、`uint32`、`uint64` 和 `int8`、`int16`、`int32`、`int64` MUST 使用固定宽度二进制表示。
3. `bool` MUST 以 `0x00=false`、`0x01=true` 发送；接收端遇到其他值 SHOULD 作为 validation error 处理。
4. `string` MUST 使用 UTF-8，不使用 NUL 终止符；长度由外层 encoding 表达。
5. `bytes` 表示不解释的原始字节序列；其长度和最大值 MUST 由 schema 或外层 envelope 限制。
6. `enum` MUST 声明合法取值集合；接收端遇到未知 enum 值时，required 字段 SHOULD 失败，optional 字段 MAY 保留原值或忽略。
7. `bitmap` bit 编号 MUST 从低位开始，bit0 为最低有效位。
8. `array<T>` MUST 声明元素类型；TLV 中可使用 repeated TLV 或 packed array，具体编码由 schema 指定。
9. `object` MUST 引用具名 schema；线上 TLV 不携带字段名，字段含义依赖 schema。
10. AXTP v1 TLV 不定义独立 null sentinel。需要可缺省值时 SHOULD 使用 optional/default；JSON 表达只有在 schema 明确允许时 MAY 使用 null。
11. stable 字段的 type 语义 MUST NOT 以不兼容方式改变。

## Registry / Schema / Tooling 模型

常用类型集合：

| 类型 | JSON 表达 | TLV/二进制表达 | 说明 |
|---|---|---|---|
| `bool` | boolean | 1B | `0x00` 或 `0x01` |
| `uint8/16/32/64` | number or string for large values | fixed width BE / network byte order | 无符号整数 |
| `int8/16/32/64` | number or string for large values | fixed width BE / network byte order | 有符号补码整数 |
| `string` | string | UTF-8 bytes | 外层长度控制 |
| `bytes` | base64/hex/profile-specific | raw bytes | 不解释字节 |
| `enum` | string or numeric value | enum8/enum16 | registry 定义取值 |
| `bitmap` | array/string/numeric profile-specific | bitmap8/16/32/64/bytes | bit0 为 LSB |
| `array<T>` | JSON array | repeated or packed | schema 声明编码 |
| `object` | JSON object | nested TLV fields | fieldId 由 schema 定义 |

Generator MUST 将 registry schema 映射到目标语言类型，并保持线上类型与宿主语言类型分离。

## 校验规则

Runtime 或 generated codec MUST 校验：

1. 固定宽度数值长度正确；
2. 数值在声明范围内；
3. string 是合法 UTF-8，并满足 max length；
4. enum/bitmap 不包含未声明或不可接受的值；
5. array 元素类型一致，packed array 元素宽度可整除；
6. required 字段存在，optional 字段缺省时应用 default 或语言空值；
7. object 字段递归符合 schema。

## 兼容规则

- 新增 optional 字段通常兼容；新增 required 字段通常不兼容。
- 扩展 enum/bitmap MAY 兼容，但接收端必须有 unknown value 策略。
- 改变 stable 字段的基础类型、单位、范围或 required/optional 语义通常是 breaking change。
- 大整数在 JSON 中可能超过部分语言安全整数范围，SDK SHOULD 按语言约定保留精度。

## 实现要求

- Runtime MUST 使用 generated schema/codec 或与其等价的验证逻辑。
- SDK MUST 保持 unknown optional field 的跳过能力；高层实现 MAY 保存 unknown fields。
- Generator SHOULD 输出目标语言的类型、校验器和序列化约束。
- Conformance SHOULD 覆盖边界数值、非法 bool、非法 UTF-8、未知 enum、缺失 required 字段和 unknown optional field。

## 示例

```yaml
schemas:
  ExampleLevelConfig:
    kind: object
    fields:
      - id: 0x01
        name: level
        type: uint8
        required: true
        min: 0
        max: 100
      - id: 0x02
        name: applyDelayMs
        type: uint16
        required: false
        default: 0
```

`level=80` 的 uint8 值在线上为 `50`，`applyDelayMs=300` 的 uint16 Big-Endian / network byte order 值为 `01 2C`。

## 非目标 / 未来

本文档不定义 TLV envelope、fieldId 编号策略、capability discovery method 或 legacy adapter 类型映射。`float32`、`float64`、复杂 nullable 模型和 schema reflection 可作为未来扩展，但不得作为 AXTP v1 required runtime 能力。
