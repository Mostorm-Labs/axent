# AXTP Domain And Feature Classification

本文说明 AXTP 业务能力如何选择 domain、feature 和候选 method/event 归属。它面向协议维护者、legacy 迁移负责人、App / 固件 / runtime 研发，以及在写 `workspace/flows/**` 和 `workspace/protocol/**` 前需要先判断“这件事应该放在哪里”的读者。

本文是规范性 domain / feature 分类原则。稳定 method、event、schema、error 和 capability 机器事实仍以 `contract/registry/**/*.yaml`、`contract/registry/domains/**/*.yaml`、`contract/protocol/axtp.protocol.yaml` 和 `contract/generated/**` 为准。命名模板和 registry 规则以 `specs/2-registry/01-Naming-and-Taxonomy.md` 为准。

## 1. Why This Exists

AXTP 要覆盖 Rooms、数字标牌、投屏接收端、Windows Launcher、音视频设备、嵌入式设备、上位机控制和设备管理等场景。这些场景经常有一个共同问题：

```text
同一个物理设备，既代表一个逻辑空间，又承载多个业务能力。
```

例如一个 Windows Launcher 设备可能同时承担：

- 设备基础信息。
- 系统时间、重启、关机和运行时状态。
- 网络、存储、日志和软件更新。
- 房间绑定或空间管理。
- 数字标牌播放。
- 投屏接收。
- 音频、视频、显示和摄像头控制。

如果所有方法都按软件名称或产品名称归类，就会出现：

```text
launcher.getDeviceInfo
launcher.setPlaylistConfig
launcher.stopCasting
uxplay.setPinCode
```

这会让协议和当前实现强绑定。更稳定的方式是先区分三件事：

| Question | Meaning | Typical domains |
|---|---|---|
| 这个 AXTP endpoint 在业务系统里代表什么？ | 逻辑端点或空间抽象。 | `room` |
| 当前物理设备和系统本身是什么、状态如何？ | 设备身份、系统运行状态、通用资源。 | `device`, `system`, `network`, `storage`, `software`, `log` |
| 当前设备实际能做什么？ | 设备提供的业务功能或媒体能力。 | `signage`, `cast`, `audio`, `video`, `camera`, `display` |

一句话原则：

```text
room 描述业务视角；
device/system 描述设备和系统视角；
signage/cast/audio/video 等描述功能视角。
```

## 2. Classification Layers

### 2.1 Logical Endpoint Layer

这一层描述当前 AXTP endpoint 在业务系统里代表什么。当前最典型的 domain 是 `room`。

`room` 不是设备硬件，也不是系统能力，而是空间、房间、会议室、教室、组织节点或云端管理对象。

适合放入 `room` 的内容：

- 房间信息。
- 房间绑定。
- 房间日程。
- 房间内资源或能力摘要。
- 房间级状态。
- 房间级遥测。

候选示例：

```text
room.getInfo
room.setInfo
room.getBindCode
room.getBindConfig
room.setBindConfig
room.bindStateChanged
room.telemetryReported
```

这些名称只是分类示例。进入正式协议前仍需要经过 flow、draft、adoption 和 generation。

### 2.2 Physical Device Layer

这一层描述当前物理设备、操作系统或通用设备资源本身。

常见 domains：

```text
device
system
network
storage
software
log
```

`device` 负责“这是谁”：

- 设备 ID、SN、产品型号、硬件摘要。
- 设备展示名等只读产品展示字段。
- 当前 AXTP endpoint 主设备信息。
- 子设备、级联设备、拓扑。

候选示例：

```text
device.getInfo
device.getChildren
device.getChildInfo
device.getTopology
```

`system` 负责“这台机器怎么运行”：

- 系统时间、时区、NTP。
- CPU、内存、在线、运行态和状态变化事件。
- 保活、重启、关机、休眠、唤醒。
- 恢复配置、恢复出厂和初始化状态。

候选示例：

```text
system.getState
system.stateChanged
system.setTimeConfig
system.reboot
system.shutdown
system.sleep
system.wake
system.reset
```

`network`、`storage`、`software`、`log` 负责通用设备资源：

```text
network.getInterfaces
network.getWifiConfig
network.setWifiConfig
network.scanWifi

storage.getSdCardState
storage.formatSdCard
storage.getDiskState

software.getInfo
software.beginUpdate
software.getUpdateState
software.cancelUpdate

log.createExport
log.getExportState
log.exportStateChanged
```

### 2.3 Functional Capability Layer

这一层描述设备实际提供的业务功能或媒体能力。

常见 domains：

