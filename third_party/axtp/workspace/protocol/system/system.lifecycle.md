---
status: draft
contract: false
generated: false
domain: system
feature: system.lifecycle
registry: ""
lastReviewed: 2026-06-15
---

# system.lifecycle

## 0. 速读结论

| 项目 | 内容 |
|---|---|
| 这个能力做什么 | 系统立即重启、计划重启、graceful shutdown、计划关机、生命周期状态读取和生命周期状态变化通知。 |
| 当前状态 | draft |
| 是否可直接实现 | 否。本文是 protocol draft；正式实现以 registry / generated 为准。 |
| 主要交互 | RPC + EVENT |
| 是否使用 STREAM | 否 |
| Registry readiness | partial |
| Conformance | needed |
| 主要未决问题 | schema 字段、错误模型、legacy 映射和 conformance case 仍需人工确认。 |

## 1. 功能说明

`system.lifecycle` 用于系统立即重启、计划重启、graceful shutdown、计划关机、生命周期状态读取和生命周期状态变化通知。

## 2. 能力边界

| 类型 | 内容 |
|---|---|
| 包含 | system.lifecycle 的能力发现、状态查询、配置或动作控制。 |
| 包含 | 与 system.lifecycle 直接相关的 method/event/schema 草案。 |
| 不包含 | 不承载其他 capability feature 的业务语义；跨域关系通过 schema 字段、引用或数据面 stream/file 表达。 |
| 不包含 | method/event 数值 ID 分配；数值以 contract/registry/generated 为准。 |
| 数据面 | 本 feature 默认不定义 STREAM payload，所有操作均通过 RPC method/event 完成。 |

## 3. 方法 Methods

### 3.0 方法速览

| Method | 调用类型 | 用途 | Params Schema | Result Schema | 是否触发事件 | 状态 |
|---|---|---|---|---|---|---|
| `system.getRebootSchedule` | query | 查询已配置的重启计划 | `GetRebootScheduleParams` | `LifecycleScheduleList` | 否 | draft |
| `system.setRebootSchedule` | command | 创建或更新设备重启计划 | `SetRebootScheduleParams` | `SetLifecycleScheduleResult` | 是，`system.lifecycleStateChanged` | draft |
| `system.cancelRebootSchedule` | action | 取消指定重启计划 | `CancelRebootScheduleParams` | `CancelScheduleResult` | 是，`system.lifecycleStateChanged` | draft |
| `system.getShutdownSchedule` | query | 查询已配置的关机计划 | `GetShutdownScheduleParams` | `LifecycleScheduleList` | 否 | draft |
| `system.setShutdownSchedule` | command | 创建或更新设备关机计划 | `SetShutdownScheduleParams` | `SetLifecycleScheduleResult` | 是，`system.lifecycleStateChanged` | draft |
| `system.cancelShutdownSchedule` | action | 取消指定关机计划 | `CancelShutdownScheduleParams` | `CancelScheduleResult` | 是，`system.lifecycleStateChanged` | draft |
| `system.getLifecycleState` | query | 查询当前 lifecycle 状态和待执行动作 | `GetLifecycleStateParams` | `LifecycleState` | 否 | draft |
| `system.reboot` | action | 请求设备执行重启动作 | `RebootParams` | `LifecycleActionResult` | 是，`system.lifecycleStateChanged` | draft |
| `system.shutdown` | action | 请求设备执行关机动作 | `ShutdownParams` | `LifecycleActionResult` | 是，`system.lifecycleStateChanged` | draft |

### 3.1 `system.getRebootSchedule`

**用途**：查询已配置的重启计划。

| 项 | 内容 |
|---|---|
| 调用类型 | query |
| Params Schema | `GetRebootScheduleParams` |
| Result Schema | `LifecycleScheduleList` |
| 是否触发事件 | 否 |
| 幂等性 / 异步性 | 幂等；同步返回当前快照。 |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_ARGUMENT`, `PERMISSION_DENIED`, `UNAVAILABLE` |

#### 3.1.1 请求参数 Params：`GetRebootScheduleParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `target` | string | no | target id | `default` | 示例值 `system`；查询对象。 |

