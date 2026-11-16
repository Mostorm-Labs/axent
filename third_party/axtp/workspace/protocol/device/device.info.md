---
status: generated
contract: true
generated: true
domain: device
feature: device.info
registry: ../../../../contract/registry/domains/device/domain.yaml
lastReviewed: 2026-06-15
---

# device.info

## 0. 速读结论

| 项目 | 内容 |
|---|---|
| 这个能力做什么 | 只读获取当前 AXTP endpoint 代表的主设备身份、产品、硬件、OS、软件、AXTP runtime 和轻量 capability 建模摘要。 |
| 当前状态 | generated；已写入 `../../../../contract/registry/domains/device/domain.yaml`，并已刷新到 `contract/protocol/axtp.protocol.yaml` 与 `contract/generated/**`。 |
| 是否可直接实现 | 是，但实现合同以 `contract/protocol/axtp.protocol.yaml` / `contract/generated/**` 为准；本文保留的 open review markers 不属于已生成合同。 |
| 主要交互 | RPC |
| 是否使用 STREAM | 否 |
| Registry readiness | ready；P0 / confirmed subset 已写入 registry source 并生成。 |
| Conformance | needed |
| 主要未决问题 | 首批 enum、legacy 敏感字段和 capability profile 来源。 |

## 1. 功能说明

`device.info` 用于只读获取当前主设备信息，回答“这台设备是谁、是什么产品、运行什么系统和 AXTP runtime”。它也可以返回轻量 capability summary，帮助 Host 或工具理解设备大体按哪些 `domain.feature` 建模。

Cast RX/TX 配对 flow 中，NA20 / NT10 的识别由 Host 基于 USB descriptor、本地设备匹配规则或人工选择完成。`device.info` 不提供用于区分配对角色的字段，也不作为自动配对角色判定的前置协议。

`device.info` 的 P0 / confirmed subset 已进入 generated 合同；正式 methodId、eventId、errorCode、fieldId 以 `contract/registry/**`、`contract/protocol/axtp.protocol.yaml` 和 `contract/generated/**` 为准。本文保留的 review 标记仅用于后续修订。

## 2. 能力边界

| 类型 | 内容 |
|---|---|
| 包含 | 当前主设备的 `identity`、`product`、`hardware`、`os`、`software`、`runtime` 和轻量 `capability` 建模摘要。 |
| 包含 | `device.identity` 的只读字段；本 flow 不再保留独立 `device.identity` capability。 |
| 不包含 | 子设备、级联设备、拓扑树；这些属于 `device.childDevice` 或 topology feature。 |
| 不包含 | CPU/内存使用率、uptime、运行状态；这些属于 `system.state`。 |
| 不包含 | 关机、重启、恢复默认、恢复出厂；这些属于 `system.lifecycle` / `system.reset`。 |
| 不包含 | 设备显示名、资产标识等写入接口；有写入需求后另起草设置协议。 |
| 不包含 | 信息变化通知事件；本轮不定义 `device.infoChanged`。 |
| 数据面 | 本 feature 不定义 STREAM payload，所有操作均通过 RPC method 完成。 |

## 3. 方法 Methods

已生成 methodId、eventId、bitOffset 和 schema fieldId 以 contract/registry/generated 为准；本文不重新分配正式 ID，保留的 draft/review 标记仅作为后续修订输入。

### 3.0 方法速览

| Method | 调用类型 | 用途 | Params Schema | Result Schema | 是否触发事件 | 状态 |
|---|---|---|---|---|---|---|
| `device.getInfo` | query | 读取当前 endpoint 主设备信息。 | `GetDeviceInfoParams` | `DeviceInfo` | 否 | review-ok |

### 3.1 `device.getInfo`

**用途**：读取当前主设备身份、产品、硬件、OS、软件组件、AXTP runtime 和轻量 capability summary。

