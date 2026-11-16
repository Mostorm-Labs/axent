# VM33 Protocol Migration Plan

> Status: migration design
> Scope: VM33 legacy preservation and VM33 Pro AXTP-based evolution
> Source evidence: `workspace/legacy-migration/evidence/VM33协议文档.md`
> Classification reference: `workspace/legacy-migration/classification/by-source/vm33_http_json.md`

本文定义 VM33 到 AXTP 的迁移策略。结论是：老 VM33 迁移方案和旧协议实现不在本轮修改；VM33 Pro 出来后，新增业务和需要调整的业务逻辑通过新增 AXTP 业务协议逐步承接，设备端和 App 端同步适配。真正落地前，先完成新版本协议解析与业务框架代码，让后续新协议 method 可以按插件式 handler 注入。

## 1. 决策摘要

| 决策 | 结论 |
|---|---|
| 老 VM33 协议 | 保持现状，不修改旧 `Seq/Class/Method/Param`、HTTP/multipart 语义和现有迁移方案。 |
| VM33 Pro 新业务 | 通过 AXTP 新业务协议新增，不继续扩展旧 VM33 泛 `Config.*` 或散落的 `Class.Method`。 |
| 待修改业务 | 不在旧 VM33 method 上原地改语义；先起草 AXTP 协议，App 和设备双端适配后逐步替换旧逻辑。 |
| 协议事实源 | 新业务事实进入 `workspace/protocol/**` 草案，评审后进入 `contract/registry/domains/**`，再由 Generator 输出。 |
| 旧迁移产物 | `AXTP_Legacy_Migration_Matrix.xlsx`、`workspace/legacy-migration/generated/**` 和旧 VM33 分类材料保留为证据和参考，不作为 VM33 Pro 新协议事实源。 |
| 代码先行边界 | 先冻结 VM33 Pro 的协议解析、session、router、handler 注入、兼容 fallback 框架，再逐项注入新业务 method。 |
| App 策略 | App 按设备能力或版本选择 AXTP 新协议；旧 VM33 设备继续走旧协议。 |

## 2. 非目标

- 不修改 `workspace/legacy-migration/evidence/VM33协议文档.md`。
- 不修改已有老 VM33 迁移矩阵、旧分类产物或 `workspace/legacy-migration/generated/**`。
- 不把 VM33 旧 `Class.Method` 或 `Config.Name` 批量注册成 AXTP method。
- 不把旧 `Param` 结构直接当成 AXTP schema。
- 不要求本轮修改 `contract/registry/**/*.yaml`、`contract/protocol/axtp.protocol.yaml` 或 generated 产物。

## 3. 旧 VM33 协议边界

旧 VM33 协议是 HTTP JSON / multipart 风格：

```json
{
  "Seq": 1,
  "Class": "Config",
  "Method": "Set",
  "Param": {
    "Name": "Wifi",
    "Config": {
      "ssid": "this_wifi",
      "key": "12345678"
    }
  }
}
```

响应外层通常为：

```json
{
  "code": 0,
  "msg": "success",
  "data": {
    "Seq": 1,
    "Class": "Config",
    "Method": "Set",
    "Param": {
      "Name": "Wifi",
      "Result": true
    }
  }
}
```

旧协议实现应按原逻辑继续维护：

- `Seq` 是旧协议请求/响应关联 ID，保持 int 语义。
- `Class` / `Method` 继续按旧路由表分发。
- `Param` 保持灵活 JSON，由旧 VM33 handler 自行解释。
- `Config.Set/Get/MultiSet/MultiGet/Restore/Subscribe` 等泛配置入口不在本轮改造。
- multipart 中的 `json` part 和文件 part 规则保持现状。

## 4. 新 VM33 Pro 目标

VM33 Pro 的方向不是“修补旧 VM33 协议”，而是建立一个可注入 AXTP 新业务的框架：

```text
Transport
  -> AXTP frame or JSON RPC parser
  -> AXTP session / request router
  -> generated method registry lookup
  -> VM33 Pro business handler injection
  -> device service implementation
```

目标能力：

