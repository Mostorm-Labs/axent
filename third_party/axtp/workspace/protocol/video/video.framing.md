---
status: draft
contract: false
generated: false
domain: video
feature: video.framing
registry:
lastReviewed: 2026-06-13
---

# video.framing

## 0. 速读结论

| 项目 | 内容 |
|---|---|
| 这个能力做什么 | 管理视频构图模式、区域追踪、gallery / no-top-bar gallery、speaker tracking 及其运行态。 |
| 当前状态 | draft；由 `workspace/business/video-framing-modes.md` 和 `workspace/flows/video-framing-modes.md` 转入。 |
| 是否可直接实现 | 否。本文是草案；正式实现以 registry / generated 为准。 |
| 主要交互 | RPC + EVENT |
| 是否使用 STREAM | 否。视频帧数据仍由 `video.stream` 和 STREAM 承载。 |
| Registry readiness | candidate；已有候选 methods/events，但 mode enum、gallery 边界和 speaker tracking 依赖仍需确认。 |
| Conformance | needed；需覆盖 capability、mode switch、region tracking、event sync、unsupported/conflict。 |
| 主要未决问题 | gallery/no-top-bar 是否归 framing，region tracking carrier 是否作为参数，speaker tracking 音频依赖如何表达。 |

## 1. 功能说明

`video.framing` 用于控制设备输出画面的构图算法和构图运行态。它覆盖全景、auto framing、区域追踪、gallery、无上边条 gallery、speaker tracking 等模式，以及这些模式下的可选配置。

本文只描述 `video.framing` 草案。稳定事实必须写入 `contract/registry/domains/video/domain.yaml` 后再由 Generator 生成。

## 2. 能力边界

| 类型 | 内容 |
|---|---|
| 包含 | framing mode 能力发现、当前配置、模式切换、区域追踪区域、physical/electronic PTZ 追踪承载方式、gallery variant、speaker tracking 配置、运行态事件。 |
| 不包含 | 视频编码/码率/分辨率、STREAM 数据、通用画面布局/PIP/路由、物理 PTZ 手动控制、zoom/focus/exposure。 |
| 数据面 | 不使用 STREAM；本能力只通过 RPC 查询/设置和 EVENT 同步控制面状态。 |

## 3. 方法 Methods

方法 ID、bitOffset 和 schema fieldId 均在采纳进入 registry 时分配正式数值。候选方法名采纳前需和 registry appendix / YAML 再次对齐。

### 3.0 方法速览

| Method | 调用类型 | 用途 | Params Schema | Result Schema | 是否触发事件 | 状态 |
|---|---|---|---|---|---|---|
| `video.getFramingCapabilities` | query | 查询支持的构图模式、配置项和约束。 | `GetFramingCapabilitiesParams` | `VideoFramingCapabilities` | 否 | `[REVIEW-DRAFT]` |
| `video.getFramingConfig` | query | 查询当前 framing 配置。 | `GetFramingConfigParams` | `VideoFramingConfig` | 否 | `[REVIEW-DRAFT]` |
| `video.setFramingConfig` | command | 设置目标构图模式和模式配置。 | `SetFramingConfigParams` | `SetFramingConfigResult` | 是，配置实际变化后触发事件。 | `[REVIEW-DRAFT]` |
| `video.resetFramingConfig` | command | 恢复 framing 默认配置。 | `ResetFramingConfigParams` | `SetFramingConfigResult` | 是 | `[REVIEW-DRAFT]` |
| `video.getFramingState` | query | 查询 framing 运行态、控制 owner 和不可用原因。 | `GetFramingStateParams` | `VideoFramingState` | 否 | `[REVIEW-DRAFT]` |

### 3.1 `video.getFramingCapabilities`

**用途**：返回设备支持哪些 mode、哪些 mode-specific config、事件和默认值。

