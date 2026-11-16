---
status: generated
contract: true
generated: true
domain: network
feature: network.ip
registry: ../../../../contract/registry/domains/network/domain.yaml
lastReviewed: 2026-06-15
---

# network.ip

## 0. 速读结论

| 项目 | 内容 |
|---|---|
| 这个能力做什么 | 按设备返回的 `interfaceId` 查询或设置 IPv4/IPv6 DHCP/static 地址、网关、DNS，并报告有效地址变化。 |
| 当前状态 | generated；已写入 `../../../../contract/registry/domains/network/domain.yaml`，并已刷新到 `contract/protocol/axtp.protocol.yaml` 与 `contract/generated/**`。 |
| 是否可直接实现 | 是，但实现合同以 `contract/protocol/axtp.protocol.yaml` / `contract/generated/**` 为准；本文保留的 open review markers 不属于已生成合同。 |
| 主要交互 | RPC + EVENT |
| 是否使用 STREAM | 否 |
| Registry readiness | ready；P0 / confirmed subset 已写入 registry source 并生成。 |
| Conformance | needed |
| 主要未决问题 | IP ready 是否作为配对验收、AP DHCP Server 地址池归属、IPv6 是否进入 MVP。 |

## 1. 功能说明

`network.ip` 描述指定网络接口上的地址配置和有效地址变化。Cast RX/TX 配对中，NT10 Wi-Fi 关联成功后，产品如需要 IP ready，可继续通过 `network.getIpConfig` 或 `network.ipConfigChanged` 确认有效地址。

本 feature 不表达 Wi-Fi 认证/关联、不表达 AP running，也不表达接口基础链路变化；这些分别由 `network.wifi`、`network.ap` 和 `network.interface` 负责。NA20 AP 本端 IP 可以通过 AP 接口的 `interfaceId` 查询；AP DHCP Server 地址池是否也放在 `network.ap` 仍待确认。

`network.ip` 的 P0 / confirmed subset 已进入 generated 合同；正式 methodId、eventId、errorCode、fieldId 以 `contract/registry/**`、`contract/protocol/axtp.protocol.yaml` 和 `contract/generated/**` 为准。本文保留的 review 标记仅用于后续修订。

## 2. 能力边界

| 类型 | 内容 |
|---|---|
| 包含 | 查询指定接口、指定地址族的当前 IP 配置和有效地址。 |
| 包含 | 设置 DHCP/static/disabled 模式、静态地址、前缀长度、默认网关和 DNS。 |
| 包含 | 地址配置或有效地址变化事件。 |
| 包含 | 配对后可选 IP ready 验收。 |
| 不包含 | 接口枚举、MAC、基础链路；这些属于 `network.interface`。 |
| 不包含 | Wi-Fi profile、扫描、认证、关联、断开；这些属于 `network.wifi`。 |
| 不包含 | AP SSID、安全、凭据导出、启停、客户端列表；这些属于 `network.ap`。 |
| 不包含 | 静态路由、DHCP lease 详情、独立 DNS 管理和 AP DHCP Server 地址池；这些保持 future / open。 |
| 数据面 | 本 feature 不定义 STREAM payload，所有操作均通过 RPC method/event 完成。 |

## 3. 方法 Methods

已生成 methodId、eventId、bitOffset 和 schema fieldId 以 contract/registry/generated 为准；本文不重新分配正式 ID，保留的 draft/review 标记仅作为后续修订输入。

### 3.0 方法速览

| Method | 调用类型 | 用途 | Params Schema | Result Schema | 是否触发事件 | 状态 |
|---|---|---|---|---|---|---|
| `network.getIpConfig` | query | 查询接口地址配置和当前有效地址。 | `NetworkGetIpConfigParams` | `NetworkIpConfig` | 否 | draft |
| `network.setIpConfig` | command | 设置 DHCP/static/disabled 地址配置。 | `NetworkSetIpConfigParams` | `NetworkSetIpConfigResult` | 是，`network.ipConfigChanged` | draft |

### 3.1 `network.getIpConfig`

**用途**：查询指定接口和地址族的 IP 配置及当前有效地址。

