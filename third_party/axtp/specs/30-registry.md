# AXTP 注册表

本文定义注册表的分类、条目模型、ID 稳定性、兼容性和校验规则。当前注册事实位于 `contract/registry/**`、`contract/protocol/axtp.protocol.yaml` 和 `contract/generated/**`。
机器可读的 domain high-byte 分配位于 `contract/registry/core/domain_registry.yaml`；runtime/tool generator MUST 使用该文件、source registry 或 Protocol IR 推导 domain 分配，MUST NOT 维护第二套手写 domain 表。

## 领域与功能分类

`domain.feature` 是业务分类单元。每个已采纳的方法、事件、schema、错误、capability 或 profile 都 MUST 能说明自己所属的 `domain.feature`。

| 规则 | 要求 |
|---|---|
| Domain | MUST 是稳定的业务或协议边界，例如 `audio`、`video`、`device`、`system`、`network`、`firmware`、`capability`、`room` 或 `signage`。 |
| Feature | MUST 是可评审、可测试、可演进的能力块；它 MUST NOT 是字段名、UI 控件、codec、transport、产品 SKU 或 legacy command id。 |
| Method name | MUST 使用 `domain.actionObject`，或已经采纳的 `domain.verbNoun` 形式。 |
| Event name | MUST 表达状态变化、结果、进度或上报语义；Event MUST NOT 替代 RPC Response。 |
| Capability name | SHOULD 使用 `domain.feature`。 |
| Stream business | 业务流属于业务 domain，例如 `video.stream` 或 `audio.stream`；公共 `stream` 保留给共享数据面或 flow-control 概念。 |

分类规则：

- `room` 描述逻辑空间或业务端点。
- `device` 描述端点身份、产品、硬件和拓扑。
- `system` 描述 OS/runtime 状态和生命周期。
- network、storage、file、log、software、firmware 等通用资源使用自己的 domain。
- audio、video、camera、display、signage、cast、input、output、diagnostic、auth、privacy 等功能能力使用能力专属 domain。
- 软件应用更新属于 `software`；底层 device image、bootloader、MCU/DSP/ISP/FPGA image 属于 `firmware`。
- power、reboot、shutdown、sleep/wake 属于 `system`，不属于 device identity。

更长的分类示例和 legacy intake 表位于 `workspace/registry-planning/**`；它们不是 release/runtime 合同。

## 状态与 ID 分配

方法、事件、错误、schema、capability 和 profile 使用同一套状态词：

| 状态 | 含义 | Runtime 合同 |
|---|---|---:|
| `draft` | 正在 review，或尚未生成。 | 否 |
| `experimental` | 已为有限互操作生成，在 stable release 前仍可能变化。 | Profile-specific |
| `stable` | 已采纳，并作为当前实现合同生成。 | 是 |
| `deprecated` | 为兼容性继续生成，但不建议新使用。 | 是，受 deprecation 规则约束 |
| `reserved` | 为未来或兼容性保留；无 runtime 行为。 | 否 |

除非生成事实另有说明，数字 ID 都是 `uint16`。新的 domain-scoped method/event/error/capability id SHOULD 使用稳定的高字节 domain 分配和低字节本地 ID。一旦在 stable release 中生成，数字 ID、名称、`bitOffset` 和 schema field id MUST NOT 被复用于不同含义。

`bitOffset` 是供生成 mask 和 discovery 使用的 domain-local metadata。它本身不是 wire id，MUST NOT 替代 methodId 或 eventId 使用。

## 方法 Methods

Method 是 RPC 业务操作。JSON RPC 使用 method name；JSON_BINARY 使用 `methodId:uint16`；两者 MUST 解析到同一个注册事实。

Method 规则：

1. Method MUST 由 RPC 承载，不能由 Frame Header、CONTROL header 或 STREAM Header 承载。
2. Method name 和 methodId MUST 全局唯一。
3. Stable methodId MUST NOT 改变含义或被复用。
4. 使用 domain-scoped method bitmap metadata 时，`bitOffset` MUST 在 domain 内唯一。
5. 每个 method MUST 绑定 request schema 和 response schema；空 request/response MUST 使用已注册的 Empty schema。
6. 每个 method MUST 声明可能的错误；每个 error MUST 解析到 error registry。
7. method 引用的 event 和 capability MUST 解析到已注册事实。
8. 连续数据 MUST 使用 STREAM；method 负责 setup、query、control、start/stop 或有限结果。

最小 source 形状：

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

Runtime MUST 使用生成的 registry 或 Protocol IR 做 method lookup，MUST NOT 维护第二套手写 method table。

Session 建立只保证 common RPC baseline，不表示当前 runtime、device、profile、mode 或 capability set 提供全部 registered methods。Caller SHOULD 在调用 optional method 前查询相关 generated capability；capability 缺失表示该 optional feature 不可用。Receiver 即使面对未先 discovery 的 caller，也 MUST 执行自己的 capability/profile policy：

- method name 或 methodId 不在 registry 中，MUST 返回 `RPC_METHOD_NOT_FOUND`；
- method 已注册但在当前 runtime、device、profile、mode 或 capability set 中不可用，新的发送方 MUST 返回 common `NOT_SUPPORTED`；
- method 可用但参数 malformed、越界或违反 schema，MUST 返回 `INVALID_ARGUMENT`、`RPC_PARAM_INVALID` 或该 method 注册的 validation error。

