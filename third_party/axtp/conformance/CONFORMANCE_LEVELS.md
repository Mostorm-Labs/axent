# AXTP Conformance Levels

本文定义 runtime / SDK / mock server 如何声明 AXTP conformance 支持等级。

Conformance level 是测试入口，不是协议功能清单的替代品。具体 case 仍以 [manifest.yaml](manifest.yaml)、[profiles/](profiles/core.yaml) 和 [cases/](cases/session/hello_identify_identified.yaml) 为准。

## Level 总览

| Level | 名称 | 核心范围 | 典型 runtime |
|---|---|---|---|
| L0 | WebSocket JSON RPC only | RPC Hello / Identify / Request / Response / Event。 | Web、Node、Python、WS-only mock server、云控制面。 |
| L1 | Standard Framed CONTROL + RPC | Frame Header、CRC16、CONTROL OPEN / ACCEPT / HEARTBEAT / CLOSE、RPC。 | C / C++ 设备端、TCP runtime、USB HID runtime、Node TCP mock-server。 |
| L2 | Standard Framed + STREAM | L1 + `PayloadType=STREAM` 16B header、stream open/data/close 验收。 | 音视频设备、需要连续数据面的 runtime。 |
| L3 | Low-bandwidth / HID / BLE profile | 低带宽、短包、分片、降级 profile。 | MCU、BLE、UART、HID-64 等受限链路。 |
| L4 | Business domain conformance | 已 generated 业务 domain 的 method/event/schema 行为。 | 交付设备、业务 SDK、客户 release runtime。 |

仓库 manifest 还可以按 generated domain 定义细分 level；当前 `sport` level 使用 [profiles/sport.yaml](profiles/sport.yaml) 声明 VM33PRO 篮球事件检测配置行为。

`cast-control` 是可选 L4 业务行为 level，覆盖三协议多路呈现控制 P0。
它不扩大现有 L0/L1/L2 或 websocket-jsonrpc 的 required set。完整声明要求
执行 C01–C11，并把 [参考模型](reference/cast-control.mjs) 中的替身 owner
观察点接到实际 runtime；本仓库的模型通过不等于某个 runtime 已通过验收。

`cast-settings` 是完整 P0/P1 的可选 L4 level，要求 C01–C12，包含
PS-01–PS-12 的协议保证。设置流独立于 P0 序列；磁盘与政策 owner 使用
替身验证。只声明 `cast-control` 的旧端不因此获得新的必选项。

`endpoint-relay` 是独立的 optional behavior level，不改变 L0-L4 的传输/业务分层。Cloud、Agent、Gateway 或需要通过 `m.src` / `m.dst` 代理下级 Endpoint 的 runtime 可以额外声明它；普通直连设备或只实现 JSON_BINARY 的 MCU 不需要声明。

## L0: WebSocket JSON RPC Only

| 项 | 要求 |
|---|---|
| 适用 runtime | App、Web、Node、Python、WS-only mock server、云端控制面。 |
| 必须 profile | `websocket-jsonrpc`，以及 shared `core` RPC 行为。 |
| 必须 case 分类 | `session/**`、`rpc/**`、`error/**` 中与 WS-JSON 相关的 case。 |
| 可选 case | `capability/**`、`event/**`、`endpoint-relay`，取决于 runtime 声明。 |
| 可声明不支持 | `framed-binary`、`stream`、低带宽 profile。 |

L0 不要求 Frame Header、CRC16、CONTROL、STREAM data packet 或 JSON_BINARY RPC Header。

## L1: Standard Framed CONTROL + RPC

| 项 | 要求 |
|---|---|
| 适用 runtime | TCP、USB HID、设备端 C/C++ runtime、需要 framed wire 的 SDK、Node TCP mock-server。 |
| 必须 profile | `framed-binary`，以及 shared `core` RPC 行为。 |
| 必须 case 分类 | `handshake/**`、`session/**`、`rpc/**`、`error/**`。 |
| 可选 case | `capability/**`、`event/**`、`stream/**`、对象编码 `endpoint-relay`。 |
| 可声明不支持 | STREAM、低带宽 profile、业务 domain conformance。 |

