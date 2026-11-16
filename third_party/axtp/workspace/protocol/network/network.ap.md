---
status: generated
contract: true
generated: true
domain: network
feature: network.ap
registry: ../../../../contract/registry/domains/network/domain.yaml
lastReviewed: 2026-06-15
---

# network.ap

## 0. 速读结论

| 项目 | 内容 |
|---|---|
| 这个能力做什么 | 管理设备自身 AP/SoftAP 的能力、SSID/安全配置、一次性凭据导出、运行状态和可选客户端列表。 |
| 当前状态 | generated；已写入 `../../../../contract/registry/domains/network/domain.yaml`，并已刷新到 `contract/protocol/axtp.protocol.yaml` 与 `contract/generated/**`。 |
| 是否可直接实现 | 是，但实现合同以 `contract/protocol/axtp.protocol.yaml` / `contract/generated/**` 为准；本文保留的 open review markers 不属于已生成合同。 |
| 主要交互 | RPC + EVENT |
| 是否使用 STREAM | 否 |
| Registry readiness | ready；P0 / confirmed subset 已写入 registry source 并生成。 |
| Conformance | needed |
| 主要未决问题 | AP 客户端列表是否作为强验收、DHCP Server 地址池归属、一次性凭据导出的有效期/重放策略。 |

## 1. 功能说明

`network.ap` 描述设备作为 AP/SoftAP/Hotspot 端点时的控制面。Cast RX/TX 配对中，Host 从 NA20 读取 AP SSID、安全类型和一次性导出的凭据，再把这些材料写入 NT10 的 `network.wifi` profile。

已确认的 flow 决策包括：NA20 AP 默认始终开启；AP SSID/密码由上位机可配置；AP 凭据允许通过 AXTP 一次性导出；`startAp` / `stopAp` 可作为低优先级 optional 能力，但配对主路径不依赖启动 AP。

`network.ap` 的 P0 / confirmed subset 已进入 generated 合同；正式 methodId、eventId、errorCode、fieldId 以 `contract/registry/**`、`contract/protocol/axtp.protocol.yaml` 和 `contract/generated/**` 为准。本文保留的 review 标记仅用于后续修订。

## 2. 能力边界

| 类型 | 内容 |
|---|---|
| 包含 | AP 能力查询，包括安全类型、频段、凭据导出模式、默认开启和可选启停能力。 |
| 包含 | AP 基础配置读取和写入，包括 SSID、安全类型、凭据、隐藏 SSID、频段和信道。 |
| 包含 | 一次性导出 AP 凭据供本地 Host 写入 STA 设备。 |
| 包含 | AP 运行状态查询和状态变化事件。 |
| 包含 | 可选 AP 客户端列表查询和客户端变化事件，用于强验收或诊断。 |
| 不包含 | 接口发现、MAC、基础链路；这些属于 `network.interface`。 |
| 不包含 | AP 本端 IP 地址；这属于 `network.ip`。 |
| 不包含 | NT10 作为 STA 保存 profile 和连接 AP；这属于 `network.wifi`。 |
| 不包含 | 上位机多设备选择、配对编排和凭据日志策略；这些属于 Host local-only 行为。 |
| 数据面 | 本 feature 不定义 STREAM payload，所有操作均通过 RPC method/event 完成。 |

## 3. 方法 Methods

已生成 methodId、eventId、bitOffset 和 schema fieldId 以 contract/registry/generated 为准；本文不重新分配正式 ID，保留的 draft/review 标记仅作为后续修订输入。

### 3.0 方法速览

| Method | 调用类型 | 用途 | Params Schema | Result Schema | 是否触发事件 | 状态 |
|---|---|---|---|---|---|---|
| `network.getApCapabilities` | query | 查询 AP 能力和策略。 | `NetworkGetApCapabilitiesParams` | `NetworkApCapabilities` | 否 | draft |
| `network.getApConfig` | query | 读取 AP 配置，可请求一次性导出凭据。 | `NetworkGetApConfigParams` | `NetworkApConfig` | 否 | draft |
| `network.setApConfig` | command | 设置 AP SSID、安全、凭据和基础无线配置。 | `NetworkSetApConfigParams` | `NetworkSetApConfigResult` | 是，`network.apConfigChanged` | draft |
| `network.getApState` | query | 查询 AP 角色运行状态。 | `NetworkGetApStateParams` | `NetworkApState` | 否 | draft |
| `network.startAp` | action | 低优先级可选：启动 AP。 | `NetworkApActionParams` | `NetworkApActionResult` | 是，`network.apStateChanged` | optional draft |
| `network.stopAp` | action | 低优先级可选：停止 AP。 | `NetworkApActionParams` | `NetworkApActionResult` | 是，`network.apStateChanged` | optional draft |
| `network.getApClients` | query | 可选查询 AP 客户端列表。 | `NetworkGetApClientsParams` | `NetworkApClients` | 否 | review-ask |

