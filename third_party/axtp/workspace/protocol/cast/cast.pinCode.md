---
status: generated
contract: true
generated: true
domain: cast
feature: cast.pinCode
registry: ../../../../contract/registry/domains/cast/domain.yaml
lastReviewed: 2026-06-22
---

# cast.pinCode

## 0. 采纳状态

| 项目 | 内容 |
|---|---|
| 当前状态 | generated；已写入 `../../../../contract/registry/domains/cast/domain.yaml`，并已刷新到 `contract/protocol/axtp.protocol.yaml` 与 `contract/generated/**`。 |
| 是否可直接实现 | 是，但实现合同以 `contract/protocol/axtp.protocol.yaml` / `contract/generated/**` 为准。 |
| 本次采纳 | PIN 配置、设置、required/auth failed 事件、明文可见性和脱敏字段外形。 |
| 未采纳 | PIN 格式、长度、进行中鉴权影响等 Review Items 不属于已生成合同；后续语义变更走 `amend-adopted-protocol`。 |

## 1. Purpose

管理 AirPlay 投屏密码保护、当前有效 PIN、展示策略和鉴权失败通知。业务已确认：密码保护默认开启，默认 PIN 自动生成，授权 response / event 可携带明文 PIN，但日志、诊断和错误摘要必须脱敏。

## 2. Candidate Surface

| Method / Event | Purpose | Schema | Notes |
|---|---|---|---|
| `cast.getPinCodeConfig` | 查询密码保护、PIN 状态和展示策略。 | `CastGetPinCodeConfigParams` -> `CastPinCodeConfig` | query |
| `cast.setPinCodeConfig` | 开关密码保护并设置展示策略。 | `CastSetPinCodeConfigParams` -> `CastPinCodeConfig` | command |
| `cast.setPinCode` | 指定当前有效 PIN。 | `CastSetPinCodeParams` -> `CastPinCodeConfig` | command |
| `cast.pinCodeChanged` | PIN 配置或状态变化。 | `CastPinCodeChangedEvent` | event |
| `cast.pinCodeRequired` | 新投屏请求等待输入 PIN。 | `CastPinCodeRequiredEvent` | event |
| `cast.pinCodeAuthFailed` | PIN 鉴权失败、超时或取消。 | `CastPinCodeAuthFailedEvent` | event |

## 3. Methods

### 3.0 方法速览

方法概览见第 2 章；本节只保留每个 method 的最小 request / success 示例。

### 3.1 `cast.getPinCodeConfig`

查询是否启用密码保护、是否存在 PIN、是否允许返回明文 PIN。

#### 3.1.1 d block 示例

request:

```json
{
  "id": 3301,
  "method": "cast.getPinCodeConfig",
  "params": {
    "includeSecret": true
  }
}
```

success:

```json
{
  "id": 3301,
  "status": {
    "ok": true,
    "code": 0
  },
  "result": {
    "enabled": true,
    "hasPinCode": true,
    "pinCode": "482913",
    "pinDisplay": "authorizedClients",
    "generatedBy": "nearcast",
    "visibility": "authorizedOnly",
    "expiresAt": null,
    "redactionRequired": true,
    "updatedAt": "2026-06-22T10:30:00Z"
  }
}
```

### 3.2 `cast.setPinCodeConfig`

开启或关闭密码保护；关闭时必须允许 source 无密码投屏。

#### 3.2.1 d block 示例

request:

```json
{
  "id": 3302,
  "method": "cast.setPinCodeConfig",
  "params": {
    "enabled": true,
    "pinDisplay": "authorizedClients"
  }
}
```

success:

```json
{
  "id": 3302,
  "status": {
    "ok": true,
    "code": 0
  },
  "result": {
    "enabled": true,
    "hasPinCode": true,
    "pinDisplay": "authorizedClients",
    "generatedBy": "nearcast",
    "visibility": "authorizedOnly",
    "changedFields": [
      "enabled",
      "pinDisplay"
    ],
    "updatedAt": "2026-06-22T10:31:00Z"
  }
}
```

### 3.3 `cast.setPinCode`

指定当前有效 PIN；格式、长度和生效时机待采纳时固定。

#### 3.3.1 d block 示例

request:

```json
{
  "id": 3303,
  "method": "cast.setPinCode",
  "params": {
    "pinCode": "482913",
    "expirePrevious": true
  }
}
```

success:

```json
{
  "id": 3303,
  "status": {
    "ok": true,
    "code": 0
  },
  "result": {
    "enabled": true,
    "hasPinCode": true,
    "pinCode": "482913",
    "pinDisplay": "authorizedClients",
    "generatedBy": "external",
    "expirePrevious": true,
    "redactionRequired": true,
    "changedFields": [
      "pinCode",
      "generatedBy"
    ],
    "updatedAt": "2026-06-22T10:32:00Z"
  }
}
```

## 4. State And Events

