# Device SDK 业务指令与事件设计 (RPC 参考)

本文档定义了设备常用 RPC 指令。底层协议细节请参阅 [API.md](./API.md)，SDK 使用说明与业务流程请参阅 [SDK.md](./SDK.md) 和 [BUSINESS.md](./BUSINESS.md)。

---

## 书写规范 (Specifications)

为保持文档一致性与协议严谨性，请遵循以下书写约定：

### 1. 命名约定

- **指令 (Commands)**: 采用 `Verb + Resource` 形式。如 `SetDeviceName`, `GetDeviceInfo`。
- **事件 (Events)**: 统一以 `On` 开头，描述已发生的事实。如 `OnBindState`, `OnConfigUpdated`。
- **条目拆分**: 一个指令或一个事件占用一个独立的小节（标题行），禁止将 Set/Get 合并。

### 2. 参数与结果 (Payloads)

- **Get 指令**: 请求参数通常为 **无**，响应结果返回具体的数据对象。
- **Set 指令**: 请求参数包含具体设置项，响应结果统一返回成功标识 `{ "ok": true }`。
- **展示格式**: 所有 Payload 必须使用多行 JSON 代码块包裹。

### 3. 研发状态标签

每个条目必须包含一个状态标识，可选值：

- `未研发`: 协议已定义，但代码尚未实现。
- `研发中`: 代码正在开发或测试中。
- `已研发`: 代码已合并至主分支。

---

## 1. 系统基础 (System)

这些指令用于管理设备的基本信息与系统状态。

### [指令] KeepAlive

- **功能描述**: 心跳保活，用于双方检测连接存活与最后在线时间。
- **状态**: 已研发
- **调用方向**: Server <-> Device
- **请求参数**: 无
- **响应结果**:
  ```json
  { "ok": true }
  ```

### [事件] KeepAlive

- **功能描述**: 心跳到达事件，用于记录最后在线时间（SDK 本地事件）。
- **状态**: 已研发
- **调用方向**: Server <-> Device
- **data**:
  ```json
  {
    "sid": "session-id",
    "deviceId": "device-id",
    "at": 1739999999999
  }
  ```

### [指令] GetDeviceInfo

- **功能描述**: 获取设备基本信息（型号、名称、资源使用情况、网络状态、版本等）。
- **状态**: 已研发
- **调用方向**: Server -> Device
- **请求参数**: 无
- **响应结果**:
  ```json
  {
    "model": "ROOMS",
    "devName": "classRooms-201",
    "cpuUsage": 60,
    "memoryUsage": 20,
    "ip": "192.168.19.165",
    "mac": "98:6E:E8:50:03:A1",
    "version": "V1.0.0.0.R.250120"
  }
  ```

### [指令] SetDeviceName

- **功能描述**: 设置设备自定义显示名称。
- **状态**: 已研发
- **调用方向**: Server -> Device，Device -> Server
- **请求参数**:
  ```json
  {
    "devName": "new-device-name"
  }
  ```
- **响应结果**:
  ```json
  { "ok": true }
  ```

### [指令] SetSysTime

- **功能描述**: 同步或手动设置设备系统时间与时区。
- **状态**: 未研发
- **调用方向**: Server -> Device
- **请求参数**:
  ```json
  {
    "timezone": "Asia/Shanghai",
    "year": 2024,
    "month": 11,
    "day": 12,
    "hour": 14,
    "minute": 30,
    "second": 45
  }
  ```
- **响应结果**:
  ```json
  { "ok": true }
  ```

### [指令] ResetConfig

- **功能描述**: 恢复出厂设置。注意：执行后设备通常会自动重启。
- **状态**: 未研发
- **调用方向**: Server -> Device
- **请求参数**: 无
- **响应结果**:
  ```json
  { "ok": true }
  ```

### [指令] GetNetworkInfo

- **功能描述**: 获取设备当前网络连接信息。
- **状态**: 已研发
- **调用方向**: Server -> Device
- **请求参数**: 无
- **响应结果**:
  ```json
  [
    {
      "type": "wifi",
      "connected": true,
      "ip": "192.168.19.165",
      "mac": "98:6E:E8:50:03:A1",
      "ssid": "Office-5G",
      "rssi": -45
    },
    {
      "type": "ethernet",
      "connected": false,
      "ip": "192.168.19.166",
      "mac": "98:6E:E8:50:03:A2",
      "ssid": null,
      "rssi": null
    }
  ]
  ```

---

## 2. 存储管理 (Storage)