| 能力 | 要求 |
|---|---|
| 协议解析 | 支持 AXTP v1 JSON/Binary 解析边界；具体 transport 按产品选择。 |
| Session | 使用 AXTP Hello / Identify / Identified 或 Standard CONTROL + RPC session 规则。 |
| Router | 以 generated methodId / method name 路由，不以 VM33 `Class.Method` 路由。 |
| Handler 注入 | 每个 AXTP method 对应一个业务 handler，可按 domain/feature 模块注册。 |
| Capability | 通过 `capability.supportedMethods` 暴露当前固件可用能力。 |
| 兼容 fallback | 对旧 App 或旧设备保留 VM33 legacy parser，不混进 AXTP Core。 |

## 5. 分阶段迁移

### Phase 0: 旧协议稳定

保持现有 VM33 协议和老迁移方案不变：

- 不重命名旧 `Class`、`Method`、`Param.Name`。
- 不改变旧响应 `code/msg/data` 和 `Param.Result` 语义。
- 不要求旧 App 立即切换。
- 仅允许 bugfix 和兼容性修复，不借旧协议做新业务扩展。

### Phase 1: 新解析和业务框架

先完成 VM33 Pro 的协议底座：

| 模块 | 职责 |
|---|---|
| `Vm33ProProtocolParser` | 接收 AXTP JSON/Binary 或产品选定的 AXTP transport 输入，输出统一 request/event payload。 |
| `Vm33ProSessionManager` | 处理 Hello/Identify/Identified、session 恢复、超时和关闭。 |
| `Vm33ProMethodRouter` | 使用 generated registry 将 methodId / method name 映射到 handler。 |
| `Vm33ProHandlerRegistry` | 支持按 domain.feature 注入 handler，例如 `network.wifi`、`video.rtsp`、`firmware.update`。 |
| `Vm33LegacyBridge` | 只用于旧 VM33 fallback 或临时灰度，不作为新业务主路径。 |
| `Vm33CapabilityProvider` | 基于已注册 handler 输出 `capability.supportedMethods`。 |

完成标准：

- 空业务 handler 下，设备能完成 AXTP session 建立、能力查询和未知 method 错误响应。
- handler 注册/注销不需要改协议解析代码。
- parser 和 router 不依赖旧 `Class.Method`。

### Phase 2: 新业务协议注入

新增业务或修改业务逻辑时按 AXTP lifecycle 执行：

```text
需求 / 旧 VM33 证据
  -> workspace/protocol/<domain>/<domain.feature>.md 草案
  -> 评审确认 method/event/schema/error/capability
  -> contract/registry/domains/<domain>/domain.yaml
  -> Generator 输出 contract/generated/protocol.md/json 和 runtime generated headers
  -> VM33 Pro handler 注入
  -> App 适配新 method
```

每个新业务必须带：

- 业务语义和适用设备型号。
- 新 AXTP method/event/schema。
- 与旧 VM33 `Class.Method` 或 `Config.Name` 的关系：替代、并存、仅参考或无关。
- App 切换策略和 fallback 策略。
- 测试向量或端到端验收用例。

### Phase 3: 逐步替换旧逻辑

旧 VM33 逻辑只有在满足以下条件后才能下线：

- AXTP 新 method 已评审采纳并生成。
- VM33 Pro 固件已实现 handler。
- App 已发布支持新 method，并可识别设备能力。
- 老 App / 老固件 fallback 策略明确。
- 线上兼容窗口和回滚方案明确。

替换方式：

| 状态 | 行为 |
|---|---|
| `legacy_only` | 只有旧 VM33 method，保持旧逻辑。 |
| `dual_stack` | 新 AXTP method 与旧 VM33 method 并存；App 优先走 AXTP，失败可 fallback。 |
| `axtp_primary` | 新 AXTP method 是主路径；旧 VM33 method 只服务旧 App。 |
| `legacy_deprecated` | 旧 VM33 method 不再新增功能，只保留兼容窗口。 |
| `legacy_removed` | 在明确版本边界后移除旧业务 handler。 |

## 6. App 端适配

App 必须与设备端同步新增协议方案：

| 场景 | App 行为 |
|---|---|
| 连接 VM33 legacy 设备 | 继续使用旧 HTTP JSON / multipart 协议。 |
| 连接 VM33 Pro 设备 | 优先完成 AXTP session，调用 `capability.supportedMethods`。 |
| 新 method 可用 | 使用 AXTP 新 method 和 generated schema。 |
| 新 method 不可用 | 按产品策略 fallback 到旧 VM33 method 或提示固件升级。 |
| 灰度期间 | 记录新旧路径结果差异，避免静默改变业务语义。 |