### 3.1 `network.getApCapabilities`

**用途**：查询指定 AP 接口的能力、默认开启语义和凭据导出策略。

| 项 | 内容 |
|---|---|
| 调用类型 | query |
| Params Schema | `NetworkGetApCapabilitiesParams` |
| Result Schema | `NetworkApCapabilities` |
| 是否触发事件 | 否 |
| 幂等性 / 异步性 | 幂等；同步返回能力快照。 |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_ARGUMENT`, `NOT_FOUND`, `PERMISSION_DENIED`, `UNAVAILABLE` |

#### 3.1.1 请求参数 Params：`NetworkGetApCapabilitiesParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `interfaceId` | string | no | AP-capable interface id | `defaults.ap` | AP 接口；省略表示 `network.interface.defaults.ap`。 |

#### 3.1.2 返回结果 Result：`NetworkApCapabilities`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `capability` | string | yes | fixed `network.ap` | none | capability 名称。 |
| `securityTypes` | `NetworkWifiSecurityType[]` | yes | see enum | none | 支持的 AP 安全类型。 |
| `bands` | `NetworkWifiBand[]` | no | see enum | omitted | 支持频段。 |
| `credentialExportModes` | `NetworkCredentialType[]` | yes | `passphrase`, `pairing_token`, `opaque_ref` | none | 可导出的凭据形态；NA20 主路径支持 `passphrase`。 |
| `credentialExportPolicies` | string[] | no | `one_time`, `redacted`, `none` | omitted | 凭据导出策略。 |
| `defaultEnabled` | boolean | no | bool | omitted | AP 是否默认开启；NA20 为 `true`。 |
| `canStartStop` | boolean | yes | bool | none | 是否支持 AXTP 启停；配对主路径不依赖此能力。 |
| `clientListSupported` | boolean | no | bool | omitted | 是否支持客户端列表。 |
| `maxClients` | uint16 | no | `0..65535` | omitted | 最大客户端数量。 |

#### 3.1.3 d block 示例

request:

```json
{
  "id": 101,
  "method": "network.getApCapabilities",
  "params": {
    "interfaceId": "wlan0"
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
    "capability": "network.ap",
    "securityTypes": [],
    "credentialExportModes": "<redacted>",
    "canStartStop": true
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
| `NOT_FOUND` | 指定 `interfaceId` 不存在或不是 AP-capable 接口。 | 使用 adopted numeric code `12`。 |
| `NOT_SUPPORTED` | 设备不支持 AP 能力。 | 使用 adopted numeric code `3`。 |

### 3.2 `network.getApConfig`

**用途**：读取当前 AP 配置；配对场景可请求一次性导出凭据。

| 项 | 内容 |
|---|---|
| 调用类型 | query |
| Params Schema | `NetworkGetApConfigParams` |
| Result Schema | `NetworkApConfig` |
| 是否触发事件 | 否 |
| 幂等性 / 异步性 | 查询幂等；`credentialExport=one_time` 会消耗或生成一次性导出材料，具体重放策略待确认。 |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_ARGUMENT`, `NOT_FOUND`, `PERMISSION_DENIED`, `UNAVAILABLE` |

#### 3.2.1 请求参数 Params：`NetworkGetApConfigParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `interfaceId` | string | no | AP-capable interface id | `defaults.ap` | AP 接口。 |
| `credentialExport` | string enum | no | `none`, `one_time` | `none` | 是否一次性导出凭据。 |

#### 3.2.2 返回结果 Result：`NetworkApConfig`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `interfaceId` | string | no | interface id | omitted | AP 接口标识。 |
| `ssid` | string | yes | non-empty | none | AP SSID。 |
| `security` | `NetworkWifiSecurityType` | yes | see enum | none | AP 安全类型。 |
| `credential` | `NetworkCredential` | no | object | omitted | 仅在请求导出且权限允许时返回；敏感字段。 |
| `hidden` | boolean | no | bool | `false` | 是否隐藏 SSID。 |
| `band` | `NetworkWifiBand` | no | see enum | omitted | AP 频段。 |
| `channel` | uint16 | no | valid channel | omitted | AP 信道。 |
| `maxClients` | uint16 | no | `0..65535` | omitted | 客户端上限。 |

