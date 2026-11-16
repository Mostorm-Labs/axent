# AXTP audio 草案重写计划

> Date: 2026-06-10
> Phase: 文档治理第六阶段 B，audio 草案重写计划
> Scope: `workspace/protocol/audio/**` 草案重写计划，不直接重写草案正文
> Authority: `contract/registry/domains/audio/domain.yaml`、`contract/protocol/axtp.protocol.yaml`、`contract/generated/**` 仍是 runtime contract；本文件只是审计和重写计划。

## 0. 输入说明

本计划应基于 `docs/archive/audits/audio-domain-consolidation.md` 的归并结论生成。但当前工作区没有该文件，因此本文以 `docs/archive/audits/business-module-audit.md` 中的 audio 审计结论、`workspace/protocol/audio/**`、`contract/registry/domains/audio/domain.yaml`、`contract/generated/**`、`docs/conformance/**` 和 `workspace/legacy-migration/classification/audio.md` 作为实际输入。

这意味着本文把缺失的 `audio-domain-consolidation.md` 视为一个治理缺口：后续如果补回该文件，应复核本计划中关于 `audio.source`、`audio.lineIn`、`audio.backgroundMusic` 的合并判断。

## 1. Audio 最终 feature 决策表

| feature | decision | reason | target_doc | registry_candidate | conformance_needed | legacy_notes |
|---|---|---|---|---|---|---|
| `audio.algorithm` | keep | 已由 `contract/registry/domains/audio/domain.yaml` 采纳，并已生成 4 个 method、1 个 event 和 capability。重写目标是把 protocol 草案收敛为 adopted reference/amendment 输入。 | `workspace/protocol/audio/audio.algorithm.md` | no, already adopted | yes, 补 schema strictness、event 完整 payload、invalid selector、unsupported algorithm。 | 授权、AI 线程暂停/恢复、beam/DOA 结果仍应标为待确认或 adapter-only，不能污染已采纳配置语义。 |
| `audio.eq` | keep | EQ 有独立 path/preset/band/gain/frequency/Q 模型，和算法增强配置不同；legacy evidence 高，草案成熟。 | `workspace/protocol/audio/audio.eq.md` | yes | yes, 需要 happy path、range error、invalid band/preset、capability discovery、event case。 | record EQ、audio EQ、default EQ 可映射到 EQ config/reset，但写默认值语义需和 reset/default 区分。 |
| `audio.volume` | split | volume 草案当前把音量、静音、增益状态混在一起；正式语义应只覆盖用户可感知的 output volume/mute/default volume。 | `workspace/protocol/audio/audio.volume.md` | yes, after split | yes, 需要 set/get/reset、mute、range、unknown target、capability discovery、event case。 | Line-out volume、mute、default volume 归本 feature；Line-in preGain 归 `audio.input`；mixer gain 归 `audio.mixer`。 |
| `audio.input` | keep | 输入端口、Mic/Line-in/UAC input 选择、preGain、detect state 有稳定业务边界，但需要从 `audio.source`、`audio.lineIn` 中吸收 schema 概念。 | `workspace/protocol/audio/audio.input.md` | yes, after volume boundary fixed | yes, 需要 input inventory、set lineIn preGain、detect event、unsupported input、capability discovery。 | `CommonSetMicUsed`、`SetLineInPreGain`、`GetLineInPreGain`、`AudioInMode`、`LineInMode` 映射到本 feature。 |
| `audio.output` | unresolved | output 可能是独立 output port/sink inventory，也可能只是 `audio.volume` target 或 `audio.routing` sink；当前草案仍是模板级。 | `workspace/protocol/audio/audio.output.md` | no | later | 只有存在 output port mode/sink state 独立业务需求时保留；Line-out volume 不应让 output 成为一等 feature。 |
| `audio.mixer` | keep | mixer 负责多路混音、bus、matrix、per-input/per-bus gain/mute，和 output volume 不是同一控制面。 | `workspace/protocol/audio/audio.mixer.md` | later | yes, after schema exists | `GetAudioMixerItem`、`SetAudioMixerItem`、`AudioMix` 映射到 mixer；不要把这些 gain 合并进 `audio.volume`。 |
| `audio.routing` | keep | routing 负责 source/input/processing/output 之间的连接关系和 active route，不负责端口 inventory 或音量。 | `workspace/protocol/audio/audio.routing.md` | later | yes, after source/input/output schema fixed | legacy 证据需要二次筛选；source 选择类旧命令可能只是 `audio.input` 或 `audio.playback`，不是 routing。 |
| `audio.source` | merge | source 是多个 feature 的 schema 概念，不宜先作为 capability；可作为 source descriptor 被 input/routing/stream/playback 引用。 | no standalone doc; merge into `audio.input.md`, `audio.routing.md`, `audio.stream.md`, `audio.playback.md` | no | no standalone cases | 旧音源选择要按语义映射：物理输入归 input，连接关系归 routing，媒体任务归 playback/stream。 |
| `audio.lineIn` | merge | Line-in 是 input source/port 类型，不应建成独立 feature。 | no standalone doc; merge into `audio.input.md` | no | covered by `audio.input` cases | `SetLineInPreGain`、`GetLineInPreGain`、`LineInMode` 映射到 `audio.input` 的 `target=lineIn` 或等价 schema。 |
| `audio.backgroundMusic` | adapter-only | 背景音乐更像设备侧播放任务或 signage 内容编排，不是底层 audio primitive。 | no standalone doc; adapter to `audio.playback.md` or signage playback docs | no | covered after playback/signage decision | 旧背景音乐命令不要直接创造 `audio.backgroundMusic` capability；先作为 adapter alias。 |
| `audio.playback` | keep | 设备侧音频播放任务可以承接 background music 的任务语义，但当前草案是模板级，需要与 signage playback 划边界。 | `workspace/protocol/audio/audio.playback.md` | later | yes, after scope confirmed | `SetAudioPlaybackMode` / `GetAudioPlaybackMode` 可映射，但若是内容编排，应转 signage。 |
| `audio.recording` | hold | 草案较完整，但依赖 file/stream 数据面边界；本轮不是第一批 audio config rewrite。 | `workspace/protocol/audio/audio.recording.md` | later | yes, stream/file cases needed | 工厂抓音、产测录音可能由 diagnostic 调用，不能全部变成通用 recording。 |
| `audio.stream` | hold | 实时音频媒体流依赖 STREAM profile 和 A/V sync；本轮不和配置型 feature 混改。 | `workspace/protocol/audio/audio.stream.md` | later | yes, stream open/data/close and profile cases | 旧 `stream.hidMedia` 类映射要避免修改公共 STREAM header。 |
| `audio.uac` | hold | UAC service 配置可保留，但需确认 host/device role、state/config、与 input/output 的关系。 | `workspace/protocol/audio/audio.uac.md` | later | yes, after role model | UAC state 旧命令可映射，但不要把 USB transport 细节建模成业务 feature。 |
| `audio.dante` | unresolved | Dante 授权、厂商信息、网络音频配置可能横跨 audio/vendor/license/network；当前不适合作为第一批。 | `workspace/protocol/audio/audio.dante.md` | no | later | Dante license/manufacturer 可能应进入 vendor/license 或 adapter metadata。 |

