---
status: generated
contract: true
generated: true
domain: cast
feature: cast.receiverSettings
registry: contract/registry/domains/cast/domain.yaml
lastReviewed: 2026-11-20
---

# cast.receiverSettings

控制台与启动器通过此能力管理接收端的持久化启动偏好。接收端负责存储和应用，控制台读取实际结果。WS 端口与监听范围更新后，当前套接字和正在进行的投屏继续工作，下次启动采用已保存的配置。

## 能力与方法

先调用 `cast.getControlCapabilities`，以返回的 `methods` 列表识别运行实例提供的接口。`cast.getReceiverSettings` 返回的 `restartOnlyFields` 宣告该实例支持哪些重启项。早期 RX 可以监听 `7020` 且提供其他控制方法，但可能缺少这组接口或 WS 设置字段。控制台此时应显示产品默认值和版本要求，禁用这些字段的保存。

| 方法 | 注册 ID | 请求及结果 |
| --- | --- | --- |
| `cast.getReceiverSettings` | `0x1624` | 空对象 → 当前接收端偏好和持久化状态 |
| `cast.setReceiverSettings` | `0x1625` | 实例、存储版本、`persist`、单字段 `patch` → 保存、应用结果与更新后快照 |
| `cast.restoreReceiverSettings` | `0x1626` | 实例、存储版本及 `source` → 恢复结果与更新后快照 |

三个方法使用 AXTP JSON RPC。现有 RX 的 JSON 名称及字段沿用。`cast.setPresentationSettings` 继续管理画布、布局和窗口等设置并使用自己的设置版本和操作标识。

## WebSocket 设置

| 字段 | 类型 | 无保存值时默认 | 保存行为 |
| --- | --- | --- | --- |
| `controlPort` | 整数 `1–65535` | `7020` | `persist: true`，下次启动绑定该端口 |
| `controlListenOnLan` | 布尔值 | `false` | `persist: true`，下次启动选择本机或局域网监听 |

在 NearCast 的 IPv4 实现中，`false` 监听回环地址 `127.0.0.1`，`true` 监听所有本机 IPv4 地址 `0.0.0.0`。启动器可临时覆盖端口和绑定地址。后台写入不会改动本次启动的参数。

`configured` 返回下次启动的 WS 设置，包含有保存值时的覆盖或产品默认。`applied` 返回当前监听值。`persistedOverrides` 只列出存储中的显式覆盖，因而首次读取时端口可显示为 `7020` 且该对象为空。`pendingRestart` 列出与当前监听不同的字段。`airplayName`、`airplayRandomPassword` 继续通过现有偏好接口读写。

## 保存和重启流程

1. 控制台获取状态，确认字段受支持且 `supportsPersistence` 为 `true`。连接建立时填写的 Endpoint 仅用于当前客户端连接。
2. 保存单字段。请求使用最新状态给出的 `runtimeInstanceId` 和十进制字符串 `storeRevision`，后者作为 `expectedStoreRevision`。两个字段分别更新，下一次写入使用上一次成功结果或重新读取的新版本。
3. 确认收到 `persistenceStatus` 为 `stored` 或 `unchanged`。`applicationStatus: pendingRestart` 表示已保存且需要重启。保存与当前监听值相同的设置时，结果为 `unchanged`，仍可形成显式存储覆盖。
4. 操作者通过本机启动管理器重启接收端。启动器从接收端的实际就绪报告取得新地址和端口，连接后重新读取状态。端口被占用时报告启动失败，使用本机启动选项临时覆盖端口恢复连接。

### 保存端口示例

```json
{
  "runtimeInstanceId": "00000000-0000-4000-8000-000000000001",
  "expectedStoreRevision": "4",
  "persist": true,
  "patch": { "controlPort": 17020 }
}
```

例如此时 `7020` 正在使用，保存结果的 `settings.configured.controlPort` 为 `17020`，`settings.applied.controlPort` 为 `7020`，`settings.pendingRestart` 包含 `controlPort`。保存局域网监听使用同一流程，以 `{"controlListenOnLan": true}` 作为单字段 patch。

## 冲突、恢复和访问权限

- 端口越界、字段类型错误、空 patch、多字段 patch、未知字段或尝试临时设置这两个字段，返回 `INVALID_ARGUMENT`。
- 运行实例已变或存储版本冲突返回 `INVALID_STATE`。保留当前值，重读后由操作者再次提交。响应丢失也先读取当前值和存储版本。
- 恢复请求使用相同实例和版本字段，`source` 可选 `defaults` 或 `backup`。只有可用备份才可恢复。恢复影响接收端设置存储，重启项在下次启动应用。
- NearCast 允许从本机回环监听实例保存或恢复，局域网监听实例仍可返回只读快照。需要继续写入时，通过本机启动管理器临时切回回环监听，再提交保存。存储不可用或被占用时反馈实际失败。接口不返回实际 AirPlay 密码、令牌或本地设置文件路径。

规范见 [Cast settings](../../../specs/46-cast-settings.md#receiver-startup-preferences)。