#### 3.1.2 返回结果 Result：`LifecycleScheduleList`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `state` | object | yes | see schema | none | 当前结果对象；示例字段包括 `scheduleId`、`enabled`、`pendingAction`。 |
| `sampledAt` | string timestamp | no | RFC 3339 | omitted | 结果采样时间；客户端可用于缓存和校准。 |

#### 3.1.3 d block 示例

request:

```json
{
  "id": 101,
  "method": "system.getRebootSchedule",
  "params": {
    "target": "system"
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
    "schedules": [
      {
        "scheduleId": "reboot-nightly",
        "enabled": true,
        "triggerAt": "2026-06-22T03:00:00Z",
        "policy": "graceful",
        "reason": "maintenance"
      }
    ],
    "sampledAt": "2026-06-21T21:00:00Z"
  }
}
```

#### 3.1.4 可能触发的事件

| Event | 触发条件 | Payload Schema | 客户端处理建议 |
|---|---|---|---|
| 无 | query method 不应因查询触发状态变化事件。 | none | 无需处理。 |

#### 3.1.5 错误

| 错误 | 场景 | 返回建议 |
|---|---|---|
| `NOT_SUPPORTED` | 设备不支持该 feature、method、target 或 scope。 | 返回 unsupported feature/method/target。 |
| `INVALID_ARGUMENT` | 请求字段非法、枚举非法或范围非法。 | 返回具体字段路径和合法范围。 |
| `PERMISSION_DENIED` | 调用方无权执行该操作。 | 返回权限错误。 |
| `BUSY` | 设备正在处理冲突操作。 | 建议稍后重试。 |

### 3.2 `system.setRebootSchedule`

**用途**：查询已配置的重启计划。

| 项 | 内容 |
|---|---|
| 调用类型 | command |
| Params Schema | `SetRebootScheduleParams` |
| Result Schema | `SetLifecycleScheduleResult` |
| 是否触发事件 | 是，状态实际变化后触发 `system.lifecycleStateChanged`。 |
| 幂等性 / 异步性 | 建议幂等；重复提交相同目标状态应成功，可不重复触发事件。 |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_ARGUMENT`, `INVALID_STATE`, `BUSY`, `PERMISSION_DENIED` |

#### 3.2.1 请求参数 Params：`SetRebootScheduleParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `target` | string | no | target id | `default` | 设置对象；具体 target 集合由 capability 声明。 |
| `config` | object | yes | see schema | none | 目标配置或状态片段；字段需在采纳前确认。 |

#### 3.2.2 返回结果 Result：`SetLifecycleScheduleResult`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `accepted` | boolean | yes | `true`, `false` | none | 设备是否接受并应用请求。 |
| `state` | object | no | see schema | omitted | 设置后的状态或配置快照。 |

#### 3.2.3 d block 示例

request:

```json
{
  "id": 102,
  "method": "system.setRebootSchedule",
  "params": {
    "target": "system",
    "config": {
      "scheduleId": "reboot-nightly",
      "enabled": true,
      "triggerAt": "2026-06-22T03:00:00Z",
      "policy": "graceful"
    }
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
    "accepted": true,
    "state": {
      "scheduleId": "reboot-nightly",
      "enabled": true,
      "pendingAction": "reboot"
    }
  }
}
```

#### 3.2.4 可能触发的事件

| Event | 触发条件 | Payload Schema | 客户端处理建议 |
|---|---|---|---|
| `system.lifecycleStateChanged` | 该方法导致状态、配置或动作状态实际变化。 | `LifecycleStateChangedEvent` | 可直接更新 UI；需要完整状态时调用对应 get method 校准。 |

#### 3.2.5 错误

| 错误 | 场景 | 返回建议 |
|---|---|---|
| `NOT_SUPPORTED` | 设备不支持该 feature、method、target 或 scope。 | 返回 unsupported feature/method/target。 |
| `INVALID_ARGUMENT` | 请求字段非法、枚举非法或范围非法。 | 返回具体字段路径和合法范围。 |
| `PERMISSION_DENIED` | 调用方无权执行该操作。 | 返回权限错误。 |
| `BUSY` | 设备正在处理冲突操作。 | 建议稍后重试。 |

