---
status: generated
contract: true
generated: true
domain: network
feature: network.wifi
registry: ../../../../contract/registry/domains/network/domain.yaml
lastReviewed: 2026-06-15
---

# network.wifi

## 0. 速读结论

| 项目 | 内容 |
|---|---|
| 这个能力做什么 | 管理设备作为 Wi-Fi STA 的能力、profile 保存、扫描、连接、断开和认证/关联状态事件。 |
| 当前状态 | generated；已写入 `../../../../contract/registry/domains/network/domain.yaml`，并已刷新到 `contract/protocol/axtp.protocol.yaml` 与 `contract/generated/**`。 |
| 是否可直接实现 | 是，但实现合同以 `contract/protocol/axtp.protocol.yaml` / `contract/generated/**` 为准；本文保留的 open review markers 不属于已生成合同。 |
| 主要交互 | RPC + EVENT |
| 是否使用 STREAM | 否 |
| Registry readiness | ready；P0 / confirmed subset 已写入 registry source 并生成。 |
| Conformance | needed |
| 主要未决问题 | pairing profile 是否默认持久化、IP ready 是否纳入配对验收、`forgetWifi` 是否进入 MVP。 |

## 1. 功能说明

`network.wifi` 描述设备作为 Wi-Fi STA 连接外部 AP 的控制面。Cast RX/TX 配对中，Host 从 NA20 的 `network.ap` 一次性导出 AP 凭据，再调用 NT10 的 `network.setWifiConfig` 写入 STA profile。

已确认的 flow 决策包括：NT10 写入新的 Wi-Fi profile 后，默认立即使用该新 profile 连接到新的 Wi-Fi；配对主路径使用来自 `network.ap` 的一次性 `passphrase`；多设备配对策略由 Host local-only 决定，不进入设备协议。

`network.wifi` 的 P0 / confirmed subset 已进入 generated 合同；正式 methodId、eventId、errorCode、fieldId 以 `contract/registry/**`、`contract/protocol/axtp.protocol.yaml` 和 `contract/generated/**` 为准。本文保留的 review 标记仅用于后续修订。

## 2. 能力边界

| 类型 | 内容 |
|---|---|
| 包含 | Wi-Fi STA 能力查询，包括安全类型、频段、扫描、profile 保存和凭据导入模式。 |
| 包含 | 查询、写入和更新 Wi-Fi profile；响应和事件不得明文回显敏感凭据。 |
| 包含 | 扫描 AP、连接、断开和查询 STA 角色状态。 |
| 包含 | profile 配置变化、扫描结果、扫描完成、认证/关联/断开/失败状态事件。 |
| 不包含 | 接口枚举、MAC、基础链路；这些属于 `network.interface`。 |
| 不包含 | DHCP/static 地址、IP ready、DNS 和网关；这些属于 `network.ip`。 |
| 不包含 | 设备自身作为 AP 的 SSID/安全/凭据导出；这些属于 `network.ap`。 |
| 不包含 | 跨设备一键配对协议；默认由 Host 编排 `network.ap`、`network.wifi` 和可选 `network.ip`。 |
| 数据面 | 本 feature 不定义 STREAM payload，所有操作均通过 RPC method/event 完成。 |

## 3. 方法 Methods

已生成 methodId、eventId、bitOffset 和 schema fieldId 以 contract/registry/generated 为准；本文不重新分配正式 ID，保留的 draft/review 标记仅作为后续修订输入。

### 3.0 方法速览

| Method | 调用类型 | 用途 | Params Schema | Result Schema | 是否触发事件 | 状态 |
|---|---|---|---|---|---|---|
| `network.getWifiCapabilities` | query | 查询 STA 能力。 | `NetworkGetWifiCapabilitiesParams` | `NetworkWifiCapabilities` | 否 | draft |
| `network.getWifiConfig` | query | 查询保存的 Wi-Fi profile 摘要。 | `NetworkGetWifiConfigParams` | `NetworkWifiConfig` | 否 | draft |
| `network.setWifiConfig` | command | 写入或更新 Wi-Fi profile。 | `NetworkSetWifiConfigParams` | `NetworkSetWifiConfigResult` | 是，`network.wifiConfigChanged`，可能触发 `network.wifiStateChanged` | draft |
| `network.scanWifi` | action/query | 扫描外部 AP。 | `NetworkScanWifiParams` | `NetworkScanWifiResult` | 可能触发 `network.wifiScanResultReported` | draft |
| `network.connectWifi` | action | 连接已保存 profile 或 inline profile。 | `NetworkConnectWifiParams` | `NetworkWifiActionResult` | 是，`network.wifiStateChanged` | draft |
| `network.disconnectWifi` | action | 断开当前 Wi-Fi 连接。 | `NetworkDisconnectWifiParams` | `NetworkWifiActionResult` | 是，`network.wifiStateChanged` | draft |
| `network.getWifiState` | query | 查询 STA 角色状态。 | `NetworkGetWifiStateParams` | `NetworkWifiState` | 否 | draft |

### 3.1 `network.getWifiCapabilities`

**用途**：查询 Wi-Fi STA 支持的安全类型、频段、扫描、profile 保存和凭据导入能力。