## 2. Audio 文档重写计划表

| file | current_role | action | target_feature | rewrite_type | priority | notes |
|---|---|---|---|---|---|---|
| `workspace/protocol/audio/audio.algorithm.md` | adopted protocol explanation plus long draft detail | rewrite | `audio.algorithm` | adopted-reference cleanup | P0 | 保留已采纳语义；不改 method/event/schema；把 legacy 待确认项收敛到 Non-goals/Legacy Mapping。 |
| `workspace/protocol/audio/audio.eq.md` | rich review draft | rewrite | `audio.eq` | registry-candidate draft | P0 | 按统一模板重排；保留现有模型，移除过长示例或降为 Non-normative。 |
| `workspace/protocol/audio/audio.volume.md` | template draft | rewrite | `audio.volume` | boundary split + candidate draft | P0 | 只保留 output volume/mute/default；明确 input preGain/mixer gain 不属于本 feature。 |
| `workspace/protocol/audio/audio.input.md` | template draft | rewrite | `audio.input` | merge source/lineIn concepts | P1 | 吸收 `audio.lineIn` 和物理 input source；补 input inventory、preGain、mode、detect state。 |
| `workspace/protocol/audio/audio.routing.md` | template draft | rewrite | `audio.routing` | dependency draft | P1 | 等 input/output/source descriptor 决定后再写 route graph、route state、active route。 |
| `workspace/protocol/audio/audio.mixer.md` | template draft | rewrite | `audio.mixer` | second-batch candidate draft | P1 | 明确 bus/mix item/matrix/gain/mute 与 volume 的边界。 |
| `workspace/protocol/audio/audio.output.md` | template draft | hold | `audio.output` | unresolved boundary | P2 | 先判断 output 是独立端口能力，还是 `routing` sink / `volume` target。 |
| no `workspace/protocol/audio/audio.source.md` | missing concept doc | merge-into | source descriptor | schema concept only | P1 | 不新增独立文档；作为 input/routing/stream/playback 的 schema 概念。 |
| no `workspace/protocol/audio/audio.lineIn.md` | missing legacy concept doc | merge-into | `audio.input` | adapter alias | P1 | 不新增独立文档；用 input target/sourceType 表达。 |
| no `workspace/protocol/audio/audio.backgroundMusic.md` | missing product concept doc | adapter-only | `audio.playback` or signage playback | product alias | P2 | 不新增独立文档；先在 playback/signage 中记录 legacy alias。 |
| `workspace/protocol/audio/audio.playback.md` | template draft | rewrite | `audio.playback` | second-batch task lifecycle | P2 | 只在确认 background music 是否设备侧播放任务后重写。 |
| `workspace/protocol/audio/audio.recording.md` | rich review draft | hold | `audio.recording` | stream/file dependent | P2 | 后续与 file/stream/diagnostic 一起审。 |
| `workspace/protocol/audio/audio.stream.md` | rich stream draft | hold | `audio.stream` | stream profile dependent | P2 | 后续与 video.stream、stream.flowControl 一起审。 |
| `workspace/protocol/audio/audio.uac.md` | template draft | hold | `audio.uac` | role/service draft | P3 | 需先确认 UAC 是 audio service 还是 input/output profile。 |
| `workspace/protocol/audio/audio.dante.md` | template draft | hold | `audio.dante` | vendor/license boundary | P3 | 需先确认 license/manufacturer 是否应进入 vendor/license。 |