| 项 | 内容 |
|---|---|
| 调用类型 | query |
| Params Schema | `GetDeviceInfoParams` |
| Result Schema | `DeviceInfo` |
| 是否触发事件 | 否 |
| 幂等性 / 异步性 | 幂等；同步返回当前快照。 |
| 常见错误 | `NOT_SUPPORTED`, `INVALID_ARGUMENT`, `PERMISSION_DENIED`, `INTERNAL_ERROR` |

#### 3.1.1 请求参数 Params：`GetDeviceInfoParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `includeCapabilitySummary` | boolean | no | `true`, `false` | `true` | 是否返回轻量 `capability` 建模摘要。 |

#### 3.1.2 返回结果 Result：`DeviceInfo`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `identity` | `DeviceIdentity` | yes | object | none | 稳定身份字段。 |
| `product` | `DeviceProduct` | yes | object | none | 品牌、产品类型、型号和展示名。 |
| `hardware` | `DeviceHardware` | no | object | omitted | 硬件摘要。 |
| `os` | `DeviceOs` | no | object | omitted | 操作系统摘要。 |
| `software` | `DeviceSoftware` | no | object | omitted | 软件组件摘要。 |
| `runtime` | `DeviceAxtpRuntime` | no | object | omitted | AXTP runtime 摘要。 |
| `capability` | `DeviceCapabilitySummary` | no | object | omitted | 轻量建模摘要；不是完整 capability registry。 |

#### 3.1.3 d block 示例

request:

```json
{
  "id": 101,
  "method": "device.getInfo",
  "params": {
    "includeCapabilitySummary": true
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
    "identity": {
      "deviceId": "dev_001",
      "serialNumber": "NH-2026-000001",
      "vendorId": "nearhub",
      "productId": "nh-win-box-a1"
    },
    "product": {
      "brand": "NearHub",
      "productType": "windowsDevice",
      "model": "NH-WIN-BOX-A1",
      "displayName": "NearHub Display Controller"
    },
    "hardware": {
      "revision": "A1",
      "cpuArch": "x86_64",
      "memoryBytes": 8589934592
    },
    "os": {
      "type": "windows",
      "name": "Windows 11 IoT Enterprise",
      "version": "10.0.22631",
      "arch": "x86_64"
    },
    "software": {
      "components": [
        {
          "id": "launcher",
          "name": "NearHub Launcher",
          "version": "1.2.3",
          "role": "axtpHost"
        }
      ]
    },
    "runtime": {
      "axtpRuntime": "axtp-ts-runtime",
      "axtpRuntimeVersion": "0.1.0",
      "hostAppId": "launcher"
    },
    "capability": {
      "domains": [
        "device",
        "system",
        "audio",
        "video",
        "network"
      ],
      "features": [
        "device.info",
        "system.state"
      ]
    }
  }
}
```

#### 3.1.4 可能触发的事件

| Event | 触发条件 | Payload Schema | 客户端处理建议 |
|---|---|---|---|
| 无 | query method 不应因查询触发状态变化事件。 | none | 需要刷新时重新调用 `device.getInfo`。 |

#### 3.1.5 错误

