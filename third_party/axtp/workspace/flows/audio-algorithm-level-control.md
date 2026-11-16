# Audio Algorithm Level Control Protocol Interaction Flow

> Status: flow design
> Scope: App UI for adjusting audio algorithm strength levels
> Source inputs: user request for an audio algorithm tuning UI with level sliders and a restore-default button
> Protocol lifecycle: Stage 10 `plan-protocol-flow`

本文根据“UI 界面上能够调控音频算法强度等级，具备各个调试等级的拖动条，以及恢复默认按钮”的业务场景，梳理 App、设备和 AXTP 协议之间的交互流程。

本文不是最终协议事实源。当前可实现协议以 `contract/generated/protocol.md`、`contract/protocol/axtp.protocol.yaml` 和 `contract/registry/domains/audio/domain.yaml` 为准；如后续 UI 需要当前 `audio.algorithm` 未覆盖的字段或行为，应转入 `amend-adopted-protocol`。

Flow 文档负责描述业务场景和交互步骤、判断每一步协议覆盖状态、识别协议缺口，并将缺口路由到 candidate `domain.feature`。Flow 文档不负责定义完整 method / event / schema / capability，不分配 methodId / eventId / errorCode / fieldId，也不能替代 `workspace/protocol/<domain>/<feature>.md`。

## 0. 速读结论

| 项目 | 内容 |
|---|---|
| Flow 目标 | App 根据设备声明能力渲染音频算法强度滑条，支持调整等级和恢复默认。 |
| 当前协议覆盖 | complete |
| 涉及 domain.feature | `audio.algorithm` |
| 已有 adopted/generated | `audio.getAlgorithmCapabilities`, `audio.getAlgorithmConfig`, `audio.setAlgorithmConfig`, `audio.resetAlgorithmConfig`, `audio.algorithmConfigChanged` |
| 缺口 | 当前“等级滑条 + 恢复默认”场景无新增协议缺口；若 UI 需要新算法对象或实时算法结果，应走 amendment 或新草案。 |
| 是否需要新增协议草案 | no |
| 是否涉及 Legacy | no |
| 是否涉及 STREAM | no |
| 下一步 | no action；如新增算法字段，走 `amend-adopted-protocol`。 |

## 1. Story Summary

| Item | Content |
|---|---|
| User goal | 用户在 App 的音频算法调试页中，通过拖动条调整设备音频算法强度等级，并可一键恢复默认值。 |
| Trigger | 用户进入音频算法调试页面。 |
| Success result | UI 展示设备支持的算法等级项、当前值、范围和默认值；用户调整后设备生效；恢复默认后 UI 和设备配置回到默认等级。 |
| Primary actors | User, App, Device AXTP server, audio algorithm service |
| Product scope | `[REVIEW-ASK]` 具体产品型号、固件版本和 UI 原型图未提供；本方案按通用 `audio.algorithm` 能力设计。 |

## 2. Source Observations

### 2.1 UI / Prototype

| Screen or control | Observed behavior | Protocol relevance |
|---|---|---|
| Audio algorithm tuning page | 展示多个音频算法强度项。 | 需要查询 `audio.getAlgorithmCapabilities` 和 `audio.getAlgorithmConfig`。 |
| Level sliders | 每个可调等级项有拖动条；范围、步进和默认值应由设备能力决定。 | 使用 capability 中的 property descriptor 渲染 slider。 |
| Restore default button | 用户点击后恢复默认等级。 | 使用 `audio.resetAlgorithmConfig`。 |
| Save/apply behavior | `[REVIEW-DRAFT]` 默认按“松手或防抖后立即提交”设计；若产品需要显式保存按钮，可把多个 slider 变化合并提交。 | 使用 `audio.setAlgorithmConfig` 的 partial update。 |
| UI prototype image | `[REVIEW-ASK]` 本轮没有可读取的图片文件；控件名称、分组、文案和是否有 enable 开关需 UI 原型确认。 | 不新增协议，只影响 UI 呈现和字段选择。 |

### 2.2 Requirement Notes

- UI 不应写死算法列表、范围、步进或默认值；必须以 `audio.getAlgorithmCapabilities` 返回为准。
- UI 只展示设备声明 `supported=true` 且存在 numeric property descriptor 的字段。
- “等级拖动条”优先覆盖 `level` / `nlpLevel` / `sensitivity` 这类强度字段。
- 时间、角度、电平等高级参数不默认放入本页，除非产品明确要求。
- 恢复默认按钮推荐只恢复本页 slider 字段，不默认重置 `enabled`、`reportingEnabled` 或其他非等级字段。

