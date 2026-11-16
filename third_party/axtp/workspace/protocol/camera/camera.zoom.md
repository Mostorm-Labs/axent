---
status: draft
contract: false
generated: false
domain: camera
feature: camera.zoom
registry:
lastReviewed: 2026-06-13
---

# camera.zoom

## 0. 速读结论

| 项目 | 内容 |
|---|---|
| 这个能力做什么 | 管理摄像头 optical/digital zoom 的倍率、位置、速度、移动状态和事件。 |
| 当前状态 | draft；由 `workspace/business/camera-lens-control.md` 和 `workspace/flows/camera-lens-control.md` 转入。 |
| 是否可直接实现 | 否。草案阶段仅供评审；正式实现以 registry / generated 为准。 |
| 主要交互 | RPC + EVENT |
| 是否使用 STREAM | 否 |
| Registry readiness | candidate；zoom type、单位、`CommonSetPanTiltZoom` 拆分仍需确认。 |
| Conformance | needed；需覆盖 capability、set ratio/position、move/stop、event 和 unsupported。 |
| 主要未决问题 | optical/digital/default zoom 如何表达；ratio 与 device position 的单位需固件确认。 |

## 1. 功能说明

`camera.zoom` 用于摄像头变焦控制。它独立于 `camera.ptz` 的物理 pan/tilt，也独立于 `video.framing` 的电子裁切构图。若设备历史协议将 pan/tilt/zoom 合在同一命令中，采纳前必须拆分正式语义。

## 2. 能力边界

| 类型 | 内容 |
|---|---|
| 包含 | optical zoom、digital zoom、zoom ratio / position、zoom speed、连续 zoom move、zoom state event、数字 zoom 区域候选。 |
| 不包含 | 物理 pan/tilt、PTZ preset、focus/autofocus、framing mode、视频编码和 STREAM 数据。 |
| 数据面 | 不使用 STREAM。 |

## 3. 方法 Methods

### 3.0 方法速览

| Method | 调用类型 | 用途 | Params Schema | Result Schema | 是否触发事件 | 状态 |
|---|---|---|---|---|---|---|
| `camera.getZoomCapabilities` | query | 查询 zoom 类型、范围、速度和单位。 | `GetZoomCapabilitiesParams` | `ZoomCapabilities` | 否 | `[REVIEW-DRAFT]` |
| `camera.getZoomState` | query | 查询当前 zoom 倍率/位置和运动态。 | `GetZoomStateParams` | `ZoomState` | 否 | `[REVIEW-DRAFT]` |
| `camera.setZoomConfig` | command | 设置 zoom ratio/position/type/region。 | `SetZoomConfigParams` | `ZoomCommandResult` | 是 | `[REVIEW-DRAFT]` |
| `camera.startZoomMove` | command | 按 in/out 方向连续变焦。 | `StartZoomMoveParams` | `ZoomCommandResult` | 是 | `[REVIEW-ASK]` |
| `camera.stopZoomMove` | command | 停止连续变焦。 | `StopZoomMoveParams` | `ZoomCommandResult` | 是 | `[REVIEW-ASK]` |
| `camera.resetZoomConfig` | command | 恢复 zoom 默认值。 | `ResetZoomConfigParams` | `ZoomCommandResult` | 是 | `[REVIEW-DRAFT]` |

### 3.1 `camera.getZoomCapabilities`

