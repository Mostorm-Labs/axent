# Rooms Protocol Migration Plan

> Status: migration design
> Scope: Rooms WebSocket JSON compatibility profile and embedded parser freeze
> Source evidence: `workspace/legacy-migration/evidence/Rooms协议文档.md`
> AXTP alignment: `specs/20-core.md`, `specs/20-core.md`

本文定义 Rooms 协议迁移到 AXTP 体系下的落地方案。结论是：Rooms 既有业务 method 和 params 作为兼容 profile 固定下来，不在本轮改名、拆字段或写入 AXTP registry；本轮只统一 session/Hello 方向和 status envelope，并冻结嵌入式协议解析代码的边界。

## 1. 决策摘要

| 决策 | 结论 |
|---|---|
| 业务方法 | Rooms 旧 `d.method` 字符串原样保留，例如 `CreateInputSource`、`SetScene`、`GetDeviceInfo`。 |
| 业务参数 | Rooms 旧 `d.params` 原样保留，由 Rooms 业务 handler 消费，不自动转换成 AXTP domain schema。 |
| 请求 ID | Rooms request id 按 string correlation id 冻结；旧文档样例 wire 字段为 `d.id`，若具体实现使用 `d.requestid` 也必须原样保留，响应必须回填同一字段和值。 |
| Session envelope | 继续使用 JSON `{sid, op, d}`。 |
| Hello 方向 | 改为 Logical Server 主动发送 `Hello(op=0)`；Rooms 设备是 Logical Server，即使它在反连场景中是 Physical Client。 |
| 客户端认证/声明 | Client 收到 Hello 后发送带 `randomSeed:uint32` 的 `Identify(op=2)`；Server 返回 `Identified(op=3)` 并分配 `sid`。 |
| 旧 HelloAck | `HelloAck(op=1)` 在新 Rooms profile 中不再作为主路径；仅可作为临时兼容入口，不进入冻结合同。 |
| status envelope | 旧 `status.{code, comment, result}` 适配为 `status.{code, msg, ok}`。 |
| 新业务开发 | 不扩展 Rooms 旧 method 表；新增能力必须走 `workspace/protocol/<domain>/<domain.feature>.md` 草案、评审、YAML 采纳和 Generator 流程。 |

## 2. 非目标

- 不修改 `contract/protocol/axtp.protocol.yaml`。
- 不新增或修改 `contract/registry/**/*.yaml`、`contract/registry/domains/**/*.yaml`。
- 不把 Rooms 旧 method 直接注册为 AXTP methodId。
- 不把 Rooms 旧 params 自动拆成 AXTP TLV schema。
- 不把 Rooms string request id 直接映射成 AXTP `requestId:uint32`。
- 不要求本轮运行 Generator。

## 3. 协议分层

Rooms 迁移后的实现分三层：

| 层 | 责任 | 是否冻结 |
|---|---|---:|
| Rooms JSON codec | 解析和编码 `{sid, op, d}`、Rooms method、params、event、status | 是 |
| Rooms business router | 以旧 method 字符串路由到设备本地业务 handler | 是 |
| AXTP business protocol | 后续新增能力的正式草案、YAML、generated 产物 | 否，按 AXTP 生命周期演进 |

Rooms JSON codec 不依赖 AXTP `RegistryLookup::methodIdByName()`。原因是 Rooms method 不是 `domain.verbObject` 命名，也不是当前 AXTP registry 事实源的一部分。Rooms 兼容 profile 应使用独立 decoder/router，避免被 AXTP Core JSON decoder 拒绝。

## 4. 连接生命周期

### 4.1 旧 Rooms 行为

旧 Rooms 文档中，Client 首次发送 `op=0`，携带 `adxdpVersion`、`rpcVersion`、`supportedMethods`、`supportedEvents`；Server 返回 `op=1`，分配 `sid`。

### 4.2 新 Rooms 行为

新 Rooms profile 与 AXTP 逻辑角色对齐：

```text
WebSocket connected
  -> Server sends Hello(op=0, sid="")
  -> Client sends Identify(op=2, sid="", randomSeed)
  -> Server sends Identified(op=3, sid="<allocated>")
  -> APP_READY
  -> Client sends Request(op=7)
  -> Server sends RequestResponse(op=8) or Event(op=6)
```

