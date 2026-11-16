---
status: generated
contract: true
generated: true
domain: audio
feature: audio.algorithm
registry: ../../../../contract/registry/domains/audio/domain.yaml
lastReviewed: 2026-06-10
---

# audio.algorithm

## 0. 速读结论

| 项目 | 内容 |
|---|---|
| 这个能力做什么 | 管理运行时音频算法的能力发现、配置查询、配置更新、默认值恢复和配置变化通知。 |
| 当前状态 | generated |
| 是否可直接实现 | 是，但实现合同以 registry / generated 为准；本文是可读说明和修订输入。 |
| 主要交互 | RPC + EVENT |
| 是否使用 STREAM | 否 |
| Registry readiness | ready |
| Conformance | needed |
| 主要未决问题 | legacy 默认值读写、算法授权、AI 线程控制和 DOA/beam 实时结果归属仍需确认。 |

## 1. 功能说明

`audio.algorithm` 用于管理设备运行时音频算法的能力发现、配置查询、配置更新、默认值恢复和配置变化通知。

本文是已采纳 feature 的可读说明。runtime 实现合同以 [contract/registry/domains/audio/domain.yaml](../../../contract/registry/domains/audio/domain.yaml)、[method registry](../../../contract/generated/method_registry.generated.md)、[event registry](../../../contract/generated/event_registry.generated.md) 和 [capability registry](../../../contract/generated/capability_registry.generated.md) 为准；本文不重新分配 methodId、eventId、capabilityId 或 fieldId。

## 2. 能力边界

| 类型 | 内容 |
|---|---|
| 包含 | 噪声抑制、回声消除、自动增益控制、波束成形、去混响、语音活动检测、DOA 配置、啸叫抑制等算法配置。 |
| 包含 | 算法对象的 `enabled`、level、时间、角度、增益、报告周期等运行时参数。 |
| 包含 | 部分更新、原子更新、恢复默认值、配置变化事件，以及设备声明的字段范围、默认值、单位和是否需要重启音频链路。 |
| 不包含 | EQ preset、EQ bands、tone/filter 配置，归 `audio.eq`。 |
| 不包含 | 用户可感知的 master/output volume、mute，归 `audio.volume`；mixer bus/channel gain 归后续 `audio.mixer`。 |
| 不包含 | 输入、输出、source、routing、playback、recording、STREAM 媒体数据。 |
| 不包含 | 算法授权、AI 算法线程暂停/恢复、DOA/beam 实时结果上报、产测诊断命令；这些只能作为 legacy adapter 或后续 feature 评审。 |
| 数据面 | 不定义 STREAM payload；所有操作都是 RPC method/event。 |

## 3. 方法 Methods

已生成 method 以 contract/registry/generated 为准。本 feature 采用复杂 schema 模式：method 下只写 schema 名称、字段定义位置和交互语义，完整字段表集中在第 6 章维护。

### 3.0 方法速览

| Method | 调用类型 | 用途 | Params Schema | Result Schema | 是否触发事件 | 状态 |
|---|---|---|---|---|---|---|
| `audio.getAlgorithmCapabilities` | query | 查询支持的算法对象、字段描述、默认值、范围、单位和更新策略。 | `AudioGetAlgorithmCapabilitiesRequest` | `AudioGetAlgorithmCapabilitiesResponse` | 否 | generated |
| `audio.getAlgorithmConfig` | query | 查询当前生效的算法配置。 | `AudioGetAlgorithmConfigRequest` | `AudioAlgorithmConfig` | 否 | generated |
| `audio.setAlgorithmConfig` | command | 部分更新一个或多个算法对象。 | `AudioSetAlgorithmConfigRequest` | `AudioSetAlgorithmConfigResponse` | 是，配置实际变化后触发 `audio.algorithmConfigChanged`。 | generated |
| `audio.resetAlgorithmConfig` | action | 将全部、部分算法对象或部分字段恢复到声明的默认值。 | `AudioResetAlgorithmConfigRequest` | `AudioSetAlgorithmConfigResponse` | 是，配置实际变化后触发 `audio.algorithmConfigChanged`。 | generated |

### 3.1 `audio.getAlgorithmCapabilities`

**用途**：查询设备支持哪些算法对象，以及每个字段的类型、范围、默认值、单位和是否需要重启音频链路。