### [指令] GetSDInfo

- **功能描述**: 获取 SD 卡挂载状态及容量信息。
- **状态**: 未研发
- **调用方向**: Server -> Device
- **请求参数**: 无
- **响应结果**:
  ```json
  {
    "status": "sdAddFormatted",
    "totalSize": 2344444,
    "availableSize": 100000
  }
  ```

### [指令] FormatSd

- **功能描述**: 格式化 SD 卡。
- **状态**: 未研发
- **调用方向**: Server -> Device
- **请求参数**: 无
- **响应结果**:
  ```json
  { "ok": true }
  ```
- **备注**: 格式化为异步过程，结果通常通过事件通知。

---

## 3. 音频设置 (Audio)

### [指令] SetLineOutVolume

- **功能描述**: 设置 Line-out 输出音量。
- **状态**: 未研发
- **调用方向**: Server -> Device
- **请求参数**:
  ```json
  {
    "volume": 10
  }
  ```
- **响应结果**:
  ```json
  { "ok": true }
  ```

### [指令] GetLineOutVolume

- **功能描述**: 获取 Line-out 输出音量。
- **状态**: 未研发
- **调用方向**: Server -> Device
- **请求参数**: 无
- **响应结果**:
  ```json
  {
    "volume": 10
  }
  ```

### [指令] SetLineInPreGain

- **功能描述**: 设置 Line-in 预增益。
- **状态**: 未研发
- **调用方向**: Server -> Device
- **请求参数**:
  ```json
  {
    "preGain": 5
  }
  ```
- **响应结果**:
  ```json
  { "ok": true }
  ```

### [指令] GetLineInPreGain

- **功能描述**: 获取 Line-in 预增益。
- **状态**: 未研发
- **调用方向**: Server -> Device
- **请求参数**: 无
- **响应结果**:
  ```json
  {
    "preGain": 5
  }
  ```

---

## 4. 系统维护 (Maintenance)

### [指令] RemoteUpgrade

- **功能描述**: 触发设备固件远程升级任务。
- **状态**: 未研发
- **调用方向**: Server -> Device
- **请求参数**:
  ```json
  {
    "url": "http://example.com/firmware.bin"
  }
  ```
- **响应结果**:
  ```json
  { "ok": true }
  ```

### [指令] UpgradeProgress

- **功能描述**: 查询当前固件下载或安装的进度。
- **状态**: 未研发
- **调用方向**: Server -> Device
- **请求参数**:
  ```json
  {
    "url": "http://example.com/firmware.bin"
  }
  ```
- **响应结果**:
  ```json
  {
    "progress": 45
  }
  ```

---

## 5. 业务流程 (Business)

这些指令在 [BUSINESS.md](./BUSINESS.md) 中有详细流程描述。

### [指令] GetBindCode

- **功能描述**: 获取设备绑定码（原配对码）。
- **状态**: 已研发
- **调用方向**: Device -> Server
- **请求参数**: 无
- **响应结果**:
  ```json
  {
    "code": "ABC123",
    "expiresAt": 1234567890,
    "expiresInSeconds": 1800
  }
  ```

### [指令] GetBindConfig

- **功能描述**: 获取设备当前绑定状态。
- **状态**: 已研发
- **调用方向**: Server -> Device, Device -> Server
- **请求参数**: 无
- **响应结果**:
  ```json
  {
    "bound": true
  }
  ```

### [指令] SetBindConfig

- **功能描述**: 服务器主动通知设备更新绑定状态。
- **状态**: 已研发
- **调用方向**: Server -> Device
- **请求参数**:
  ```json
  {
    "bound": true
  }
  ```
- **响应结果**:
  ```json
  { "ok": true }
  ```

### [事件] OnBindState

- **功能描述**: 绑定状态变更通知。
- **状态**: 未研发
- **调用方向**: Device -> Server
- **data**:
  ```json
  {
    "status": "success",
    "code": "ABC123",
    "message": ""
  }
  ```

### [事件] OnTelemetryReport

- **功能描述**: 遥测数据上报（如温度、电池电量）。
- **状态**: 未研发
- **调用方向**: Device -> Server
- **data**:
  ```json
  {
    "temp": 25.5,
    "battery": 90
  }
  ```

---

## 6. 数字标牌 (Digital Signage)

这些指令用于同步数字标牌播放列表配置到设备。

### [指令] SetPlaylistConfig

