# AXTP 产品领域状态

本页是产品 / 架构负责人查看 AXTP 业务能力覆盖、采纳优先级和下一步动作的入口。

这里展示的是产品状态看板，不是 runtime 实现合同。runtime 能直接依赖的合同仍然是 `contract/registry/**`、`contract/protocol/axtp.protocol.yaml`、`contract/generated/**`、`specs/**` 和 `conformance/**`。

## 领域状态矩阵

> 更新规则：新增、删除、采纳、废弃 domain 草案或 registry domain 后，必须同步本矩阵的评审状态、优先级和下一步。
> 计数规则：草案数和已生成数量由 `tooling/scripts/check-protocol-status.mjs` 校验。
> 验证方式：只有 `contract/registry/domains/<domain>/domain.yaml` 存在，并且 `pnpm --dir tooling/generators validate:sources` / `validate:protocol` 通过，才算 generated/adopted；其余为草案状态。
> 草案健康和示例质量：见 [协议草案健康度](protocol-draft-health.md)，由 `tooling/scripts/report-protocol-draft-health.mjs` 生成并校验。

本表用于让产品、架构、研发和测试快速判断每个 domain 当前走到哪里。`已生成` 统计当前 generated protocol 中已经落地的方法和事件数量；`video` / `audio` 的 P0 stream 优先级表示要同时采纳 RPC 建流/关流控制面和 STREAM 数据面字段约束。

| 领域 | 草案数 | 评审 | 已生成 | 优先级 | 下一步 |
|---|---:|---|---:|---|---|
| audio | 12 | ASK | 13 | 旁路高覆盖 / P0 stream | `audio.algorithm` 与 `audio.stream` 已进入 generated；后续按修订流程维护并补齐剩余 audio 草案确认。 |
| auth | 3 | ASK | 0 | 待排期 | 补产品/设备/legacy 确认。 |
| cast | 12 | ASK / generated | 47 | P0/P1 多路控制已采纳 | `cast.presentation`、`cast.audioPolicy`、`cast.controlOperations`、`cast.presentationSettings` 已登记；P0 原子快照/提交组及 P1 单字段配置、保存/应用部分结果、独立设置流见 specs/45-cast-control.md 与 specs/46-cast-settings.md。实际产品接入仍需下游实现验收。 |
| camera | 7 | ASK | 0 | P3/P4 | 补产品/设备/legacy 确认。 |
| capability | 1 | ASK | 0 | 待排期 | 补产品/设备/legacy 确认。 |
| device | 6 | ASK | 5 | P1 | `device.info`、`device.enrollment` 已进入 generated；继续补 `device.childDevice` 等通用底座确认。 |
| diagnostic | 10 | ASK | 0 | P5 | 补产品/设备/legacy 确认。 |
| display | 6 | ASK | 0 | 待排期 | 补产品/设备/legacy 确认。 |
| file | 2 | ASK | 0 | 待排期 | 补产品/设备/legacy 确认。 |
| firmware | 3 | ASK | 6 | 旁路高覆盖 | `firmware.update` P0 已进入 generated；后续补 firmware info/policy 与 P1/P2 更新能力确认。 |
| input | 5 | ASK | 0 | 待排期 | 补产品/设备/legacy 确认。 |
| log | 3 | ASK | 0 | 待排期 | 补产品/设备/legacy 确认。 |
| network | 6 | ASK | 26 | 旁路高覆盖 | `network.interface`、`network.ip`、`network.wifi`、`network.ap` 已进入 generated；继续补蓝牙和服务端点草案确认。 |
| output | 1 | ASK | 0 | P2b | 补产品/设备/legacy 确认。 |
| privacy | 3 | ASK | 0 | 待排期 | 补产品/设备/legacy 确认。 |
| room | 5 | ASK | 0 | P7 | 补产品/设备/legacy 确认。 |
| signage | 2 | ASK | 7 | P7 | `signage.playlist` 已进入 generated；补产品/设备/legacy 确认。`signage.media`/`signage.osd`/`signage.schedule` 已合并到其他域。 |
| software | 2 | ASK | 8 | P7 | `software.config`、`software.updatePolicy` 已进入 generated，承接应用/launcher 侧配置和更新策略。 |
| sport | 2 | generated | 9 | P2 | `sport.eventDetection` 与 `sport.basketball` 已进入 generated；新增多设备 Goal/Shot 水印和事件视频片段窗口配置，后续按修订流程维护。 |
| storage | 6 | ASK | 0 | 待排期 | 补产品/设备/legacy 确认。 |
| stream | 2 | ASK | 12 | P0 data-plane plumbing | Phase 1 需要通用 STREAM open/data/close 语义支撑 audio/video；具体业务参数仍由 audio/video profile 定义。 |
| system | 6 | ASK | 0 | P1 | 补产品/设备/legacy 确认，优先进入采纳批次。 |
| video | 12 | ASK | 9 | P0 stream | `video.stream` 已进入 generated；后续按修订流程维护，并推进 framing/layout/encoder 等后续能力。 |

## 协议采纳/生成优先级