| 项 | 内容 |
|---|---|
| 调用类型 | query |
| Params Schema | `GetZoomCapabilitiesParams` |
| Result Schema | `ZoomCapabilities` |
| 是否触发事件 | 否 |
| 幂等性 / 异步性 | 幂等；同步返回。 |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_ARGUMENT`, `PERMISSION_DENIED`, `UNAVAILABLE` |

#### 3.1.1 请求参数 Params：`GetZoomCapabilitiesParams`

#### 3.1.2 返回结果 Result：`ZoomCapabilities`

#### 3.1.3 d block 示例

request:

```json
{
  "id": 101,
  "method": "camera.getZoomCapabilities",
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
    "capability": "camera.zoom",
    "zoomTypes": 1.5,
    "regionZoom": 1.5
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
| `NOT_SUPPORTED` | 设备无可控 zoom。 | 隐藏控件。 |

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

### 3.2 `camera.getZoomState`

| 项 | 内容 |
|---|---|
| 调用类型 | query |
| Params Schema | `GetZoomStateParams` |
| Result Schema | `ZoomState` |
| 是否触发事件 | 否 |
| 幂等性 / 异步性 | 幂等。 |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_ARGUMENT`, `PERMISSION_DENIED`, `UNAVAILABLE` |

#### 3.2.1 请求参数 Params：`GetZoomStateParams`

#### 3.2.2 返回结果 Result：`ZoomState`

#### 3.2.3 d block 示例

request:

```json
{
  "id": 102,
  "method": "camera.getZoomState",
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
    "zoom": 1.5,
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
| `UNAVAILABLE` | zoom 状态不可读。 | 返回 unavailable detail。 |

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

### 3.3 `camera.setZoomConfig`

| 项 | 内容 |
|---|---|
| 调用类型 | command |
| Params Schema | `SetZoomConfigParams` |
| Result Schema | `ZoomCommandResult` |
| 是否触发事件 | 是，状态实际变化后触发 `camera.zoomStateChanged`。 |
| 幂等性 / 异步性 | 设置同一倍率应成功；变焦可能异步。 |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_ARGUMENT`, `OUT_OF_RANGE`, `DEVICE_MODE_CONFLICT`, `BUSY`, `PERMISSION_DENIED` |

#### 3.3.1 请求参数 Params：`SetZoomConfigParams`

#### 3.3.2 返回结果 Result：`ZoomCommandResult`

#### 3.3.3 d block 示例

request:

```json
{
  "id": 103,
  "method": "camera.setZoomConfig",
  "params": {
    "cameraId": "main"
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
      "zoom": 1.5,
      "moving": false
    }
  }
}
```

#### 3.3.4 可能触发的事件

| Event | 触发条件 | Payload Schema | 客户端处理建议 |
|---|---|---|---|
| `camera.zoomStateChanged` | ratio/position/type/state 变化。 | `ZoomStateChangedEvent` | 更新 zoom slider。 |

#### 3.3.5 错误

| 错误 | 场景 | 返回建议 |
|---|---|---|
| `OUT_OF_RANGE` | ratio 或 position 超范围。 | 返回合法范围。 |
| `DEVICE_MODE_CONFLICT` | framing 或 PTZ preset 正在占用 lens。 | 返回 owner。 |

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

### 3.4 `camera.startZoomMove`

| 项 | 内容 |
|---|---|
| 调用类型 | command |
| Params Schema | `StartZoomMoveParams` |
| Result Schema | `ZoomCommandResult` |
| 是否触发事件 | 是 |
| 幂等性 / 异步性 | 非幂等；必须由 `stopZoomMove` 或 timeout 结束。 |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_ARGUMENT`, `DEVICE_MODE_CONFLICT`, `BUSY`, `PERMISSION_DENIED` |

#### 3.4.1 请求参数 Params：`StartZoomMoveParams`

#### 3.4.2 返回结果 Result：`ZoomCommandResult`

#### 3.4.3 d block 示例

request:

```json
{
  "id": 104,
  "method": "camera.startZoomMove",
  "params": {
    "cameraId": "main",
    "direction": "in",
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
      "zoom": 1.5,
      "moving": false
    }
  }
}
```

#### 3.4.4 可能触发的事件

| Event | 触发条件 | Payload Schema | 客户端处理建议 |
|---|---|---|---|
| `camera.zoomStateChanged` | zoom 进入 moving。 | `ZoomStateChangedEvent` | UI 显示移动中。 |

#### 3.4.5 错误

| 错误 | 场景 | 返回建议 |
|---|---|---|
| `INVALID_ARGUMENT` | direction/speed 非法。 | 返回字段路径。 |

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

### 3.5 `camera.stopZoomMove`

| 项 | 内容 |
|---|---|
| 调用类型 | command |
| Params Schema | `StopZoomMoveParams` |
| Result Schema | `ZoomCommandResult` |
| 是否触发事件 | 是 |
| 幂等性 / 异步性 | 幂等；已停止时成功。 |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_ARGUMENT`, `PERMISSION_DENIED`, `UNAVAILABLE` |

#### 3.5.1 请求参数 Params：`StopZoomMoveParams`

#### 3.5.2 返回结果 Result：`ZoomCommandResult`

#### 3.5.3 d block 示例

request:

```json
{
  "id": 105,
  "method": "camera.stopZoomMove",
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
      "zoom": 1.5,
      "moving": false
    }
  }
}
```

#### 3.5.4 可能触发的事件

| Event | 触发条件 | Payload Schema | 客户端处理建议 |
|---|---|---|---|
| `camera.zoomStateChanged` | zoom 停止。 | `ZoomStateChangedEvent` | 释放按下态。 |

#### 3.5.5 错误

| 错误 | 场景 | 返回建议 |
|---|---|---|
| `UNAVAILABLE` | zoom 控制不可用。 | 清除移动 UI。 |

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

### 3.6 `camera.resetZoomConfig`

| 项 | 内容 |
|---|---|
| 调用类型 | command |
| Params Schema | `ResetZoomConfigParams` |
| Result Schema | `ZoomCommandResult` |
| 是否触发事件 | 是 |
| 幂等性 / 异步性 | 幂等；可同步或异步。 |
| 常见错误 | `NOT_SUPPORTED`, `BUSY`, `PERMISSION_DENIED` |

#### 3.6.1 请求参数 Params：`ResetZoomConfigParams`

#### 3.6.2 返回结果 Result：`ZoomCommandResult`

#### 3.6.3 d block 示例

request:

```json
{
  "id": 106,
  "method": "camera.resetZoomConfig",
  "params": {
    "cameraId": "main"
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
      "zoom": 1.5,
      "moving": false
    }
  }
}
```

#### 3.6.4 可能触发的事件

| Event | 触发条件 | Payload Schema | 客户端处理建议 |
|---|---|---|---|
| `camera.zoomStateChanged` | 默认值不同于当前值。 | `ZoomStateChangedEvent` | 更新 slider。 |

#### 3.6.5 错误

| 错误 | 场景 | 返回建议 |
|---|---|---|
| `BUSY` | zoom 正在移动。 | 先 stop 或稍后重试。 |

#### 3.6.6 错误 d block 示例

```json
{
  "id": 106,
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
| `camera.zoomStateChanged` | zoom ratio/position/type/state 变化。 | `ZoomStateChangedEvent` | 更新 zoom UI 或调用 get 校准。 | `[REVIEW-DRAFT]` |

### 4.1 `camera.zoomStateChanged`

#### Payload：`ZoomStateChangedEvent`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `cameraId` | string | no | device-defined | `main` | 摄像头对象。 |
| `state` | `ZoomState` | yes | see schema | none | 当前 zoom 状态。 |
| `changedFields` | string[] | no | field path array | omitted | 变化字段。 |
| `reason` | string enum | no | `user_request`, `preset`, `framing_algorithm`, `physical_control`, `unknown` | `unknown` | 变化原因。 |

#### d block 示例

```json
{
  "event": "camera.zoomStateChanged",
  "intent": 1,
  "data": {
    "state": {
      "cameraId": "main",
      "zoom": 1.5,
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
| 完整状态 | 直接更新 slider。 |
| event 丢失或重连 | 调用 `camera.getZoomState`。 |

## 5. Capability

| 能力字段 | 类型 | 必填 | 取值范围 / 枚举 | 说明 |
|---|---|---:|---|---|
| `capability` | string | yes | fixed `camera.zoom` | capability 名称。 |
| `zoomTypes` | string[] | yes | `optical`, `digital`, `hybrid`, `default` | 支持的 zoom 类型。 |
| `ratioRange` | object | no | min/max/step/unit | 倍率范围。 |
| `positionRange` | object | no | device steps | 原生位置范围。 |
| `speedRange` | object | no | min/max/step | 速度范围。 |
| `regionZoom` | bool | no | true/false | 是否支持数字 zoom 区域。 |

## 6. 字段 / Schemas

### 6.1 Schema 层级速览

| 层级 | 用在哪里 | 作用 |
|---|---|---|
| `ZoomCapabilities` | get capabilities result | 描述 zoom 类型、范围和速度。 |
| `ZoomState` | get state/event | 描述当前倍率/位置和运动态。 |
| `ZoomCommandResult` | set/move/reset result | 描述动作接受和当前状态。 |

### 6.2 请求与响应 Schemas

#### `GetZoomCapabilitiesParams` / `GetZoomStateParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `cameraId` | string | no | device-defined | `main` | 摄像头对象。 |

#### `SetZoomConfigParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `cameraId` | string | no | device-defined | `main` | 摄像头对象。 |
| `zoomType` | string enum | no | `optical`, `digital`, `hybrid`, `default` | `default` | 变焦路径。 |
| `ratio` | number | no | capability range | omitted | 目标倍率，例如 `2.0`。 |
| `position` | int32 | no | device steps | omitted | 原生变焦位置。 |
| `speed` | uint8 | no | capability range | omitted | 速度。 |
| `region` | object | no | normalized rect | omitted | 数字 zoom 区域。 |

#### `StartZoomMoveParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `cameraId` | string | no | device-defined | `main` | 摄像头对象。 |
| `direction` | string enum | yes | `in`, `out` | none | 变焦方向。 |
| `zoomType` | string enum | no | capability values | `default` | 变焦路径。 |
| `speed` | uint8 | no | capability range | default | 速度。 |
| `timeoutMs` | uint16 | no | capability range | device default | 安全超时。 |

#### `StopZoomMoveParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `cameraId` | string | no | device-defined | `main` | 摄像头对象。 |
| `operationId` | string | no | device-defined | omitted | 要停止的操作。 |

#### `ResetZoomConfigParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `cameraId` | string | no | device-defined | `main` | 摄像头对象。 |
| `scope` | string enum | no | `all`, `ratio`, `digital_region`, `speed` | `all` | reset 范围。 |

#### `ZoomCommandResult`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `accepted` | bool | yes | true/false | none | 动作是否接受。 |
| `operationId` | string | no | device-defined | omitted | 异步操作 ID。 |
| `state` | `ZoomState` | yes | see schema | none | 当前状态。 |
| `applyState` | string enum | no | `applied`, `moving`, `failed` | omitted | 应用状态。 |

### 6.3 Capability Schemas

#### `ZoomCapabilities`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `capability` | string | yes | fixed `camera.zoom` | none | capability 名称。 |
| `zoomTypes` | string[] | yes | `optical`, `digital`, `hybrid`, `default` | none | 支持类型。 |
| `ratioRange` | object | no | min/max/step/unit | omitted | 倍率范围。 |
| `positionRange` | object | no | min/max/step/unit | omitted | 原生位置范围。 |
| `speedRange` | object | no | min/max/step | omitted | 速度范围。 |
| `regionZoom` | bool | no | true/false | false | 数字区域 zoom 支持。 |
| `events` | string[] | no | event names | omitted | 支持事件。 |

### 6.4 Config / State 总结构

#### `ZoomState`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `cameraId` | string | no | device-defined | `main` | 摄像头对象。 |
| `zoomType` | string enum | yes | capability values | none | 当前变焦类型。 |
| `ratio` | number | no | capability range | omitted | 当前倍率。 |
| `position` | int32 | no | device steps | omitted | 当前原生位置。 |
| `motionState` | string enum | yes | `idle`, `moving`, `limited`, `unavailable` | none | 运动态。 |
| `controlOwner` | string enum | no | `user`, `preset`, `framing_algorithm`, `physical_control`, `unknown` | `unknown` | owner。 |

## 7. JSON 示例

### 7.1 场景：设置 optical zoom 倍率

```json
{
  "id": 1,
  "method": "camera.setZoomConfig",
  "params": { "cameraId": "main", "zoomType": "optical", "ratio": 2.0, "speed": 50 }
}
```

```json
{
  "id": 1,
  "status": { "ok": true, "code": 0 },
  "result": {
    "accepted": true,
    "applyState": "moving",
    "state": { "cameraId": "main", "zoomType": "optical", "ratio": 1.8, "motionState": "moving" }
  }
}
```

### 7.2 场景：状态事件

```json
{
  "event": "camera.zoomStateChanged",
  "data": {
    "cameraId": "main",
    "state": { "zoomType": "optical", "ratio": 2.0, "motionState": "idle" },
    "reason": "user_request"
  }
}
```

### 7.3 场景：失败响应

```json
{
  "id": 2,
  "status": {
    "ok": false,
    "code": 11,
    "msg": "Argument is out of range.",
    "details": { "field": "ratio", "min": 1.0, "max": 4.0, "candidateError": "OUT_OF_RANGE" }
  }
}
```

## 8. 错误

| 错误 | 适用场景 | 说明 |
|---|---|---|
| `NOT_SUPPORTED` | 不支持 zoomType 或移动方式。 | 返回 unsupported detail。 |
| `INVALID_ARGUMENT` | ratio/position 同时设置且设备不支持。 | 返回字段路径。 |
| `OUT_OF_RANGE` | 倍率、位置或速度越界。 | 返回合法范围。 |
| `DEVICE_MODE_CONFLICT` | 被 preset/framing/其他客户端占用。 | 返回 owner。 |
| `BUSY` | 正在变焦。 | 稍后重试或 stop。 |

## 9. Legacy 映射

| legacy 项 | 候选映射 | 状态 | 说明 |
|---|---|---|---|
| `Focus.ManualZoom` | `camera.startZoomMove` / `camera.stopZoomMove` | candidate | `Dir/Speed/Enable` 映射待确认。 |
| `Focus.SetZoomSpeedMode` / `Focus.GetZoomSpeedMode` | `speedRange` / speed config | candidate | SpeedMode 枚举待确认。 |
| `Focus.DigitalZoom` | `camera.setZoomConfig(zoomType=digital, region)` | candidate | 区域坐标和 base 单位待确认。 |
| `Focus.OpticsZoom` | `camera.setZoomConfig(zoomType=optical)` | candidate | base 与 ratio/position 映射待确认。 |
| `Focus.GetZoomInfo` | `camera.getZoomState` | candidate | `digitalBase` / `opticsBase` 字段映射待确认。 |
| `CommonSetPanTiltZoom` / `CommonGetPanTiltZoom` | `camera.zoom` + `camera.ptz` 拆分候选 | `[REVIEW-ASK]` | 名称含 PTZ，不能直接全部归 zoom。 |


## 10. 测试要点

| 类型 | 要点 |
|---|---|
| happy path | 查询能力，设置倍率，收到 moving/idle 事件。 |
| error path | unsupported zoomType、ratio 越界、owner conflict。 |
| boundary case | min/max ratio、重复 set、start 后 stop、timeout。 |
| capability discovery | UI slider 和 zoom type 与 capability 一致。 |
| event | set/start/stop/reset 都能同步最终状态。 |

## 11. 待确认问题

| 问题 | 影响 | 当前建议 | 状态 |
|---|---|---|---|
| `optical` / `digital` / `hybrid` / `default` 枚举是否正确？ | 影响正式 schema。 | 保留候选枚举。 | `[REVIEW-ASK]` |
| ratio 与 position 是否二选一？ | 影响 set 参数校验。 | 两者都可选，但设备可按 capability 限制。 | `[REVIEW-DRAFT]` |
| 是否需要 start/stop zoom move？ | 影响按钮长按体验。 | 推荐保留。 | `[REVIEW-ASK]` |
