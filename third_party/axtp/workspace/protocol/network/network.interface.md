---
status: generated
contract: true
generated: true
domain: network
feature: network.interface
registry: ../../../../contract/registry/domains/network/domain.yaml
lastReviewed: 2026-06-15
---

# network.interface

## 0. 速读结论

| 项目 | 内容 |
|---|---|
| 这个能力做什么 | 枚举设备网络接口，并提供 `network.ap`、`network.wifi`、`network.ip` 引用所需的 `interfaceId`、`roles` 和默认接口信息。 |
| 当前状态 | generated；已写入 `../../../../contract/registry/domains/network/domain.yaml`，并已刷新到 `contract/protocol/axtp.protocol.yaml` 与 `contract/generated/**`。 |
| 是否可直接实现 | 是，但实现合同以 `contract/protocol/axtp.protocol.yaml` / `contract/generated/**` 为准；本文保留的 open review markers 不属于已生成合同。 |
| 主要交互 | RPC + EVENT |
| 是否使用 STREAM | 否 |
| Registry readiness | ready；P0 / confirmed subset 已写入 registry source 并生成。 |
| Conformance | needed |
| 主要未决问题 | `interfaceId` 跨重启稳定性、MAC 脱敏规则、是否采纳受限接口配置方法。 |

## 1. 功能说明

`network.interface` 是 network 域的前置发现层。Host 先查询设备有哪些网络接口，再用设备返回的 `interfaceId` 调用 AP、Wi-Fi STA 或 IP 配置方法。

Cast RX/TX 配对 flow 已确认：Host 不硬编码内部网卡名；`network.getInterfaces` 返回的 `roles` 和 `defaults` 足以选择 NA20 的 AP 接口和 NT10 的 STA 接口。本文不定义 Wi-Fi 认证、AP 运行状态或 IP 地址变化，这些分别归 `network.wifi`、`network.ap` 和 `network.ip`。

`network.interface` 的 P0 / confirmed subset 已进入 generated 合同；正式 methodId、eventId、errorCode、fieldId 以 `contract/registry/**`、`contract/protocol/axtp.protocol.yaml` 和 `contract/generated/**` 为准。本文保留的 review 标记仅用于后续修订。

## 2. 能力边界

| 类型 | 内容 |
|---|---|
| 包含 | 枚举接口列表，返回 `interfaceId`、接口类型、角色、默认接口和基础状态。 |
| 包含 | 查询单个接口的基础详情，例如 MAC 摘要、MTU、显示名和基础链路状态。 |
| 包含 | 接口出现/消失、启用/禁用、基础链路 up/down 的事件。 |
| 不包含 | Wi-Fi STA profile、扫描、认证、关联和断开；这些属于 `network.wifi`。 |
| 不包含 | AP SSID、安全、凭据导出、启停和客户端列表；这些属于 `network.ap`。 |
| 不包含 | DHCP/static 地址、网关、DNS 和 IP ready；这些属于 `network.ip`。 |
| 不包含 | MAC 写入、MTU 设置等管理员/产测配置；本草案仅保留为 future / optional。 |
| 数据面 | 本 feature 不定义 STREAM payload，所有操作均通过 RPC method/event 完成。 |

## 3. 方法 Methods

已生成 methodId、eventId、bitOffset 和 schema fieldId 以 contract/registry/generated 为准；本文不重新分配正式 ID，保留的 draft/review 标记仅作为后续修订输入。

### 3.0 方法速览

| Method | 调用类型 | 用途 | Params Schema | Result Schema | 是否触发事件 | 状态 |
|---|---|---|---|---|---|---|
| `network.getInterfaces` | query | 列出网络接口并返回默认接口绑定。 | `NetworkGetInterfacesParams` | `NetworkInterfaces` | 否 | draft |
| `network.getInterfaceInfo` | query | 查询单个接口基础详情。 | `NetworkGetInterfaceInfoParams` | `NetworkInterfaceInfo` | 否 | draft |

### 3.1 `network.getInterfaces`

**用途**：列出设备当前可见的网络接口，供 Host 选择后续 AP/STA/IP 调用目标。

