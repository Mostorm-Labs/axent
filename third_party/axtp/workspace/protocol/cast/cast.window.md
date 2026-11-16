---
status: generated
contract: true
generated: true
domain: cast
feature: cast.window
registry: ../../../../contract/registry/domains/cast/domain.yaml
lastReviewed: 2026-06-22
---

# cast.window

## 0. 采纳状态

| 项目 | 内容 |
|---|---|
| 当前状态 | generated；已写入 `../../../../contract/registry/domains/cast/domain.yaml`，并已刷新到 `contract/protocol/axtp.protocol.yaml` 与 `contract/generated/**`。 |
| 是否可直接实现 | 是，但实现合同以 `contract/protocol/axtp.protocol.yaml` / `contract/generated/**` 为准。 |
| 本次采纳 | `cast.getWindowState`、`cast.setWindowState`、`cast.windowChanged`、窗口模式和 bounds 字段外形。 |
| 未采纳 | 无窗口时预设行为和 show/hide scope 等 Review Items 不属于已生成合同；后续语义变更走 `amend-adopted-protocol`。 |

## 1. Purpose

控制投屏窗口可见性、全屏、置顶和 normal 还原状态。它不建模完整 OS 窗口管理，也不处理 PIN 窗口。

## 2. Candidate Surface

| Method / Event | Purpose | Schema | Notes |
|---|---|---|---|
| `cast.getWindowState` | 查询投屏窗口状态。 | `CastGetWindowStateParams` -> `CastWindowState` | query |
| `cast.setWindowState` | 设置全屏、置顶或 normal 模式。 | `CastSetWindowStateParams` -> `CastWindowState` | command |
| `cast.windowChanged` | 投屏窗口状态变化。 | `CastWindowChangedEvent` | event |

## 3. Methods

### 3.0 方法速览

方法概览见第 2 章；本节只保留每个 method 的最小 request / success 示例。

### 3.1 `cast.getWindowState`

查询当前是否有投屏窗口、是否可见、是否全屏和是否置顶。

#### 3.1.1 d block 示例

request:

```json
{
  "id": 3401,
  "method": "cast.getWindowState",
  "params": {}
}
```

success:

```json
{
  "id": 3401,
  "status": {
    "ok": true,
    "code": 0
  },
  "result": {
    "hasWindow": true,
    "visible": true,
    "mode": "fullscreen",
    "fullscreen": true,
    "alwaysOnTop": false,
    "sessionId": "cast_sess_001",
    "bounds": {
      "x": 0,
      "y": 0,
      "width": 1920,
      "height": 1080
    },
    "updatedAt": "2026-06-22T10:30:00Z"
  }
}
```

### 3.2 `cast.setWindowState`

设置窗口状态。`mode=normal` 表示退出全屏、取消置顶，并恢复进入全屏 / 置顶前的窗口尺寸和位置。

#### 3.2.1 d block 示例

request:

```json
{
  "id": 3402,
  "method": "cast.setWindowState",
  "params": {
    "mode": "normal"
  }
}
```

success:

```json
{
  "id": 3402,
  "status": {
    "ok": true,
    "code": 0
  },
  "result": {
    "hasWindow": true,
    "visible": true,
    "mode": "normal",
    "fullscreen": false,
    "alwaysOnTop": false,
    "sessionId": "cast_sess_001",
    "bounds": {
      "x": 160,
      "y": 120,
      "width": 1280,
      "height": 720
    },
    "changedFields": [
      "mode",
      "fullscreen",
      "alwaysOnTop",
      "bounds"
    ],
    "updatedAt": "2026-06-22T10:31:00Z"
  }
}
```

## 4. State And Events

| Field | Meaning |
|---|---|
| `hasWindow` | 当前是否存在投屏窗口。 |
| `visible` | 窗口是否可见。 |
| `mode` | `normal` 或 `fullscreen`。 |
| `fullscreen` | 是否全屏；`mode=normal` 时必须为 `false`。 |
| `alwaysOnTop` | 是否置顶；`mode=normal` 时必须为 `false`。 |
| `sessionId` | 关联会话 id。 |
| `applyWhenNoWindow` | `reject` 或 `remember`；无窗口行为仍待确认。 |

`cast.windowChanged` 应携带变化后的完整窗口摘要。事件丢失时，客户端调用 `cast.getWindowState` 校准。

### 4.1 Event 示例

event:

```json
{
  "event": "cast.windowChanged",
  "intent": 1,
  "data": {
    "changedFields": [
      "mode",
      "fullscreen",
      "alwaysOnTop",
      "bounds"
    ],
    "state": {
      "hasWindow": true,
      "visible": true,
      "mode": "normal",
      "fullscreen": false,
      "alwaysOnTop": false,
      "sessionId": "cast_sess_001",
      "bounds": {
        "x": 160,
        "y": 120,
        "width": 1280,
        "height": 720
      }
    },
    "reason": "externalSet",
    "updatedAt": "2026-06-22T10:31:00Z"
  }
}
```

## 5. Rules