| 错误 | 场景 | 返回建议 |
|---|---|---|
| `INVALID_ARGUMENT` | 请求结构或参数类型非法。 | 使用 adopted numeric code `10`。 |
| `PERMISSION_DENIED` | 当前调用方无权读取设备身份或序列号。 | 使用 adopted numeric code `9`；敏感字段可省略。 |
| `INTERNAL_ERROR` | 设备信息服务、OS 查询或 runtime 查询失败。 | 使用 adopted numeric code `14`。 |

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
      "field": "includeCapabilitySummary",
      "reason": "invalid field value"
    }
  }
}
```

## 4. 事件 Events

### 4.0 事件速览

| Event | 触发条件 | Payload Schema | 客户端处理建议 | 状态 |
|---|---|---|---|---|
| none | 本 feature 当前不定义事件。 | none | 需要刷新时重新调用 `device.getInfo`。 | deferred |

### 4.1 Deferred：`device.infoChanged`

本轮不定义 `device.infoChanged`。如未来出现设备名、资产标识、软件版本或 capability summary 变化通知需求，应基于具体业务场景另行起草事件。

## 5. Capability

Capability name: `device.info`。

| 能力字段 | 类型 | 必填 | 取值范围 / 枚举 | 说明 |
|---|---|---:|---|---|
| `capability` | string | yes | fixed `device.info` | capability 名称。 |
| `readOnly` | boolean | yes | `true` | 本轮只读，不提供 set/reset/change event。 |
| `supportsCapabilitySummary` | boolean | no | `true`, `false` | 是否支持在 `device.getInfo` 中返回轻量建模摘要。 |
| `identityMerged` | boolean | no | `true` | 表示 `device.identity` 已合并到 `device.info`，不再作为独立 capability。 |

## 6. 字段 / Schemas

### 6.1 Schema 层级速览

`device.info` 是只读信息快照，核心结构按信息来源分组，不把所有字段压在一个平面对象里。

```text
DeviceInfo
  identity: DeviceIdentity
  product: DeviceProduct
  hardware: DeviceHardware
  os: DeviceOs
  software: DeviceSoftware
    components: SoftwareComponent[]
  runtime: DeviceAxtpRuntime
  capability: DeviceCapabilitySummary
```

### 6.2 请求与响应 Schemas

#### `GetDeviceInfoParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `includeCapabilitySummary` | boolean | no | `true`, `false` | `true` | 是否返回 `DeviceInfo.capability`。 |

#### `DeviceInfo`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `identity` | `DeviceIdentity` | yes | see 6.5 | none | 稳定身份字段。 |
| `product` | `DeviceProduct` | yes | see 6.5 | none | 产品和展示信息。 |
| `hardware` | `DeviceHardware` | no | see 6.5 | omitted | 硬件摘要。 |
| `os` | `DeviceOs` | no | see 6.5 | omitted | OS 摘要。 |
| `software` | `DeviceSoftware` | no | see 6.5 | omitted | 软件组件摘要。 |
| `runtime` | `DeviceAxtpRuntime` | no | see 6.5 | omitted | AXTP runtime 摘要。 |
| `capability` | `DeviceCapabilitySummary` | no | see 6.5 | omitted | 轻量建模摘要。 |

### 6.3 Capability Schemas

#### `DeviceInfoCapability`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `capability` | string | yes | fixed `device.info` | none | capability 名称。 |
| `readOnly` | boolean | yes | `true` | `true` | 当前只有只读方法。 |
| `supportsCapabilitySummary` | boolean | no | bool | `true` | 是否支持建模摘要。 |
| `identityMerged` | boolean | no | `true` | `true` | 是否合并 device identity。 |

### 6.4 Event Schemas

本轮没有 event payload schema。

### 6.5 State / Config / Object Schemas

#### `DeviceIdentity`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `deviceId` | string | yes | product/runtime-defined stable id | none | AXTP/业务系统使用的稳定设备 ID。 |
| `serialNumber` | string | no | vendor serial number | omitted | 厂商序列号；可按权限省略。 |
| `vendorId` | string | no | vendor id | omitted | 厂商 ID。 |
| `productId` | string | no | product id | omitted | 产品 ID。 |

#### `DeviceProduct`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `brand` | string | no | brand name | omitted | 品牌。 |
| `productType` | string enum | yes | `windowsDevice`, `androidDevice`, `embeddedDevice`, `rtosDevice`, `cameraDevice`, `displayDevice`, `unknown` candidate | none | 产品类型；首批 enum 仍需确认。 |
| `model` | string | yes | hardware / whole-product model | none | 硬件或整机型号。 |
| `displayName` | string | no | user-visible name | omitted | 用户可见设备名称；本轮只读返回。 |