#### 3.2.3 d block 示例

request:

```json
{
  "id": 102,
  "method": "network.getApConfig",
  "params": {
    "interfaceId": "wlan0"
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
    "interfaceId": "wlan0",
    "ssid": "NearHub-Cast-A1",
    "securityType": "wpa2_psk",
    "credential": {
      "type": "passphrase",
      "secretRef": "<redacted>"
    },
    "band": "5GHz",
    "channel": 36
  }
}
```

#### 3.2.4 可能触发的事件

| Event | 触发条件 | Payload Schema | 客户端处理建议 |
|---|---|---|---|
| 无 | 查询不改变 AP 配置。 | none | 无需处理。 |

#### 3.2.5 错误

| 错误 | 场景 | 返回建议 |
|---|---|---|
| `PERMISSION_DENIED` | 当前会话无权导出凭据。 | 使用 adopted numeric code `9`，details 可标注候选 `NETWORK_CREDENTIAL_EXPORT_DENIED`。 |
| `INVALID_ARGUMENT` | `credentialExport` 枚举非法。 | 使用 adopted numeric code `10`。 |

### 3.3 `network.setApConfig`

**用途**：设置 AP 基础配置。NA20 的 SSID/密码由上位机可配置，进入本方法语义。

| 项 | 内容 |
|---|---|
| 调用类型 | command |
| Params Schema | `NetworkSetApConfigParams` |
| Result Schema | `NetworkSetApConfigResult` |
| 是否触发事件 | 是，实际配置变化后触发 `network.apConfigChanged`；如需要重启 AP，随后可能触发 `network.apStateChanged`。 |
| 幂等性 / 异步性 | 建议幂等；重复设置相同配置应成功，可不重复触发事件。 |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_ARGUMENT`, `OUT_OF_RANGE`, `INVALID_STATE`, `BUSY`, `PERMISSION_DENIED` |

#### 3.3.1 请求参数 Params：`NetworkSetApConfigParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `config` | `NetworkApConfig` | yes | object | none | 要写入的 AP 配置。 |
| `apply` | string enum | no | `immediate`, `on_restart` | `immediate` | 生效策略。 |

#### 3.3.2 返回结果 Result：`NetworkSetApConfigResult`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `config` | `NetworkApConfig` | yes | object | none | 设备接受的配置摘要，敏感 credential 不应明文回显。 |
| `applied` | boolean | yes | bool | none | 是否已生效。 |
| `requiresApRestart` | boolean | no | bool | omitted | 是否需要 AP 重启后生效。 |

#### 3.3.3 d block 示例

request:

```json
{
  "id": 103,
  "method": "network.setApConfig",
  "params": {
    "config": {
      "interfaceId": "wlan0",
      "ssid": "NearHub-Cast-A1",
      "securityType": "wpa2_psk",
      "credential": {
        "type": "passphrase",
        "secretRef": "<redacted>"
      },
      "band": "5GHz",
      "channel": 36
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
    "interfaceId": "wlan0",
    "ssid": "NearHub-Cast-A1",
    "securityType": "wpa2_psk",
    "credential": {
      "type": "passphrase",
      "secretRef": "<redacted>"
    },
    "band": "5GHz",
    "channel": 36
  }
}
```

#### 3.3.4 可能触发的事件

| Event | 触发条件 | Payload Schema | 客户端处理建议 |
|---|---|---|---|
| `network.apConfigChanged` | AP 配置实际变化。 | `NetworkApConfigChangedEvent` | 重新读取 AP config，必要时重建 NT10 profile。 |
| `network.apStateChanged` | 配置应用导致 AP 重启或异常。 | `NetworkApStateChangedEvent` | 展示状态并等待 running。 |

#### 3.3.5 错误

| 错误 | 场景 | 返回建议 |
|---|---|---|
| `OUT_OF_RANGE` | SSID 长度、信道或客户端上限超出能力。 | 使用 adopted numeric code `11`。 |
| `BUSY` | AP 正在启动、停止或重配置。 | 使用 adopted numeric code `5`，客户端稍后重试。 |

### 3.4 `network.getApState`

**用途**：查询 AP 当前角色状态。AP 本端 IP 不在此返回。

| 项 | 内容 |
|---|---|
| 调用类型 | query |
| Params Schema | `NetworkGetApStateParams` |
| Result Schema | `NetworkApState` |
| 是否触发事件 | 否 |
| 幂等性 / 异步性 | 幂等；同步返回当前快照。 |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_ARGUMENT`, `NOT_FOUND`, `UNAVAILABLE` |

#### 3.4.1 请求参数 Params：`NetworkGetApStateParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `interfaceId` | string | no | AP-capable interface id | `defaults.ap` | AP 接口。 |