| 项 | 内容 |
|---|---|
| 调用类型 | query |
| Params Schema | `NetworkGetWifiCapabilitiesParams` |
| Result Schema | `NetworkWifiCapabilities` |
| 是否触发事件 | 否 |
| 幂等性 / 异步性 | 幂等；同步返回能力快照。 |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_ARGUMENT`, `NOT_FOUND`, `PERMISSION_DENIED`, `UNAVAILABLE` |

#### 3.1.1 请求参数 Params：`NetworkGetWifiCapabilitiesParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `interfaceId` | string | no | STA-capable interface id | `defaults.wifiSta` | STA 接口；省略表示默认 STA 接口。 |

#### 3.1.2 返回结果 Result：`NetworkWifiCapabilities`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `capability` | string | yes | fixed `network.wifi` | none | capability 名称。 |
| `securityTypes` | `NetworkWifiSecurityType[]` | yes | see enum | none | 支持安全类型。 |
| `bands` | `NetworkWifiBand[]` | no | see enum | omitted | 支持频段。 |
| `credentialImportModes` | `NetworkCredentialType[]` | yes | `passphrase`, `pairing_token`, `opaque_ref` | none | 可写入的凭据形态。 |
| `savedProfilesSupported` | boolean | yes | bool | none | 是否支持保存 profile。 |
| `scanSupported` | boolean | yes | bool | none | 是否支持扫描。 |
| `autoConnectSupported` | boolean | no | bool | omitted | 是否支持 profile 自动连接。 |

#### 3.1.3 d block 示例

request:

```json
{
  "id": 101,
  "method": "network.getWifiCapabilities",
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
    "capability": "network.wifi",
    "securityTypes": [
      "wpa2_psk",
      "wpa3_sae"
    ],
    "credentialImportModes": [
      "passphrase",
      "pairing_token",
      "opaque_ref"
    ],
    "savedProfilesSupported": true,
    "scanSupported": true
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
| `NOT_FOUND` | 指定接口不存在或不是 STA-capable。 | 使用 adopted numeric code `12`。 |
| `NOT_SUPPORTED` | 设备不支持 Wi-Fi STA 能力。 | 使用 adopted numeric code `3`。 |

### 3.2 `network.getWifiConfig`

**用途**：查询保存的 Wi-Fi profile 摘要；不得返回明文敏感凭据。

| 项 | 内容 |
|---|---|
| 调用类型 | query |
| Params Schema | `NetworkGetWifiConfigParams` |
| Result Schema | `NetworkWifiConfig` |
| 是否触发事件 | 否 |
| 幂等性 / 异步性 | 幂等；同步返回当前配置摘要。 |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_ARGUMENT`, `NOT_FOUND`, `PERMISSION_DENIED`, `UNAVAILABLE` |

#### 3.2.1 请求参数 Params：`NetworkGetWifiConfigParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `interfaceId` | string | no | STA-capable interface id | `defaults.wifiSta` | STA 接口。 |
| `includeProfiles` | boolean | no | bool | `true` | 是否返回 profile 摘要。 |

#### 3.2.2 返回结果 Result：`NetworkWifiConfig`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `interfaceId` | string | no | interface id | omitted | STA 接口。 |
| `profiles` | `NetworkWifiProfile[]` | no | array | omitted | 已保存 profile 摘要；不含明文 credential。 |
| `defaultProfileId` | string | no | profile id | omitted | 默认 profile。 |

#### 3.2.3 d block 示例

request:

```json
{
  "id": 102,
  "method": "network.getWifiConfig",
  "params": {
    "interfaceId": "wlan0",
    "includeProfiles": true
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
    "profiles": [
      {
        "profileId": "profile_na20",
        "ssid": "NearHub-Cast-A1",
        "securityType": "wpa2_psk",
        "credential": {
          "type": "passphrase",
          "secretRef": "<redacted>"
        },
        "source": "pairing"
      }
    ],
    "defaultProfileId": "profile_na20"
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
| `PERMISSION_DENIED` | 调用方无权读取配置摘要。 | 使用 adopted numeric code `9`。 |

### 3.3 `network.setWifiConfig`

**用途**：写入或更新 Wi-Fi profile。配对场景用于把 NA20 AP profile 写入 NT10。

| 项 | 内容 |
|---|---|
| 调用类型 | command |
| Params Schema | `NetworkSetWifiConfigParams` |
| Result Schema | `NetworkSetWifiConfigResult` |
| 是否触发事件 | 是，profile 变化触发 `network.wifiConfigChanged`；若保存后连接，随后触发 `network.wifiStateChanged`。 |
| 幂等性 / 异步性 | 建议幂等；连接过程异步。`source=pairing` 时 `connectAfterSave` 默认 `true`。 |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_ARGUMENT`, `OUT_OF_RANGE`, `INVALID_STATE`, `BUSY`, `PERMISSION_DENIED` |

