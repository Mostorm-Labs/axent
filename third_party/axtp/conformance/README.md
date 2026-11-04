# AXTP Conformance

AXTP conformance 用例维护在根目录 `conformance/` 下，是 runtime、SDK、mock server 和工具仓库共享的行为验收输入。runtime 不应重新定义这些用例，而应加载本目录，并根据自己声明的 support level 执行匹配 case。

> Runtime 支持等级、profile 声明和 `AXTP_CONFORMANCE.yaml` 示例见 [Conformance Levels](CONFORMANCE_LEVELS.md)。
> Runtime 实现清单和常见排障见 [Runtime / SDK Guide](../docs/guides/runtime.md)。

## 目录结构

| 路径 | 说明 |
| --- | --- |
| `manifest.yaml` | 声明 conformance levels 以及每个 level 的 required cases。 |
| `schemas/` | case 和 result 的 JSON Schema。 |
| `profiles/` | profile 级验收期望。 |
| `fixtures/` | 协议行为用例使用的设备画像。 |
| `cases/` | YAML 格式的用例描述。 |

篮球事件检测配置的多设备行为由 `sport` level 覆盖，包含 MAC 路由、get/set 往返、status-only 成功响应，以及参数校验和原子更新失败语义。

## Phase 1 MVP Scope

| 能力 | WebSocket JSON runtime | Standard Framed runtime |
| --- | ---: | ---: |
| RPC Hello / Identify / Identified | 是 | 是 |
| RPC Request / RequestResponse / Event | 是 | 是 |
| `sid` 固定 8 位 hex string / binary `uint32` | 是 | 是 |
| `requestId` 匹配未完成请求 | 是 | 是 |
| 标准错误响应形状 | 是 | 是 |
| Standard Frame Header / CRC16 | 否 | 是 |
| CONTROL OPEN / ACCEPT | 否 | 是 |
| CONTROL HEARTBEAT / HEARTBEAT_ACK | 否 | 是 |
| CONTROL CLOSE / CLOSE_ACK | 否 | 是 |
| STREAM open / data / close | 否 | 是，P0 用于 audio/video 媒体流 |

Hello / Identify / Identified 字段规则不在 conformance 文档中重复定义；完整规范见 [RPC Session Spec](../specs/20-core.md)。

`session.axtp_version_advisory` 是共享 cross-version matrix。它使用 executable `scenarios`，每个 scenario 都包含具体 Hello payload（absent scenario 明确省略字段）、Identify / Identified 和握手后的 core session-safe `REIDENTIFY / IDENTIFIED` exchange。该 matrix 不依赖任何 optional business-domain method 或 capability。Runtime 必须对 `1.0.0 -> 1.1.0`、`1.1.0 -> 1.0.0`、patch difference、`2.0.0`、malformed 和 absent 六种输入逐项执行；版本值只可用于 diagnostics。

## Required Levels

| runtime 类型 | 最小声明 level | 可选增强 level |
| --- | --- | --- |
| WebSocket JSON | `core`、`websocket-jsonrpc` | `capability`、`event` |
| Standard Framed | `core`、`framed-binary` | `capability`、`event`、`stream` |
| mock server | 按声明 profile：WS-only mock 使用 `core`、`websocket-jsonrpc`；当前 Node TCP mock-server 使用 `core`、`framed-binary` | `capability`、`event`、`stream` |
| 固件 / MCU | `core`、`framed-binary` | `stream` 或后续低带宽 profile |

runtime 必须显式声明自己支持的 level。某 case 属于未声明 level 时，可以不执行；属于已声明 level 时，不应在 runner 内偷偷 skip。

mock server 不再被固定为 WebSocket JSON。测试时先读取 mock server 的 profile 声明：当前作为跨 runtime 基准的 generated Node mock-server 应覆盖 TCP Standard Framed 的 `core + framed-binary`；只提供 RPC-only WebSocket 控制面的 mock 才声明 `websocket-jsonrpc`。

