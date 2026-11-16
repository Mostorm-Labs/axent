---
status: draft
contract: false
generated: false
domain: camera
feature: camera.focus
registry:
lastReviewed: 2026-06-15
---

# camera.focus

## 0. 速读结论

| 项目 | 内容 |
|---|---|
| 这个能力做什么 | 管理自动对焦、连续自动对焦、手动对焦位置、点选/区域对焦、focus jog 和状态事件。 |
| 当前状态 | draft；由 `workspace/flows/camera-lens-control.md` 收敛，仍需 registry review。 |
| 是否可直接实现 | 否。草案阶段仅供评审；正式实现以 registry / generated 为准。 |
| 主要交互 | RPC + EVENT |
| 是否使用 STREAM | 否 |
| Registry readiness | candidate；方法和事件轮廓已具备，ID/bitOffset/fieldId 待采纳分配。 |
| Conformance | needed；需覆盖 capabilities、mode、AF action、manual position、region、jog、state/config events 和 mode conflict。 |
| 主要未决问题 | VM33 `Focus.SetFocus` 是绝对位置还是触发 AF；`startFocusMove` 是否进入首批 registry。 |

## 1. 功能说明

`camera.focus` 用于普通运行时对焦控制。它覆盖对焦模式、一次性自动对焦、连续自动对焦、手动对焦位置、点选/区域对焦和可选 jog 移动。AF calibration 和工厂校准不属于本 feature。

本文保留 legacy 映射线索，但 legacy command 名称和旧 payload 不构成 runtime 实现合同。

## 2. 能力边界

| 类型 | 内容 |
|---|---|
| 包含 | focus capabilities、focus mode、manual focus position、auto focus target、focus region、trigger autofocus、focus jog、focus state/config event。 |
| 不包含 | zoom、PTZ、framing、AF calibration、产测校准、视频帧传输。 |
| 数据面 | 不使用 STREAM；所有操作均通过 RPC method/event 完成。 |

## 3. 方法 Methods

### 3.0 方法速览

| Method | 调用类型 | 用途 | Params Schema | Result Schema | 是否触发事件 | 状态 |
|---|---|---|---|---|---|---|
| `camera.getFocusCapabilities` | query | 查询 focus modes、position、region、manual move 能力。 | `GetFocusCapabilitiesParams` | `FocusCapabilities` | 否 | `[REVIEW-DRAFT]` |
| `camera.getFocusState` | query | 查询完整 focus state。 | `GetFocusStateParams` | `FocusState` | 否 | `[REVIEW-DRAFT]` |
| `camera.setFocusMode` | command | 切换 manual/auto/continuous/fixed 等模式。 | `SetFocusModeParams` | `FocusCommandResult` | 是，`camera.focusModeChanged`、可能 `camera.focusStateChanged` | `[REVIEW-DRAFT]` |
| `camera.getFocusMode` | query | 查询当前 focus mode。 | `GetFocusModeParams` | `FocusModeState` | 否 | `[REVIEW-DRAFT]` |
| `camera.setFocusPosition` | command | 设置绝对手动对焦位置。 | `SetFocusPositionParams` | `FocusCommandResult` | 是，`camera.focusPositionChanged`、可能 `camera.focusStateChanged` | `[REVIEW-DRAFT]` |
| `camera.getFocusPosition` | query | 查询当前手动对焦位置。 | `GetFocusPositionParams` | `FocusPositionState` | 否 | `[REVIEW-DRAFT]` |
| `camera.setFocusRegion` | command | 设置持久点选或区域 AF 目标。 | `SetFocusRegionParams` | `FocusCommandResult` | 是，`camera.focusRegionChanged` | `[REVIEW-DRAFT]` |
| `camera.getFocusRegion` | query | 查询当前 AF target / point / region。 | `GetFocusRegionParams` | `FocusRegionState` | 否 | `[REVIEW-DRAFT]` |
| `camera.triggerAutoFocus` | action | 触发一次自动对焦。 | `TriggerAutoFocusParams` | `FocusCommandResult` | 是，`camera.focusStateChanged` | `[REVIEW-DRAFT]` |
| `camera.startFocusMove` | command | near/far jog 移动。 | `StartFocusMoveParams` | `FocusCommandResult` | 是，`camera.focusStateChanged` | `[REVIEW-DRAFT]` |
| `camera.stopFocus` | command | 停止 AF、扫描或 jog。 | `StopFocusParams` | `FocusCommandResult` | 是，`camera.focusStateChanged` | `[REVIEW-DRAFT]` |

### 3.1 `camera.getFocusCapabilities`

**用途**：查询设备支持的 focus modes、manual position 范围、AF target 类型、jog 能力和事件能力。

| 项 | 内容 |
|---|---|
| 调用类型 | query |
| Params Schema | `GetFocusCapabilitiesParams` |
| Result Schema | `FocusCapabilities` |
| 是否触发事件 | 否 |
| 幂等性 / 异步性 | 幂等；同步返回能力快照。 |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_ARGUMENT`, `PERMISSION_DENIED`, `UNAVAILABLE` |

#### 3.1.1 请求参数 Params：`GetFocusCapabilitiesParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `cameraId` | string | no | device-defined | `main` | 摄像头对象。 |

#### 3.1.2 返回结果 Result：`FocusCapabilities`

#### 3.1.3 d block 示例

request:

```json
{
  "id": 101,
  "method": "camera.getFocusCapabilities",
  "params": {
    "cameraId": "main"
  }
}
```

读法：客户端用该请求决定 UI 是否显示 focus 控件、哪些模式可选、position 和 region 的合法范围。

success:

```json
{
  "id": 101,
  "status": {
    "ok": true,
    "code": 0
  },
  "result": {
    "capability": "camera.focus",
    "modes": [
      "manual",
      "auto",
      "continuous_auto"
    ],
    "defaultMode": "auto",
    "positionRange": {
      "min": 0,
      "max": 1023,
      "step": 1,
      "unit": "native_step"
    },
    "targets": [
      "center",
      "full_frame",
      "point",
      "region"
    ],
    "manualMove": {
      "directions": [
        "near",
        "far"
      ],
      "speedRange": {
        "min": 1,
        "max": 10,
        "step": 1
      }
    },
    "events": [
      "camera.focusStateChanged",
      "camera.focusModeChanged",
      "camera.focusPositionChanged",
      "camera.focusRegionChanged"
    ]
  }
}
```

读法：`positionRange` 和 `targets` 是后续 set/action 请求的合法边界；未声明的 mode/target 不应在 UI 中启用。

#### 3.1.4 可能触发的事件

| Event | 触发条件 | Payload Schema | 客户端处理建议 |
|---|---|---|---|
| 无 | 查询不改变状态。 | none | 无需处理。 |

#### 3.1.5 错误

| 错误 | 场景 | 返回建议 |
|---|---|---|
| `NOT_SUPPORTED` | 设备固定焦且无可控 focus。 | 返回 unsupported feature。 |
| `UNAVAILABLE` | camera service 尚未初始化。 | 返回可重试原因。 |


#### 3.1.6 规则