App 不应通过旧 VM33 `Config.Name` 猜测 AXTP capability；能力判断以 generated protocol 和设备返回的 supported methods 为准。

## 7. 旧 VM33 到 AXTP 的参考关系

`workspace/legacy-migration/classification/by-source/vm33_http_json.md` 已把旧 VM33 条目归入候选 AXTP domain.feature。示例：

| 旧 VM33 条目 | 候选 AXTP 方向 | 迁移策略 |
|---|---|---|
| `Config.Set/Get:Wifi`、`Wifi.ScanWifi`、`Wifi.ConnectWifi` | `network.wifi` | VM33 Pro 新 Wi-Fi 能力走 AXTP `network.wifi` 草案/采纳流程。 |
| `Config.Set/Get:APInfo`、`Wifi.OpenApService` | `network.ap` | SoftAP 新需求走 AXTP `network.ap`。 |
| `Upgrade.Setup/Upgrade/Progress/CloudUpgrade` | `firmware.update` | 新的固件更新不继承 multipart 旧形态，按 AXTP `firmware.update` + STREAM 设计。 |
| `Rtsp.*` | `video.rtsp` | 新 RTSP 配置走 AXTP `video.rtsp`。 |
| `InputSource.*` | `room.source` | 新输入源管理走 AXTP `room.source`。 |
| `Meeting.*` | `room.schedule` | 新会议日程走 AXTP `room.schedule`。 |
| `ProductTest.*` | `diagnostic.*` | 新产测能力走 AXTP diagnostic domain。 |

这些分类是 intake/reference，不是自动采纳结果。每个进入 VM33 Pro 的新能力都必须重新评审业务语义、字段、错误码和兼容影响。

## 8. Handler 注入合同

VM33 Pro handler 注入建议固定为以下合同：

| 字段 | 规则 |
|---|---|
| method identity | 使用 generated methodId 或 generated method name。 |
| request schema | 使用 generated schema codec，不直接接收旧 VM33 `Param`。 |
| response schema | 使用 generated response schema 和 AXTP status/error。 |
| legacy reference | 可记录旧 `Class.Method` / `Config.Name`，但只作注释和测试溯源。 |
| capability exposure | handler 注册后由 `Vm33CapabilityProvider` 暴露。 |
| removal | handler 移除必须先走 deprecation / app readiness 检查。 |

不得让 handler 注入接口接受任意 `Class.Method + Param` 作为新业务入口；那会把 VM33 legacy 重新带进 VM33 Pro 主路径。

## 9. 测试计划

### 9.1 Framework fixtures

| fixture | 期望 |
|---|---|
| `vm33pro-session-ready` | 完成 AXTP session，进入 APP_READY。 |
| `vm33pro-supported-methods-empty` | 无业务 handler 时 capability 返回空或最小集合。 |
| `vm33pro-unknown-method` | 未注册 method 返回 AXTP method not found。 |
| `vm33pro-handler-injection` | 注入一个测试 method 后 capability 和 router 同步可见。 |
| `vm33pro-legacy-fallback` | 旧 VM33 请求仍由 legacy bridge 处理，不进入 AXTP router。 |

### 9.2 Per-business gates

每个新增 AXTP 业务进入 VM33 Pro 前必须通过：

- `workspace/protocol/**` 草案评审。
- YAML source validation。
- Generator build/test。
- 设备 handler 单测。
- App 新 method 调用测试。
- 旧 VM33 fallback 或下线策略验证。

## 10. 发布策略

建议按以下节奏推进：

1. **Framework first**：先发布 VM33 Pro 协议解析、session、router、capability 和 handler 注入框架，不携带大规模业务迁移。
2. **Business by business**：按业务价值逐个新增 AXTP 协议并注入，例如 Wi-Fi、RTSP、固件更新、输入源、会议日程。
3. **Dual-stack window**：App 和设备双栈运行一段时间，记录差异和 fallback。
4. **Deprecation**：确认 App 覆盖率和固件覆盖率后，标记旧 VM33 业务逻辑 deprecated。
5. **Removal**：到达版本边界后取消旧业务逻辑。

验收标准：

- 老 VM33 方案没有被本轮文档或代码改写。
- VM33 Pro 解析框架能在无业务状态下独立跑通。
- 新业务只通过 AXTP 草案/YAML/Generator 进入。
- App 有明确的新协议适配和 fallback 策略。
- 每个旧业务的取消都有对应 AXTP 新能力、版本边界和回滚说明。