| 项 | 内容 |
|---|---|
| 调用类型 | query |
| Params Schema | `NetworkGetInterfacesParams` |
| Result Schema | `NetworkInterfaces` |
| 是否触发事件 | 否 |
| 幂等性 / 异步性 | 幂等；同步返回当前快照。 |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_ARGUMENT`, `PERMISSION_DENIED`, `UNAVAILABLE` |

#### 3.1.1 请求参数 Params：`NetworkGetInterfacesParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `roleFilter` | `NetworkInterfaceRole[]` | no | `uplink`, `sta`, `ap`, `control`, `management` | omitted | 只返回指定角色接口；配对时 Host 可用 `ap` 查询 NA20 AP 接口，用 `sta` 查询 NT10 STA 接口。 |
| `typeFilter` | `NetworkInterfaceType[]` | no | `ethernet`, `wifi`, `usb_network`, `cellular`, `bridge`, `virtual`, `unknown` | omitted | 只返回指定接口类型。 |

#### 3.1.2 返回结果 Result：`NetworkInterfaces`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `interfaces` | `NetworkInterfaceSummary[]` | yes | array | none | 接口摘要列表。 |
| `defaults` | `NetworkDefaultInterfaceIds` | no | object | omitted | 默认接口绑定；同一 role 多接口时 Host 优先使用对应默认接口。 |

#### 3.1.3 d block 示例

request:

```json
{
  "id": 101,
  "method": "network.getInterfaces",
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
  "result": {
    "interfaces": [
      {
        "interfaceId": "wlan0",
        "type": 1,
        "state": 1
      }
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
| `INVALID_ARGUMENT` | `roleFilter` 或 `typeFilter` 包含非法枚举。 | 使用 adopted numeric code `10`，并在 details 中指出字段路径。 |
| `PERMISSION_DENIED` | 当前调用方无权读取接口信息。 | 使用 adopted numeric code `9`。 |

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
      "field": "roleFilter",
      "reason": "invalid field value"
    }
  }
}
```

### 3.2 `network.getInterfaceInfo`

**用途**：查询单个接口基础详情，用于确认 Host 已选择的 `interfaceId` 仍然可用。

| 项 | 内容 |
|---|---|
| 调用类型 | query |
| Params Schema | `NetworkGetInterfaceInfoParams` |
| Result Schema | `NetworkInterfaceInfo` |
| 是否触发事件 | 否 |
| 幂等性 / 异步性 | 幂等；同步返回当前快照。 |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_ARGUMENT`, `NOT_FOUND`, `PERMISSION_DENIED`, `UNAVAILABLE` |

#### 3.2.1 请求参数 Params：`NetworkGetInterfaceInfoParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `interfaceId` | string | yes | device-returned id | none | 目标接口标识，必须来自 `network.getInterfaces`，不要求等同 OS 内部网卡名。 |

#### 3.2.2 返回结果 Result：`NetworkInterfaceInfo`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `interfaceId` | string | yes | device-returned id | none | 接口标识。 |
| `type` | `NetworkInterfaceType` | yes | see enum | none | 接口类型。 |
| `roles` | `NetworkInterfaceRole[]` | no | see enum | omitted | 接口角色；配对时用于确认 AP/STA 目标。 |
| `state` | `NetworkInterfaceState` | yes | object | none | 基础接口状态。 |
| `macAddress` | string | no | redacted or plain by policy | omitted | 当前 MAC；可按隐私策略省略或脱敏。 |
| `mtu` | uint16 | no | `0..65535` | omitted | 当前 MTU。 |
| `displayName` | string | no | UI label | omitted | 面向 UI 的显示名。 |

#### 3.2.3 d block 示例

request:

```json
{
  "id": 102,
  "method": "network.getInterfaceInfo",
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
    "type": "wifi",
    "macAddress": "02:00:00:00:00:01",
    "state": "up"
  }
}
```

#### 3.2.4 可能触发的事件

| Event | 触发条件 | Payload Schema | 客户端处理建议 |
|---|---|---|---|
| 无 | query method 不应因查询触发状态变化事件。 | none | 无需处理。 |

#### 3.2.5 错误

| 错误 | 场景 | 返回建议 |
|---|---|---|
| `NOT_FOUND` | 指定 `interfaceId` 不存在或当前不可见。 | 使用 adopted numeric code `12`；可在 details 标注候选 `NETWORK_INTERFACE_NOT_FOUND`。 |
| `PERMISSION_DENIED` | 当前调用方无权读取 MAC 或接口详情。 | 返回可诊断原因，不泄露受限字段。 |

#### 3.2.6 错误 d block 示例