Rooms 设备提供 methods/events/streams，因此 Rooms 设备始终是 Logical Server。物理连接角色不改变 Hello 方向：云端反连时设备可能是 Physical Client，但 Hello 仍由设备发送。

### 4.3 Hello

Server 在 WebSocket 建立后立即发送：

```json
{
  "sid": "",
  "op": 0,
  "d": {
    "axtpVersion": "1.0.0",
    "profile": "rooms-ws-json",
    "roomsVersion": "5.2.3",
    "authentication": null
  }
}
```

规则：

- `sid` 必须是空字符串。
- `profile` 固定为 `rooms-ws-json`，用于客户端确认进入 Rooms 兼容 profile。
- `roomsVersion` 对应旧文档中的 `adxdpVersion` 语义；字段名不再使用 `adxdpVersion`。
- 不需要认证时 `authentication` 可省略；需要认证时按 AXTP Hello challenge 语义放入对象。

### 4.4 Identify

Client 收到 Hello 后发送：

```json
{
  "sid": "",
  "op": 2,
  "d": {
    "randomSeed": 305419896,
    "clientName": "rooms-client",
    "clientVersion": "1.0.0",
    "eventMasks": "",
    "resumeSid": ""
  }
}
```

规则：

- 新连接 `sid` 为空字符串。
- `randomSeed` 是 uint32 随机种子，Server 用它播种或混入 sid 生成器。
- 断线恢复时 `sid` 仍为空字符串，旧 session 放入 `d.resumeSid`。
- `eventMasks` 允许为空。Rooms MVP 可忽略该字段并继续按旧 Rooms 事件策略推送。
- 旧 `supportedMethods` / `supportedEvents` 不再由 Client 在首次消息中声明。

### 4.5 Identified

Server 验证通过后发送：

```json
{
  "sid": "12345678",
  "op": 3,
  "d": {
    "expireIn": 300
  }
}
```

规则：

- `sid` 由设备生成，后续 Request/Event/Response 必须携带。
- `expireIn` 作为 Rooms 兼容字段保留；客户端不得只依赖它判断连接存活，仍需处理 WebSocket 断开。
- 后续能力发现不再塞进 Hello/Identified；Rooms 旧 method 表由固定解析代码和产品固件版本定义。

### 4.6 Bye

Rooms 继续支持旧 `Bye(op=14)` / `ByeAck(op=15)`：

```json
{
  "sid": "123456",
  "op": 14,
  "d": {}
}
```

Server 返回：

```json
{
  "sid": "123456",
  "op": 15,
  "d": {}
}
```

## 5. Request 和 Event

### 5.1 Request

请求结构固定为：

```json
{
  "sid": "123456",
  "op": 7,
  "d": {
    "id": "ab22-dddaa",
    "method": "CreateInputSource",
    "params": {
      "name": "V520D-V1",
      "type": "Network",
      "streamContent": "rtsp://192.168.16.8/mainstream"
    }
  }
}
```

冻结规则：

- `method` 大小写敏感，必须原样匹配 Rooms 旧 method 名。
- `params` 必须作为 JSON object 原样交给 handler；字段名、枚举值、数值单位不在 codec 层修改。
- `params` 缺省时按空对象处理。
- Rooms request id 使用 string。旧文档样例 wire 字段为 `d.id`；如果具体产品实现使用 `d.requestid`，字段名也必须原样保留。
- `d.id` / `d.requestid` 必须原样保存并回填，不得转成数字，不得复用 AXTP `requestId:uint32` 的递增规则。
- `sid` 在 codec 层统一归一化为 string；输入为 number 时允许接收，但内部存储和输出使用 string。

### 5.2 Request ID 边界

Rooms JSON profile 和 AXTP RPC 的 request id 类型不同：

| 协议层 | Wire 字段 | 类型 | 规则 |
|---|---|---|---|
| Rooms JSON profile | `d.id` 或具体实现中的 `d.requestid` | string | opaque correlation id，例如 `"ab22-dddaa"`；字段名和值都只做匹配和回填。 |
| AXTP JSON RPC | `d.id` | uint32 | AXTP 正式 JSON RPC 的请求序号。 |
| AXTP Binary RPC | `requestId` | uint32 | Binary RPC 15B header 中的请求序号。 |

因此 Rooms parser 必须把 Rooms request id 作为字符串保存。如果某个 Rooms handler 内部桥接到正式 AXTP RPC，adapter 必须分配一个本地 `uint32` AXTP requestId，并维护映射：