- Query method MUST NOT 因查询本身触发事件。
- `modes`、`targets`、`positionRange` 缺失时，客户端 MUST 认为对应控制不可用。

### 3.2 `camera.getFocusState`

**用途**：查询完整 focus state，用于页面初始化、重连后校准或事件丢失后的状态恢复。

| 项 | 内容 |
|---|---|
| 调用类型 | query |
| Params Schema | `GetFocusStateParams` |
| Result Schema | `FocusState` |
| 是否触发事件 | 否 |
| 幂等性 / 异步性 | 幂等；同步返回当前快照。 |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_ARGUMENT`, `PERMISSION_DENIED`, `UNAVAILABLE` |

#### 3.2.1 请求参数 Params：`GetFocusStateParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `cameraId` | string | no | device-defined | `main` | 摄像头对象。 |

#### 3.2.2 返回结果 Result：`FocusState`

#### 3.2.3 d block 示例

request:

```json
{
  "id": 102,
  "method": "camera.getFocusState",
  "params": {
    "cameraId": "main"
  }
}
```

读法：客户端在打开 focus 面板、重连或收到部分 event 后，可用该请求获取完整状态。

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
    "position": 420,
    "target": "point",
    "point": {
      "x": 0.5,
      "y": 0.42
    },
    "focusState": "focused",
    "confidence": 92
  }
}
```

读法：`result` 是完整 focus 快照，客户端可以直接覆盖本地 focus 缓存。

#### 3.2.4 可能触发的事件

| Event | 触发条件 | Payload Schema | 客户端处理建议 |
|---|---|---|---|
| 无 | 查询不改变状态。 | none | 无需处理。 |

#### 3.2.5 错误

| 错误 | 场景 | 返回建议 |
|---|---|---|
| `UNAVAILABLE` | 摄像头未打开或被校准占用。 | 返回不可用原因。 |


#### 3.2.6 规则

- `getFocusState` SHOULD 返回设备当前可观测的完整 focus state。
- 如果某些字段不可观测，字段 MAY 省略，但 MUST 保留 `focusState`。

### 3.3 `camera.setFocusMode`

**用途**：切换 focus mode，例如 manual、auto、continuous_auto 或 fixed。

| 项 | 内容 |
|---|---|
| 调用类型 | command |
| Params Schema | `SetFocusModeParams` |
| Result Schema | `FocusCommandResult` |
| 是否触发事件 | 是，mode 实际变化后触发 `camera.focusModeChanged`；运行态变化时触发 `camera.focusStateChanged`。 |
| 幂等性 / 异步性 | 设置相同 mode 应成功；切换 continuous AF 可能异步。 |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_ARGUMENT`, `DEVICE_MODE_CONFLICT`, `BUSY`, `PERMISSION_DENIED` |

#### 3.3.1 请求参数 Params：`SetFocusModeParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `cameraId` | string | no | device-defined | `main` | 摄像头对象。 |
| `mode` | string enum | yes | supported mode | none | 目标 focus mode。 |

#### 3.3.2 返回结果 Result：`FocusCommandResult`

#### 3.3.3 d block 示例

request:

```json
{
  "id": 103,
  "method": "camera.setFocusMode",
  "params": {
    "cameraId": "main",
    "mode": "continuous_auto"
  }
}
```

读法：客户端请求切换到连续自动对焦；设备如果接受请求，可能立即返回 accepted，并通过后续事件报告运行态。

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
    "applyState": "focusing",
    "state": {
      "cameraId": "main",
      "mode": "continuous_auto",
      "focusState": "focusing"
    }
  }
}
```

读法：成功响应表示 mode 请求已被接受；`applyState=focusing` 表示运行态仍在变化中，客户端应继续等待事件或调用 `getFocusState` 校准。

#### 3.3.4 可能触发的事件

| Event | 触发条件 | Payload Schema | 客户端处理建议 |
|---|---|---|---|
| `camera.focusModeChanged` | mode 实际变化。 | `FocusModeChangedEvent` | 更新 mode selector。 |
| `camera.focusStateChanged` | state 进入 focusing/focused/idle。 | `FocusStateChangedEvent` | 更新运行态。 |

读法：该事件说明 mode 已实际切换；如果事件只包含部分字段，客户端可用 `camera.getFocusState` 校准完整状态。

#### 3.3.5 错误

| 错误 | 场景 | 返回建议 |
|---|---|---|
| `NOT_SUPPORTED` | mode 不支持。 | 返回合法 modes。 |
| `DEVICE_MODE_CONFLICT` | framing、calibration 或其他 owner 占用。 | 返回 owner。 |


#### 3.3.6 规则

- `mode` MUST 来自 `FocusCapabilities.modes`。
- 重复设置相同 mode SHOULD 成功；状态未变化时 MAY 不触发事件。
- 失败响应 MUST NOT 触发 mode/state changed event。

### 3.4 `camera.getFocusMode`

**用途**：只查询当前 focus mode，适合轻量 UI 刷新。

| 项 | 内容 |
|---|---|
| 调用类型 | query |
| Params Schema | `GetFocusModeParams` |
| Result Schema | `FocusModeState` |
| 是否触发事件 | 否 |
| 幂等性 / 异步性 | 幂等；同步返回。 |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_ARGUMENT`, `PERMISSION_DENIED`, `UNAVAILABLE` |

#### 3.4.1 请求参数 Params：`GetFocusModeParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `cameraId` | string | no | device-defined | `main` | 摄像头对象。 |

#### 3.4.2 返回结果 Result：`FocusModeState`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `cameraId` | string | no | device-defined | `main` | 摄像头对象。 |
| `mode` | string enum | yes | supported mode | none | 当前 focus mode。 |

#### 3.4.3 d block 示例

request:

```json
{
  "id": 104,
  "method": "camera.getFocusMode",
  "params": {
    "cameraId": "main"
  }
}
```

读法：仅需要 mode selector 状态时使用；需要完整 state 时应调用 `camera.getFocusState`。

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
    "mode": "auto"
  }
}
```

读法：结果是 mode 的轻量快照，不保证包含 position/region/focusState。

#### 3.4.4 可能触发的事件

| Event | 触发条件 | Payload Schema | 客户端处理建议 |
|---|---|---|---|
| 无 | 查询不改变状态。 | none | 无需处理。 |

#### 3.4.5 错误

| 错误 | 场景 | 返回建议 |
|---|---|---|
| `UNAVAILABLE` | camera service 不可用。 | 返回不可用原因。 |


#### 3.4.6 规则

- Query method MUST NOT 触发事件。
- 如果实现不提供轻量 get，可在 registry review 中合并到 `camera.getFocusState`。[REVIEW-DRAFT]

### 3.5 `camera.setFocusPosition`

**用途**：设置绝对手动对焦位置。

| 项 | 内容 |
|---|---|
| 调用类型 | command |
| Params Schema | `SetFocusPositionParams` |
| Result Schema | `FocusCommandResult` |
| 是否触发事件 | 是，position 实际变化后触发 `camera.focusPositionChanged`；镜头移动状态变化时触发 `camera.focusStateChanged`。 |
| 幂等性 / 异步性 | 设置相同 position 应成功；镜头移动可能异步。 |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_ARGUMENT`, `OUT_OF_RANGE`, `DEVICE_MODE_CONFLICT`, `BUSY`, `PERMISSION_DENIED` |

