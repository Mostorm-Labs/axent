# AXTP 编码

本文定义 AXTP 数据类型、schema 模型、capability 模型、TLV 编码和 schema 字段编号。当前 schema 事实位于 `contract/registry/**` 和生成产物中。

## 类型系统

所有 AXTP 多字节 wire integer MUST 使用 Big-Endian / network byte order。

| 类型 | JSON 表达 | Binary/TLV 表达 | 规则 |
|---|---|---|---|
| `bool` | boolean | 1B | `0x00=false`，`0x01=true`；其他值 SHOULD 校验失败。 |
| `uint8/16/32/64` | number；大值可用 string | 固定宽度 unsigned BE | 范围 MUST 匹配类型宽度和 schema constraints。 |
| `int8/16/32/64` | number；大值可用 string | 固定宽度 signed BE | 二进制补码有符号整数。 |
| `string` | string | UTF-8 bytes | 无 NUL terminator；长度来自外层编码。 |
| `bytes` | base64/hex/profile-specific | raw bytes | Opaque bytes；schema/profile 限制长度。 |
| `enum` | string 或 numeric value | enum8/enum16/profile-specific | 合法值 MUST 已声明。 |
| `bitmap` | array/string/numeric profile-specific | bitmap8/16/32/64/bytes | bit0 是最低有效位。 |
| `array<T>` | JSON array | repeated 或 packed | Element type MUST 已声明。 |
| `object` | JSON object | nested TLV fields | 字段含义来自 schema。 |

AXTP v1 TLV 没有独立 null sentinel。缺失值使用 optional/default 表达。JSON `null` 只在 schema 显式允许时可用。

稳定字段的 type、unit、required/optional 语义和 range MUST NOT 发生不兼容变化。

JSON、TLV 和生成 SDK 视图是同一个 schema fact 的不同投影。它们 MUST 保留相同的 required/optional 状态、default 语义、enum values、array item type、range、unit 和 deprecation metadata。无法在声明编码之间一致表达的字段 MUST 被校验拒绝，或标记为 profile-specific。

## Schema 模型

Object schema fields MUST 拥有 schema-local `fieldId`。Field id 不是全局 ID。

最小 object 形状：

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

Schema 规则：

1. Required fields MUST present，即必需字段必须出现在编码数据中。
2. Optional fields MAY omitted；接收方只在 schema 声明 default 时应用默认值。
3. Unknown、structurally valid optional fields MUST 可跳过；成功跳过字段 MUST NOT 使 message 或 session 失效。
4. Object fields 引用 built-in types 或 named schemas。
5. Array fields MUST 声明 item type，并在适用时声明 item schema。
6. Empty request/response 使用已注册的 Empty schema。
7. 生成 SDK SHOULD 区分 wire types 和 host-language types。
8. Object field MAY 声明 `variants: { discriminator, mapping }` variant 绑定：`discriminator` MUST 指向同一 object schema 内 type 为 `enum` 且已声明合法值的字段；`mapping` 的 key MUST 与该 enum 的合法值一致（全覆盖），value MUST 是已注册 object schema。带 `variants` 的字段 type MUST 为 `object`；编码时由 discriminator 的运行时值选择应用哪个 variant schema；未声明的 enum 值遵循 unknown-enum policy。

## Capability 模型

Capability 描述设备在当前 firmware、configuration、session 和 authorization state 下支持什么。Capability 不改变 methodId、eventId、PayloadType、CONTROL、RPC 或 STREAM wire shape。

Capability 规则：

1. Capability names SHOULD 使用 `domain.feature`。
2. Capability schema MAY 描述 limits、supported modes、ranges、defaults、update policy 和 availability。
3. Capability references MUST 解析到已注册的 method/event/schema/profile facts。
4. Capability discovery 如果被采纳，是 RPC business method；它不是 CONTROL。
5. Unknown optional capability fields MUST 按 SDK policy 忽略或保留。

Capability/profile compatibility 在 semantic validation 层执行，而不是用 peer 的 `axtpVersion` 选择 codec。Known field 请求当前 capability/profile 不支持的 optional feature 时，包含它的 method MUST 返回 `NOT_SUPPORTED`；decoder MUST 保持 session 可用，并继续处理后续受支持的 message。

完整 capability reflection、capability trees 和复杂 profile negotiation 属于 future 或单独采纳的业务协议，除非已经出现在 generated contract 中。

## RPC 对象 Envelope 编码

JSON、CBOR 和 MSGPACK 使用同一 RPC 对象语义：

```json
{ "sid": "12345678", "op": 7, "m": { "src": "ep-app-001", "dst": "ep-device-001" }, "d": {} }
```

`m` 是 optional object；省略 `m` 时，receiver MUST 按既有 `{ sid, op, d }` 规则处理消息。对象编码的 `m` 与业务 `d` 分层：`m` 只承载跨 `op` 的消息级元数据，`d` 继续承载 Request、RequestResponse、Event 等 op-specific 数据。