推荐 runtime 在自己的 conformance 配置或测试命令中声明 level，例如：

```yaml
axtp_conformance:
  levels:
    - core
    - websocket-jsonrpc
```

Standard Framed runtime 的最小声明应为：

```yaml
axtp_conformance:
  levels:
    - core
    - framed-binary
```

## WS-JSON 必测项

声明 `websocket-jsonrpc` 的 runtime 至少覆盖：

| 行为 | Required cases |
|---|---|
| Hello / Identify / Identified 顺序 | `session.hello_identify_identified` |
| Identified 前拒绝业务请求 | `session.request_before_identified` |
| JSON Request / RequestResponse | `rpc.request_response_json` |
| 未知 method | `rpc.method_not_found` |
| `requestId` 匹配 | `rpc.request_id_match` |
| Advisory version matrix | `session.axtp_version_advisory` |

WebSocket JSON 不承载 CONTROL、Frame Header、CRC16 或 STREAM data packet。

## Standard Framed 必测项

声明 `framed-binary` 的 runtime 至少覆盖：

| 行为 | Required cases |
|---|---|
| CONTROL OPEN / ACCEPT | `handshake.open_accept` |
| CONTROL HEARTBEAT / HEARTBEAT_ACK | `handshake.heartbeat` |
| CONTROL CLOSE / CLOSE_ACK | `handshake.close` |
| RPC `requestId` 匹配 | `rpc.request_id_match` |
| STREAM open / data / close | `stream.stream_open`、`stream.stream_data`、`stream.stream_close` |

Standard Framed runtime 同时需要通过 `core` level 的 RPC 和错误形状 case。

## Compatibility degradation 必测项

声明 `capability` level 的 runtime 必须区分 unknown method 与 registered-but-unavailable method：前者精确返回 `RPC_METHOD_NOT_FOUND`，后者精确返回 common `NOT_SUPPORTED`。`capability.session_survives_not_supported` 随后在同一 session 发起受支持请求来证明 liveness；`capability.unknown_optional_field_ignored` 对 structurally valid unknown optional field 做同样验证。

声明 `event` level 的 runtime 必须执行三类独立行为：`event.unknown_event_ignored` 验证 receiver 对 peer fuzz input 的容忍；`event.unsubscribe_event` 验证已取消订阅 event 的 bounded non-emission；`event.unsupported_capability_not_emitted` 验证 sender 在 event capability 缺失时不发送该 event。每类 case 都以随后成功的受支持 RPC 证明 session liveness。

Case DSL 的 `expect.no_event` 表示 bounded negative expectation；`withinMs` 是有限 observation window，不允许 runner 用无限等待实现。Unknown-event compatibility/fuzz input 使用 `peer_to_runtime`，并要求 `applicationDispatched=false`，因此 diagnostics 记录不等同于向应用分发 event。

Executable step 使用唯一 `id` 标识步骤，以 `captureAs` 命名其输出，并可用 `{ ref: identified.sid }` 引用前一步捕获的字段。Capture name 和 step id 在同一 execution/scenario 内 MUST 唯一；reference 只能指向更早声明的 capture 和该输出实际存在的 field path。Missing、forward、ambiguous 或 invalid-field reference 必须在执行前使 case validation 失败。Runner MUST 解析 reference 后再发送 message；例如 advisory-version scenario 的 `REIDENTIFY.sid` 必须精确等于前一 `IDENTIFIED`（`captureAs: identified`）分配的非空 sid，不能把 reference object 或占位字符串直接发到 wire。

Case-level `semantic.kind` 和 step `role`（如 `trigger`、`degraded`、`observe`、`liveness`）承载 machine-readable invariants。Validator 按 semantic/role 关联 exact error、bounded non-emission 和后续 success，不依赖 YAML 中的固定 step index；runner 可在不破坏 capture dependency 和 protocol ordering 的前提下插入 diagnostics 或等价重排。