#### 3.3.1 请求参数 Params：`NetworkSetWifiConfigParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `interfaceId` | string | no | STA-capable interface id | `defaults.wifiSta` | STA 接口。 |
| `profile` | `NetworkWifiProfile` | yes | object | none | 要创建或更新的 Wi-Fi profile。 |
| `replaceExisting` | boolean | no | bool | `false` | 同 SSID/BSSID 或同 profileId 已存在时是否覆盖。 |
| `makeDefault` | boolean | no | bool | `false` | 是否设为默认 profile。 |
| `connectAfterSave` | boolean | no | bool | `true` when `profile.source=pairing`; otherwise `false` | 保存后是否立即连接。 |

#### 3.3.2 返回结果 Result：`NetworkSetWifiConfigResult`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `profileId` | string | yes | profile id | none | 设备接受或分配的 profileId。 |
| `config` | `NetworkWifiConfig` | no | object | omitted | 更新后的配置摘要，不含明文 credential。 |
| `connectStarted` | boolean | no | bool | omitted | 是否已经开始连接。 |

#### 3.3.3 d block 示例

request:

```json
{
  "id": 103,
  "method": "network.setWifiConfig",
  "params": {
    "interfaceId": "wlan0",
    "profile": {
      "profileId": "profile_na20",
      "ssid": "NearHub-Cast-A1",
      "securityType": "wpa2_psk",
      "credential": {
        "type": "passphrase",
        "secretRef": "<redacted>"
      },
      "source": "pairing"
    },
    "replaceExisting": true,
    "makeDefault": true,
    "connectAfterSave": true
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
    "profiles": [
      {
        "profileId": "profile_na20",
        "ssid": "NearHub-Cast-A1",
        "securityType": "wpa2_psk",
        "credential": {
          "type": "passphrase",
          "secretRef": "<redacted>"
        },
        "source": "pairing"
      }
    ],
    "defaultProfileId": "profile_na20"
  }
}
```

#### 3.3.4 可能触发的事件

| Event | 触发条件 | Payload Schema | 客户端处理建议 |
|---|---|---|---|
| `network.wifiConfigChanged` | profile 新增、更新或默认 profile 改变。 | `NetworkWifiConfigChangedEvent` | 更新本地 profile 摘要；不要期待敏感凭据回显。 |
| `network.wifiStateChanged` | 保存后默认或显式连接。 | `NetworkWifiStateChangedEvent` | 展示连接进度；connected 后如需要继续查 `network.ip`。 |

#### 3.3.5 错误

| 错误 | 场景 | 返回建议 |
|---|---|---|
| `INVALID_ARGUMENT` | `profile` 缺少 SSID、安全类型或凭据组合非法。 | 使用 adopted numeric code `10`。 |
| `PERMISSION_DENIED` | 当前策略禁止保存 profile 或导入凭据。 | 使用 adopted numeric code `9`。 |
| `BUSY` | Wi-Fi 正在连接、扫描或写配置。 | 使用 adopted numeric code `5`。 |

### 3.4 `network.scanWifi`

**用途**：扫描外部 AP。配对主路径不强制扫描，但扫描能力有助于诊断或人工选择。

| 项 | 内容 |
|---|---|
| 调用类型 | action/query |
| Params Schema | `NetworkScanWifiParams` |
| Result Schema | `NetworkScanWifiResult` |
| 是否触发事件 | 可能触发 `network.wifiScanResultReported`。 |
| 幂等性 / 异步性 | 可同步返回部分结果，也可异步通过 scan event 上报。 |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_ARGUMENT`, `BUSY`, `TIMEOUT`, `UNAVAILABLE` |

#### 3.4.1 请求参数 Params：`NetworkScanWifiParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `interfaceId` | string | no | STA-capable interface id | `defaults.wifiSta` | STA 接口。 |
| `ssidFilter` | string | no | SSID | omitted | 只扫描指定 SSID。 |
| `timeoutMs` | uint32 | no | `0..uint32 max` | omitted | 扫描超时。 |

#### 3.4.2 返回结果 Result：`NetworkScanWifiResult`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `scanId` | string | no | opaque id | omitted | 异步扫描标识。 |
| `results` | `NetworkWifiScanResult[]` | no | array | omitted | 同步返回结果。 |
| `complete` | boolean | no | bool | omitted | 同步结果是否完整。 |

#### 3.4.3 d block 示例

request:

```json
{
  "id": 104,
  "method": "network.scanWifi",
  "params": {
    "interfaceId": "wlan0",
    "timeoutMs": 5000
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
  "result": {}
}
```

#### 3.4.4 可能触发的事件

| Event | 触发条件 | Payload Schema | 客户端处理建议 |
|---|---|---|---|
| `network.wifiScanResultReported` | 异步扫描返回结果或结束。 | `NetworkWifiScanResultReportedEvent` | 累积结果；complete 后停止等待。 |

#### 3.4.5 错误

| 错误 | 场景 | 返回建议 |
|---|---|---|
| `BUSY` | 正在连接或已有扫描进行中。 | 使用 adopted numeric code `5`。 |
| `TIMEOUT` | 扫描超时。 | 使用 adopted numeric code `6`。 |

### 3.5 `network.connectWifi`

**用途**：连接已保存 profile 或 inline profile。配对写入后默认连接，显式 connect 可用于手动重试。

