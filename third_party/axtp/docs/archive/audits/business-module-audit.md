# AXTP 业务模块审计报告

> Date: 2026-06-10
> Phase: 文档治理第五阶段，业务模块文档审计
> Scope: business / flows / protocol drafts / legacy migration / registry domains / generated / conformance
> Constraint: 本报告不修改 registry、Protocol IR、generated、conformance、specs、README，也不改变任何 methodId、eventId、errorCode、schema 或 fieldId。

## 1. 总体结论

当前业务模块体系已经有较完整的草案目录，但正式 runtime contract 仍然很窄：`contract/registry/domains/audio/domain.yaml` 目前只采纳了 `audio.algorithm`，generated method registry 只有 4 个 audio algorithm methods，generated event registry 只有 1 个 `audio.algorithmConfigChanged`，generated capability registry 只有 `audio.algorithm` 这一项业务 capability。

`workspace/protocol/**` 共有 103 个草案文件，覆盖 20 个 domain。大多数草案是按模板生成的 draft intake：有 capability 名称和候选 method/event，但缺少 schema、error、legacyRefs、conformance case 和边界复核。它们适合作为 review 输入，不适合作为 runtime 实现合同。

优先级上，建议先处理两类模块：

1. 已有 flow、legacy 覆盖高、schema 边界较清楚的模块，例如 `audio.eq`、`audio.volume`、`audio.input`、`device.info`、`system.lifecycle`。
2. 媒体流 P0 主线依赖的模块，例如 `audio.stream`、`video.stream`、`stream.flowControl`，但这些需要先统一 STREAM profile、source、clockDomain 和 conformance。

本轮没有发现需要修改 specs 的协议语义冲突。主要问题是业务草案之间的边界和生命周期状态不够清楚，尤其是 source/input/routing/output/volume/mixer 这类音频链路概念容易互相吞并。

## 2. 模块状态概览

| 类别 | 现状 | 证据 | 审计结论 |
|---|---|---|---|
| Generated / adopted | `audio.algorithm` | `contract/registry/domains/audio/domain.yaml`、`contract/generated/method_registry.generated.md`、`contract/generated/event_registry.generated.md`、`contract/generated/capability_registry.generated.md` | 可作为 runtime contract；后续修改走 amendment。 |
| Rich review draft | `audio.eq`、`audio.recording`、`audio.stream`、`video.stream`、部分 device/system drafts | 草案中已有较多 schema、错误、边界或 flow 说明 | 可进入下一轮精修，不能直接进 registry。 |
| Template draft | 多数 `workspace/protocol/<domain>/<feature>.md` | 只有候选 methods/events 和 `[REVIEW-DRAFT]` / `[REVIEW-ASK]` | 需要补 flow/schema/capability/error/legacyRefs。 |
| Business / flow input | `device-system-info`、`device-streaming-audio-video`、`signage-device-management` 等 | `workspace/business/**`、`workspace/flows/**` | 是 protocol 草案和采纳优先级输入，不是实现合同。 |
| Legacy evidence | AXDP / Rooms / VM33 / Signage 分类和 evidence | `workspace/legacy-migration/classification/**` | 只能作为 mapping 证据；不能直接生成 registry。 |
| Conformance | core/session/rpc/event/stream/capability cases；audio fixture 指向 `audio.algorithm` | `docs/conformance/**` | 有基础框架，但业务模块级 coverage 仍薄。 |

## 3. 重复或边界模糊的 domain.feature

