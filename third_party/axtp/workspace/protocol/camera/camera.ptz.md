---
status: draft
contract: false
generated: false
domain: camera
feature: camera.ptz
registry:
lastReviewed: 2026-06-13
---

# camera.ptz

## 0. 速读结论

| 项目 | 内容 |
|---|---|
| 这个能力做什么 | 管理物理 PTZ pan/tilt 移动、位置状态、home/reset、preset 和算法接管状态。 |
| 当前状态 | draft；由 `workspace/business/camera-lens-control.md` 和 `workspace/flows/camera-lens-control.md` 转入。 |
| 是否可直接实现 | 否。草案阶段仅供评审；正式实现以 registry / generated 为准。 |
| 主要交互 | RPC + EVENT |
| 是否使用 STREAM | 否 |
| Registry readiness | candidate；movement / stop / preset 是否作为独立方法仍需确认。 |
| Conformance | needed；需覆盖 capability、move/stop、preset、owner conflict 和 event。 |
| 主要未决问题 | `CommonSetPanTiltZoom` 是否拆分到 PTZ+zoom，preset 是否包含 zoom/focus/framing。 |

## 1. 功能说明

`camera.ptz` 用于控制摄像头物理云台 pan/tilt、查询运动状态、处理限位和保存/调用 PTZ preset。它可记录 `controlOwner`，用来表达用户、遥控器、framing 算法或其他客户端对物理云台的占用。

## 2. 能力边界

| 类型 | 内容 |
|---|---|
| 包含 | 物理 pan/tilt 能力、绝对/相对/连续移动、停止移动、home/reset、PTZ preset、限位、状态事件、控制 owner。 |
| 不包含 | 独立 optical/digital zoom 倍率配置、focus/autofocus、电子裁切 PTZ、framing 算法策略、视频流数据。 |
| 数据面 | 不使用 STREAM；所有交互为 RPC 和 EVENT。 |

## 3. 方法 Methods

方法 ID、bitOffset 和 fieldId 均在采纳进入 registry 时分配正式数值。

### 3.0 方法速览

| Method | 调用类型 | 用途 | Params Schema | Result Schema | 是否触发事件 | 状态 |
|---|---|---|---|---|---|---|
| `camera.getPtzCapabilities` | query | 查询 PTZ 范围、速度、move 类型和 preset 支持。 | `GetPtzCapabilitiesParams` | `PtzCapabilities` | 否 | `[REVIEW-DRAFT]` |
| `camera.getPtzState` | query | 查询当前位置、运动状态、owner 和限位。 | `GetPtzStateParams` | `PtzState` | 否 | `[REVIEW-DRAFT]` |
| `camera.setPtzConfig` | command | 设置绝对/相对 PTZ 目标或 home/reset。 | `SetPtzConfigParams` | `PtzCommandResult` | 是 | `[REVIEW-DRAFT]` |
| `camera.startPtzMove` | command | 连续移动，适合方向键按住场景。 | `StartPtzMoveParams` | `PtzCommandResult` | 是 | `[REVIEW-DRAFT]` |
| `camera.stopPtzMove` | command | 停止连续移动。 | `StopPtzMoveParams` | `PtzCommandResult` | 是 | `[REVIEW-DRAFT]` |
| `camera.callPtzPreset` | command | 调用已保存 preset。 | `CallPtzPresetParams` | `PtzCommandResult` | 是 | `[REVIEW-DRAFT]` |
| `camera.savePtzPreset` | command | 保存当前 PTZ 为 preset。 | `SavePtzPresetParams` | `PtzPreset` | 否/可选 | `[REVIEW-DRAFT]` |

### 3.1 `camera.getPtzCapabilities`