- **功能描述**: 设置设备数字标牌播放列表配置（全量同步）。
- **状态**: 已研发
- **调用方向**: Server -> Device
- **请求参数**:
  ```json
  {
    "playlists": [
      {
        "id": "uuid-1",
        "type": "default",
        "startDate": "2024-01-01",
        "endDate": "2024-12-31",
        "startTime": "08:00:00",
        "endTime": "20:00:00",
        "days": [1, 2, 3, 4, 5],
        "items": [
          {
            "id": "uuid-2",
            "type": "image",
            "duration": 60,
            "sort": 0,
            "settings": {
              "urls": ["https://example.com/resource/file-1.jpg", "https://example.com/resource/file-2.jpg"],
              "delaySeconds": 5,
              "expiresAt": 1704067200
            }
          },
          {
            "id": "uuid-3",
            "type": "video",
            "duration": 120,
            "sort": 1,
            "settings": {
              "url": "https://example.com/resource/video-1",
              "expiresAt": 1704153600,
              "muted": false
            }
          },
          {
            "id": "uuid-4",
            "type": "website",
            "duration": 300,
            "sort": 2,
            "settings": {
              "url": "https://example.com",
              "ignoreCertificateError": false,
              "refreshIntervalSecs": 300
            }
          }
        ]
      }
    ]
  }
  ```
- **响应结果**:
  ```json
  { "ok": true }
  ```

**请求字段：**

| 字段名                                              | 类型           | 描述                                                                                     | 值限制                                            | 默认行为 |
| --------------------------------------------------- | -------------- | ---------------------------------------------------------------------------------------- | ------------------------------------------------- | -------- |
| playlists                                           | Array          | 播放列表配置数组                                                                         | Non-empty                                         | N/A      |
| playlists[].id                                      | String         | 播放列表唯一标识                                                                         | UUID format                                       | N/A      |
| playlists[].type                                    | String         | 播放列表类型：`default`-默认播放列表，`scheduled`-定时播放列表                           | `default`, `scheduled`                            | N/A      |
| playlists[].startDate                               | String         | 开始日期 (YYYY-MM-DD)                                                                    | ISO 8601 date                                     | `null`   |
| playlists[].endDate                                 | String         | 结束日期 (YYYY-MM-DD)                                                                    | ISO 8601 date                                     | `null`   |
| playlists[].startTime                               | String         | 开始时间 (HH:mm:ss)                                                                      | ISO 8601 time                                     | `null`   |
| playlists[].endTime                                 | String         | 结束时间 (HH:mm:ss)                                                                      | ISO 8601 time                                     | `null`   |
| playlists[].days                                    | Array          | 生效星期 (1-7, 1=周一)                                                                   | `[1..7]`                                          | `null`   |
| playlists[].items                                   | Array          | 播放项数组                                                                               | Non-empty                                         | N/A      |
| playlists[].items[].id                              | String         | 播放项唯一标识                                                                           | UUID format                                       | N/A      |
| playlists[].items[].type                            | String         | 播放项类型：`image`-图片，`slideshow`-幻灯片，`website`-网站，`video`-视频，`clock`-时钟 | `image`, `slideshow`, `website`, `video`, `clock` | N/A      |
| playlists[].items[].duration                        | Number         | 播放时长（秒）                                                                           | `0 - 86400`                                       | `60`     |
| playlists[].items[].sort                            | Number         | 播放顺序                                                                                 | Non-negative                                      | `0`      |
| playlists[].items[].settings                        | Object         | 播放项设置（根据 type 不同结构不同）                                                     | N/A                                               | N/A      |
| playlists[].items[].settings.urls                   | Array          | 图片 URL 数组（`image` 类型）                                                            | Non-empty                                         | N/A      |
| playlists[].items[].settings.delaySeconds           | Number         | 播放间隔秒数（`image` 类型，每个图片显示的时长）                                         | `> 0`                                             | `5`      |
| playlists[].items[].settings.url                    | String         | 资源 URL（`slideshow`, `video`, `website` 类型）                                         | Valid URL                                         | N/A      |
| playlists[].items[].settings.expiresAt              | Number         | URL 过期时间（可选，`image`, `slideshow`, `video` 类型，`null` 表示永不过期）            | Unix timestamp or `null`                          | `null`   |
| playlists[].items[].settings.muted                  | Boolean        | 是否静音播放（`video` 类型）                                                             | `true`, `false`                                   | `false`  |
| playlists[].items[].settings.ignoreCertificateError | Boolean        | 忽略证书错误（`website` 类型）                                                           | `true`, `false`                                   | `false`  |
| playlists[].items[].settings.refreshIntervalSecs    | Number or null | 刷新间隔秒数（`website` 类型），`null` 表示不刷新                                        | `> 0` or `null`                                   | `null`   |
| playlists[].items[].settings.clocks                 | Array          | 时钟列表（`clock` 类型）                                                                 | Non-empty                                         | N/A      |
| playlists[].items[].settings.clocks[].timezone      | String         | 时区 IANA 标识（如 `Asia/Shanghai`）                                                     | IANA timezone                                     | N/A      |
| playlists[].items[].settings.clocks[].label         | String         | 城市标签                                                                                 | Non-empty                                         | N/A      |