| 边界 | 问题 | 建议 |
|---|---|---|
| `audio.input` / `audio.source` / `audio.routing` / `audio.lineIn` | `audio.source` 和 `audio.lineIn` 当前没有独立草案；legacy 把 Line-in、Mic、音源选择都归到 `audio.input`。`audio.routing` 又定义输入、输出、处理链路之间的关系，容易和 input source selection 重叠。 | 暂不新增 `audio.source` / `audio.lineIn` capability。把 line-in 作为 `audio.input` 的 target/source type；把 routing 限定为“输入、处理节点、输出 sink 的连接矩阵”。 |
| `audio.volume` / `audio.input` / `audio.mixer` | `audio.volume` legacy 包含 line-out volume、mute、default volume，也容易被塞入 line-in preGain、channel gain、mixer gain。 | `audio.volume` 只治理用户可感知的 output volume/mute/default；line-in preGain 归 `audio.input`；mixer gain 归 `audio.mixer`；算法 AGC 归 `audio.algorithm`。 |
| `audio.eq` / `audio.algorithm` | EQ 是频段/preset/tonal shaping；algorithm 是降噪、AEC、AGC、beamforming、DOA 等 DSP 配置。 | 保持拆分。`audio.eq` 不并入 `audio.algorithm`，`audio.algorithm` 也不承载 EQ preset/band。 |
| `audio.backgroundMusic` / `audio.playback` / `signage.media` | `audio.backgroundMusic` 当前没有草案；它可能是设备侧播放任务，也可能是 signage playlist/media 的一部分。 | 若是设备侧播放音频任务，归 `audio.playback`；若是数字标牌内容编排，归 `signage.media` / `signage.playback`；不要新增缺少边界的 `audio.backgroundMusic`。 |
| `audio.output` / `audio.volume` / `audio.routing` | `audio.output` 目前是模板草案，legacy 分类没有明显覆盖；容易和 output port、volume target、routing sink 混在一起。 | 暂缓采纳。只有当设备有独立 output port mode / sink capability 时保留 `audio.output`；否则作为 `audio.routing` sink 或 `audio.volume` target。 |
| `stream.*` / `audio.stream` / `video.stream` | 公共 stream 域提供数据面和流控；音视频域负责业务建流和 profile metadata。 | 保持分层。业务 open/close/state 在 audio/video，STREAM 16B header 不增加业务字段。 |

## 4. 可以进入 registry 的候选模块

| 模块 | 理由 | 前置动作 |
|---|---|---|
| `audio.algorithm` | 已 generated。 | 只做 amendment；补齐 conformance schema 严格性。 |
| `audio.eq` | legacy 覆盖高，草案已有正式协议语气、边界、错误策略和 default/reset 处理结论。 | 补 frontmatter/status，提取 schema 和 errors，补 conformance cases。 |
| `audio.input` | legacy 覆盖高，尤其 Line-in preGain、AudioInMode、Mic used。 | 先补 target/source 模型、lineIn preGain range/unit、detect state event。 |
| `audio.volume` | legacy 覆盖高，覆盖 line-out volume、mute、default volume、HID reports。 | 先拆清 output volume / input preGain / mixer gain；补 state vs config 方法。 |
| `device.info` / `system.lifecycle` / `system.reset` / `system.state` | 已有 business/flow 驱动，且是设备管理底座。 | 按新 checklist 复核 schema、events、reset/default/factory 关系和 conformance。 |

## 5. 需要先补 flow/schema/capability/event 的模块

| 模块 | 缺口 |
|---|---|
| `audio.routing` | 需要明确 routing graph：source、processing node、sink、routeId、active route、routing state event。 |
| `audio.output` | 需要明确 output port/sink 的业务价值；否则并入 routing/volume。 |
| `audio.mixer` | 需要定义 mixer item、bus、gain、mute、solo、matrix/mix mode，与 volume 的边界。 |
| `audio.playback` | 需要区分设备侧播放任务、背景音乐、signage media playback、local file playback。 |
| `audio.stream` | 需要固定 AAC transportFormat、clockDomain、receiverClockDomain、stream profile、AudioChunkHeader 是否属于 payload envelope。 |
| `audio.recording` | 需要等 `file.transfer` / file 域明确，解决 `deliveryMode=file` 和 fileRef 生命周期。 |
| `audio.uac` | 需要确认 UAC service 状态、host role、enable/config/state 关系。 |
| `audio.dante` | 需要确认 Dante license/vendor 信息是否应进入 audio.dante，还是 auth/vendor/license。 |

## 6. 应合并的模块

