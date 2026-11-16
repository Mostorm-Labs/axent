---
status: generated
contract: true
generated: true
domain: firmware
feature: firmware.update
registry: ../../../../contract/registry/domains/firmware/domain.yaml
lastReviewed: 2026-06-15
---

# AXTP firmware.update 协议草案

版本：v0.5

归属域：`firmware`

Capability ID：`firmware.update`

数据面：Standard Framed `STREAM`，默认 Stream Profile 为 `firmware.update`。

本文是 Stage 20 `draft-business-protocol` 草案，基于 `workspace/flows/device-firmware-update.md`。本文不是 machine truth，也不是 runtime implementation contract；稳定事实必须在人工评审后进入 contract/registry/YAML，再由 Generator 生成 `contract/protocol/axtp.protocol.yaml` 和 `contract/generated/**`。

本版按评审意见收敛字段：P0 只保留“能力查询、创建会话、STREAM 上传、finish 移交、状态/进度观察”必须字段。断点续传、批次提交、Host 外部 verify/install、URL/file 暂存、A/B confirm、rollback、签名和 sha256 均作为 P1/P2 预留，不在 P0 字段表中展开。

## 0. 速读结论

| 项目 | 内容 |
|---|---|
| 协议目的 | 让 PC Host 上传单 `.bin` 或多 `.bin` 固件文件；Host 上传完调用 `firmware.finishUpdate`，设备自主 md5 校验、安装和自动重启。 |
| 当前状态 | generated；已写入 `../../../../contract/registry/domains/firmware/domain.yaml`，并已刷新到 `contract/protocol/axtp.protocol.yaml` 与 `contract/generated/**`。 |
| Stage 50 生成 | P0 最小字段集已写入 registry source 并生成；P1/P2 reserved 与 legacy open questions 未采纳。 |
| P0 主流程 | `getUpdateCapabilities` -> `beginUpdate` -> STREAM bytes -> `finishUpdate` -> state/progress events -> reconnect -> `firmware.getInfo`。 |
| P0 方法 | `firmware.getUpdateCapabilities`, `firmware.beginUpdate`, `firmware.finishUpdate`, `firmware.getUpdateState`。 |
| P0 事件 | `firmware.updateProgressReported`, `firmware.updateStateChanged`。 |
| STREAM 使用 | 固件字节只走 STREAM；`fileId` 与 `streamId` 的绑定来自 `beginUpdate` 响应。 |
| 校验和安全基线 | `[REVIEW-OK]` P0 使用 md5 完整性校验；生产包不强制签名。 |
| 多文件顺序 | `[REVIEW-OK]` 安装顺序由设备策略决定；Host 只按设备策略准备和上传文件。 |
| 重启策略 | `[REVIEW-OK]` 设备自动重启；Host 不调用 `system.reboot`。 |
| 字段收敛 | `[REVIEW-OK]` P0 去掉 `policy`、`completion`、`finishId`、`hostActionAfterFinish`、`phase`、`timestampMs`、`windowSize`、`resume*`、`missingRanges`、外部 verify/install 字段。 |
| 未决问题 | `devicePolicyVersion` 是否必填、manifest 来源、`firmware.info` 与 `device.info` 边界、A/B confirm/rollback 是否进入 P1。 |

## 1. 功能说明

`firmware.update` 定义一次固件更新会话的最小业务控制面：

- 查询设备是否支持 P0 固件更新。
- 创建更新会话，向设备提交精简 manifest。
- 通过 STREAM 上传固件字节。
- Host 调用 `finishUpdate` 后，设备接管校验、安装和自动重启。
- Host 通过事件或 `getUpdateState` 观察状态和错误。

P0 不让 Host 统一编排设备内部流程。不同设备可以在 `finishUpdate` 之后使用自己的校验、分区、写入、切槽和重启策略，只需要暴露状态和错误。

## 2. 能力边界

### 2.1 Included