#### 3.4.2 返回结果 Result：`NetworkApState`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `interfaceId` | string | no | interface id | omitted | AP 接口标识。 |
| `state` | string enum | yes | `stopped`, `starting`, `running`, `stopping`, `error` | none | AP 服务状态。 |
| `ssid` | string | no | SSID | omitted | 当前运行的 SSID。 |
| `bssid` | string | no | MAC/BSSID | omitted | 当前 BSSID。 |
| `clientCount` | uint16 | no | `0..65535` | omitted | 当前客户端数量。 |
| `lastError` | string enum | no | see 6.5 | omitted | 最近一次失败原因。 |

#### 3.4.3 d block 示例

request:

```json
{
  "id": 104,
  "method": "network.getApState",
  "params": {
    "interfaceId": "wlan0"
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
    "interfaceId": "wlan0",
    "enabled": true,
    "state": "enabled",
    "ssid": "NearHub-Cast-A1",
    "clientCount": 1
  }
}
```

#### 3.4.4 可能触发的事件

| Event | 触发条件 | Payload Schema | 客户端处理建议 |
|---|---|---|---|
| 无 | 查询不改变 AP 状态。 | none | 无需处理。 |

#### 3.4.5 错误

| 错误 | 场景 | 返回建议 |
|---|---|---|
| `NOT_FOUND` | 指定 AP 接口不存在。 | 使用 adopted numeric code `12`。 |

### 3.5 `network.startAp`

**用途**：低优先级可选：启动 AP。NA20 配对主路径默认 AP 已开启，不要求调用此方法。

| 项 | 内容 |
|---|---|
| 调用类型 | action |
| Params Schema | `NetworkApActionParams` |
| Result Schema | `NetworkApActionResult` |
| 是否触发事件 | 是，状态实际变化后触发 `network.apStateChanged`。 |
| 幂等性 / 异步性 | 对已 running 的 AP 建议返回成功；启动过程可异步。 |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_STATE`, `BUSY`, `PERMISSION_DENIED`, `TIMEOUT` |

#### 3.5.1 请求参数 Params：`NetworkApActionParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `interfaceId` | string | no | AP-capable interface id | `defaults.ap` | AP 接口。 |
| `timeoutMs` | uint32 | no | `0..uint32 max` | omitted | 动作等待超时。 |

#### 3.5.2 返回结果 Result：`NetworkApActionResult`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `accepted` | boolean | yes | bool | none | 动作是否被接受。 |
| `state` | `NetworkApState` | yes | object | none | 操作后的当前或目标状态。 |

#### 3.5.3 d block 示例

request:

```json
{
  "id": 105,
  "method": "network.startAp",
  "params": {
    "interfaceId": "wlan0",
    "timeoutMs": 5000
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
      "interfaceId": "wlan0",
      "enabled": true,
      "state": "enabled",
      "ssid": "NearHub-Cast-A1",
      "clientCount": 1
    }
  }
}
```

#### 3.5.4 可能触发的事件

| Event | 触发条件 | Payload Schema | 客户端处理建议 |
|---|---|---|---|
| `network.apStateChanged` | AP 进入 starting / running / error。 | `NetworkApStateChangedEvent` | 等待 running 或处理失败。 |

#### 3.5.5 错误

| 错误 | 场景 | 返回建议 |
|---|---|---|
| `NOT_SUPPORTED` | 设备不支持远程启动 AP。 | 使用 adopted numeric code `3`。 |
| `BUSY` | AP 正在处理冲突动作。 | 使用 adopted numeric code `5`。 |

### 3.6 `network.stopAp`

**用途**：低优先级可选：停止 AP。此操作可能影响已配对或正在投屏的设备。

| 项 | 内容 |
|---|---|
| 调用类型 | action |
| Params Schema | `NetworkApActionParams` |
| Result Schema | `NetworkApActionResult` |
| 是否触发事件 | 是，状态实际变化后触发 `network.apStateChanged`。 |
| 幂等性 / 异步性 | 对已 stopped 的 AP 建议返回成功；停止过程可异步。 |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_STATE`, `BUSY`, `PERMISSION_DENIED`, `TIMEOUT` |

#### 3.6.1 请求参数 Params：`NetworkApActionParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `interfaceId` | string | no | AP-capable interface id | `defaults.ap` | AP 接口。 |
| `timeoutMs` | uint32 | no | `0..uint32 max` | omitted | 动作等待超时。 |