| 项 | 内容 |
|---|---|
| 调用类型 | action |
| Params Schema | `NetworkConnectWifiParams` |
| Result Schema | `NetworkWifiActionResult` |
| 是否触发事件 | 是，连接过程触发 `network.wifiStateChanged`。 |
| 幂等性 / 异步性 | 连接过程异步；重复连接当前 profile 可返回成功或当前状态。 |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_ARGUMENT`, `NOT_FOUND`, `INVALID_STATE`, `BUSY`, `TIMEOUT` |

#### 3.5.1 请求参数 Params：`NetworkConnectWifiParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `interfaceId` | string | no | STA-capable interface id | `defaults.wifiSta` | STA 接口。 |
| `profileId` | string | conditional | profile id | omitted | 连接已保存 profile；与 `profile` 二选一。 |
| `profile` | `NetworkWifiProfile` | conditional | object | omitted | 使用 inline profile 连接；是否保存由 `profile.persist` 决定。 |
| `timeoutMs` | uint32 | no | `0..uint32 max` | omitted | 连接超时。 |

#### 3.5.2 返回结果 Result：`NetworkWifiActionResult`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `accepted` | boolean | yes | bool | none | 动作是否被接受。 |
| `state` | `NetworkWifiState` | yes | object | none | 操作后的当前或目标状态。 |

#### 3.5.3 d block 示例

request:

```json
{
  "id": 105,
  "method": "network.connectWifi",
  "params": {
    "interfaceId": "wlan0",
    "profileId": "profile_na20",
    "profile": {
      "profileId": "profile_na20",
      "ssid": "NearHub-Cast-A1",
      "securityType": "wpa2_psk",
      "credential": {
        "type": "passphrase",
        "secretRef": "<redacted>"
      },
      "source": "pairing"
    },
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
      "state": "disconnected",
      "profileId": "profile_na20"
    }
  }
}
```

#### 3.5.4 可能触发的事件

| Event | 触发条件 | Payload Schema | 客户端处理建议 |
|---|---|---|---|
| `network.wifiStateChanged` | STA 扫描、认证、关联、断开或失败。 | `NetworkWifiStateChangedEvent` | 用 state/failureReason 展示进度和失败原因。 |

#### 3.5.5 错误

| 错误 | 场景 | 返回建议 |
|---|---|---|
| `NOT_FOUND` | `profileId` 不存在。 | 使用 adopted numeric code `12`，details 可标注候选 `NETWORK_PROFILE_NOT_FOUND`。 |
| `INVALID_ARGUMENT` | `profileId` 与 `profile` 同时缺失或同时出现。 | 使用 adopted numeric code `10`。 |

### 3.6 `network.disconnectWifi`

**用途**：断开当前 Wi-Fi STA 连接。

| 项 | 内容 |
|---|---|
| 调用类型 | action |
| Params Schema | `NetworkDisconnectWifiParams` |
| Result Schema | `NetworkWifiActionResult` |
| 是否触发事件 | 是，状态实际变化后触发 `network.wifiStateChanged`。 |
| 幂等性 / 异步性 | 对已 disconnected 的 STA 建议返回成功；断开过程可异步。 |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_STATE`, `BUSY`, `PERMISSION_DENIED`, `TIMEOUT` |

#### 3.6.1 请求参数 Params：`NetworkDisconnectWifiParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `interfaceId` | string | no | STA-capable interface id | `defaults.wifiSta` | STA 接口。 |

#### 3.6.2 返回结果 Result：`NetworkWifiActionResult`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `accepted` | boolean | yes | bool | none | 动作是否被接受。 |
| `state` | `NetworkWifiState` | yes | object | none | 操作后的当前或目标状态。 |

#### 3.6.3 d block 示例

request:

```json
{
  "id": 106,
  "method": "network.disconnectWifi",
  "params": {
    "interfaceId": "wlan0"
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
      "state": "disconnected",
      "profileId": "profile_na20"
    }
  }
}
```

#### 3.6.4 可能触发的事件

| Event | 触发条件 | Payload Schema | 客户端处理建议 |
|---|---|---|---|
| `network.wifiStateChanged` | STA 进入 disconnecting / disconnected。 | `NetworkWifiStateChangedEvent` | 更新 UI；IP 地址失效由 `network.ipConfigChanged` 表达。 |

#### 3.6.5 错误

| 错误 | 场景 | 返回建议 |
|---|---|---|
| `PERMISSION_DENIED` | 当前策略禁止断开。 | 使用 adopted numeric code `9`。 |

### 3.7 `network.getWifiState`

**用途**：查询当前 Wi-Fi STA 角色状态。

| 项 | 内容 |
|---|---|
| 调用类型 | query |
| Params Schema | `NetworkGetWifiStateParams` |
| Result Schema | `NetworkWifiState` |
| 是否触发事件 | 否 |
| 幂等性 / 异步性 | 幂等；同步返回当前快照。 |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_ARGUMENT`, `NOT_FOUND`, `UNAVAILABLE` |

#### 3.7.1 请求参数 Params：`NetworkGetWifiStateParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `interfaceId` | string | no | STA-capable interface id | `defaults.wifiSta` | STA 接口。 |