| 建议合并 | 原因 | 目标 |
|---|---|---|
| `audio.lineIn` -> `audio.input` | 当前没有独立草案；legacy 中 Line-in preGain、LineInMode 都已归 `audio.input`。 | `audio.input` schema 增加 `target=lineIn`、`preGain`、`mode`、`detectState`。 |
| `audio.backgroundMusic` -> `audio.playback` 或 `signage.media` | 当前没有独立草案；“背景音乐”更像播放任务或内容编排，不是底层音频源。 | 根据产品语义选择 `audio.playback` 或 `signage.media`。 |
| `audio.source` -> `audio.input` / `audio.stream` source schema / `audio.routing` node | 当前没有独立草案；source 是多个 feature 的字段概念，不必先成为 capability。 | 作为 schema model 统一 source descriptor，暂不建独立 capability。 |

## 7. 应拆分的模块

| 模块 | 拆分建议 | 理由 |
|---|---|---|
| `audio.volume` | 保留 `audio.volume` = output volume/mute；把 input preGain 放入 `audio.input`，mixer gain 放入 `audio.mixer`。 | legacy “volume/gain/mute”混杂，直接采纳会造成 schema 过大和边界不清。 |
| `audio.input` | 区分 input port inventory、input config、input state/detect event。 | `CommonAudioInputDetect` 是 detect/state，不应只映射成 setConfig。 |
| `audio.routing` | 区分 route config 与 route state；避免承载 input/output 的静态能力。 | routing 应表达连接关系，不表达单个端口能力。 |
| `audio.algorithm` 未采纳遗留项 | beam/DOA result 可拆未来 `audio.beam`；算法授权拆到 auth/vendor/license 或 diagnostic。 | 当前 generated 范围只覆盖配置；实时结果和授权不是 configChanged。 |

## 8. 只适合 legacy adapter 的模块或条目

| 条目 | 原因 | 建议 |
|---|---|---|
| `CommonGetAlgAuthContent` / `CommonSetAlgAuthContent` | 授权内容不等于运行时算法配置。 | 先 adapter-only；确认后归 auth/vendor/license 或 diagnostic。 |
| `CommonPauseAiAlgThrd` / `CommonContinueAiAlgThrd` | 算法线程控制偏运行态/诊断，不是普通 setAlgorithmConfig。 | adapter-only，或后续拆 algorithm runtime action。 |
| `CommonAudioBeamReport` | 更像 beam/DOA result 上报，不是 config set。 | adapter-only，或未来 `audio.beam` event。 |
| Legacy “写默认值”命令 | 语义可能是写默认 profile，不等同于 reset 当前配置。 | 不直接映射 reset；需要确认 default profile 语义。 |
| 厂商/授权/Dante manufacturer/license 字段 | 可能属于 vendor/auth/license，不一定是 audio runtime。 | 暂缓进入 audio.dante stable schema。 |

## 9. 与 specs 冲突或潜在冲突的模块

| 模块 | 潜在冲突 | 处理建议 |
|---|---|---|
| `audio.stream` | 草案中的 chunk envelope、clockDomain、sync fields 不能改变 STREAM 16B header。 | 保持所有业务 metadata 在 RPC schema / stream profile / STREAM data payload 内。 |
| `video.stream` / `audio.stream` | source proxy control 可能把 NA20 与 NT10 内部协议误写成 AXTP wire。 | 只定义 Host 到 AXTP Logical Server 的 method；设备内部实现不进入 AXTP wire。 |
| `audio.volume` | 如果包含 line-in preGain 或 mixer gain，会违反 `domain.feature` 控制范围清晰原则。 | 采纳前拆 schema target。 |
| `audio.routing` | 如果用来表达所有 source/input/output 静态能力，会和 input/output/source 重复。 | routing 只表达连接关系和 active route。 |
| `audio.algorithm` conformance | 当前 conformance event 示例只给 `reason/applyState`，而 registry event schema 还要求 `requiresAudioRestart` 和 `config`。 | 后续补 schema validation conformance；本报告不改 conformance。 |
| `eventMasks` conformance | `event.subscribe_event` 示例的 mask 形状需要和 RPC Session spec 的 Domain Block 格式复核。 | 后续 conformance 专项修正；本报告不改 conformance。 |

## 10. Conformance 缺口

