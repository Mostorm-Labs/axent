---
status: generated
contract: true
generated: true
domain: cast
feature: cast.backend
registry: ../../../../contract/registry/domains/cast/domain.yaml
lastReviewed: 2026-06-22
---

# cast.backend

## 0. 采纳状态

| 项目 | 内容 |
|---|---|
| 当前状态 | generated；已写入 `../../../../contract/registry/domains/cast/domain.yaml`，并已刷新到 `contract/protocol/axtp.protocol.yaml` 与 `contract/generated/**`。 |
| 是否可直接实现 | 是，但实现合同以 `contract/protocol/axtp.protocol.yaml` / `contract/generated/**` 为准。 |
| 本次采纳 | `cast.getBackendStatus`、`cast.restartBackend`、`cast.backendChanged` 和 backend 状态字段外形。 |
| 未采纳 | Review Items 中仍待确认的事件顺序等行为策略不属于已生成合同；后续语义变更走 `amend-adopted-protocol`。 |

## 1. Purpose

查询和重启投屏 backend。当前 backend type 为 UxPlay；重启 backend 不等同于退出 Launcher、重启 receiver runtime 或重启设备。

## 2. Candidate Surface

| Method / Event | Purpose | Schema | Notes |
|---|---|---|---|
| `cast.getBackendStatus` | 查询 backend 状态和最近错误。 | `CastGetBackendStatusParams` -> `CastBackendStatus` | query |
| `cast.restartBackend` | 重启 backend。 | `CastRestartBackendParams` -> `CastRestartBackendResult` | command，触发 `cast.backendChanged` |
| `cast.backendChanged` | backend 状态变化。 | `CastBackendChangedEvent` | event |

## 3. Methods

### 3.0 方法速览

方法概览见第 2 章；本节只保留每个 method 的最小 request / success 示例。

### 3.1 `cast.getBackendStatus`

返回 backend type、运行状态、可发现性、进程摘要和最近错误。

#### 3.1.1 d block 示例

request:

```json
{
  "id": 3501,
  "method": "cast.getBackendStatus",
  "params": {
    "includeLastError": true
  }
}
```

success:

```json
{
  "id": 3501,
  "status": {
    "ok": true,
    "code": 0
  },
  "result": {
    "backendType": "uxplay",
    "state": "ready",
    "discoverable": true,
    "pid": 48216,
    "version": "uxplay-nearcast-2026.06",
    "activeSessionId": "cast_sess_001",
    "restartInProgress": false,
    "lastError": null,
    "updatedAt": "2026-06-22T10:30:00Z"
  }
}
```

### 3.2 `cast.restartBackend`

请求重启 UxPlay backend。如存在活动投屏，会先强制结束当前会话，并通过 session/backend 事件上报。

#### 3.2.1 d block 示例

request:

```json
{
  "id": 3502,
  "method": "cast.restartBackend",
  "params": {
    "reason": "manualRecovery"
  }
}
```

success:

```json
{
  "id": 3502,
  "status": {
    "ok": true,
    "code": 0
  },
  "result": {
    "accepted": true,
    "backendType": "uxplay",
    "state": "restarting",
    "restartId": "backend_restart_001",
    "activeSessionEnded": true,
    "endedSessionId": "cast_sess_001",
    "sessionStopReason": "backendRestart",
    "estimatedReadyInMs": 3000,
    "updatedAt": "2026-06-22T10:33:00Z"
  }
}
```

## 4. State And Events

| Field | Meaning |
|---|---|
| `backendType` | 当前为 `uxplay`。 |
| `state` | `starting`、`ready`、`restarting`、`exited`、`failed`、`disabled`。 |
| `discoverable` | AirPlay 服务是否可被发现。 |
| `pid` | 可选进程 id。 |
| `activeSessionEnded` | 本次重启是否结束了活动投屏。 |
| `lastError` | 最近 backend 错误摘要。 |

`cast.backendChanged` 用于上报 `ready`、`restarting`、`exited`、`failed` 等低频状态。backend 重启导致会话结束时，还需要 `cast.sessionStopped(reason=backendRestart)`。

### 4.1 Event 示例

event:

```json
{
  "event": "cast.backendChanged",
  "intent": 1,
  "data": {
    "changedFields": [
      "state",
      "restartInProgress"
    ],
    "state": {
      "backendType": "uxplay",
      "state": "restarting",
      "discoverable": false,
      "restartId": "backend_restart_001",
      "activeSessionEnded": true,
      "endedSessionId": "cast_sess_001"
    },
    "reason": "manualRecovery",
    "updatedAt": "2026-06-22T10:33:00Z"
  }
}
```

## 5. Rules

- `restartBackend` 只操作 backend，不退出 Launcher。
- 活动会话不阻止重启；重启前必须结束会话并给出用户可理解原因。
- UxPlay backend 内部控制口不作为公共 AXTP contract。
- `restartBackend` 是朴素可调用状态操作，不在本 feature 内拆分权限 scope。

## 6. Errors

| Error | Scenario |
|---|---|
| `BUSY` | 已有 backend 重启正在执行。 |
| `UNAVAILABLE` | backend adapter 不可用。 |
| `INVALID_STATE` | backend 被禁用或当前状态不允许重启。 |

## 7. Review Items

| 问题 | 影响 | 当前建议 | 状态 |
|---|---|---|---|
| 重启时 session/backend 事件顺序如何固定？ | UI / conformance | 先 `backendChanged(restarting)`，再 `sessionStopped(backendRestart)`，最后 `backendChanged(ready/failed)`。 | `[REVIEW-ASK]` |