| 项 | 内容 |
|---|---|
| 调用类型 | query |
| Params Schema | `NetworkGetIpConfigParams` |
| Result Schema | `NetworkIpConfig` |
| 是否触发事件 | 否 |
| 幂等性 / 异步性 | 幂等；同步返回当前快照。 |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_ARGUMENT`, `NOT_FOUND`, `PERMISSION_DENIED`, `UNAVAILABLE` |

#### 3.1.1 请求参数 Params：`NetworkGetIpConfigParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `interfaceId` | string | yes | device-returned id | none | 目标接口，来自 `network.interface`。 |
| `family` | `NetworkIpFamily` | no | `ipv4`, `ipv6` | `ipv4` | 地址族。 |

#### 3.1.2 返回结果 Result：`NetworkIpConfig`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `interfaceId` | string | yes | device-returned id | none | 接口标识。 |
| `family` | `NetworkIpFamily` | yes | `ipv4`, `ipv6` | none | 地址族。 |
| `mode` | `NetworkIpMode` | yes | `dhcp`, `static`, `disabled` | none | 地址模式。 |
| `address` | string | no | IP address | omitted | 当前有效地址。 |
| `prefixLength` | uint8 | no | IPv4 `0..32`, IPv6 `0..128` | omitted | 前缀长度。 |
| `gateway` | string | no | IP address | omitted | 默认网关。 |
| `dnsServers` | string[] | no | IP address array | omitted | DNS 服务器。 |
| `source` | string enum | no | `static`, `dhcp`, `system_policy`, `runtime` | omitted | 当前有效配置来源。 |
| `effective` | boolean | yes | bool | none | 返回内容是否为当前已生效配置。 |

#### 3.1.3 d block 示例

request:

```json
{
  "id": 101,
  "method": "network.getIpConfig",
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
    "interfaceId": "wlan0",
    "mode": "dhcp",
    "address": "192.0.2.10",
    "gateway": "192.0.2.1",
    "dns": [
      "192.0.2.1"
    ]
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
| `NOT_FOUND` | 指定接口不存在或不支持 IP 配置。 | 使用 adopted numeric code `12`。 |
| `NOT_SUPPORTED` | 地址族或 feature 不支持。 | 使用 adopted numeric code `3`。 |

#### 3.1.6 错误 d block 示例

```json
{
  "id": 101,
  "status": {
    "ok": false,
    "code": 12,
    "msg": "Request failed.",
    "details": {
      "candidateError": "NOT_FOUND",
      "field": "interfaceId",
      "reason": "resource not found"
    }
  }
}
```

### 3.2 `network.setIpConfig`

**用途**：设置指定接口和地址族的 DHCP/static/disabled 配置。

| 项 | 内容 |
|---|---|
| 调用类型 | command |
| Params Schema | `NetworkSetIpConfigParams` |
| Result Schema | `NetworkSetIpConfigResult` |
| 是否触发事件 | 是，配置或有效地址实际变化后触发 `network.ipConfigChanged`。 |
| 幂等性 / 异步性 | 建议幂等；`apply=immediate` 可能导致当前网络会话重连。 |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_ARGUMENT`, `OUT_OF_RANGE`, `INVALID_STATE`, `BUSY`, `PERMISSION_DENIED`, `TIMEOUT` |

#### 3.2.1 请求参数 Params：`NetworkSetIpConfigParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `interfaceId` | string | yes | device-returned id | none | 目标接口。 |
| `family` | `NetworkIpFamily` | no | `ipv4`, `ipv6` | `ipv4` | 地址族。 |
| `mode` | `NetworkIpMode` | yes | `dhcp`, `static`, `disabled` | none | 地址模式。 |
| `address` | string | conditional | IP address | omitted | static 模式必填。 |
| `prefixLength` | uint8 | conditional | IPv4 `0..32`, IPv6 `0..128` | omitted | static 模式必填。 |
| `gateway` | string | no | IP address | omitted | 默认网关。 |
| `dnsServers` | string[] | no | IP address array | omitted | DNS 服务器。 |
| `apply` | `NetworkConfigApplyPolicy` | no | `immediate`, `on_reconnect`, `on_reboot` | `immediate` | 生效策略。 |

