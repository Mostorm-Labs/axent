---
status: generated
contract: true
generated: true
domain: input
feature: input.stream
registry: ../../../../contract/registry/domains/input/domain.yaml
lastReviewed: 2026-11-03
---

# input.stream

## 0. 速读结论

| 项目 | 内容 |
|---|---|
| 业务流 | `streamKind=touchback` |
| 方法 | `input.openStream` (`0x0A01`), `input.closeStream` (`0x0A02`) |
| capability | `input.stream` (`0x0A01`) |
| 方向 | `receiver_to_source` |
| 格式 | `touch-batch-v1` |
| 承载 | WebCast control DataChannel 或 AXTP HID stream |
| 可靠性 | ordered + reliable；无逐帧应用 ACK |
| 坐标 | content viewport normalized `uint16` 0..65535 |
| 模式 | `touch` 默认 preferred；`mouse` 可显式 required |

本文解释已生成 registry 的输入 stream 事实。数字 ID、schema field ID、错误
集合和能力字段必须从 registry/generated 读取；本文不重新分配 ID。

## 1. `input.openStream`

请求 `InputOpenStreamParams` 必须包含 `controlVersion`、`runtimeInstanceId`、
完整 `target`、`streamKind=touchback`、`format=touch-batch-v1`、
`supportedModes`、`canvasRevision`、`geometryRevision`、`canvasToken`、
`geometryToken` 和幂等 `commandId`。`preferredMode` 默认 `touch`；
`modeRequirement=preferred` 可按对端明确声明的能力回退，`required` 不能回退。
`maxBatchEvents`、`maxFrameBytes` 和 `maxContacts` 是请求方可接受的上限。

target fence 的字段为 `sessionId`、`sessionGeneration`、`slotId`、
`bindingGeneration` 和 `lifecycleGeneration`。所有 generation 都必须与当前
presentation binding 完全匹配；只匹配 slot 或投屏码是不安全的。

结果 `InputOpenStreamResult` 返回 `streamId`、`epoch`、`acceptedMode`、
`format`、服务端重新确认的 canvas/geometry token、协商 limits、`revision` 和
`accepted`。`accepted=true` 只表示 owner lane 接受了请求；数据发送必须等
产品层确认 target 的 `effectiveEnabled` 和 active 状态。

模式选择：

```text
preferred=touch, peer supports touch       -> touch
preferred=touch, peer explicitly lacks it,
  peer supports mouse and requirement=preferred -> mouse
mode=mouse, requirement=required,
  peer lacks mouse                          -> NOT_SUPPORTED/unavailable
```

未知或未声明的模式不得推断为支持。模式变更必须先 reset、close 再 open，新的
epoch 才能发送新模式事件。

## 2. `input.closeStream`

请求必须包含相同 target fence、`streamKind`、`streamId`、`epoch` 和幂等
`commandId`；`reason` 可为 `requested`、`route_changed`、`disconnect`、
`timeout`、`reset` 或 `shutdown`，`finalSeqId` 表示最后接受的序号。

close 的语义是释放该 epoch 拥有的所有 contacts/buttons 并使后续旧 frame
失效。重复 close 必须幂等收敛；结果通过 `closed`、`alreadyClosed`、`revision`
表达，不要求再发送应用 ACK。

## 3. Canonical STREAM packet

每条 WebCast binary message 和 HID stream 数据都必须遵守：

```text
AXTP STREAM header, 16 bytes, big-endian
  streamId:uint32
  seqId:uint32
  cursor:uint64          // sender monotonic timestamp, microseconds

TouchBatch payload header, 28 bytes, big-endian
  magic:uint16 = 0x5442
  version:uint8 = 1
  kind:uint8 = batch(0) | reset(1) | close(2) | bind(3) | capability(4)
  flags:uint16
  headerLen:uint16 = 28
  epoch:uint32
  canvasToken:uint32
  geometryToken:uint32
  eventCount:uint16
  payloadLen:uint16
  crc32:uint32
```