#### `DeviceHardware`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `revision` | string | no | hardware revision | omitted | 硬件修订号。 |
| `cpuArch` | string enum | no | `x86_64`, `arm64`, `armv7`, `riscv64`, `unknown` candidate | omitted | CPU 架构。 |
| `memoryBytes` | uint64 | no | `0..uint64 max` | omitted | 物理内存容量。 |

#### `DeviceOs`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `type` | string enum | yes | `windows`, `android`, `linux`, `rtos`, `unknown` candidate | none | OS 类型。 |
| `name` | string | no | OS display name | omitted | OS 名称。 |
| `version` | string | no | OS version string | omitted | OS 版本。 |
| `arch` | string enum | no | `x86_64`, `arm64`, `armv7`, `riscv64`, `unknown` candidate | omitted | OS 架构。 |

#### `DeviceSoftware`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `components` | `SoftwareComponent[]` | no | array | omitted | Launcher、Signage、Cast Receiver 等软件组件。 |

#### `SoftwareComponent`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `id` | string | yes | stable component id | none | 软件组件稳定 ID。 |
| `name` | string | no | display name | omitted | 软件显示名。 |
| `version` | string | no | semver or product version | omitted | 软件版本。 |

#### `DeviceAxtpRuntime`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `axtpRuntime` | string | no | runtime implementation name | omitted | AXTP runtime 名称。 |
| `axtpRuntimeVersion` | string | no | runtime version | omitted | AXTP runtime 版本。 |
| `hostAppId` | string | no | `SoftwareComponent.id` | omitted | 承载 AXTP runtime 的 host app ID。 |

#### `DeviceCapabilitySummary`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `profile` | string | no | product/profile id | omitted | 产品或设备建模 profile。 |
| `domains` | string[] | no | domain name array | omitted | 设备暴露的主要 domain 摘要。 |
| `features` | string[] | no | `domain.feature` array | omitted | 设备暴露的主要 feature 摘要；不是完整 registry。 |

## 7. JSON 示例

示例只展示 RPC data block，不包裹外层 wire envelope。字段和 ID 在采纳前均为草案；序列号等设备相关字段使用占位符。

### 7.1 场景：读取主设备信息

#### request

```json
{
  "id": 1,
  "method": "device.getInfo",
  "params": {
    "includeCapabilitySummary": true
  }
}
```

#### response

```json
{
  "id": 1,
  "status": {
    "ok": true,
    "code": 0
  },
  "result": {
    "identity": {
      "deviceId": "device-001",
      "serialNumber": "SN-2026-0001",
      "vendorId": "nearhub",
      "productId": "na20"
    },
    "product": {
      "brand": "NearHub",
      "productType": "embeddedDevice",
      "model": "NA20",
      "displayName": "NA20"
    },
    "runtime": {
      "axtpRuntime": "axtp-runtime",
      "axtpRuntimeVersion": "0.1.0",
      "hostAppId": "cast-receiver"
    },
    "capability": {
      "domains": [
        "device",
        "network"
      ],
      "features": [
        "device.info",
        "network.interface",
        "network.ap",
        "network.ip"
      ]
    }
  }
}
```

读法：`device.info` 返回设备身份和产品信息，但不返回用于自动配对的设备角色字段。NA20 / NT10 的区分由 Host 本地完成。

### 7.2 场景：只读取身份和产品信息

#### request

```json
{
  "id": 2,
  "method": "device.getInfo",
  "params": {
    "includeCapabilitySummary": false
  }
}
```

#### response

```json
{
  "id": 2,
  "status": {
    "ok": true,
    "code": 0
  },
  "result": {
    "identity": {
      "deviceId": "device-001",
      "serialNumber": "SN-2026-0001",
      "vendorId": "nearhub",
      "productId": "nt10"
    },
    "product": {
      "brand": "NearHub",
      "productType": "embeddedDevice",
      "model": "NT10",
      "displayName": "NT10"
    }
  }
}
```

读法：`includeCapabilitySummary=false` 只影响 `capability` 摘要，不表示隐藏 `identity` 或 `product`。

