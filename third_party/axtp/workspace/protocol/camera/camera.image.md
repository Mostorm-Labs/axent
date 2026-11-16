---
status: draft
contract: false
generated: false
domain: camera
feature: camera.image
registry:
lastReviewed: 2026-06-13
---

# camera.image

## 0. 速读结论

| 项目 | 内容 |
|---|---|
| 这个能力做什么 | 管理亮度、对比度、饱和度、锐度、色调、图像风格和视角等基础图像参数。 |
| 当前状态 | draft；由 `workspace/business/camera-image-settings.md` 和 `workspace/flows/camera-image-settings.md` 转入。 |
| 是否可直接实现 | 否。草案阶段仅供评审；正式实现以 registry / generated 为准。 |
| 主要交互 | RPC + EVENT |
| 是否使用 STREAM | 否 |
| Registry readiness | candidate；字段集合、视角归属和 VM33 Camera 配置拆分仍需确认。 |
| Conformance | needed；需覆盖 capability、partial set、reset、event、unsupported/range。 |
| 主要未决问题 | sightAngle 是否归 image，VM33 Camera 泛配置如何拆分到 image/exposure/whiteBalance。 |

## 1. 功能说明

`camera.image` 用于基础图像质量参数配置。它不承载曝光、白平衡、focus/zoom/PTZ、framing 或视频流编码参数。

## 2. 能力边界

| 类型 | 内容 |
|---|---|
| 包含 | brightness、contrast、saturation、sharpness、hue、imageStyle、sightAngle 候选、能力范围、默认值、reset、配置变化事件。 |
| 不包含 | ISO/gain/shutter/EV/WDR/防频闪、white balance、PTZ/zoom/focus、framing、encoder/stream。 |
| 数据面 | 不使用 STREAM。 |

## 3. 方法 Methods

### 3.0 方法速览

| Method | 调用类型 | 用途 | Params Schema | Result Schema | 是否触发事件 | 状态 |
|---|---|---|---|---|---|---|
| `camera.getImageCapabilities` | query | 查询基础图像字段、范围、默认值和单位。 | `GetImageCapabilitiesParams` | `ImageCapabilities` | 否 | `[REVIEW-DRAFT]` |
| `camera.getImageConfig` | query | 查询当前基础图像配置。 | `GetImageConfigParams` | `ImageConfig` | 否 | `[REVIEW-DRAFT]` |
| `camera.setImageConfig` | command | 部分更新基础图像配置。 | `SetImageConfigParams` | `SetImageConfigResult` | 是 | `[REVIEW-DRAFT]` |
| `camera.resetImageConfig` | command | 恢复基础图像默认值。 | `ResetImageConfigParams` | `SetImageConfigResult` | 是 | `[REVIEW-DRAFT]` |

### 3.1 `camera.getImageCapabilities`