## 3. 第一批重写范围

第一批只建议重写：

1. `workspace/protocol/audio/audio.algorithm.md`
2. `workspace/protocol/audio/audio.eq.md`
3. `workspace/protocol/audio/audio.volume.md`

优先原因：

| file | why first | expected output |
|---|---|---|
| `audio.algorithm.md` | 唯一已进入 contract/registry/generated 的 audio feature，是后续草案的模板和事实边界。 | adopted reference 版草案，明确“已采纳事实以 generated 为准”，legacy 扩展只保留为待确认。 |
| `audio.eq.md` | 边界清楚，和 `audio.algorithm` 的关系已经有较完整结论，legacy evidence 高。 | registry candidate 版草案，补齐 capability/method/event/schema/error/conformance 结构。 |
| `audio.volume.md` | legacy evidence 高，也是 input/mixer/output 边界的核心冲突点。先拆清 volume，后续 input/mixer 才不会被污染。 | 边界收敛后的 candidate 草案，明确 output volume/mute/default 与 input preGain/mixer gain 的分工。 |

第一批不应修改 contract/registry/generated/conformance，只产出可评审的 protocol draft rewrite。

## 4. 第二批重写范围

第二批包括 `audio.source`、`audio.routing`、`audio.mixer`、`audio.output`、`audio.input`、`audio.lineIn`、`audio.backgroundMusic`。其中 `audio.source`、`audio.lineIn`、`audio.backgroundMusic` 当前没有独立 `workspace/protocol/audio/*.md` 文件，应作为合并/adapter 决策写入目标文档，而不是补建一等 feature 文档。