### 3.3 `system.cancelRebootSchedule`

**用途**：查询已配置的重启计划。

| 项 | 内容 |
|---|---|
| 调用类型 | action |
| Params Schema | `CancelRebootScheduleParams` |
| Result Schema | `CancelScheduleResult` |
| 是否触发事件 | 是，状态实际变化后触发 `system.lifecycleStateChanged`。 |
| 幂等性 / 异步性 | 建议幂等；重复提交相同目标状态应成功，可不重复触发事件。 |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_ARGUMENT`, `INVALID_STATE`, `BUSY`, `PERMISSION_DENIED` |

#### 3.3.1 请求参数 Params：`CancelRebootScheduleParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `target` | string | no | target id | `default` | 动作对象；具体 target 集合由 capability 声明。 |
| `reason` | string | no | caller-defined reason | omitted | 调用方给出的动作原因。 |

#### 3.3.2 返回结果 Result：`CancelScheduleResult`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `accepted` | boolean | yes | `true`, `false` | none | 设备是否接受动作请求。 |
| `actionId` | string | no | opaque action id | omitted | 动作 ID，用于日志或异步关联。 |

#### 3.3.3 d block 示例

request:

```json
{
  "id": 103,
  "method": "system.cancelRebootSchedule",
  "params": {
    "target": "system",
    "scheduleId": "reboot-nightly",
    "reason": "user_request"
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
    "actionId": "cancel-reboot-nightly"
  }
}
```

#### 3.3.4 可能触发的事件

| Event | 触发条件 | Payload Schema | 客户端处理建议 |
|---|---|---|---|
| `system.lifecycleStateChanged` | 该方法导致状态、配置或动作状态实际变化。 | `LifecycleStateChangedEvent` | 可直接更新 UI；需要完整状态时调用对应 get method 校准。 |

#### 3.3.5 错误

| 错误 | 场景 | 返回建议 |
|---|---|---|
| `NOT_SUPPORTED` | 设备不支持该 feature、method、target 或 scope。 | 返回 unsupported feature/method/target。 |
| `INVALID_ARGUMENT` | 请求字段非法、枚举非法或范围非法。 | 返回具体字段路径和合法范围。 |
| `PERMISSION_DENIED` | 调用方无权执行该操作。 | 返回权限错误。 |
| `BUSY` | 设备正在处理冲突操作。 | 建议稍后重试。 |

### 3.4 `system.getShutdownSchedule`

**用途**：查询已配置的重启计划。

| 项 | 内容 |
|---|---|
| 调用类型 | query |
| Params Schema | `GetShutdownScheduleParams` |
| Result Schema | `LifecycleScheduleList` |
| 是否触发事件 | 否 |
| 幂等性 / 异步性 | 幂等；同步返回当前快照。 |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_ARGUMENT`, `PERMISSION_DENIED`, `UNAVAILABLE` |

#### 3.4.1 请求参数 Params：`GetShutdownScheduleParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `target` | string | no | target id | `default` | 示例值 `system`；查询对象。 |

#### 3.4.2 返回结果 Result：`LifecycleScheduleList`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `state` | object | yes | see schema | none | 当前结果对象；示例字段包括 `scheduleId`、`enabled`、`pendingAction`。 |
| `sampledAt` | string timestamp | no | RFC 3339 | omitted | 结果采样时间；客户端可用于缓存和校准。 |

#### 3.4.3 d block 示例

request:

```json
{
  "id": 104,
  "method": "system.getShutdownSchedule",
  "params": {
    "target": "system"
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
    "schedules": [
      {
        "scheduleId": "shutdown-weekend",
        "enabled": false,
        "triggerAt": "2026-06-27T22:00:00Z",
        "policy": "graceful"
      }
    ],
    "sampledAt": "2026-06-21T21:00:00Z"
  }
}
```