### 2.3 Device / System State Observations

| State | Meaning | Protocol relevance |
|---|---|---|
| session ready | AXTP RPC session 已建立。 | precondition；App 才能调用 `audio.*` methods。 |
| capability loaded | 设备已返回算法、字段、范围、默认值和更新策略。 | query；决定 UI 是否展示 slider。 |
| config loaded | 当前算法配置已读取。 | query；用于初始化当前值。 |
| dirty local edit | 用户拖动 slider，但尚未提交或防抖未完成。 | local-only；不产生协议调用。 |
| applying | App 正在提交 set/reset。 | request/response；按钮可进入 pending 状态。 |
| pending_restart | 配置已接受，但需要音频链路重启或稍后生效。 | result / event；UI 展示提示。 |
| config changed | 本端 set/reset、其他端设置或设备策略导致配置变化。 | event；`audio.algorithmConfigChanged`。 |

## 3. Assumptions And Non-Goals

| Type | Item | Status |
|---|---|---|
| Assumption | 拖动条调整的是算法强度等级，而不是算法开关、音频路由、音量或 EQ。 | `[REVIEW-DRAFT]` |
| Assumption | App 连接设备后可完成 AXTP session，并已加载当前产品生成的 AXTP registry。 | `[REVIEW-DRAFT]` |
| Assumption | UI 变更提交采用松手提交或 300-500 ms 防抖提交，避免拖动过程高频 RPC。 | `[REVIEW-DRAFT]` |
| Question | 恢复默认是恢复本页所有 slider 字段，还是恢复 `audio.algorithm` 的全部字段，包括 enabled 和高级参数？ | `[REVIEW-ASK]` |
| Question | UI 是否需要显示算法开关、当前算法运行状态、DOA/beam 实时结果或试听预览？ | `[REVIEW-ASK]` |
| Non-goal | 不新增音频算法协议方法。当前场景由已生成的 `audio.algorithm` 方法覆盖。 | `[REVIEW-OK]` |
| Non-goal | 不把 UI 控件布局、文案、主题或本地校验样式写入协议。 | `[REVIEW-OK]` |

## 4. Protocol Coverage

| Need | Coverage state | AXTP protocol | Evidence | Next action |
|---|---|---|---|---|
| 确认设备是否支持音频算法配置 | generated | `audio.getAlgorithmCapabilities`, `audio.algorithm` capability | `contract/generated/protocol.md`, `contract/registry/domains/audio/domain.yaml` | App 实现能力门禁。 |
| 查询可展示的算法和 slider 范围 | generated | `audio.getAlgorithmCapabilities` | `contract/generated/protocol.md` | App 按返回 descriptor 渲染 UI。 |
| 查询当前等级值 | generated | `audio.getAlgorithmConfig` | `contract/generated/protocol.md` | App 初始化页面。 |
| 调整单个或多个等级值 | generated | `audio.setAlgorithmConfig` | `contract/generated/protocol.md` | App 在松手、防抖或保存时提交 partial config。 |
| 恢复默认等级 | generated | `audio.resetAlgorithmConfig` | `contract/generated/protocol.md` | App 传入本页 slider 字段 selector。 |
| 多端或设备策略改变配置后同步 UI | generated | `audio.algorithmConfigChanged` | `contract/generated/protocol.md` | App 处理 event 并刷新展示。 |
| UI 需要新增算法对象或新字段 | adopted | `audio.algorithm` schema extension | `contract/registry/domains/audio/domain.yaml`, `workspace/protocol/audio/audio.algorithm.md` | 转 `amend-adopted-protocol`。 |
| UI 需要实时算法结果，如 DOA / beam info | missing | Candidate domain TBD, likely not `audio.algorithm` config | none | 转 `draft-business-protocol`，不要塞进 config changed event。 |

Coverage 取值：

| Coverage | Meaning |
|---|---|
| generated | 已进入 `contract/generated/**` 或 protocol IR，可作为实现合同视图。 |
| adopted | 已写入 registry YAML，但当前 flow 未直接引用 generated 输出。 |
| draft | 已有 `workspace/protocol/**` 草案，但尚未 adopted/generated。 |
| missing | 没有合适的 adopted/generated/draft 协议覆盖。 |
| local-only | App/UI/runtime 本地逻辑，不需要 AXTP 协议。 |
| non-protocol | 产品规则、人工流程、运营策略或文档说明，不进入协议。 |