| item | target file | dependency | risk |
|---|---|---|---|
| `audio.input` | `workspace/protocol/audio/audio.input.md` | 依赖 `audio.volume` 已排除 lineIn preGain；依赖 source descriptor 命名。 | 可能把 input inventory、source selection、detect event 和 gain 全塞进一个过大 schema。 |
| `audio.lineIn` | `workspace/protocol/audio/audio.input.md` | 依赖 `audio.input` target/sourceType 设计。 | 如果建独立 feature，会和 input/source/routing 重复。 |
| `audio.source` | `audio.input.md`、`audio.routing.md`、`audio.stream.md`、`audio.playback.md` | 依赖每个业务 feature 明确自己使用 source descriptor 的位置。 | 如果作为 capability，会变成字段级概念，违反 `domain.feature` 粒度规则。 |
| `audio.routing` | `workspace/protocol/audio/audio.routing.md` | 依赖 input/output/source descriptor；依赖 mixer 边界。 | 容易把 source inventory、output port 和 mixer gain 都吞进去。 |
| `audio.mixer` | `workspace/protocol/audio/audio.mixer.md` | 依赖 volume 已剥离 mixer gain；依赖 routing graph 是否引用 mixer node。 | mixer gain 与 user volume 混淆；matrix/bus 模型测试复杂。 |
| `audio.output` | `workspace/protocol/audio/audio.output.md` | 依赖 routing sink 和 volume target 判断。 | 业务价值不足时会成为 `audio.volume` / `audio.routing` 的重复壳。 |
| `audio.backgroundMusic` | `workspace/protocol/audio/audio.playback.md` or signage playback docs | 依赖判断其是设备侧播放任务还是 signage 内容编排。 | 若直接建 `audio.backgroundMusic`，会把产品场景名当 feature。 |

## 5. 统一 audio protocol draft 模板

后续重写每个 `workspace/protocol/audio/*.md` MUST 使用以下结构：

```markdown
---
domain: audio
feature: audio.<feature>
status: draft | review-ok | adopted | deprecated | adapter-only
contract: false
source:
  - docs/archive/audits/audio-rewrite-plan.md
  - workspace/legacy-migration/classification/audio.md
---

# AXTP audio.<feature> 协议草案

## Purpose

## Scope

## Lifecycle Status

## Capability Model

## Methods

## Events

## Schemas

## Errors

## Stream Usage

## Legacy Mapping

## Registry Readiness

## Conformance Notes

## Non-goals
```

模板规则：

- `contract: false` 是默认值。只有已由 contract/registry/generated 支撑的 adopted reference 可以说明 generated 是实现合同。
- Methods、Events、Schemas、Errors 可以列候选名称和候选结构，但不得新增 methodId、eventId、errorCode 或 fieldId。
- Stream Usage 必须说明是否使用 STREAM；配置型 feature 应明确“不使用 STREAM”。
- Legacy Mapping 必须区分 adopted mapping、candidate mapping、adapter-only 和 unresolved。
- Registry Readiness 必须引用准入条件，不能把 protocol draft 直接等同于 registry contract。

## 6. Registry candidate 规则

只有满足以下条件的 audio feature 才能进入 registry candidate：

- 有清楚的 `domain.feature` 边界，且 feature 是可评审、可测试、可演进的能力块。
- method、event、schema、capability 齐全，且命名符合 `specs/30-registry.md`。
- params、result、event payload 可以 schema 化，并能后续分配稳定 fieldId。
- error model 清楚，能复用正式 ErrorCode，不发明局部错误语义。
- legacy mapping 不污染正式语义；旧命令名、旧字段名、旧状态码只作为 mapping 或 adapter metadata。
- 能由 registry YAML 生成 Protocol IR 和 `contract/generated/**`，不需要手写 generated artifacts。
- conformance 至少能写 happy path、error path、boundary case、capability discovery case。