#### 3.6.2 返回结果 Result：`NetworkApActionResult`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `accepted` | boolean | yes | bool | none | 动作是否被接受。 |
| `state` | `NetworkApState` | yes | object | none | 操作后的当前或目标状态。 |

#### 3.6.3 d block 示例

request:

```json
{
  "id": 106,
  "method": "network.stopAp",
  "params": {
    "interfaceId": "wlan0",
    "timeoutMs": 5000
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
      "interfaceId": "wlan0",
      "enabled": true,
      "state": "enabled",
      "ssid": "NearHub-Cast-A1",
      "clientCount": 1
    }
  }
}
```

#### 3.6.4 可能触发的事件

| Event | 触发条件 | Payload Schema | 客户端处理建议 |
|---|---|---|---|
| `network.apStateChanged` | AP 进入 stopping / stopped / error。 | `NetworkApStateChangedEvent` | 更新 UI，并提示可能断开 STA。 |

#### 3.6.5 错误

| 错误 | 场景 | 返回建议 |
|---|---|---|
| `PERMISSION_DENIED` | 当前策略禁止关闭 AP。 | 使用 adopted numeric code `9`。 |

### 3.7 `network.getApClients`

**用途**：可选查询 AP 客户端列表。可用于配对强验收或诊断，不是默认成功条件。

| 项 | 内容 |
|---|---|
| 调用类型 | query |
| Params Schema | `NetworkGetApClientsParams` |
| Result Schema | `NetworkApClients` |
| 是否触发事件 | 否 |
| 幂等性 / 异步性 | 幂等；同步返回当前快照。 |
| 常见错误 | `NOT_SUPPORTED`, `NOT_FOUND`, `PERMISSION_DENIED`, `UNAVAILABLE` |

#### 3.7.1 请求参数 Params：`NetworkGetApClientsParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `interfaceId` | string | no | AP-capable interface id | `defaults.ap` | AP 接口。 |

#### 3.7.2 返回结果 Result：`NetworkApClients`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `interfaceId` | string | no | interface id | omitted | AP 接口。 |
| `clients` | `NetworkApClientInfo[]` | yes | array | none | 当前客户端列表。 |

#### 3.7.3 d block 示例

request:

```json
{
  "id": 107,
  "method": "network.getApClients",
  "params": {
    "interfaceId": "wlan0"
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
    "clients": [
      {
        "clientId": "client_001"
      }
    ]
  }
}
```

#### 3.7.4 可能触发的事件

| Event | 触发条件 | Payload Schema | 客户端处理建议 |
|---|---|---|---|
| 无 | 查询不改变客户端列表。 | none | 无需处理。 |

#### 3.7.5 错误

| 错误 | 场景 | 返回建议 |
|---|---|---|
| `NOT_SUPPORTED` | 设备不支持客户端列表。 | 使用 adopted numeric code `3`；details 可标注候选 `NETWORK_AP_CLIENT_LIST_UNAVAILABLE`。 |

## 4. 事件 Events

### 4.0 事件速览

| Event | 触发条件 | Payload Schema | 客户端处理建议 | 状态 |
|---|---|---|---|---|
| `network.apConfigChanged` | AP 配置被本会话、其他会话或设备策略改变。 | `NetworkApConfigChangedEvent` | 重新读取 AP config；配对中应重建 NT10 profile。 | draft |
| `network.apStateChanged` | AP 服务启动、停止、异常或恢复。 | `NetworkApStateChangedEvent` | 更新 AP 状态；等待 running 或处理失败。 | draft |
| `network.apClientChanged` | 客户端加入或离开 AP。 | `NetworkApClientChangedEvent` | 仅在采纳客户端列表时用于强验收或诊断。 | review-ask |

### 4.1 `network.apConfigChanged`

**触发条件**：

- `network.setApConfig` 导致配置变化。
- 其他会话、设备策略、恢复默认或工厂设置改变 AP 配置。

#### Payload：`NetworkApConfigChangedEvent`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `interfaceId` | string | no | interface id | omitted | AP 接口。 |
| `config` | `NetworkApConfig` | yes | object | none | 变化后的配置摘要；不应包含明文 credential。 |
| `changedFields` | string[] | no | field paths | omitted | 变化字段。 |
| `reason` | string enum | no | `user_request`, `system_policy`, `factory_reset`, `unknown` | `unknown` | 变化原因。 |

#### d block 示例