| 内容 | 说明 |
|---|---|
| 能力查询 | 返回 P0 是否支持、是否多文件、md5、自动重启、chunk 上限和设备策略版本。 |
| 会话创建 | `beginUpdate` 接收精简 manifest，返回 `updateSessionId` 和 `fileId -> streamId` 绑定。 |
| STREAM 数据面绑定 | STREAM 负责传字节；业务字段不进入 STREAM header。 |
| finish 移交 | `finishUpdate` 表示 Host 已完成上传，后续由设备自主处理。 |
| 状态和错误观察 | 设备用 progress/state event 或 `getUpdateState` 暴露 receiving/verifying/installing/rebooting/confirmed/failed。 |

### 2.2 Excluded

| 内容 | 归属建议 | P0 处理 |
|---|---|---|
| STREAM ACK、window、resume、missing ranges | `stream` / future profile | 不进入 P0 字段。 |
| 通用文件暂存 | `file.transfer` | P1。 |
| URL 远程升级 | `firmware.update` extension | P1。 |
| Host 外部提交、校验、安装 | `firmware.update` advanced mode | P1 advanced；P0 用 `finishUpdate`。 |
| A/B confirm、rollback | `firmware.update` extension | P1。 |
| sha256、签名 | security extension | P1/P2；P0 只 md5。 |
| 通用设备重启 | `system.lifecycle` | P0 由设备自动重启。 |

### 2.3 数据面规则

```text
firmware.getUpdateCapabilities
firmware.beginUpdate(manifest)
STREAM packets using returned streamId
firmware.finishUpdate(updateSessionId)
device verifies md5, installs, auto reboots if required
firmware.getInfo after reconnect, from firmware.info
```

STREAM header 只携带 `streamId`、`seqId`、`cursor` 和 payload bytes。`fileId`、`target`、`size`、`md5` 来自 manifest 和 `beginUpdate` 返回的 stream binding。

## 3. 方法 Methods

### 3.0 方法速览

| Method | 调用类型 | 用途 | Params Schema | Result Schema | 是否触发事件 | 状态 |
|---|---|---|---|---|---|---|
| `firmware.getUpdateCapabilities` | query | 查询 P0 更新能力。 | `EmptyParams` | `FirmwareUpdateCapabilities` | 否 | P0 draft |
| `firmware.beginUpdate` | command | 创建更新会话，提交 manifest，绑定 STREAM。 | `BeginUpdateParams` | `BeginUpdateResult` | 可触发 state event | P0 draft |
| `firmware.finishUpdate` | command | 上传完成后移交设备自主处理。 | `FinishUpdateParams` | `FinishUpdateResult` | 触发 state/progress event | P0 draft |
| `firmware.getUpdateState` | query | 查询当前更新状态，作为事件丢失或重连兜底。 | `GetUpdateStateParams` | `FirmwareUpdateState` | 否 | P0 draft |

`firmware.getInfo` 属于 `firmware.info` 草案，不在本文定义；本流程只在升级前和重连后依赖它读取版本。

### 3.1 `firmware.getUpdateCapabilities`

| 条目 | 内容 |
|---|---|
| Purpose | 查询设备是否支持 P0 固件更新，以及最小上传约束。 |
| Call type | query |
| Params Schema | `EmptyParams` |
| Result Schema | `FirmwareUpdateCapabilities` |
| Event trigger | 不触发事件。 |
| Idempotency / async | 幂等；不创建会话。 |
| Common errors | `NOT_SUPPORTED`, `INVALID_STATE`, `FW_DEVICE_NOT_READY` |

请求参数 Params：`EmptyParams`

| Field | Type | Required | Range/Enum | Default | Description |
|---|---|---:|---|---|---|
| none | - | - | - | - | P0 不需要请求参数。 |

#### 3.1.1 错误 d block 示例

```json
{
  "id": 101,
  "status": {
    "ok": false,
    "code": 3,
    "msg": "Request failed.",
    "details": {
      "candidateError": "NOT_SUPPORTED",
      "field": "params",
      "reason": "unsupported value"
    }
  }
}
```

#### 3.1.2 d block 示例

request:

```json
{
  "id": 101,
  "method": "firmware.getUpdateCapabilities",
  "params": {}
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
  "result": {}
}
```

### 3.2 `firmware.beginUpdate`