#### 3.5.1 请求参数 Params：`SetFocusPositionParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `cameraId` | string | no | device-defined | `main` | 摄像头对象。 |
| `position` | int32 | yes | capability range | none | 原生对焦位置。 |
| `applyMode` | string enum | no | `require_manual`, `switch_to_manual` | `require_manual` | 非 manual 时处理策略。 |

#### 3.5.2 返回结果 Result：`FocusCommandResult`

#### 3.5.3 d block 示例

request:

```json
{
  "id": 105,
  "method": "camera.setFocusPosition",
  "params": {
    "cameraId": "main",
    "position": 512,
    "applyMode": "switch_to_manual"
  }
}
```

读法：`applyMode=switch_to_manual` 表示设备可先切换到 manual 再应用 position；如果设备不允许自动切换，应返回 `DEVICE_MODE_CONFLICT`。

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
    "applyState": "moving",
    "state": {
      "cameraId": "main",
      "mode": "manual",
      "position": 512,
      "focusState": "moving"
    }
  }
}
```

读法：`position=512` 是目标位置；如果镜头仍在移动，客户端应等待 `focusStateChanged` 或后续 `getFocusState`。

#### 3.5.4 可能触发的事件

| Event | 触发条件 | Payload Schema | 客户端处理建议 |
|---|---|---|---|
| `camera.focusPositionChanged` | position 变化。 | `FocusPositionChangedEvent` | 更新位置 UI。 |
| `camera.focusStateChanged` | 镜头 moving/focused/failed。 | `FocusStateChangedEvent` | 更新运行态。 |

读法：事件表示 position 已被设备接受或实际变化；如设备只能上报移动完成事件，可在 `focusStateChanged` 中补齐。

#### 3.5.5 错误

| 错误 | 场景 | 返回建议 |
|---|---|---|
| `OUT_OF_RANGE` | position 超出能力范围。 | 返回合法范围。 |
| `DEVICE_MODE_CONFLICT` | 当前不是 manual 且 `applyMode=require_manual`。 | 提示切换手动模式。 |


#### 3.5.6 规则

- `position` MUST 落在 `FocusCapabilities.positionRange` 内。
- `applyMode=require_manual` 时，如果当前不是 manual，设备 SHOULD 返回 `DEVICE_MODE_CONFLICT`。
- 重复设置相同 position SHOULD 成功，可不重复触发事件。

### 3.6 `camera.getFocusPosition`

**用途**：只查询当前手动对焦位置。

| 项 | 内容 |
|---|---|
| 调用类型 | query |
| Params Schema | `GetFocusPositionParams` |
| Result Schema | `FocusPositionState` |
| 是否触发事件 | 否 |
| 幂等性 / 异步性 | 幂等；同步返回。 |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_ARGUMENT`, `PERMISSION_DENIED`, `UNAVAILABLE` |

#### 3.6.1 请求参数 Params：`GetFocusPositionParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `cameraId` | string | no | device-defined | `main` | 摄像头对象。 |

#### 3.6.2 返回结果 Result：`FocusPositionState`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `cameraId` | string | no | device-defined | `main` | 摄像头对象。 |
| `position` | int32 | yes | capability range | none | 当前对焦位置。 |
| `focusState` | string enum | no | `idle`, `moving`, `focusing`, `focused`, `failed`, `locked`, `unavailable` | omitted | 当前位置相关运行态。 |

#### 3.6.3 d block 示例

request:

```json
{
  "id": 106,
  "method": "camera.getFocusPosition",
  "params": {
    "cameraId": "main"
  }
}
```

读法：仅需要手动位置显示时使用；完整状态请使用 `camera.getFocusState`。

success:

```json
{
  "id": 106,
  "status": {
    "ok": true,
    "code": 0
  },
  "result": {
    "cameraId": "main",
    "position": 512,
    "focusState": "focused"
  }
}
```

读法：轻量结果不包含 mode/region；如客户端需要关联 mode，应调用 `getFocusState`。

#### 3.6.4 可能触发的事件

| Event | 触发条件 | Payload Schema | 客户端处理建议 |
|---|---|---|---|
| 无 | 查询不改变状态。 | none | 无需处理。 |

#### 3.6.5 错误

| 错误 | 场景 | 返回建议 |
|---|---|---|
| `NOT_SUPPORTED` | 固定焦或设备无可读 position。 | 返回 unsupported。 |


#### 3.6.6 规则

- Query method MUST NOT 触发事件。
- 对固定焦设备，registry review 可决定是否省略该 method 或返回 `NOT_SUPPORTED`。[REVIEW-DRAFT]

### 3.7 `camera.setFocusRegion`

**用途**：设置持久点选或区域 AF 目标。

| 项 | 内容 |
|---|---|
| 调用类型 | command |
| Params Schema | `SetFocusRegionParams` |
| Result Schema | `FocusCommandResult` |
| 是否触发事件 | 是，target/point/region 实际变化后触发 `camera.focusRegionChanged`。 |
| 幂等性 / 异步性 | 设置相同 region 应成功；是否立即 AF 由参数决定。 |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_ARGUMENT`, `OUT_OF_RANGE`, `DEVICE_MODE_CONFLICT`, `PERMISSION_DENIED` |

#### 3.7.1 请求参数 Params：`SetFocusRegionParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `cameraId` | string | no | device-defined | `main` | 摄像头对象。 |
| `target` | string enum | yes | `center`, `full_frame`, `point`, `region` | none | AF 目标类型。 |
| `point` | object | no | normalized x/y | omitted | `target=point` 时的点选目标。 |
| `region` | object | no | normalized rect | omitted | `target=region` 时的区域目标。 |
| `lockAfterFocus` | bool | no | true/false | false | AF 成功后是否锁定。 |

#### 3.7.2 返回结果 Result：`FocusCommandResult`

#### 3.7.3 d block 示例

request:

```json
{
  "id": 107,
  "method": "camera.setFocusRegion",
  "params": {
    "cameraId": "main",
    "target": "point",
    "point": {
      "x": 0.5,
      "y": 0.42
    },
    "lockAfterFocus": false
  }
}
```

读法：`point` 使用归一化坐标；坐标原点和方向需在 registry review 中与 video/camera 坐标系统统一。

success:

```json
{
  "id": 107,
  "status": {
    "ok": true,
    "code": 0
  },
  "result": {
    "accepted": true,
    "applyState": "applied",
    "state": {
      "cameraId": "main",
      "mode": "auto",
      "target": "point",
      "point": {
        "x": 0.5,
        "y": 0.42
      },
      "focusState": "idle"
    }
  }
}
```

读法：成功响应说明 AF target 已设置；是否立即触发 AF 应由单独 action 参数或 `triggerAutoFocus` 控制。

#### 3.7.4 可能触发的事件