```json
{
  "event": "network.apConfigChanged",
  "intent": 1,
  "data": {
    "config": {
      "interfaceId": "wlan0",
      "ssid": "NearHub-Cast-A1",
      "securityType": "wpa2_psk",
      "credential": {
        "type": "passphrase",
        "secretRef": "<redacted>"
      },
      "band": "5GHz",
      "channel": 36
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
| 配对过程中 AP 配置变化 | 重新调用 `network.getApConfig`，必要时重新导出凭据并写入 NT10。 |
| event 丢失或重连 | 重连后主动调用 `network.getApConfig` 校准。 |

### 4.2 `network.apStateChanged`

**触发条件**：

- AP 启动、停止、重启、异常或恢复。
- 配置应用导致 AP 服务状态变化。

#### Payload：`NetworkApStateChangedEvent`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `interfaceId` | string | no | interface id | omitted | AP 接口。 |
| `state` | `NetworkApState` | yes | object | none | 变化后的 AP 状态。 |
| `previousState` | `NetworkApState` | no | object | omitted | 变化前状态。 |
| `reason` | string enum | no | `user_request`, `system_policy`, `config_applied`, `error`, `unknown` | `unknown` | 变化原因。 |

#### d block 示例

```json
{
  "event": "network.apStateChanged",
  "intent": 1,
  "data": {
    "state": {
      "interfaceId": "wlan0",
      "enabled": true,
      "state": "enabled",
      "ssid": "NearHub-Cast-A1",
      "clientCount": 1
    },
    "previousState": {
      "interfaceId": "wlan0",
      "enabled": true,
      "state": "enabled",
      "ssid": "NearHub-Cast-A1",
      "clientCount": 1
    },
    "reason": "user_request"
  }
}
```


#### 客户端处理建议

| 场景 | 建议 |
|---|---|
| AP 默认开启 | 配对主路径通常只校验 state 是否 running，不主动 start。 |
| AP 被关闭 | 若产品允许且能力支持，可调用 `network.startAp`；否则提示用户或中止。 |

### 4.3 `network.apClientChanged`

**触发条件**：

- STA 客户端加入或离开 AP。
- 设备刷新客户端列表时发现客户端状态变化。

#### Payload：`NetworkApClientChangedEvent`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `interfaceId` | string | no | interface id | omitted | AP 接口。 |
| `change` | string enum | yes | `joined`, `left`, `updated` | none | 客户端变化类型。 |
| `client` | `NetworkApClientInfo` | yes | object | none | 客户端摘要。 |
| `reason` | string enum | no | `association`, `disconnect`, `timeout`, `unknown` | `unknown` | 变化原因。 |

#### d block 示例

```json
{
  "event": "network.apClientChanged",
  "intent": 1,
  "data": {
    "change": "joined",
    "client": {
      "clientId": "client_001"
    },
    "reason": "user_request"
  }
}
```


#### 客户端处理建议

| 场景 | 建议 |
|---|---|
| 强验收 | 若产品要求 NA20 侧确认 NT10 可见，可用该事件或 `network.getApClients` 辅助判断。 |
| 隐私限制 | 客户端标识可使用 opaque id，避免在普通日志中记录 MAC。 |

## 5. Capability

Capability name: `network.ap`。

| 能力字段 | 类型 | 必填 | 取值范围 / 枚举 | 说明 |
|---|---|---:|---|---|
| `capability` | string | yes | fixed `network.ap` | capability 名称。 |
| `securityTypes` | `NetworkWifiSecurityType[]` | yes | see enum | 支持安全类型。 |
| `bands` | `NetworkWifiBand[]` | no | see enum | 支持频段。 |
| `credentialExportModes` | `NetworkCredentialType[]` | yes | see enum | 可导出凭据形态。 |
| `credentialExportPolicies` | string[] | no | `one_time`, `redacted`, `none` | 凭据导出策略。 |
| `defaultEnabled` | boolean | no | bool | AP 是否默认开启；NA20 为 `true`。 |
| `canStartStop` | boolean | yes | bool | 是否支持远程启停 AP。 |
| `clientListSupported` | boolean | no | bool | 是否支持客户端列表。 |
| `maxClients` | uint16 | no | `0..65535` | 最大客户端数量。 |

## 6. 字段 / Schemas

### 6.1 Schema 层级速览

```text
NetworkApCapabilities
NetworkApConfig
  credential: NetworkCredential
NetworkApState
NetworkApClients
  clients: NetworkApClientInfo[]
NetworkApConfigChangedEvent
NetworkApStateChangedEvent
NetworkApClientChangedEvent
```

Capability 描述设备能做什么；`NetworkApConfig` 描述当前或目标配置；`NetworkApState` 描述 AP 服务角色状态。

### 6.2 请求与响应 Schemas

#### `NetworkGetApCapabilitiesParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `interfaceId` | string | no | AP-capable interface id | `defaults.ap` | AP 接口。 |