## 5. End-To-End Sequence

```mermaid
sequenceDiagram
    participant User
    participant App
    participant Device as Device AXTP Server
    participant Audio as Audio Algorithm Service

    User->>App: Open audio algorithm tuning page
    App->>App: Load generated AXTP registry
    App->>Device: audio.getAlgorithmCapabilities
    Device-->>App: algorithms, fields, min/max/step/default
    App->>Device: audio.getAlgorithmConfig
    Device-->>App: current algorithm config
    App-->>User: Render supported sliders with current values

    User->>App: Drag slider and release
    App->>Device: audio.setAlgorithmConfig(partial config)
    Device->>Audio: Validate and apply atomically
    Audio-->>Device: effective config and apply state
    Device-->>App: applyState, requiresAudioRestart, config
    Device-->>App: audio.algorithmConfigChanged
    App-->>User: Update slider state and restart hint if needed

    User->>App: Tap restore default
    App->>Device: audio.resetAlgorithmConfig(field selector)
    Device->>Audio: Reset selected fields to default
    Audio-->>Device: effective default config
    Device-->>App: applyState, requiresAudioRestart, config
    Device-->>App: audio.algorithmConfigChanged
    App-->>User: Show restored values
```

## 6. Interaction Steps

| Step | Actor | Action | Capability / precondition | Protocol call/event | Payload fields | Result / state change | Coverage | Error / fallback |
|---:|---|---|---|---|---|---|---|---|
| 1 | App | 进入页面后检查当前生成协议是否包含音频算法方法。 | App 已加载产品 generated registry。 | Generated registry lookup | method/event names | 若包含相关方法，继续运行时查询。 | generated | 当前 App 包不含这些方法时隐藏页面或提示版本不支持；lookup 本身是本地行为。 |
| 2 | App | 加载算法参数能力。 | `audio.algorithm` capability supported。 | `audio.getAlgorithmCapabilities` | `items` 可省略查询全部，也可传产品关心的算法对象数组。 | 返回支持算法、字段、默认值、范围、步进、单位和是否需要重启。 | generated | `NOT_SUPPORTED` / `INVALID_ARGUMENT` 时页面不可用或降级。 |
| 3 | App | 加载当前配置。 | capability query 成功。 | `audio.getAlgorithmConfig` | `items` 与 capability 查询保持一致。 | 返回当前有效配置。 | generated | 读取失败时保留页面空态并允许重试。 |
| 4 | App | 渲染 sliders。 | capability + config 已加载。 | local-only | 只渲染 numeric descriptor 字段。 | UI slider 值来自当前 config；默认标记来自 capabilities。 | local-only | 不支持或缺少 descriptor 的字段不展示。 |
| 5 | User / App | 用户拖动等级 slider。 | 本地字段范围已知。 | local-only | 拖动过程本地更新预览值。 | UI 展示待提交状态。 | local-only | 若产品需要实时生效，使用防抖提交，不逐像素发送 RPC。 |
| 6 | App | 松手、防抖结束或点击保存。 | 提交字段通过本地 range/step 校验。 | `audio.setAlgorithmConfig` | changed config fields，例如 `noiseSuppression.level`。 | 返回 `applyState`、`requiresAudioRestart` 和最终有效 config。 | generated | `OUT_OF_RANGE` 回滚到设备返回值；`BUSY` 稍后重试；`PERMISSION_DENIED` 提示无权限。 |
| 7 | Device | 配置变化通知。 | App 已订阅或默认接收事件。 | `audio.algorithmConfigChanged` | `reason`, `changedFields`, config summary | App 用 event 校正 UI，支持多端同步。 | generated | 若未收到 event，仍以 set response 更新当前页面；后台同步可下次读取。 |
| 8 | User / App | 用户点击恢复默认。 | capability 已返回默认值；产品确认 reset 范围。 | `audio.resetAlgorithmConfig` | 推荐传本页 slider 字段 map；若产品确认重置全部，可传 `all`。 | 返回默认后的有效 config，并触发 changed event。 | generated | selector 无效返回 `INVALID_ARGUMENT`，App 应重新拉 capabilities。 |

## 7. State Changes And Events