| 项 | 内容 |
|---|---|
| 调用类型 | query |
| Params Schema | `GetPtzCapabilitiesParams` |
| Result Schema | `PtzCapabilities` |
| 是否触发事件 | 否 |
| 幂等性 / 异步性 | 幂等；同步返回能力快照。 |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_ARGUMENT`, `PERMISSION_DENIED`, `UNAVAILABLE` |

#### 3.1.1 请求参数 Params：`GetPtzCapabilitiesParams`

#### 3.1.2 返回结果 Result：`PtzCapabilities`

#### 3.1.3 d block 示例

request:

```json
{
  "id": 101,
  "method": "camera.getPtzCapabilities",
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
    "capability": "camera.ptz",
    "continuousMove": true
  }
}
```

#### 3.1.4 可能触发的事件

| Event | 触发条件 | Payload Schema | 客户端处理建议 |
|---|---|---|---|
| 无 | 查询不改变状态。 | none | 无需处理。 |

#### 3.1.5 错误

| 错误 | 场景 | 返回建议 |
|---|---|---|
| `NOT_SUPPORTED` | 设备无物理 PTZ。 | 隐藏 PTZ 控件。 |
| `UNAVAILABLE` | 云台服务不可读。 | 稍后重试。 |

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

### 3.2 `camera.getPtzState`

| 项 | 内容 |
|---|---|
| 调用类型 | query |
| Params Schema | `GetPtzStateParams` |
| Result Schema | `PtzState` |
| 是否触发事件 | 否 |
| 幂等性 / 异步性 | 幂等；同步返回状态。 |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_ARGUMENT`, `PERMISSION_DENIED`, `UNAVAILABLE` |

#### 3.2.1 请求参数 Params：`GetPtzStateParams`

#### 3.2.2 返回结果 Result：`PtzState`

#### 3.2.3 d block 示例

request:

```json
{
  "id": 102,
  "method": "camera.getPtzState",
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
    "pan": 0,
    "tilt": 0,
    "zoom": 1.0,
    "moving": false
  }
}
```

#### 3.2.4 可能触发的事件

| Event | 触发条件 | Payload Schema | 客户端处理建议 |
|---|---|---|---|
| 无 | 查询不改变状态。 | none | 无需处理。 |

#### 3.2.5 错误

| 错误 | 场景 | 返回建议 |
|---|---|---|
| `UNAVAILABLE` | 云台状态未知。 | 返回状态不可用原因。 |

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

### 3.3 `camera.setPtzConfig`

| 项 | 内容 |
|---|---|
| 调用类型 | command |
| Params Schema | `SetPtzConfigParams` |
| Result Schema | `PtzCommandResult` |
| 是否触发事件 | 是，位置或状态变化后触发 `camera.ptzStateChanged`。 |
| 幂等性 / 异步性 | 目标位置相同应成功；移动可能异步。 |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_ARGUMENT`, `OUT_OF_RANGE`, `DEVICE_MODE_CONFLICT`, `BUSY`, `PERMISSION_DENIED` |

#### 3.3.1 请求参数 Params：`SetPtzConfigParams`

`mode` 可为 `absolute`、`relative`、`home` 或 `reset`。

#### 3.3.2 返回结果 Result：`PtzCommandResult`

#### 3.3.3 d block 示例

request:

```json
{
  "id": 103,
  "method": "camera.setPtzConfig",
  "params": {
    "cameraId": "main",
    "mode": "auto"
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
    "accepted": true,
    "state": {
      "cameraId": "main",
      "pan": 0,
      "tilt": 0,
      "zoom": 1.0,
      "moving": false
    }
  }
}
```

#### 3.3.4 可能触发的事件

| Event | 触发条件 | Payload Schema | 客户端处理建议 |
|---|---|---|---|
| `camera.ptzStateChanged` | 位置、运动状态、owner 或限位变化。 | `PtzStateChangedEvent` | 更新方向控件和状态。 |

#### 3.3.5 错误

| 错误 | 场景 | 返回建议 |
|---|---|---|
| `OUT_OF_RANGE` | pan/tilt 超出范围。 | 返回合法范围。 |
| `DEVICE_MODE_CONFLICT` | framing 算法或其他 owner 正在控制云台。 | 返回当前 owner。 |
| `BUSY` | 云台正在执行不可打断动作。 | 稍后重试或先 stop。 |

#### 3.3.6 错误 d block 示例

```json
{
  "id": 103,
  "status": {
    "ok": false,
    "code": 10,
    "msg": "Invalid argument.",
    "details": {
      "candidateError": "OUT_OF_RANGE",
      "field": "cameraId",
      "reason": "value is outside supported range"
    }
  }
}
```

### 3.4 `camera.startPtzMove`

| 项 | 内容 |
|---|---|
| 调用类型 | command |
| Params Schema | `StartPtzMoveParams` |
| Result Schema | `PtzCommandResult` |
| 是否触发事件 | 是 |
| 幂等性 / 异步性 | 非幂等动作；必须有超时保护。 |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_ARGUMENT`, `DEVICE_MODE_CONFLICT`, `BUSY`, `PERMISSION_DENIED` |