#### 3.7.2 返回结果 Result：`NetworkWifiState`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `interfaceId` | string | no | interface id | omitted | STA 接口。 |
| `state` | string enum | yes | `disabled`, `disconnected`, `scanning`, `connecting`, `connected`, `disconnecting`, `error` | none | Wi-Fi STA 状态。 |
| `profileId` | string | no | profile id | omitted | 当前或最近使用的 profile。 |
| `ssid` | string | no | SSID | omitted | 当前目标 SSID。 |
| `bssid` | string | no | BSSID | omitted | 当前连接 BSSID。 |
| `rssiDbm` | int16 | no | dBm | omitted | 信号强度。 |
| `failureReason` | `NetworkWifiFailureReason` | no | see enum | omitted | 最近失败原因。 |

#### 3.7.3 d block 示例

request:

```json
{
  "id": 107,
  "method": "network.getWifiState",
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
    "interfaceId": "wlan0",
    "state": "connected",
    "profileId": "profile_na20",
    "ssid": "NearHub-Cast-A1",
    "ipReady": true
  }
}
```

#### 3.7.4 可能触发的事件

| Event | 触发条件 | Payload Schema | 客户端处理建议 |
|---|---|---|---|
| 无 | 查询不改变状态。 | none | 无需处理。 |

#### 3.7.5 错误

| 错误 | 场景 | 返回建议 |
|---|---|---|
| `NOT_FOUND` | 指定接口不存在。 | 使用 adopted numeric code `12`。 |

## 4. 事件 Events

### 4.0 事件速览

| Event | 触发条件 | Payload Schema | 客户端处理建议 | 状态 |
|---|---|---|---|---|
| `network.wifiConfigChanged` | Wi-Fi profile 新增、更新、删除、恢复默认或设备策略改变。 | `NetworkWifiConfigChangedEvent` | 更新 profile 摘要；敏感凭据不应从事件读取。 | draft |
| `network.wifiStateChanged` | STA 扫描、认证、关联、断开或 Wi-Fi 角色错误。 | `NetworkWifiStateChangedEvent` | 展示连接进度和失败原因；connected 后可查 `network.ip`。 | draft |
| `network.wifiScanResultReported` | 异步扫描发现 AP 或扫描完成。 | `NetworkWifiScanResultReportedEvent` | 累积扫描结果；complete 后停止等待。 | draft |

### 4.1 `network.wifiConfigChanged`

**触发条件**：

- `network.setWifiConfig` 新增或更新 profile。
- 设备策略、恢复默认或 future forget/reset 改变 profile。

#### Payload：`NetworkWifiConfigChangedEvent`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `interfaceId` | string | no | interface id | omitted | STA 接口。 |
| `config` | `NetworkWifiConfig` | yes | object | none | 变化后的配置摘要，不含明文 credential。 |
| `changedFields` | string[] | no | field paths | omitted | 变化字段。 |
| `reason` | string enum | no | `user_request`, `pairing`, `system_policy`, `factory_reset`, `unknown` | `unknown` | 变化原因。 |

#### d block 示例

```json
{
  "event": "network.wifiConfigChanged",
  "intent": 1,
  "data": {
    "config": {
      "interfaceId": "wlan0",
      "profiles": [
        {
          "profileId": "profile_na20",
          "ssid": "NearHub-Cast-A1",
          "securityType": "wpa2_psk",
          "credential": {
            "type": "passphrase",
            "secretRef": "<redacted>"
          },
          "source": "pairing"
        }
      ],
      "defaultProfileId": "profile_na20"
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
| 配对 profile 写入 | 记录 profileId；等待 `network.wifiStateChanged` 进入 connecting / connected。 |
| event 丢失或重连 | 重连后主动调用 `network.getWifiConfig` 校准。 |

### 4.2 `network.wifiStateChanged`

**触发条件**：

- 设备扫描、认证、关联、断开或失败。
- `network.setWifiConfig(connectAfterSave=true)` 或 `network.connectWifi` 发起连接。

#### Payload：`NetworkWifiStateChangedEvent`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `interfaceId` | string | no | interface id | omitted | STA 接口。 |
| `state` | `NetworkWifiState` | yes | object | none | 变化后的 Wi-Fi 状态。 |
| `previousState` | `NetworkWifiState` | no | object | omitted | 变化前状态。 |
| `reason` | string enum | no | `connect_started`, `connect_completed`, `auth_failed`, `ap_not_found`, `disconnect`, `link_lost`, `unknown` | `unknown` | 变化原因。 |

#### d block 示例

```json
{
  "event": "network.wifiStateChanged",
  "intent": 1,
  "data": {
    "state": {
      "interfaceId": "wlan0",
      "state": "disconnected",
      "profileId": "profile_na20"
    },
    "previousState": {
      "interfaceId": "wlan0",
      "state": "connected",
      "profileId": "profile_na20",
      "ssid": "NearHub-Cast-A1",
      "ipReady": true
    },
    "reason": "disconnect"
  }
}
```


#### 客户端处理建议

| 场景 | 建议 |
|---|---|
| `state=connected` | 表示 Wi-Fi 认证/关联成功；如果产品要求 IP ready，继续查询 `network.getIpConfig`。 |
| 认证失败 | 展示可诊断失败原因，并可重新读取 `network.ap` 凭据后重试。 |
| event 丢失或重连 | 调用 `network.getWifiState` 校准。 |

### 4.3 `network.wifiScanResultReported`

**触发条件**：

- 异步扫描返回一条或多条 AP 结果。
- 扫描完成或超时。

#### Payload：`NetworkWifiScanResultReportedEvent`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `interfaceId` | string | no | interface id | omitted | STA 接口。 |
| `scanId` | string | no | opaque id | omitted | 扫描标识。 |
| `result` | `NetworkWifiScanResult` | no | object | omitted | 单条扫描结果。 |
| `complete` | boolean | no | bool | `false` | 是否扫描完成。 |
| `reason` | string enum | no | `result`, `complete`, `timeout`, `error` | omitted | 上报原因。 |

#### d block 示例

```json
{
  "event": "network.wifiScanResultReported",
  "intent": 1,
  "data": {
    "reason": "result"
  }
}
```


#### 客户端处理建议

| 场景 | 建议 |
|---|---|
| `complete=false` 且有 result | 累积扫描结果。 |
| `complete=true` | 停止等待该 scanId。 |

## 5. Capability

Capability name: `network.wifi`。

| 能力字段 | 类型 | 必填 | 取值范围 / 枚举 | 说明 |
|---|---|---:|---|---|
| `capability` | string | yes | fixed `network.wifi` | capability 名称。 |
| `securityTypes` | `NetworkWifiSecurityType[]` | yes | see enum | 支持安全类型。 |
| `bands` | `NetworkWifiBand[]` | no | see enum | 支持频段。 |
| `credentialImportModes` | `NetworkCredentialType[]` | yes | see enum | 可写入凭据形态。 |
| `savedProfilesSupported` | boolean | yes | bool | 是否支持保存 profile。 |
| `scanSupported` | boolean | yes | bool | 是否支持扫描。 |
| `autoConnectSupported` | boolean | no | bool | 是否支持自动连接。 |

## 6. 字段 / Schemas

### 6.1 Schema 层级速览

```text
NetworkWifiCapabilities
NetworkWifiConfig
  profiles: NetworkWifiProfile[]
