# Device SDK 业务指令与事件设计

本文档定义了具体的业务指令（RPC）与事件报文格式，以及如何添加新的业务指令和事件。底层协议细节请参阅 [API.md](file:///Users/was/Desktop/Lichenghuan/code/saas-platform/packages/device-sdk/docs/API.md)，SDK 使用说明请参阅 [SDK.md](file:///Users/was/Desktop/Lichenghuan/code/saas-platform/packages/device-sdk/docs/SDK.md)。

## 目录

- [核心概念](#核心概念)
- [添加新业务指令的流程](#添加新业务指令的流程)
- [业务指令（Commands/RPC）](#业务指令commandsrpc)
- [业务事件（Events）](#业务事件events)
- [完整示例](#完整示例)
- [最佳实践](#最佳实践)

## 核心概念

Device SDK 支持两种通信模式：

### Command (指令)

指令采用**请求-响应模式**，用于需要确认结果的操作：

- **DeviceToServerCommands**: 设备向服务端发送的指令（如获取配对码、上传日志）
- **ServerToDeviceCommands**: 服务端向设备发送的指令（如重启设备、更新固件）

**特点:**

- 使用 `sdk.call()` 调用，返回 Promise
- 使用 `sdk.handle()` 注册处理器
- 必须定义 `result` 类型，可选定义 `params` 类型

### Event (事件)

事件采用**单向通知模式**，用于状态上报和通知：

- **DeviceToServerEvents**: 设备向服务端发送的事件（如遥测数据、告警通知）
- **ServerToDeviceEvents**: 服务端向设备发送的事件（如绑定状态变更、配置更新）

**特点:**

- 使用 `sdk.notify()` 发送，无返回值
- 使用 `sdk.on()` 监听
- 只需定义 `data` 类型

### 对比表格

| 特性         | Command (指令)      | Event (事件)    |
| ------------ | ------------------- | --------------- |
| 通信模式     | 请求-响应           | 单向通知        |
| 是否等待返回 | 是 (`await call()`) | 否 (`notify()`) |
| 类型定义     | `params` + `result` | `data`          |
| 注册方式     | `sdk.handle()`      | `sdk.on()`      |
| 调用方式     | `sdk.call()`        | `sdk.notify()`  |
| 使用场景     | 需要确认结果的操作  | 状态上报、通知  |

## 添加新业务指令的流程

### 步骤 1: 定义协议类型

在 `src/protocol/contract.ts` 中定义新的指令或事件类型。

#### 添加指令示例

```typescript
// 设备向服务端发送的指令
export type DeviceToServerCommands = {
  GetBindCode: { params?: void; result: { code: string; expiresAt: number } };

  // 新增：设备请求配置
  GetDeviceConfig: {
    params: { configKey: string };
    result: { value: string };
  };

  // 新增：设备上传日志
  UploadLogs: {
    params: { logs: string[]; level: 'info' | 'error' };
    result: { uploaded: number; logId: string };
  };
};

// 服务端向设备发送的指令
export type ServerToDeviceCommands = {
  Restart: { params?: { reason?: string }; result: { ok: true } };

  // 新增：固件更新
  UpdateFirmware: {
    params: { version: string; url: string };
    result: { status: 'started' | 'failed' };
  };
};
```

#### 添加事件示例

```typescript
// 设备向服务端发送的事件
export type DeviceToServerEvents = {
  OnTelemetryReport: { data: { temp: number; battery: number } };

  // 新增：告警触发
  OnAlertTriggered: {
    data: { alertType: string; severity: 'low' | 'high' };
  };
};

// 服务端向设备发送的事件
export type ServerToDeviceEvents = {
  OnBindState: { data: { status: 'success' | 'failed'; code: string; message?: string } };

  // 新增：配置更新通知
  OnConfigUpdated: {
    data: { configKey: string; newValue: string };
  };
};
```

**类型定义规则:**

- 指令必须定义 `result` 类型
- `params` 和 `data` 可选，使用 `?` 标记
- 使用 TypeScript 类型确保类型安全

### 步骤 2: 设备端实现

#### 场景 A: 设备处理来自服务端的指令

```typescript
import { createDeviceSdk } from '@saas-platform/device-sdk';

const sdk = createDeviceSdk({
  url: 'ws://localhost:8080',
  deviceId: 'device-1',
});

// 注册指令处理器
sdk.handle('UpdateFirmware', async (ctx, params) => {
  console.log('[device] 收到固件更新指令', params.version, params.url);

  try {
    await downloadAndInstallFirmware(params.url);
    return { status: 'started' };
  } catch (error) {
    return { status: 'failed' };
  }
});

await sdk.connect();
```

#### 场景 B: 设备主动调用服务端指令

```typescript
// 设备请求配置
const config = await sdk.call('GetDeviceConfig', {
  configKey: 'update_interval',
});
console.log('[device] 获取配置', config.value);

// 设备上传日志
const result = await sdk.call('UploadLogs', {
  logs: ['启动成功', '连接服务器'],
  level: 'info',
});
console.log(`[device] 上传了 ${result.uploaded} 条日志`);
```

#### 场景 C: 设备发送和监听事件

```typescript
// 发送告警事件
sdk.notify('OnAlertTriggered', {
  alertType: 'temperature',
  severity: 'high',
});

// 监听配置更新事件
sdk.on('OnConfigUpdated', (ctx, data) => {
  console.log('[device] 配置已更新', data.configKey, data.newValue);
  applyConfig(data.configKey, data.newValue);
});
```

### 步骤 3: 服务端实现

#### 场景 A: 服务端处理来自设备的指令

```typescript
import { createServerSdk } from '@saas-platform/device-sdk';

const sdk = createServerSdk({});

// 注册指令处理器
sdk.handle('GetDeviceConfig', ({ sid, deviceId, data }) => {
  console.log('[server] 设备请求配置', deviceId, data.configKey);
  const value = getConfigFromDB(deviceId, data.configKey);
  return { value };
});

sdk.handle('UploadLogs', ({ sid, deviceId, data }) => {
  const logId = saveLogsToDatabase(deviceId, data.logs, data.level);
  return { uploaded: data.logs.length, logId };
});

await sdk.listen(8080);
```

#### 场景 B: 服务端主动向设备下发指令和事件

```typescript
// 向设备下发固件更新指令
const result = await sdk.call(sid, 'UpdateFirmware', {
  version: '2.0.0',
  url: 'https://cdn.example.com/firmware.bin',
});

// 通知设备配置已更新
sdk.notify(sid, 'OnConfigUpdated', {
  configKey: 'update_interval',
  newValue: '3600',
});
```

#### 场景 C: 服务端监听设备事件

```typescript
// 监听设备告警
sdk.on('OnAlertTriggered', ({ sid, deviceId, data }) => {
  console.log('[server] 收到告警', deviceId, data.alertType);
  if (data.severity === 'high') {
    sendAlertNotification(deviceId, data.alertType);
  }
});
```

## 业务指令（Commands/RPC）

### [指令] GetBindCode

- **method**: `GetBindCode`
- **说明**: 获取设备绑定码（原配对码）。
- **请求参数**: 无
- **响应结果**: `{ code: string, expiresAt: number }`

---

## 业务事件（Events）

### [事件] OnBindState

- **事件名**: `OnBindState`
- **说明**: 绑定状态变更通知。
- **data**: `{ status: 'success' | 'failed', code: string, message?: string }`

## 事件回调约定

- 所有事件回调只接收单一 payload，如需传多个值请使用对象封装。

## 完整示例

### 设备日志上报功能

#### 1. 定义协议 (contract.ts)

```typescript
export type DeviceToServerCommands = {
  UploadLogs: {
    params: {
      logs: Array<{ timestamp: number; level: string; message: string }>;
      deviceInfo: { version: string; model: string };
    };
    result: {
      uploaded: number;
      logId: string;
      nextUploadTime: number;
    };
  };
};
```

#### 2. 设备端实现

```typescript
const sdk = createDeviceSdk({ url: 'ws://server.example.com', deviceId: 'device-001' });

class LogCollector {
  private logs = [];

  add(level: string, message: string) {
    this.logs.push({ timestamp: Date.now(), level, message });
    if (this.logs.length >= 100) this.upload();
  }

  async upload() {
    if (this.logs.length === 0) return;
    const result = await sdk.call('UploadLogs', {
      logs: this.logs,
      deviceInfo: { version: '1.0.0', model: 'IoT-X' },
    });
    console.log(`上传 ${result.uploaded} 条日志, ID: ${result.logId}`);
    this.logs = [];
  }
}

const logger = new LogCollector();
logger.add('info', '设备启动');
```

#### 3. 服务端实现

```typescript
const sdk = createServerSdk({});

sdk.handle('UploadLogs', ({ sid, deviceId, data }) => {
  const logId = saveLogsToDatabase({ deviceId, logs: data.logs });

  const errorCount = data.logs.filter(log => log.level === 'error').length;
  if (errorCount > 0) triggerAlert(deviceId, errorCount);

  return {
    uploaded: data.logs.length,
    logId,
    nextUploadTime: Date.now() + 60000,
  };
});

await sdk.listen(8080);
```

## 最佳实践

### 1. 选择合适的通信模式

- **使用 Command**: 需要确认结果、获取返回数据、处理错误
- **使用 Event**: 定期上报状态、发送通知、不关心对方是否收到

### 2. 错误处理

```typescript
// 指令调用应该处理错误
try {
  const result = await sdk.call('UploadLogs', { ... });
} catch (error) {
  console.error('上传失败', error);
  // 重试逻辑
}

// 事件发送不需要错误处理
sdk.notify('OnTelemetryReport', { temp: 25 }); // fire-and-forget

```

### 3. 类型安全

```typescript
// ✅ 正确：类型安全
const result = await sdk.call('UploadLogs', {
  logs: [{ timestamp: Date.now(), level: 'info', message: 'test' }],
  deviceInfo: { version: '1.0.0', model: 'X' },
});

// ❌ 错误：编译时报错
await sdk.call('UploadLogs'); // 缺少参数
await sdk.call('UploadLogs', { logs: 'invalid' }); // 类型错误
```

### 4. 性能优化

- 批量上传日志而不是逐条上传
- 设置合理的超时时间
- 使用事件进行高频数据上报

## 相关文件

- `src/protocol/contract.ts` - 协议类型定义
- `src/sdk.ts` - SDK 主入口
- [API 文档](./API.md) - 底层协议细节
- [SDK 文档](./SDK.md) - SDK 使用说明
- [Commands_RPC 文档](./Commands_RPC.md) - 常用 RPC 指令参考