#### 3.4.4 可能触发的事件

| Event | 触发条件 | Payload Schema | 客户端处理建议 |
|---|---|---|---|
| 无 | query method 不应因查询触发状态变化事件。 | none | 无需处理。 |

#### 3.4.5 错误

| 错误 | 场景 | 返回建议 |
|---|---|---|
| `NOT_SUPPORTED` | 设备不支持该 feature、method、target 或 scope。 | 返回 unsupported feature/method/target。 |
| `INVALID_ARGUMENT` | 请求字段非法、枚举非法或范围非法。 | 返回具体字段路径和合法范围。 |
| `PERMISSION_DENIED` | 调用方无权执行该操作。 | 返回权限错误。 |
| `BUSY` | 设备正在处理冲突操作。 | 建议稍后重试。 |

### 3.5 `system.setShutdownSchedule`

**用途**：查询已配置的重启计划。

| 项 | 内容 |
|---|---|
| 调用类型 | command |
| Params Schema | `SetShutdownScheduleParams` |
| Result Schema | `SetLifecycleScheduleResult` |
| 是否触发事件 | 是，状态实际变化后触发 `system.lifecycleStateChanged`。 |
| 幂等性 / 异步性 | 建议幂等；重复提交相同目标状态应成功，可不重复触发事件。 |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_ARGUMENT`, `INVALID_STATE`, `BUSY`, `PERMISSION_DENIED` |

#### 3.5.1 请求参数 Params：`SetShutdownScheduleParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `target` | string | no | target id | `default` | 设置对象；具体 target 集合由 capability 声明。 |
| `config` | object | yes | see schema | none | 目标配置或状态片段；字段需在采纳前确认。 |

#### 3.5.2 返回结果 Result：`SetLifecycleScheduleResult`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `accepted` | boolean | yes | `true`, `false` | none | 设备是否接受并应用请求。 |
| `state` | object | no | see schema | omitted | 设置后的状态或配置快照。 |

#### 3.5.3 d block 示例

request:

```json
{
  "id": 105,
  "method": "system.setShutdownSchedule",
  "params": {
    "target": "system",
    "config": {
      "scheduleId": "shutdown-weekend",
      "enabled": true,
      "triggerAt": "2026-06-27T22:00:00Z",
      "policy": "graceful"
    }
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
      "scheduleId": "shutdown-weekend",
      "enabled": true,
      "pendingAction": "shutdown"
    }
  }
}
```

#### 3.5.4 可能触发的事件

| Event | 触发条件 | Payload Schema | 客户端处理建议 |
|---|---|---|---|
| `system.lifecycleStateChanged` | 该方法导致状态、配置或动作状态实际变化。 | `LifecycleStateChangedEvent` | 可直接更新 UI；需要完整状态时调用对应 get method 校准。 |

#### 3.5.5 错误

| 错误 | 场景 | 返回建议 |
|---|---|---|
| `NOT_SUPPORTED` | 设备不支持该 feature、method、target 或 scope。 | 返回 unsupported feature/method/target。 |
| `INVALID_ARGUMENT` | 请求字段非法、枚举非法或范围非法。 | 返回具体字段路径和合法范围。 |
| `PERMISSION_DENIED` | 调用方无权执行该操作。 | 返回权限错误。 |
| `BUSY` | 设备正在处理冲突操作。 | 建议稍后重试。 |

### 3.6 `system.cancelShutdownSchedule`

**用途**：查询已配置的重启计划。

| 项 | 内容 |
|---|---|
| 调用类型 | action |
| Params Schema | `CancelShutdownScheduleParams` |
| Result Schema | `CancelScheduleResult` |
| 是否触发事件 | 是，状态实际变化后触发 `system.lifecycleStateChanged`。 |
| 幂等性 / 异步性 | 建议幂等；重复提交相同目标状态应成功，可不重复触发事件。 |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_ARGUMENT`, `INVALID_STATE`, `BUSY`, `PERMISSION_DENIED` |

#### 3.6.1 请求参数 Params：`CancelShutdownScheduleParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `target` | string | no | target id | `default` | 动作对象；具体 target 集合由 capability 声明。 |
| `reason` | string | no | caller-defined reason | omitted | 调用方给出的动作原因。 |