| Event | 触发条件 | Payload Schema | 客户端处理建议 |
|---|---|---|---|
| `camera.focusRegionChanged` | target/point/region 变化。 | `FocusRegionChangedEvent` | 更新区域 UI。 |

读法：事件可直接更新焦点区域叠层；需要完整 focus state 时调用 `camera.getFocusState`。

#### 3.7.5 错误

| 错误 | 场景 | 返回建议 |
|---|---|---|
| `INVALID_ARGUMENT` | `target=point` 但缺少 `point`，或 `target=region` 但缺少 `region`。 | 返回缺失字段。 |
| `OUT_OF_RANGE` | point/region 坐标越界。 | 返回合法范围。 |


#### 3.7.6 规则

- `target=point` 时 SHOULD 携带 `point`。
- `target=region` 时 SHOULD 携带 `region`。
- 坐标 SHOULD 使用归一化 `0..1` 表达；正式坐标规则待 camera 坐标系统统一确认，见待确认问题。

### 3.8 `camera.getFocusRegion`

**用途**：查询当前 AF target / point / region。

| 项 | 内容 |
|---|---|
| 调用类型 | query |
| Params Schema | `GetFocusRegionParams` |
| Result Schema | `FocusRegionState` |
| 是否触发事件 | 否 |
| 幂等性 / 异步性 | 幂等；同步返回。 |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_ARGUMENT`, `PERMISSION_DENIED`, `UNAVAILABLE` |

#### 3.8.1 请求参数 Params：`GetFocusRegionParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `cameraId` | string | no | device-defined | `main` | 摄像头对象。 |

#### 3.8.2 返回结果 Result：`FocusRegionState`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `cameraId` | string | no | device-defined | `main` | 摄像头对象。 |
| `target` | string enum | yes | `center`, `full_frame`, `point`, `region` | none | 当前 AF 目标。 |
| `point` | object | no | normalized x/y | omitted | 点选目标。 |
| `region` | object | no | normalized rect | omitted | 区域目标。 |

#### 3.8.3 d block 示例

request:

```json
{
  "id": 108,
  "method": "camera.getFocusRegion",
  "params": {
    "cameraId": "main"
  }
}
```

读法：用于恢复 UI 中的 focus point/region overlay。

success:

```json
{
  "id": 108,
  "status": {
    "ok": true,
    "code": 0
  },
  "result": {
    "cameraId": "main",
    "target": "point",
    "point": {
      "x": 0.5,
      "y": 0.42
    }
  }
}
```

读法：轻量结果只表达 AF target，不表达 focus mode 或 movement state。

#### 3.8.4 可能触发的事件

| Event | 触发条件 | Payload Schema | 客户端处理建议 |
|---|---|---|---|
| 无 | 查询不改变状态。 | none | 无需处理。 |

#### 3.8.5 错误

| 错误 | 场景 | 返回建议 |
|---|---|---|
| `NOT_SUPPORTED` | 设备不支持 point/region AF。 | 返回 unsupported target。 |


#### 3.8.6 规则

- Query method MUST NOT 触发事件。
- 如果当前 target 不是 `point` 或 `region`，对应 `point`/`region` 字段 SHOULD 省略。

### 3.9 `camera.triggerAutoFocus`

**用途**：触发一次自动对焦动作，可使用当前 target，也可在请求中携带 point/region。

| 项 | 内容 |
|---|---|
| 调用类型 | action |
| Params Schema | `TriggerAutoFocusParams` |
| Result Schema | `FocusCommandResult` |
| 是否触发事件 | 是，AF 开始和完成/失败触发 `camera.focusStateChanged`。 |
| 幂等性 / 异步性 | 动作型；返回 accepted 不代表完成。 |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_ARGUMENT`, `OUT_OF_RANGE`, `DEVICE_MODE_CONFLICT`, `BUSY`, `TIMEOUT`, `PERMISSION_DENIED` |

#### 3.9.1 请求参数 Params：`TriggerAutoFocusParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `cameraId` | string | no | device-defined | `main` | 摄像头对象。 |
| `target` | string enum | no | `center`, `full_frame`, `point`, `region` | current target | 本次 AF 目标。 |
| `point` | object | no | normalized x/y | omitted | 点选目标。 |
| `region` | object | no | normalized rect | omitted | 区域目标。 |
| `lockAfterFocus` | bool | no | true/false | false | AF 成功后是否锁定。 |
| `timeoutMs` | uint16 | no | capability range | device default | 超时。 |

#### 3.9.2 返回结果 Result：`FocusCommandResult`

#### 3.9.3 d block 示例

request:

```json
{
  "id": 109,
  "method": "camera.triggerAutoFocus",
  "params": {
    "cameraId": "main",
    "target": "point",
    "point": {
      "x": 0.5,
      "y": 0.42
    },
    "timeoutMs": 3000
  }
}
```

读法：这是一次动作请求；成功响应只表示动作被接受，不保证已经对焦完成。

success:

```json
{
  "id": 109,
  "status": {
    "ok": true,
    "code": 0
  },
  "result": {
    "accepted": true,
    "operationId": "af-42",
    "applyState": "focusing",
    "state": {
      "cameraId": "main",
      "mode": "auto",
      "target": "point",
      "point": {
        "x": 0.5,
        "y": 0.42
      },
      "focusState": "focusing"
    }
  }
}
```

读法：`operationId` 可用于日志或后续 stop；AF 完成/失败必须通过 `focusStateChanged` 或查询确认。

#### 3.9.4 可能触发的事件

| Event | 触发条件 | Payload Schema | 客户端处理建议 |
|---|---|---|---|
| `camera.focusStateChanged` | AF focusing/focused/failed。 | `FocusStateChangedEvent` | 更新状态；失败显示原因。 |

读法：事件表示 AF 已完成；客户端可把 UI 从 focusing 切到 focused。

#### 3.9.5 错误

| 错误 | 场景 | 返回建议 |
|---|---|---|
| `BUSY` | 已有 AF 或校准动作进行中。 | 稍后重试或先 stop。 |
| `TIMEOUT` | AF 超时。 | 可允许重试。 |


#### 3.9.6 规则

- 成功响应中的 `accepted=true` 不等于 AF 完成。
- AF 完成、失败或取消 SHOULD 通过 `camera.focusStateChanged` 上报。
- 请求失败时 MUST NOT 触发新的 state changed event。

### 3.10 `camera.startFocusMove`

**用途**：启动 near/far jog 移动，通常用于按住按钮持续调焦。

| 项 | 内容 |
|---|---|
| 调用类型 | command |
| Params Schema | `StartFocusMoveParams` |
| Result Schema | `FocusCommandResult` |
| 是否触发事件 | 是，进入 moving 后触发 `camera.focusStateChanged`。 |
| 幂等性 / 异步性 | 非幂等；用于 near/far jog，需 stop 或 timeout。 |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_ARGUMENT`, `DEVICE_MODE_CONFLICT`, `BUSY`, `PERMISSION_DENIED` |

#### 3.10.1 请求参数 Params：`StartFocusMoveParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `cameraId` | string | no | device-defined | `main` | 摄像头对象。 |
| `direction` | string enum | yes | `near`, `far` | none | 移动方向。 |
| `speed` | uint8 | no | capability range | device default | 移动速度。 |
| `timeoutMs` | uint16 | no | capability range | device default | 安全超时。 |

