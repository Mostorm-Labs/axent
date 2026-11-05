---
status: flow-planned
contract: false
generated: false
domain: cast
feature: cast.touchback
registry: ""
lastReviewed: 2026-11-05
---

# Typed Input R2：WebCast 与 NA20/NT10 端到端流程

本文是与本轮正式合同对齐的业务 flow；机器字段以 registry/generated 为准，样例见
[input.stream R2](../protocol/input/input.stream.md)，门控见
[cast.touchback](../protocol/cast/cast.touchback.md)，目标见
[业务需求](../business/nearcast-touchback.md)。旧流程由 Git e986dcf 追溯。

## 1. 角色、方向和三层就绪

```text
媒体：TX ───────────────► NearCast RX
输入：TX native ◄─ InputEngine ◄─ AXTP/Axent ◄─ RX Touch/Mouse 采集
HID：NT10 input ◄─ 通用 session/stream route ◄─ NA20 ◄─ RX
```

RX 为 input producer/logical client，TX/NT10 为 consumer/logical server。
媒体发送者、DC 创建者与 RPC client 不是同一个定义。区分 transport connected、
AXTP APP_READY、input Active；前两者均不授予 native 注入权限。

## 2. 发现、信任和媒体关联

1. RX 按已有 NearCast mDNS 提供候选与能力摘要；名称/IP 不作为身份或授权。
2. TX 首次连接确认指纹，后续固定信任；投屏码继续按现场产品流程显示/校验。
3. TX 读取 cast.getControlCapabilities/getStatus，选择确切 target；AirPlay 无
   touchback capability，保持普通媒体，不尝试 open 输入。
4. 建立媒体时保存认证来源与 RX 完整 target 的关联。slot 复用、新 runtime 或
   session/binding/lifecycle generation 变化都不能沿用旧输入许可。
5. TX 对真实采集/输入区域生成绑定；认证媒体/设备来源描述提供 sourceBindingId，
   RX open 带回，TX 验证实际区域；缺少该描述就拒绝，禁止根据自报句柄猜测。

## 3. WebCast 标准承载建立

```text
RX producer                           TX consumer
  |--- 关联既有认证媒体的 control DC -----|
  |--- CONTROL OPEN -------------------->|
  |<-- CONTROL ACCEPT -------------------|
  |<-- RPC Hello ------------------------|
  |--- RPC Identify -------------------->|
  |<-- RPC Identified -------------------|
  |    APP_READY（还不能注入）            |
```

上图沿用 Core logical client/server 方向。AXTP-WEBRTC-DC 的 ordered/reliable DC 上 AXTP 全部
使用 binary Standard Frame；现有非 AXTP product text 继续原 handler。RPC JSON
仍需要 rpcEncoding byte 和 sid/op/d，不额外套私有 AXTP JSON。peer/session 固定绑定本次认证媒体与控制 DC 实例，
重连/peer 变化使旧 context 失效；复用 session 也必须满足本 profile 的身份与角色。READY 不变成额外必需握手，不启用
逐输入 ACK，也不改变媒体 RTP。

## 4. 能力、订阅和 gate

RX 声明实际可采集软件类型，TX 声明可解码类型和实际 native 功能。初版 RX
只请求 Touch，即使协议同时定义 Mouse。以后已经实现 Mouse 的 RX 可开流允许
两类，插入设备不再改变记录解释。无共同类型明确失败，native 不自动转换。

open 前订阅 stream.stateChanged 状态子集和 cast.touchbackChanged。前者用于
输入终点停流/清理状态，后者用于 RX 产品 owner；不能用一个覆盖另一个的所有权。
stream.getState 可恢复通用状态，但 NOT_FOUND/传输关闭不证明 native 已清理。

receiverOptIn 默认 false，controllerRequested 是控制 intent。任一不满足保持
inactive；启用请求可以被接受并处于 pending，effective 只有实际 Active 才 true。
等待期间出现 gate/target 变化提升 generation，取消该 pending；不能晚激活。

## 5. open/prepare 与当前请求确认

```text
RX target owner                       TX input owner
  | gate+target+binding+APP_READY         |
  |--- input.openStream ---------------->|
  |                                      | 验证身份/目标/来源
  |                                      | native probe + geometry + 独占
  |                                      | prepare，分配非零新 streamId
  |<-- 实际允许类型/约束，accepted=true ---|
  | 检查 pending/generation/gates          |
  | Active，只接收新生命周期              |
  |--- STREAM(seq=1,cursor,data) --------->| 校验→映射→提交
```

format/类型限额/maxDataBytes 在此确认，sourceBinding 不逐包发送。成功必须已准备好
消费；不能把“收到开流请求”当 accepted=true。第二个 native owner 返回 BUSY。
commandId 绑定调用者、目标和请求语义；同键异内容拒绝，旧成功不能复活已关闭流。
两端 prepare/open 有界超时，晚资源按旧 generation 清理，结果不明则 Blocked。

## 6. Touch 一次采样、多指同包

data=inputType(1B)+TouchRecord(7B)×N，N 精确从长度推导。同一个采样只对应一个
cursor，不能把多个时刻的记录平铺。本版使用完整采样：

| seq | data 记录 | data 字节 | native 提交后的活动指 |
|---:|---|---:|---|
| 1 | type=Touch；7Down、8Down | 15 | 7,8 |
| 2 | type=Touch；7Move、8Move | 15 | 7,8 |
| 3 | type=Touch；7Up、8Move | 15 | 8 |
| 4 | type=Touch；8Up | 8 | 空 |

静止活动指仍可 Move 原坐标。本次终止指发送一次，下一采样移除；不能根据缺失
推断 Up。RX 先聚合一次真实采样，再按 target 构建该流完整集合；采集 API 只有
逐 pointer 消息时须证明其帧分组，不能把任意发送队列 drain 当作一个采样。