| 条目 | 内容 |
|---|---|
| Purpose | 创建更新会话，提交精简 manifest，并返回每个文件的 STREAM 绑定。 |
| Call type | command |
| Params Schema | `BeginUpdateParams` |
| Result Schema | `BeginUpdateResult` |
| Event trigger | 可触发 `firmware.updateStateChanged(state=receiving)`。 |
| Idempotency / async | P0 不定义幂等键；Host 超时后可查询 state 或重新开始。 |
| Common errors | `INVALID_ARGUMENT`, `BUSY`, `FW_VERSION_UNSUPPORTED`, `FW_STORAGE_NOT_ENOUGH`, `FW_DEVICE_NOT_READY` |

请求参数 Params：`BeginUpdateParams`

| Field | Type | Required | Range/Enum | Default | Description |
|---|---|---:|---|---|---|
| `manifest` | `FirmwareUpdateManifest` | yes | object | none | 固件包最小摘要。 |

#### 3.2.1 错误 d block 示例

```json
{
  "id": 102,
  "status": {
    "ok": false,
    "code": 10,
    "msg": "Invalid argument.",
    "details": {
      "candidateError": "INVALID_ARGUMENT",
      "field": "manifest",
      "reason": "invalid field value"
    }
  }
}
```

#### 3.2.2 d block 示例

request:

```json
{
  "id": 102,
  "method": "firmware.beginUpdate",
  "params": {
    "manifest": {
      "packageId": "pkg_20260615_001",
      "version": "1.2.3",
      "files": [
        {
          "fileId": "app",
          "target": "main",
          "size": 1048576,
          "md5": "0123456789abcdef0123456789abcdef"
        }
      ]
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
    "updateSessionId": "upd_20260615_001",
    "state": "receiving",
    "streams": [
      {
        "fileId": "app",
        "streamId": 4097
      }
    ],
    "chunkSize": 131072
  }
}
```

### 3.3 `firmware.finishUpdate`

| 条目 | 内容 |
|---|---|
| Purpose | Host 声明本次会话的固件字节已上传完成，请设备接管校验、安装和重启。 |
| Call type | command |
| Params Schema | `FinishUpdateParams` |
| Result Schema | `FinishUpdateResult` |
| Event trigger | 触发或允许轮询到 verifying/installing/rebooting/confirmed/failed。 |
| Idempotency / async | P0 按 `updateSessionId` 幂等处理重复 finish；`accepted=true` 不代表升级成功。 |
| Common errors | `INVALID_STATE`, `STREAM_CHUNK_MISSING`, `FW_SIZE_MISMATCH`, `FW_DEVICE_NOT_READY`, `BUSY` |

请求参数 Params：`FinishUpdateParams`

| Field | Type | Required | Range/Enum | Default | Description |
|---|---|---:|---|---|---|
| `updateSessionId` | string | yes | opaque | none | 更新会话 ID。 |

#### 3.3.1 错误 d block 示例

```json
{
  "id": 103,
  "status": {
    "ok": false,
    "code": 11,
    "msg": "Request failed.",
    "details": {
      "candidateError": "INVALID_STATE",
      "field": "updateSessionId",
      "reason": "state does not allow this operation"
    }
  }
}
```

#### 3.3.2 d block 示例

request:

```json
{
  "id": 103,
  "method": "firmware.finishUpdate",
  "params": {
    "updateSessionId": "upd_20260615_001"
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
    "updateSessionId": "upd_20260615_001",
    "accepted": true,
    "state": "receiving"
  }
}
```

### 3.4 `firmware.getUpdateState`

| 条目 | 内容 |
|---|---|
| Purpose | 查询当前更新状态，作为事件丢失、Host 重连或 UI 刷新的兜底。 |
| Call type | query |
| Params Schema | `GetUpdateStateParams` |
| Result Schema | `FirmwareUpdateState` |
| Event trigger | 不触发事件。 |
| Idempotency / async | 幂等。 |
| Common errors | `NOT_FOUND`, `FW_TRANSFER_NOT_STARTED` |

请求参数 Params：`GetUpdateStateParams`