## Cast / video encoder parameter conformance

`cast.setVideoStreamParams` 的 conformance 覆盖 Nearcast 通过 WS 控制面配置 NT10 source encoder 的目标帧率和码率。该配置只属于当前 cast session；一次 `video.openStream` 中显式提供的 `frameRate` / `bitrateKbps` 优先级更高，且不会写回 session 默认值。`cast.setRenderFps` 仍然只控制本地 renderer，不属于本组 case。

声明 `capability` level 的 runtime 至少执行 `capability.video_stream_params_not_supported`：当 `supportsVideoStreamParams` 或选定 source 的 encoder capability 缺失时，必须精确返回 `NOT_SUPPORTED`，不能关闭 AXTP session，随后受支持 RPC 必须成功。

声明 `stream` level 的 runtime 还必须覆盖以下生命周期约束：

| Required case | 必测行为 |
|---|---|
| `stream.video_stream_params_idle_open` | source 完全没有 active audio/video stream 时，直接以新 streamId 和新 `syncGroupId` 建立完整 audio/video AV pair，不得发送 close；结果回显 negotiated frame rate / bitrate。存在 partial media state 时不得按 idle 处理。 |
| `stream.video_stream_params_active_reconfigure` | active 音视频 pair 必须先分别 `closeStream` 并等待两路 terminal `closed` barrier，再以新 streamId 和共享的新 `syncGroupId` 分别 `openStream(peerRole=transmitter)`；视频 close 使用 `reason=encodingReconfigure`，且 `applied` 只能在两路 open result 之后。 |
| `stream.video_stream_params_precedence` | `video.openStream` 显式值覆盖 session 参数，且显式值不持久化到 session。 |
| `stream.video_stream_params_rollback` | 新 open 返回 `MEDIA_FRAMERATE_UNSUPPORTED`、`MEDIA_BITRATE_UNSUPPORTED` 或 `MEDIA_STREAM_START_FAILED` 时，先完成 audio/video 双 close barrier，再以 fresh `syncGroupId` 双路新 open；清理 partial replacement 后尝试旧参数恢复，分别验证 `rolledBack + rollbackApplied` 和失败时无 complete active AV pair。 |
| `stream.video_stream_params_validation` | 空更新、字段与 `resetFields` 冲突、profile 越界以及重配置并发请求分别返回 `INVALID_ARGUMENT`、媒体能力错误或 `BUSY`。 |

不支持 source encoder 参数或 active reconfigure 的 UxPlay/AirPlay receiver 不得模拟成功，应返回 `NOT_SUPPORTED`；该降级不影响同一 session 的其他 RPC。

## Optional cast-control P0

The [cast-control profile](profiles/cast-control.yaml) binds C01–C11 under
`cases/castcontrol/` to the [normative contract](../specs/45-cast-control.md).
Its `since: spec/v0.18.0` denotes the planned next additive release, not a
published tag. The optional [cast-settings profile](profiles/cast-settings.yaml)
combines C01–C12 and maps all PS-01–PS-12 protocol obligations to executable
scenarios in C12. P0-only `cast-control` requirements stay
unchanged. The fixture's small queue/window values are test data, not defaults.

`runtime_local` steps with `control_model: { action, input, expect }` are executable
substitute-owner oracles. `input` supplies messages, faults or observations;
`expect` recursively matches object subsets and compares arrays/scalars exactly.
Each scenario starts from `cast_control_receiver`. Inputs are independent values;
unknown actions/missing expected results fail DSL validation. Capture/install,
receive/flush, admit/execute/query and monotonic advance actions expose the
separate control boundaries. Overflow/gap/invalid snapshot observations require
resync, and negative value/encoding/privacy observations fail validation.