| 缺口 | 影响 | 建议 |
|---|---|---|
| `audio.algorithm` schema validation 不足 | 已 generated 模块缺少 required/range/invalid selector/partial update 的严格验收。 | 增加 get/set/reset happy path、invalid params、out-of-range、missing required、unknown algorithm cases。 |
| `audio.algorithmConfigChanged` event shape 不完整 | event case 不能验证 generated event schema。 | 增加完整 payload case，并验证 `requiresAudioRestart`、`config`、`changedFields`。 |
| capability discovery 仍偏通用 | 只验证 capability-method binding，未验证 `audio.getAlgorithmCapabilities` 返回策略和默认值。 | 增加 capability response schema 和 unsupported capability cases。 |
| stream conformance 是通用层 | `audio.stream` / `video.stream` 的 business open/close/profile 尚无 case。 | 待采纳后增加 media stream profile conformance。 |
| draft-only 模块无 conformance stub | 进入 registry 前无法判断测试成本。 | 每个 review-ok draft 至少列出 conformance case outline。 |

## 11. audio 模块重点审计

| module | current_status | evidence | problems | action | priority |
|---|---|---|---|---|---|
| `audio.algorithm` | generated / contract | `contract/registry/domains/audio/domain.yaml`; generated 4 methods + 1 event + capability; flow `audio-algorithm-level-control` | 未采纳 legacy 授权、默认值读写、AI 线程、beam report；conformance schema 覆盖薄。 | 保持 generated；后续走 amendment 补 conformance 和待确认 legacy 分类。 | P0 maintain |
| `audio.volume` | draft / high legacy | protocol draft; legacy count 16; signage flow line-out volume | volume/mute/default/lineOut 清楚，但容易吞 input preGain、channel gain、mixer gain；缺 schema/range/unit/state event。 | 第一批 candidate 之一，但先拆清 target：output volume/mute 留本模块，input preGain 归 input，mixer gain 归 mixer。 | P1 |
| `audio.source` | missing / concept only | 未发现 `workspace/protocol/audio/audio.source.md`; source 字段出现在 audio.stream/video.stream/room.source | 作为 capability 会和 input/routing/stream source descriptor 重复。 | 暂不建独立 capability；沉淀为 source descriptor schema，被 input/routing/stream 引用。 | P2 defer |
| `audio.routing` | draft / weak evidence | protocol draft; no direct legacy count in classification summary | 与 source/input/output 边界模糊；缺 route graph schema、route state event、legacyRefs。 | 暂缓采纳；先定义 routing graph 和 active route，不承载 port/source 静态能力。 | P2 |
| `audio.eq` | rich draft / high legacy | protocol draft v0.2; legacy count 8; review conclusions already distinguish algorithm/default reset | 较成熟，但没有 generated/frontmatter；需要 fieldId/schema/error/conformance 转译。 | 作为第一批 registry candidate；补 schema、errors、capability binding、conformance。 | P1 |
| `audio.mixer` | template draft / medium legacy | protocol draft; legacy count 3 | mixer item/bus/gain/mute/mix mode 未建模；易与 volume 重叠。 | 第二批；先明确 mixer gain 与 output volume 的边界。 | P2 |
| `audio.output` | template draft / low evidence | protocol draft; classification summary未显示 `audio.output` 命中 | 与 volume target、routing sink 可能重复；缺明确业务场景。 | 暂缓；若只是 line-out volume，合并到 `audio.volume` target；若是 output port mode，再保留。 | P3 |
| `audio.input` | draft / high legacy | protocol draft; legacy count 11; signage flow line-in preGain | 把 input port、input source、Line-in、Mic、detect state 混在一起；`CommonAudioInputDetect` 不应作为 setConfig。 | 第一批 candidate，但先补 target/source/preGain/mode/detectState schema 和 event。 | P1 |
| `audio.backgroundMusic` | missing / product concept | 未发现独立 protocol draft；可能来自 signage/playback 场景 | 名称偏应用场景，不是底层 audio capability；可能和 signage media/playback 重叠。 | 不新增独立模块；按语义并入 `audio.playback` 或 `signage.media`/`signage.playback`。 | P3 defer |
| `audio.lineIn` | missing / should merge | 未发现独立 protocol draft；legacy LineInMode/LineInPreGain 已归 `audio.input` | 单独建 feature 会和 input/routing/volume 重复。 | 合并到 `audio.input`，用 `target=lineIn` / `sourceType=line_in` / `preGain` 表达。 | P1 merge |