#### 3.2.2 返回结果 Result：`NetworkSetIpConfigResult`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `config` | `NetworkIpConfig` | yes | object | none | 写入后的配置摘要。 |
| `applied` | boolean | yes | bool | none | 是否已经生效。 |
| `effectiveAfter` | `NetworkConfigApplyPolicy` | yes | see enum | none | 实际生效时机。 |
| `requiresReconnect` | boolean | no | bool | omitted | 是否可能导致当前 AXTP 会话断开或需要客户端重连。 |
| `requiresReboot` | boolean | no | bool | omitted | 是否需要设备重启。 |

#### 3.2.3 d block 示例

request:

```json
{
  "id": 102,
  "method": "network.setIpConfig",
  "params": {
    "interfaceId": "wlan0",
    "mode": "auto",
    "address": "192.0.2.10",
    "prefixLength": 1
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
    "mode": "dhcp",
    "address": "192.0.2.10",
    "gateway": "192.0.2.1",
    "dns": [
      "192.0.2.1"
    ]
  }
}
```

#### 3.2.4 可能触发的事件

| Event | 触发条件 | Payload Schema | 客户端处理建议 |
|---|---|---|---|
| `network.ipConfigChanged` | 配置或有效地址实际变化。 | `NetworkIpConfigChangedEvent` | 更新 IP 缓存；配对验收可在有效地址出现时通过。 |

#### 3.2.5 错误

| 错误 | 场景 | 返回建议 |
|---|---|---|
| `INVALID_ARGUMENT` | static 模式缺少 `address` 或 `prefixLength`。 | 使用 adopted numeric code `10`，details 指出字段路径。 |
| `OUT_OF_RANGE` | `prefixLength` 超出地址族范围。 | 使用 adopted numeric code `11`。 |
| `INVALID_STATE` | 当前接口状态不允许立即应用配置。 | 使用 adopted numeric code `4`。 |

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
      "field": "interfaceId",
      "reason": "invalid field value"
    }
  }
}
```

## 4. 事件 Events

### 4.0 事件速览

| Event | 触发条件 | Payload Schema | 客户端处理建议 | 状态 |
|---|---|---|---|---|
| `network.ipConfigChanged` | IP 模式、地址、网关、DNS 或有效地址发生变化。 | `NetworkIpConfigChangedEvent` | 若产品要求 IP ready，则在有效地址出现后通过验收。 | draft |

### 4.1 `network.ipConfigChanged`

**触发条件**：

- `network.setIpConfig` 导致 IP 配置变化。
- DHCP 获取、续租、失效或地址冲突。
- 接口 link down 导致有效地址清空。
- 设备策略或恢复默认改变地址配置。

不因 Wi-Fi 连接成功或 AP 启停本身触发；只有有效地址或 IP 配置变化才触发。

#### Payload：`NetworkIpConfigChangedEvent`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `interfaceId` | string | yes | device-returned id | none | 发生变化的接口。 |
| `family` | `NetworkIpFamily` | yes | `ipv4`, `ipv6` | none | 地址族。 |
| `config` | `NetworkIpConfig` | yes | object | none | 变化后的 IP 配置。 |
| `previousConfig` | `NetworkIpConfig` | no | object | omitted | 变化前配置。 |
| `reason` | string enum | no | `user_request`, `dhcp_updated`, `lease_expired`, `link_lost`, `system_policy`, `apply_failed`, `unknown` | `unknown` | 变化原因。 |

#### d block 示例

```json
{
  "event": "network.ipConfigChanged",
  "intent": 1,
  "data": {
    "interfaceId": "wlan0",
    "family": "ipv4",
    "config": {
      "interfaceId": "wlan0",
      "mode": "dhcp",
      "address": "192.0.2.10",
      "gateway": "192.0.2.1",
      "dns": [
        "192.0.2.1"
      ]
    },
    "previousConfig": {
      "interfaceId": "wlan0",
      "mode": "dhcp",
      "address": "192.0.2.10",
      "gateway": "192.0.2.1",
      "dns": [
        "192.0.2.1"
      ]
    },
    "reason": "user_request"
  }
}
```


#### 客户端处理建议

| 场景 | 建议 |
|---|---|
| `config.effective=true` 且有 `address` | 可判定该接口地址 ready。 |
| Wi-Fi connected 后等待 IP | 订阅该事件或轮询 `network.getIpConfig`。 |
| event 丢失或重连 | 重连后主动调用 `network.getIpConfig` 校准。 |

## 5. Capability

Capability name: `network.ip`。

| 能力字段 | 类型 | 必填 | 取值范围 / 枚举 | 说明 |
|---|---|---:|---|---|
| `capability` | string | yes | fixed `network.ip` | capability 名称。 |
| `families` | `NetworkIpFamily[]` | yes | `ipv4`, `ipv6` | 支持的地址族。 |
| `modes` | `NetworkIpMode[]` | yes | `dhcp`, `static`, `disabled` | 支持模式。 |
| `supportsGateway` | boolean | no | bool | 是否支持默认网关字段。 |
| `supportsDnsServers` | boolean | no | bool | 是否支持 DNS 服务器字段。 |
| `applyPolicies` | `NetworkConfigApplyPolicy[]` | no | see enum | 支持的生效策略。 |
| `ipReadyObservable` | boolean | no | bool | 是否能通过查询或事件观察有效地址。 |

## 6. 字段 / Schemas

### 6.1 Schema 层级速览

```text
NetworkIpConfig
NetworkSetIpConfigResult
  config: NetworkIpConfig