#### 3.4.1 请求参数 Params：`StartPtzMoveParams`

#### 3.4.2 返回结果 Result：`PtzCommandResult`

#### 3.4.3 d block 示例

request:

```json
{
  "id": 104,
  "method": "camera.startPtzMove",
  "params": {
    "cameraId": "main",
    "direction": "left",
    "timeoutMs": 5000
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
    "accepted": true,
    "state": {
      "cameraId": "main",
      "pan": 0,
      "tilt": 0,
      "zoom": 1.0,
      "moving": false
    }
  }
}
```

#### 3.4.4 可能触发的事件

| Event | 触发条件 | Payload Schema | 客户端处理建议 |
|---|---|---|---|
| `camera.ptzStateChanged` | PTZ 进入 `moving`。 | `PtzStateChangedEvent` | UI 显示移动中。 |

#### 3.4.5 错误

| 错误 | 场景 | 返回建议 |
|---|---|---|
| `INVALID_ARGUMENT` | direction 或 speed 非法。 | 返回字段路径。 |

#### 3.4.6 错误 d block 示例

```json
{
  "id": 104,
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

### 3.5 `camera.stopPtzMove`

| 项 | 内容 |
|---|---|
| 调用类型 | command |
| Params Schema | `StopPtzMoveParams` |
| Result Schema | `PtzCommandResult` |
| 是否触发事件 | 是 |
| 幂等性 / 异步性 | 幂等；已经停止时应成功。 |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_ARGUMENT`, `PERMISSION_DENIED`, `UNAVAILABLE` |

#### 3.5.1 请求参数 Params：`StopPtzMoveParams`

#### 3.5.2 返回结果 Result：`PtzCommandResult`

#### 3.5.3 d block 示例

request:

```json
{
  "id": 105,
  "method": "camera.stopPtzMove",
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
    "accepted": true,
    "state": {
      "cameraId": "main",
      "pan": 0,
      "tilt": 0,
      "zoom": 1.0,
      "moving": false
    }
  }
}
```

#### 3.5.4 可能触发的事件

| Event | 触发条件 | Payload Schema | 客户端处理建议 |
|---|---|---|---|
| `camera.ptzStateChanged` | PTZ 停止或限位停止。 | `PtzStateChangedEvent` | UI 释放按下态。 |

#### 3.5.5 错误

| 错误 | 场景 | 返回建议 |
|---|---|---|
| `UNAVAILABLE` | 云台控制链路不可用。 | 清除移动 UI 状态。 |

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

### 3.6 `camera.callPtzPreset`

| 项 | 内容 |
|---|---|
| 调用类型 | command |
| Params Schema | `CallPtzPresetParams` |
| Result Schema | `PtzCommandResult` |
| 是否触发事件 | 是，移动开始/完成触发 state event。 |
| 幂等性 / 异步性 | 调用同一 preset 可重复；移动可能异步。 |
| 常见错误 | `NOT_FOUND`, `NOT_SUPPORTED`, `DEVICE_MODE_CONFLICT`, `BUSY`, `PERMISSION_DENIED` |