| 项 | 内容 |
|---|---|
| 调用类型 | query |
| Params Schema | `GetFramingCapabilitiesParams` |
| Result Schema | `VideoFramingCapabilities` |
| 是否触发事件 | 否 |
| 幂等性 / 异步性 | 幂等；同步返回能力快照。 |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_ARGUMENT`, `PERMISSION_DENIED`, `UNAVAILABLE` |

#### 3.1.1 请求参数 Params：`GetFramingCapabilitiesParams`

主要字段为 `cameraId` 和 `sourceId`。

#### 3.1.2 返回结果 Result：`VideoFramingCapabilities`

必须能表达 `modes`、`galleryVariants`、`trackingCarriers` 和默认配置。

#### 3.1.3 d block 示例

request:

```json
{
  "id": 101,
  "method": "video.getFramingCapabilities",
  "params": {
    "cameraId": "main",
    "sourceId": "camera.main"
  }
}
```

success:

```json
{
  "id": 101,
  "status": {
    "ok": true,
    "code": 0
  },
  "result": {
    "capability": "video.framing",
    "modes": [
      "auto",
      "manual"
    ],
    "regionCoordinateUnit": "normalized"
  }
}
```

#### 3.1.4 可能触发的事件

| Event | 触发条件 | Payload Schema | 客户端处理建议 |
|---|---|---|---|
| 无 | query 不应产生状态变化。 | none | 无需处理。 |

#### 3.1.5 错误

| 错误 | 场景 | 返回建议 |
|---|---|---|
| `NOT_SUPPORTED` | 设备没有 framing 服务。 | UI 隐藏 framing 设置。 |
| `UNAVAILABLE` | 摄像头链路或算法服务暂不可用。 | 稍后重试或展示不可用原因。 |

#### 3.1.6 错误 d block 示例

```json
{
  "id": 101,
  "status": {
    "ok": false,
    "code": 3,
    "msg": "Request failed.",
    "details": {
      "candidateError": "NOT_SUPPORTED",
      "field": "cameraId",
      "reason": "unsupported value"
    }
  }
}
```

### 3.2 `video.getFramingConfig`

**用途**：查询当前生效或待生效的 framing 配置。

| 项 | 内容 |
|---|---|
| 调用类型 | query |
| Params Schema | `GetFramingConfigParams` |
| Result Schema | `VideoFramingConfig` |
| 是否触发事件 | 否 |
| 幂等性 / 异步性 | 幂等；同步返回配置快照。 |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_ARGUMENT`, `PERMISSION_DENIED`, `UNAVAILABLE` |

#### 3.2.1 请求参数 Params：`GetFramingConfigParams`

#### 3.2.2 返回结果 Result：`VideoFramingConfig`

#### 3.2.3 d block 示例

request:

```json
{
  "id": 102,
  "method": "video.getFramingConfig",
  "params": {
    "cameraId": "main"
  }
}
```

success:

```json
{
  "id": 102,
  "status": {
    "ok": true,
    "code": 0
  },
  "result": {
    "cameraId": "main",
    "mode": "auto",
    "target": "speaker",
    "margin": 12
  }
}
```

#### 3.2.4 可能触发的事件

| Event | 触发条件 | Payload Schema | 客户端处理建议 |
|---|---|---|---|
| 无 | 查询不改变配置。 | none | 无需处理。 |

#### 3.2.5 错误

| 错误 | 场景 | 返回建议 |
|---|---|---|
| `INVALID_ARGUMENT` | `cameraId` 或 `sourceId` 非法。 | 返回字段路径。 |
| `UNAVAILABLE` | framing 状态不可读。 | 返回 `unavailableReason` detail。 |

#### 3.2.6 错误 d block 示例

```json
{
  "id": 102,
  "status": {
    "ok": false,
    "code": 10,
    "msg": "Invalid argument.",
    "details": {
      "candidateError": "INVALID_ARGUMENT",
      "field": "cameraId",
      "reason": "invalid field value"
    }
  }
}
```

### 3.3 `video.setFramingConfig`