```text
signage
cast
audio
video
camera
display
```

`signage` 表示设备自身的数字标牌播放和展示能力：

```text
signage.getPlaylistConfig
signage.setPlaylistConfig
signage.getPlaylistItemUrl
signage.getAppearanceConfig
signage.setAppearanceConfig
signage.playbackStateChanged
```

`cast` 表示设备自身的投屏能力。投屏后端实现，例如 UxPlay，不应该进入 domain 或 method name：

```text
cast.getSession
cast.stopSession
cast.getPinCode
cast.setPinCode
cast.showWindow
cast.hideWindow
cast.getBackendStatus
cast.restartBackend
```

`audio`、`video`、`camera`、`display` 表示设备的媒体和输出能力：

```text
audio.getVolumeConfig
audio.setVolumeConfig
audio.getInputConfig
audio.setInputConfig

video.openStream
video.closeStream
video.getStreamState

camera.getPtzConfig
camera.setPtzConfig

display.getBrightnessConfig
display.setBrightnessConfig
```

## 3. Domain Decision Rules

新增或迁移一个能力时，按以下顺序问问题。

### 3.1 Is It Describing A Logical Space?

如果它描述的是房间、会议室、教室、空间、组织节点或云端管理对象，优先放到 `room`。

Examples:

| Requirement | Domain direction |
|---|---|
| 获取房间绑定码 | `room` |
| 上报房间级遥测 | `room` |
| 查询房间日程 | `room.schedule` or a room-related calendar feature |

不要因为房间信息由某台设备上报，就把它放进 `device`。

### 3.2 Is It Describing Device Identity Or Topology?

如果它回答“这是谁、是什么设备、下面还有谁”，放到 `device`。

Examples:

| Requirement | Domain direction |
|---|---|
| 获取 deviceId、SN、产品型号 | `device.info` |
| 设置设备显示名 | future setting protocol; not current read-only `device.info` |
| 获取一级子设备 | `device.childDevice` |
| 获取完整拓扑 | `device.childDevice` or topology-specific device feature |

`device.getInfo` 应该保持轻量稳定。默认只返回当前 AXTP endpoint 代表的主设备，不默认返回全部子设备。

### 3.3 Is It Describing System Runtime Or Lifecycle?

如果它回答“这台机器现在怎么运行，能不能重启、关机或恢复运行时状态”，放到 `system`。

Examples:

| Requirement | Domain direction |
|---|---|
| CPU、内存、在线、运行态变化 | `system.state` candidate |
| 电池、供电、电源状态 | 本轮不作为独立 system power feature；如需要，另行定域为 telemetry/sensor/external power control |
| 定时关机、定时重启 | `system.lifecycle` |
| 重启、关机、休眠、唤醒 | `system.lifecycle` |
| 恢复默认、恢复出厂 | `system.reset` or `system.initialization` |

软件关机不另建独立 power-off 方法，统一使用 `system.shutdown`。电池/供电/外部电源控制不建议直接归到 `device`，也不在本轮保留独立 system power feature；如产品确实需要，应另行评审为 telemetry/sensor 或外部电源控制能力。

### 3.4 Is It Managing Generic Device Resources?

如果它管理的是网络、存储、软件包、日志等通用资源，放到对应资源域。

Examples:

| Requirement | Domain direction |
|---|---|
| Wi-Fi 扫描、连接、配置 | `network.wifi` |
| IP/DHCP/DNS | `network.ip` |
| SD 卡状态和格式化 | `storage.sdCard` |
| Windows Launcher 应用升级 | `software.update` candidate |
| 设备日志导出 | `log.export` |

### 3.5 Is It Controlling A Device Function?

如果它控制的是设备实际提供的业务功能，放到功能域。

Examples:

| Requirement | Domain direction |
|---|---|
| 数字标牌播放列表 | `signage.playlist` |
| 数字标牌资源 URL 刷新 | `signage.media` |
| 投屏 PIN 码和会话 | `cast` |
| Line-out 音量 | `audio.volume` |
| 视频流 | `video.stream` |
| 摄像头 PTZ | `camera.ptz` |
| 显示亮度 | `display.brightness` |

## 4. Feature Granularity Rules

Feature 是能力块，不是字段名、状态名或临时实现名。

Good feature names:

```text
device.info
device.childDevice
system.lifecycle
network.wifi
storage.sdCard
firmware.update
log.export
signage.playlist
audio.volume
video.stream
```

Avoid feature names like:

```text
device.model
system.lifecycleState
network.wifiConfig
firmware.updateProgress
display.brightnessMin
stream.fileTransfer
```