```json
{
  "id": 102,
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

## 4. 事件 Events

### 4.0 事件速览

| Event | 触发条件 | Payload Schema | 客户端处理建议 | 状态 |
|---|---|---|---|---|
| `network.interfaceStateChanged` | 接口新增/移除、启用/禁用、基础链路 up/down。 | `NetworkInterfaceStateChangedEvent` | 更新接口缓存；目标接口消失时重新发现或中止当前流程。 | draft |

### 4.1 `network.interfaceStateChanged`

**触发条件**：

- 接口出现或消失。
- 接口管理状态变化，例如 enabled / disabled / unavailable。
- 基础链路状态变化，例如 link up / down。

不因 Wi-Fi 扫描/认证/关联、AP start/stop 或 IP 地址变化而重复触发；这些状态由各自 feature 的事件表达。

#### Payload：`NetworkInterfaceStateChangedEvent`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `interfaceId` | string | yes | device-returned id | none | 发生变化的接口。 |
| `state` | `NetworkInterfaceState` | yes | object | none | 变化后的基础接口状态。 |
| `previousState` | `NetworkInterfaceState` | no | object | omitted | 变化前状态。 |
| `roles` | `NetworkInterfaceRole[]` | no | see enum | omitted | 接口角色摘要，便于 Host 判断目标接口是否仍是 AP/STA。 |
| `reason` | string enum | no | `user_request`, `system_policy`, `link_lost`, `link_restored`, `interface_added`, `interface_removed`, `error`, `unknown` | `unknown` | 变化原因。 |

#### d block 示例

```json
{
  "event": "network.interfaceStateChanged",
  "intent": 1,
  "data": {
    "interfaceId": "wlan0",
    "state": {
      "admin": "up",
      "link": "up"
    },
    "previousState": {
      "admin": "up",
      "link": "up"
    },
    "reason": "user_request"
  }
}
```


#### 客户端处理建议

| 场景 | 建议 |
|---|---|
| 目标接口消失 | 停止当前 AP/Wi-Fi/IP 操作，重新调用 `network.getInterfaces`。 |
| 基础链路 down | UI 或 Host 状态可提示链路异常；不要推断 Wi-Fi 认证或 IP 失败原因。 |
| event 丢失或重连 | 重连后主动调用 `network.getInterfaces` 校准。 |

## 5. Capability

Capability name: `network.interface`。

| 能力字段 | 类型 | 必填 | 取值范围 / 枚举 | 说明 |
|---|---|---:|---|---|
| `capability` | string | yes | fixed `network.interface` | capability 名称。 |
| `roles` | `NetworkInterfaceRole[]` | no | see enum | 设备可能暴露的接口角色集合。 |
| `types` | `NetworkInterfaceType[]` | no | see enum | 设备可能暴露的接口类型集合。 |
| `supportsDefaults` | boolean | no | `true`, `false` | 是否在接口列表中返回默认接口绑定。 |
| `macAddressPolicy` | string enum | no | `plain`, `redacted`, `omitted` | MAC 返回策略。 |
| `supportsInterfaceConfig` | boolean | no | `true`, `false` | 是否存在受限接口配置能力；本草案不采纳对应 method。 |

## 6. 字段 / Schemas

### 6.1 Schema 层级速览

本文采用复杂 feature 的集中 schema 展开模式。Method / event 章节已经列出关键字段，本章集中定义对象关系。

```text
NetworkInterfaces
  interfaces: NetworkInterfaceSummary[]
  defaults: NetworkDefaultInterfaceIds
NetworkInterfaceInfo
  state: NetworkInterfaceState
NetworkInterfaceStateChangedEvent
  state: NetworkInterfaceState
```

### 6.2 请求与响应 Schemas

#### `NetworkGetInterfacesParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `roleFilter` | `NetworkInterfaceRole[]` | no | see 6.5 | omitted | 按角色过滤。 |
| `typeFilter` | `NetworkInterfaceType[]` | no | see 6.5 | omitted | 按类型过滤。 |

#### `NetworkGetInterfaceInfoParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `interfaceId` | string | yes | device-returned id | none | 目标接口。 |

#### `NetworkInterfaces`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `interfaces` | `NetworkInterfaceSummary[]` | yes | array | none | 接口摘要列表。 |
| `defaults` | `NetworkDefaultInterfaceIds` | no | object | omitted | 默认接口绑定。 |

#### `NetworkDefaultInterfaceIds`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `uplink` | string | no | interface id | omitted | 默认上联网口。 |
| `wifiSta` | string | no | interface id | omitted | 默认 Wi-Fi STA 接口，用于 `network.wifi` 和 STA 侧 `network.ip`。 |
| `ap` | string | no | interface id | omitted | 默认 AP/SoftAP 接口，用于 `network.ap` 和 AP 本端 `network.ip`。 |