#### 3.6.1 请求参数 Params：`CallPtzPresetParams`

#### 3.6.2 返回结果 Result：`PtzCommandResult`

#### 3.6.3 d block 示例

request:

```json
{
  "id": 106,
  "method": "camera.callPtzPreset",
  "params": {
    "cameraId": "main",
    "presetId": "preset_1"
  }
}
```

success:

```json
{
  "id": 106,
  "status": {
    "ok": true,
    "code": 0
  },
  "result": {
    "accepted": true,
    "state": {
      "cameraId": "main",
      "pan": 0,
      "tilt": 0,
      "zoom": 1.0,
      "moving": false
    }
  }
}
```

#### 3.6.4 可能触发的事件

| Event | 触发条件 | Payload Schema | 客户端处理建议 |
|---|---|---|---|
| `camera.ptzStateChanged` | 调用 preset 导致位置变化。 | `PtzStateChangedEvent` | 更新位置和状态。 |

#### 3.6.5 错误

| 错误 | 场景 | 返回建议 |
|---|---|---|
| `NOT_FOUND` | preset 不存在。 | 刷新 preset 列表。 |

#### 3.6.6 错误 d block 示例

```json
{
  "id": 106,
  "status": {
    "ok": false,
    "code": 12,
    "msg": "Request failed.",
    "details": {
      "candidateError": "NOT_FOUND",
      "field": "cameraId",
      "reason": "resource not found"
    }
  }
}
```

### 3.7 `camera.savePtzPreset`

| 项 | 内容 |
|---|---|
| 调用类型 | command |
| Params Schema | `SavePtzPresetParams` |
| Result Schema | `PtzPreset` |
| 是否触发事件 | 可选；若 preset 列表订阅存在可触发列表变化事件。 |
| 幂等性 / 异步性 | 相同 `presetId` 可覆盖或冲突，策略待确认。 |
| 常见错误 | `INVALID_ARGUMENT`, `ALREADY_EXISTS`, `PERMISSION_DENIED`, `RESOURCE_EXHAUSTED` |

#### 3.7.1 请求参数 Params：`SavePtzPresetParams`

#### 3.7.2 返回结果 Result：`PtzPreset`

#### 3.7.3 d block 示例

request:

```json
{
  "id": 107,
  "method": "camera.savePtzPreset",
  "params": {
    "cameraId": "main",
    "include": [
      "pan",
      "tilt",
      "zoom"
    ]
  }
}
```

success:

```json
{
  "id": 107,
  "status": {
    "ok": true,
    "code": 0
  },
  "result": {
    "presetId": "preset_1",
    "ptz": {
      "cameraId": "main",
      "pan": 0,
      "tilt": 0,
      "zoom": 1.0,
      "moving": false
    },
    "include": [
      "pan",
      "tilt",
      "zoom"
    ]
  }
}
```

#### 3.7.4 可能触发的事件

| Event | 触发条件 | Payload Schema | 客户端处理建议 |
|---|---|---|---|
| `camera.ptzConfigChanged` | preset 列表变化。 | `PtzConfigChangedEvent` | preset event 是否独立保留见待确认问题。 |

#### 3.7.5 错误

| 错误 | 场景 | 返回建议 |
|---|---|---|
| `RESOURCE_EXHAUSTED` | preset 数量达到上限。 | 返回上限。 |

#### 3.7.6 错误 d block 示例

```json
{
  "id": 107,
  "status": {
    "ok": false,
    "code": 15,
    "msg": "Request failed.",
    "details": {
      "candidateError": "RESOURCE_EXHAUSTED",
      "field": "cameraId",
      "reason": "request cannot be applied"
    }
  }
}
```