Rules:

- 字段进入 schema，不提升为 feature。
- `Config`、`State`、`Mode`、`Policy` 通常进入 method 或 schema，不作为 feature 后缀。
- Capability 可以粗一点，method 可以细一点。
- Method name 使用 `domain.verbObject`，不需要把 feature 作为中间命名空间。
- Event name 表达变化、进度、结果或上报。

Example:

```text
Capability: signage.playlist
Methods:    signage.getPlaylistConfig, signage.setPlaylistConfig
Event:      signage.playlistConfigChanged
```

Do not use:

```text
signage.playlist.getConfig
```

## 5. Important Boundaries

### 5.1 Device Info

`device.getInfo` should answer:

```text
Who am I?
What product am I?
What hardware and OS do I run on?
Which software hosts AXTP?
What capability summary do I expose?
```

Recommended conceptual shape:

```json
{
  "deviceId": "dev_001",
  "serialNumber": "NH-2026-000001",
  "product": {
    "brand": "NearHub",
    "productType": "windowsDevice",
    "model": "NH-WIN-BOX-A1",
    "displayName": "NearHub Display Controller"
  },
  "hardware": {
    "revision": "A1",
    "manufacturer": "NearHub",
    "cpuArch": "x86_64"
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
    "hostAppId": "launcher",
    "hostAppVersion": "1.2.3"
  },
  "capability": {
    "domains": ["device", "system", "network", "storage", "audio", "signage", "cast", "log"]
  }
}
```

Do not put software names into `product.model`:

```json
{
  "model": "NearHub Launcher"
}
```

Launcher is software. It belongs in `software.components` or `runtime.hostAppId`.

### 5.2 Child Devices And Topology

Do not make `device.getInfo` return all child devices by default.

Use separate conceptual methods:

```text
device.getInfo        -> current endpoint main device
device.getChildren    -> direct child summaries
device.getChildInfo   -> one child detail
device.getTopology    -> full tree, optional
```

Principle:

```text
Info answers "who am I?"
Children / Topology answers "who is below me?"
```

Reason:

- 主设备信息稳定，适合缓存。
- 子设备在线、端口、健康和能力变化更频繁。
- 子设备数量可能不可控。
- 子设备详情可能需要更高权限。

### 5.3 Room Vs Device

一个 AXTP endpoint 可以同时具备 `room`、`device`、`system`、`signage`、`cast` 等能力。

Use this split:

| Domain | Meaning |
|---|---|
| `room` | 业务系统中的空间或房间身份。 |
| `device` | 实际运行 AXTP 的设备身份和拓扑。 |
| `system` | 设备 OS / runtime state / lifecycle controls。 |
| `signage`, `cast`, etc. | 设备实际功能。 |

Example:

```json
{
  "room": {
    "roomId": "room_201",
    "name": "Meeting Room 201"
  },
  "device": {
    "deviceId": "dev_001",
    "serialNumber": "NH-2026-000001",
    "product": {
      "productType": "windowsDevice",
      "model": "NH-WIN-BOX-A1"
    }
  },
  "capabilities": ["signage", "cast", "audio", "video"]
}
```

### 5.4 Signage Vs Room

`signage` is the device's digital signage playback and display capability. It is not the room itself.

Good:

```text
signage.setPlaylistConfig
signage.getPlaylistItemUrl
signage.setAppearanceConfig
```

If a playlist is selected because of a room, pass `roomId` or `playlistId` as data. Do not make it a nested method namespace:

```text
room.signage.setPlaylistConfig
```

### 5.5 Cast Vs Backend Implementation

`cast` is the device's casting capability. UxPlay, AirPlay receiver, or another engine is implementation detail.

Good:

```text
cast.getSession
cast.stopSession
cast.getPinCode
cast.setPinCode
cast.showWindow
cast.getBackendStatus
cast.restartBackend
```

Use fields for role and backend:

```json
{
  "roles": ["receiver"],
  "activeRole": "receiver",
  "backend": {
    "type": "uxplay",
    "state": "running"
  }
}
```

Avoid:

```text
uxplay.setPinCode
cast.receiver.setPinCode
```

### 5.6 Software Vs Firmware

Use `firmware` for low-level device firmware:

- MCU firmware.
- Bootloader.
- Embedded image.
- DSP / ISP / FPGA firmware.

Use `software` for OS-level applications and services:

- Windows Launcher application.
- Digital signage application.
- Cast receiver application.
- Desktop agent.
- Runtime component.

Conceptual example:

```text
software.beginUpdate
software.getUpdateState
software.cancelUpdate
```