| Field | Type | Required | Range/Enum | Default | Description |
|---|---|---:|---|---|---|
| `updateSessionId` | string | yes | opaque | none | 更新会话 ID。 |

#### 3.4.1 错误 d block 示例

```json
{
  "id": 104,
  "status": {
    "ok": false,
    "code": 12,
    "msg": "Request failed.",
    "details": {
      "candidateError": "NOT_FOUND",
      "field": "updateSessionId",
      "reason": "resource not found"
    }
  }
}
```

#### 3.4.2 d block 示例

request:

```json
{
  "id": 104,
  "method": "firmware.getUpdateState",
  "params": {
    "updateSessionId": "upd_20260615_001"
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
    "updateSessionId": "upd_20260615_001",
    "state": "receiving",
    "progress": 42
  }
}
```

## 4. 事件 Events

### 4.0 事件速览

| Event | 触发条件 | Payload Schema | 客户端处理建议 | 状态 |
|---|---|---|---|---|
| `firmware.updateProgressReported` | 接收、校验、安装阶段进度变化。 | `FirmwareUpdateProgressEvent` | 更新 UI；事件丢失时调用 `getUpdateState`。 | P0 draft |
| `firmware.updateStateChanged` | 更新状态变化或失败。 | `FirmwareUpdateStateChangedEvent` | 根据 state 更新 UI；失败时展示 error。 | P0 draft |

### 4.1 `firmware.updateProgressReported`

触发条件：

- STREAM 接收进度变化。
- `finishUpdate` 后设备内部校验或安装进度变化。

Payload：`FirmwareUpdateProgressEvent`

| Field | Type | Required | Range/Enum | Description |
|---|---|---:|---|---|
| `updateSessionId` | string | yes | opaque | 更新会话 ID。 |
| `state` | string | yes | state enum | 当前状态。 |
| `progress` | uint8 | no | `0..100` | 整体进度。 |
| `fileId` | string | no | manifest fileId | 当前文件；没有文件粒度时省略。 |

#### d block 示例

```json
{
  "event": "firmware.updateProgressReported",
  "intent": 1,
  "data": {
    "updateSessionId": "upd_20260615_001",
    "state": "receiving",
    "progress": 42
  }
}
```


客户端处理建议：事件只用于 UI 更新，不作为唯一事实源；Host 可用 `getUpdateState` 校准。

### 4.2 `firmware.updateStateChanged`

触发条件：

- `beginUpdate` 接受后进入 receiving。
- `finishUpdate` 接受后进入 verifying/installing/rebooting。
- 更新成功、失败或设备即将自动重启。

Payload：`FirmwareUpdateStateChangedEvent`

| Field | Type | Required | Range/Enum | Description |
|---|---|---:|---|---|
| `updateSessionId` | string | yes | opaque | 更新会话 ID。 |
| `state` | string | yes | state enum | 新状态。 |
| `error` | `FirmwareUpdateErrorInfo` | no | object | 失败详情。 |

#### d block 示例

```json
{
  "event": "firmware.updateStateChanged",
  "intent": 1,
  "data": {
    "updateSessionId": "upd_20260615_001",
    "state": "receiving"
  }
}
```


客户端处理建议：当 `state=failed` 时显示 `error`；当 `state=rebooting` 时提示保持供电并等待设备断连/重连。

## 5. Capability

Capability：`FirmwareUpdateCapabilities`

| Field | Type | Required | Range/Enum | Description |
|---|---|---:|---|---|
| `supported` | boolean | yes | `true`, `false` | 是否支持 `firmware.update` P0。 |
| `supportsMultiFile` | boolean | yes | `true`, `false` | 是否支持 manifest 中多个文件。 |
| `streamLayout` | string | yes | `file` | P0 使用每个 `fileId` 绑定一个 `streamId`。 |
| `hashAlgorithm` | string | yes | `md5` | P0 固定 md5。 |
| `autoReboot` | boolean | yes | `true`, `false` | 安装完成后是否由设备自动重启。 |
| `maxChunkSize` | uint32 | no | bytes | Host 可使用的最大 STREAM data 大小。 |
| `devicePolicyVersion` | string | no | product-defined | 设备策略版本，用于 Host 解析多文件包；是否必填见待确认问题。 |