#### 3.6.2 返回结果 Result：`CancelScheduleResult`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `accepted` | boolean | yes | `true`, `false` | none | 设备是否接受动作请求。 |
| `actionId` | string | no | opaque action id | omitted | 动作 ID，用于日志或异步关联。 |

#### 3.6.3 d block 示例

request:

```json
{
  "id": 106,
  "method": "system.cancelShutdownSchedule",
  "params": {
    "target": "system",
    "scheduleId": "shutdown-weekend",
    "reason": "operator_cancelled"
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
    "actionId": "cancel-shutdown-weekend"
  }
}
```

#### 3.6.4 可能触发的事件

| Event | 触发条件 | Payload Schema | 客户端处理建议 |
|---|---|---|---|
| `system.lifecycleStateChanged` | 该方法导致状态、配置或动作状态实际变化。 | `LifecycleStateChangedEvent` | 可直接更新 UI；需要完整状态时调用对应 get method 校准。 |

#### 3.6.5 错误

| 错误 | 场景 | 返回建议 |
|---|---|---|
| `NOT_SUPPORTED` | 设备不支持该 feature、method、target 或 scope。 | 返回 unsupported feature/method/target。 |
| `INVALID_ARGUMENT` | 请求字段非法、枚举非法或范围非法。 | 返回具体字段路径和合法范围。 |
| `PERMISSION_DENIED` | 调用方无权执行该操作。 | 返回权限错误。 |
| `BUSY` | 设备正在处理冲突操作。 | 建议稍后重试。 |

### 3.7 `system.getLifecycleState`

**用途**：查询已配置的重启计划。

| 项 | 内容 |
|---|---|
| 调用类型 | query |
| Params Schema | `GetLifecycleStateParams` |
| Result Schema | `LifecycleState` |
| 是否触发事件 | 否 |
| 幂等性 / 异步性 | 幂等；同步返回当前快照。 |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_ARGUMENT`, `PERMISSION_DENIED`, `UNAVAILABLE` |

#### 3.7.1 请求参数 Params：`GetLifecycleStateParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `target` | string | no | target id | `default` | 示例值 `system`；查询对象。 |

#### 3.7.2 返回结果 Result：`LifecycleState`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `lifecycleState` | string | yes | see example | none | 当前生命周期状态。 |
| `uptimeSeconds` | uint32 | yes | see example | none | 启动后运行时长，单位秒。 |
| `pendingAction` | string or null | yes | see example | none | 待执行动作；没有待执行动作时可为空。 |
| `sampledAt` | string timestamp | no | RFC 3339 | omitted | 结果采样时间；客户端可用于缓存和校准。 |

#### 3.7.3 d block 示例

request:

```json
{
  "id": 107,
  "method": "system.getLifecycleState",
  "params": {
    "target": "system"
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
    "lifecycleState": "running",
    "uptimeSeconds": 86400,
    "pendingAction": null,
    "sampledAt": "2026-06-21T21:00:00Z"
  }
}
```

#### 3.7.4 可能触发的事件

| Event | 触发条件 | Payload Schema | 客户端处理建议 |
|---|---|---|---|
| 无 | query method 不应因查询触发状态变化事件。 | none | 无需处理。 |

#### 3.7.5 错误

| 错误 | 场景 | 返回建议 |
|---|---|---|
| `NOT_SUPPORTED` | 设备不支持该 feature、method、target 或 scope。 | 返回 unsupported feature/method/target。 |
| `INVALID_ARGUMENT` | 请求字段非法、枚举非法或范围非法。 | 返回具体字段路径和合法范围。 |
| `PERMISSION_DENIED` | 调用方无权执行该操作。 | 返回权限错误。 |
| `BUSY` | 设备正在处理冲突操作。 | 建议稍后重试。 |

### 3.8 `system.reboot`

**用途**：查询已配置的重启计划。