**用途**：设置 framing mode 和 mode-specific 配置。

| 项 | 内容 |
|---|---|
| 调用类型 | command |
| Params Schema | `SetFramingConfigParams` |
| Result Schema | `SetFramingConfigResult` |
| 是否触发事件 | 是，实际变化后触发 `video.framingConfigChanged` 和/或 `video.framingStateChanged`。 |
| 幂等性 / 异步性 | 建议幂等；切换算法可能异步，先返回 `applyState=applying`。 |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_ARGUMENT`, `OUT_OF_RANGE`, `INVALID_STATE`, `BUSY`, `DEVICE_MODE_CONFLICT`, `PERMISSION_DENIED` |

#### 3.3.1 请求参数 Params：`SetFramingConfigParams`

`config.mode` 是必填或部分更新字段，未携带的字段保持原值。

#### 3.3.2 返回结果 Result：`SetFramingConfigResult`

返回最终配置或 `applying` 状态下的目标配置。

#### 3.3.3 d block 示例

request:

```json
{
  "id": 103,
  "method": "video.setFramingConfig",
  "params": {
    "cameraId": "main",
    "config": {
      "cameraId": "main",
      "mode": "auto",
      "target": "speaker",
      "margin": 12
    },
    "expectedVersion": "1.2.3"
  }
}
```

success:

```json
{
  "id": 103,
  "status": {
    "ok": true,
    "code": 0
  },
  "result": {
    "cameraId": "main",
    "mode": "auto",
    "target": "speaker",
    "margin": 12
  }
}
```

#### 3.3.4 可能触发的事件

| Event | 触发条件 | Payload Schema | 客户端处理建议 |
|---|---|---|---|
| `video.framingConfigChanged` | 配置实际变化。 | `VideoFramingConfigChangedEvent` | 可更新 UI；若 payload 是 patch，调用 get 校准。 |
| `video.framingStateChanged` | mode 切换进入 applying / active / degraded / failed。 | `VideoFramingStateChangedEvent` | 更新状态文案和禁用态。 |
| `camera.ptzStateChanged` | `trackingCarrier=physical_ptz` 时 framing 接管或释放 PTZ。 | `PtzStateChangedEvent` | 禁用或恢复手动 PTZ 控件。 |

#### 3.3.5 错误

| 错误 | 场景 | 返回建议 |
|---|---|---|
| `NOT_SUPPORTED` | mode、gallery variant 或 tracking carrier 不支持。 | 返回不支持的字段路径。 |
| `OUT_OF_RANGE` | tracking region 越界。 | 返回合法范围。 |
| `DEVICE_MODE_CONFLICT` | physical PTZ 正被其他控制端占用，或 privacy cover 生效。 | 返回冲突 owner。 |
| `BUSY` | 算法服务正在切换。 | 建议稍后重试。 |

#### 3.3.6 错误 d block 示例

```json
{
  "id": 103,
  "status": {
    "ok": false,
    "code": 3,
    "msg": "Request failed.",
    "details": {
      "candidateError": "NOT_SUPPORTED",
      "field": "cameraId",
      "reason": "unsupported value"
    }
  }
}
```

### 3.4 `video.resetFramingConfig`

**用途**：按 scope 恢复 framing 默认配置。

| 项 | 内容 |
|---|---|
| 调用类型 | command |
| Params Schema | `ResetFramingConfigParams` |
| Result Schema | `SetFramingConfigResult` |
| 是否触发事件 | 是，配置变化后触发 `video.framingConfigChanged`。 |
| 幂等性 / 异步性 | 幂等；可同步或异步应用。 |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_ARGUMENT`, `PERMISSION_DENIED`, `BUSY` |

#### 3.4.1 请求参数 Params：`ResetFramingConfigParams`

#### 3.4.2 返回结果 Result：`SetFramingConfigResult`

#### 3.4.3 d block 示例

request:

```json
{
  "id": 104,
  "method": "video.resetFramingConfig",
  "params": {
    "cameraId": "main"
  }
}
```

success:

```json
{
  "id": 104,
  "status": {
    "ok": true,
    "code": 0
  },
  "result": {
    "cameraId": "main",
    "mode": "auto",
    "target": "speaker",
    "margin": 12
  }
}
```

#### 3.4.4 可能触发的事件

| Event | 触发条件 | Payload Schema | 客户端处理建议 |
|---|---|---|---|
| `video.framingConfigChanged` | 默认配置与当前配置不同。 | `VideoFramingConfigChangedEvent` | 更新 UI。 |

#### 3.4.5 错误

| 错误 | 场景 | 返回建议 |
|---|---|---|
| `NOT_SUPPORTED` | 设备不支持 reset。 | UI 隐藏恢复默认。 |
| `BUSY` | 正在切换 mode。 | 稍后重试。 |

#### 3.4.6 错误 d block 示例

```json
{
  "id": 104,
  "status": {
    "ok": false,
    "code": 3,
    "msg": "Request failed.",
    "details": {
      "candidateError": "NOT_SUPPORTED",
      "field": "cameraId",
      "reason": "unsupported value"
    }
  }
}
```

### 3.5 `video.getFramingState`

**用途**：查询 framing 运行态、当前控制 owner 和不可用原因。