NetworkIpConfigChangedEvent
  config: NetworkIpConfig
  previousConfig: NetworkIpConfig
```

`NetworkIpConfig` 同时用于查询结果、设置结果和事件 payload。Capability 只描述设备支持范围，不混入运行时地址。

### 6.2 请求与响应 Schemas

#### `NetworkGetIpConfigParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `interfaceId` | string | yes | device-returned id | none | 目标接口。 |
| `family` | `NetworkIpFamily` | no | `ipv4`, `ipv6` | `ipv4` | 地址族。 |

#### `NetworkSetIpConfigParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `interfaceId` | string | yes | device-returned id | none | 目标接口。 |
| `family` | `NetworkIpFamily` | no | `ipv4`, `ipv6` | `ipv4` | 地址族。 |
| `mode` | `NetworkIpMode` | yes | `dhcp`, `static`, `disabled` | none | 地址模式。 |
| `address` | string | conditional | IP address | omitted | static 模式必填。 |
| `prefixLength` | uint8 | conditional | IPv4 `0..32`, IPv6 `0..128` | omitted | static 模式必填。 |
| `gateway` | string | no | IP address | omitted | 默认网关。 |
| `dnsServers` | string[] | no | IP address array | omitted | DNS 服务器。 |
| `apply` | `NetworkConfigApplyPolicy` | no | see enum | `immediate` | 生效策略。 |

#### `NetworkSetIpConfigResult`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `config` | `NetworkIpConfig` | yes | object | none | 写入后的配置摘要。 |
| `applied` | boolean | yes | bool | none | 是否已生效。 |
| `effectiveAfter` | `NetworkConfigApplyPolicy` | yes | see enum | none | 生效时机。 |
| `requiresReconnect` | boolean | no | bool | omitted | 是否可能需要客户端重连。 |
| `requiresReboot` | boolean | no | bool | omitted | 是否需要设备重启。 |

### 6.3 Capability Schemas

#### `NetworkIpCapability`

字段同第 5 章 Capability 表；采纳时可作为 capability object。

### 6.4 Event Schemas

#### `NetworkIpConfigChangedEvent`

字段同第 4.1 节 payload 表；采纳时应作为独立 event schema。

### 6.5 State / Config / Object Schemas

#### `NetworkIpConfig`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `interfaceId` | string | yes | device-returned id | none | 接口标识。 |
| `family` | `NetworkIpFamily` | yes | `ipv4`, `ipv6` | none | 地址族。 |
| `mode` | `NetworkIpMode` | yes | `dhcp`, `static`, `disabled` | none | 地址模式。 |
| `address` | string | no | IP address | omitted | 当前有效地址。 |
| `prefixLength` | uint8 | no | IPv4 `0..32`, IPv6 `0..128` | omitted | 前缀长度。 |
| `gateway` | string | no | IP address | omitted | 默认网关。 |
| `dnsServers` | string[] | no | IP address array | omitted | DNS 服务器。 |
| `source` | string enum | no | `static`, `dhcp`, `system_policy`, `runtime` | omitted | 配置来源。 |
| `effective` | boolean | yes | bool | none | 是否为当前有效配置。 |