| 项 | 内容 |
|---|---|
| 调用类型 | query |
| Params Schema | `AudioGetAlgorithmCapabilitiesRequest` |
| Result Schema | `AudioGetAlgorithmCapabilitiesResponse` |
| 是否触发事件 | 否 |
| 幂等性 / 异步性 | 幂等；同步返回能力快照。 |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_ARGUMENT`, `INTERNAL_ERROR` |

#### 3.1.1 请求参数 Params：`AudioGetAlgorithmCapabilitiesRequest`

| 字段定义 | 内容 |
|---|---|
| 字段表 | 见 6.2「查询类请求」。 |
| 备注 | `items` 省略表示查询全部支持的算法对象。 |

#### 3.1.2 返回结果 Result：`AudioGetAlgorithmCapabilitiesResponse`

| 字段定义 | 内容 |
|---|---|
| 顶层字段表 | 见 6.2「能力查询结果」。 |
| 嵌套 capability 字段 | 见 6.3「Capability Schemas」。 |

#### 3.1.3 d block 示例

request:

```json
{
  "id": 101,
  "method": "audio.getAlgorithmCapabilities",
  "params": {
    "items": [
      "noiseSuppression",
      "echoCancellation",
      "autoGainControl"
    ]
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
    "capability": "audio.algorithm",
    "updatePolicy": {
      "partialUpdateSupported": true,
      "multiAlgorithmUpdateSupported": true,
      "atomicUpdateSupported": true
    },
    "algorithms": {
      "noiseSuppression": {
        "supported": true,
        "displayName": "Noise suppression",
        "enabled": {
          "type": "boolean",
          "defaultBool": true
        },
        "level": {
          "type": "uint8",
          "defaultInt32": 2,
          "min": 0,
          "max": 3,
          "step": 1
        }
      },
      "echoCancellation": {
        "supported": true,
        "displayName": "Echo cancellation",
        "enabled": {
          "type": "boolean",
          "defaultBool": true
        },
        "tailLengthMs": {
          "type": "uint32",
          "defaultInt32": 128,
          "min": 64,
          "max": 512,
          "step": 1,
          "unit": "ms",
          "requiresAudioRestart": true
        }
      }
    }
  }
}
```

#### 3.1.4 可能触发的事件

| Event | 触发条件 | Payload Schema | 客户端处理建议 |
|---|---|---|---|
| 无 | query method 不应因查询触发配置变化事件。 | none | 无需处理。 |

#### 3.1.5 错误

| 错误 | 场景 | 返回建议 |
|---|---|---|
| `NOT_SUPPORTED` | 设备不支持 `audio.algorithm` 或不支持指定 selector。 | 返回 unsupported feature 或 selector。 |
| `INVALID_ARGUMENT` | `items` 中包含非法算法对象名。 | 返回具体非法 item。 |
| `INTERNAL_ERROR` | 读取能力描述失败。 | 返回内部错误摘要。 |

#### 3.1.6 错误 d block 示例

```json
{
  "id": 101,
  "status": {
    "ok": false,
    "code": 10,
    "msg": "Invalid argument.",
    "details": {
      "candidateError": "INVALID_ARGUMENT",
      "field": "items[0]",
      "reason": "unsupported algorithm object"
    }
  }
}
```

### 3.2 `audio.getAlgorithmConfig`

**用途**：查询当前生效的算法配置。

| 项 | 内容 |
|---|---|
| 调用类型 | query |
| Params Schema | `AudioGetAlgorithmConfigRequest` |
| Result Schema | `AudioAlgorithmConfig` |
| 是否触发事件 | 否 |
| 幂等性 / 异步性 | 幂等；同步返回当前配置快照。 |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_ARGUMENT`, `INTERNAL_ERROR` |

#### 3.2.1 请求参数 Params：`AudioGetAlgorithmConfigRequest`

| 字段定义 | 内容 |
|---|---|
| 字段表 | 见 6.2「查询类请求」。 |
| 备注 | `items` 省略表示查询全部支持的算法对象；响应可只包含被选择或被设备支持的算法对象。 |

#### 3.2.2 返回结果 Result：`AudioAlgorithmConfig`

| 字段定义 | 内容 |
|---|---|
| 总结构 | 见 6.4「Config 总结构：`AudioAlgorithmConfig`」。 |
| 各算法对象字段 | 见 6.5「各算法对象配置字段」。 |

#### 3.2.3 d block 示例

request:

```json
{
  "id": 102,
  "method": "audio.getAlgorithmConfig",
  "params": {
    "items": [
      "noiseSuppression",
      "echoCancellation"
    ]
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
    "noiseSuppression": {
      "enabled": true,
      "level": 2
    },
    "echoCancellation": {
      "enabled": true,
      "tailLengthMs": 128,
      "nlpLevel": 2
    }
  }
}
```

#### 3.2.4 可能触发的事件

| Event | 触发条件 | Payload Schema | 客户端处理建议 |
|---|---|---|---|
| 无 | query method 不应因查询触发配置变化事件。 | none | 无需处理。 |

#### 3.2.5 错误