### 6.3 Capability Schemas

#### `NetworkInterfaceCapability`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `capability` | string | yes | fixed `network.interface` | none | capability 名称。 |
| `roles` | `NetworkInterfaceRole[]` | no | see 6.5 | omitted | 支持角色。 |
| `types` | `NetworkInterfaceType[]` | no | see 6.5 | omitted | 支持类型。 |
| `supportsDefaults` | boolean | no | bool | `true` | 是否返回默认接口绑定。 |

### 6.4 Event Schemas

#### `NetworkInterfaceStateChangedEvent`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `interfaceId` | string | yes | device-returned id | none | 发生变化的接口。 |
| `state` | `NetworkInterfaceState` | yes | object | none | 变化后状态。 |
| `previousState` | `NetworkInterfaceState` | no | object | omitted | 变化前状态。 |
| `roles` | `NetworkInterfaceRole[]` | no | see 6.5 | omitted | 角色摘要。 |
| `reason` | string enum | no | see 4.1 | `unknown` | 变化原因。 |

### 6.5 State / Config / Object Schemas

#### `NetworkInterfaceSummary`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `interfaceId` | string | yes | device-returned id | none | 其他 network feature 引用的稳定标识。 |
| `type` | `NetworkInterfaceType` | yes | see enum | none | 接口类型。 |
| `roles` | `NetworkInterfaceRole[]` | no | see enum | omitted | 接口角色。 |
| `state` | `NetworkInterfaceState` | yes | object | none | 基础接口状态。 |
| `macAddress` | string | no | policy-dependent | omitted | 当前 MAC，可脱敏或省略。 |

#### `NetworkInterfaceInfo`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `interfaceId` | string | yes | device-returned id | none | 接口标识。 |
| `type` | `NetworkInterfaceType` | yes | see enum | none | 接口类型。 |
| `roles` | `NetworkInterfaceRole[]` | no | see enum | omitted | 接口角色。 |
| `state` | `NetworkInterfaceState` | yes | object | none | 基础接口状态。 |
| `macAddress` | string | no | policy-dependent | omitted | 当前 MAC。 |
| `mtu` | uint16 | no | `0..65535` | omitted | 当前 MTU。 |
| `displayName` | string | no | UI label | omitted | 显示名。 |

#### `NetworkInterfaceState`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `admin` | string enum | yes | `enabled`, `disabled`, `unavailable`, `unsupported` | none | 管理状态。 |
| `link` | string enum | yes | `up`, `down`, `unknown` | none | 基础链路状态，不表达 Wi-Fi/AP/IP 业务状态。 |

#### 枚举

| Type | 候选值 | 说明 |
|---|---|---|
| `NetworkInterfaceType` | `ethernet`, `wifi`, `usb_network`, `cellular`, `bridge`, `virtual`, `unknown` | 接口类型。 |
| `NetworkInterfaceRole` | `uplink`, `sta`, `ap`, `control`, `management` | 接口角色。 |

## 7. JSON 示例

示例只展示 RPC data block，不包裹外层 wire envelope。字段和 ID 在采纳前均为草案。

### 7.1 场景：配对前查询 AP/STA 接口

#### request

```json
{
  "id": 301,
  "method": "network.getInterfaces",
  "params": {
    "roleFilter": [
      "ap",
      "sta"
    ]
  }
}
```

#### response

```json
{
  "id": 301,
  "status": {
    "ok": true,
    "code": 0
  },
  "result": {
    "interfaces": [
      {
        "interfaceId": "wlan0",
        "type": "wifi",
        "roles": [
          "sta"
        ],
        "state": {
          "admin": "enabled",
          "link": "up"
        },
        "macAddress": "02:00:00:00:10:01"
      },
      {
        "interfaceId": "ap0",
        "type": "wifi",
        "roles": [
          "ap"
        ],
        "state": {
          "admin": "enabled",
          "link": "up"
        },
        "macAddress": "02:00:00:00:20:01"
      }
    ],
    "defaults": {
      "wifiSta": "wlan0",
      "ap": "ap0"
    }
  }
}
```

读法：Host 使用设备返回的 `interfaceId` 继续调用 AP/Wi-Fi/IP 方法。`interfaceId` 不要求等同设备内部 OS 网卡名。