| Field | Meaning |
|---|---|
| `enabled` | 是否启用密码保护。 |
| `hasPinCode` | 当前是否有有效 PIN。 |
| `pinCode` | 明文 PIN；仅授权 response / event 可见。 |
| `pinDisplay` | `hidden`、`authorizedClients`、`localUi`、`both`。 |
| `source` | 等待鉴权或鉴权失败的 source 摘要。 |
| `authFailureReason` | `wrongPin`、`timeout`、`cancelled`、`tooManyAttempts`。 |

事件规则：

- `cast.pinCodeChanged` 用于配置、PIN 状态或明文可见性变化。
- `cast.pinCodeRequired` 用于投屏请求等待 PIN，不代表 casting。
- `cast.pinCodeAuthFailed` 用于鉴权失败，不应误报为 `cast.sessionStarted`。

### 4.1 Event 示例

pin required:

```json
{
  "event": "cast.pinCodeRequired",
  "intent": 1,
  "data": {
    "sessionId": "cast_sess_001",
    "source": {
      "name": "Qing iPhone",
      "address": "192.168.31.24"
    },
    "pinCode": "482913",
    "visibility": "authorizedOnly",
    "redactionRequired": true,
    "requestedAt": "2026-06-22T10:29:30Z"
  }
}
```

auth failed:

```json
{
  "event": "cast.pinCodeAuthFailed",
  "intent": 1,
  "data": {
    "sessionId": "cast_sess_001",
    "source": {
      "name": "Qing iPhone"
    },
    "authFailureReason": "wrongPin",
    "attemptCount": 2,
    "failedAt": "2026-06-22T10:29:45Z"
  }
}
```

## 5. Rules

- 密码保护默认开启；默认 PIN 由 NearCast 或 UxPlay 内部生成。
- 密码保护关闭时，source 必须可以无密码投屏。
- 明文 PIN 可以返回给控制端；普通日志、诊断和错误摘要必须脱敏。
- PIN 配置和读取按朴素可调用状态操作设计，不在本 feature 内拆分权限 scope。
- PIN 展示是 UI 行为，不需要单独建 public show / hide method。

## 6. Errors

| Error | Scenario |
|---|---|
| `INVALID_ARGUMENT` | PIN 格式、展示策略或字段组合非法。 |
| `INVALID_STATE` | 当前 backend / session 状态不允许修改 PIN。 |
| `UNAVAILABLE` | backend PIN service 不可用。 |

## 7. Review Items

| 问题 | 影响 | 当前建议 | 状态 |
|---|---|---|---|
| PIN 格式、长度和字符集如何定义？ | backend / schema | 采纳前按 UxPlay / NearCast 实现收敛。 | `[REVIEW-ASK]` |
| 修改 PIN 是否影响正在进行的鉴权？ | behavior | 默认影响后续请求；进行中鉴权策略待确认。 | `[REVIEW-ASK]` |
| 明文 PIN 的可见性和脱敏如何验证？ | security | 保留明文 response / event，日志和诊断必须脱敏。 | `[REVIEW-ASK]` |

## 8. Schema Reference

> 本节按当前 `contract/registry/domains/cast/domain.yaml` 整理字段事实；`Required=yes` 表示编码数据必须携带该字段，`Required=no` 表示可省略。`Empty` schema 无字段，未展开。

### CastGetPinCodeConfigParams

Selector for cast PIN protection configuration.

| Field | Required | Type | Field ID | Constraints / default | Description |
|---|---:|---|---:|---|---|
| `includeSecret` | no | `bool` | `0x01` | default=false | Whether authorized clients request plaintext PIN material. |

### CastPinCodeConfig

Cast PIN protection state and optional plaintext PIN value.

| Field | Required | Type | Field ID | Constraints / default | Description |
|---|---:|---|---:|---|---|
| `enabled` | yes | `bool` | `0x01` | default=true | Whether PIN protection is enabled. |
| `hasPinCode` | yes | `bool` | `0x02` | - | Whether a current PIN exists. |
| `pinCode` | no | `string` | `0x03` | - | Plaintext PIN value when visible to the caller. |
| `pinDisplay` | no | `enum` | `0x04` | enum=hidden/authorizedClients/localUi/both | Where the current PIN may be displayed. |
| `generatedBy` | no | `enum` | `0x05` | enum=nearcast/uxplay/external/unknown | Component or actor that generated the current PIN. |
| `visibility` | no | `enum` | `0x06` | enum=hidden/authorizedOnly/localUi/both | Visibility policy for the PIN value. |
| `expiresAt` | no | `string` | `0x07` | maxLength=64 | Expiration timestamp when applicable. |
| `redactionRequired` | no | `bool` | `0x08` | default=true | Whether logs, diagnostics, and error summaries must redact the PIN. |
| `changedFields` | no | `Array<string>` | `0x09` | itemType=string | Field names changed by the latest operation or event. |
| `updatedAt` | no | `string` | `0x0A` | maxLength=64 | Timestamp for this PIN state. |
| `redacted` | no | `bool` | `0x0B` | - | Whether sensitive fields were withheld in this snapshot. |