| 错误 | 场景 | 返回建议 |
|---|---|---|
| `NOT_SUPPORTED` | 设备不支持 `audio.algorithm` 或不支持指定算法对象。 | 返回 unsupported feature 或 item。 |
| `INVALID_ARGUMENT` | selector 结构或对象名非法。 | 返回具体字段路径。 |
| `INTERNAL_ERROR` | 读取当前配置失败。 | 返回内部错误摘要。 |

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
      "field": "items[1]",
      "reason": "unknown algorithm object"
    }
  }
}
```

### 3.3 `audio.setAlgorithmConfig`

**用途**：部分更新一个或多个算法对象。

| 项 | 内容 |
|---|---|
| 调用类型 | command |
| Params Schema | `AudioSetAlgorithmConfigRequest` |
| Result Schema | `AudioSetAlgorithmConfigResponse` |
| 是否触发事件 | 是，配置实际变化后触发 `audio.algorithmConfigChanged`。 |
| 幂等性 / 异步性 | 建议幂等；重复设置相同配置应成功，可不重复触发事件。可能返回 `pending_restart` 表示等待音频链路重启。 |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_ARGUMENT`, `OUT_OF_RANGE`, `INVALID_STATE`, `BUSY`, `PERMISSION_DENIED`, `INTERNAL_ERROR` |

#### 3.3.1 请求参数 Params：`AudioSetAlgorithmConfigRequest`

| 字段定义 | 内容 |
|---|---|
| 字段表 | 见 6.2「更新请求：`AudioSetAlgorithmConfigRequest`」。 |
| 配置对象 | 见 6.4 和 6.5。 |
| 备注 | 请求必须符合 `updatePolicy`；如果校验失败，不应部分应用失败请求。 |

#### 3.3.2 返回结果 Result：`AudioSetAlgorithmConfigResponse`

| 字段定义 | 内容 |
|---|---|
| 字段表 | 见 6.2「更新 / reset 结果：`AudioSetAlgorithmConfigResponse`」。 |
| 配置对象 | `config` 字段使用 `AudioAlgorithmConfig`，见 6.4 和 6.5。 |

#### 3.3.3 d block 示例

request:

```json
{
  "id": 103,
  "method": "audio.setAlgorithmConfig",
  "params": {
    "config": {
      "noiseSuppression": {
        "level": 3
      },
      "echoCancellation": {
        "tailLengthMs": 256
      }
    }
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
    "applyState": "pending_restart",
    "requiresAudioRestart": true,
    "config": {
      "noiseSuppression": {
        "enabled": true,
        "level": 3
      },
      "echoCancellation": {
        "enabled": true,
        "tailLengthMs": 256,
        "nlpLevel": 2
      }
    }
  }
}
```

#### 3.3.4 可能触发的事件

| Event | 触发条件 | Payload Schema | 客户端处理建议 |
|---|---|---|---|
| `audio.algorithmConfigChanged` | 配置实际发生变化。 | `AudioAlgorithmConfigChangedEvent` | 可用变化片段更新 UI；如需完整算法配置，调用 `audio.getAlgorithmConfig` 校准。 |

#### 3.3.5 错误

| 错误 | 场景 | 返回建议 |
|---|---|---|
| `NOT_SUPPORTED` | 设备不支持某个算法对象或字段。 | 返回 unsupported object/field。 |
| `INVALID_ARGUMENT` | 请求结构、对象名、字段名或 enum 值非法。 | 返回具体字段路径。 |
| `OUT_OF_RANGE` | 数字字段超出 capability 声明范围或 step。 | 返回合法范围。 |
| `INVALID_STATE` | 当前音频模式禁止修改算法配置。 | 返回当前状态摘要。 |
| `BUSY` | DSP 或音频 pipeline 正忙。 | 建议客户端稍后重试。 |
| `PERMISSION_DENIED` | 调用方没有权限。 | 返回权限错误。 |
| `INTERNAL_ERROR` | 应用配置失败。 | 返回内部错误摘要。 |

#### 3.3.6 错误 d block 示例

```json
{
  "id": 103,
  "status": {
    "ok": false,
    "code": 11,
    "msg": "Value is outside the supported range.",
    "details": {
      "field": "noiseSuppression.level",
      "actual": 5,
      "min": 0,
      "max": 3
    }
  }
}
```

读法：失败请求不得部分应用，也不得触发 `audio.algorithmConfigChanged`。

### 3.4 `audio.resetAlgorithmConfig`

**用途**：将全部、部分算法对象或部分字段恢复到 capability 声明的默认值。