P0 约定：manifest 必填、finish 必填、不要求签名、不要求 resume、不要求 parallel transfer。为减少字段数量，这些固定规则不再重复放入 capability 字段。

## 6. 字段 / Schemas

### 6.1 Schema hierarchy

| Schema | 用途 |
|---|---|
| `FirmwareUpdateManifest` | Host 传给设备的固件包最小摘要。 |
| `FirmwareUpdateFile` | manifest 内单个文件。 |
| `FirmwareUpdateStreamBinding` | `fileId` 与 `streamId` 绑定。 |
| `FirmwareUpdateState` | 当前更新状态。 |
| `FirmwareUpdateErrorInfo` | 错误详情。 |

### 6.2 请求/响应 schema

Schema：`FirmwareUpdateManifest`

| Field | Type | Required | Range/Enum | Default | Description |
|---|---|---:|---|---|---|
| `packageId` | string | yes | product-defined | none | 固件更新包 ID。 |
| `targetVersion` | string | yes | product-defined | none | 目标版本。 |
| `files` | array<`FirmwareUpdateFile`> | yes | non-empty | none | 文件列表；单 `.bin` 也是一个文件。 |
| `devicePolicyVersion` | string | no | product-defined | none | Host 使用的设备策略版本；是否必填见待确认问题。 |

Schema：`FirmwareUpdateFile`

| Field | Type | Required | Range/Enum | Default | Description |
|---|---|---:|---|---|---|
| `fileId` | string | yes | unique in manifest | none | 文件 ID，例如 `app`、`resource`。 |
| `target` | string | yes | `bootloader`, `application`, `resource`, `model`, `vendor` | none | 写入目标。 |
| `size` | uint64 | yes | bytes | none | 文件大小。 |
| `md5` | string | yes | lowercase hex | none | 文件 md5。 |

Schema：`FirmwareUpdateStreamBinding`

| Field | Type | Required | Range/Enum | Default | Description |
|---|---|---:|---|---|---|
| `fileId` | string | yes | manifest fileId | none | 文件 ID。 |
| `streamId` | uint32 | yes | stream id | none | STREAM 数据面 ID。 |

### 6.3 运行态状态 schema

Schema：`FirmwareUpdateState`

| Field | Type | Required | Range/Enum | Default | Description |
|---|---|---:|---|---|---|
| `updateSessionId` | string | yes | opaque | none | 更新会话 ID。 |
| `state` | string | yes | state enum | none | 当前状态。 |
| `progress` | uint8 | no | `0..100` | none | 整体进度。 |
| `fileId` | string | no | manifest fileId | none | 当前处理文件；无文件粒度时省略。 |
| `error` | `FirmwareUpdateErrorInfo` | no | object | none | 最近错误。 |

Schema：`FirmwareUpdateErrorInfo`

| Field | Type | Required | Range/Enum | Default | Description |
|---|---|---:|---|---|---|
| `code` | uint32 | yes | adopted ErrorCode | none | 数字错误码。 |
| `candidateError` | string | no | error name | none | 草案可读错误名。 |
| `fileId` | string | no | manifest fileId | none | 相关文件。 |
| `message` | string | no | diagnostic string | none | 诊断消息，不含敏感信息。 |

### 6.4 状态枚举

| State | Description |
|---|---|
| `idle` | 无固件更新任务。 |
| `receiving` | 正在接收 STREAM 数据。 |
| `verifying` | 设备内部校验 md5。 |
| `installing` | 设备内部安装。 |
| `rebooting` | 设备准备或正在自动重启。 |
| `confirmed` | 新版本已启动或设备确认升级完成。 |
| `failed` | 失败。 |

## 7. 交互流程示例 Flow Examples

本节示例只写 RPC `d` 数据块，不包裹外层 `sid` / `op` / `d` wire envelope。成功 `status.code=0`；`FW_HASH_MISMATCH=1034` (`0x040A`)；`STREAM_CHUNK_MISSING=1290` (`0x050A`)。

### 7.1 场景：查询能力

Request:

```json
{
  "id": 1001,
  "method": "firmware.getUpdateCapabilities"
}
```

Response:

```json
{
  "id": 1001,
  "status": {
    "ok": true,
    "code": 0
  },
  "result": {
    "supported": true,
    "supportsMultiFile": true,
    "streamLayout": "file",
    "hashAlgorithm": "md5",
    "autoReboot": true,
    "maxChunkSize": 65536,
    "devicePolicyVersion": "2026.06"
  }
}
```

读法：P0 固定为 md5、file-level STREAM、设备自动重启，不需要返回 resume、parallel、signature 等字段。

### 7.2 场景：创建更新会话

Request:

```json
{
  "id": 1002,
  "method": "firmware.beginUpdate",
  "params": {
    "manifest": {
      "packageId": "pkg_2026_0605_001",
      "targetVersion": "2.3.0",
      "devicePolicyVersion": "2026.06",
      "files": [
        {
          "fileId": "app",
          "target": "application",
          "size": 8388608,
          "md5": "d41d8cd98f00b204e9800998ecf8427e"
        },
        {
          "fileId": "resource",
          "target": "resource",
          "size": 5242880,
          "md5": "d41d8cd98f00b204e9800998ecf8427e"
        }
      ]
    }
  }
}
```

Response:

```json
{
  "id": 1002,
  "status": {
    "ok": true,
    "code": 0
  },
  "result": {
    "updateSessionId": "upd_001",
    "state": "receiving",
    "chunkSize": 65536,
    "streams": [
      {
        "fileId": "app",
        "streamId": 33
      },
      {
        "fileId": "resource",
        "streamId": 34
      }
    ]
  }
}
```

读法：Host 后续按 `fileId -> streamId` 绑定发送 STREAM bytes；安装顺序仍由设备策略决定。

### 7.3 场景：上传完成并移交设备

Request:

```json
{
  "id": 1003,
  "method": "firmware.finishUpdate",
  "params": {
    "updateSessionId": "upd_001"
  }
}
```

Response:

```json
{
  "id": 1003,
  "status": {
    "ok": true,
    "code": 0
  },
  "result": {
    "updateSessionId": "upd_001",
    "accepted": true,
    "state": "verifying"
  }
}
```

读法：`accepted=true` 不是升级成功；Host 等待状态事件或轮询。

### 7.4 场景：设备上报进度和状态

Event:

```json
{
  "event": "firmware.updateProgressReported",
  "intent": 1,
  "data": {
    "updateSessionId": "upd_001",
    "state": "receiving",
    "progress": 62,
    "fileId": "resource"
  }
}
```

Event:

```json
{
  "event": "firmware.updateStateChanged",
  "intent": 1,
  "data": {
    "updateSessionId": "upd_001",
    "state": "rebooting"
  }
}
```

读法：设备自主推进内部阶段；Host 不调用 install 或 reboot。

### 7.5 场景：md5 校验失败

Event:

```json
{
  "event": "firmware.updateStateChanged",
  "intent": 1,
  "data": {
    "updateSessionId": "upd_001",
    "state": "failed",
    "error": {
      "code": 1034,
      "candidateError": "FW_HASH_MISMATCH",
      "fileId": "app",
      "message": "app.bin md5 mismatch"
    }
  }
}
```

读法：设备不得安装失败包；Host 显示失败文件和错误码。

### 7.6 场景：finish 因数据不完整被拒绝

Response:

```json
{
  "id": 1003,
  "status": {
    "ok": false,
    "code": 1290,
    "message": "Update data is incomplete",
    "details": {
      "candidateError": "STREAM_CHUNK_MISSING",
      "updateSessionId": "upd_001",
      "fileId": "resource"
    }
  }
}
```

读法：P0 不要求设备返回 `missingRanges`；Host 可重新发送当前文件或重新开始会话。

## 8. 错误