### 7.3 场景：读取失败响应

```json
{
  "id": 3,
  "status": {
    "ok": false,
    "code": 14,
    "msg": "Read device info failed.",
    "details": {
      "candidateError": "DEVICE_INFO_READ_FAILED"
    }
  }
}
```

读法：`status.code=14` 对应 adopted `INTERNAL_ERROR`。失败不改变设备状态，也不会触发事件。

## 8. 错误

| 错误 | 适用场景 | 说明 |
|---|---|---|
| `NOT_SUPPORTED` | 设备或产品 profile 不支持 `device.info`。 | 使用 adopted numeric code `3`。 |
| `INVALID_ARGUMENT` | 请求结构非法。 | 使用 adopted numeric code `10`。 |
| `PERMISSION_DENIED` | 当前调用方无权读取设备身份、序列号或资产字段。 | 使用 adopted numeric code `9`。 |
| `INTERNAL_ERROR` | 设备信息服务、OS 查询或 runtime 查询失败。 | 使用 adopted numeric code `14`。 |
| `DEVICE_INFO_READ_FAILED` | 候选业务错误：读取本机信息失败。 | `[REVIEW-DRAFT]`；采纳前确认是否需要 feature-specific errorCode。 |

## 9. Legacy 映射

Legacy 映射是迁移证据，不是 runtime 合同。旧字段名、旧命令名和敏感 payload 不得直接污染正式 `DeviceInfo` schema。

| legacy 项 | 候选映射 | 状态 | 说明 |
|---|---|---|---|
| AXDP `CommonGetEncryptedInfo` 等设备信息命令 | `device.getInfo` | `[REVIEW-ASK]` | 字段级映射和是否包含敏感加密信息需确认；敏感字段可能不属于 `device.info`。 |
| Rooms `GetDeviceInfo` / `GetDevInfo` / `GetSn` | `device.getInfo` | `[REVIEW-ASK]` | SN 和设备详情进入 `identity` / `product`。 |
| Signage / Rooms `SetDeviceName` | out of current draft / future setting protocol | `[REVIEW-OK]` | 本轮不映射到 `device.info`；有明确设置需求后另起草设备名或资产设置协议。 |
| VM33 `System.GetDevInfo` | `device.getInfo` | `[REVIEW-ASK]` | 旧 System namespace 不决定新 domain；按语义归 `device.info`。 |

## 10. 测试要点

| 类型 | 要点 |
|---|---|
| happy path | `device.getInfo` 返回 `identity`、`product`，并按设备能力返回 optional 段。 |
| capability summary | `includeCapabilitySummary=true` 返回 `capability`；`false` 时可省略。 |
| cast pairing | Host 不通过 `device.info` 区分 NA20/NT10；设备识别属于 USB descriptor、本地规则或人工选择。 |
| boundary case | optional 段缺失、未知 enum、序列号按权限省略。 |
| error case | 信息服务读取失败、无权限、请求结构非法。 |
| compatibility | `device.identity` 不再作为独立 capability；`device.getInfo` 默认不返回 children。 |

## 11. 待确认问题

| 问题 | 影响 | 当前建议 | 状态 |
|---|---|---|---|
| `device.info` 是否承载 Cast RX/TX 配对角色字段？ | cast pairing / schema | 不承载；NA20/NT10 识别由 USB descriptor、本地规则或人工选择完成。 | decided |
| `productType`、`os.type`、`cpuArch` 的首批 enum 是什么？ | schema / conformance | 首批提供常见候选值，并保留 `unknown` fallback。 | open |
| `capability.profile` 是否来自 profiles registry？ | registry / capability | 先作为产品 profile 字符串；正式采纳前和 profiles registry 对齐。 | open |
| legacy 设备信息是否包含敏感字段？ | legacy / security | 敏感字段不进入 `device.info`，必要时拆到 auth/vendor/diagnostic。 | open |