| 项 | 内容 |
|---|---|
| 调用类型 | query |
| Params Schema | `GetFramingStateParams` |
| Result Schema | `VideoFramingState` |
| 是否触发事件 | 否 |
| 幂等性 / 异步性 | 幂等；同步返回状态快照。 |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_ARGUMENT`, `PERMISSION_DENIED`, `UNAVAILABLE` |

#### 3.5.1 请求参数 Params：`GetFramingStateParams`

#### 3.5.2 返回结果 Result：`VideoFramingState`

#### 3.5.3 d block 示例

request:

```json
{
  "id": 105,
  "method": "video.getFramingState",
  "params": {
    "cameraId": "main"
  }
}
```

success:

```json
{
  "id": 105,
  "status": {
    "ok": true,
    "code": 0
  },
  "result": {
    "cameraId": "main",
    "mode": "auto",
    "active": true,
    "target": "speaker"
  }
}
```

#### 3.5.4 可能触发的事件

| Event | 触发条件 | Payload Schema | 客户端处理建议 |
|---|---|---|---|
| 无 | 查询不改变状态。 | none | 无需处理。 |

#### 3.5.5 错误

| 错误 | 场景 | 返回建议 |
|---|---|---|
| `UNAVAILABLE` | camera pipeline 不可读。 | 返回 unavailable detail。 |

#### 3.5.6 错误 d block 示例

```json
{
  "id": 105,
  "status": {
    "ok": false,
    "code": 13,
    "msg": "Request failed.",
    "details": {
      "candidateError": "UNAVAILABLE",
      "field": "cameraId",
      "reason": "service unavailable"
    }
  }
}
```

## 4. 事件 Events

### 4.0 事件速览

| Event | 触发条件 | Payload Schema | 客户端处理建议 | 状态 |
|---|---|---|---|---|
| `video.framingConfigChanged` | RPC、本地按键、profile、restore、legacy adapter 改变配置。 | `VideoFramingConfigChangedEvent` | 更新 UI；需要完整状态时调用 get。 | `[REVIEW-DRAFT]` |
| `video.framingStateChanged` | applying / active / degraded / failed / unavailable 变化。 | `VideoFramingStateChangedEvent` | 更新状态和错误提示。 | `[REVIEW-DRAFT]` |
| `video.framingModeChanged` | 仅 mode 改变时的轻量事件。 | `VideoFramingModeChangedEvent` | 可选事件；是否保留需评审。 | `[REVIEW-ASK]` |

### 4.1 `video.framingConfigChanged`

**触发条件**：

- `video.setFramingConfig` 或 `video.resetFramingConfig` 导致配置变化。
- 本地按键、遥控器、profile、restore 或 legacy adapter 改变配置。

#### Payload：`VideoFramingConfigChangedEvent`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `cameraId` | string | no | device-defined | `main` | 摄像头对象。 |
| `changedFields` | string[] | no | field path array | omitted | 变化字段。 |
| `config` | `VideoFramingConfig` | yes | see schema | none | 变化后的完整或目标配置；是否允许 patch 需评审。 |
| `applyState` | string enum | yes | `applied`, `applying`, `pending_restart`, `failed` | none | 应用状态。 |
| `reason` | string enum | no | `user_request`, `physical_control`, `profile_changed`, `restore_config`, `legacy_adapter`, `device_policy`, `unknown` | `unknown` | 变化来源。 |

#### d block 示例

```json
{
  "event": "video.framingConfigChanged",
  "intent": 1,
  "data": {
    "changedFields": [
      "state"
    ],
    "config": {
      "cameraId": "main",
      "mode": "auto",
      "target": "speaker",
      "margin": 12
    },
    "applyState": "active",
    "reason": "user_request"
  }
}
```


#### 客户端处理建议

| 场景 | 建议 |
|---|---|
| payload 是完整配置 | 直接更新本地 UI。 |
| payload 是变化片段 | 调用 `video.getFramingConfig` 校准。 |
| event 丢失或重连 | 重连后调用 `video.getFramingConfig` 和 `video.getFramingState`。 |

### 4.2 `video.framingStateChanged`

**触发条件**：

- mode 切换进入 applying / active / degraded / failed。
- speaker tracking 音频依赖不可用。
- region tracking 接管或释放 physical PTZ。

#### Payload：`VideoFramingStateChangedEvent`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `cameraId` | string | no | device-defined | `main` | 摄像头对象。 |
| `state` | `VideoFramingState` | yes | see schema | none | 当前运行态。 |
| `reason` | string enum | no | `config_changed`, `algorithm_ready`, `resource_conflict`, `dependency_unavailable`, `unknown` | `unknown` | 状态变化原因。 |

#### d block 示例

```json
{
  "event": "video.framingStateChanged",
  "intent": 1,
  "data": {
    "state": {
      "cameraId": "main",
      "mode": "auto",
      "active": true,
      "target": "speaker"
    },
    "reason": "user_request"
  }
}
```


#### 客户端处理建议

| 场景 | 建议 |
|---|---|
| `state.applyState=active` | 显示模式已生效。 |
| `state.applyState=degraded` | 展示降级原因，保留当前配置。 |
| `state.applyState=failed` | 回滚 UI 或提示用户重新设置。 |

### 4.3 `video.framingModeChanged`

**触发条件**：只在产品需要低成本 mode 订阅时发送；否则由 `video.framingConfigChanged` 覆盖。

#### Payload：`VideoFramingModeChangedEvent`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `cameraId` | string | no | device-defined | `main` | 摄像头对象。 |
| `mode` | string enum | yes | `panorama`, `auto_framing`, `region_tracking`, `gallery`, `speaker_tracking`, `manual` | none | 新 mode。 |
| `previousMode` | string enum | no | same as `mode` | omitted | 旧 mode。 |
| `reason` | string enum | no | see config event | `unknown` | 变化原因。 |

#### d block 示例

```json
{
  "event": "video.framingModeChanged",
  "intent": 1,
  "data": {
    "mode": "auto",
    "previousMode": "auto",
    "reason": "user_request"
  }
}
```


#### 客户端处理建议

| 场景 | 建议 |
|---|---|
| 已订阅 config event | 可以不订阅本事件。 |
| 仅关心 mode selector | 可使用本事件快速更新控件，再按需 get config。 |

## 5. Capability

| 能力字段 | 类型 | 必填 | 取值范围 / 枚举 | 说明 |
|---|---|---:|---|---|
| `capability` | string | yes | fixed `video.framing` | capability 名称。 |
| `modes` | string[] | yes | `panorama`, `auto_framing`, `region_tracking`, `gallery`, `speaker_tracking`, `manual` candidate | 支持的 framing modes。 |
| `defaultMode` | string enum | no | one of `modes` | 默认模式。 |
| `trackingCarriers` | string[] | no | `physical_ptz`, `electronic_ptz` | 区域追踪承载方式。 |
| `galleryVariants` | string[] | no | `standard`, `no_top_bar` | gallery 展示变体。 |
| `speakerTracking` | object | no | see schema | speaker tracking 能力摘要。 |
| `events` | string[] | no | event names | 支持的事件。 |

## 6. 字段 / Schemas

本草案采用复杂 feature 展开模式：method/event 章节只写 schema 名称，完整字段集中在本章。

### 6.1 Schema 层级速览

| 层级 | 用在哪里 | 作用 |
|---|---|---|
| `VideoFramingCapabilities` | `video.getFramingCapabilities` result | 描述支持模式、默认值和模式约束。 |
| `VideoFramingConfig` | get/set/reset result、config event | 表示当前或目标 framing 配置。 |
| `VideoFramingState` | get state、state event | 表示算法运行态、owner、失败/降级原因。 |

### 6.2 请求与响应 Schemas

#### `GetFramingCapabilitiesParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `cameraId` | string | no | device-defined | `main` | 摄像头对象。 |
| `sourceId` | string | no | device-defined | omitted | 视频源对象。 |