| State change | Trigger | Event needed | Payload | Client handling | Coverage |
|---|---|---|---|---|---|
| algorithm capability loaded | 页面加载或设备切换 | no event | capability response | 渲染 slider 范围和默认值。 | generated |
| algorithm config loaded | 页面加载 / 刷新 / event 后校准 | no event | config response | 设置 UI 当前值。 | generated |
| local dirty value | 用户拖动 slider | no event | local field path/value | 标记待提交或 debounce。 | local-only |
| config applied | `audio.setAlgorithmConfig` 成功 | `audio.algorithmConfigChanged` | changedFields, reason, effective config | 以 response 立即更新 UI，以 event 校准多端状态。 | generated |
| config reset to default | `audio.resetAlgorithmConfig` 成功 | `audio.algorithmConfigChanged` | reset fields, effective config | 恢复 UI 值并清除 dirty 标记。 | generated |
| pending restart | set/reset response 指示需要重启音频链路 | optional same event / response field | applyState, requiresAudioRestart | 显示稍后生效或需要重启提示。 | generated |
| unsupported algorithm field | capability 未声明或设备返回 `NOT_SUPPORTED` | no event | error response | 隐藏字段或提示固件不支持。 | generated |

## 8. Protocol Details

### 8.1 Adopted / Generated Protocols

| Method/Event | Purpose in this flow | Source |
|---|---|---|
| `audio.getAlgorithmCapabilities` | 发现支持算法、字段、范围、步进、默认值和更新策略。 | `contract/generated/protocol.md` |
| `audio.getAlgorithmConfig` | 读取当前算法等级值。 | `contract/generated/protocol.md` |
| `audio.setAlgorithmConfig` | 提交拖动条变化，支持 partial config 和原子更新。 | `contract/generated/protocol.md` |
| `audio.resetAlgorithmConfig` | 将选定算法或选定字段恢复到 capability 声明的默认值。 | `contract/generated/protocol.md` |
| `audio.algorithmConfigChanged` | 配置变化后通知 App，支持多端同步和设备策略变化。 | `contract/generated/protocol.md` |

### 8.2 Draft Or Missing Protocol Gaps

| Gap | Candidate domain.feature | Candidate method/event/schema | Routed skill | Review question |
|---|---|---|---|---|
| UI 仅调整已有等级字段 | none | none | none | 当前 generated/YAML 已覆盖。 |
| UI 需要新增算法对象，例如新的 AI 降噪算法 | `audio.algorithm` | Add schema fields / capabilities | `amend-adopted-protocol` | `[REVIEW-ASK]` 新算法名称、字段、范围、默认值和是否需要重启。 |
| UI 需要展示 DOA/beam 实时结果 | TBD, likely `audio.beam` | New event such as beam info reported | `draft-business-protocol` | `[REVIEW-ASK]` 这是算法结果上报，不应塞进 `audio.algorithmConfigChanged`。 |
| UI 需要算法授权、license 或产测参数 | TBD | New or amended protocol | `draft-business-protocol` | `[REVIEW-ASK]` 真实业务归属需确认。 |

### 8.3 Suggested Slider Fields

UI 应从 capabilities 动态生成；下面只是当前 generated schema 中适合“强度等级”页面的候选字段：

| UI label candidate | Config path | Type/range | Notes |
|---|---|---|---|
| Noise Suppression Level | `noiseSuppression.level` | `uint8`, `0..3` | 噪声抑制强度。 |
| Echo Cancellation Level | `echoCancellation.nlpLevel` | `uint8`, `0..3` | 非线性处理强度；UI 文案可显示为回声消除强度。 |
| Dereverberation Level | `dereverberation.level` | `uint8`, `0..3` | 去混响强度。 |
| Howling Suppression Level | `howlingSuppression.level` | `uint8`, `0..3` | 啸叫抑制强度。 |
| Voice Activity Sensitivity | `voiceActivityDetection.sensitivity` | `uint8`, `0..3` | 不是 `level` 字段，但可作为“检测灵敏度” slider。 |

高级 numeric 字段如 `echoCancellation.tailLengthMs`、`beamforming.lookDirectionDeg`、`beamforming.beamWidthDeg`、`autoGainControl.targetLevelDb` 和 `autoGainControl.maxGainDb` 不默认归入“强度等级”页面；若 UI 原型要求展示，应按 capabilities 渲染为高级参数区，而不是新增协议。

### 8.4 Payload Examples

单个 slider 松手后提交：