NetworkWifiProfile
  credential: NetworkCredential
NetworkWifiState
NetworkWifiScanResult
NetworkWifiConfigChangedEvent
NetworkWifiStateChangedEvent
NetworkWifiScanResultReportedEvent
```

Capability 描述设备能做什么；Config/Profile 描述保存的目标网络；State 描述当前 STA 角色状态。

### 6.2 请求与响应 Schemas

#### `NetworkGetWifiCapabilitiesParams` / `NetworkGetWifiConfigParams` / `NetworkGetWifiStateParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `interfaceId` | string | no | STA-capable interface id | `defaults.wifiSta` | STA 接口。 |
| `includeProfiles` | boolean | no | bool | method-specific | 仅 `getWifiConfig` 使用，控制是否返回 profile 摘要。 |

#### `NetworkSetWifiConfigParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `interfaceId` | string | no | STA-capable interface id | `defaults.wifiSta` | STA 接口。 |
| `profile` | `NetworkWifiProfile` | yes | object | none | 要保存的 profile。 |
| `replaceExisting` | boolean | no | bool | `false` | 是否覆盖已存在 profile。 |
| `makeDefault` | boolean | no | bool | `false` | 是否设为默认 profile。 |
| `connectAfterSave` | boolean | no | bool | see 3.3 | 保存后是否立即连接。 |

#### `NetworkSetWifiConfigResult`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `profileId` | string | yes | profile id | none | 设备接受或分配的 profileId。 |
| `config` | `NetworkWifiConfig` | no | object | omitted | 更新后的配置摘要。 |
| `connectStarted` | boolean | no | bool | omitted | 是否已经开始连接。 |

#### `NetworkScanWifiParams` / `NetworkScanWifiResult`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `interfaceId` | string | no | STA-capable interface id | `defaults.wifiSta` | STA 接口。 |
| `ssidFilter` | string | no | SSID | omitted | 扫描过滤。 |
| `timeoutMs` | uint32 | no | `0..uint32 max` | omitted | 超时。 |
| `scanId` | string | no | opaque id | omitted | 响应字段，异步扫描标识。 |
| `results` | `NetworkWifiScanResult[]` | no | array | omitted | 响应字段，同步扫描结果。 |
| `complete` | boolean | no | bool | omitted | 响应字段，是否完成。 |

#### `NetworkConnectWifiParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `interfaceId` | string | no | STA-capable interface id | `defaults.wifiSta` | STA 接口。 |
| `profileId` | string | conditional | profile id | omitted | 连接已保存 profile。 |
| `profile` | `NetworkWifiProfile` | conditional | object | omitted | inline profile；与 `profileId` 二选一。 |
| `timeoutMs` | uint32 | no | `0..uint32 max` | omitted | 连接超时。 |

#### `NetworkDisconnectWifiParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `interfaceId` | string | no | STA-capable interface id | `defaults.wifiSta` | STA 接口。 |

#### `NetworkWifiActionResult`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `accepted` | boolean | yes | bool | none | 动作是否被接受。 |
| `state` | `NetworkWifiState` | yes | object | none | 操作后的状态摘要。 |

