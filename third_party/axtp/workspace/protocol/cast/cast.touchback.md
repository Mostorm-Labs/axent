---
status: generated
contract: true
generated: true
domain: cast
feature: cast.touchback
registry: ../../../contract/registry/domains/cast/domain.yaml
lastReviewed: 2026-11-05
---

# cast.touchback：统一开关与 Typed Input R2 产品控制合同

本轮是用户确认的 draft 合同破坏性修订；机器事实与生成物按 R2 一致更新。
旧正文撤下并由 Git e986dcf 追溯。完整输入字段与流程见
[input.stream R2](../input/input.stream.md)、[flow](../../flows/nearcast-touchback-na20-nt10.md)。
本文控制产品授权和可见状态，不在 Axent Core/Runtime 加 NearCast 产品策略。

## 1. API 保留、移除与兼容边界

| 控制入口 | 本轮行为 |
|---|---|
| cast.getTouchbackState | 保留方法/ID，查询精确 target 的完整当前快照 |
| cast.setTouchbackEnabled | 保留方法/ID，只改 controllerRequested，受本地 gate 限制 |
| cast.setTouchbackResolutionPreference | 保留，inherit/prefer1080p 与输入类型独立 |
| cast.touchbackChanged | 保留事件/ID，发布 owner 状态、允许类型、stream 与原因 |
| cast.getControlCapabilities/getStatus | 同步嵌套类型、能力与当前快照 |
| 旧模式设置入口 | 从当前方法表/schema/能力生成中退役，ID/fieldId 不复用 |

本修订不交付旧协议双栈。新 input format 需独立协商；旧 producer 不能继续用
原布局发送。不认识的方法返回标准 RPC_METHOD_NOT_FOUND；注册但当前实现
不支持的调用按 NOT_SUPPORTED 处理，媒体继续。具体登记错误不在产品中自造。

## 2. 身份、CAS 和幂等

所有逐路操作验证 runtimeInstanceId 和完整 target：sessionId、sessionGeneration、
slotId、bindingGeneration、lifecycleGeneration。只匹配 slot、投屏码或显示名不足。
请求 commandId 绑定认证调用者/目标/内容，CAS revision 若使用则比较当前 owner；
同键异内容拒绝，旧成功也不能在新绑定上生效。

快照与事件始终包含完整 target 和当前 revision。旧事件不覆盖新状态；事件丢失
可通过 getTouchbackState/getStatus 恢复。订阅完成后取快照并按 revision 收敛；
重连不恢复现场授权或旧 pending，必须重新验证当前目标与来源。

## 3. 状态模型

| 字段 | 类型/内容 | 含义 |
|---|---|---|
| receiverOptIn | bool | RX 现场许可，默认 false，不持久化 |
| controllerRequested | bool | 控制端启用意图，不等于生效 |
| effectiveEnabled | bool | 当前 Active 且所有授权/能力/绑定仍满足 |
| lifecycleState | inactive/opening/active/closing/faulted/cleanup_blocked | 真实 owner 生命周期 |
| acceptedInputTypes | touch/mouse 集合 | Active 时实际允许，非 Active 对外为空 |
| streamId | optional u32 | 活动流或待清理流；非零，同认证 session 不复用 |
| format | optional string | 本次实际 open 格式，input-sample-v1 |
| resolutionPreference | inherit/prefer1080p | 源质量意图 |
| resolutionState | bounded 状态/原因 | 实际接受、降级、不支持、pending 等 |
| canvasRevision/geometryRevision | 本地/控制面版本 | 呈现/映射诊断；不逐 input 包传 token |
| revision/diagnosticReason | 当前修订与有界原因 | 用于事件/快照和 UI 收敛 |

字段名与 schema fieldId 见本轮生成合同；非 Active 仍可保留 streamId 用于
清理，不得因它存在而允许采集。effective=false 不能宣称 native 已释放。
能力软件类型、输入允许类型和 TX 输出策略分别展示，不再使用单 stream 固定模式。

## 4. gate 和写入响应

```text
mayOpen = receiverOptIn && controllerRequested
       && currentTarget && presented && protocolSupportsTouchback
       && authenticatedSourceBinding && transportAPP_READY
effectiveEnabled = mayOpen && currentOpenPrepared && nativeReady && mappingReady
```

本地 UI 和控制端写入都进入同一 owner lane。setTouchbackEnabled 的 accepted
仅表示设置意图被接受，响应同时给当前快照；不能将其视为 input.openStream 的
native prepared 成功。UI 至少显示 intent、pending、effective 和失败/清理原因。

从 false 到 true：准备来源/能力/映射并 open，成功且仍属于当前 generation 才
Active。从 true 到 false：先推进 generation、停采集入队并置 closing，再异步
close/release；晚 open 结果不能重新激活。重试不补发已经按住的手指/按钮。