#### 枚举

| Type | 候选值 | 说明 |
|---|---|---|
| `NetworkIpFamily` | `ipv4`, `ipv6` | 地址族。 |
| `NetworkIpMode` | `dhcp`, `static`, `disabled` | 地址模式。 |
| `NetworkConfigApplyPolicy` | `immediate`, `on_reconnect`, `on_reboot` | 生效策略。 |

## 7. JSON 示例

示例只展示 RPC data block，不包裹外层 wire envelope。字段和 ID 在采纳前均为草案；IP 地址均使用占位符。

### 7.1 场景：配对后查询 NT10 STA IPv4

#### request

```json
{
  "id": 401,
  "method": "network.getIpConfig",
  "params": {
    "interfaceId": "wlan0",
    "family": "ipv4"
  }
}
```

#### response

```json
{
  "id": 401,
  "status": {
    "ok": true,
    "code": 0
  },
  "result": {
    "interfaceId": "wlan0",
    "family": "ipv4",
    "mode": "dhcp",
    "address": "192.0.2.10",
    "prefixLength": 24,
    "gateway": "192.0.2.1",
    "dnsServers": [
      "198.51.100.53"
    ],
    "source": "dhcp",
    "effective": true
  }
}
```

读法：当产品把 IP ready 作为验收条件时，`effective=true` 且 `address` 存在即可作为通过依据之一。

### 7.2 场景：切换接口为 DHCP

#### request

```json
{
  "id": 402,
  "method": "network.setIpConfig",
  "params": {
    "interfaceId": "wlan0",
    "family": "ipv4",
    "mode": "dhcp",
    "apply": "immediate"
  }
}
```

#### response

```json
{
  "id": 402,
  "status": {
    "ok": true,
    "code": 0
  },
  "result": {
    "config": {
      "interfaceId": "wlan0",
      "family": "ipv4",
      "mode": "dhcp",
      "address": "192.0.2.20",
      "prefixLength": 24,
      "gateway": "192.0.2.1",
      "dnsServers": [
        "198.51.100.53"
      ],
      "source": "dhcp",
      "effective": true
    },
    "applied": true,
    "effectiveAfter": "immediate",
    "requiresReconnect": true,
    "requiresReboot": false
  }
}
```

读法：`requiresReconnect=true` 提醒 Host 当前控制链路可能受影响。

### 7.3 场景：IP 配置变化事件

```json
{
  "event": "network.ipConfigChanged",
  "intent": 2,
  "data": {
    "interfaceId": "wlan0",
    "family": "ipv4",
    "config": {
      "interfaceId": "wlan0",
      "family": "ipv4",
      "mode": "dhcp",
      "address": "192.0.2.20",
      "prefixLength": 24,
      "gateway": "192.0.2.1",
      "dnsServers": [
        "198.51.100.53"
      ],
      "source": "dhcp",
      "effective": true
    },
    "reason": "dhcp_updated"
  }
}
```

读法：这是地址变化事件，不表示 Wi-Fi 认证状态变化；Wi-Fi 仍由 `network.wifiStateChanged` 表达。

### 7.4 场景：静态地址缺少前缀失败响应

```json
{
  "id": 403,
  "status": {
    "ok": false,
    "code": 10,
    "msg": "Static IP address requires prefixLength.",
    "details": {
      "candidateError": "NETWORK_IP_CONFIG_INVALID",
      "field": "prefixLength"
    }
  }
}
```

读法：`status.code=10` 对应 adopted `INVALID_ARGUMENT`。候选业务错误名只作为草案 details。

## 8. 错误