上述失败只降级当前 operation，MUST NOT 使 session 失效，也 MUST NOT 阻止后续无关且受支持的 request。接收方 MAY 继续接受 legacy `RPC_METHOD_NOT_SUPPORTED`、`CAPABILITY_METHOD_UNSUPPORTED` 等细粒度 unsupported errors，但新发送方 MUST 使用 canonical `NOT_SUPPORTED` 表示 registered-but-unavailable。

## 事件 Events

Event 是 RPC 异步通知，用于状态、进度、结果或上报语义。

Event 规则：

1. Event MUST 由 RPC `op=EVENT` 承载。
2. Event name 和 eventId MUST 全局唯一。
3. Stable eventId MUST NOT 改变含义或被复用。
4. `bitOffset` MUST 在 domain 内唯一，并由 domain-scoped `eventMasks` 使用。
5. 每个 event MUST 绑定 event payload schema。
6. Event MUST NOT 表达某个 request 的同步成功/失败；这由 RPC Response 表达。
7. 高频连续数据 MUST 使用 STREAM。

Domain-scoped event mask 格式由核心 RPC 行为定义。bit 0 映射到该 domain 中 registry `bitOffset=0` 的 event。

接收方 MUST 按 profile policy 容忍未知或未订阅事件；未知事件处理本身 MUST NOT 使 session 失效。

发送方 MUST NOT 向未声明相应 capability 或未订阅该 event 的 peer 主动发送 optional event。接收方收到未知 event 时 MUST 忽略或只记录 diagnostics，并 MUST 保持 session 及不相关 RPC 可用。

## 错误 Errors

ErrorCode 是 RPC Response status、CONTROL status 和 STREAM/profile error mapping 共用的数字错误空间。

Error 规则：

1. `0x0000` MUST 表示成功；失败 MUST 使用非零 errorCode。
2. ErrorCode MUST 是全局唯一的 `uint16`。
3. Stable errorCode MUST NOT 改变含义或被复用。
4. RPC 业务失败 SHOULD 由 RPC Response 表达，而不是 Frame Header 或 CONTROL NACK。
5. CONTROL 协商、frame parse、transport limit 和 stream data-plane 失败 SHOULD 使用 common/frame/control/stream errors。
6. 细粒度诊断 SHOULD 放在 status details、event payload、diagnostic payload 或 vendor detail fields 中，而不是制造大量重复 error code。

核心范围：

| 范围 | 分类 |
|---:|---|
| `0x0000-0x00FF` | common / frame / control / rpc |
| `0x0100-0x05FF` | device / capability / system / firmware / stream |
| `0x0600-0x16FF` | business domains |
| `0x7000-0x7EFF` | vendor |
| `0x7F00-0x7FFF` | legacy adapter |

Runtime MUST 使用生成的 error enum/lookup，并 SHOULD 为诊断保留未知 error code。

## 配置 Profile

Profile 是具名实现要求集合。它可以引用 methods、events、types、errors、capabilities、transport profiles 和 frame profiles；它 MUST NOT 重新定义 wire fields。

Profile 规则：

1. Profile MUST 引用已定义事实。
2. Profile MUST NOT 改变 methodId、eventId、errorCode、PayloadType、Standard Frame Header、RPC envelope 或 STREAM Header 语义。
3. Standard Framed profiles MAY 要求 CONTROL、RPC 和 STREAM。
4. WebSocket Unframed JSON profiles MUST NOT 要求 CONTROL 或 STREAM。
5. 支持某个 profile 意味着满足 required facts，并通过对应 conformance scope。
6. 向 stable profile 添加 required facts 可能是 breaking change；添加 optional capability 通常兼容。

Profile 或 capability 不匹配 MUST 只降级受影响的 operation。已注册但因 profile/capability 不可用的 operation MUST 返回 `NOT_SUPPORTED`；它不能触发 session rejection、retry/reconnect，或关闭不相关功能。

Runtime 仓库 MUST 声明自己支持的 profile，并绑定精确 spec tag、commit 或 release artifact metadata。

## 退役分配与 Typed Input

移除已生成 draft 方法时，`retired_allocations` 保留 kind/id/domain/bitOffset；
bitOffset 的连续性计算包含这些不可用槽，支持掩码不重新编号。退役记录不是
可调用方法，不进入有效 method lookup。schema 的 `retired_field_ids` 同样禁止
复用；source/IR/JSON 均保留墓碑且验证碰撞。

Typed Input 的允许类型、控制上下文、7×N Touch 与 Mouse 变体见
[48-input-stream](48-input-stream.md)；数据面事实由 input domain 的 input_stream
元数据进入生成 IR、文档、向量及 C++ 常量，不在某个产品私造第二份布局。

## 校验

生成器校验 MUST 至少检查：

- method/event/error/profile 名称和数字 ID 全局唯一；
- method/event/capability 名称与 domain prefix 对齐；
- method/event 的 bitOffset 在 domain 内唯一；
- schema、error、event、capability、transport 和 profile 引用存在；
- stable/deprecated/reserved ids 未被复用；
- core registry 和 domain YAML 没有重复定义同一事实；
- Protocol IR 和 generated docs 与 source YAML 匹配。

`workspace/registry-planning/**` 下的 candidate planning tables 不是 validation input，除非它们被显式采纳进 `contract/registry/**`。