## 5. 状态转移

| 起点/触发 | 同步动作 | 后续结果 |
|---|---|---|
| inactive + 双 gate 成立 | 记录当前 target/generation，opening | prepare/open 成功才 active |
| opening + gate/target 变更 | fence pending，停止激活资格 | 晚资源清理，inactive/blocked |
| active + gate 关闭 | effective=false，closing，隔离队列 | native 清理确认后 inactive |
| active + native/权限/几何故障 | 停输入，faulted，触发 close | 明确恢复后新 ID；失败 blocked |
| closing + 清理不明 | cleanup_blocked，不允许新 owner | 同身份重试清理或明确恢复 |
| active + 已允许类型交错 | 不改变 gate/上下文 | Touch/Mouse 生命周期分别维护 |

所有 open/close/prepare 和队列等待有界；清理不能排在无界输入后。初版 TX native
域独占，另一 RX target 请求反控可 BUSY，但媒体继续。接收端本地多路采集目标
锁定仍须逐路隔离，不能靠全局 disabled 清除其它 owner。

## 6. 呈现、坐标与源质量

RX canvas 和 letterbox 属于 RX，归一坐标与 TX 真实采集/输入区域的映射在控制
上下文确认。sourceBindingId 由认证媒体/设备来源描述提供，RX open 带回，TX 验证本机实际绑定；
不直接相信客户端自报窗口/尺寸。网络 input 包不重复 canvas/geometry/source/device。

纯 RX 显示变换不必重开，但受影响接触须取消且不能跨 target。TX 来源/区域/
方向/单位或权限改变时旧 context 失效：停输入、释放、应用并验证新几何、新 open。
各端保留本地 generation，旧回调不能在新几何执行。

inherit 保持当前源质量；prefer1080p 请求更高编码质量，TX 可以降级/拒绝。
状态区分 requested/applied/failed/unsupported，放大 RX canvas 不能证明源升级。
仅接受重配置请求不能提前改变输入映射；重建失败应给出实际来源/几何状态。

## 7. 能力与平台事实

cast.touchback 声明实际可用路径、逐路控制和 RX 采集类型。初版 RX 只有真实
Touch，不宣称 Mouse/Pen 采集。TX input.stream 的 decoder/native 类型及约束是
另一个事实，允许集合在 open 求交集。发现设备清单不等于软件能力。

AirPlay 省略反控能力且不打开 input；不支持类型/权限/几何时报告原因，普通媒体
持续。macOS mouse/touch-scroll 为已有明确转换实现，不能被 native Touch 能力
冒名声明。未来 force-touch/force-mouse 作为 TX 显式策略单独设计/验证。

## 8. 关闭、通用 stream 状态与诊断

input.closeStream 负责输入终点的所有权释放；cast.touchbackChanged 报告 RX
产品 owner 状态，stream.stateChanged/getState 报告终点通用状态。AXTP-TYPED-INPUT-V1 要求
复用状态子集，不因此开启 ACK/window/pause 或新增 input 私有 Reset 控制包。

接收非 streaming 事件先停可疑输入；NOT_FOUND、通道断开、消息已转发不能证明
native 清理。cleanupPending 必须保留，关闭未确认不得假报 inactive/clean。
重连依据快照/当前认证和新 open 恢复，不重用旧 ID。

诊断至少包括 target/revision、合同指纹、streamId、本地 generation、允许类型、
native readiness、几何、队列、失败阶段及 cleanup。不得记录认证密钥；received/
validated/submitted/application observed 分开。标准错误按已登记集合使用。

## 9. 采纳任务与验收

1. 移除旧模式 method/schema/fields/trigger 与当前默认示例，并保留 ID 退役记录；
   其它 cast.presentation 和媒体方法不顺带改语义。
2. 更新 getState/changed/capability/getStatus 嵌套 schema，精确冻结新字段和错误，
   与 input R2 同一 source/generated fingerprint。
3. 验证 gate 四组合、pending/晚结果、CAS/幂等、旧 target/slot 复用、两路隔离、
   来源/几何变化、权限/native 失败、blocked 清理和重连快照。
4. 验证 UI 统一开关、AirPlay 排除、质量偏好独立；真实 Touch 结果与媒体持续。

本轮采用 R2 字段并同步生成；Runtime/Axent/RX/TX 必须消费相同 fingerprint。
生成物只通过工具产生。生产采集/本机注入迁移与协议采纳分别验收。

## 10. 修订记录

2026-11-05，基于用户确认的 R2 替换 e986dcf 单模式 draft；移除的 method/mask/
字段槽保留退役元数据，不保留旧当前正文，不重编号未变方法。正式输入规范见
[48-input-stream](../../../specs/48-input-stream.md)。