### CastSetPinCodeConfigParams

Request to update cast PIN protection configuration.

| Field | Required | Type | Field ID | Constraints / default | Description |
|---|---:|---|---:|---|---|
| `enabled` | no | `bool` | `0x01` | - | Whether PIN protection is enabled. |
| `pinDisplay` | no | `enum` | `0x02` | enum=hidden/authorizedClients/localUi/both | Where the current PIN may be displayed. |
| `rotatePin` | no | `bool` | `0x03` | default=false | Whether the receiver should rotate or regenerate the PIN. |
| `visibility` | no | `enum` | `0x04` | enum=hidden/authorizedOnly/localUi/both | Visibility policy for responses and events. |

### CastSetPinCodeParams

Request to set the active cast PIN value.

| Field | Required | Type | Field ID | Constraints / default | Description |
|---|---:|---|---:|---|---|
| `pinCode` | yes | `string` | `0x01` | - | Opaque PIN value; concrete format is backend or product policy. |
| `expirePrevious` | no | `bool` | `0x02` | default=true | Whether prior PIN material should stop being accepted. |
| `visibility` | no | `enum` | `0x03` | enum=hidden/authorizedOnly/localUi/both | Visibility policy for the new PIN. |

### CastPinCodeChangedEvent

Event payload for PIN configuration or PIN state changes.

| Field | Required | Type | Field ID | Constraints / default | Description |
|---|---:|---|---:|---|---|
| `changedFields` | yes | `Array<string>` | `0x01` | itemType=string | Field names changed by this event. |
| `config` | yes | `CastPinCodeConfig` | `0x02` | - | PIN state after the change. |
| `reason` | no | `enum` | `0x03` | enum=externalSet/localUi/generated/backendChanged/unknown | Change reason. |
| `updatedAt` | no | `string` | `0x04` | maxLength=64 | Timestamp for this event. |

### CastPinCodeRequiredEvent

Event payload for a session waiting for PIN authentication.

| Field | Required | Type | Field ID | Constraints / default | Description |
|---|---:|---|---:|---|---|
| `sessionId` | no | `string` | `0x01` | maxLength=128 | Receiver-local session id. |
| `source` | no | `CastSourceSummary` | `0x02` | - | Source waiting for authentication. |
| `pinCode` | no | `string` | `0x03` | - | Plaintext PIN value when visible to the event subscriber. |
| `visibility` | no | `enum` | `0x04` | enum=hidden/authorizedOnly/localUi/both | Visibility policy for this event payload. |
| `redactionRequired` | no | `bool` | `0x05` | default=true | Whether logs and diagnostics must redact this PIN value. |
| `requestedAt` | no | `string` | `0x06` | maxLength=64 | Timestamp when PIN input was requested. |

### CastSourceSummary

Summary of a cast source device or local AXTP sender.

| Field | Required | Type | Field ID | Constraints / default | Description |
|---|---:|---|---:|---|---|
| `name` | no | `string` | `0x01` | maxLength=128 | User-visible source name when known. |
| `model` | no | `string` | `0x02` | maxLength=128 | Source model identifier when known. |
| `address` | no | `string` | `0x03` | maxLength=128 | Network or transport address summary when safe to expose. |
| `sourceId` | no | `string` | `0x04` | maxLength=128 | Receiver-local source identifier. |
| `protocol` | no | `enum` | `0x05` | enum=airplay/hid/unknown | Protocol path that produced the source summary. |

### CastPinCodeAuthFailedEvent

Event payload for failed PIN authentication.

| Field | Required | Type | Field ID | Constraints / default | Description |
|---|---:|---|---:|---|---|
| `sessionId` | no | `string` | `0x01` | maxLength=128 | Receiver-local session id. |
| `source` | no | `CastSourceSummary` | `0x02` | - | Source that failed authentication. |
| `authFailureReason` | no | `enum` | `0x03` | enum=wrongPin/timeout/cancelled/tooManyAttempts/unknown | Authentication failure reason. |
| `attemptCount` | no | `uint16` | `0x04` | - | Attempt count visible to the receiver. |
| `failedAt` | no | `string` | `0x05` | maxLength=64 | Timestamp when authentication failed. |

### CastPinCodeCapability

Capability descriptor for cast.pinCode.

| Field | Required | Type | Field ID | Constraints / default | Description |
|---|---:|---|---:|---|---|
| `defaultEnabled` | no | `bool` | `0x01` | default=true | Whether PIN protection is enabled by default. |
| `supportsPlaintextResponse` | no | `bool` | `0x02` | default=true | Whether authorized responses or events may carry plaintext PIN values. |
| `supportedPinDisplays` | no | `Array<string>` | `0x03` | itemType=string | PIN display policies supported by the receiver. |
| `supportsGeneratedPin` | no | `bool` | `0x04` | default=true | Whether the receiver can generate a default PIN. |
| `redactionRequired` | no | `bool` | `0x05` | default=true | Whether logs, diagnostics, and error summaries must redact PIN values. |