```text
rooms sid + rooms request id field name + rooms string request id
  <-> adapter-local uint32 AXTP requestId
```

AXTP response 回来后，adapter 用映射恢复 Rooms 原始 string request id，再编码 Rooms response。该映射只存在于 adapter/session 内部，不进入 Rooms wire，不进入 AXTP registry。

### 5.3 Event

事件结构保持 Rooms 旧风格：

```json
{
  "sid": "123456",
  "op": 6,
  "d": {
    "event": "InputSourceChange",
    "data": {
      "inputSourceId": 1
    }
  }
}
```

冻结规则：

- `event` 原样使用 Rooms 旧事件名。
- `data` 原样使用 Rooms 旧字段。
- 事件订阅过滤可以后置；MVP 允许按旧 Rooms 行为广播核心事件。

## 6. Status Envelope

### 6.1 外部 Rooms profile

Rooms 对外响应统一改为：

```json
{
  "sid": "123456",
  "op": 8,
  "d": {
    "id": "ab22-dddaa",
    "method": "CreateInputSource",
    "status": {
      "code": 100,
      "ok": true
    },
    "result": {
      "inputSourceId": 6,
      "name": "V520D-V1"
    }
  }
}
```

失败响应：

```json
{
  "sid": "123456",
  "op": 8,
  "d": {
    "id": "ab22-dddaa",
    "method": "CreateInputSource",
    "status": {
      "code": 205,
      "ok": false,
      "msg": "the input source has existed"
    }
  }
}
```

映射规则：

| 旧字段 | 新字段 | 规则 |
|---|---|---|
| `status.code` | `status.code` | 外部 Rooms profile 保留旧 Rooms 数值，例如 `100`、`205`、`400`、`500`。 |
| `status.result` | `status.ok` | `true` -> `true`，`false` -> `false`。 |
| `status.comment` | `status.msg` | 有错误说明时复制到 `msg`；成功时通常省略。 |
| `d.result` | `d.result` | 业务返回对象保留，不与 `status.ok` 混用。 |

外部 Rooms profile 中，`status.ok` 是程序判断成功与否的主字段。`status.code` 保留旧 Rooms 数值是为了兼容旧业务语义；不要在 Rooms profile 中把 `100` 改成 `0`。

### 6.2 内部 AXTP 桥接

如果后续某个 Rooms handler 内部调用正式 AXTP business method，则桥接层按下表转换：

| Rooms status | AXTP internal error | 说明 |
|---|---|---|
| `code=100, ok=true` | `SUCCESS(0)` | 内部成功。 |
| `code=203/204/205, ok=false` | `RPC_PARAM_INVALID` 或 `RPC_METHOD_NOT_FOUND` | 按 handler 具体错误归类。 |
| `code=400-499, ok=false` | `RPC_PARAM_INVALID` / `UNAUTHORIZED` / `FORBIDDEN` | 认证和请求错误。 |
| `code=500-799, ok=false` | `INTERNAL` / `BUSY` / domain error | 资源或执行失败。 |

桥接只发生在 adapter 内部；对外仍返回 Rooms `code/msg/ok`。

## 7. 嵌入式解析代码冻结边界

落地前需要冻结一套独立的 Rooms parser，建议模块如下：

| 模块 | 职责 |
|---|---|
| `RoomsJsonCodec` | 解析 WebSocket text message；校验 `op`、`sid`、`d`；编码响应和事件。 |
| `RoomsSessionManager` | 发送 Hello、处理 Identify、分配/恢复 sid、处理 Bye。 |
| `RoomsStatusAdapter` | 在旧 `code/comment/result` 和新 `code/msg/ok` 之间做兼容读写。 |
| `RoomsMethodRouter` | 以旧 method 字符串分发表驱动 handler。 |
| `RoomsEventEmitter` | 以旧 event 字符串编码事件。 |
| `RoomsDiscoveryResponder` | 保持 UDP multicast `DeviceDiscovery` 响应，返回 WebSocket 入口。 |

冻结后，codec 层只允许兼容性修复，不允许新增业务字段解释。新增业务能力必须通过 AXTP 正式业务协议进入 `workspace/protocol/**` 和 `contract/registry/domains/**`。

## 8. Parser Contract

嵌入式解析代码必须固定以下行为：