| 场景 | 推荐错误码 | 状态 |
|---|---|---|
| 不支持 `firmware.update` | `NOT_SUPPORTED` / `RPC_METHOD_NOT_SUPPORTED` | adopted |
| 参数或 manifest 摘要非法 | `INVALID_ARGUMENT` | adopted |
| 当前状态不允许 begin/finish/query | `INVALID_STATE` | adopted |
| 已有 active 更新会话 | `BUSY` | adopted |
| 版本或硬件不兼容 | `FW_VERSION_UNSUPPORTED` | adopted firmware |
| 存储空间不足 | `FW_STORAGE_NOT_ENOUGH` | adopted firmware |
| 设备状态不满足升级条件 | `FW_DEVICE_NOT_READY` | adopted firmware |
| finish 时数据不完整 | `STREAM_CHUNK_MISSING` / `FW_SIZE_MISMATCH` | adopted |
| md5 不匹配 | `FW_HASH_MISMATCH` | adopted firmware |
| 设备内部安装失败 | `FW_APPLY_FAILED` | adopted firmware |

本文不新增错误码；新增候选错误必须在 adoption 时分配，Markdown 中不得预分配 final numeric code。

## 9. Legacy mapping candidates

以下映射只作为人工审查输入；open review 条目不得直接写入稳定 `legacyRefs`。

| Source | Legacy item | Candidate mapping | Review status |
|---|---|---|---|
| AXDP HID | `AlphaUpgradeInfo`, `BetaStartUpgrade`, `BetaUpgradeInfo`, `BetaUpgradeInfoEx` | `firmware.beginUpdate` | open review：旧包格式、block 信息、md5 字段需设备侧确认。 |
| AXDP HID | `AlphaUpgradeData`, `BetaUpgradeData`, `BetaUpgradeDataEx` | STREAM `firmware.update` | open review：旧 slice index 可映射到 `seqId` 或 byte cursor；最终由 stream profile/adaptor 确认。 |
| AXDP HID | `BetaStopUpgrade` | `firmware.finishUpdate` | open review：旧名是否表示正常结束发送需确认；如果表示停止/取消则应转 P1 cancel。 |
| Rooms / Signage / VM33 | `RemoteUpgrade`, `CloudUpgrade` | `firmware.beginUpdate(source.type=url)` P1 | open review：URL 模式不进入 P0。 |
| Rooms / Signage / VM33 | `UpgradeProgress`, `CloudProgress` | `firmware.getUpdateState` or progress/state events | `[REVIEW-DRAFT]` 主动查询和事件订阅均可表达。 |
| VM33 | `Upgrade.Version` | `firmware.getInfo` in `firmware.info` | open review：与 `device.info` 边界需确认。 |

## 10. Registry 草案输入摘要

| Fact | P-level | Notes |
|---|---|---|
| capability `firmware.update` | P0 | 最小字段：multi-file、file stream layout、md5、auto reboot、chunk size、device policy version。 |
| method `firmware.getUpdateCapabilities` | P0 | 无请求参数。 |
| method `firmware.beginUpdate` | P0 | 只接收 `manifest`。 |
| method `firmware.finishUpdate` | P0 | 只接收 `updateSessionId`。 |
| method `firmware.getUpdateState` | P0 | 只接收 `updateSessionId`。 |
| events `updateProgressReported`, `updateStateChanged` | P0 | 最小进度和状态观察。 |
| advanced methods | P1/P2 | 不进入 P0 字段集。 |

## 11. 测试要点

| Case | Given | When | Then |
|---|---|---|---|
| happy path | 设备支持 stream、多文件、md5、autoReboot | Host begin、STREAM 上传、finish | 设备自主校验/安装/重启，Host 重连后读到新版本。 |
| single file | Manifest 只有一个 application 文件 | Host begin/STREAM/finish | 走同一 manifest 模型。 |
| multi file | Manifest 包含 application/resource/model/vendor | Host 上传各 fileId 对应 stream | 设备按策略安装，Host 不依赖文件顺序作为安装顺序。 |
| finish rejected | 数据不完整 | Host 调用 finish | 设备返回 `STREAM_CHUNK_MISSING` 或 `FW_SIZE_MISMATCH`，不进入安装。 |
| md5 mismatch | 数据完整但 md5 不一致 | finish 后设备校验 | 设备上报 `FW_HASH_MISMATCH`，不得安装。 |
| auto reboot | 设备安装后需要重启 | 设备自主安装 | 设备自动重启，Host 等待重连。 |
| event loss | Host 未收到状态事件 | Host 调用 `getUpdateState` | UI 可校准状态。 |