| 项 | 内容 |
|---|---|
| 调用类型 | action |
| Params Schema | `AudioResetAlgorithmConfigRequest` |
| Result Schema | `AudioSetAlgorithmConfigResponse` |
| 是否触发事件 | 是，配置实际变化后触发 `audio.algorithmConfigChanged`。 |
| 幂等性 / 异步性 | 幂等；重复 reset 已经是默认值的字段应成功，可不重复触发事件。可能返回 `pending_restart`。 |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_ARGUMENT`, `OUT_OF_RANGE`, `INVALID_STATE`, `BUSY`, `PERMISSION_DENIED`, `INTERNAL_ERROR` |

#### 3.4.1 请求参数 Params：`AudioResetAlgorithmConfigRequest`

| 字段定义 | 内容 |
|---|---|
| 字段表 | 见 6.2「恢复默认请求：`AudioResetAlgorithmConfigRequest`」。 |
| selector 写法 | 支持 `"all"`、算法对象名数组、或 `{ algorithm: [field] }`，详见 6.2 的 `items` 写法表。 |
| 备注 | reset 是恢复当前运行时配置，不表示写入新的设备默认 profile。 |

#### 3.4.2 返回结果 Result：`AudioSetAlgorithmConfigResponse`

| 字段定义 | 内容 |
|---|---|
| 字段表 | 见 6.2「更新 / reset 结果：`AudioSetAlgorithmConfigResponse`」。 |
| 配置对象 | `config` 字段使用 `AudioAlgorithmConfig`，见 6.4 和 6.5。 |

#### 3.4.3 d block 示例

request:

```json
{
  "id": 104,
  "method": "audio.resetAlgorithmConfig",
  "params": {
    "items": {
      "noiseSuppression": [
        "level"
      ],
      "echoCancellation": [
        "nlpLevel"
      ]
    }
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
    "applyState": "applied",
    "requiresAudioRestart": false,
    "config": {
      "noiseSuppression": {
        "enabled": true,
        "level": 2
      },
      "echoCancellation": {
        "enabled": true,
        "nlpLevel": 2
      }
    }
  }
}
```

#### 3.4.4 可能触发的事件

| Event | 触发条件 | Payload Schema | 客户端处理建议 |
|---|---|---|---|
| `audio.algorithmConfigChanged` | reset 后配置实际发生变化。 | `AudioAlgorithmConfigChangedEvent` | 可用变化片段更新 UI；如需完整算法配置，调用 `audio.getAlgorithmConfig` 校准。 |

#### 3.4.5 错误

| 错误 | 场景 | 返回建议 |
|---|---|---|
| `NOT_SUPPORTED` | 设备不支持 reset 指定算法对象或字段。 | 返回 unsupported object/field。 |
| `INVALID_ARGUMENT` | `items` 写法非法、对象名非法或字段名非法。 | 返回具体字段路径。 |
| `OUT_OF_RANGE` | 默认值或恢复结果无法落在当前 capability 范围内。 | 返回设备内部范围冲突。 |
| `INVALID_STATE` | 当前音频模式禁止 reset。 | 返回当前状态摘要。 |
| `BUSY` | DSP 或音频 pipeline 正忙。 | 建议客户端稍后重试。 |
| `PERMISSION_DENIED` | 调用方没有权限。 | 返回权限错误。 |
| `INTERNAL_ERROR` | 恢复默认值失败。 | 返回内部错误摘要。 |

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
      "field": "items.echoCancellation[0]",
      "reason": "unknown reset field"
    }
  }
}
```

## 4. 事件 Events

### 4.0 事件速览

| Event | 触发条件 | Payload Schema | 客户端处理建议 | 状态 |
|---|---|---|---|---|
| `audio.algorithmConfigChanged` | `set`/`reset` 成功改变配置；profile change；restore config；factory reset；device policy 改变配置。 | `AudioAlgorithmConfigChangedEvent` | 可用变化片段更新 UI；如需完整算法配置，调用 `audio.getAlgorithmConfig` 校准。失败请求不得触发该事件。 | generated |

### 4.1 `audio.algorithmConfigChanged`

**触发条件**：

- `audio.setAlgorithmConfig` 成功改变一个或多个算法字段。
- `audio.resetAlgorithmConfig` 成功恢复一个或多个算法字段。
- profile change、restore config、factory reset 或 device policy 改变算法配置。

#### Payload：`AudioAlgorithmConfigChangedEvent`

本 feature 采用复杂 schema 模式，完整 Payload 字段表集中在 6.2「事件 Payload：`AudioAlgorithmConfigChangedEvent`」。

| 字段定义 | 内容 |
|---|---|
| Payload Schema | `AudioAlgorithmConfigChangedEvent` |
| 字段表 | 见 6.2「事件 Payload：`AudioAlgorithmConfigChangedEvent`」。 |
| 配置对象 | `config` 字段使用 `AudioAlgorithmConfig`，见 6.4 和 6.5。 |

#### d block 示例

```json
{
  "event": "audio.algorithmConfigChanged",
  "intent": 1,
  "data": {
    "reason": "user_request",
    "applyState": "pending_restart",
    "requiresAudioRestart": true,
    "config": {
      "noiseSuppression": {
        "level": 3
      },
      "echoCancellation": {
        "tailLengthMs": 256
      }
    },
    "changedFields": [
      "noiseSuppression.level",
      "echoCancellation.tailLengthMs"
    ]
  }
}
```

读法：事件中的 `config` 可以是变化片段，不一定是完整配置。客户端需要完整状态时，应调用 `audio.getAlgorithmConfig` 校准。

#### 客户端处理建议

| 场景 | 建议 |
|---|---|
| payload 是完整配置 | 可直接更新 UI 或本地缓存。 |
| payload 是变化片段 | 按 `changedFields` 局部更新；如需完整配置，调用 `audio.getAlgorithmConfig` 校准。 |
| event 丢失或重连 | 重连后主动调用 `audio.getAlgorithmConfig`。 |
| 请求失败 | 失败请求不得触发 `audio.algorithmConfigChanged`。 |