本表用于安排后续从草案到 YAML、再到 generated 产物的顺序。统计依据为 `workspace/legacy-migration/classification/legacy-protocol-classification.csv` 和 `workspace/legacy-migration/classification/README.md`；当前 `contract/generated/protocol.md` 已覆盖 `audio.algorithm`、`audio.stream`、`device.info`、`firmware.update` P0、`network.interface`、`network.ip`、`network.wifi`、`network.ap` 和 `video.stream`，其余业务域仍按草案评审推进。

| 优先级 | 生成批次 | Legacy 覆盖 | 当前状态 | 建议动作 |
|---|---|---:|---|---|
| P1 | `device.*` / `system.*`：`device.info`、`device.childDevice`、`system.state`、`system.lifecycle`、`system.reset`、`system.initialization`、`system.time` | 64 条，覆盖 AXDP / Rooms / Signage / VM33 | `device.info` 已进入 generated；其余草案待采纳 | 继续采纳通用底座。它们是设备识别、运行时状态事件、重启/关机、软重置、初始化、时间同步等基础事实；独立 system power 和 system health 草案已移除。 |
| P2 | `video.framing` | 22 条，主要来自 AXDP，少量 VM33 | 草案已存在，未进入 generated | 紧跟 P1 生成。framing mode 是 legacy 高频控制项，也会影响 camera tracking、布局和画面输出的语义边界。 |
| P2b | `output.layout` / `video.layout` | 28 条，`output.layout` 20 条、`video.layout` 8 条 | 草案已存在，未进入 generated | 原始优先级清单未单列，但 legacy 覆盖高。若近期要做输出画面、拼接、多画面或分屏，建议插在 P2 后、camera 控制前。 |
| P3 | `camera.focus`、`camera.zoom`、`camera.ptz` | 20 条，覆盖 AXDP / VM33 | 草案已存在，未进入 generated | 按同一 camera control 批次采纳。focus 和 zoom 命中多，PTZ 条目少但和控制体验强相关，适合一起确认 schema 与状态事件。 |
| P4 | `camera.image`、`camera.exposure`、`camera.calibration`、`camera.whiteBalance` | 12 条直接归类；VM33 Camera 配置另有 whiteBalance 字段线索 | 草案已存在，未进入 generated；`camera.image` 有低置信度拆分问题 | 先补 VM33 Camera 配置字段，再采纳 image/exposure/calibration；whiteBalance 当前分类 CSV 没有直接命中，建议随 camera 配置批次保留，但放在本批次后段确认。 |
| P5 | `diagnostic.*` 产测：`diagnostic.networkTest`、`diagnostic.manufacturing`、`diagnostic.selfTest`、`diagnostic.audioTest`、`diagnostic.inputTest`、`diagnostic.storageTest`、`diagnostic.videoTest`、`diagnostic.kvmTest` | 33 条，覆盖 AXDP / Rooms / VM33 | 草案已存在，未进入 generated | 面向工厂和维修闭环，建议在用户侧 camera / video 基础能力之后生成；如果产线接入排期更早，可把 `diagnostic.manufacturing` 与 `diagnostic.networkTest` 提前成 P3b。 |
| P6 | AXTP core / `axtpctl` | 不属于 legacy business 分类；core 已在 `contract/registry/core/**`、`contract/registry/schema/**`、`contract/registry/error/**` 和 generated 中存在 | Core 已生成；`axtpctl` 属于工具/SDK 跟随项 | 不作为普通业务草案插队。每完成一批业务生成后，再让 `axtpctl` 命令、测试向量和 runtime 示例跟随 generated 事实更新。 |
| P7 | `room.*` / `signage.*` / enrollment / appearance：`room.source`、`room.layout`、`room.schedule`、`room.participant`、`signage.playlist`、`signage.playback`、`device.enrollment`、`software.config` | 28 条，主要来自 Rooms / Signage / VM33 | 草案已存在，未进入 generated | 放后面生成。它们更偏产品场景和应用层编排；若 Rooms 业务先启动，可单独把 `room.source` 提前，因为它有 11 条 legacy 命中。原 `device.binding` 已收敛为 `device.enrollment`，原 `device.appearance` 已合并到 `software.config`。 |

### 旁路高覆盖候选

以下能力不在本轮主优先级清单里，但 legacy 覆盖量高，排期时不应遗漏：

| 候选能力 | Legacy 覆盖 | 建议 |
|---|---:|---|
| `network.wifi`、`network.ip`、`network.ap` | 38 条 | 若设备上线、配网或发现流程先行，应作为 P1 后的并行批次。 |
| `firmware.update` / `firmware.updatePolicy` | 22 条 | 跨 AXDP / Rooms / Signage / VM33，适合在基础 device/system 生成后启动采纳。 |
| `audio.volume`、`audio.input`、`audio.recording` | 31 条 | `audio.algorithm` 已 generated；这些可作为音频第二批。 |
| `video.stream`、`video.ndi`、`video.rtsp` | 23 条 | 与 framing、camera、layout 相关，建议等 P2/P3 的控制面边界稳定后采纳。 |

## 后续治理事项

- 将本页从手动维护逐步迁移为从 registry、generated JSON 和 protocol frontmatter 自动生成。
- legacy 拼写兼容页已移除；活跃文档只使用 `cast-receiver-uxplay` 和 `cast-rxtx-pairing` 正确路径。