#### 3.10.2 返回结果 Result：`FocusCommandResult`

#### 3.10.3 d block 示例

request:

```json
{
  "id": 110,
  "method": "camera.startFocusMove",
  "params": {
    "cameraId": "main",
    "direction": "near",
    "speed": 4,
    "timeoutMs": 2000
  }
}
```

读法：客户端应在 UI 松手时调用 `camera.stopFocus`；`timeoutMs` 是防止异常长时间移动的安全边界。

success:

```json
{
  "id": 110,
  "status": {
    "ok": true,
    "code": 0
  },
  "result": {
    "accepted": true,
    "operationId": "focus-move-7",
    "applyState": "moving",
    "state": {
      "cameraId": "main",
      "mode": "manual",
      "focusState": "moving"
    }
  }
}
```

读法：设备已进入 jog 移动态；客户端应展示 pressed/moving 状态。

#### 3.10.4 可能触发的事件

| Event | 触发条件 | Payload Schema | 客户端处理建议 |
|---|---|---|---|
| `camera.focusStateChanged` | focus 进入 moving。 | `FocusStateChangedEvent` | UI 显示 moving。 |

读法：事件可用于多端 UI 同步；如果 event 丢失，客户端应在 stop 后调用 `getFocusState`。

#### 3.10.5 错误

| 错误 | 场景 | 返回建议 |
|---|---|---|
| `DEVICE_MODE_CONFLICT` | 当前不是 manual 或设备不能自动切换。 | 提示切换手动。 |
| `BUSY` | 已有 AF/jog 动作进行中。 | 稍后重试或 stop。 |


#### 3.10.6 规则

- 本方法是否进入首批 registry 见待确认问题。
- 如果设备支持 jog，`startFocusMove` SHOULD 搭配 `stopFocus`。
- 实现 SHOULD 使用 `timeoutMs` 或设备默认超时防止持续移动。

### 3.11 `camera.stopFocus`

**用途**：停止 AF、扫描或 jog。没有动作进行时应幂等成功。

| 项 | 内容 |
|---|---|
| 调用类型 | command |
| Params Schema | `StopFocusParams` |
| Result Schema | `FocusCommandResult` |
| 是否触发事件 | 是，moving/focusing 状态停止后触发 `camera.focusStateChanged`。 |
| 幂等性 / 异步性 | 幂等；没有动作时成功返回当前状态。 |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_ARGUMENT`, `PERMISSION_DENIED`, `UNAVAILABLE` |

#### 3.11.1 请求参数 Params：`StopFocusParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `cameraId` | string | no | device-defined | `main` | 摄像头对象。 |
| `operationId` | string | no | device-defined | omitted | 要停止的动作；省略表示停止当前 focus 动作。 |

#### 3.11.2 返回结果 Result：`FocusCommandResult`

#### 3.11.3 d block 示例

request:

```json
{
  "id": 111,
  "method": "camera.stopFocus",
  "params": {
    "cameraId": "main",
    "operationId": "focus-move-7"
  }
}
```

读法：用于释放 UI 按住态或取消正在进行的 AF/jog。

success:

```json
{
  "id": 111,
  "status": {
    "ok": true,
    "code": 0
  },
  "result": {
    "accepted": true,
    "applyState": "applied",
    "state": {
      "cameraId": "main",
      "mode": "manual",
      "position": 530,
      "focusState": "idle"
    }
  }
}
```

读法：停止后返回当前状态；客户端可释放操作态。

#### 3.11.4 可能触发的事件

| Event | 触发条件 | Payload Schema | 客户端处理建议 |
|---|---|---|---|
| `camera.focusStateChanged` | moving/focusing 停止。 | `FocusStateChangedEvent` | 释放 UI 操作态。 |

读法：该事件通知所有端 focus 动作已经停止。

#### 3.11.5 错误

| 错误 | 场景 | 返回建议 |
|---|---|---|
| `UNAVAILABLE` | focus 控制链路不可用。 | 清除本地操作态。 |


#### 3.11.6 规则

- `stopFocus` SHOULD be idempotent：没有进行中的 focus 动作时也可成功返回当前状态。
- 如果指定 `operationId` 不存在，设备 MAY 返回成功的当前状态，或返回 `INVALID_ARGUMENT`；采纳前需固定 conformance 期望。

## 4. 事件 Events

### 4.0 事件速览

| Event | 触发条件 | Payload Schema | 客户端处理建议 | 状态 |
|---|---|---|---|---|
| `camera.focusStateChanged` | focusing/moving/focused/failed/locked/unavailable 变化。 | `FocusStateChangedEvent` | 更新状态，必要时 get 校准。 | `[REVIEW-DRAFT]` |
| `camera.focusModeChanged` | mode 实际变化。 | `FocusModeChangedEvent` | 更新 mode selector，必要时 get 校准。 | `[REVIEW-DRAFT]` |
| `camera.focusPositionChanged` | manual position 实际变化。 | `FocusPositionChangedEvent` | 更新手动位置 UI，必要时 get 校准。 | `[REVIEW-DRAFT]` |
| `camera.focusRegionChanged` | target/point/region 实际变化。 | `FocusRegionChangedEvent` | 更新 focus overlay，必要时 get 校准。 | `[REVIEW-DRAFT]` |

### 4.1 `camera.focusStateChanged`

**触发条件**：

- AF 从 idle 进入 focusing，并最终 focused/failed。
- 手动 jog 进入 moving 或停止。
- 设备策略、local panel、restore 或 legacy adapter 改变 focus runtime state。

#### 4.1.1 Payload：`FocusStateChangedEvent`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `cameraId` | string | no | device-defined | `main` | 摄像头对象。 |
| `state` | `FocusState` | yes | see schema | none | 当前状态。 |
| `source` | string enum | no | `remoteApp`, `localPanel`, `hardwareKey`, `autoAlgorithm`, `devicePolicy`, `legacyAdapter`, `unknown` | `unknown` | 状态变化来源。 |
| `reason` | string enum | no | `trigger_auto_focus`, `focus_completed`, `focus_failed`, `manual_move`, `stop`, `restore_config`, `unknown` | `unknown` | 变化原因。 |
| `stateRevision` | uint32 | no | monotonic counter | omitted | 状态版本，用于多端同步和去重。 |

#### 4.1.2 d block 示例

```json
{
  "event": "camera.focusStateChanged",
  "intent": 1,
  "data": {
    "cameraId": "main",
    "state": {
      "cameraId": "main",
      "mode": "auto",
      "target": "point",
      "point": {
        "x": 0.5,
        "y": 0.42
      },
      "focusState": "focused",
      "confidence": 92
    },
    "source": "remoteApp",
    "reason": "focus_completed",
    "stateRevision": 1028
  }
}
```

读法：payload 是完整 focus state 快照时，客户端可直接更新 UI；如果实现只上报部分字段，应在本草案中明确并要求客户端 get 校准。

#### 4.1.3 客户端处理建议