#### `NetworkGetApConfigParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `interfaceId` | string | no | AP-capable interface id | `defaults.ap` | AP 接口。 |
| `credentialExport` | string enum | no | `none`, `one_time` | `none` | 是否一次性导出凭据。 |

#### `NetworkSetApConfigParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `config` | `NetworkApConfig` | yes | object | none | 目标 AP 配置。 |
| `apply` | string enum | no | `immediate`, `on_restart` | `immediate` | 生效策略。 |

#### `NetworkSetApConfigResult`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `config` | `NetworkApConfig` | yes | object | none | 设备接受的配置摘要。 |
| `applied` | boolean | yes | bool | none | 是否已经生效。 |
| `requiresApRestart` | boolean | no | bool | omitted | 是否需要 AP 重启。 |

#### `NetworkApActionParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `interfaceId` | string | no | AP-capable interface id | `defaults.ap` | AP 接口。 |
| `timeoutMs` | uint32 | no | `0..uint32 max` | omitted | 等待超时。 |

#### `NetworkApActionResult`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `accepted` | boolean | yes | bool | none | 动作是否被接受。 |
| `state` | `NetworkApState` | yes | object | none | 操作后的状态摘要。 |

### 6.3 Capability Schemas

#### `NetworkApCapabilities`

字段同第 5 章 Capability 表；采纳时可作为 capability object 或 query result 复用。

### 6.4 Event Schemas

事件 payload 字段见第 4 章；采纳时应为每个 event 分配独立 event schema。

### 6.5 State / Config / Object Schemas

#### `NetworkApConfig`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `interfaceId` | string | no | interface id | omitted | AP 接口标识。 |
| `ssid` | string | yes | non-empty | none | AP SSID。 |
| `security` | `NetworkWifiSecurityType` | yes | see enum | none | AP 安全类型。 |
| `credential` | `NetworkCredential` | no | object | omitted | AP 凭据；响应和事件必须按敏感字段处理。 |
| `hidden` | boolean | no | bool | `false` | 是否隐藏 SSID。 |
| `band` | `NetworkWifiBand` | no | see enum | omitted | 频段。 |
| `channel` | uint16 | no | valid channel | omitted | 信道。 |
| `maxClients` | uint16 | no | `0..65535` | omitted | 客户端上限。 |

#### `NetworkCredential`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `type` | `NetworkCredentialType` | yes | `passphrase`, `pairing_token`, `opaque_ref` | none | 凭据形态；配对主路径使用 `passphrase`。 |
| `value` | string | yes | secret or reference | none | 敏感凭据内容或引用值；不得进入普通日志。 |
| `exportPolicy` | string enum | no | `one_time`, `redacted`, `reusable_ref` | omitted | 导出策略。 |
| `expiresAtMs` | uint64 | no | epoch ms or `0` | omitted | 过期时间；`0` 表示设备未声明。 |
| `exportId` | string | no | opaque id | omitted | 导出标识，用于审计或防重放。 |

#### `NetworkApState`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `interfaceId` | string | no | interface id | omitted | AP 接口。 |
| `state` | string enum | yes | `stopped`, `starting`, `running`, `stopping`, `error` | none | AP 服务状态。 |
| `ssid` | string | no | SSID | omitted | 当前运行 SSID。 |
| `bssid` | string | no | BSSID | omitted | 当前 BSSID。 |
| `clientCount` | uint16 | no | `0..65535` | omitted | 客户端数量。 |
| `lastError` | string enum | no | `unsupported_security`, `start_failed`, `policy_denied`, `unknown` | omitted | 最近失败原因。 |

#### `NetworkApClients`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `interfaceId` | string | no | interface id | omitted | AP 接口。 |
| `clients` | `NetworkApClientInfo[]` | yes | array | none | 客户端列表。 |

#### `NetworkApClientInfo`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `clientId` | string | yes | opaque id | none | 客户端标识；优先使用 opaque id。 |
| `macAddress` | string | no | policy-dependent | omitted | 客户端 MAC，可脱敏或省略。 |
| `ipAddress` | string | no | IP address | omitted | 客户端租约地址，不表示 AP 本端 IP。 |
| `hostname` | string | no | host name | omitted | 客户端主机名。 |
| `connectedMs` | uint64 | no | milliseconds | omitted | 已连接时长。 |

#### 枚举