### 6.3 Capability Schemas

#### `NetworkWifiCapabilities`

字段同第 5 章 Capability 表；采纳时可作为 capability object 或 query result 复用。

### 6.4 Event Schemas

事件 payload 字段见第 4 章；采纳时应为每个 event 分配独立 event schema。

### 6.5 State / Config / Object Schemas

#### `NetworkWifiConfig`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `interfaceId` | string | no | interface id | omitted | STA 接口。 |
| `profiles` | `NetworkWifiProfile[]` | no | array | omitted | profile 摘要；不含明文 credential。 |
| `defaultProfileId` | string | no | profile id | omitted | 默认 profile。 |

#### `NetworkWifiProfile`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `profileId` | string | no | profile id | omitted | 已保存 profile 标识；新建可省略。 |
| `ssid` | string | yes | non-empty | none | 目标 AP SSID。 |
| `security` | `NetworkWifiSecurityType` | yes | see enum | none | 目标 AP 安全类型。 |
| `credential` | `NetworkCredential` | no | object | omitted | 敏感凭据；响应和事件不得明文回显。 |
| `bssid` | string | no | BSSID | omitted | 目标 BSSID，用于锁定 NA20 AP。 |
| `hidden` | boolean | no | bool | `false` | 是否连接隐藏 SSID。 |
| `persist` | boolean | no | bool | pairing 建议 `true` | 是否持久化保存；最终默认值见待确认问题。 |
| `autoConnect` | boolean | no | bool | `true` when source=pairing | 后续是否自动连接。 |
| `source` | string enum | no | `user`, `pairing`, `provisioning`, `legacy` | `user` | profile 来源。 |

#### `NetworkCredential`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `type` | `NetworkCredentialType` | yes | `passphrase`, `pairing_token`, `opaque_ref` | none | 凭据形态。 |
| `value` | string | yes | secret or reference | none | 敏感凭据内容或引用值。 |
| `exportPolicy` | string enum | no | `one_time`, `redacted`, `reusable_ref` | omitted | 导出策略。 |
| `expiresAtMs` | uint64 | no | epoch ms or `0` | omitted | 过期时间。 |
| `exportId` | string | no | opaque id | omitted | 来自 `network.ap` 的导出标识。 |

#### `NetworkWifiState`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `interfaceId` | string | no | interface id | omitted | STA 接口。 |
| `state` | string enum | yes | `disabled`, `disconnected`, `scanning`, `connecting`, `connected`, `disconnecting`, `error` | none | STA 状态。 |
| `profileId` | string | no | profile id | omitted | 当前或最近使用的 profile。 |
| `ssid` | string | no | SSID | omitted | 当前目标 SSID。 |
| `bssid` | string | no | BSSID | omitted | 当前 BSSID。 |
| `rssiDbm` | int16 | no | dBm | omitted | 信号强度。 |
| `failureReason` | `NetworkWifiFailureReason` | no | see enum | omitted | 失败原因。 |

#### `NetworkWifiScanResult`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `ssid` | string | yes | SSID | none | AP SSID。 |
| `bssid` | string | no | BSSID | omitted | AP BSSID。 |
| `security` | `NetworkWifiSecurityType` | yes | see enum | none | 安全类型。 |
| `band` | `NetworkWifiBand` | no | see enum | omitted | 频段。 |
| `channel` | uint16 | no | valid channel | omitted | 信道。 |
| `rssiDbm` | int16 | no | dBm | omitted | 信号强度。 |

#### 枚举

| Type | 候选值 | 说明 |
|---|---|---|
| `NetworkWifiSecurityType` | `open`, `wpa2_psk`, `wpa3_sae`, `wpa2_wpa3_mixed` | 与 `network.ap` 共享。 |
| `NetworkWifiBand` | `2g4`, `5g`, `6g`, `auto` | 与 `network.ap` 共享。 |
| `NetworkCredentialType` | `passphrase`, `pairing_token`, `opaque_ref` | 与 `network.ap` 共享。 |
| `NetworkWifiFailureReason` | `auth_failed`, `ap_not_found`, `timeout`, `unsupported_security`, `credential_invalid`, `policy_denied`, `link_lost`, `unknown` | 连接失败原因；DHCP/IP 失败归 `network.ip`。 |

## 7. 交互流程示例 Flow Examples

本章只保留端到端顺序；单个 method 的 request / success 示例见第 3 章，event payload 示例见第 4 章。

### 7.1 查询 Wi-Fi 能力和当前状态

客户端先调用 `network.getWifiCapabilities` 判断 STA/AP、认证方式和保存 profile 能力，再调用 `network.getWifiState` 获取当前连接状态。UI 不应启用 capability 未声明的安全类型、频段或操作。

### 7.2 写入 profile 并连接

配对路径先用 `network.setWifiConfig` 写入 profile；当 `source=pairing` 时，草案建议 `connectAfterSave=true`。随后可显式调用 `network.connectWifi` 做手动重试或连接非默认 profile。最终联网结果以 `network.wifiStateChanged` 或 `network.getWifiState` 为准。

### 7.3 连接成功或失败