| 项 | 内容 |
|---|---|
| 调用类型 | action |
| Params Schema | `RebootParams` |
| Result Schema | `LifecycleActionResult` |
| 是否触发事件 | 是，状态实际变化后触发 `system.lifecycleStateChanged`。 |
| 幂等性 / 异步性 | 建议幂等；重复提交相同目标状态应成功，可不重复触发事件。 |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_ARGUMENT`, `INVALID_STATE`, `BUSY`, `PERMISSION_DENIED` |

#### 3.8.1 请求参数 Params：`RebootParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `target` | string | no | target id | `default` | 动作对象；具体 target 集合由 capability 声明。 |
| `reason` | string | no | caller-defined reason | omitted | 调用方给出的动作原因。 |

#### 3.8.2 返回结果 Result：`LifecycleActionResult`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `accepted` | boolean | yes | `true`, `false` | none | 设备是否接受动作请求。 |
| `actionId` | string | no | opaque action id | omitted | 动作 ID，用于日志或异步关联。 |

#### 3.8.3 d block 示例

request:

```json
{
  "id": 108,
  "method": "system.reboot",
  "params": {
    "target": "system",
    "delaySeconds": 30,
    "policy": "graceful",
    "reason": "firmware_update_complete"
  }
}
```

success:

```json
{
  "id": 108,
  "status": {
    "ok": true,
    "code": 0
  },
  "result": {
    "accepted": true,
    "actionId": "reboot-20260621-001"
  }
}
```

#### 3.8.4 可能触发的事件

| Event | 触发条件 | Payload Schema | 客户端处理建议 |
|---|---|---|---|
| `system.lifecycleStateChanged` | 该方法导致状态、配置或动作状态实际变化。 | `LifecycleStateChangedEvent` | 可直接更新 UI；需要完整状态时调用对应 get method 校准。 |

#### 3.8.5 错误

| 错误 | 场景 | 返回建议 |
|---|---|---|
| `NOT_SUPPORTED` | 设备不支持该 feature、method、target 或 scope。 | 返回 unsupported feature/method/target。 |
| `INVALID_ARGUMENT` | 请求字段非法、枚举非法或范围非法。 | 返回具体字段路径和合法范围。 |
| `PERMISSION_DENIED` | 调用方无权执行该操作。 | 返回权限错误。 |
| `BUSY` | 设备正在处理冲突操作。 | 建议稍后重试。 |

### 3.9 `system.shutdown`

**用途**：查询已配置的重启计划。

| 项 | 内容 |
|---|---|
| 调用类型 | action |
| Params Schema | `ShutdownParams` |
| Result Schema | `LifecycleActionResult` |
| 是否触发事件 | 是，状态实际变化后触发 `system.lifecycleStateChanged`。 |
| 幂等性 / 异步性 | 建议幂等；重复提交相同目标状态应成功，可不重复触发事件。 |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_ARGUMENT`, `INVALID_STATE`, `BUSY`, `PERMISSION_DENIED` |

#### 3.9.1 请求参数 Params：`ShutdownParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `target` | string | no | target id | `default` | 动作对象；具体 target 集合由 capability 声明。 |
| `reason` | string | no | caller-defined reason | omitted | 调用方给出的动作原因。 |

#### 3.9.2 返回结果 Result：`LifecycleActionResult`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `accepted` | boolean | yes | `true`, `false` | none | 设备是否接受动作请求。 |
| `actionId` | string | no | opaque action id | omitted | 动作 ID，用于日志或异步关联。 |

#### 3.9.3 d block 示例

request:

```json
{
  "id": 109,
  "method": "system.shutdown",
  "params": {
    "target": "system",
    "delaySeconds": 60,
    "policy": "graceful",
    "reason": "scheduled_maintenance"
  }
}
```

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
    "actionId": "shutdown-20260621-001"
  }
}
```

#### 3.9.4 可能触发的事件

| Event | 触发条件 | Payload Schema | 客户端处理建议 |
|---|---|---|---|
| `system.lifecycleStateChanged` | 该方法导致状态、配置或动作状态实际变化。 | `LifecycleStateChangedEvent` | 可直接更新 UI；需要完整状态时调用对应 get method 校准。 |

#### 3.9.5 错误

| 错误 | 场景 | 返回建议 |
|---|---|---|
| `NOT_SUPPORTED` | 设备不支持该 feature、method、target 或 scope。 | 返回 unsupported feature/method/target。 |
| `INVALID_ARGUMENT` | 请求字段非法、枚举非法或范围非法。 | 返回具体字段路径和合法范围。 |
| `PERMISSION_DENIED` | 调用方无权执行该操作。 | 返回权限错误。 |
| `BUSY` | 设备正在处理冲突操作。 | 建议稍后重试。 |

## 4. 事件 Events

### 4.0 事件速览

| Event | 触发条件 | Payload Schema | 客户端处理建议 | 状态 |
|---|---|---|---|---|
| `system.lifecycleStateChanged` | lifecycle 状态、计划或 reboot/shutdown 动作状态变化 | `LifecycleStateChangedEvent` | 更新 UI 或调用对应 get method 校准 | draft |
| `system.stateChanged` | runtime online、health、uptime 或 recovery 状态变化 | `StateChangedEvent` | 更新 UI 或调用对应 get method 校准 | draft |

### 4.1 `system.lifecycleStateChanged`

**触发条件**：lifecycle 状态、计划或 reboot/shutdown 动作状态变化。

#### 4.1.1 Payload：`LifecycleStateChangedEvent`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `changedFields` | string[] | no | field path array | omitted | 变化字段路径。 |
| `state` | object | no | see schema | omitted | 变化后的状态、配置或摘要。 |
| `source` | string enum | no | `remoteApp`, `localPanel`, `devicePolicy`, `adapter`, `unknown` | `unknown` | 状态变化来源。 |
| `reason` | string enum | no | feature-specific | `unknown` | 状态变化原因。 |
| `stateRevision` | uint32 | no | monotonic counter | omitted | 状态版本，用于多端同步和去重。 |

#### 4.1.2 d block 示例

```json
{
  "event": "system.lifecycleStateChanged",
  "intent": 1,
  "data": {
    "changedFields": [
      "state"
    ],
    "state": {
      "target": "device",
      "allowRemoteReboot": true,
      "drainTimeoutSeconds": 30
    },
    "source": "remoteApp",
    "reason": "lifecycle_action",
    "stateRevision": 1
  }
}
```

#### 4.1.3 客户端处理建议

| 场景 | 建议 |
|---|---|
| payload 是完整状态 | 可直接更新 UI 或本地缓存。 |
| payload 是变化片段 | 调用对应 get method 校准完整状态。 |
| event 丢失或重连 | 重连后主动调用 get method 校准。 |

#### 4.1.4 规则

- Event MUST 使用 `op=6`。
- Event MUST NOT 携带 `d.id`。
- Event payload MUST 放在 `d.data` 中。

### 4.2 `system.stateChanged`

**触发条件**：runtime online、health、uptime 或 recovery 状态变化。

#### 4.2.1 Payload：`StateChangedEvent`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `changedFields` | string[] | no | field path array | omitted | 变化字段路径。 |
| `state` | object | no | see schema | omitted | 变化后的状态、配置或摘要。 |
| `source` | string enum | no | `remoteApp`, `localPanel`, `devicePolicy`, `adapter`, `unknown` | `unknown` | 状态变化来源。 |
| `reason` | string enum | no | feature-specific | `unknown` | 状态变化原因。 |
| `stateRevision` | uint32 | no | monotonic counter | omitted | 状态版本，用于多端同步和去重。 |

#### 4.2.2 d block 示例

```json
{
  "event": "system.stateChanged",
  "intent": 1,
  "data": {
    "changedFields": [
      "state"
    ],
    "state": {
      "target": "device",
      "allowRemoteReboot": true,
      "drainTimeoutSeconds": 30
    },
    "source": "remoteApp",
    "reason": "lifecycle_action",
    "stateRevision": 1
  }
}
```

#### 4.2.3 客户端处理建议

| 场景 | 建议 |
|---|---|
| payload 是完整状态 | 可直接更新 UI 或本地缓存。 |
| payload 是变化片段 | 调用对应 get method 校准完整状态。 |
| event 丢失或重连 | 重连后主动调用 get method 校准。 |

#### 4.2.4 规则

- Event MUST 使用 `op=6`。
- Event MUST NOT 携带 `d.id`。
- Event payload MUST 放在 `d.data` 中。

## 5. Capability

Capability name: `system.lifecycle`。

设备通过 capability 声明是否支持该 feature，以及支持哪些范围、模式、对象或约束。Capability 字段只描述“设备能做什么”，不得混入 method params/result 或 event payload。

| 能力字段 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `capability` | string | yes | fixed `system.lifecycle` | none | capability 名称。 |
| `supportedTargets` | string[] | no | target id array | omitted | 支持的对象、通道、端口、组件或 scope。 |

## 6. 字段 / Schemas

### 6.1 Schema 层级速览

```text
LifecycleCapability
  capability / supportedTargets