| 项 | 内容 |
|---|---|
| 调用类型 | query |
| Params Schema | `GetImageCapabilitiesParams` |
| Result Schema | `ImageCapabilities` |
| 是否触发事件 | 否 |
| 幂等性 / 异步性 | 幂等；同步返回能力。 |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_ARGUMENT`, `PERMISSION_DENIED`, `UNAVAILABLE` |

#### 3.1.1 请求参数 Params：`GetImageCapabilitiesParams`

#### 3.1.2 返回结果 Result：`ImageCapabilities`

#### 3.1.3 d block 示例

request:

```json
{
  "id": 101,
  "method": "camera.getImageCapabilities",
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
    "capability": "camera.image",
    "fields": {},
    "partialUpdate": true
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
| `NOT_SUPPORTED` | 设备不支持基础图像调节。 | 隐藏图像设置。 |

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

### 3.2 `camera.getImageConfig`

| 项 | 内容 |
|---|---|
| 调用类型 | query |
| Params Schema | `GetImageConfigParams` |
| Result Schema | `ImageConfig` |
| 是否触发事件 | 否 |
| 幂等性 / 异步性 | 幂等。 |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_ARGUMENT`, `PERMISSION_DENIED`, `UNAVAILABLE` |

#### 3.2.1 请求参数 Params：`GetImageConfigParams`

#### 3.2.2 返回结果 Result：`ImageConfig`

#### 3.2.3 d block 示例

request:

```json
{
  "id": 102,
  "method": "camera.getImageConfig",
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
    "brightness": 50,
    "contrast": 50,
    "saturation": 50,
    "sharpness": 50
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
| `UNAVAILABLE` | camera pipeline 不可读。 | 返回不可用原因。 |

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

### 3.3 `camera.setImageConfig`

| 项 | 内容 |
|---|---|
| 调用类型 | command |
| Params Schema | `SetImageConfigParams` |
| Result Schema | `SetImageConfigResult` |
| 是否触发事件 | 是，配置实际变化后触发 `camera.imageConfigChanged`。 |
| 幂等性 / 异步性 | 建议幂等；支持 partial update，原子性待确认。 |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_ARGUMENT`, `OUT_OF_RANGE`, `INVALID_STATE`, `BUSY`, `PERMISSION_DENIED` |

#### 3.3.1 请求参数 Params：`SetImageConfigParams`

#### 3.3.2 返回结果 Result：`SetImageConfigResult`

#### 3.3.3 d block 示例

request:

```json
{
  "id": 103,
  "method": "camera.setImageConfig",
  "params": {
    "cameraId": "main",
    "config": {
      "cameraId": "main",
      "brightness": 50,
      "contrast": 50,
      "saturation": 50,
      "sharpness": 50
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
    "brightness": 50,
    "contrast": 50,
    "saturation": 50,
    "sharpness": 50
  }
}
```

#### 3.3.4 可能触发的事件

| Event | 触发条件 | Payload Schema | 客户端处理建议 |
|---|---|---|---|
| `camera.imageConfigChanged` | 配置实际变化。 | `ImageConfigChangedEvent` | 更新 UI；需要完整状态时调用 get。 |

#### 3.3.5 错误

| 错误 | 场景 | 返回建议 |
|---|---|---|
| `OUT_OF_RANGE` | brightness 等字段超范围。 | 返回合法范围。 |
| `INVALID_ARGUMENT` | 字段不支持或组合非法。 | 返回字段路径。 |
| `BUSY` | ISP 正在切换。 | 稍后重试。 |

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

### 3.4 `camera.resetImageConfig`

| 项 | 内容 |
|---|---|
| 调用类型 | command |
| Params Schema | `ResetImageConfigParams` |
| Result Schema | `SetImageConfigResult` |
| 是否触发事件 | 是 |
| 幂等性 / 异步性 | 幂等；可同步或异步。 |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_ARGUMENT`, `BUSY`, `PERMISSION_DENIED` |

#### 3.4.1 请求参数 Params：`ResetImageConfigParams`

#### 3.4.2 返回结果 Result：`SetImageConfigResult`

#### 3.4.3 d block 示例

request:

```json
{
  "id": 104,
  "method": "camera.resetImageConfig",
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
    "brightness": 50,
    "contrast": 50,
    "saturation": 50,
    "sharpness": 50
  }
}
```

#### 3.4.4 可能触发的事件

| Event | 触发条件 | Payload Schema | 客户端处理建议 |
|---|---|---|---|
| `camera.imageConfigChanged` | reset 后配置变化。 | `ImageConfigChangedEvent` | 更新 UI。 |

#### 3.4.5 错误

| 错误 | 场景 | 返回建议 |
|---|---|---|
| `NOT_SUPPORTED` | 不支持 reset。 | 隐藏恢复默认。 |

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

## 4. 事件 Events

### 4.0 事件速览

| Event | 触发条件 | Payload Schema | 客户端处理建议 | 状态 |
|---|---|---|---|---|
| `camera.imageConfigChanged` | set/reset/profile/local control 改变配置。 | `ImageConfigChangedEvent` | 更新 UI 或调用 get 校准。 | `[REVIEW-DRAFT]` |

### 4.1 `camera.imageConfigChanged`

#### Payload：`ImageConfigChangedEvent`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `cameraId` | string | no | device-defined | `main` | 摄像头对象。 |
| `changedFields` | string[] | no | field path array | omitted | 变化字段。 |
| `config` | `ImageConfig` | yes | see schema | none | 变化后的配置。 |
| `applyState` | string enum | yes | `applied`, `pending_restart`, `failed` | none | 应用状态。 |
| `reason` | string enum | no | `user_request`, `reset_to_default`, `profile_changed`, `device_policy`, `legacy_adapter`, `unknown` | `unknown` | 变化原因。 |

#### d block 示例

```json
{
  "event": "camera.imageConfigChanged",
  "intent": 1,
  "data": {
    "changedFields": [
      "state"
    ],
    "config": {
      "cameraId": "main",
      "brightness": 50,
      "contrast": 50,
      "saturation": 50,
      "sharpness": 50
    },
    "applyState": "active",
    "reason": "user_request"
  }
}
```


#### 客户端处理建议

| 场景 | 建议 |
|---|---|
| 完整配置 | 直接更新控件。 |
| event 丢失或重连 | 调用 `camera.getImageConfig`。 |

## 5. Capability

| 能力字段 | 类型 | 必填 | 取值范围 / 枚举 | 说明 |
|---|---|---:|---|---|
| `capability` | string | yes | fixed `camera.image` | capability 名称。 |
| `fields` | object | yes | field capability map | 每个基础图像字段的范围、默认值、单位。 |
| `partialUpdate` | bool | no | true/false | 是否支持部分更新。 |
| `resetScopes` | string[] | no | `all` 或字段名 | reset 范围。 |
| `events` | string[] | no | event names | 支持事件。 |

## 6. 字段 / Schemas

### 6.1 Schema 层级速览

| 层级 | 用在哪里 | 作用 |
|---|---|---|
| `ImageCapabilities` | get capabilities result | 描述基础字段范围和默认值。 |
| `ImageConfig` | get/set/reset/event | 表示基础图像配置。 |
| `SetImageConfigResult` | set/reset result | 表示应用状态和最终配置。 |

### 6.2 请求与响应 Schemas

#### `GetImageCapabilitiesParams` / `GetImageConfigParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `cameraId` | string | no | device-defined | `main` | 摄像头对象。 |
| `fields` | string[] | no | field names | omitted | 可选字段筛选。 |

#### `SetImageConfigParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `cameraId` | string | no | device-defined | `main` | 摄像头对象。 |
| `config` | `ImageConfig` | yes | see schema | none | 目标配置或 patch。 |
| `expectedVersion` | uint32 | no | version token | omitted | 并发保护。 |

#### `ResetImageConfigParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `cameraId` | string | no | device-defined | `main` | 摄像头对象。 |
| `fields` | string[] | no | field names or `all` | `all` | reset 范围。 |

#### `SetImageConfigResult`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `config` | `ImageConfig` | yes | see schema | none | 最终配置。 |
| `applyState` | string enum | yes | `applied`, `pending_restart`, `failed` | none | 应用状态。 |
| `version` | uint32 | no | monotonically increasing | omitted | 配置版本。 |

### 6.3 Capability Schemas

#### `ImageCapabilities`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `capability` | string | yes | fixed `camera.image` | none | capability 名称。 |
| `fields` | object | yes | field descriptors | none | 每个字段的 min/max/step/default/unit。 |
| `supportedStyles` | string[] | no | device-defined | omitted | 图像风格枚举。 |
| `partialUpdate` | bool | no | true/false | true | 是否支持 partial set。 |
| `events` | string[] | no | event names | omitted | 支持事件。 |

### 6.4 Config / State 总结构

#### `ImageConfig`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `brightness` | int16 | no | capability range | omitted | 亮度。 |
| `contrast` | int16 | no | capability range | omitted | 对比度。 |
| `saturation` | int16 | no | capability range | omitted | 饱和度。 |
| `sharpness` | int16 | no | capability range | omitted | 锐度。 |
| `hue` | int16 | no | capability range | omitted | 色调。 |
| `imageStyle` | string enum | no | capability styles | omitted | 图像风格。 |
| `sightAngle` | int16 | no | capability range | omitted | `[REVIEW-ASK]` 视角字段，归属待确认。 |
| `version` | uint32 | no | monotonically increasing | omitted | 配置版本。 |

## 7. JSON 示例

### 7.1 场景：设置亮度和锐度

```json
{
  "id": 1,
  "method": "camera.setImageConfig",
  "params": {
    "cameraId": "main",
    "config": { "brightness": 55, "sharpness": 40 }
  }
}
```

```json
{
  "id": 1,
  "status": { "ok": true, "code": 0 },
  "result": {
    "applyState": "applied",
    "config": { "brightness": 55, "sharpness": 40, "version": 8 }
  }
}
```

### 7.2 场景：配置变化事件

```json
{
  "event": "camera.imageConfigChanged",
  "data": {
    "cameraId": "main",
    "changedFields": ["brightness", "sharpness"],
    "config": { "brightness": 55, "sharpness": 40, "version": 8 },
    "applyState": "applied",
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
    "details": { "field": "brightness", "min": 0, "max": 100, "candidateError": "OUT_OF_RANGE" }
  }
}
```

## 8. 错误

| 错误 | 适用场景 | 说明 |
|---|---|---|
| `NOT_SUPPORTED` | 字段或 image feature 不支持。 | 返回字段路径。 |
| `INVALID_ARGUMENT` | 字段组合非法。 | 例如未知 style。 |
| `OUT_OF_RANGE` | 数值越界。 | 返回范围。 |
| `INVALID_STATE` | camera pipeline 不允许设置。 | 例如 privacy/camera off。 |
| `BUSY` | ISP 正在切换。 | 稍后重试。 |

## 9. Legacy 映射

| legacy 项 | 候选映射 | 状态 | 说明 |
|---|---|---|---|
| `CommonSetImageStyle` / `CommonGetImageStyle` | `camera.setImageConfig.imageStyle` / `camera.getImageConfig` | candidate | style 枚举待确认。 |
| `CommonSetSightAngle` / `CommonGetSightAngle` | `camera.image.sightAngle` | `[REVIEW-ASK]` | 需确认是否归 lens/framing。 |
| `Config.MultiGet:Camera` / `Config.MultiGet:camera` | 拆分到 image/exposure/whiteBalance | `[REVIEW-ASK]` | 不能整体塞进 `camera.image`。 |


## 10. 测试要点

| 类型 | 要点 |
|---|---|
| happy path | 查询能力，部分设置 brightness/sharpness，事件同步。 |
| error path | unsupported field、越界、busy、permission denied。 |
| boundary case | min/max/step、partial update、reset 部分字段。 |
| capability discovery | UI 字段、范围、默认值来自 capability。 |
| event | set/reset/profile 变化均可同步。 |

## 11. 待确认问题

| 问题 | 影响 | 当前建议 | 状态 |
|---|---|---|---|
| 基础图像 MVP 字段集合是什么？ | 决定 schema 和 conformance。 | 先保留 brightness/contrast/saturation/sharpness/hue/style。 | `[REVIEW-ASK]` |
| `sightAngle` 归属哪里？ | 影响 image vs lens/framing 边界。 | 暂作为 image 候选字段。 | `[REVIEW-ASK]` |
| VM33 Camera 配置如何拆分？ | 影响 legacy adapter。 | 按字段拆到 image/exposure/whiteBalance。 | `[REVIEW-ASK]` |