**响应字段：**

| 字段名 | 类型    | 描述         |
| ------ | ------- | ------------ |
| ok     | Boolean | 操作成功标识 |

### [指令] GetPlaylistConfig

- **功能描述**: 获取设备当前播放列表配置。
- **状态**: 已研发
- **调用方向**: Server -> Device，Device -> Server
- **请求参数**: 无

**响应字段：**

| 字段名                                              | 类型           | 描述                                                                                     |
| --------------------------------------------------- | -------------- | ---------------------------------------------------------------------------------------- |
| playlists                                           | Array          | 当前播放列表配置数组                                                                     |
| playlists[].id                                      | String         | 播放列表唯一标识                                                                         |
| playlists[].type                                    | String         | 播放列表类型：`default`-默认播放列表，`scheduled`-定时播放列表                           |
| playlists[].startDate                               | String         | 开始日期 (YYYY-MM-DD)                                                                    |
| playlists[].endDate                                 | String         | 结束日期 (YYYY-MM-DD)                                                                    |
| playlists[].startTime                               | String         | 开始时间 (HH:mm:ss)                                                                      |
| playlists[].endTime                                 | String         | 结束时间 (HH:mm:ss)                                                                      |
| playlists[].days                                    | Array          | 生效星期 (1-7, 1=周一)                                                                   |
| playlists[].items                                   | Array          | 播放项数组                                                                               |
| playlists[].items[].id                              | String         | 播放项唯一标识                                                                           |
| playlists[].items[].type                            | String         | 播放项类型：`image`-图片，`slideshow`-幻灯片，`website`-网站，`video`-视频，`clock`-时钟 |
| playlists[].items[].duration                        | Number         | 播放时长（秒）                                                                           |
| playlists[].items[].sort                            | Number         | 播放顺序                                                                                 |
| playlists[].items[].settings                        | Object         | 播放项设置（根据 type 不同结构不同）                                                     |
| playlists[].items[].settings.urls                   | Array          | 图片 URL 数组（`image` 类型）                                                            |
| playlists[].items[].settings.delaySeconds           | Number         | 播放间隔秒数（`image` 类型）                                                             |
| playlists[].items[].settings.url                    | String         | 资源 URL（`slideshow`, `video`, `website` 类型）                                         |
| playlists[].items[].settings.expiresAt              | Number         | URL 过期时间（可选，`image`, `slideshow`, `video` 类型，`null` 表示永不过期）            |
| playlists[].items[].settings.muted                  | Boolean        | 是否静音播放（`video` 类型）                                                             |
| playlists[].items[].settings.ignoreCertificateError | Boolean        | 忽略证书错误（`website` 类型）                                                           |
| playlists[].items[].settings.refreshIntervalSecs    | Number or null | 刷新间隔秒数（`website` 类型），`null` 表示不刷新                                        |
| playlists[].items[].settings.clocks                 | Array          | 时钟列表（`clock` 类型）                                                                 |
| playlists[].items[].settings.clocks[].timezone      | String         | 时区 IANA 标识                                                                           |
| playlists[].items[].settings.clocks[].label         | String         | 城市标签                                                                                 |

### [指令] GetPlaylistItemUrl

- **功能描述**: 根据播放项 ID 获取最新资源 URL（用于资源 URL 即将过期时刷新）。
- **状态**: 已研发
- **调用方向**: Device -> Server
- **请求参数**:
  ```json
  {
    "itemId": "uuid-2"
  }
  ```
- **响应结果**:

image 类型示例：

```json
{
  "urls": ["https://example.com/resource/file-1-new.jpg"],
  "expiresAt": 1704153600
}
```

video 类型示例：

```json
{
  "url": "https://example.com/resource/video-1-new.mp4",
  "expiresAt": 1704153600
}
```