## 8. Schema Reference

> 本节按当前 `contract/registry/domains/cast/domain.yaml` 整理字段事实；`Required=yes` 表示编码数据必须携带该字段，`Required=no` 表示可省略。`Empty` schema 无字段，未展开。

### CastGetBackendStatusParams

Selector for cast backend status.

| Field | Required | Type | Field ID | Constraints / default | Description |
|---|---:|---|---:|---|---|
| `includeLastError` | no | `bool` | `0x01` | default=false | Whether to include the last backend error summary. |

### CastBackendStatus

Cast backend state, process summary, discoverability, and last error.

| Field | Required | Type | Field ID | Constraints / default | Description |
|---|---:|---|---:|---|---|
| `backendType` | yes | `enum` | `0x01` | enum=uxplay/unknown | Backend implementation type. |
| `state` | yes | `enum` | `0x02` | enum=starting/ready/restarting/exited/failed/disabled | Backend runtime state. |
| `discoverable` | yes | `bool` | `0x03` | - | Whether the cast service is discoverable by sources. |
| `pid` | no | `uint32` | `0x04` | - | Backend process id when available. |
| `version` | no | `string` | `0x05` | maxLength=128 | Backend version or build identifier. |
| `activeSessionId` | no | `string` | `0x06` | maxLength=128 | Active cast session id currently owned by the backend. |
| `restartInProgress` | yes | `bool` | `0x07` | default=false | Whether a backend restart is currently in progress. |
| `lastError` | no | `CastLastError` | `0x08` | - | Last backend error summary when requested and available. |
| `updatedAt` | no | `string` | `0x09` | maxLength=64 | Timestamp for this backend status. |

### CastLastError

Redactable backend or receiver error summary.

| Field | Required | Type | Field ID | Constraints / default | Description |
|---|---:|---|---:|---|---|
| `code` | no | `string` | `0x01` | maxLength=64 | Backend-local or AXTP-visible error code. |
| `message` | no | `string` | `0x02` | maxLength=512 | Human-readable error summary suitable for authorized clients. |
| `occurredAt` | no | `string` | `0x03` | maxLength=64 | Timestamp when the error was observed. |
| `redacted` | no | `bool` | `0x04` | default=false | Whether sensitive details were removed from this summary. |

### CastRestartBackendParams

Request to restart the cast backend.

| Field | Required | Type | Field ID | Constraints / default | Description |
|---|---:|---|---:|---|---|
| `reason` | no | `enum` | `0x01` | enum=manualRecovery/configChanged/backendUnhealthy/unknown | Caller-visible restart reason. |
| `force` | no | `bool` | `0x02` | default=false | Whether the backend adapter may force cleanup before restart. |

### CastRestartBackendResult

Result of requesting a cast backend restart.

| Field | Required | Type | Field ID | Constraints / default | Description |
|---|---:|---|---:|---|---|
| `accepted` | yes | `bool` | `0x01` | - | Whether the receiver accepted the restart request. |
| `backendType` | yes | `enum` | `0x02` | enum=uxplay/unknown | Backend implementation affected by the restart. |
| `state` | yes | `enum` | `0x03` | enum=starting/ready/restarting/exited/failed/disabled | Backend state after accepting the restart request. |
| `restartId` | no | `string` | `0x04` | maxLength=128 | Receiver-local restart operation id. |
| `activeSessionEnded` | yes | `bool` | `0x05` | - | Whether an active cast session was ended by the restart. |
| `endedSessionId` | no | `string` | `0x06` | maxLength=128 | Session ended by the restart. |
| `sessionStopReason` | no | `enum` | `0x07` | enum=backendRestart/backendExited/error/unknown | Session stop reason reported for the ended session. |
| `estimatedReadyInMs` | no | `uint32` | `0x08` | - | Estimated backend recovery time in milliseconds. |
| `updatedAt` | no | `string` | `0x09` | maxLength=64 | Timestamp for this restart result. |

### CastBackendChangedEvent

Event payload for cast backend state changes.

| Field | Required | Type | Field ID | Constraints / default | Description |
|---|---:|---|---:|---|---|
| `changedFields` | yes | `Array<string>` | `0x01` | itemType=string | Field names changed by this event. |
| `state` | yes | `CastBackendStatus` | `0x02` | - | Backend status after the change. |
| `reason` | no | `enum` | `0x03` | enum=manualRecovery/configChanged/backendUnhealthy/backendExited/unknown | Change reason. |
| `restartId` | no | `string` | `0x04` | maxLength=128 | Restart operation id when this event is restart-related. |
| `activeSessionEnded` | no | `bool` | `0x05` | - | Whether this backend change ended an active session. |
| `endedSessionId` | no | `string` | `0x06` | maxLength=128 | Session ended by this backend change. |
| `updatedAt` | no | `string` | `0x07` | maxLength=64 | Timestamp for this event. |

### CastBackendCapability

Capability descriptor for cast.backend.

| Field | Required | Type | Field ID | Constraints / default | Description |
|---|---:|---|---:|---|---|
| `backendTypes` | yes | `Array<string>` | `0x01` | itemType=string | Backend implementations controlled by the receiver. |
| `supportsRestart` | no | `bool` | `0x02` | default=true | Whether cast.restartBackend is supported. |
| `reportsProcess` | no | `bool` | `0x03` | default=true | Whether backend process metadata such as pid may be reported. |
| `supportsLastError` | no | `bool` | `0x04` | default=true | Whether last backend error summaries are available. |