ID 唯一、Down/Move/Up 前置状态、记录总数和坐标先整组验证，再映射 native ID
并提交。失败不先提交合法的另一根手指；native 部分成功则记前缀/清理，不虚构
系统原子回滚。maxContacts 包含本采样的终态记录；超出已协商能力就拒绝/清理，
不能拆成两个没有原始采样语义的包。

## 7. Mouse 与类型交错

后续 RX 实现真实 Mouse 采集后，每包一个 Mouse 事件，Move/Buttons/Wheel/
Cancel/Modifiers 按精确变体解码。正式规范明确按钮、修饰键与滚动单位/阶段/anchor。
按钮为事件后的完整状态，Move/Wheel 不能暗中改变按钮；结束释放必须可观察。

```text
seq1 Touch 7Down,8Down
seq2 Mouse Move             # Touch 7/8 仍活动
seq3 Touch 7Up,8Move
seq4 Mouse LeftDown
seq5 Touch 8Up
seq6 Mouse LeftUp
```

全流 seq 严格递增，cursor 单调非递减；没有类型切换协商/隐式 reset。Mouse Cancel
只取消 Mouse，Touch Cancel 只结束指定接触，整流 close 才释放全部 owner 状态。
设备移除由 RX 本地映射决定取消范围，不新增每包 deviceId。

## 8. 正常关闭、事件丢失和重连

1. 任一 gate 关闭，RX 同步 fence generation 并停采集/入队，发布 closing。
2. RX 用完整身份/target/streamId 发 input.closeStream；控制传输未成功不妨碍
   本地先清理采集/映射。finalSeqId 用于诊断，不授权重放或无限等待。
3. TX 停止接收该 context、隔离队列、等待在飞任务收敛，按本 owner 账本释放。
4. 释放已确认才 closed；不明返回 UNAVAILABLE/cleanupPending 并保持 closing/
   blocked。重试同一 close 可继续清理，不能缓存未释放为成功。
5. RX 若漏事件先停止可疑流，通过 stream.getState/input.closeStream 收敛，
   cast.getTouchbackState 获取 RX 当前产品快照；不是靠旧 effective 恢复。
6. 断线两端独立清理，不等远端 Reset。重连重新认证、快照、来源绑定和新开流。
   同 session 不复用 streamId；新 session 也不能复活旧回调/资源账本。

## 9. 几何、权限与 source 变化

RX 布局仅改变显示时可本地重算归一化，受影响指明确 Cancel。TX 来源/区域/
方向/单位/权限改变：先 fence 旧输入、释放、退役 ID，再应用/验证新绑定，最后
新 open；媒体按自身流程继续。质量 intent 可接受/降级/拒绝，不等于 applied 几何。

权限撤销、native 错误或目标消失由 TX 先本地停止，通过通用状态通知 RX；收到
非 streaming 状态的 RX 停止发送并确认业务清理。旧流事件不能关闭新 owner，
旧异步回调只能记旧代结果。

## 10. 故障与恢复矩阵

| 故障 | 立即处理 | 恢复条件 |
|---|---|---|
| 无能力/无权限/无映射 | 拒绝 open，报告原因 | 条件改变后明确新 open |
| 外来/退役 streamId | 丢弃计数 | 不影响合法 owner |
| CRC/Frame 损坏 | 不信任其中 ID，按绑定链路错误策略停止 | 链路/输入 context 明确恢复 |
| 跳序/坏长度/未知类型 | 当前 context 故障并释放 | 新 ID；不跳过继续生命周期 |
| 完整 Touch 集合非法 | 不提交本采样任何前缀 | 清理当前输入 |
| 队列满/超龄 | fence，优先释放 | 不静默丢终态，不重放 |
| native 部分失败 | 记录确定前缀，尝试 release | 无法确认则禁止新 owner |
| close/prepare 超时 | 退役本地 generation | 晚资源清理确认，不能晚激活 |

所有队列、重试、pending 和清理等待有界。协议层、native submitted 与应用消费
各自记录；不依赖 injectionCount 宣称触摸生效。输入失败隔离于媒体和无关 RPC。

## 11. NA20/NT10 路径

RX 经 NA20 将同一 input.stream 语义传给 NT10。能力、来源绑定、open、close 和
目标授权仍指向真实终点；中继使用既有通用转发，不新增猜测性 relay capability。
逐跳 Frame source/destination、分片和 CRC 可重建，业务 data 必须保持原义。

NA20 路由依据控制建立的 session/stream table，不解析 Touch、Mouse 或坐标。
本版要求保持端到端逻辑 session/context，不声明两个独立 RPC session 的桥接；
同号 streamId 不能代替认证/路由关联，未来桥接只能由通用 stream 路由实现。NT10 声明真实 HID/native 能力、完整帧上限和故障释放责任；Mouse-only
终点不自动接受 native Touch。各跳断开本地停流，终点不等 NA20 消息才释放。

## 12. 评审与验收

R2 裁决已落实到 specs/registry/生成器和正式 corpus；本 flow 与 input/cast 正文
一起替换旧单模式方案。Runtime/Axent 精确绑定、跨语言 codec 与 profile 验证后
才完成 TI-1；RX/TX 生产接入和真实硬件验收属于后续里程碑，不能由模型代替。

联合验收必须覆盖同采样 1/2/5 指、终态、类型交错、目标隔离、门控、几何、队列、
native 部分失败/blocked、关闭重开/断线和媒体持续；真实 Windows Touch 与 macOS
实际 Mouse/Scroll 能力分别取证，不能互相替代。