### 7.2 场景：接口基础链路变化事件

```json
{
  "event": "network.interfaceStateChanged",
  "intent": 2,
  "data": {
    "interfaceId": "wlan0",
    "previousState": {
      "admin": "enabled",
      "link": "up"
    },
    "state": {
      "admin": "enabled",
      "link": "down"
    },
    "roles": [
      "sta"
    ],
    "reason": "link_lost"
  }
}
```

读法：这是基础链路变化，不表示 Wi-Fi 认证失败或 IP 地址失效；客户端需要时分别查询 `network.wifi` 或 `network.ip`。

### 7.3 场景：接口不存在失败响应

```json
{
  "id": 302,
  "status": {
    "ok": false,
    "code": 12,
    "msg": "Network interface was not found.",
    "details": {
      "candidateError": "NETWORK_INTERFACE_NOT_FOUND",
      "interfaceId": "missing0"
    }
  }
}
```

读法：`status.code=12` 对应 adopted `NOT_FOUND`。候选业务错误名只放在 details 中，正式编号待采纳。

## 8. 错误

| 错误 | 适用场景 | 说明 |
|---|---|---|
| `NOT_SUPPORTED` | 设备不支持 `network.interface`。 | 使用 adopted numeric code `3`。 |
| `INVALID_ARGUMENT` | 过滤条件或 `interfaceId` 参数非法。 | 使用 adopted numeric code `10`。 |
| `NOT_FOUND` | 指定接口不存在。 | 使用 adopted numeric code `12`。 |
| `PERMISSION_DENIED` | 调用方无权读取接口详情或 MAC。 | 使用 adopted numeric code `9`。 |
| `NETWORK_INTERFACE_NOT_FOUND` | 候选业务错误：接口不存在或当前不可见。 | `[REVIEW-DRAFT]`；采纳前确认是否需要 feature-specific errorCode。 |

## 9. Legacy 映射

Legacy 映射是迁移证据，不是 runtime 合同。

| legacy 项 | 候选映射 | 状态 | 说明 |
|---|---|---|---|
| AXDP `CommonGetMacAddress` | `network.getInterfaceInfo` | `[REVIEW-ASK]` | 需确认返回当前 MAC、出厂 MAC 还是默认网口 MAC。 |
| AXDP `CommonSetMacAddress` | future `network.setInterfaceConfig` | `[REVIEW-ASK]` | MAC 写入不进入默认 MVP；需确认权限和重启。 |
| Rooms `GetMacAddr` | `network.getInterfaceInfo` | `[REVIEW-ASK]` | 需确认是否映射到默认 `interfaceId`。 |
| Rooms `SetMacAddr` | future `network.setInterfaceConfig` | `[REVIEW-ASK]` | 需确认权限、持久化和状态码。 |

## 10. 测试要点

| 类型 | 要点 |
|---|---|
| happy path | `network.getInterfaces` 返回 AP/STA 接口和默认接口绑定。 |
| pairing path | Host 只使用设备返回的 `interfaceId`，不写死内部网卡名。 |
| event path | 接口 up/down 触发 `network.interfaceStateChanged`；Wi-Fi/AP/IP 状态变化不重复触发。 |
| boundary case | 同一 role 多接口、无默认接口、optional MAC 省略或脱敏。 |
| error case | `interfaceId` 不存在、非法 role filter、权限不足。 |
| compatibility | 旧 MAC 查询可迁移到 `network.getInterfaceInfo`，MAC 写入暂缓。 |

## 11. 待确认问题

| 问题 | 影响 | 当前建议 | 状态 |
|---|---|---|---|
| `interfaceId` 是否必须跨重启稳定？ | schema / conformance | 建议同一固件和硬件拓扑下稳定；若无法保证，Host 每次会话重新查询。 | open |
| `interfaceId` 是否直接暴露 OS 名称？ | security / portability | 建议设备返回逻辑别名，不要求暴露 OS 内部名称。 | open |
| MAC 地址是否可普通查询返回？ | security / privacy | 建议允许设备按策略返回 plain、redacted 或 omitted。 | open |
| 是否采纳 `network.setInterfaceConfig`？ | registry / permissions | 当前不进入 MVP；如需 MAC/MTU 写入，另行确认权限和防断连策略。 | open |
| `roles` 和默认接口是否足以选择 AP/STA？ | cast pairing | flow 已确认足够；本草案按 `[REVIEW-OK]` 处理。 | decided |