#### `GetFramingConfigParams` / `GetFramingStateParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `cameraId` | string | no | device-defined | `main` | 摄像头对象。 |

#### `SetFramingConfigParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `cameraId` | string | no | device-defined | `main` | 摄像头对象。 |
| `config` | `VideoFramingConfig` | yes | see schema | none | 目标配置或 patch。 |
| `expectedVersion` | uint32 | no | version token | omitted | 防止覆盖并发修改。 |

#### `ResetFramingConfigParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `cameraId` | string | no | device-defined | `main` | 摄像头对象。 |
| `scope` | string enum | no | `all`, `mode`, `tracking`, `gallery`, `speaker_tracking` | `all` | reset 范围。 |

#### `SetFramingConfigResult`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `config` | `VideoFramingConfig` | yes | see schema | none | 应用后的当前或目标配置。 |
| `state` | `VideoFramingState` | no | see schema | omitted | 当前运行态。 |
| `applyState` | string enum | yes | `applied`, `applying`, `pending_restart`, `failed` | none | 设置应用状态。 |
| `version` | uint32 | no | monotonically increasing | omitted | 配置版本。 |

### 6.3 Capability Schemas

#### `VideoFramingCapabilities`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `capability` | string | yes | fixed `video.framing` | none | capability 名称。 |
| `modes` | string[] | yes | framing mode enum | none | 支持 modes。 |
| `defaultConfig` | `VideoFramingConfig` | no | see schema | omitted | 默认配置。 |
| `trackingCarriers` | string[] | no | `physical_ptz`, `electronic_ptz` | omitted | 区域追踪承载方式。 |
| `regionCoordinateUnit` | string enum | no | `normalized` | `normalized` | 区域坐标单位。 |
| `galleryVariants` | string[] | no | `standard`, `no_top_bar` | omitted | gallery 变体。 |
| `speakerTracking` | object | no | implementation-defined | omitted | speaker tracking delay/strategy 能力。 |
| `events` | string[] | no | event names | omitted | 支持事件。 |

### 6.4 Config / State 总结构

#### `VideoFramingConfig`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `mode` | string enum | yes | `panorama`, `auto_framing`, `region_tracking`, `gallery`, `speaker_tracking`, `manual` | none | 主 framing mode。 |
| `tracking` | `FramingTrackingConfig` | no | see schema | omitted | region tracking 配置。 |
| `gallery` | `FramingGalleryConfig` | no | see schema | omitted | gallery 配置。 |
| `speakerTracking` | `SpeakerTrackingConfig` | no | see schema | omitted | speaker tracking 配置。 |
| `version` | uint32 | no | monotonically increasing | omitted | 配置版本。 |