`crc32` 覆盖 TouchBatch payload header（将 crc 字段置零）和 payload，不覆盖
外层 16 字节 STREAM header。AXTP 的所有整数都是 network byte order。batch
中的每个事件固定 16 字节：

```text
contactId:uint16
phase:uint8       // down=0, move=1, up=2, cancel=3
buttons:uint8
x:uint16          // normalized 0..65535
y:uint16          // normalized 0..65535
modifiers:uint16
wheel:int16
dtMs:uint16
recordFlags:uint16
```

`eventCount` 为 batch 事件数量；reset/close/bind/capability 帧不携带普通
事件，控制内容放在 payload。未知 magic/version、非法长度、CRC 错误、未知
phase 或超过协商 limit 的帧直接丢弃并计数。

## 4. Ordering, coalescing and reset

- `seqId` 在一个 stream/epoch 中单调递增；重复或跳跃都必须被检测。
- `down`、`up`、`cancel` 必须按 contact 顺序交付。`move` 可以按 contact 合并，
  但不得跨越尚未交付的 up/cancel。
- DataChannel 配置 ordered + reliable；AXTP HID transport 使用其等价的可靠
  stream profile。应用层不实现每帧 ACK，以免增加高延迟链路的反向流量。
- 发生 sequence gap、epoch/token 不匹配、CRC/长度错误、超时或队列溢出时，
  receiver/NA20 必须停止普通输入，释放 active contacts/buttons，尽可能发送
  幂等 reset，然后关闭并用新 epoch 重建。
- `touch.reset` 是 stream control frame，不是 ACK；它必须可重复接收且结果
  相同。close、disconnect、失焦、隐藏、target generation 变化也必须执行
  release/reset。

## 5. Canvas and coordinate fence

NearCast receiver 先把本地屏幕点转换到 content viewport，再转换旋转、镜像和
fit 矩阵，最后量化为 normalized `x/y`；Tauri sender/NT10 只消费已归一化坐标。
帧只能使用 open 返回的
`canvasToken`/`geometryToken`；token 只在 `streamId + epoch` 内有效，不能跨
target 或 runtime 复用。

canvas/geometry 变化必须关闭旧 epoch。收到旧 token 的 batch 应丢弃并留下
stale-geometry 诊断，不能把旧坐标重放到新布局；诊断可映射到标准
`INVALID_STATE`/`INVALID_ARGUMENT`，不要求新增错误码。

## 6. Touch and mouse semantics

AXTP 只传输 contact/button/wheel 基础语义。`touch` 模式保留 down/move/up/cancel；
`mouse` 模式可携带 primary/secondary/middle、drag、wheel 和 modifiers。长按
右键、长按拖选文本等识别在 NearCast receiver 的 presentation input owner 中
完成，NA20 和 NT10 只执行已确定的语义。

## 7. Adapter responsibilities

| Adapter | MUST | MUST NOT |
|---|---|---|
| WebCast receiver | 采集本地输入并向 source 发送 control DataChannel binary message；校验本地 frame/target/token，并将事件交给对应 target owner | 不另建 JSON touch 协议，不把 airplay 连接当作 touchback |
| Tauri sender/source | 校验 receiver 发来的 frame/target/token，并在绑定的 Windows source 上执行 touch/mouse injection | 不把 stale/gap/CRC 或 injection error 报成成功 |
| NA20 | 原样透传 envelope、epoch、sequence、tokens、phase、buttons、wheel、reset/close，按序报告 transport error | 不改坐标、不识别手势、不混合媒体、不新增 relay capability |
| NT10 | 校验并把 normalized 坐标映射到本地分辨率，生成 HID touch/mouse/wheel，错误时 release | 不接受旧 epoch、不静默吞掉 up/cancel、不把 injection error 报成成功 |

## 8. Conformance vectors

实现至少验证：空 batch、down/move/up、multi-contact、cancel、reset/close、
大端字段、CRC failure、unknown version、长度边界、seq gap、旧 epoch/token、
move coalescing、queue overflow 和断链 release。golden vector 的前 16 字节
必须始终是 AXTP STREAM header，不能出现私有替代 header。