## 5. Capability

Capability name: `audio.algorithm`。

| 能力字段 | 类型 | 必填 | 取值范围 / 枚举 | 说明 |
|---|---|---:|---|---|
| `configSchemaVersion` | string | no | max length 16 | 设备暴露的算法配置 schema 版本标签。 |
| `updatePolicy` | `AudioAlgorithmUpdatePolicy` | yes | see schema | set/reset 的更新策略。 |
| `supportedAlgorithms` | bytes；JSON 视图为 `string[]` | no | max length 64 | 支持的算法对象名紧凑列表。 |

### 支持的算法对象

| 对象 | 说明 | 常用字段 |
|---|---|---|
| `noiseSuppression` | 背景噪声抑制。 | `enabled`, `level` |
| `echoCancellation` | 声学回声消除。 | `enabled`, `tailLengthMs`, `nlpLevel` |
| `autoGainControl` | DSP 链路内自动增益控制。 | `enabled`, `targetLevelDb`, `maxGainDb`, `attackTimeMs`, `releaseTimeMs` |
| `beamforming` | 波束方向和波束宽度配置。 | `enabled`, `lookDirectionDeg`, `beamWidthDeg` |
| `dereverberation` | 混响抑制。 | `enabled`, `level` |
| `voiceActivityDetection` | 语音活动检测。 | `enabled`, `sensitivity`, `hangoverMs` |
| `directionOfArrival` | DOA 配置，不包含实时结果事件。 | `enabled`, `reportingEnabled`, `reportIntervalMs`, `smoothingMs` |
| `howlingSuppression` | 啸叫反馈抑制。 | `enabled`, `level` |

## 6. 字段 / Schemas

这一节按“读者实际怎么理解协议”来排版：先看整体层级，再看请求/响应，再看 capability，最后看每个算法对象的配置字段。本 feature 采用复杂 schema 模式：第 3/4 章只引用 schema，本章集中维护完整字段表。

### 6.1 Schema 层级速览

`audio.algorithm` 有两类核心数据：

| 层级 | 用在哪里 | 作用 |
|---|---|---|
| capability descriptor | `audio.getAlgorithmCapabilities` result | 告诉客户端设备支持哪些算法、每个字段的范围、默认值、单位和重启要求。 |
| runtime config | `audio.getAlgorithmConfig` result、`audio.setAlgorithmConfig` params、event payload | 表示当前生效配置，或要修改/已变化的配置。 |

结构关系如下：

```text
AudioGetAlgorithmCapabilitiesResponse
  capability: "audio.algorithm"
  updatePolicy: AudioAlgorithmUpdatePolicy
  algorithms: AudioAlgorithmCapabilities
    noiseSuppression: AudioNoiseSuppressionCapabilities
    echoCancellation: AudioEchoCancellationCapabilities
    autoGainControl: AudioAutoGainControlCapabilities
    ...

AudioAlgorithmConfig
  noiseSuppression: AudioNoiseSuppressionConfig
  echoCancellation: AudioEchoCancellationConfig
  autoGainControl: AudioAutoGainControlConfig
  beamforming: AudioBeamformingConfig
  dereverberation: AudioDereverberationConfig
  voiceActivityDetection: AudioVoiceActivityDetectionConfig
  directionOfArrival: AudioDirectionOfArrivalConfig
  howlingSuppression: AudioHowlingSuppressionConfig
```

阅读规则：

- `Capabilities` 结尾的 schema 描述“设备能做什么”。
- `Config` 结尾的 schema 描述“现在是什么配置”或“要改成什么配置”。
- `AudioAlgorithmPropertyCapability` 是字段级说明，例如 `level` 的 min/max/step/default。
- JSON 视图中，registry 里标为 `bytes` 但描述为 JSON selector/list 的字段，按数组、字符串或对象展示。

### 6.2 请求与响应 Schemas

#### 查询类请求

`AudioGetAlgorithmCapabilitiesRequest` 和 `AudioGetAlgorithmConfigRequest` 都使用同一个 selector 语义。

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `items` | bytes；JSON 视图为 `string[]` | no | `AudioAlgorithmConfig` 中的算法对象名 | omitted | 选择要查询的算法对象；省略表示查询全部支持对象。 |

#### 能力查询结果：`AudioGetAlgorithmCapabilitiesResponse`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `capability` | string | yes | fixed `audio.algorithm` | none | capability 名称。 |
| `updatePolicy` | `AudioAlgorithmUpdatePolicy` | yes | see 6.3 | none | set/reset 的部分更新、多算法更新和原子性策略。 |
| `algorithms` | `AudioAlgorithmCapabilities` | yes | see 6.3 | none | 按算法对象名组织的 capability descriptor。 |