#### `VideoFramingState`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `mode` | string enum | yes | same as config `mode` | none | 当前运行 mode。 |
| `applyState` | string enum | yes | `idle`, `applying`, `active`, `degraded`, `failed`, `unavailable` | none | 运行态。 |
| `controlOwner` | string enum | no | `user`, `framing_algorithm`, `physical_control`, `remote_client`, `unknown` | `unknown` | 当前控制 owner。 |
| `unavailableReason` | string enum | no | `privacy_cover`, `camera_offline`, `ptz_busy`, `audio_dependency_unavailable`, `low_light`, `unknown` | omitted | 不可用或降级原因。 |

### 6.5 各对象字段

#### `FramingTrackingConfig`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `enabled` | bool | no | true/false | true | 是否启用追踪。 |
| `trackingCarrier` | string enum | no | `physical_ptz`, `electronic_ptz` | capability default | 追踪承载方式。 |
| `region` | object | no | normalized rect | omitted | 追踪区域，`x/y/width/height` 均为 `0..1`。 |
| `horizontalStrategy` | string enum | no | `auto`, `lock`, `follow` | `auto` | 水平方向策略。 |
| `verticalStrategy` | string enum | no | `auto`, `lock`, `follow` | `auto` | 垂直方向策略。 |

#### `FramingGalleryConfig`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `variant` | string enum | no | `standard`, `no_top_bar` | `standard` | gallery 样式。 |
| `maxTiles` | uint8 | no | capability range | omitted | 最大分格数。 |

#### `SpeakerTrackingConfig`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `enabled` | bool | no | true/false | true | 是否启用 speaker tracking。 |
| `delayMs` | uint16 | no | capability range | omitted | 说话人切换延迟。 |
| `strategy` | string enum | no | `auto`, `fast_switch`, `stable_speaker` | `auto` | 切换策略。 |
| `audioDependency` | string enum | no | `none`, `audio_algorithm`, `beamforming` | `audio_algorithm` | 依赖的音频能力。 |

## 7. JSON 示例

### 7.1 场景：设置区域追踪 over physical PTZ

#### request

```json
{
  "id": 1,
  "method": "video.setFramingConfig",
  "params": {
    "cameraId": "main",
    "config": {
      "mode": "region_tracking",
      "tracking": {
        "trackingCarrier": "physical_ptz",
        "region": { "x": 0.25, "y": 0.20, "width": 0.50, "height": 0.45 },
        "horizontalStrategy": "follow",
        "verticalStrategy": "auto"
      }
    }
  }
}
```

#### response

```json
{
  "id": 1,
  "status": { "ok": true, "code": 0 },
  "result": {
    "applyState": "applying",
    "config": {
      "mode": "region_tracking",
      "tracking": {
        "trackingCarrier": "physical_ptz",
        "region": { "x": 0.25, "y": 0.20, "width": 0.50, "height": 0.45 }
      }
    },
    "state": {
      "mode": "region_tracking",
      "applyState": "applying",
      "controlOwner": "framing_algorithm"
    }
  }
}
```

读法：RPC 成功表示设备接受设置；最终 active/degraded/failed 通过 state event 或 get state 确认。

### 7.2 场景：事件通知

```json
{
  "event": "video.framingStateChanged",
  "data": {
    "cameraId": "main",
    "state": {
      "mode": "region_tracking",
      "applyState": "active",
      "controlOwner": "framing_algorithm"
    },
    "reason": "algorithm_ready"
  }
}
```

### 7.3 场景：失败响应