- **备注**: 当设备检测到资源 URL 即将过期时，主动调用此指令获取新的 URL。响应只包含 `url` 或 `urls` 字段（二选一），设备根据非空字段判断使用哪个。

**请求字段：**

| 字段名 | 类型   | 描述           | 值限制      | 默认行为 |
| ------ | ------ | -------------- | ----------- | -------- |
| itemId | String | 播放项唯一标识 | UUID format | N/A      |

**响应字段：**

| 字段名    | 类型   | 描述                                             |
| --------- | ------ | ------------------------------------------------ |
| url       | String | 资源 URL（slideshow, video, website 类型）       |
| urls      | Array  | 资源 URL 数组（image 类型）                      |
| expiresAt | Number | URL 过期时间（Unix 时间戳，`null` 表示永不过期） |

---

## 7. 外观设置 (Appearance)

这些指令用于管理设备的外观配置。

### [指令] GetAppearanceConfig

- **功能描述**: 获取设备外观配置。
- **状态**: 已研发
- **调用方向**: Server -> Device, Device -> Server
- **请求参数**: 无
- **响应结果**:
  ```json
  {
    "panelLayout": "sidebar",
    "autoHidePanel": false,
    "autoHideDelay": 5
  }
  ```

**响应字段：**

| 字段名        | 类型    | 描述                   | 值限制             | 默认值    |
| ------------- | ------- | ---------------------- | ------------------ | --------- |
| panelLayout   | String  | 面板布局模式           | `focus`, `sidebar` | `sidebar` |
| autoHidePanel | Boolean | 自动隐藏面板开关       | `true`, `false`    | `false`   |
| autoHideDelay | Number  | 自动隐藏延迟时间（秒） | `> 0`              | `5`       |

### [指令] SetAppearanceConfig

- **功能描述**: 设置设备外观配置。
- **状态**: 已研发
- **调用方向**: Server -> Device, Device -> Server
- **请求参数**:
  ```json
  {
    "panelLayout": "sidebar",
    "autoHidePanel": false,
    "autoHideDelay": 5
  }
  ```
- **响应结果**:
  ```json
  { "ok": true }
  ```

**请求字段：**

| 字段名        | 类型    | 描述                   | 值限制             | 默认行为 |
| ------------- | ------- | ---------------------- | ------------------ | -------- |
| panelLayout   | String  | 面板布局模式           | `focus`, `sidebar` | 必填     |
| autoHidePanel | Boolean | 自动隐藏面板开关       | `true`, `false`    | 必填     |
| autoHideDelay | Number  | 自动隐藏延迟时间（秒） | `> 0`              | 必填     |

---

## 8. 更新设置 (Update)

这些指令用于管理设备的自动更新配置。

### [指令] GetUpdateConfig

- **功能描述**: 获取设备更新配置。
- **状态**: 已研发
- **调用方向**: Server -> Device, Device -> Server
- **请求参数**: 无
- **响应结果**:
  ```json
  {
    "autoUpdate": true,
    "autoUpdateWindow": {
      "start": "02:00",
      "end": "06:00"
    },
    "channel": "release"
  }
  ```

**响应字段：**

| 字段名                 | 类型    | 描述             | 值限制                     | 默认值    |
| ---------------------- | ------- | ---------------- | -------------------------- | --------- |
| autoUpdate             | Boolean | 自动更新开关     | `true`, `false`            | `true`    |
| autoUpdateWindow       | Object  | 自动更新时间窗口 | N/A                        | N/A       |
| autoUpdateWindow.start | String  | 开始时间         | `HH:mm` 格式               | `02:00`   |
| autoUpdateWindow.end   | String  | 结束时间         | `HH:mm` 格式               | `06:00`   |
| channel                | String  | 更新通道         | `release`, `beta`, `alpha` | `release` |

### [指令] SetUpdateConfig

- **功能描述**: 设置设备更新配置。
- **状态**: 已研发
- **调用方向**: Server -> Device, Device -> Server
- **请求参数**:
  ```json
  {
    "autoUpdate": true,
    "autoUpdateWindow": {
      "start": "02:00",
      "end": "06:00"
    },
    "channel": "release"
  }
  ```
- **响应结果**:
  ```json
  { "ok": true }
  ```

**请求字段：**

| 字段名                 | 类型    | 描述             | 值限制                     | 默认行为 |
| ---------------------- | ------- | ---------------- | -------------------------- | -------- |
| autoUpdate             | Boolean | 自动更新开关     | `true`, `false`            | 必填     |
| autoUpdateWindow       | Object  | 自动更新时间窗口 | N/A                        | 必填     |
| autoUpdateWindow.start | String  | 开始时间         | `HH:mm` 格式               | 必填     |
| autoUpdateWindow.end   | String  | 结束时间         | `HH:mm` 格式               | 必填     |
| channel                | String  | 更新通道         | `release`, `beta`, `alpha` | 必填     |