```json
{
  "method": "audio.setAlgorithmConfig",
  "params": {
    "config": {
      "noiseSuppression": {
        "level": 3
      }
    }
  }
}
```

多个 slider 批量保存时提交：

```json
{
  "method": "audio.setAlgorithmConfig",
  "params": {
    "config": {
      "noiseSuppression": {
        "level": 2
      },
      "echoCancellation": {
        "nlpLevel": 3
      },
      "dereverberation": {
        "level": 1
      }
    }
  }
}
```

推荐只恢复本页 slider 字段：

```json
{
  "method": "audio.resetAlgorithmConfig",
  "params": {
    "items": {
      "noiseSuppression": ["level"],
      "echoCancellation": ["nlpLevel"],
      "dereverberation": ["level"],
      "howlingSuppression": ["level"],
      "voiceActivityDetection": ["sensitivity"]
    }
  }
}
```

如果产品确认“恢复默认”应重置全部算法字段，可以改为：

```json
{
  "method": "audio.resetAlgorithmConfig",
  "params": {
    "items": "all"
  }
}
```

`audio.resetAlgorithmConfig` 的默认值来自 `audio.getAlgorithmCapabilities`，不是 App 本地写死。

## 9. Test / Conformance Notes

| Case | Given | When | Then | Protocol evidence |
|---|---|---|---|---|
| happy path | generated registry 包含 `audio.algorithm`，设备 capability 支持多个 numeric 字段 | 用户打开页面并调整 `noiseSuppression.level` | App 渲染 sliders，set 成功后显示最终值 | `audio.getAlgorithmCapabilities`, `audio.getAlgorithmConfig`, `audio.setAlgorithmConfig` |
| unsupported | 当前产品 registry 或设备 capability 不支持 `audio.algorithm` | 用户打开页面 | 页面隐藏或显示不支持，不发送 draft/missing method | generated registry / `NOT_SUPPORTED` |
| boundary case | capability 声明 `level=0..3` | 用户输入或拖动到范围外 | App 本地阻止；设备仍返回 `OUT_OF_RANGE` 时回滚 | capability descriptor / `audio.setAlgorithmConfig` |
| event path | 其他端修改算法配置 | 设备发送 `audio.algorithmConfigChanged` | App 更新 slider；需要完整状态时调用 get 校准 | `audio.algorithmConfigChanged`, `audio.getAlgorithmConfig` |
| reset path | capability 已返回默认值 | 用户点击恢复默认 | App 调用 reset selector，UI 恢复默认字段 | `audio.resetAlgorithmConfig`, `audio.algorithmConfigChanged` |
| pending restart | 设备返回 `pending_restart` | 用户修改需要重启音频链路的字段 | UI 显示待重启/稍后生效提示 | set/reset response |

## 10. Acceptance Gates

- 页面只在当前 generated registry 包含 `audio.algorithm` 相关方法，且设备 `audio.getAlgorithmCapabilities` 返回支持时可用。
- Slider 范围、步进、默认值和是否可展示全部来自 `audio.getAlgorithmCapabilities`。
- 当前值来自 `audio.getAlgorithmConfig` 或 set/reset response，不由 App 猜测。
- Slider 修改使用 `audio.setAlgorithmConfig` partial update。
- 恢复默认使用 `audio.resetAlgorithmConfig`，默认值来自设备 capability。
- App 处理 `audio.algorithmConfigChanged`，支持多端同步。
- Error UI 覆盖 unsupported、invalid argument、out of range、busy、permission denied 和 pending restart。

## 11. Open Questions

| Question | Impact | Owner | Status | Next action |
|---|---|---|---|---|
| UI 原型中具体有哪些算法 slider？是否只包含 `level` 类字段，还是包含 AGC、beamforming、DOA 等高级参数？ | UI / protocol | TBD | REVIEW-ASK | 产品提供原型后判断是否需要 amendment。 |
| 拖动时是否需要实时预览生效，还是松手/保存后生效？ | product / firmware | TBD | REVIEW-ASK | 决定 App 提交节流策略。 |
| 恢复默认按钮是恢复本页 slider 字段，还是恢复整个 `audio.algorithm` 配置？ | product / protocol | TBD | REVIEW-ASK | 确认 reset selector。 |
| 是否需要按设备角色、会议场景、房间 profile 或用户权限限制可调字段？ | product / permission | TBD | REVIEW-ASK | 如需要，补权限或 profile 约束。 |