```json
{
  "id": 2,
  "status": {
    "ok": false,
    "code": 263,
    "msg": "Device mode conflict.",
    "details": {
      "field": "config.tracking.trackingCarrier",
      "owner": "remote_client",
      "candidateError": "DEVICE_MODE_CONFLICT"
    }
  }
}
```

读法：physical PTZ 被其他 owner 占用；客户端应提示冲突或切换到 electronic PTZ。

## 8. 错误

| 错误 | 适用场景 | 说明 |
|---|---|---|
| `NOT_SUPPORTED` | mode、variant 或 carrier 不支持。 | 返回具体字段路径。 |
| `INVALID_ARGUMENT` | 字段组合非法。 | 例如 gallery config 出现在非 gallery mode。 |
| `OUT_OF_RANGE` | region 坐标越界。 | 返回合法范围。 |
| `INVALID_STATE` | 摄像头或算法状态不允许切换。 | 例如 privacy cover 生效。 |
| `DEVICE_MODE_CONFLICT` | physical PTZ 或 speaker dependency 被占用。 | 返回当前 owner。 |
| `BUSY` | 正在切换 mode。 | 客户端稍后重试。 |

## 9. Legacy 映射

| legacy 项 | 候选映射 | 状态 | 说明 |
|---|---|---|---|
| `CommonSetVideoMode` / `CommonGetVideoMode` | `video.setFramingConfig` / `video.getFramingConfig` | candidate | `VideoMode(0 全景, 1 智能跟踪)` 等枚举需确认。 |
| `CommonSetVideoTrackMode` / `CommonGetVideoTrackMode` | `video.setFramingConfig` / `video.getFramingConfig` | candidate | tracking mode 与本草案 mode enum 对齐待确认。 |
| `CommonSetRegionTracking` / `CommonGetRegionTracking` | `video.setFramingConfig` / `video.getFramingConfig` | candidate | payload 未完全确认。 |
| `CommonSetHorTrackingStrategy` / `CommonGetHorTrackingStrategy` | `tracking.horizontalStrategy` | candidate | 枚举和单位待确认。 |
| `CommonSetVerTrackingStrategy` / `CommonGetVerTrackingStrategy` | `tracking.verticalStrategy` | candidate | 枚举和单位待确认。 |
| `CommonSetSpeakerTrackDelay` / `CommonGetSpeakerTrackDelay` | `speakerTracking.delayMs` | candidate | 单位是否毫秒待确认。 |
| `Config.MultiSet:Video mode=auto-framing` | `mode=auto_framing` | candidate | VM33 字段仍需按实际 payload 核对。 |


## 10. 测试要点

| 类型 | 要点 |
|---|---|
| happy path | 查询能力，设置 `auto_framing`，收到 active state。 |
| error path | unsupported mode、region 越界、physical PTZ 冲突、speaker dependency unavailable。 |
| boundary case | region `x + width <= 1`、`y + height <= 1`，gallery variant 支持性。 |
| capability discovery | mode、trackingCarrier、galleryVariants 与 set 参数一致。 |
| event | set 成功触发 config/state event；失败不触发成功事件。 |

## 11. 待确认问题

| 问题 | 影响 | 当前建议 | 状态 |
|---|---|---|---|
| gallery/no-top-bar 是否归 framing？ | 决定 schema 字段或是否转到 `video.layout` / `video.overlay`。 | 先作为 `gallery.variant` 候选。 | `[REVIEW-ASK]` |
| physical/electronic PTZ 是 mode 还是参数？ | 影响 mode enum 和兼容性。 | 先作为 `tracking.trackingCarrier`。 | `[REVIEW-DRAFT]` |
| speaker tracking 是否依赖 `audio.algorithm`？ | 影响 capability dependency 和 unavailable reason。 | 先用 `audioDependency` 字段表达。 | `[REVIEW-ASK]` |
| 是否保留 `video.framingModeChanged`？ | 影响事件数量和客户端订阅。 | 优先用 `framingConfigChanged`，mode event 作为可选。 | `[REVIEW-ASK]` |