当前建议：

| feature | registry candidate readiness |
|---|---|
| `audio.algorithm` | 已 adopted；后续走 amendment，不再作为普通 candidate。 |
| `audio.eq` | 第一批 candidate，重写后可进入 registry review。 |
| `audio.volume` | 第一批 candidate，但必须先完成 output volume/input preGain/mixer gain 拆分。 |
| `audio.input` | 第二批 candidate，依赖 lineIn/source 合并模型。 |
| `audio.mixer` | 第二批之后 candidate，依赖 volume/routing 边界。 |
| `audio.routing` | 第二批之后 candidate，依赖 input/output/source descriptor。 |
| `audio.output` | 暂不进入 candidate，先确认是否独立存在。 |
| `audio.playback` | 暂不进入 candidate，先确认 background music 与 signage playback 边界。 |

## 7. 不要做的事情

- 不要修改 `contract/registry/**`。
- 不要修改 `protocol/**`。
- 不要修改 `contract/generated/**`。
- 不要修改 `docs/conformance/**`。
- 不要修改 `workspace/protocol/audio/**` 正文，直到进入对应重写批次。
- 不要修改 `specs/**`。
- 不要修改 `README.md`。
- 不要新增 methodId、eventId、errorCode 或 fieldId。
- 不要删除 legacy 证据。
- 不要把 `workspace/registry-planning/candidates/**` candidate 表当成正式 registry。
- 不要把 `workspace/protocol/audio/**` 草案标成 `contract: true`，除非它已经明确由 contract/registry/generated 支撑；即便是 `audio.algorithm`，也应表述为“实现合同在 contract/registry/generated，本文是 adopted reference 或 amendment input”。

## 8. audio-domain-consolidation 未解决问题

由于当前工作区缺少 `docs/archive/audits/audio-domain-consolidation.md`，本文无法逐条复核该文件中的未解决问题。基于现有审计材料，仍需在后续补齐或确认：

| issue | status | impact |
|---|---|---|
| `audio.source` 是否需要独立 feature | unresolved but leaning merge | 建议不建独立 feature；作为 source descriptor 被 input/routing/stream/playback 复用。 |
| `audio.lineIn` 是否需要独立 feature | resolved in this plan: merge | 建议并入 `audio.input`。 |
| `audio.backgroundMusic` 是否保留在 audio 域 | unresolved | 如果是设备侧播放任务，进入 `audio.playback`；如果是内容编排，进入 signage playback/media；作为旧协议别名先 adapter-only。 |
| `audio.output` 是否独立 | unresolved | 需产品场景证明 output port/sink mode 独立于 volume/routing。 |
| `audio.dante` license/manufacturer 边界 | unresolved | 可能属于 vendor/license；暂不进入重写第一、二批。 |
| conformance coverage | open | 当前只覆盖 `audio.algorithm` 的通用 capability/event 框架，业务 feature 专项 case 仍需后续补。 |

## 9. 后续执行顺序

1. 重写 `audio.algorithm.md`，把它整理成 adopted reference/amendment 输入模板。
2. 重写 `audio.eq.md`，产出第一批 registry candidate。
3. 重写 `audio.volume.md`，完成 output volume/mute/default 与 input/mixer gain 的边界拆分。
4. 重写 `audio.input.md`，吸收 `audio.lineIn` 和 source descriptor。
5. 重写 `audio.routing.md`，建立 route graph 和 active route。
6. 重写 `audio.mixer.md`，建立 mixer bus/item/matrix/gain 模型。
7. 复核 `audio.output.md` 是否保留；如果无独立业务，降级为 routing sink / volume target 说明。
8. 复核 `audio.playback.md` 与 background music/signage playback 的归属。
9. 单独批次处理 `audio.stream.md`、`audio.recording.md`、`audio.uac.md`、`audio.dante.md`。
