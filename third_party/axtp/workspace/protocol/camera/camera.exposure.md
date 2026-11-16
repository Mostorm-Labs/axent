---
status: draft
contract: false
generated: false
domain: camera
feature: camera.exposure
registry:
lastReviewed: 2026-06-13
---

# camera.exposure

## 0. 速读结论

| 项目 | 内容 |
|---|---|
| 这个能力做什么 | 管理曝光模式、ISO/gain、shutter、EV、WDR 和防频闪/工频设置。 |
| 当前状态 | draft；由 `workspace/business/camera-image-settings.md` 和 `workspace/flows/camera-image-settings.md` 转入。 |
| 是否可直接实现 | 否。草案阶段仅供评审；正式实现以 registry / generated 为准。 |
| 主要交互 | RPC + EVENT |
| 是否使用 STREAM | 否 |
| Registry readiness | candidate；ISO/gain/shutter/EV 单位和 WDR 归属仍需确认。 |
| Conformance | needed；需覆盖 capability、mode dependency、set/reset/event、非法组合。 |
| 主要未决问题 | ISO 与 gain 是否同字段，shutter 单位，EV 枚举/范围，WDR 是否归 exposure。 |

## 1. 功能说明

`camera.exposure` 用于摄像头曝光链路配置。它覆盖自动/手动曝光模式、ISO/gain、快门、曝光补偿、WDR 和防频闪。基础画质参数归 `camera.image`，白平衡归 `camera.whiteBalance`。

## 2. 能力边界

| 类型 | 内容 |
|---|---|
| 包含 | exposure mode、ISO/gain、shutter、EV、WDR、powerLineFrequency、防频闪、reset、事件。 |
| 不包含 | brightness/contrast/saturation、white balance、focus/zoom/PTZ、视频 encoder/stream。 |
| 数据面 | 不使用 STREAM。 |

## 3. 方法 Methods

### 3.0 方法速览

| Method | 调用类型 | 用途 | Params Schema | Result Schema | 是否触发事件 | 状态 |
|---|---|---|---|---|---|---|
| `camera.getExposureCapabilities` | query | 查询曝光模式、字段范围、单位和依赖关系。 | `GetExposureCapabilitiesParams` | `ExposureCapabilities` | 否 | `[REVIEW-DRAFT]` |
| `camera.getExposureConfig` | query | 查询当前曝光配置。 | `GetExposureConfigParams` | `ExposureConfig` | 否 | `[REVIEW-DRAFT]` |
| `camera.setExposureConfig` | command | 部分更新曝光配置。 | `SetExposureConfigParams` | `SetExposureConfigResult` | 是 | `[REVIEW-DRAFT]` |
| `camera.resetExposureConfig` | command | 恢复曝光默认值。 | `ResetExposureConfigParams` | `SetExposureConfigResult` | 是 | `[REVIEW-DRAFT]` |

### 3.1 `camera.getExposureCapabilities`

| 项 | 内容 |
|---|---|
| 调用类型 | query |
| Params Schema | `GetExposureCapabilitiesParams` |
| Result Schema | `ExposureCapabilities` |
| 是否触发事件 | 否 |
| 幂等性 / 异步性 | 幂等。 |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_ARGUMENT`, `PERMISSION_DENIED`, `UNAVAILABLE` |

#### 3.1.1 请求参数 Params：`GetExposureCapabilitiesParams`

#### 3.1.2 返回结果 Result：`ExposureCapabilities`

#### 3.1.3 d block 示例

request:

```json
{
  "id": 101,
  "method": "camera.getExposureCapabilities",
  "params": {
    "cameraId": "main"
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
    "capability": "camera.exposure",
    "modes": [
      "auto",
      "manual"
    ],
    "fields": {}
  }
}
```

#### 3.1.4 可能触发的事件

| Event | 触发条件 | Payload Schema | 客户端处理建议 |
|---|---|---|---|
| 无 | 查询不改变配置。 | none | 无需处理。 |

#### 3.1.5 错误

| 错误 | 场景 | 返回建议 |
|---|---|---|
| `NOT_SUPPORTED` | 设备不支持曝光控制。 | 隐藏曝光设置。 |

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

### 3.2 `camera.getExposureConfig`

| 项 | 内容 |
|---|---|
| 调用类型 | query |
| Params Schema | `GetExposureConfigParams` |
| Result Schema | `ExposureConfig` |
| 是否触发事件 | 否 |
| 幂等性 / 异步性 | 幂等。 |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_ARGUMENT`, `PERMISSION_DENIED`, `UNAVAILABLE` |