Run `pnpm --dir tooling/generators test` for the executable reference evidence and
`tooling/scripts/validate-conformance.sh` for manifest/schema/reference checks.
The reference module ships in spec artifacts; runtime runners must map these
observations to real public RPC/events and owner instrumentation before claiming
cast-control conformance. No HWND/GStreamer or human audio proof is claimed.
The [legacy fixture](fixtures/cast-control/legacy-baseline.json) freezes identifiers
and field facts from `a2a550313bcf570361f1bfe9872851141b847520`; descriptions are
excluded. Generated [JSON/TLV8 pairs](../contract/test-vectors/cast_control_encoding.json)
exercise decimal string fidelity, including >2^53 and uint64 bounds.

P1 uses [substitute settings/store/policy owners](reference/cast-settings.mjs)
and the cast_settings_receiver fixture. `settings_admit/execute/complete_store`
separate admission, owner validation and blocked storage; P0 control can execute
during the storage boundary. Settings capture/install/publish/receive/flush use an
independent cursor. Store/observe/restart actions inject foreign edits, invalid
stores, known policy limits or a crash/new epoch; they are oracle actions, not new
wire APIs or product restart authorization. Generated
[settings JSON/TLV8 pairs](../contract/test-vectors/cast_settings_encoding.json)
prove string fidelity separately from [P0 pairs](../contract/test-vectors/cast_control_encoding.json).
The frozen [local P0 baseline](fixtures/cast-control/p0-baseline.json) guards all
earlier work. Real disk algorithms, GStreamer, arbitration, layout and UI still
require downstream implementation and physical acceptance.

## STREAM P0 范围

Phase 1 STREAM conformance 验收的是 audio/video 媒体流数据面：

| 内容 | Phase 1 期望 |
|---|---|
| 建流 / 关流 | 通过已采纳或测试 profile 中的业务 RPC 创建 / 关闭 Stream Context。 |
| STREAM Header | 使用 16B header：`streamId:uint32`、`seqId:uint32`、`cursor:uint64`。 |
| STREAM data | Standard Framed `PayloadType=STREAM`，不走 WebSocket JSON。 |
| ACK/NACK | 不作为 Phase 1 strict retransmission 前置要求。 |

`stream.stream_open` / `stream.stream_close` 验收的是 P0 媒体流业务域建流，不是要求存在通用 `stream.open` / `stream.close` 业务 method。

## Deferred 范围

Phase 1 暂不验收：

- ACK/NACK 严格重传。
- RESUME / SESSION_RESET。
- PING/PONG RTT 测量。
- STREAM 固件 / 文件 / 日志 profile。
- 链路加密、证书、token 或签名协商。
- 低带宽 BLE / UART / HID-64 降级 profile。

## 运行命令

主库验证 conformance 源文件：

```bash
pnpm --dir tooling/generators install --frozen-lockfile
pnpm --dir tooling/generators build
tooling/scripts/validate-conformance.sh
```

runtime 仓库接入时应设置：

```bash
export AXTP_SPEC_PATH=/path/to/axtp-spec-or-release-artifact
```

runtime runner 应读取 canonical conformance 路径：

```text
$AXTP_SPEC_PATH/conformance
```

## 失败分类

| 分类 | 判断方式 | 处理 |
|---|---|---|
| runtime bug | case 与 spec/generated 一致，runtime 行为不符合 | 修 runtime。 |
| spec-case mismatch | case 与 `specs/**` 或 `contract/protocol/axtp.protocol.yaml` 冲突 | 先修主库 conformance case 或 spec。 |
| generated mismatch | case 依赖 method/event/schema，但 generated 中不存在 | 回到 registry / Generator，不手写 generated。 |
| profile declaration error | runtime 未实现某 level 却声明支持 | 调整 runtime support level 或补实现。 |
| test environment | 设备画像、transport、mock 配置或 spec path 错误 | 修 runner 配置和 fixtures。 |

若 case 与 Phase 1 裁决冲突，应先修正主库 conformance case 和 manifest，再要求 runtime 通过。