Firmware update remains:

```text
firmware.beginUpdate
firmware.getUpdateState
firmware.installUpdate
```

The target should be expressed in payload, not by making product-specific domains:

```json
{
  "target": {
    "type": "application",
    "id": "launcher",
    "name": "NearHub Launcher"
  },
  "package": {
    "url": "https://example.com/launcher-1.2.4.zip",
    "version": "1.2.4"
  }
}
```

### 5.7 Power Belongs To System

Power is a runtime/lifecycle concern, not a device identity concern.

Prefer:

```text
system.reboot
system.shutdown
system.sleep
system.wake
```

If legacy commands contain schedule fields for shutdown or reboot, map them to system lifecycle, not to signage schedule. Do not introduce a separate system power schedule unless a future review creates a dedicated non-duplicative capability.

## 6. Legacy Migration Examples

The following examples show classification direction only. They are not adopted method names.

### 6.1 NearHub Launcher Common Management

| Legacy command/event | Classification direction | Notes |
|---|---|---|
| `KeepAlive` | `system.lifecycle` or core heartbeat | Prefer transport/session heartbeat unless business last-online is required. |
| `GetDeviceInfo` | `device.info` | Main endpoint identity/product/software/runtime summary. |
| `SetDeviceName` | deferred setting protocol | Current `device.info` remains read-only; define a concrete setting protocol only when required. |
| `SetSysTime` | `system.time` | System time and timezone. |
| `ResetConfig` | `system.reset` / `system.initialization` | Confirm reset scope. |
| `GetNetworkInfo` | `network.interface` + `network.ip` + `network.wifi` | Legacy aggregate may split across features. |
| `GetSDInfo` | `storage.sdCard` | State query. |
| `FormatSd` | `storage.sdCard` | Action method, not config set. |
| `RemoteUpgrade` | `software.update` or `firmware.update` | Depends whether target is application/service or firmware image. |
| `UpgradeProgress` | matching update state/progress | Same target domain as begin update. |
| `GetBindCode` | `room` or `auth` depending on semantics | Room binding vs device auth must be confirmed. |
| `OnTelemetryReport` | `room`, `system.state`, or sensor domain | Depends whether telemetry is room-level, system runtime, or sensor-specific. |
| `RequestLogUpload` | `log.export` | Async export task. |
| `NotifyLogUploadResult` | `log.export` event | Prefer event/result state over method-style notify. |

### 6.2 Digital Signage

| Legacy command/event | Classification direction | Notes |
|---|---|---|
| `SetPlaylistConfig` | `signage.playlist` | Full playlist config. |
| `GetPlaylistConfig` | `signage.playlist` | Same schema as set response/config. |
| `GetPlaylistItemUrl` | `signage.media` | URL refresh by item ID. |
| `GetAppearanceConfig` | `signage.osd` or signage appearance feature | Confirm whether OSD is accurate. |
| `GetScheduleConfig` with shutdown/reboot | `system.lifecycle` | Do not classify as signage schedule if it controls device lifecycle. |

### 6.3 Cast Receiver

| Legacy command/event | Classification direction | Notes |
|---|---|---|
| `getStatus` | `cast` | Cast capability status. |
| `stopCasting` | `cast` | Session action. |
| `getPin` / `setPin` | `cast` | PIN config/state. |
| `showCastWindow` / `hideCastWindow` | `cast` | Receiver UI window control. |
| `restartUxPlay` | `cast` | Backend restart action; UxPlay remains payload data. |
| `uxplay.ready` / `uxplay.exited` | `cast` event | Backend lifecycle event. |

## 7. Review Checklist

Before creating or updating a protocol draft, check:

1. Does the requirement describe a logical endpoint, physical device/system, generic resource, or functional capability?
2. Is the chosen domain a stable business semantic, not a software package name, process name, backend implementation, transport, codec, or product SKU?
3. Is the feature a capability block rather than a field, state, config, mode, codec, or old command name?
4. Can future config/state/action/event methods fit under the same feature without renaming it?
5. Does the method use `domain.verbObject` rather than `domain.feature.verb`?
6. Is a high-frequency state being split away from stable identity information?
7. Are child devices or topology separated from main device info?
8. Are lifecycle, health and runtime state separated from device identity, and are power/battery/external power needs explicitly deferred or separately classified?
9. Is software update separated from firmware update?
10. Is legacy mapping documented as `legacy command -> domain.feature -> method/event -> schema adapter`?

When in doubt, write a Stage 10 flow first. The flow should record the competing classifications and route unresolved gaps to `draft-business-protocol`.