## 4. 事件 Events

### 4.0 事件速览

| Event | 触发条件 | Payload Schema | 客户端处理建议 | 状态 |
|---|---|---|---|---|
| `camera.ptzStateChanged` | 位置、运动状态、owner、限位变化。 | `PtzStateChangedEvent` | 更新 PTZ UI；必要时 get 校准。 | `[REVIEW-DRAFT]` |
| `camera.ptzConfigChanged` | 默认速度、home、preset 配置变化。 | `PtzConfigChangedEvent` | 刷新能力或 preset 列表。 | `[REVIEW-DRAFT]` |

### 4.1 `camera.ptzStateChanged`

#### Payload：`PtzStateChangedEvent`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `cameraId` | string | no | device-defined | `main` | 摄像头对象。 |
| `state` | `PtzState` | yes | see schema | none | 当前 PTZ 状态。 |
| `changedFields` | string[] | no | field path array | omitted | 变化字段。 |
| `reason` | string enum | no | `user_request`, `preset`, `framing_algorithm`, `physical_control`, `limit_reached`, `unknown` | `unknown` | 变化原因。 |

#### d block 示例

```json
{
  "event": "camera.ptzStateChanged",
  "intent": 1,
  "data": {
    "state": {
      "cameraId": "main",
      "pan": 0,
      "tilt": 0,
      "zoom": 1.0,
      "moving": false
    },
    "changedFields": [
      "state"
    ],
    "reason": "user_request"
  }
}
```


#### 客户端处理建议

| 场景 | 建议 |
|---|---|
| payload 是完整状态 | 更新方向键、位置、owner、限位。 |
| event 丢失或重连 | 调用 `camera.getPtzState`。 |

### 4.2 `camera.ptzConfigChanged`

#### Payload：`PtzConfigChangedEvent`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `cameraId` | string | no | device-defined | `main` | 摄像头对象。 |
| `changedFields` | string[] | no | field path array | omitted | 变化字段。 |
| `reason` | string enum | no | `user_request`, `profile_changed`, `restore_config`, `unknown` | `unknown` | 变化原因。 |

#### d block 示例

```json
{
  "event": "camera.ptzConfigChanged",
  "intent": 1,
  "data": {
    "changedFields": [
      "state"
    ],
    "reason": "user_request"
  }
}
```


#### 客户端处理建议

| 场景 | 建议 |
|---|---|
| preset list changed | 重新查询 preset 列表。 |

## 5. Capability

| 能力字段 | 类型 | 必填 | 取值范围 / 枚举 | 说明 |
|---|---|---:|---|---|
| `capability` | string | yes | fixed `camera.ptz` | capability 名称。 |
| `pan` | object | no | min/max/step/unit | pan 轴能力。 |
| `tilt` | object | no | min/max/step/unit | tilt 轴能力。 |
| `continuousMove` | bool | no | true/false | 是否支持 start/stop 连续移动。 |
| `speedRange` | object | no | min/max/step | 移动速度范围。 |
| `preset` | object | no | maxCount, includes | preset 能力。 |
| `ownerReporting` | bool | no | true/false | 是否上报 control owner。 |

## 6. 字段 / Schemas

### 6.1 Schema 层级速览

| 层级 | 用在哪里 | 作用 |
|---|---|---|
| `PtzCapabilities` | get capabilities result | 描述范围、速度、preset 和事件。 |
| `PtzState` | get state result、state event | 描述当前位置、运动态、owner。 |
| `PtzCommandResult` | set/move/stop/preset result | 描述动作是否接受及当前状态。 |

### 6.2 请求与响应 Schemas

#### `GetPtzCapabilitiesParams` / `GetPtzStateParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `cameraId` | string | no | device-defined | `main` | 摄像头对象。 |