#### 更新请求：`AudioSetAlgorithmConfigRequest`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `config` | `AudioAlgorithmConfig` | yes | see 6.5 | none | 要更新的算法对象和字段；省略的对象或字段保持不变。 |

#### 恢复默认请求：`AudioResetAlgorithmConfigRequest`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `items` | bytes；JSON 视图为 string / array / object | yes | `"all"`、算法对象名数组、或 `{ algorithm: [field] }` | none | 指定恢复全部、部分算法对象或部分字段。 |

`items` 的三种写法：

| 写法 | 示例 | 含义 |
|---|---|---|
| string | `"all"` | 恢复全部支持的算法配置。 |
| array | `["noiseSuppression", "echoCancellation"]` | 恢复这些算法对象的全部字段。 |
| object | `{ "noiseSuppression": ["level"] }` | 只恢复指定字段。 |

#### 更新 / reset 结果：`AudioSetAlgorithmConfigResponse`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `applyState` | enum | yes | `applied`, `pending_restart` | none | 变更是否已经生效，或等待音频链路重启。 |
| `requiresAudioRestart` | bool | yes | `true`, `false` | none | 本次变更是否需要重启音频链路或重建 pipeline。 |
| `config` | `AudioAlgorithmConfig` | yes | see 6.5 | none | 本次操作影响的最终生效配置；通常只返回受影响对象。 |

#### 事件 Payload：`AudioAlgorithmConfigChangedEvent`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `reason` | enum | yes | `user_request`, `reset_to_default`, `factory_reset`, `profile_changed`, `device_policy`, `restore_config`, `unknown` | none | 配置变化原因。 |
| `applyState` | enum | yes | `applied`, `pending_restart` | none | 变更应用状态。 |
| `requiresAudioRestart` | bool | yes | `true`, `false` | none | 是否需要重启音频链路。 |
| `config` | `AudioAlgorithmConfig` | yes | see 6.5 | none | 已改变或受影响的配置值。 |
| `changedFields` | bytes；JSON 视图为 `string[]` | no | field path 数组 | omitted | 变化字段路径，例如 `noiseSuppression.level`。 |

### 6.3 Capability Schemas

#### `AudioAlgorithmCapability`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `configSchemaVersion` | string | no | max length 16 | omitted | 设备暴露的算法配置 schema 版本标签。 |
| `updatePolicy` | `AudioAlgorithmUpdatePolicy` | yes | see below | none | set/reset 的更新策略。 |
| `supportedAlgorithms` | bytes；JSON 视图为 `string[]` | no | max length 64 | omitted | 支持的算法对象名紧凑列表。 |

#### `AudioAlgorithmUpdatePolicy`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `partialUpdateSupported` | bool | yes | `true`, `false` | none | 是否允许只发送要修改的字段。 |
| `multiAlgorithmUpdateSupported` | bool | yes | `true`, `false` | none | 一个请求是否允许更新多个算法对象。 |
| `atomicUpdateSupported` | bool | yes | `true`, `false` | none | set/reset 是否按原子操作应用。 |

#### `AudioAlgorithmPropertyCapability`

每一个可配置字段都可以用这个结构描述范围、默认值、单位和重启要求。

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `type` | enum | yes | `boolean`, `enum`, `uint8`, `uint16`, `uint32`, `int32`, `float`, `string`, `object`, `array` | none | 属性类型。 |
| `defaultBool` | bool | no | `true`, `false` | omitted | boolean 默认值。 |
| `defaultEnum` | string | no | max length 32 | omitted | enum 默认值。 |
| `defaultInt32` | int32 | no | int32 | omitted | 整数类默认值。 |
| `min` | int32 | no | inclusive min | omitted | 数字最小值。 |
| `max` | int32 | no | inclusive max | omitted | 数字最大值。 |
| `step` | int32 | no | positive integer | omitted | 数字步进。 |
| `values` | bytes；JSON 视图为 `string[]` | no | max length 128 | omitted | enum 可选值。 |
| `unit` | string | no | `ms`, `dB`, `degree` 等 | omitted | 单位。 |
| `requiresAudioRestart` | bool | no | `true`, `false` | omitted | 修改该字段是否需要重启音频链路。 |

#### `AudioAlgorithmCapabilities`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `noiseSuppression` | `AudioNoiseSuppressionCapabilities` | no | see object capability | omitted | 噪声抑制能力。 |
| `echoCancellation` | `AudioEchoCancellationCapabilities` | no | see object capability | omitted | 回声消除能力。 |
| `autoGainControl` | `AudioAutoGainControlCapabilities` | no | see object capability | omitted | 自动增益控制能力。 |
| `beamforming` | `AudioBeamformingCapabilities` | no | see object capability | omitted | 波束成形能力。 |
| `dereverberation` | `AudioDereverberationCapabilities` | no | see object capability | omitted | 去混响能力。 |
| `voiceActivityDetection` | `AudioVoiceActivityDetectionCapabilities` | no | see object capability | omitted | VAD 能力。 |
| `directionOfArrival` | `AudioDirectionOfArrivalCapabilities` | no | see object capability | omitted | DOA 配置能力。 |
| `howlingSuppression` | `AudioHowlingSuppressionCapabilities` | no | see object capability | omitted | 啸叫抑制能力。 |