#### 3.2.1 请求参数 Params：`GetExposureConfigParams`

#### 3.2.2 返回结果 Result：`ExposureConfig`

#### 3.2.3 d block 示例

request:

```json
{
  "id": 102,
  "method": "camera.getExposureConfig",
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
    "ev": 0,
    "powerLineFrequency": "50Hz",
    "wdrEnabled": true
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
| `UNAVAILABLE` | sensor 状态不可读。 | 返回 unavailable detail。 |

#### 3.2.6 错误 d block 示例

```json
{
  "id": 102,
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

### 3.3 `camera.setExposureConfig`

| 项 | 内容 |
|---|---|
| 调用类型 | command |
| Params Schema | `SetExposureConfigParams` |
| Result Schema | `SetExposureConfigResult` |
| 是否触发事件 | 是，配置实际变化后触发 `camera.exposureConfigChanged`。 |
| 幂等性 / 异步性 | 建议幂等；支持 partial update，非法组合必须整体失败。 |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_ARGUMENT`, `OUT_OF_RANGE`, `DEVICE_MODE_CONFLICT`, `BUSY`, `PERMISSION_DENIED` |

#### 3.3.1 请求参数 Params：`SetExposureConfigParams`

#### 3.3.2 返回结果 Result：`SetExposureConfigResult`

#### 3.3.3 d block 示例

request:

```json
{
  "id": 103,
  "method": "camera.setExposureConfig",
  "params": {
    "cameraId": "main",
    "config": {
      "cameraId": "main",
      "mode": "auto",
      "ev": 0,
      "powerLineFrequency": "50Hz",
      "wdrEnabled": true
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
    "ev": 0,
    "powerLineFrequency": "50Hz",
    "wdrEnabled": true
  }
}
```

#### 3.3.4 可能触发的事件

| Event | 触发条件 | Payload Schema | 客户端处理建议 |
|---|---|---|---|
| `camera.exposureConfigChanged` | 配置实际变化。 | `ExposureConfigChangedEvent` | 更新 UI；根据 mode 禁用/启用字段。 |

#### 3.3.5 错误

| 错误 | 场景 | 返回建议 |
|---|---|---|
| `DEVICE_MODE_CONFLICT` | auto mode 下写 manual-only 字段。 | 返回 required mode。 |
| `OUT_OF_RANGE` | ISO/gain/shutter/EV 越界。 | 返回合法范围。 |
| `INVALID_ARGUMENT` | 字段组合非法。 | 整体失败，不部分生效。 |

#### 3.3.6 错误 d block 示例

```json
{
  "id": 103,
  "status": {
    "ok": false,
    "code": 10,
    "msg": "Invalid argument.",
    "details": {
      "candidateError": "DEVICE_MODE_CONFLICT",
      "field": "cameraId",
      "reason": "request cannot be applied"
    }
  }
}
```

### 3.4 `camera.resetExposureConfig`

| 项 | 内容 |
|---|---|
| 调用类型 | command |
| Params Schema | `ResetExposureConfigParams` |
| Result Schema | `SetExposureConfigResult` |
| 是否触发事件 | 是 |
| 幂等性 / 异步性 | 幂等。 |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_ARGUMENT`, `BUSY`, `PERMISSION_DENIED` |

#### 3.4.1 请求参数 Params：`ResetExposureConfigParams`

#### 3.4.2 返回结果 Result：`SetExposureConfigResult`

#### 3.4.3 d block 示例

request:

```json
{
  "id": 104,
  "method": "camera.resetExposureConfig",
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
    "ev": 0,
    "powerLineFrequency": "50Hz",
    "wdrEnabled": true
  }
}
```

#### 3.4.4 可能触发的事件

| Event | 触发条件 | Payload Schema | 客户端处理建议 |
|---|---|---|---|
| `camera.exposureConfigChanged` | reset 后配置变化。 | `ExposureConfigChangedEvent` | 更新 UI。 |

#### 3.4.5 错误

| 错误 | 场景 | 返回建议 |
|---|---|---|
| `BUSY` | sensor 正忙。 | 稍后重试。 |

#### 3.4.6 错误 d block 示例

```json
{
  "id": 104,
  "status": {
    "ok": false,
    "code": 5,
    "msg": "Request failed.",
    "details": {
      "candidateError": "BUSY",
      "field": "cameraId",
      "reason": "resource is busy"
    }
  }
}
```

## 4. 事件 Events

### 4.0 事件速览

| Event | 触发条件 | Payload Schema | 客户端处理建议 | 状态 |
|---|---|---|---|---|
| `camera.exposureConfigChanged` | set/reset/profile/auto algorithm 改变配置。 | `ExposureConfigChangedEvent` | 更新 UI 或调用 get 校准。 | `[REVIEW-DRAFT]` |

### 4.1 `camera.exposureConfigChanged`

#### Payload：`ExposureConfigChangedEvent`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `cameraId` | string | no | device-defined | `main` | 摄像头对象。 |
| `changedFields` | string[] | no | field path array | omitted | 变化字段。 |
| `config` | `ExposureConfig` | yes | see schema | none | 变化后的配置。 |
| `applyState` | string enum | yes | `applied`, `pending_restart`, `failed` | none | 应用状态。 |
| `reason` | string enum | no | `user_request`, `reset_to_default`, `auto_algorithm`, `profile_changed`, `legacy_adapter`, `unknown` | `unknown` | 变化原因。 |

#### d block 示例

```json
{
  "event": "camera.exposureConfigChanged",
  "intent": 1,
  "data": {
    "changedFields": [
      "state"
    ],
    "config": {
      "cameraId": "main",
      "mode": "auto",
      "ev": 0,
      "powerLineFrequency": "50Hz",
      "wdrEnabled": true
    },
    "applyState": "active",
    "reason": "user_request"
  }
}
```


#### 客户端处理建议

| 场景 | 建议 |
|---|---|
| mode 变化 | 重新计算字段启用状态。 |
| event 丢失或重连 | 调用 `camera.getExposureConfig`。 |

## 5. Capability

| 能力字段 | 类型 | 必填 | 取值范围 / 枚举 | 说明 |
|---|---|---:|---|---|
| `capability` | string | yes | fixed `camera.exposure` | capability 名称。 |
| `modes` | string[] | yes | `auto`, `manual`, `auto_iso`, `auto_shutter` candidate | 支持模式。 |
| `fields` | object | yes | descriptors | ISO/gain/shutter/EV/WDR/frequency 范围。 |
| `modeDependencies` | object | no | field -> modes | 字段可写依赖。 |
| `partialUpdate` | bool | no | true/false | 是否支持部分更新。 |
| `events` | string[] | no | event names | 支持事件。 |

## 6. 字段 / Schemas

### 6.1 Schema 层级速览

| 层级 | 用在哪里 | 作用 |
|---|---|---|
| `ExposureCapabilities` | get capabilities result | 描述模式、字段范围、单位和依赖。 |
| `ExposureConfig` | get/set/reset/event | 表示曝光配置。 |
| `SetExposureConfigResult` | set/reset result | 表示应用状态和最终配置。 |

### 6.2 请求与响应 Schemas

#### `GetExposureCapabilitiesParams` / `GetExposureConfigParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `cameraId` | string | no | device-defined | `main` | 摄像头对象。 |

#### `SetExposureConfigParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `cameraId` | string | no | device-defined | `main` | 摄像头对象。 |
| `config` | `ExposureConfig` | yes | see schema | none | 目标配置或 patch。 |
| `expectedVersion` | uint32 | no | version token | omitted | 并发保护。 |

#### `ResetExposureConfigParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `cameraId` | string | no | device-defined | `main` | 摄像头对象。 |
| `fields` | string[] | no | field names or `all` | `all` | reset 范围。 |

#### `SetExposureConfigResult`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `config` | `ExposureConfig` | yes | see schema | none | 最终配置。 |
| `applyState` | string enum | yes | `applied`, `pending_restart`, `failed` | none | 应用状态。 |
| `version` | uint32 | no | monotonically increasing | omitted | 配置版本。 |

### 6.3 Capability Schemas

#### `ExposureCapabilities`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `capability` | string | yes | fixed `camera.exposure` | none | capability 名称。 |
| `modes` | string[] | yes | exposure modes | none | 支持模式。 |
| `fields` | object | yes | descriptors | none | 字段范围、默认值和单位。 |
| `modeDependencies` | object | no | field -> modes | omitted | 哪些字段在哪些 mode 可写。 |
| `events` | string[] | no | event names | omitted | 支持事件。 |

### 6.4 Config / State 总结构

#### `ExposureConfig`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `mode` | string enum | yes | `auto`, `manual`, `auto_iso`, `auto_shutter` candidate | none | 曝光模式。 |
| `iso` | uint32 | no | capability range | omitted | ISO 值；与 `gain` 关系待确认。 |
| `gain` | number | no | capability range | omitted | sensor gain，单位待确认。 |
| `shutterUs` | uint32 | no | microseconds candidate | omitted | 快门时间候选单位。 |
| `ev` | int16 | no | capability range | omitted | 曝光补偿。 |
| `wdr` | bool | no | true/false | omitted | WDR 开关。 |
| `powerLineFrequency` | string enum | no | `auto`, `50hz`, `60hz`, `off` | omitted | 防频闪/工频。 |
| `version` | uint32 | no | monotonically increasing | omitted | 配置版本。 |

## 7. JSON 示例

### 7.1 场景：设置手动曝光

```json
{
  "id": 1,
  "method": "camera.setExposureConfig",
  "params": {
    "cameraId": "main",
    "config": { "mode": "manual", "iso": 400, "shutterUs": 8333, "ev": 0 }
  }
}
```

```json
{
  "id": 1,
  "status": { "ok": true, "code": 0 },
  "result": {
    "applyState": "applied",
    "config": { "mode": "manual", "iso": 400, "shutterUs": 8333, "ev": 0, "version": 5 }
  }
}
```

### 7.2 场景：事件通知

```json
{
  "event": "camera.exposureConfigChanged",
  "data": {
    "cameraId": "main",
    "changedFields": ["mode", "iso", "shutterUs"],
    "config": { "mode": "manual", "iso": 400, "shutterUs": 8333, "version": 5 },
    "applyState": "applied",
    "reason": "user_request"
  }
}
```

### 7.3 场景：自动模式下写手动字段失败

```json
{
  "id": 2,
  "status": {
    "ok": false,
    "code": 263,
    "msg": "Device mode conflict.",
    "details": { "field": "shutterUs", "requiredMode": "manual", "candidateError": "DEVICE_MODE_CONFLICT" }
  }
}
```

## 8. 错误

| 错误 | 适用场景 | 说明 |
|---|---|---|
| `NOT_SUPPORTED` | mode 或字段不支持。 | 返回 capability detail。 |
| `INVALID_ARGUMENT` | 字段组合非法。 | 例如 `iso` 与 `gain` 不可同时写。 |
| `OUT_OF_RANGE` | ISO/shutter/EV 越界。 | 返回范围。 |
| `DEVICE_MODE_CONFLICT` | mode 下字段不可写。 | 返回 required mode。 |
| `BUSY` | sensor pipeline 正忙。 | 稍后重试。 |

## 9. Legacy 映射

| legacy 项 | 候选映射 | 状态 | 说明 |
|---|---|---|---|
| `CommonSetPowerLineFreq` / `CommonGetPowerLineFreq` | `powerLineFrequency` | candidate | 枚举映射待确认。 |
| `CommonSetWdrState` / `CommonGetWdrState` | `wdr` | candidate | WDR 当前候选归 exposure。 |
| VM33 `Exposure.Mode` | `mode` | candidate | mode 枚举待确认。 |
| VM33 `Exposure.ExposureValue` | `ev` 或 sensor sensitivity | `[REVIEW-ASK]` | 注释和命名不完全一致。 |


## 10. 测试要点

| 类型 | 要点 |
|---|---|
| happy path | 查询能力，设置 manual exposure，事件同步。 |
| error path | auto mode 下写 shutter、ISO 越界、unsupported WDR。 |
| boundary case | shutter/EV/ISO min/max、partial update、reset。 |
| capability discovery | UI 根据 modeDependencies 启禁字段。 |
| event | set/reset/auto algorithm/profile 变化均同步。 |

## 11. 待确认问题

| 问题 | 影响 | 当前建议 | 状态 |
|---|---|---|---|
| ISO 与 gain 是否同字段？ | 影响 schema。 | 先保留二者，采纳前确认互斥或别名。 | `[REVIEW-ASK]` |
| shutter 单位是什么？ | 影响单位与兼容。 | 候选使用 `shutterUs`。 | `[REVIEW-ASK]` |
| WDR 是否归 exposure？ | 影响 feature 边界。 | 当前归 exposure，若产品认为是 enhancement 再拆。 | `[REVIEW-DRAFT]` |
