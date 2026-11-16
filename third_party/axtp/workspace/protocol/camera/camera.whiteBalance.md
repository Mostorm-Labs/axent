---
status: draft
contract: false
generated: false
domain: camera
feature: camera.whiteBalance
registry:
lastReviewed: 2026-06-13
---

# camera.whiteBalance

## 0. 速读结论

| 项目 | 内容 |
|---|---|
| 这个能力做什么 | 管理白平衡模式、色温、RGB gain 和白平衡锁定。 |
| 当前状态 | draft；由 `workspace/business/camera-image-settings.md` 和 `workspace/flows/camera-image-settings.md` 转入。 |
| 是否可直接实现 | 否。草案阶段仅供评审；正式实现以 registry / generated 为准。 |
| 主要交互 | RPC + EVENT |
| 是否使用 STREAM | 否 |
| Registry readiness | candidate；手动白平衡使用色温、RGB gain 或二者都支持仍需确认。 |
| Conformance | needed；需覆盖 mode dependency、set/reset/event、unsupported/range。 |
| 主要未决问题 | VM33 `WhiteBalance.Mode` 枚举、色温单位和 RGB gain 范围需确认。 |

## 1. 功能说明

`camera.whiteBalance` 用于摄像头白平衡控制，覆盖自动白平衡、手动色温、RGB gain 和锁定行为。曝光和基础图像参数分别归 `camera.exposure` 和 `camera.image`。

## 2. 能力边界

| 类型 | 内容 |
|---|---|
| 包含 | white balance mode、色温、red/green/blue gain、lock、reset、配置变化事件。 |
| 不包含 | exposure、WDR、brightness/contrast、PTZ/zoom/focus、视频编码/STREAM。 |
| 数据面 | 不使用 STREAM。 |

## 3. 方法 Methods

### 3.0 方法速览

| Method | 调用类型 | 用途 | Params Schema | Result Schema | 是否触发事件 | 状态 |
|---|---|---|---|---|---|---|
| `camera.getWhiteBalanceCapabilities` | query | 查询白平衡模式、色温/RGB gain 范围和依赖。 | `GetWhiteBalanceCapabilitiesParams` | `WhiteBalanceCapabilities` | 否 | `[REVIEW-DRAFT]` |
| `camera.getWhiteBalanceConfig` | query | 查询当前白平衡配置。 | `GetWhiteBalanceConfigParams` | `WhiteBalanceConfig` | 否 | `[REVIEW-DRAFT]` |
| `camera.setWhiteBalanceConfig` | command | 部分更新白平衡配置。 | `SetWhiteBalanceConfigParams` | `SetWhiteBalanceConfigResult` | 是 | `[REVIEW-DRAFT]` |
| `camera.resetWhiteBalanceConfig` | command | 恢复白平衡默认值。 | `ResetWhiteBalanceConfigParams` | `SetWhiteBalanceConfigResult` | 是 | `[REVIEW-DRAFT]` |

### 3.1 `camera.getWhiteBalanceCapabilities`