v1 已定义 metadata field：

| 字段 | JSON/CBOR/MSGPACK 类型 | 规则 |
|---|---|---|
| `m.src` | string | Optional logical source Endpoint ID。非空；推荐不超过 128 UTF-8 bytes。 |
| `m.dst` | string | Optional logical destination Endpoint ID。非空；推荐不超过 128 UTF-8 bytes；MUST NOT 是 array。 |

对象编码 receiver：

1. MUST 接受缺失 `m`；
2. MUST 在 `m` 存在时要求它是 object；
3. MUST 在已知 `src` / `dst` 存在时要求其类型为 string 且非空；
4. MUST 拒绝 array 形式的 `m.dst`，不能把一个 Request 隐式扩展为多目标 Request；
5. MUST 对 structurally valid 的未知 optional `m` field 使用通用 unknown-field policy：忽略或按 SDK policy 保留，并继续处理已知字段；
6. MUST NOT 因未知 optional `m` field 关闭 session 或拒绝不相关后续 RPC。

Event fanout 不通过 `m.dst` array 编码。一个 relay MAY 根据 subscription/policy 复制 Event 到多个 outbound sessions；每个对象编码 Event message 的 `m.dst` 仍是单个 string 或缺失。

JSON_BINARY 是独立 fixed binary envelope。当前 15B JSON_BINARY header 不增加 `m`、metadata length、`src` 或 `dst` 字段；对象编码 Endpoint Relay 不得改变 JSON_BINARY v1 wire offset。若未来为 binary RPC 增加 Endpoint metadata，MUST 使用独立版本/profile/codec 设计，而不是在现有 15B header 中原位插入字段。

## TLV 编码

TLV body encoding 使用 field id、length 和 raw value：

```text
TLV8  = fieldId:uint8 + length:uint8 + value(length)
TLV16 = fieldId:uint16 + length:uint16 + value(length)
```

CONTROL 使用 short TLV，并支持 optional extended length marker：

```text
type:uint8 + length:uint8 + value
type:uint8 + 0xFF + extendedLength:uint16 + value
```

TLV 规则：

1. Field order SHOULD 为 canonical generation 按升序排列。
2. 接收方 MUST 在读取 value bytes 前校验 length。
3. Unknown fields 只要 length 有效，MUST 被跳过。
4. Duplicate non-repeated fields SHOULD 在 strict validation 下失败。
5. Repeated fields MUST 由 schema/profile 显式声明。
6. TLV object decoding 使用 schema-local field ids；field names 不在 wire 上。
7. Packed array encoding 只在 schema/profile 定义 element width 和 layout 时允许。

## 字段编号

Schema field id 规则：

1. `fieldId=0x00` 保留，MUST NOT 用于普通字段。
2. 对 TLV8-compatible object fields，fieldId MUST 是 `0x01..0xFE`。
3. `0xFF` 在 short TLV contexts 中保留给 extended length marker。
4. Field id MUST 在 schema 内唯一。
5. Stable field id MUST NOT 被复用于不同名称或含义。
6. Deprecated fields 保留其 id，直到 compatibility policy 移除它们。
7. New fields SHOULD 默认为 optional，除非声明 breaking version boundary。
8. Rename SHOULD 使用新字段加 deprecated old field；禁止 silent replacement。

兼容性规则：

| 变更 | 默认兼容性 |
|---|---|
| 新增 optional field | 兼容。 |
| 新增 required field | 通常 breaking。 |
| 新增 enum/bitmap value | 只有定义 unknown-value policy 时兼容。 |
| 新增 variant（新 enum 值 + mapping 项） | 需要 discriminator 字段的 unknown-value policy 配合。 |
| 修改 type/unit/range/requiredness | 通常 breaking。 |
| 复用已移除的 field id | 对 stable contracts 禁止。 |

Runtime 或 generated codec MUST 校验 numeric ranges、UTF-8 strings、enum/bitmap values、required fields、object recursion、array element types 和 length boundaries。

Unknown-field policy：

- Unknown TLV fields 只要 length 有效，MUST 被 forward-compatible decoder 跳过。
- Unknown JSON/CBOR/MSGPACK optional object fields 只要 structurally valid，也 MUST 被 forward-compatible decoder 忽略或按 SDK policy 保留。
- Unknown enum 或 bitmap values 在可能时 MUST 为诊断保留；strict business validation MAY 在 decode 后拒绝。
- Defaults 只在 successful decode 后应用，并且只在 schema 声明 default 时应用。
- Deprecated fields MAY 为兼容性发出，但新发送方 SHOULD NOT 依赖它们。

成功跳过 unknown optional field 后，receiver MUST 继续处理已知字段，并 MUST 保持 session 与不相关 RPC 可用。Unknown event payload 也遵循相同 liveness 原则：无法解析为已注册 event 时忽略或记录 diagnostics，不得关闭 session。
