# 3-codec/03《AXTP TLV 编码规范》

> 状态： AXTP v1 规范性 codec 规范
> 范围：TLV field envelope、基础类型/object/array 编码、unknown field 处理、canonical encoding 和 parser error
> 权威边界：本文只定义 TLV 编码；不定义业务 schema 内容、methodId、eventId 或 capabilityId。

## 文档目的

本文档定义 AXTP TLV schema encoding。TLV 在线上只传 `fieldId`、`length` 和 `value`；字段名、类型、required/default 规则来自 registry schema。

## 范围

本文档覆盖 Basic TLV、Extended Length TLV、Empty Value、基础类型值编码、array/object 编码、optional/default、unknown field、canonical encoding 和解析错误。业务 method、event、capability schema 的字段语义由 registry YAML 和 generated docs 定义。

## 规范规则

1. Basic TLV MUST 使用 `fieldId:uint8 + length:uint8 + value:bytes[length]`。
2. 当 `length=0xFF` 时，后续 MUST 是 `extendedLength:uint16` Big-Endian / network byte order，再跟随 `value:bytes[extendedLength]`。
3. `fieldId=0x00` MUST NOT 用作普通字段。`fieldId=0xFF` 在 Basic TLV 中保留给 extended length marker，不得作为普通字段。
4. 多字节数值 MUST 使用 Big-Endian，即 network byte order。
5. 单个 TLV value 超过 `uint16` extended length 能表达的大小时 MUST 使用 STREAM 或业务分块，不得塞入单个 TLV 字段。
6. scalar 字段默认 strict mode；重复出现 SHOULD 报 duplicate field，除非 schema 明确声明 repeated array 或 legacy duplicate policy。
7. 接收端遇到未知 fieldId 时，若 length 合法且不越界，MUST 跳过，不得因未知 optional field 失败。
8. Required 字段缺失 MUST 作为 schema validation failure。
9. Canonical encoding SHOULD 按 fieldId 升序输出字段；解析器 MUST NOT 依赖字段顺序。

## Registry / Schema / Tooling 模型

TLV envelope：

```text
Basic TLV:       fieldId(1) + length(1) + value(N)
Extended TLV:    fieldId(1) + FF + extendedLength(2, BE) + value(N)
```

常用 value 编码：

| 类型 | TLV value |
|---|---|
| `uint8/int8/bool` | 1B |
| `uint16/int16` | 2B Big-Endian / network byte order |
| `uint32/int32` | 4B Big-Endian / network byte order |
| `uint64/int64` | 8B Big-Endian / network byte order |
| `enum8/enum16` | 1B 或 2B Big-Endian / network byte order |
| `bitmap8/16/32/64` | 固定宽度 Big-Endian / network byte order |
| `string` | UTF-8 字节 |
| `bytes` | 原始 bytes |
| `object` | 嵌套 TLV sequence |
| `array` | 按 schema 声明使用 repeated TLV 或 packed array |

## 校验规则

Parser/codec MUST 校验：

1. TLV header 不越界；
2. extended length 不越界；
3. value length 与 declared type 固定宽度匹配；
4. bool、UTF-8、enum、range、array 元素宽度合法；
5. required 字段存在；
6. duplicate field 符合 schema policy；
7. nested object depth 和 total decoded size 在实现限制内。

## 兼容规则

- 新增 optional TLV field 通常兼容，因为旧解析器会跳过 unknown field。
- 改变 stable fieldId 或 type 通常不兼容。
- deprecated 字段仍应可解析；新实现 SHOULD 不再发送。
- Canonical encoding 改变不应改变语义；测试向量 SHOULD 使用 canonical order。

## 实现要求

- Runtime MUST 对 CONTROL/RPC TLV body 执行长度边界检查，防止越界读取和内存放大。
- Runtime SHOULD 针对最大 TLV body size、最大 nested depth、最大 repeated count 设置实现限制。
- Generator SHOULD 从 schema 生成 TLV encoder/decoder 和 validation cases。
- SDK MAY 使用 JSON 作为应用层结构，但与 TLV 的转换 MUST 遵守 schema。

## 示例

```text
01 01 50        # fieldId=0x01, length=1, uint8 value=80
02 02 01 2C     # fieldId=0x02, length=2, uint16 value=300
10 FF 01 2C ... # fieldId=0x10, extendedLength=300
```

对象是多个 TLV 字段的串联；array 可用同一 fieldId 重复出现表示 repeated elements。

## 非目标 / 未来

本文档不定义业务 schema、大型业务建流示例、legacy adapter payload 映射或新的 PayloadType。可靠分片、压缩、加密和超过 `uint16` length 的对象传输应通过 STREAM/profile-specific 机制处理。