| 项 | 内容 |
|---|---|
| 调用类型 | query |
| Params Schema | `GetWhiteBalanceCapabilitiesParams` |
| Result Schema | `WhiteBalanceCapabilities` |
| 是否触发事件 | 否 |
| 幂等性 / 异步性 | 幂等。 |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_ARGUMENT`, `PERMISSION_DENIED`, `UNAVAILABLE` |

#### 3.1.1 请求参数 Params：`GetWhiteBalanceCapabilitiesParams`

#### 3.1.2 返回结果 Result：`WhiteBalanceCapabilities`

#### 3.1.3 d block 示例

request:

```json
{
  "id": 101,
  "method": "camera.getWhiteBalanceCapabilities",
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
    "capability": "camera.whiteBalance",
    "modes": [
      "auto",
      "manual"
    ]
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
| `NOT_SUPPORTED` | 设备无白平衡控制。 | 隐藏白平衡设置。 |

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

### 3.2 `camera.getWhiteBalanceConfig`

| 项 | 内容 |
|---|---|
| 调用类型 | query |
| Params Schema | `GetWhiteBalanceConfigParams` |
| Result Schema | `WhiteBalanceConfig` |
| 是否触发事件 | 否 |
| 幂等性 / 异步性 | 幂等。 |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_ARGUMENT`, `PERMISSION_DENIED`, `UNAVAILABLE` |

#### 3.2.1 请求参数 Params：`GetWhiteBalanceConfigParams`

#### 3.2.2 返回结果 Result：`WhiteBalanceConfig`

#### 3.2.3 d block 示例

request:

```json
{
  "id": 102,
  "method": "camera.getWhiteBalanceConfig",
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
    "temperatureK": 5600,
    "tint": 0
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
| `UNAVAILABLE` | white balance 状态不可读。 | 返回 unavailable detail。 |

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

### 3.3 `camera.setWhiteBalanceConfig`

| 项 | 内容 |
|---|---|
| 调用类型 | command |
| Params Schema | `SetWhiteBalanceConfigParams` |
| Result Schema | `SetWhiteBalanceConfigResult` |
| 是否触发事件 | 是，配置实际变化后触发 `camera.whiteBalanceConfigChanged`。 |
| 幂等性 / 异步性 | 建议幂等；非法组合必须整体失败。 |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_ARGUMENT`, `OUT_OF_RANGE`, `DEVICE_MODE_CONFLICT`, `BUSY`, `PERMISSION_DENIED` |

#### 3.3.1 请求参数 Params：`SetWhiteBalanceConfigParams`

#### 3.3.2 返回结果 Result：`SetWhiteBalanceConfigResult`

#### 3.3.3 d block 示例

request:

```json
{
  "id": 103,
  "method": "camera.setWhiteBalanceConfig",
  "params": {
    "cameraId": "main",
    "config": {
      "cameraId": "main",
      "mode": "auto",
      "temperatureK": 5600,
      "tint": 0
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
    "temperatureK": 5600,
    "tint": 0
  }
}
```

#### 3.3.4 可能触发的事件

| Event | 触发条件 | Payload Schema | 客户端处理建议 |
|---|---|---|---|
| `camera.whiteBalanceConfigChanged` | 配置实际变化。 | `WhiteBalanceConfigChangedEvent` | 更新 UI；根据 mode 启禁字段。 |

#### 3.3.5 错误

| 错误 | 场景 | 返回建议 |
|---|---|---|
| `DEVICE_MODE_CONFLICT` | auto mode 下写 manual-only 字段。 | 返回 required mode。 |
| `OUT_OF_RANGE` | 色温或 RGB gain 越界。 | 返回范围。 |

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

### 3.4 `camera.resetWhiteBalanceConfig`

| 项 | 内容 |
|---|---|
| 调用类型 | command |
| Params Schema | `ResetWhiteBalanceConfigParams` |
| Result Schema | `SetWhiteBalanceConfigResult` |
| 是否触发事件 | 是 |
| 幂等性 / 异步性 | 幂等。 |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_ARGUMENT`, `BUSY`, `PERMISSION_DENIED` |

#### 3.4.1 请求参数 Params：`ResetWhiteBalanceConfigParams`

#### 3.4.2 返回结果 Result：`SetWhiteBalanceConfigResult`

#### 3.4.3 d block 示例

request:

```json
{
  "id": 104,
  "method": "camera.resetWhiteBalanceConfig",
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
    "temperatureK": 5600,
    "tint": 0
  }
}
```

#### 3.4.4 可能触发的事件

| Event | 触发条件 | Payload Schema | 客户端处理建议 |
|---|---|---|---|
| `camera.whiteBalanceConfigChanged` | reset 后配置变化。 | `WhiteBalanceConfigChangedEvent` | 更新 UI。 |

#### 3.4.5 错误

| 错误 | 场景 | 返回建议 |
|---|---|---|
| `BUSY` | ISP 正忙。 | 稍后重试。 |

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
| `camera.whiteBalanceConfigChanged` | set/reset/auto algorithm/profile 改变配置。 | `WhiteBalanceConfigChangedEvent` | 更新 UI 或调用 get 校准。 | `[REVIEW-DRAFT]` |

### 4.1 `camera.whiteBalanceConfigChanged`

#### Payload：`WhiteBalanceConfigChangedEvent`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `cameraId` | string | no | device-defined | `main` | 摄像头对象。 |
| `changedFields` | string[] | no | field path array | omitted | 变化字段。 |
| `config` | `WhiteBalanceConfig` | yes | see schema | none | 变化后的配置。 |
| `applyState` | string enum | yes | `applied`, `pending_restart`, `failed` | none | 应用状态。 |
| `reason` | string enum | no | `user_request`, `reset_to_default`, `auto_algorithm`, `profile_changed`, `legacy_adapter`, `unknown` | `unknown` | 变化原因。 |

#### d block 示例

```json
{
  "event": "camera.whiteBalanceConfigChanged",
  "intent": 1,
  "data": {
    "changedFields": [
      "state"
    ],
    "config": {
      "cameraId": "main",
      "mode": "auto",
      "temperatureK": 5600,
      "tint": 0
    },
    "applyState": "active",
    "reason": "user_request"
  }
}
```


#### 客户端处理建议

| 场景 | 建议 |
|---|---|
| mode 变化 | 重新计算色温/RGB gain 启用状态。 |
| event 丢失或重连 | 调用 `camera.getWhiteBalanceConfig`。 |

## 5. Capability

| 能力字段 | 类型 | 必填 | 取值范围 / 枚举 | 说明 |
|---|---|---:|---|---|
| `capability` | string | yes | fixed `camera.whiteBalance` | capability 名称。 |
| `modes` | string[] | yes | `auto`, `manual`, `locked`, `off` candidate | 支持模式。 |
| `temperatureRange` | object | no | Kelvin range | 色温范围。 |
| `rgbGainRange` | object | no | channel ranges | RGB gain 范围。 |
| `modeDependencies` | object | no | field -> modes | 字段可写依赖。 |
| `events` | string[] | no | event names | 支持事件。 |

## 6. 字段 / Schemas

### 6.1 Schema 层级速览

| 层级 | 用在哪里 | 作用 |
|---|---|---|
| `WhiteBalanceCapabilities` | get capabilities result | 描述模式、色温/RGB gain 范围和依赖。 |
| `WhiteBalanceConfig` | get/set/reset/event | 表示白平衡配置。 |
| `SetWhiteBalanceConfigResult` | set/reset result | 表示应用状态和最终配置。 |

### 6.2 请求与响应 Schemas

#### `GetWhiteBalanceCapabilitiesParams` / `GetWhiteBalanceConfigParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `cameraId` | string | no | device-defined | `main` | 摄像头对象。 |

#### `SetWhiteBalanceConfigParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `cameraId` | string | no | device-defined | `main` | 摄像头对象。 |
| `config` | `WhiteBalanceConfig` | yes | see schema | none | 目标配置或 patch。 |
| `expectedVersion` | uint32 | no | version token | omitted | 并发保护。 |

#### `ResetWhiteBalanceConfigParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `cameraId` | string | no | device-defined | `main` | 摄像头对象。 |
| `fields` | string[] | no | field names or `all` | `all` | reset 范围。 |

#### `SetWhiteBalanceConfigResult`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `config` | `WhiteBalanceConfig` | yes | see schema | none | 最终配置。 |
| `applyState` | string enum | yes | `applied`, `pending_restart`, `failed` | none | 应用状态。 |
| `version` | uint32 | no | monotonically increasing | omitted | 配置版本。 |

### 6.3 Capability Schemas

#### `WhiteBalanceCapabilities`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `capability` | string | yes | fixed `camera.whiteBalance` | none | capability 名称。 |
| `modes` | string[] | yes | white balance modes | none | 支持模式。 |
| `temperatureRange` | object | no | min/max/step/unit=kelvin | omitted | 色温范围。 |
| `rgbGainRange` | object | no | per-channel ranges | omitted | RGB gain 范围。 |
| `modeDependencies` | object | no | field -> modes | omitted | 字段可写依赖。 |
| `events` | string[] | no | event names | omitted | 支持事件。 |

### 6.4 Config / State 总结构

#### `WhiteBalanceConfig`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `mode` | string enum | yes | `auto`, `manual`, `locked`, `off` candidate | none | 白平衡模式。 |
| `colorTemperatureK` | uint16 | no | capability range | omitted | 色温，候选单位 Kelvin。 |
| `redGain` | number | no | capability range | omitted | 红色增益。 |
| `greenGain` | number | no | capability range | omitted | 绿色增益。 |
| `blueGain` | number | no | capability range | omitted | 蓝色增益。 |
| `locked` | bool | no | true/false | omitted | 是否锁定当前白平衡。 |
| `version` | uint32 | no | monotonically increasing | omitted | 配置版本。 |

## 7. JSON 示例

### 7.1 场景：设置手动色温

```json
{
  "id": 1,
  "method": "camera.setWhiteBalanceConfig",
  "params": {
    "cameraId": "main",
    "config": { "mode": "manual", "colorTemperatureK": 4500 }
  }
}
```

```json
{
  "id": 1,
  "status": { "ok": true, "code": 0 },
  "result": {
    "applyState": "applied",
    "config": { "mode": "manual", "colorTemperatureK": 4500, "version": 3 }
  }
}
```

### 7.2 场景：事件通知

```json
{
  "event": "camera.whiteBalanceConfigChanged",
  "data": {
    "cameraId": "main",
    "changedFields": ["mode", "colorTemperatureK"],
    "config": { "mode": "manual", "colorTemperatureK": 4500, "version": 3 },
    "applyState": "applied",
    "reason": "user_request"
  }
}
```

### 7.3 场景：自动模式下写色温失败

```json
{
  "id": 2,
  "status": {
    "ok": false,
    "code": 263,
    "msg": "Device mode conflict.",
    "details": { "field": "colorTemperatureK", "requiredMode": "manual", "candidateError": "DEVICE_MODE_CONFLICT" }
  }
}
```

## 8. 错误

| 错误 | 适用场景 | 说明 |
|---|---|---|
| `NOT_SUPPORTED` | mode 或字段不支持。 | 返回 capability detail。 |
| `INVALID_ARGUMENT` | 字段组合非法。 | 例如只允许色温或 RGB gain 二选一。 |
| `OUT_OF_RANGE` | 色温或 gain 越界。 | 返回范围。 |
| `DEVICE_MODE_CONFLICT` | mode 下字段不可写。 | 返回 required mode。 |
| `BUSY` | ISP 正忙。 | 稍后重试。 |

## 9. Legacy 映射

| legacy 项 | 候选映射 | 状态 | 说明 |
|---|---|---|---|
| VM33 `WhiteBalance.Mode` | `mode` | candidate | 旧值 `0/1/15` 等枚举需确认。 |
| VM33 `WhiteBalance.Temperature` | `colorTemperatureK` | candidate | 单位和范围需确认。 |
| `Config.MultiGet:Camera` / `Config.MultiGet:camera` | 拆分到 whiteBalance/image/exposure | `[REVIEW-ASK]` | 不能整体归 `camera.image`。 |


## 10. 测试要点

| 类型 | 要点 |
|---|---|
| happy path | 查询能力，设置 manual 色温，事件同步。 |
| error path | auto mode 写色温、色温越界、unsupported RGB gain。 |
| boundary case | 色温 min/max、RGB gain min/max、reset。 |
| capability discovery | UI 根据 modes/modeDependencies 启禁字段。 |
| event | set/reset/auto algorithm/profile 变化均同步。 |

## 11. 待确认问题

| 问题 | 影响 | 当前建议 | 状态 |
|---|---|---|---|
| 手动白平衡使用色温还是 RGB gain？ | 决定 schema 必填/互斥规则。 | 两者都作为 optional capability。 | `[REVIEW-ASK]` |
| VM33 mode 枚举如何映射？ | 影响 legacy adapter。 | 采纳前确认 `0/1/15` 等语义。 | `[REVIEW-ASK]` |
| `locked` 与 `mode=locked` 是否重复？ | 影响字段简化。 | 暂保留，采纳前二选一。 | `[REVIEW-DRAFT]` |