| 场景 | 建议 |
|---|---|
| `state.focusState=focused` | 显示对焦完成。 |
| `state.focusState=failed` | 展示失败原因，允许重试。 |
| event 丢失或重连 | 调用 `camera.getFocusState` 校准完整状态。 |

#### 4.1.4 规则

- Event MUST 使用 `op=6`。
- Event MUST NOT 携带 `d.id`。
- 如果状态变化是异步动作结果，SHOULD 带 `stateRevision`。

### 4.2 `camera.focusModeChanged`

**触发条件**：

- `camera.setFocusMode` 成功导致 mode 实际变化。
- 本地面板、profile、restore 或设备策略导致 mode 变化。

#### 4.2.1 Payload：`FocusModeChangedEvent`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `cameraId` | string | no | device-defined | `main` | 摄像头对象。 |
| `mode` | string enum | yes | supported mode | none | 当前 focus mode。 |
| `state` | `FocusState` | no | see schema | omitted | 变化后的完整状态快照。 |
| `source` | string enum | no | `remoteApp`, `localPanel`, `devicePolicy`, `legacyAdapter`, `unknown` | `unknown` | 变化来源。 |
| `reason` | string enum | no | `user_request`, `profile_changed`, `restore_config`, `unknown` | `unknown` | 变化原因。 |
| `stateRevision` | uint32 | no | monotonic counter | omitted | 状态版本。 |

#### 4.2.2 d block 示例

```json
{
  "event": "camera.focusModeChanged",
  "intent": 1,
  "data": {
    "cameraId": "main",
    "mode": "continuous_auto",
    "source": "remoteApp",
    "reason": "user_request",
    "stateRevision": 1025
  }
}
```

读法：payload 至少包含 mode；若缺少 `state`，客户端需要完整状态时调用 `camera.getFocusState`。

#### 4.2.3 客户端处理建议

| 场景 | 建议 |
|---|---|
| payload 包含 `state` | 可直接更新完整 focus UI。 |
| payload 只有 `mode` | 更新 mode selector，必要时调用 `camera.getFocusState`。 |
| 多端同时控制 | 使用 `stateRevision` 或 get 结果解决冲突。 |

#### 4.2.4 规则

- 只有 mode 实际变化时 SHOULD 触发。
- 重复设置相同 mode 成功时 MAY 不触发。

### 4.3 `camera.focusPositionChanged`

**触发条件**：

- `camera.setFocusPosition` 成功导致 position 变化。
- `camera.startFocusMove` / `camera.stopFocus` 过程中 position 变化。
- 本地按键或 legacy adapter 改变 position。

#### 4.3.1 Payload：`FocusPositionChangedEvent`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `cameraId` | string | no | device-defined | `main` | 摄像头对象。 |
| `position` | int32 | yes | capability range | none | 当前对焦位置。 |
| `state` | `FocusState` | no | see schema | omitted | 变化后的完整状态快照。 |
| `source` | string enum | no | `remoteApp`, `localPanel`, `hardwareKey`, `legacyAdapter`, `unknown` | `unknown` | 变化来源。 |
| `reason` | string enum | no | `user_request`, `manual_move`, `restore_config`, `unknown` | `unknown` | 变化原因。 |
| `stateRevision` | uint32 | no | monotonic counter | omitted | 状态版本。 |

#### 4.3.2 d block 示例

```json
{
  "event": "camera.focusPositionChanged",
  "intent": 1,
  "data": {
    "cameraId": "main",
    "position": 512,
    "source": "remoteApp",
    "reason": "user_request",
    "stateRevision": 1026
  }
}
```

读法：position 可直接用于更新 slider；需要 mode/focusState 时调用 `camera.getFocusState`。

#### 4.3.3 客户端处理建议

| 场景 | 建议 |
|---|---|
| slider 更新 | 直接使用 `position`。 |
| 需要完整状态 | 调用 `camera.getFocusState`。 |
| position 高频变化 | 客户端 SHOULD 去抖或只展示最新 revision。 |

#### 4.3.4 规则

- 如果 jog 移动产生高频 position 变化，设备 SHOULD 合并或限频上报。
- `stateRevision` 若存在，客户端 SHOULD 使用最新 revision。

### 4.4 `camera.focusRegionChanged`

**触发条件**：

- `camera.setFocusRegion` 成功导致 target/point/region 变化。
- `camera.triggerAutoFocus` 携带 target 并被设备采纳为当前 target。
- 本地触屏或外部策略改变 AF target。

#### 4.4.1 Payload：`FocusRegionChangedEvent`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `cameraId` | string | no | device-defined | `main` | 摄像头对象。 |
| `target` | string enum | yes | `center`, `full_frame`, `point`, `region` | none | 当前 AF 目标。 |
| `point` | object | no | normalized x/y | omitted | 点选目标。 |
| `region` | object | no | normalized rect | omitted | 区域目标。 |
| `state` | `FocusState` | no | see schema | omitted | 变化后的完整状态快照。 |
| `source` | string enum | no | `remoteApp`, `localPanel`, `devicePolicy`, `legacyAdapter`, `unknown` | `unknown` | 变化来源。 |
| `reason` | string enum | no | `user_request`, `touch_focus`, `profile_changed`, `unknown` | `unknown` | 变化原因。 |
| `stateRevision` | uint32 | no | monotonic counter | omitted | 状态版本。 |

#### 4.4.2 d block 示例

```json
{
  "event": "camera.focusRegionChanged",
  "intent": 1,
  "data": {
    "cameraId": "main",
    "target": "point",
    "point": {
      "x": 0.5,
      "y": 0.42
    },
    "source": "remoteApp",
    "reason": "user_request",
    "stateRevision": 1027
  }
}
```

读法：客户端可直接更新 overlay；如果 event 只包含 target/point/region，完整 focus state 仍通过 `camera.getFocusState` 获取。

#### 4.4.3 客户端处理建议

| 场景 | 建议 |
|---|---|
| overlay 更新 | 直接使用 target/point/region。 |
| 坐标系变化或重连 | 调用 `camera.getFocusRegion` 或 `camera.getFocusState` 校准。 |
| 多端同时控制 | 使用 `stateRevision` 或后续 get 结果解决冲突。 |

#### 4.4.4 规则

- `target=point` 时 SHOULD 携带 `point`。
- `target=region` 时 SHOULD 携带 `region`。
- 坐标字段规范化规则待 camera 坐标系统统一确认，见待确认问题。

## 5. Capability

Capability name: `camera.focus`。

设备通过 capability 声明是否支持 focus 控制，以及支持哪些模式、范围、目标类型、jog 能力和事件。

| 能力字段 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `capability` | string | yes | fixed `camera.focus` | none | capability 名称。 |
| `modes` | string[] | yes | `manual`, `auto`, `continuous_auto`, `one_shot_auto`, `fixed` | none | 支持的 focus mode。 |
| `defaultMode` | string enum | no | one of `modes` | omitted | 默认 focus mode。 |
| `positionRange` | object | no | min/max/step/unit | omitted | 手动对焦位置范围。 |
| `targets` | string[] | no | `center`, `full_frame`, `point`, `region` | omitted | AF target 能力。 |
| `manualMove` | object | no | directions/speed | omitted | near/far jog 能力。 |
| `events` | string[] | no | event names | omitted | 支持事件。 |