- `mode=normal` 后必须满足 `fullscreen=false` 且 `alwaysOnTop=false`。
- `mode=normal` 应恢复进入全屏 / 置顶前记录的窗口尺寸和位置；历史位置不可用时使用 runtime 默认普通窗口位置。
- `mode=normal` 不应同时携带 `fullscreen=true` 或 `alwaysOnTop=true`。
- `CastWindowState.bounds` 表示当前窗口位置；进入全屏前的普通窗口位置和本次还原位置不作为独立协议字段暴露。
- 完整窗口坐标、多显示器选择和系统级窗口管理不进入本 feature。
- 当前不新增 public show / hide method；隐藏 / 显示由会话和本地 UI 驱动。

## 6. Errors

| Error | Scenario |
|---|---|
| `INVALID_ARGUMENT` | mode、fullscreen、alwaysOnTop 字段冲突。 |
| `INVALID_STATE` | 无窗口或当前状态不允许切换。 |
| `UNAVAILABLE` | 窗口服务不可用。 |

## 7. Review Items

| 问题 | 影响 | 当前建议 | 状态 |
|---|---|---|---|
| 无活动投屏窗口时是否允许预设窗口状态？ | runtime / UI | 默认拒绝，capability 可声明支持 remember。 | `[REVIEW-ASK]` |
| 是否需要 public show / hide window method？ | scope | 当前不需要，隐藏 / 显示由会话和本地 UI 驱动。 | `[REVIEW-DRAFT]` |

## 8. Amendment History

| Date | Change | Compatibility |
|---|---|---|
| 2026-06-24 | 从 `CastWindowState` 中移除 `previousNormalBounds` 和 `restoredBounds`；统一使用 `bounds` 表达当前窗口位置。 | draft 字段删除；`0x08` / `0x09` 不复用于其他含义。 |

## 9. Schema Reference

> 本节按当前 `contract/registry/domains/cast/domain.yaml` 整理字段事实；`Required=yes` 表示编码数据必须携带该字段，`Required=no` 表示可省略。`Empty` schema 无字段，未展开。

### CastWindowState

Cast window visibility, mode, and bounds state.

| Field | Required | Type | Field ID | Constraints / default | Description |
|---|---:|---|---:|---|---|
| `hasWindow` | yes | `bool` | `0x01` | - | Whether a cast window currently exists. |
| `visible` | yes | `bool` | `0x02` | - | Whether the cast window is visible. |
| `mode` | yes | `enum` | `0x03` | enum=normal/fullscreen | Current cast window mode. |
| `fullscreen` | yes | `bool` | `0x04` | - | Whether the cast window is fullscreen. |
| `alwaysOnTop` | yes | `bool` | `0x05` | - | Whether the cast window is topmost. |
| `sessionId` | no | `string` | `0x06` | maxLength=128 | Receiver-local session id associated with the window. |
| `bounds` | no | `CastRect` | `0x07` | - | Current window bounds when available. |
| `changedFields` | no | `Array<string>` | `0x0A` | itemType=string | Field names changed by the latest operation or event. |
| `updatedAt` | no | `string` | `0x0B` | maxLength=64 | Timestamp for this window state. |

### CastRect

Cast window rectangle in screen coordinates.

| Field | Required | Type | Field ID | Constraints / default | Description |
|---|---:|---|---:|---|---|
| `x` | yes | `int32` | `0x01` | - | Rectangle left coordinate. |
| `y` | yes | `int32` | `0x02` | - | Rectangle top coordinate. |
| `width` | yes | `uint32` | `0x03` | - | Rectangle width in pixels. |
| `height` | yes | `uint32` | `0x04` | - | Rectangle height in pixels. |

### CastSetWindowStateParams

Request to update cast window state.

| Field | Required | Type | Field ID | Constraints / default | Description |
|---|---:|---|---:|---|---|
| `mode` | no | `enum` | `0x01` | enum=normal/fullscreen | Target cast window mode. |
| `fullscreen` | no | `bool` | `0x02` | - | Whether the cast window should be fullscreen. |
| `alwaysOnTop` | no | `bool` | `0x03` | - | Whether the cast window should stay above normal windows. |
| `bounds` | no | `CastRect` | `0x04` | - | Optional target normal-mode window bounds. |

### CastWindowChangedEvent

Event payload for cast window state changes.

| Field | Required | Type | Field ID | Constraints / default | Description |
|---|---:|---|---:|---|---|
| `changedFields` | yes | `Array<string>` | `0x01` | itemType=string | Field names changed by this event. |
| `state` | yes | `CastWindowState` | `0x02` | - | Window state after the change. |
| `reason` | no | `enum` | `0x03` | enum=externalSet/localUi/sessionStarted/sessionStopped/unknown | Change reason. |
| `updatedAt` | no | `string` | `0x04` | maxLength=64 | Timestamp for this event. |

### CastWindowCapability

Capability descriptor for cast.window.

| Field | Required | Type | Field ID | Constraints / default | Description |
|---|---:|---|---:|---|---|
| `supportsFullscreen` | no | `bool` | `0x01` | default=true | Whether fullscreen window mode is supported. |
| `supportsAlwaysOnTop` | no | `bool` | `0x02` | default=true | Whether topmost window state is supported. |
| `supportsNormalRestore` | no | `bool` | `0x03` | default=true | Whether normal mode can restore remembered normal-mode bounds. |
| `noWindowPolicy` | no | `enum` | `0x04` | enum=reject/remember/runtimeDefault | Receiver policy when a window command is received without an active cast window. |