每个算法对象的 capability schema 都遵循相同模式：

| 字段名 | 类型 | 必填 | 说明 |
|---|---|---:|---|
| `supported` | bool | yes | 是否支持该算法对象。 |
| `displayName` | string | no | UI 可读名称。 |
| 具体配置字段名 | `AudioAlgorithmPropertyCapability` | no | 对应配置字段的范围、默认值、单位和重启要求。 |

### 6.4 Config 总结构：`AudioAlgorithmConfig`

`AudioAlgorithmConfig` 是一个按算法对象分组的对象。get response 可以返回完整配置；set request 可以只带要修改的对象和字段。

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `noiseSuppression` | `AudioNoiseSuppressionConfig` | no | see 6.5 | omitted | 噪声抑制配置。 |
| `echoCancellation` | `AudioEchoCancellationConfig` | no | see 6.5 | omitted | 回声消除配置。 |
| `autoGainControl` | `AudioAutoGainControlConfig` | no | see 6.5 | omitted | 自动增益控制配置。 |
| `beamforming` | `AudioBeamformingConfig` | no | see 6.5 | omitted | 波束成形配置。 |
| `dereverberation` | `AudioDereverberationConfig` | no | see 6.5 | omitted | 去混响配置。 |
| `voiceActivityDetection` | `AudioVoiceActivityDetectionConfig` | no | see 6.5 | omitted | 语音活动检测配置。 |
| `directionOfArrival` | `AudioDirectionOfArrivalConfig` | no | see 6.5 | omitted | DOA 配置。 |
| `howlingSuppression` | `AudioHowlingSuppressionConfig` | no | see 6.5 | omitted | 啸叫抑制配置。 |

### 6.5 各算法对象配置字段

#### `noiseSuppression`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `enabled` | bool | no | `true`, `false` | capability declared | 是否启用噪声抑制。 |
| `level` | uint8 | no | 0..3 | capability declared | 噪声抑制强度。 |

#### `echoCancellation`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `enabled` | bool | no | `true`, `false` | capability declared | 是否启用回声消除。 |
| `tailLengthMs` | uint32 | no | 64..512 ms | capability declared | 回声尾长；修改可能需要重启音频链路。 |
| `nlpLevel` | uint8 | no | 0..3 | capability declared | Non-linear processing 强度。 |

#### `autoGainControl`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `enabled` | bool | no | `true`, `false` | capability declared | 是否启用 AGC。 |
| `targetLevelDb` | int32 | no | -36..-6 dB | capability declared | 目标输出电平。 |
| `maxGainDb` | uint8 | no | 0..36 dB | capability declared | 最大增益。 |
| `attackTimeMs` | uint32 | no | 1..1000 ms | capability declared | 增益 attack 时间。 |
| `releaseTimeMs` | uint32 | no | 10..5000 ms | capability declared | 增益 release 时间。 |

#### `beamforming`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `enabled` | bool | no | `true`, `false` | capability declared | 是否启用波束成形。 |
| `lookDirectionDeg` | int32 | no | -180..180 degree | capability declared | 固定波束朝向角度。 |
| `beamWidthDeg` | uint32 | no | 10..180 degree | capability declared | 波束宽度。 |

#### `dereverberation`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `enabled` | bool | no | `true`, `false` | capability declared | 是否启用去混响。 |
| `level` | uint8 | no | 0..3 | capability declared | 去混响强度。 |

#### `voiceActivityDetection`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `enabled` | bool | no | `true`, `false` | capability declared | 是否启用 VAD。 |
| `sensitivity` | uint8 | no | 0..3 | capability declared | 检测灵敏度。 |
| `hangoverMs` | uint32 | no | 0..2000 ms | capability declared | 语音结束 hangover 时间。 |

#### `directionOfArrival`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `enabled` | bool | no | `true`, `false` | capability declared | 是否启用 DOA 估计。 |
| `reportingEnabled` | bool | no | `true`, `false` | capability declared | 是否启用 DOA/beam 结果报告配置；本 feature 不定义报告事件 payload。 |
| `reportIntervalMs` | uint32 | no | 20..5000 ms | capability declared | 结果报告周期。 |
| `smoothingMs` | uint32 | no | 0..5000 ms | capability declared | 平滑窗口。 |

#### `howlingSuppression`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `enabled` | bool | no | `true`, `false` | capability declared | 是否启用啸叫抑制。 |
| `level` | uint8 | no | 0..3 | capability declared | 啸叫抑制强度。 |

## 7. 交互流程示例 Flow Examples

本章只展示多个 method/event 组成的端到端流程。单个 method 的 Request / Success Response / Error Response 示例见第 3 章；单个 event 的 Event 示例见第 4 章。

### 7.1 场景：渲染算法页并修改降噪强度