| Type | 候选值 | 说明 |
|---|---|---|
| `NetworkWifiSecurityType` | `open`, `wpa2_psk`, `wpa3_sae`, `wpa2_wpa3_mixed` | 与 `network.wifi` 共享。 |
| `NetworkWifiBand` | `2g4`, `5g`, `6g`, `auto` | 与 `network.wifi` 共享。 |
| `NetworkCredentialType` | `passphrase`, `pairing_token`, `opaque_ref` | 与 `network.wifi` 共享。 |

## 7. 错误

| 错误 | 适用场景 | 说明 |
|---|---|---|
| `NOT_SUPPORTED` | 设备或接口不支持 AP 能力、启停或客户端列表。 | 使用 adopted numeric code `3`。 |
| `INVALID_ARGUMENT` | SSID、安全类型、导出策略或 action 参数非法。 | 使用 adopted numeric code `10`。 |
| `OUT_OF_RANGE` | 信道、SSID 长度或客户端上限超范围。 | 使用 adopted numeric code `11`。 |
| `PERMISSION_DENIED` | 凭据导出、设置配置或停止 AP 被策略拒绝。 | 使用 adopted numeric code `9`。 |
| `BUSY` | AP 正在启动、停止或重配置。 | 使用 adopted numeric code `5`。 |
| `NETWORK_CREDENTIAL_EXPORT_DENIED` | 候选业务错误：一次性凭据导出被拒绝。 | `[REVIEW-DRAFT]`；采纳前确认是否需要 feature-specific errorCode。 |
| `NETWORK_AP_CLIENT_LIST_UNAVAILABLE` | 候选业务错误：客户端列表不可用。 | `[REVIEW-DRAFT]`；若客户端列表不进 MVP，可不采纳。 |

## 8. Legacy 映射

Legacy 映射是迁移证据，不是 runtime 合同。

| legacy 项 | 候选映射 | 状态 | 说明 |
|---|---|---|---|
| VM33 `Config.Get:APInfo` | `network.getApConfig` | `[REVIEW-DRAFT]` | 字段线索：`Config.Ssid` -> `ssid`，`Config.Password` -> `credential.value`，`Config.FreqBand` -> `band`。 |
| VM33 `Config.Set:APInfo` | `network.setApConfig` | `[REVIEW-DRAFT]` | `Ssid` 和 `Password` 可映射到 AP config；是否立即重启 AP 待确认。 |
| VM33 `Wifi.OpenApService` / `Wifi.openApService` | optional `network.startAp` + possible `network.setApConfig` | `[REVIEW-DRAFT]` | 旧 payload 同时带 `Password`、`FreqBand`、`Retransmit`；新协议中 AP 默认开启，开关低优先级。 |

## 9. 测试要点

| 类型 | 要点 |
|---|---|
| happy path | NA20 AP 默认 running，Host 读取 config 并一次性导出凭据。 |
| config path | `network.setApConfig` 可设置 SSID/密码，响应不回显明文密码。 |
| event path | AP 配置变化触发 `network.apConfigChanged`；启停或异常触发 `network.apStateChanged`。 |
| boundary case | `credentialExport=none` 不返回 credential；接口省略时使用默认 AP 接口。 |
| error case | 凭据导出被拒绝、接口不是 AP-capable、AP 正在重配置。 |
| compatibility | VM33 `APInfo` / `OpenApService` 字段可迁移，但旧 `Retransmit` 暂不进入 MVP。 |
| pairing path | Host 用 AP config 组装 NT10 Wi-Fi profile；AP 本端 IP 和客户端列表仅作为可选验收。 |

## 10. 待确认问题

| 问题 | 影响 | 当前建议 | 状态 |
|---|---|---|---|
| NA20 AP 是否默认开启？ | pairing / conformance | 是，配对主路径不调用 `startAp`。 | decided |
| NA20 AP SSID/密码是否可由上位机配置？ | schema / product behavior | 是，使用 `network.setApConfig`。 | decided |
| AP 凭据是否允许通过 AXTP 导出？ | security / pairing | 是，允许一次性导出；日志必须脱敏。 | decided |
| `network.getApClients` 是否是配对成功强制验收？ | conformance / product behavior | 当前建议 optional；默认以 NT10 Wi-Fi state 或 IP ready 验收。 | open |
| AP DHCP Server 地址池归属哪里？ | schema boundary | AP 本端 IP 归 `network.ip`；DHCP Server 地址池暂留 open。 | open |
| 一次性凭据导出的有效期、频率和目标设备绑定如何定义？ | security / registry | 建议采纳前补充 exportId、expiresAtMs 和重放策略。 | open |