## 6. 字段 / Schemas

本 feature 采用复杂 feature 展开模式：method/event 小节提供关键字段和 JSON `d` block 示例；本章集中定义可复用对象。

### 6.1 Schema 层级速览

| 层级 | 用在哪里 | 作用 |
|---|---|---|
| `FocusCapabilities` | `camera.getFocusCapabilities` result / capability discovery | 描述 modes、position、targets、manual move 和 events。 |
| `FocusState` | `camera.getFocusState` result、command result、event payload | 描述 mode、position、target、focusState 和失败/置信度。 |
| `FocusCommandResult` | set/trigger/move/stop result | 描述 accepted、operationId、applyState 和当前 state。 |
| `FocusModeState` | `camera.getFocusMode` result / `focusModeChanged` payload | 描述 mode 轻量状态。 |
| `FocusPositionState` | `camera.getFocusPosition` result / `focusPositionChanged` payload | 描述 position 轻量状态。 |
| `FocusRegionState` | `camera.getFocusRegion` result / `focusRegionChanged` payload | 描述 target/point/region 轻量状态。 |

```text
FocusCapabilities
  modes[]
  positionRange
  targets[]
  manualMove

FocusState
  cameraId
  mode
  position
  target / point / region
  focusState
  confidence / failureReason

FocusCommandResult
  accepted
  operationId
  applyState
  state: FocusState
```

### 6.2 请求与响应 Schemas

#### `GetFocusCapabilitiesParams` / `GetFocusStateParams` / `GetFocusModeParams` / `GetFocusPositionParams` / `GetFocusRegionParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `cameraId` | string | no | device-defined | `main` | 摄像头对象。 |

#### `SetFocusModeParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `cameraId` | string | no | device-defined | `main` | 摄像头对象。 |
| `mode` | string enum | yes | supported mode | none | 目标模式。 |

#### `SetFocusPositionParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `cameraId` | string | no | device-defined | `main` | 摄像头对象。 |
| `position` | int32 | yes | capability range | none | 原生对焦位置。 |
| `applyMode` | string enum | no | `require_manual`, `switch_to_manual` | `require_manual` | 非 manual 时处理策略。 |

#### `SetFocusRegionParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `cameraId` | string | no | device-defined | `main` | 摄像头对象。 |
| `target` | string enum | yes | `center`, `full_frame`, `point`, `region` | none | AF 目标。 |
| `point` | object | no | normalized x/y | omitted | 点选目标；`target=point` 时使用。 |
| `region` | object | no | normalized rect | omitted | 区域目标；`target=region` 时使用。 |
| `lockAfterFocus` | bool | no | true/false | false | AF 成功后是否锁定。 |

#### `TriggerAutoFocusParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `cameraId` | string | no | device-defined | `main` | 摄像头对象。 |
| `target` | string enum | no | `center`, `full_frame`, `point`, `region` | current target | 本次 AF 目标。 |
| `point` | object | no | normalized x/y | omitted | 点选目标。 |
| `region` | object | no | normalized rect | omitted | 区域目标。 |
| `lockAfterFocus` | bool | no | true/false | false | AF 成功后是否锁定。 |
| `timeoutMs` | uint16 | no | capability range | device default | 超时。 |

#### `StartFocusMoveParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `cameraId` | string | no | device-defined | `main` | 摄像头对象。 |
| `direction` | string enum | yes | `near`, `far` | none | 移动方向。 |
| `speed` | uint8 | no | capability range | device default | 速度。 |
| `timeoutMs` | uint16 | no | capability range | device default | 安全超时。 |

#### `StopFocusParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `cameraId` | string | no | device-defined | `main` | 摄像头对象。 |
| `operationId` | string | no | device-defined | omitted | 要停止的动作；省略表示当前 focus 动作。 |

#### `FocusCommandResult`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `accepted` | bool | yes | true/false | none | 动作是否接受。 |
| `operationId` | string | no | device-defined | omitted | 异步动作 ID。 |
| `state` | `FocusState` | yes | see schema | none | 当前状态。 |
| `applyState` | string enum | no | `applied`, `focusing`, `moving`, `failed` | omitted | 应用状态。 |

#### `FocusModeState`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `cameraId` | string | no | device-defined | `main` | 摄像头对象。 |
| `mode` | string enum | yes | supported mode | none | 当前 focus mode。 |

#### `FocusPositionState`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `cameraId` | string | no | device-defined | `main` | 摄像头对象。 |
| `position` | int32 | yes | capability range | none | 当前对焦位置。 |
| `focusState` | string enum | no | focus state enum | omitted | 当前位置相关运行态。 |

#### `FocusRegionState`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `cameraId` | string | no | device-defined | `main` | 摄像头对象。 |
| `target` | string enum | yes | `center`, `full_frame`, `point`, `region` | none | 当前 AF 目标。 |
| `point` | object | no | normalized x/y | omitted | 点选目标。 |
| `region` | object | no | normalized rect | omitted | 区域目标。 |

### 6.3 Capability Schemas

#### `FocusCapabilities`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `capability` | string | yes | fixed `camera.focus` | none | capability 名称。 |
| `modes` | string[] | yes | focus modes | none | 支持模式。 |
| `defaultMode` | string enum | no | one of modes | omitted | 默认模式。 |
| `positionRange` | object | no | min/max/step/unit | omitted | 位置范围。 |
| `targets` | string[] | no | `center`, `full_frame`, `point`, `region` | omitted | AF target 能力。 |
| `manualMove` | object | no | directions/speed | omitted | jog 能力。 |
| `events` | string[] | no | event names | omitted | 支持事件。 |

#### `Range`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `min` | number | yes | device-defined | none | 最小值。 |
| `max` | number | yes | device-defined | none | 最大值。 |
| `step` | number | no | positive | omitted | 步进。 |
| `unit` | string | no | `native_step`, `normalized`, `ms`, `level` | omitted | 单位。 |

### 6.4 State / Config Schemas

#### `FocusState`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `cameraId` | string | no | device-defined | `main` | 摄像头对象。 |
| `mode` | string enum | yes | `manual`, `auto`, `continuous_auto`, `one_shot_auto`, `fixed` | none | 当前模式。 |
| `position` | int32 | no | capability range | omitted | 当前位置。 |
| `target` | string enum | no | `center`, `full_frame`, `point`, `region` | omitted | 当前 AF 目标。 |
| `point` | object | no | normalized x/y | omitted | 点目标。 |
| `region` | object | no | normalized rect | omitted | 区域目标。 |
| `focusState` | string enum | yes | `idle`, `moving`, `focusing`, `focused`, `failed`, `locked`, `unavailable` | none | 运行态。 |
| `confidence` | uint8 | no | `0..100` | omitted | 对焦置信度。 |
| `failureReason` | string enum | no | `low_light`, `no_target`, `timeout`, `resource_conflict`, `unknown` | omitted | 失败原因。 |