设备应通过 `network.wifiStateChanged` 上报 `connecting -> connected/failed`，并在失败响应或 failed event 中携带 `failureReason` / candidate error，避免客户端猜测。Wi-Fi `connected` 只表示 STA 链路完成；IP ready 由 `network.ip` 查询或事件确认。

## 8. 错误

| 错误 | 适用场景 | 说明 |
|---|---|---|
| `NOT_SUPPORTED` | 设备或接口不支持 Wi-Fi STA、扫描或保存 profile。 | 使用 adopted numeric code `3`。 |
| `INVALID_ARGUMENT` | profile 缺字段、枚举非法或参数组合非法。 | 使用 adopted numeric code `10`。 |
| `NOT_FOUND` | 指定 profile 或接口不存在。 | 使用 adopted numeric code `12`。 |
| `BUSY` | STA 正在扫描、连接或写配置。 | 使用 adopted numeric code `5`。 |
| `TIMEOUT` | 扫描或连接超时。 | 使用 adopted numeric code `6`。 |
| `NETWORK_PROFILE_NOT_FOUND` | 候选业务错误：profileId 不存在。 | `[REVIEW-DRAFT]`；采纳前确认是否需要 feature-specific errorCode。 |
| `NETWORK_AUTH_FAILED` | 候选业务错误：认证失败。 | `[REVIEW-DRAFT]`；也可通过 `failureReason=auth_failed` 表达。 |
| `NETWORK_CREDENTIAL_INVALID` | 候选业务错误：凭据格式、一次性导出值或 token 无效。 | `[REVIEW-DRAFT]`。 |

## 9. Legacy 映射

Legacy 映射是迁移证据，不是 runtime 合同。

| legacy 项 | 候选映射 | 状态 | 说明 |
|---|---|---|---|
| AXDP `CommonSetTailWiFiSSID` | `network.setWifiConfig` | `[REVIEW-DRAFT]` | 字段线索显示 payload 是 raw SSID bytes，只能映射 `profile.ssid`；密码/安全类型需由其他来源补齐。 |
| AXDP `CommonGetTailWiFiSSID` | `network.getWifiConfig` | `[REVIEW-DRAFT]` | 旧接收 case 为空，无完整 profile；新响应必须只返回脱敏摘要。 |
| Rooms `WifiConnect` | `network.connectWifi` | `[REVIEW-DRAFT]` | 可能映射 inline profile 或已保存 profile 连接。 |
| Rooms `GetWifiSignalStrength` | `network.getWifiState` | `[REVIEW-ASK]` | 可映射 RSSI，但不等同完整状态。 |
| VM33 `Config.Set:Wifi` / `Config.MultiSet:Wifi` | `network.setWifiConfig` | `[REVIEW-DRAFT]` | 可映射到 profile config；多 profile、持久化和字段路径仍需确认。 |
| VM33 `Config.Get:Wifi` / `Config.MultiGet:Wifi` | `network.getWifiConfig` | `[REVIEW-DRAFT]` | 敏感字段不得明文返回。 |
| VM33 `Wifi.ScanWifi` | `network.scanWifi` | `[REVIEW-ASK]` | 需确认同步/异步扫描结果。 |
| VM33 `Wifi.ConnectWifi` / `Wifi.ConnectWif` | `network.connectWifi` | `[REVIEW-DRAFT]` | 字段线索：`SSID` -> `profile.ssid`，`key` -> `profile.credential.value`。 |
| VM33 `Config.Subscribe:Wifi` | `network.wifiConfigChanged` | `[REVIEW-ASK]` | 需确认事件 payload。 |

## 10. 测试要点

| 类型 | 要点 |
|---|---|
| happy path | NT10 支持保存 profile，`source=pairing` 写入后默认连接。 |
| event path | profile 保存触发 `network.wifiConfigChanged`；连接过程触发 `network.wifiStateChanged`。 |
| boundary case | `connectAfterSave` 省略但 `source=pairing` 时默认 true；响应不回显 credential。 |
| error case | profile 不存在、凭据非法、认证失败、目标 AP 未发现、扫描/连接 busy。 |
| compatibility | 旧 `TailWiFiSSID` 只可映射 SSID，不能隐含完整 credential。 |
| pairing path | Wi-Fi `connected` 后，IP ready 由 `network.ip` 决定是否继续验收。 |

## 11. 待确认问题

| 问题 | 影响 | 当前建议 | 状态 |
|---|---|---|---|
| NT10 写入 Wi-Fi 后是否立即连接？ | pairing / conformance | 是；`source=pairing` 时 `connectAfterSave` 默认 `true`。 | decided |
| pairing profile 是否默认 `persist=true`？ | product behavior / schema | 当前建议默认持久化并设为默认 profile，但需产品确认。 | open |
| `network.connectWifi` 是否仍需保留？ | method surface | 保留为手动重试或非默认连接动作。 | decided |
| Wi-Fi `connected` 是否包含 IP ready？ | event boundary | 不包含；IP ready 由 `network.ip` 查询或事件确认。 | open |
| 是否采纳 `forgetWifi` / `resetWifiConfig`？ | registry | 当前不进入主路径，可作为 optional future。 | open |