## 12. audio 分层建议

| 层级 | 建议 feature | 说明 |
|---|---|---|
| Port/source inventory | `audio.input`、可选 `audio.output` | 描述物理/逻辑音频端口、lineIn/mic/uac 等输入能力和状态。 |
| User-facing level | `audio.volume` | 输出音量、静音、默认音量；不含 mixer/internal gain。 |
| Signal processing | `audio.algorithm`、`audio.eq` | 算法增强与 EQ 分开。AGC 是 algorithm，不是 volume。 |
| Mixing/routing | `audio.mixer`、`audio.routing` | mixer 负责 bus/gain/mix item；routing 负责源到处理链/输出的连接。 |
| Runtime media stream | `audio.stream` | 业务建流和媒体 profile；连续数据走 STREAM。 |
| Task playback/recording | `audio.playback`、`audio.recording` | 设备侧播放任务和录制任务；文件化结果与 file domain 协作。 |

## 13. 建议的模块整改优先级

1. `audio.algorithm` conformance amendment：补 schema validation 和 event payload case。
2. `audio.eq` registry candidate：它边界清楚、legacy 证据集中、草案成熟。
3. `audio.volume` + `audio.input` 联合精修：同时解决 lineOut volume 与 lineIn preGain，避免互相污染。
4. `audio.stream` + `video.stream` P0 media stream 采纳准备：统一 source、syncGroupId、clockDomain、profile 和 STREAM conformance。
5. `device.info` / `system.lifecycle` / `system.reset` / `system.state`：作为设备管理底座进入下一批。
6. `display/signage` 与 `room`：按产品场景排序，不先污染底层音视频模块。
7. legacy-only 清理：把授权、产测、vendor、写默认 profile 等条目从通用业务模块里剥离。

## 14. 后续审计批次计划

| 批次 | 重点文件 | 主要边界问题 | 预期产物 |
|---|---|---|---|
| device/system | `workspace/business/device-system-info.md`; `workspace/flows/device-system-info.md`; `workspace/protocol/device/*.md`; `workspace/protocol/system/*.md`; launcher/signage legacy evidence | `device.info` 与 system runtime state；default settings vs factory settings；shutdown/reboot schedules；stateChanged event 粒度。 | device/system module audit；registry candidate list；reset/lifecycle conformance outline。 |
| display/signage | `workspace/flows/signage-device-management.md`; `workspace/protocol/display/*.md`; `workspace/protocol/signage/*.md`; signage migration plan/evidence | display input/output/power 与 signage playback/schedule/media 的边界；OSD、playlist、media storage 是否跨 domain。 | display/signage boundary matrix；first signage candidates；adapter-only list。 |
| video/camera/stream | `workspace/flows/device-streaming-audio-video.md`; `workspace/protocol/video/*.md`; `workspace/protocol/camera/*.md`; `workspace/protocol/stream/*.md` | video.stream vs stream.flowControl；camera control vs video framing/layout；wireless_cast source proxy；A/V sync。 | media stream adoption plan；STREAM conformance outline；camera/video split recommendations。 |
| room | `workspace/protocol/room/*.md`; Rooms evidence/classification; rooms migration plan | room.source vs output.layout/video.layout/signage; participant/schedule/layout 的应用层边界。 | room module audit；room.source adoption readiness；Rooms adapter mapping gaps。 |
| legacy-only | `workspace/legacy-migration/classification/**`; generated migration maps; vendor classification | 哪些 legacy command 只适合 adapter，不应进入 formal registry；vendor/private ranges；旧状态码映射。 | adapter-only registry notes；legacy cleanup backlog；do-not-adopt list。 |

## 15. 本轮未做的事情

- 未修改 `contract/registry/**`。
- 未修改 `protocol/**`。
- 未修改 `contract/generated/**`。
- 未修改 `docs/conformance/**`。
- 未修改 `specs/**`。
- 未重写任何 `workspace/protocol/**` 草案。
- 未删除 legacy evidence。
- 未把 appendix candidate 表当成正式 registry。