L1 必须完成 `CONTROL OPEN / ACCEPT` 后才允许进入 RPC Hello / Identify / Identified。

## L2: Standard Framed + STREAM

| 项 | 要求 |
|---|---|
| 适用 runtime | 音视频设备、需要 `PayloadType=STREAM` 的 runtime、媒体 mock server。 |
| 必须 profile | `framed-binary` + `stream`。 |
| 必须 case 分类 | L1 required cases + `stream/**` P0 cases。 |
| 可选 case | 业务 domain STREAM profile，例如未来 firmware/file/log stream。 |
| 可声明不支持 | 低带宽 profile、未采纳业务 stream profile。 |

Phase 1 的 L2 STREAM P0 重点是 audio/video 媒体流。严格 ACK/NACK 重传、RESUME 和非媒体 STREAM profile 是 deferred 范围。

## L3: Low-Bandwidth / HID / BLE Profile

| 项 | 要求 |
|---|---|
| 适用 runtime | MCU、BLE、UART、HID-64、低资源设备。 |
| 必须 profile | 对应低带宽 profile；具体 profile 采纳后在 `conformance/profiles/**` 声明。 |
| 必须 case 分类 | L1 的必要子集 + 低带宽分片、MTU、降级编码和超时 case。 |
| 可选 case | STREAM、业务 domain、增强重传。 |
| 可声明不支持 | 非目标 transport profile 和不适配的业务 domain。 |

L3 当前是规划等级。没有对应 profile 和 case 前，runtime 不应宣称 full L3 pass。

## L4: Business Domain Conformance

| 项 | 要求 |
|---|---|
| 适用 runtime | 交付设备、业务 SDK、mock server、客户 release runtime。 |
| 必须 profile | runtime 已声明的 transport profile + 对应 generated domain。 |
| 必须 case 分类 | 已 generated business domain 的 method/event/schema/error 行为 case。 |
| 可选 case | 未采纳草案、adapter-private legacy 行为。 |
| 可声明不支持 | 设备不具备的 generated capability，但必须在 capability discovery 中明确。 |

L4 只验收已经 generated 的业务 domain。`workspace/protocol/**` draft-only 能力不能直接成为 L4 必测项。

## 如何声明不支持

Runtime 可以不支持某个 level 或 profile，但必须显式声明。

mock server 也必须按实际传输声明 profile：只支持 WebSocket JSON 时声明 L0 / `websocket-jsonrpc`；作为当前互操作基准的 Node TCP mock-server 应声明 L1 / `framed-binary`。

建议规则：

| 情况 | 声明方式 |
|---|---|
| 完全不支持某 profile | 在 `unsupported` 列出 profile 或能力名称。 |
| 支持 profile 但不支持某 optional case | 在 runtime conformance 配置中列出 skip reason。 |
| 设备没有某业务 capability | 在 capability discovery 中不声明该 capability，或返回稳定的 `NOT_SUPPORTED`。 |
| 暂未实现但计划实现 | 标记为 `planned`，不能计入当前 pass。 |

已声明支持的 required case 不应被 runner 静默 skip。

## AXTP_CONFORMANCE.yaml 示例

Runtime 仓库建议提供 `AXTP_CONFORMANCE.yaml`：

```yaml
runtime: axtp-cpp-runtime
spec: spec/v0.0.x
profiles:
  - websocket-jsonrpc
  - framed-binary
levels:
  - L0
  - L1
unsupported:
  - stream
```

带 STREAM 的设备 runtime 示例：

```yaml
runtime: axtp-device-runtime
spec: spec/v0.0.x
profiles:
  - framed-binary
  - stream
levels:
  - L1
  - L2
unsupported:
  - low-bandwidth
  - business-domain-conformance
```

## 验收输出建议

测试报告至少包含：

```text
runtime: <repo>@<commit>
spec: <spec tag or commit>
profiles: <declared profiles>
levels: <declared levels>
result: pass | fail
failed cases: <case ids>
unsupported: <declared unsupported list>
```

这样发布负责人可以判断失败是 runtime bug、声明错误、case 问题还是 spec/generator 冲突。