1. 客户端先调用 `audio.getAlgorithmCapabilities`，根据 `algorithms.*` 的字段范围渲染 UI。
2. 客户端调用 `audio.getAlgorithmConfig`，读取当前运行时配置。
3. 用户修改 `noiseSuppression.level` 后，客户端调用 `audio.setAlgorithmConfig`。
4. 设备返回 `AudioSetAlgorithmConfigResponse`；如配置实际变化，随后发送 `audio.algorithmConfigChanged`。

读法：如果 `audio.algorithmConfigChanged.data.config` 只是变化片段，客户端可以局部更新 UI；如果需要完整状态，调用 `audio.getAlgorithmConfig` 校准。失败响应不得部分应用，也不得触发配置变化事件。

## 8. 错误

本 feature 复用 generated ErrorCode，不新增 feature-specific errorCode。

| 错误 | 适用场景 | 说明 |
|---|---|---|
| `NOT_SUPPORTED` | 设备不支持 `audio.algorithm`、某个算法对象或某个字段。 | 也可用于当前设备能力不支持的 selector。 |
| `INVALID_ARGUMENT` | selector、算法对象名、字段名、enum 值或请求结构非法。 | 请求结构错误时不应应用任何配置。 |
| `OUT_OF_RANGE` | 数字值超出 generated range 或 step。 | 例如 `noiseSuppression.level > 3`。 |
| `INVALID_STATE` | 当前模式禁止修改。 | 例如音频链路处于不可配置状态。 |
| `BUSY` | DSP 或音频 pipeline 正忙。 | 客户端可稍后重试。 |
| `PERMISSION_DENIED` | 调用方没有权限。 | 由 runtime/session 权限决定。 |
| `INTERNAL_ERROR` | 读取或应用配置失败。 | 设备内部错误。 |

## 9. Legacy 映射

Legacy 映射只是迁移证据，不是 runtime 合同。旧命令别名不得创造新的正式 AXTP method，也不得扩大 `audio.algorithm` 已生成语义。

| legacy 项 | 候选映射 | 状态 | 说明 |
|---|---|---|---|
| 降噪、回声消除、混响抑制、AGC level/mode 类命令 | `audio.getAlgorithmConfig` / `audio.setAlgorithmConfig` | candidate | 需要 adapter 把旧字段转成对应算法对象字段。 |
| 默认算法参数读取类命令 | `audio.getAlgorithmCapabilities` 的默认值或 `audio.getAlgorithmConfig` | unresolved | 旧协议“读默认值”和“读当前值”的语义需要确认。 |
| 默认算法参数写入类命令 | adapter-only / future default-profile feature | adapter-only | 写入默认 profile 不等同于 `resetAlgorithmConfig`。 |
| Beam map、DOA、speaker highlight 配置 | `beamforming` 或 `directionOfArrival` 配置字段 | candidate | 只映射配置；实时方向/说话人结果不在本 feature 内。 |
| 算法授权命令 | adapter-only / future license feature | adapter-only | 授权不是算法配置。 |
| AI 算法线程暂停/恢复 | adapter-only / future runtime action | adapter-only | 暂停线程不是普通配置字段。 |
| DOA/beam 实时报告 | adapter-only / future event feature | adapter-only | 当前 generated event 只报告配置变化。 |

## 10. 测试要点

| 类型 | 要点 |
|---|---|
| happy path | 查询能力、查询配置、更新单个算法、更新多个算法、恢复默认值。 |
| error path | 不支持的算法对象、未知字段、非法 enum、无权限、BUSY。 |
| boundary case | 每个数字字段的 min/max/out-of-range；`items` 省略、数组、map、`all`。 |
| capability discovery | `audio.algorithm` capability 绑定 4 个 generated methods 和 `audio.algorithmConfigChanged` event。 |
| event | set/reset 成功后发 `audio.algorithmConfigChanged`；失败请求不发事件；多字段原子变更建议合并事件。 |
| compatibility | runtime 应绑定 contract/registry/generated 或明确 spec tag/commit，不依赖本文 prose 作为唯一事实源。 |

## 11. 待确认问题

| 问题 | 影响 | 当前建议 | 状态 |
|---|---|---|---|
| legacy 默认算法参数读取是读当前值、默认值还是出厂 profile？ | legacy / schema | 读当前值映射 `getAlgorithmConfig`；读默认值映射 capability 默认值；写默认 profile 暂不进入本 feature。 | open |
| 算法授权内容是否属于 `audio.algorithm`？ | domain boundary | 不属于普通算法配置，倾向 auth/vendor/license 或 diagnostic。 | open |
| AI 算法线程暂停/恢复是否映射为配置？ | domain boundary / runtime action | 不映射为普通 config；后续按 runtime action 或 diagnostic 评审。 | open |
| DOA/beam 实时结果是否进入 `audio.algorithmConfigChanged`？ | event / conformance | 不进入；当前 event 只表示配置变化，实时结果另起 feature。 | decided |