| 错误 | 适用场景 | 说明 |
|---|---|---|
| `NOT_SUPPORTED` | 设备或接口不支持 IP 配置、地址族或模式。 | 使用 adopted numeric code `3`。 |
| `INVALID_ARGUMENT` | static 配置缺少必填字段或参数组合非法。 | 使用 adopted numeric code `10`。 |
| `OUT_OF_RANGE` | prefixLength 或地址范围非法。 | 使用 adopted numeric code `11`。 |
| `NOT_FOUND` | 指定 `interfaceId` 不存在。 | 使用 adopted numeric code `12`。 |
| `BUSY` | IP 配置正在应用。 | 使用 adopted numeric code `5`。 |
| `NETWORK_IP_CONFIG_INVALID` | 候选业务错误：IP 配置组合无效。 | `[REVIEW-DRAFT]`；采纳前确认是否需要 feature-specific errorCode。 |
| `NETWORK_DHCP_FAILED` | 候选业务错误：DHCP 获取或续租失败。 | `[REVIEW-DRAFT]`；也可通过事件 reason 表达。 |
| `NETWORK_IP_ADDRESS_CONFLICT` | 候选业务错误：地址冲突。 | `[REVIEW-DRAFT]`。 |

## 9. Legacy 映射

Legacy 映射是迁移证据，不是 runtime 合同。

| legacy 项 | 候选映射 | 状态 | 说明 |
|---|---|---|---|
| AXDP `CommonGetIPConfig` | `network.getIpConfig` | open review | 需确认旧 payload 是否聚合 DHCP、IP、netmask、gateway。 |
| AXDP `CommonSetDHCPState` / `CommonGetDHCPState` | `network.setIpConfig` / `network.getIpConfig` | open review | DHCP state 映射到 `mode=dhcp/static/disabled` 的规则待确认。 |
| AXDP `CommonSetIPAddress` / `CommonGetIPAddress` | `network.setIpConfig` / `network.getIpConfig` | open review | 映射到 `address`。 |
| AXDP `CommonSetNetMask` / `CommonGetNetMask` | `network.setIpConfig` / `network.getIpConfig` | open review | 旧 netmask 需转换为 `prefixLength`。 |
| AXDP `CommonSetGateway` / `CommonGetGateway` | `network.setIpConfig` / `network.getIpConfig` | open review | 映射到 `gateway`。 |
| Signage `GetNetworkInfo` | `network.getIpConfig` | open review | 需确认是否还包含接口、Wi-Fi 或服务端点字段。 |
| VM33 `NetWork.SetNetwork` / `NetWork.SetIP` | `network.setIpConfig` | open review | 需确认字段路径、静态/DHCP 切换和重启要求。 |
| VM33 `NetWork.GetNetwork` / `NetWork.GetNetworkStatus` | `network.getIpConfig` | open review | 需确认状态字段。 |

## 10. 测试要点

| 类型 | 要点 |
|---|---|
| happy path | `network.getIpConfig` 返回有效 DHCP 地址。 |
| config path | `network.setIpConfig(mode=dhcp/static/disabled)` 校验必填字段和生效策略。 |
| event path | DHCP 地址变化触发 `network.ipConfigChanged`；Wi-Fi connected 不直接触发 IP 事件。 |
| boundary case | IPv4/IPv6 family、省略 family 默认 IPv4、static 缺少 prefixLength。 |
| error case | 接口不存在、地址族不支持、静态地址冲突、配置应用 busy。 |
| pairing path | 若产品要求 IP ready，NT10 connected 后等待 `effective=true` 且 `address` 存在。 |
| compatibility | legacy netmask 转 `prefixLength`，gateway/DNS 字段映射需确认。 |

## 11. 待确认问题

| 问题 | 影响 | 当前建议 | 状态 |
|---|---|---|---|
| 配对成功是否必须确认 IP ready？ | product / conformance | 当前作为 optional 验收；默认 Wi-Fi connected 可先算基础成功。 | `[REVIEW-ASK]` |
| AP 本端地址是否由 `network.setIpConfig` 写入？ | schema boundary | 查询 AP 本端地址用 `network.ip`；写入入口和 AP DHCP Server 地址池需确认。 | `[REVIEW-ASK]` |
| IPv6 是否进入 MVP？ | registry / conformance | 保留 `family=ipv6` 形状，MVP 可先要求 IPv4。 | `[REVIEW-ASK]` |
| DNS 是否需要独立方法？ | schema | 当前随 IP 配置读写；复杂 DNS 策略 future。 | `[REVIEW-ASK]` |
| 修改当前控制链路 IP 时如何避免断连？ | runtime / conformance | 使用 `effectiveAfter` 和 `requiresReconnect` 表达风险。 | `[REVIEW-ASK]` |