LifecycleState
  target / status / sampledAt
LifecycleChangedEvent
  changedFields / state / source / reason / stateRevision
```

### 6.2 请求与响应 Schemas

| Schema | 用途 | 字段定义 |
|---|---|---|
| `GetRebootScheduleParams` | `system.getRebootSchedule` request params | 见 `system.getRebootSchedule` 方法小节。 |
| `LifecycleScheduleList` | `system.getRebootSchedule` result | 见 `system.getRebootSchedule` 方法小节。 |
| `SetRebootScheduleParams` | `system.setRebootSchedule` request params | 见 `system.setRebootSchedule` 方法小节。 |
| `SetLifecycleScheduleResult` | `system.setRebootSchedule` result | 见 `system.setRebootSchedule` 方法小节。 |
| `CancelRebootScheduleParams` | `system.cancelRebootSchedule` request params | 见 `system.cancelRebootSchedule` 方法小节。 |
| `CancelScheduleResult` | `system.cancelRebootSchedule` result | 见 `system.cancelRebootSchedule` 方法小节。 |
| `GetShutdownScheduleParams` | `system.getShutdownSchedule` request params | 见 `system.getShutdownSchedule` 方法小节。 |
| `LifecycleScheduleList` | `system.getShutdownSchedule` result | 见 `system.getShutdownSchedule` 方法小节。 |
| `SetShutdownScheduleParams` | `system.setShutdownSchedule` request params | 见 `system.setShutdownSchedule` 方法小节。 |
| `SetLifecycleScheduleResult` | `system.setShutdownSchedule` result | 见 `system.setShutdownSchedule` 方法小节。 |
| `CancelShutdownScheduleParams` | `system.cancelShutdownSchedule` request params | 见 `system.cancelShutdownSchedule` 方法小节。 |
| `CancelScheduleResult` | `system.cancelShutdownSchedule` result | 见 `system.cancelShutdownSchedule` 方法小节。 |
| `GetLifecycleStateParams` | `system.getLifecycleState` request params | 见 `system.getLifecycleState` 方法小节。 |
| `LifecycleState` | `system.getLifecycleState` result | 见 `system.getLifecycleState` 方法小节。 |
| `RebootParams` | `system.reboot` request params | 见 `system.reboot` 方法小节。 |
| `LifecycleActionResult` | `system.reboot` result | 见 `system.reboot` 方法小节。 |
| `ShutdownParams` | `system.shutdown` request params | 见 `system.shutdown` 方法小节。 |
| `LifecycleActionResult` | `system.shutdown` result | 见 `system.shutdown` 方法小节。 |

### 6.3 Capability Schemas


### 6.4 Event Schemas

| Schema | Event | 字段定义 |
|---|---|---|
| `LifecycleStateChangedEvent` | `system.lifecycleStateChanged` | 见 `system.lifecycleStateChanged` 事件小节。 |
| `StateChangedEvent` | `system.stateChanged` | 见 `system.stateChanged` 事件小节。 |