#### `SetPtzConfigParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `cameraId` | string | no | device-defined | `main` | 摄像头对象。 |
| `mode` | string enum | yes | `absolute`, `relative`, `home`, `reset` | none | PTZ 设置模式。 |
| `pan` | int32 | no | capability range | omitted | pan 目标或相对步进。 |
| `tilt` | int32 | no | capability range | omitted | tilt 目标或相对步进。 |
| `speed` | uint8 | no | capability range | omitted | 移动速度。 |

#### `StartPtzMoveParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `cameraId` | string | no | device-defined | `main` | 摄像头对象。 |
| `direction` | string enum | yes | `up`, `down`, `left`, `right`, `up_left`, `up_right`, `down_left`, `down_right` | none | 连续移动方向。 |
| `speed` | uint8 | no | capability range | default | 速度。 |
| `timeoutMs` | uint16 | no | capability range | device default | 安全超时。 |

#### `StopPtzMoveParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `cameraId` | string | no | device-defined | `main` | 摄像头对象。 |
| `operationId` | string | no | device-defined | omitted | 要停止的移动操作。 |

#### `CallPtzPresetParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `cameraId` | string | no | device-defined | `main` | 摄像头对象。 |
| `presetId` | string | yes | device-defined | none | preset 标识。 |

#### `SavePtzPresetParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `cameraId` | string | no | device-defined | `main` | 摄像头对象。 |
| `presetId` | string | no | device-defined | auto | preset 标识。 |
| `name` | string | no | max length TBD | omitted | preset 名称。 |
| `include` | string[] | no | `ptz`, `zoom`, `focus`, `framing` | `ptz` | preset 保存范围；是否允许包含非 PTZ 状态见待确认问题。 |

#### `PtzCommandResult`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `accepted` | bool | yes | true/false | none | 动作是否接受。 |
| `operationId` | string | no | device-defined | omitted | 异步动作标识。 |
| `state` | `PtzState` | yes | see schema | none | 当前状态。 |
| `applyState` | string enum | no | `applied`, `moving`, `failed` | omitted | 动作状态。 |

### 6.3 Capability Schemas

#### `PtzCapabilities`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `capability` | string | yes | fixed `camera.ptz` | none | capability 名称。 |
| `pan` | object | no | min/max/step/unit | omitted | pan 轴范围。 |
| `tilt` | object | no | min/max/step/unit | omitted | tilt 轴范围。 |
| `continuousMove` | bool | no | true/false | false | 是否支持连续移动。 |
| `preset` | object | no | maxCount, includes | omitted | preset 能力。 |
| `events` | string[] | no | event names | omitted | 支持事件。 |

### 6.4 Config / State 总结构

#### `PtzState`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `cameraId` | string | no | device-defined | `main` | 摄像头对象。 |
| `pan` | int32 | no | capability range | omitted | 当前 pan。 |
| `tilt` | int32 | no | capability range | omitted | 当前 tilt。 |
| `motionState` | string enum | yes | `idle`, `moving`, `homing`, `limited`, `unavailable` | none | 运动状态。 |
| `controlOwner` | string enum | no | `user`, `framing_algorithm`, `physical_control`, `remote_client`, `unknown` | `unknown` | 当前 owner。 |
| `limits` | string[] | no | `pan_min`, `pan_max`, `tilt_min`, `tilt_max` | omitted | 当前触发的限位。 |

### 6.5 各对象字段

#### `PtzPreset`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `presetId` | string | yes | device-defined | none | preset 标识。 |
| `name` | string | no | max length TBD | omitted | 显示名称。 |
| `ptz` | `PtzState` | yes | see schema | none | 保存的 PTZ 状态。 |
| `include` | string[] | no | `ptz`, `zoom`, `focus`, `framing` | `ptz` | 包含的状态类别，需评审。 |

## 7. JSON 示例

### 7.1 场景：方向键开始移动并停止

```json
{
  "id": 1,
  "method": "camera.startPtzMove",
  "params": { "cameraId": "main", "direction": "left", "speed": 60, "timeoutMs": 3000 }
}
```