#### `Point`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `x` | float | yes | `0..1` | none | 归一化横坐标。 |
| `y` | float | yes | `0..1` | none | 归一化纵坐标。 |

#### `Region`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `x` | float | yes | `0..1` | none | 左上角横坐标。 |
| `y` | float | yes | `0..1` | none | 左上角纵坐标。 |
| `width` | float | yes | `0..1` | none | 宽度。 |
| `height` | float | yes | `0..1` | none | 高度。 |

### 6.5 Event Schemas

事件 payload 字段已在 [4. 事件 Events](#4-事件-events) 中就地展开。若后续 registry review 需要复用 `source/reason/stateRevision`，可抽象为共享 event metadata schema。[REVIEW-DRAFT]

## 7. 交互流程示例 Flow Examples

本章只展示多个 method/event 组成的端到端业务流程，不再承担单个 method/event 的 API 契约示例。单个 method 的 request / success / error 示例写在对应 method 小节中；单个 event 示例写在对应 event 小节中。

### 7.1 场景：查询能力 -> 点选 AF -> 收到完成事件

客户端先查询能力，再发起点选自动对焦；成功响应只表示动作已接受，最终状态以 `camera.focusStateChanged` 或后续 `camera.getFocusState` 为准。

capability request:

```json
{
  "id": 201,
  "method": "camera.getFocusCapabilities",
  "params": {
    "cameraId": "main"
  }
}
```

trigger request:

```json
{
  "id": 202,
  "method": "camera.triggerAutoFocus",
  "params": {
    "cameraId": "main",
    "target": "point",
    "point": {
      "x": 0.5,
      "y": 0.42
    },
    "timeoutMs": 3000
  }
}
```

completion event:

```json
{
  "event": "camera.focusStateChanged",
  "intent": 1,
  "data": {
    "cameraId": "main",
    "state": {
      "cameraId": "main",
      "mode": "auto",
      "target": "point",
      "point": {
        "x": 0.5,
        "y": 0.42
      },
      "focusState": "focused",
      "confidence": 92
    },
    "reason": "focus_completed",
    "stateRevision": 1028
  }
}
```

### 7.2 场景：设置手动位置失败

该场景只补充跨流程后果：失败响应不携带业务 `result`，也不应触发 changed event；客户端应保留原 UI 状态并提示合法范围。

error:

```json
{
  "id": 203,
  "status": {
    "ok": false,
    "code": 10,
    "msg": "Focus position is out of range.",
    "details": {
      "candidateError": "OUT_OF_RANGE",
      "field": "position",
      "min": 0,
      "max": 1023,
      "actual": 2048
    }
  }
}
```

## 8. 错误

| 错误 | 适用场景 | 说明 |
|---|---|---|
| `NOT_SUPPORTED` | mode、target、manual move、position 不支持。 | 返回 capability detail。 |
| `INVALID_ARGUMENT` | target 与 point/region 不匹配，字段缺失或枚举非法。 | 返回字段路径。 |
| `OUT_OF_RANGE` | position、point 或 region 越界。 | 返回合法范围。 |
| `DEVICE_MODE_CONFLICT` | 当前 mode 不允许该操作或资源被占用。 | 返回 required mode / owner。 |
| `BUSY` | AF、jog 或 calibration 进行中。 | 稍后重试或 stop。 |
| `TIMEOUT` | AF 超时。 | 可重试。 |
| `PERMISSION_DENIED` | 调用方无权控制摄像头。 | 返回权限错误。 |

## 9. Legacy 映射

Legacy 映射是迁移证据，不是 runtime 合同。

| legacy 项 | 候选映射 | 状态 | 说明 |
|---|---|---|---|
| `CommonSetAutoFocusState` / `CommonGetAutoFocusState` | `camera.setFocusMode` / `camera.getFocusState` | candidate | `EnableState` 枚举需确认。 |
| `CommonSetManualFocusPosition` / `CommonGetManualFocusPosition` | `camera.setFocusPosition` / `camera.getFocusState` 或 `camera.getFocusPosition` | candidate | position 为原生步进。 |
| `CommonSetLensCenter` | `camera.triggerAutoFocus` 或 calibration | `[REVIEW-ASK]` | 名称可能是中心 AF、镜头归中或校准。 |
| `Focus.SetMode` / `Focus.GetMode` | `camera.setFocusMode` / `camera.getFocusMode` | candidate | VM33 mode 枚举需确认。 |
| `Focus.Manual` | `camera.setFocusPosition` 或 `camera.startFocusMove` | candidate | 参数语义需确认。 |
| `Focus.SetFocus` | `camera.setFocusPosition` 或 `camera.triggerAutoFocus` | `[REVIEW-ASK]` | 原文 `pos` 像绝对位置，但历史分类曾映射到 AF。 |
| `Focus.SetFocusRegion` | `camera.setFocusRegion` / `camera.triggerAutoFocus` | candidate | 点选坐标归一化。 |
| `Focus.*Zoom` | `camera.zoom` | out-of-scope | 不归 `camera.focus`。 |

采纳提醒：registry readiness 为 candidate；事件拆分、坐标系统和 jog 控制仍需确认，feature-specific 验收重点见下方测试要点。

## 10. 测试要点

| 类型 | 要点 |
|---|---|
| happy path | 查询能力，触发 AF，收到 focusing/focused 事件。 |
| event path | `setFocusMode` -> `focusModeChanged`；`setFocusPosition` -> `focusPositionChanged`；`setFocusRegion` -> `focusRegionChanged`；AF 完成 -> `focusStateChanged`。 |
| boundary case | normalized point/region 边界，position min/max，重复设置相同值。 |
| error case | unsupported target、position 越界、mode conflict、busy、timeout、permission denied。 |
| capability discovery | UI 根据 modes/targets/positionRange/manualMove 启禁控件。 |
| compatibility | legacy focus mode / position / region command 只能作为 adapter mapping，不污染正式 schema。 |

## 11. 待确认问题

| 问题 | 影响 | 当前建议 | 状态 |
|---|---|---|---|
| `Focus.SetFocus` 真实语义是什么？ | 决定 legacy adapter 和正式方法映射。 | 先按绝对位置候选，保留 AF 可能性。 | `[REVIEW-ASK]` |
| 是否需要 `camera.getFocusMode` / `camera.getFocusPosition` / `camera.getFocusRegion` 这类轻量 query？ | 影响 method 数量和 conformance。 | 保留 draft，registry review 时可合并到 `camera.getFocusState`。 | `[REVIEW-DRAFT]` |
| `startFocusMove` 是否进入首批 registry？ | 影响 jog 控制范围。 | 如设备支持 near/far 按住控制则保留；否则 adapter-only。 | `[REVIEW-ASK]` |
| point/region 坐标系统是否已统一？ | 影响 schema 可采纳性。 | 推荐与 camera image/video normalized coordinate 统一后采纳。 | `[REVIEW-ASK]` |
| 细分事件是否优于单一 `focusConfigChanged`？ | 影响 event 数量和客户端处理。 | 本文采用细分事件以提升可读性；可在 review 中合并。 | `[REVIEW-DRAFT]` |