## 12. 待确认问题

| Issue | Impact | Current recommendation | Status |
|---|---|---|---|
| `devicePolicyVersion` 是否为 P0 必填？ | 影响 Host 多文件解析和设备策略一致性。 | 先保留可选；产品包规则明确后再决定是否必填。 | `[REVIEW-ASK]` |
| Manifest 来源是包内文件、Host 生成，还是设备解析 vendor package？ | 影响 Host 工具链和 begin 参数。 | P0 允许 Host 生成最小 manifest。 | `[REVIEW-ASK]` |
| `firmware.info` 与 `device.info` 如何划分版本字段？ | 影响升级前/重连后的版本读取。 | 固件组件版本归 `firmware.info`；设备型号/硬件身份归 `device.info`。 | `[REVIEW-ASK]` |
| P1 是否需要 cancel、resume、URL、file 暂存、A/B confirm、rollback？ | 影响后续里程碑。 | P0 先不展开字段；按产品排期逐项补草案。 | `[REVIEW-ASK]` |
| AXDP/Rooms/Signage/VM33 哪些字段可写入稳定 legacyRefs？ | 影响 migration/adaptor 准确性。 | 仅把已确认字段写入 registry；不确定项保留 adapter-only。 | `[REVIEW-ASK]` |

## 附录 A. 协议审核标记

| 标记 | 条目 | 结论 |
|---|---|---|
| `[REVIEW-OK]` | Capability name | `firmware.update` 表达固件更新能力，OTA/USB/URL 是来源或传输模式。 |
| `[REVIEW-OK]` | P0 host orchestration | P0 使用 `finishUpdate` 移交设备；Host 不统一编排校验/安装。 |
| `[REVIEW-OK]` | P0 minimal fields | P0 仅保留闭环必要字段，扩展能力不提前展开。 |
| `[REVIEW-OK]` | Security baseline | P0 md5，不强制签名。 |
| `[REVIEW-OK]` | Reboot baseline | 设备自动重启，Host 等待重连。 |
| `[REVIEW-OK]` | Old P0 `commit/verify/install` | 已处理：不再作为 P0 主流程；降为 P1/advanced，未写入本次 registry source。 |

## 附录 B. 协议决策记录

| Decision | Result |
|---|---|
| Reuse/modify/create | Modify existing draft。 |
| Implementation degree | Drafted only。 |
| P0 transfer | Sequential STREAM transfer, file-level stream binding。 |
| P0 completion | `firmware.finishUpdate(updateSessionId)`。 |
| P0 integrity | md5。 |
| P0 reboot | Device auto reboot。 |
| P0 non-goals | No resume, missing ranges, batch commit, Host-triggered verify/install, URL, file staging, production signature, Host-triggered reboot. |

## 附录 C. Registry 草案输入

```yaml
capabilities:
  - id: firmware.update
    status: draft
    methods:
      - firmware.getUpdateCapabilities
      - firmware.beginUpdate
      - firmware.finishUpdate
      - firmware.getUpdateState
    events:
      - firmware.updateProgressReported
      - firmware.updateStateChanged
    streamProfiles:
      - firmware.update
```

以上保留为后续修订输入；已生成 methodId/eventId/fieldId 以 contract/registry/generated 为准。

## 附录 D. 采纳检查清单

- [ ] 确认 P0 是否只采纳 4 个 method 和 2 个 event。
- [ ] 确认 `FirmwareUpdateManifest` 最小字段是否足够。
- [ ] 确认 `devicePolicyVersion` 是否必填。
- [ ] 确认 `streamLayout=file` 是否作为 P0 唯一 layout。
- [ ] 确认 state enum 是否满足 UI 和测试。
- [ ] 确认 legacy mappings 中可进入稳定 `legacyRefs` 的条目。
- [x] Stage 30 已写入 registry YAML，并通过 source validation。
- [x] 已运行 `generate-axtp-protocol` 刷新 generated artifacts。