---

## 9. 计划任务 (Schedule)

这些指令用于管理设备的定时任务配置（定时关机/重启）。

### [指令] GetScheduleConfig

- **功能描述**: 获取设备计划任务配置（定时关机/重启）。
- **状态**: 已研发
- **调用方向**: Server -> Device, Device -> Server
- **请求参数**: 无
- **响应结果**:
  ```json
  {
    "shutdown": {
      "enabled": false,
      "time": "22:00",
      "days": [1, 2, 3, 4, 5, 6, 7]
    },
    "reboot": {
      "enabled": false,
      "time": "04:00",
      "days": [1, 2, 3, 4, 5, 6, 7]
    }
  }
  ```

**响应字段：**

| 字段名           | 类型    | 描述             | 值限制           | 默认值            |
| ---------------- | ------- | ---------------- | ---------------- | ----------------- |
| shutdown         | Object  | 定时关机配置     | N/A              | N/A               |
| shutdown.enabled | Boolean | 是否启用定时关机 | `true`, `false`  | `false`           |
| shutdown.time    | String  | 关机时间         | `HH:mm` 格式     | `22:00`           |
| shutdown.days    | Array   | 执行星期         | `[1..7]`, 1=周一 | `[1,2,3,4,5,6,7]` |
| reboot           | Object  | 定时重启配置     | N/A              | N/A               |
| reboot.enabled   | Boolean | 是否启用定时重启 | `true`, `false`  | `false`           |
| reboot.time      | String  | 重启时间         | `HH:mm` 格式     | `04:00`           |
| reboot.days      | Array   | 执行星期         | `[1..7]`, 1=周一 | `[1,2,3,4,5,6,7]` |

### [指令] SetScheduleConfig

- **功能描述**: 设置设备计划任务配置（定时关机/重启）。
- **状态**: 已研发
- **调用方向**: Server -> Device, Device -> Server
- **请求参数**:
  ```json
  {
    "shutdown": {
      "enabled": false,
      "time": "22:00",
      "days": [1, 2, 3, 4, 5, 6, 7]
    },
    "reboot": {
      "enabled": false,
      "time": "04:00",
      "days": [1, 2, 3, 4, 5, 6, 7]
    }
  }
  ```
- **响应结果**:
  ```json
  { "ok": true }
  ```

**请求字段：**

| 字段名           | 类型    | 描述             | 值限制           | 默认行为 |
| ---------------- | ------- | ---------------- | ---------------- | -------- |
| shutdown         | Object  | 定时关机配置     | N/A              | 必填     |
| shutdown.enabled | Boolean | 是否启用定时关机 | `true`, `false`  | 必填     |
| shutdown.time    | String  | 关机时间         | `HH:mm` 格式     | 必填     |
| shutdown.days    | Array   | 执行星期         | `[1..7]`, 1=周一 | 必填     |
| reboot           | Object  | 定时重启配置     | N/A              | 必填     |
| reboot.enabled   | Boolean | 是否启用定时重启 | `true`, `false`  | 必填     |
| reboot.time      | String  | 重启时间         | `HH:mm` 格式     | 必填     |
| reboot.days      | Array   | 执行星期         | `[1..7]`, 1=周一 | 必填     |

---

## 10. 日志管理 (Log Management)

这些指令用于管理设备日志的收集与上传。

### [指令] RequestLogUpload

- **功能描述**: 请求设备打包日志并上传到 OSS。
- **状态**: 未研发
- **调用方向**: Server -> Device
- **请求参数**: 无
- **响应结果**:
  ```json
  { "ok": true }
  ```

### [指令] NotifyLogUploadResult

- **功能描述**: 设备通知日志上传结果。
- **状态**: 未研发
- **调用方向**: Device -> Server
- **请求参数**:
  ```json
  {
    "url": "https://bucket.oss-cn-hangzhou.aliyuncs.com/logs/..."
  }
  ```
- **响应结果**:
  ```json
  { "ok": true }
  ```

**请求字段：**

| 字段名 | 类型   | 描述     | 值限制    | 默认行为 |
| ------ | ------ | -------- | --------- | -------- |
| url    | String | 日志地址 | Valid URL | 必填     |