```json
{
  "id": 1,
  "status": { "ok": true, "code": 0 },
  "result": {
    "accepted": true,
    "operationId": "ptz-move-7",
    "applyState": "moving",
    "state": { "cameraId": "main", "motionState": "moving", "controlOwner": "user" }
  }
}
```

```json
{
  "id": 2,
  "method": "camera.stopPtzMove",
  "params": { "cameraId": "main", "operationId": "ptz-move-7" }
}
```

### 7.2 场景：PTZ 状态事件

```json
{
  "event": "camera.ptzStateChanged",
  "data": {
    "cameraId": "main",
    "state": {
      "pan": -120,
      "tilt": 20,
      "motionState": "idle",
      "controlOwner": "user"
    },
    "reason": "user_request"
  }
}
```

### 7.3 场景：framing 算法占用导致失败

```json
{
  "id": 3,
  "status": {
    "ok": false,
    "code": 263,
    "msg": "Device mode conflict.",
    "details": { "owner": "framing_algorithm", "candidateError": "DEVICE_MODE_CONFLICT" }
  }
}
```

## 8. 错误

| 错误 | 适用场景 | 说明 |
|---|---|---|
| `NOT_SUPPORTED` | 无物理 PTZ、无 preset 或不支持连续移动。 | 返回 unsupported feature/method/field。 |
| `INVALID_ARGUMENT` | direction、presetId 或字段组合非法。 | 返回字段路径。 |
| `OUT_OF_RANGE` | pan/tilt/speed 超出范围。 | 返回能力范围。 |
| `DEVICE_MODE_CONFLICT` | framing 算法或其他控制端占用。 | 返回 owner。 |
| `BUSY` | 正在移动、归位或调用 preset。 | 稍后重试。 |

## 9. Legacy 映射

| legacy 项 | 候选映射 | 状态 | 说明 |
|---|---|---|---|
| `CommonGetPositionNumberJson` / `CommonSetPositionNumberJson` | preset 查询/保存候选 | `[REVIEW-ASK]` | 需确认 position number 是 PTZ preset 还是房间座位编号。 |
| `CommonSetPanTiltZoom` / `CommonGetPanTiltZoom` | `camera.ptz` + `camera.zoom` 拆分候选 | `[REVIEW-ASK]` | 名称含 pan/tilt/zoom；payload 单位和字段边界需确认。 |


## 10. 测试要点

| 类型 | 要点 |
|---|---|
| happy path | 查询能力，start move，stop move，状态事件同步。 |
| error path | PTZ 不支持、owner 冲突、限位、speed 超范围。 |
| boundary case | pan/tilt min/max、连续移动 timeout、重复 stop。 |
| capability discovery | UI 控件与 capability 完全一致。 |
| event | 移动开始/停止/限位/owner 变化均可同步。 |

## 11. 待确认问题

| 问题 | 影响 | 当前建议 | 状态 |
|---|---|---|---|
| 是否新增 `startPtzMove` / `stopPtzMove`？ | 决定方向键体验和 method 数量。 | 推荐保留动作型方法，`setPtzConfig` 只做目标设置。 | `[REVIEW-ASK]` |
| preset 是否包含 zoom/focus/framing？ | 决定 `PtzPreset.include` 范围。 | 默认只含 PTZ，其它作为可选扩展。 | `[REVIEW-ASK]` |
| 是否保留 `camera.ptzConfigChanged` 作为 preset/config 事件？ | 影响事件数量和客户端订阅。 | 如 preset 列表和 home/default speed 会变化，建议保留；否则可由 `ptzStateChanged` + get 校准。 | `[REVIEW-ASK]` |
| `CommonSetPanTiltZoom` 如何拆分？ | 影响 legacy adapter 和正式 schema。 | 暂不写入 YAML，先保留候选映射。 | `[REVIEW-ASK]` |