| 输入条件 | 行为 |
|---|---|
| JSON 非 object | 关闭连接或返回协议错误，不进入业务 handler。 |
| 缺少 `op` | 返回 `status.ok=false`，错误码使用 Rooms 参数错误类。 |
| 未 APP_READY 收到 `op=7` | 拒绝请求，不进入业务 handler。 |
| `sid` 缺失 | Hello/Identify 阶段允许；APP_READY 后拒绝。 |
| `sid` 类型为 number | 接收并归一化为 string。 |
| `d.id` / `d.requestid` 均缺失 | 返回参数错误，不进入业务 handler。 |
| request id 字段存在但非 string | 返回参数错误，不进入业务 handler。 |
| request id 字段为 string | 接收并按原字段名原样回填；不得解析为数字。 |
| `d.method` 未知 | 返回 `status.ok=false`，保留 Rooms unknown-method 错误码。 |
| `d.params` 缺省 | 传空 object 给 handler。 |
| `d.params` 非 object | 返回参数错误。 |
| handler 成功且无返回值 | 返回 `status.ok=true`，只携带 `status`，不携带业务 `result`。 |
| handler 失败 | 返回 `status.ok=false`、`status.code`、`status.msg`，不携带业务 `result`。 |

## 9. 与现有分类文档的关系

`workspace/legacy-migration/classification/by-source/rooms_ws_json.md` 已经把 Rooms 旧 method/event 粗分类到候选 AXTP domain.feature，例如：

- `CreateInputSource` / `GetInputSource` / `SetInputSource` -> `room.source`
- `CreateScene` / `SetScene` / `StartScene` -> `room.layout`
- `StartLiveStream` / `StopLiveStream` -> `video.stream`
- `SetLineInPreGain` / `GetLineInPreGain` -> `audio.input`
- `WifiConnect` / `GetWifiSignalStrength` -> `network.wifi`

这些分类仅作为后续新增 AXTP 业务协议的参考，不作为本轮 Rooms parser 的自动路由表。Rooms parser 的路由表以旧 method 名为 key。

## 10. 测试计划

### 10.1 Golden fixtures

至少冻结以下 JSON fixtures：

| fixture | 期望 |
|---|---|
| `rooms-server-hello` | WebSocket connected 后 Server 首条消息为 `op=0`、`sid=""`。 |
| `rooms-identify` | Client `op=2` 后 Server 返回 `op=3` 和非空 `sid`。 |
| `rooms-keepalive` | `KeepAlive` method/params 不修改，响应 `code/msg/ok`。 |
| `rooms-create-input-source-success` | `CreateInputSource` params 原样进入 handler，响应保留业务 `result`。 |
| `rooms-create-input-source-failure` | 旧 `comment` 语义输出为 `status.msg`。 |
| `rooms-sid-number-input` | 输入数字 sid，输出字符串 sid。 |
| `rooms-id-string-roundtrip` | 字符串 request id 原样回填。 |
| `rooms-id-number-rejected` | 数字 request id 被拒绝，不进入业务 handler。 |
| `rooms-unknown-method` | 未知 method 不进入业务 handler，返回 `ok=false`。 |

### 10.2 Compatibility gates

提交嵌入式 parser 前必须确认：

- Server Hello 不依赖 Client 首发消息。
- `op=1` 不在新主路径产生。
- 所有 Rooms method 名称与旧文档一致。
- 所有 Rooms params 字段名和枚举值与旧文档一致。
- 所有响应 status 不再输出 `comment/result` 子字段，而是输出 `msg/ok`。
- 业务 `d.result` 仍保留，不和 `status.ok` 混淆。

## 11. 发布与演进

建议分三步发布：

1. **Parser freeze**：固化 Rooms JSON codec、session manager、status adapter 和 method router；用 golden fixtures 锁定行为。
2. **Firmware integration**：Rooms 设备接入 server Hello 策略，替换旧 client-first Hello 流程，保持旧业务 handlers 不变。
3. **AXTP expansion**：后续新增业务不扩 Rooms method 表，按 AXTP 草案、评审、YAML 事实源和 Generator 流程新增正式 business protocol。

验收标准：

- 旧 Rooms 业务调用在 method/params 层零迁移。
- 新连接流程符合 Logical Server Hello 规则。
- status envelope 输出为 `code/msg/ok`。
- 嵌入式 parser 不依赖 generated AXTP method registry。
- 新业务需求有明确入口进入 `workspace/protocol/**`，不会继续堆进 Rooms legacy profile。
