# AXTP Typed Input v1：控制流程与 Touch/Mouse 二进制合同

日期：2026-11-05。状态：**R2 已确认采纳；未发布的开发合同**。

精确事实源为 `contract/registry/domains/input/domain.yaml`、Cast registry 和 Core
transport metadata；正式规范为 `specs/48-input-stream.md`，按同一事实源生成合同/向量。

本轮根据用户确认的精简方向重写：业务 data 仅为 inputType 与实际记录；Touch
每条 7B，同一次采样的 N 条记录整体发送。协议同时定义 Mouse，首版 RX 的
生产采集范围仍为 Touch-only，不能据此宣称已经实现 Mouse 采集。

用户本轮确认按 R2 实施。本合同替换 e986dcf 的单模式草案；不变数字 ID 保持，
退役 method/field/mask 只保留分配墓碑并由工具拒绝复用。旧原文从 Git 追溯，
不作为当前有效协议并列保留。生产切换须等 Runtime/Axent/RX/TX 精确绑定与迁移。

## 1. 业务目标、角色与分层

媒体从 TX 流向 RX；反控从 RX 流向该画面的 TX 输入终点。输入类型描述事件，
TX 输出策略描述本机行为。默认 native 保持类型；force-touch/force-mouse 是
未来明确、有损、需独立规则与验证的策略，不改变线上 decoder 的解释方式。

| 角色 | 职责 | 必须保持的边界 |
|---|---|---|
| RX 产品 owner | 现场授权、精确 target、采集/去重、物理到逻辑 ID、归一化和发送 | 本地布局/设备管理留在 RX，不按 TX 策略预先猜成另一种类型 |
| TX 输入终点 | 认证来源绑定、类型/权限探测、Stream Context、校验、坐标映射、native 提交及清理 | 不信任客户端自报窗口/屏幕，不让旧回调使用新上下文 |
| 共用输入引擎 | 排序、有界队列、独立 Touch/Mouse 状态、输出策略与所有权账本 | 解析成功、API submitted 和应用实际收到分别记录 |
| native provider | 本机 API、单位转换、确定的成功前缀与释放结果 | 不解析网络 bytes，不拥有业务路由或隐式类型回退 |
| AXTP Runtime | 标准 framing、CONTROL/RPC/STREAM、schema 和协议机制 | 不加入 NearCast UI/隐私/重试策略，不改标准 STREAM header |
| Axent | 对产品暴露合同、adapter/provider、会话和路由 | 公共类型不泄露 Runtime headers/IDs，Core 不承接产品策略 |
| NA20 / NT10 | 分别为通用承载/会话中继与实际输入终点 | 中继不识别手势或转换类型；终点如实声明输出能力 |

发现设备、首次指纹信任、投屏码/产品配对和反控授权继续分开。mDNS/名称/IP
只提供候选；已看到画面不表示允许输入。AirPlay target 不启用本 profile。

## 2. 本轮确定的分工与明确变化

- 完整 AXTP Frame 负责 magic/version、长度、逐跳寻址、分片与 CRC16。
- 标准 16B STREAM header 保持 streamId:u32、seqId:u32、cursor:u64。
- 开流一次确认业务 format、输入类型、限额、来源绑定、授权与本机几何。
- data 中只保留 inputType 与该类型记录；无独立 input header，无重复 CRC32。
- 无每包 epoch、canvasToken、geometryToken、sourceId、deviceId 或 eventCount。
- 同一认证 session 内输入 streamId 不复用；影响输入语义的绑定变化换新 ID。
- 绑定、能力、关闭、全 owner 清理均通过控制面和本机生命周期处理，STREAM
  不承载 BIND/Capability/Reset/Close 业务控制帧，也不使用 streamId=0。
- 首版每个 Touch 包完整表达一次逻辑采样；Mouse 包表达一个鼠标事件。

外层 Version 只规定 AXTP frame layout。业务记录版本仍由开流 format 一次选择，
不能因为外层有 Version 就让不同记录布局混用。本合同 format 为
`input-sample-v1`；它不兼容先前单模式或撤回的 typed 实验。

## 3. 线上完整结构与校验边界

```text
AXTP Standard Frame
  Header 12B
    Magic='AX', Version=1, PayloadType=STREAM,
    PayloadLength, hop SourceId/DestinationId,
    MessageId, FrameIndex/FrameCount
  Payload
    STREAM Header 16B
      streamId:u32 | seqId:u32 | cursor:u64
    Input data
      inputType:u8 | records/body
  CRC16 2B
```

多字节整数均大端，signed 字段使用二进制补码；wire 紧凑排列，不插入 ABI
padding。Frame 的 hop SourceId 不是被删除的物理输入设备 ID。

| 层 | 必须验证 | 不承担的事情 |
|---|---|---|
| Frame | 既有 magic/version/type、帧长度、分片关系、CRC16、承载预算 | 不定义 Touch/Mouse 记录或授权 |
| 重组/STREAM | 完整消息、16B header、非零已知 streamId、会话归属、序号与 data 上限 | 不从 MessageId 推导输入序号或设备身份 |
| input profile | 已协商 format/type、精确记录长度、采样状态和功能限额 | 不再扫描业务 magic 或尝试旧格式 |
| native 执行 | 当前绑定/权限/generation、完整候选状态、原生结果与清理 | 不把网络接收成功当作系统应用消费 |

未分片时 Frame.PayloadLength=16+dataLength；分片由标准 Frame 层重组后才进入
STREAM/input parser。FrameIndex/FrameCount 不表示手指数或采样数。逻辑 input
data 硬上限 4096B，实际取开流约束、本地与承载预算交集。

未分片包的理论长度：Touch 为 `31+7N` bytes；Mouse 为 `31+recordBytes` bytes。
Touch 的单指/双指/五指/十指分别为 38/45/66/101B。数字包含 12B Frame、16B
STREAM、1B inputType 和 2B CRC，不计 WebRTC/USB/TCP 本身的承载开销。

## 4. Stream Context：每包省略字段的前提

双方保存不可变的 Stream Context，通过 `(authenticatedSession, streamId)` 查找：

| 内容 | 持有方/来源 | 失效规则 |
|---|---|---|
| 认证会话与精确 RX target | 既有配对/媒体关联、open 控制 | runtime/session/slot/lifecycle 变更失效 |
| TX sourceBindingId | TX 对已授权投屏/输入来源生成的不透明引用 | 选屏/区域/来源几何改变失效 |
| TX 本机 capture/input region 和单位 | TX 本机事实，prepare 时验证 | 旧 context 不能自动改成新区域 |
| format 与 acceptedInputTypes | 本次 open 的实际结果 | 已允许类型正常交错不失效；新增未允许类型须新 open |
| Touch/Mouse 约束与 maxDataBytes | 能力和请求交集 | 不允许单方扩大 |
| seq/cursor、生命周期、generation | 各端本地状态 | 断开、关闭、故障使旧任务失效 |
| Touch 表、Mouse 状态、native ledger | 分开维护 | 按所有权清理，失败阻止新 owner |

sourceBindingId **只在控制面出现**，不是物理设备 ID。产品集成通过已认证的
媒体/设备来源描述向 RX 提供该不透明引用，RX 在 open 原样带回；TX 验证其
会话、授权及本机 capture/input region。此来源描述属于承载/产品绑定接口，
不是新的 AXTP 查询方法或 input 二进制 BIND。缺少可信描述就不允许 open；
不能从 HWND、显示名称或“当前选中来源”猜测。来源失效必须退役引用。

RX 的画布、letterbox、布局、设备 ID 和本地采样帧号留在 RX。RX 先将触点
转换为内容归一坐标；TX 用自己保存的来源映射恢复本机位置。纯 RX 显示布局
变化若不改变线上坐标含义，可本地处理；受影响接触要取消，不能跨目标转移。

TX 来源区域、方向、拓扑或授权发生实质变化时，立即 fence 旧 context 并清理，
新绑定准备好才允许新 streamId。开流成功缓存也不能把退役 ID 重新激活。
同一 session 内 ID 耗尽就拒绝新的输入开流，保留媒体；不能通过回绕复用绕过。
异步 native 回调仍须检查本地 generation，删掉 wire epoch 不会取消这项义务。

## 5. 能力、开关与开流控制

### 5.1 软件能力和首版实现范围

协议定义 Touch 与 Mouse 不等于各软件都已实现。RX 声明实际可采集的类型，
不按当前插着什么设备改变软件能力；TX 区分 decoder 支持、native 输出支持和
当前权限/几何 readiness。开流允许集合是本次实际能消费的非空交集。

首版 RX 只采集真实 Touch，实际请求集合为 `[touch]`。以后 RX 已实现 Mouse
采集，即使尚未插鼠标，也可在开流时请求 `[touch,mouse]`；允许之后的设备切换
无需再次协商。未知/未允许类型仍不得发送。native 不支持 Touch 时不能暗转 Mouse。

约束采用类型专属对象，避免要求 Mouse 填无意义的 maxContacts：

```json
{
  "capability": "input.stream",
  "direction": "receiver_to_source",
  "reliability": "ordered_reliable",
  "profiles": ["AXTP-TYPED-INPUT-V1"],
  "formats": ["input-sample-v1"],
  "inputTypes": ["touch", "mouse"],
  "maxDataBytes": 4096,
  "touch": {"maxContacts": 10},
  "mouse": {
    "buttonsMask": 31,
    "modifierMask": 15,
    "allowChords": true,
    "wheelUnits": ["detent120", "regionQ16_16"],
    "continuousScroll": true
  }
}
```

此例是能力完整的示例对端，不声称 Windows/macOS 当前均已实现。某 provider
不能表达连续滚动、组合按键或修饰键就收窄相应能力；RX 不发送未协商动作。

### 5.2 统一 gate 与状态

receiverOptIn 是默认关闭的现场授权；controllerRequested 是控制端 intent。
任一未满足时不注入。两处写入进入同一个 target owner，能力不支持时给出原因。

| 产品状态 | effectiveEnabled | acceptedInputTypes | 行为 |
|---|---|---|---|
| inactive | false | [] | 未授权、未绑定或已关闭 |
| opening | false | [] | 等待当前 open/prepare，不能补发已有按压 |
| active | true，且其它条件仍满足 | 非空 | 允许新的输入生命周期 |
| closing | false | [] | 已 fence，等待释放/关闭 |
| faulted | false | [] | 保留原因，等待明确恢复 |
| cleanup_blocked | false | [] | 释放无法确认，禁止新 owner/重初始化绕过 |

effective=false 不证明已经清理。当前 streamId 可为 Closing/Blocked 清理保留，
不能用于继续输入。初版同一 TX native 输入域只允许一个 controller，第二个 BUSY。

### 5.3 input.openStream 字段

沿用已登记方法名/ID；新字段的 schema fieldId 已在 registry 分配，不复用退役槽。

| 字段 | 类型 | 必填 | 校验/语义 |
|---|---|---|---|
| controlVersion | u32 | 是 | 本控制合同版本 1 |
| runtimeInstanceId | string | 是 | 与认证 RX runtime 及媒体关联一致 |
| target | InputStreamTarget | 是 | sessionId、sessionGeneration、slotId、bindingGeneration、lifecycleGeneration 全匹配 |
| sourceBindingId | string | 是 | TX 当前授权输入来源的引用，见 B1 |
| streamKind | enum | 是 | touchback |
| format | enum | 是 | input-sample-v1，开流一次确认记录版本 |
| inputTypes | string[] | 是 | 非空、唯一、当前定义 touch/mouse |
| maxDataBytes | u32 | 是 | 业务 data 预算，硬上限 4096，不含外层/STREAM |
| touch | InputTouchConstraints | 含 touch 时 | maxContacts，1..256，受 native 完整帧和 data 预算限制 |
| mouse | InputMouseConstraints | 含 mouse 时 | 上面的位掩码、组合按钮和滚动约束 |
| commandId | string | 是 | 有界不透明幂等键，关联认证调用者、完整目标和请求内容 |

响应字段：streamId（非零新 ID）、format、acceptedInputTypes、maxDataBytes、
对应 touch/mouse 约束、revision、accepted=true。成功要求认证、target、绑定、
native/权限、坐标映射和独占 owner 都已准备，并且操作仍属于当前 generation。
没有共同类型返回 NOT_SUPPORTED；不返回空集合的成功结果。

Touch.maxContacts 的有效值还受 `floor((maxDataBytes-1)/7)` 限制；Mouse 基础
Move/Buttons 需要 data≥12B，若允许 Wheel 则需要 data≥22B。Mouse 的掩码取
位交集、allowChords/continuousScroll 取双方都支持、wheelUnits 取集合交集。
不同动作的不足不偷偷转换，返回实际允许能力并在 UI 显示限制。

幂等键命中前仍检查当前身份和 readiness。同键不同内容拒绝；旧成功已经关闭
不能恢复 Active。输入记录不重试；有界控制重试只复用相同语义。prepare/open
超时先 retire generation，晚资源清理不明则 Blocked，不能晚激活。

### 5.4 关闭、取消与状态恢复复用

input.closeStream 负责业务释放，参数保留完整目标/认证身份、streamId、reason、
finalSeqId（可选）和 commandId；不再以 wire epoch 寻址。正常关闭只由 producer
请求，consumer 本地失效可先 fence，再用现有 `stream.stateChanged` 通知 producer。

AXTP-TYPED-INPUT-V1 要求支持 `stream.getState` 与 `stream.stateChanged` 的状态子集；
不因此要求 ACK/window/pause/resume 等其它 stream.flowControl 操作。capability 的
profiles 明确包含 AXTP-TYPED-INPUT-V1；它仅要求下列方法/事件子集，不将整个
stream.flowControl feature 的所有方法声明为可用。其它已注册但不可用操作返回 NOT_SUPPORTED。

| input 生命周期 | 通用 STREAM 状态 | producer 处理 |
|---|---|---|
| prepare 尚未完成 | opening | 等待对应 open，不发 data |
| 可消费 | streaming | 仍须本地 current open 成功，event 不能代替授权 |
| 已停止输入，正在清理/清理受阻 | closing | 立即停采集/发送，走 close/状态恢复 |
| 本 input owner 释放已确认 | closed | 退役 ID；新开流必须用新 ID |
| 其它 failed/aborted/非 streaming 通知 | 按通用状态 | 先停止输入，再确认业务清理，不能继续旧流 |

本 input profile 对 closed 增加强约束：只有本 owner 的 native 释放确认后才
确认 closed。通用传输停止、STREAM_NOT_FOUND 或消息已转发本身不证明全局
native 状态干净。清理受阻保留业务账本，input.closeStream 返回 UNAVAILABLE，
详情包含 cleanupPending=true；重复 close 可重试清理，不能从缓存假成功。

订阅状态事件在 open 前完成；同 session 怀疑漏事件时用 stream.getState 查询，
仍有疑义先 fence，再以 input.closeStream 确认。重连使旧 context 全部失效，
不能用旧查询结果恢复。断线时各端本地清理，不等待网络消息。

## 6. Input data：逐包只带类型与本次数据

| data offset | 字段 | 大小 | 值 |
|---:|---|---:|---|
| 0 | inputType | u8 | Touch=1、Mouse=2；0 不用于 data，其它未定义值拒绝 |
| 1.. | 该类型 body | 依类型 | Touch 多触点采样；Mouse 单个事件 |

inputType 是业务语义，不能塞进外层 PayloadType，也不能靠 buttons/坐标/包长
猜测。未允许类型拒绝后终止当前输入 context；未知 data 不能当成空包或 Move。
通用 schema 的未知 optional object 字段仍遵守 AXTP 核心规则，这不授权未知
输入枚举/二进制记录进入 native。

### 6.1 Touch：7×N，整个采样一起发送

```text
inputType(1B) | TouchRecord(7B) × N
```

| 单条 record offset | 字段 | 类型/大小 | 语义 |
|---:|---|---|---|
| 0 | contactId | u16 / 2B | stream 内的逻辑接触 ID，允许 0 |
| 2 | phase | u8 / 1B | Down=0、Move=1、Up=2、Cancel=3 |
| 3 | x | u16 / 2B | 内容归一坐标，0..65535 含端点 |
| 5 | y | u16 / 2B | 同上 |

N=(dataLength-1)/7，必须整除且 1≤N≤协商上限。**7B 是每个触点，不是整包**。
一个包对应一次逻辑触控采样，所有记录共享 STREAM.cursor；无 dtUs、独立 flags
或隐式 padding。Down/Move/Up 属于不同时刻时分别发包，不混入一个没有采样边界的数组。

本轮选择**完整采样集合**：持续接触也有记录（Move，可保持坐标不变），本次
Up/Cancel 终态记录发送一次，下一包才移除。这样每次采样的多指共同表达。
不能从一个活动 ID 缺失推断它已抬起；这类包是不完整采样并使当前输入流故障。

令 A_prev 为上一成功采样后的活动集合，S 为本包 ID 集合：

- S 中 ID 唯一；所有 A_prev 都必须在本次 Touch 采样中显式出现。
- Down 的 ID 不在 A_prev；Move/Up/Cancel 的 ID 必须在 A_prev。
- A_next=(A_prev ∪ Down) − Up − Cancel。终态之后才可复用 ID。
- S 包含仍活动与本次终态，合计不得超过 native 能一次表达的协商 maxContacts。
- 先验证完整 S/候选状态，再映射并提交；任一记录非法不能先提交其它手指。
- 没有新 Touch 采样就不发包；用户保持不动或 Mouse 包到达不代表任何 Touch Up。

示例：

| 采样 | 同包记录 | 提交后活动集合 |
|---|---|---|
| t1 | 7 Down；8 Down | {7,8} |
| t2 | 7 Move；8 Move | {7,8} |
| t3 | 7 Up；8 Move（可静止） | {8} |
| t4 | 8 Move | {8} |
| t5 | 8 Up | {} |

RX 将物理设备/系统 pointer ID 映射成 stream 内唯一 contactId；映射寿命覆盖
Down 到 Up/Cancel。设备拔出时用 Cancel 结束它的接触，其它接触仍放入同次完整
采样。ID 不是第几根手指或本机 Windows ID。TX 映射到本机 ID 池，不能直接
把 wire ID 当 OS ID；候选映射也仅在确定提交后更新。

原生适配按一组多接触状态执行。平台若要求完整接触数组，则包含本采样活动
和终态；不能把双指 Move 当两个独立手势。只有 native 契约确实要求额外步骤
时才能在同一串行执行事务中展开，并记录确定成功前缀及失败清理，不虚构跨
OS 的回滚或应用消费原子性。

### 6.2 Mouse：一只逻辑鼠标，每包一个事件

Mouse body 首 byte 是 eventKind，按事件实际需要分为以下精确形状；没有
逐记录时间、flags、占位或 eventCount。

| eventKind | 值 | record 长度 | 业务 data 长度（含 inputType） |
|---|---:|---:|---:|
| Move | 0 | 11B | 12B |
| Buttons | 1 | 11B | 12B |
| Wheel | 2 | 21B | 22B |
| Cancel | 3 | 1B | 2B |
| Modifiers | 4 | 3B | 4B |

每种形状长度必须精确匹配；不接受尾随 bytes。Mouse 本版是归一绝对指针；
相对鼠标、独立多鼠标、Pen 或其它类型以后独立扩展，不把 dx/dy 塞进绝对 x/y。
RX 本地合成一只逻辑鼠标，物理来源切换不让旧按压消失。

Move/Buttons 的共同形状（offset 相对 Mouse record，不含 inputType）：

| Offset | 字段 | 类型/大小 | 语义 |
|---:|---|---|---|
| 0 | eventKind | u8 / 1B | Move=0 或 Buttons=1 |
| 1 | modifiers | u16 / 2B | 本事件时刻完整修饰键上下文 |
| 3 | buttons | u32 / 4B | 本事件之后的完整逻辑按钮状态 |
| 7 | x | u16 / 2B | 内容归一绝对位置 |
| 9 | y | u16 / 2B | 同上 |

| 位 | buttons 含义 | modifiers 含义 |
|---:|---|---|
| 0 | Left，值 1 | Shift，值 1 |
| 1 | Right，值 2 | Ctrl，值 2 |
| 2 | Middle，值 4 | Alt，值 4 |
| 3 | Back，值 8 | Meta，值 8 |
| 4 | Forward，值 16 | 未定义，必须 0 |

其余位未定义，必须为 0；实现还受协商 mask 限制。位定义来自 input_stream
机器事实；复合转移和原生能力映射必须遵守下述顺序，不能把位图当单个按钮编号。

Move 的 buttons 必须等于之前状态；它可以表达 hover 或已有拖动，不能补造
Down/Up。Buttons 必须有按钮转移，last Up 的 buttons=0。设前状态 B、后状态 B'：
released=B & ~B'，pressed=B' & ~B。允许组合键时按完整状态比较；需要展开的
适配先释放再按下，按位升序并确保位置/修饰键属于本事件。不能将整个 bitmap
当成单个按钮编号，不能把 buttons=0 的 Touch 猜成左键。

Mouse 刚开流时按钮状态为 0；采集开启前已经持有的本地按钮不补造远端 Down。
RX 跟踪捕获归属，等有效新生命周期；本地导航/控制按钮不会转发成远端点击。

### 6.3 Wheel：明确单位、方向和连续阶段

| Offset | 字段 | 类型/大小 | 语义 |
|---:|---|---|---|
| 0..10 | Mouse 共同字段 | 11B | eventKind=2；buttons 不改变；x/y 为滚动路由位置 |
| 11 | wheelUnit | u8 / 1B | 0=detent120；1=regionQ16_16 |
| 12 | wheelPhase | u8 / 1B | 0=None、1=Began、2=Changed、3=Ended、4=Cancelled |
| 13 | wheelX | i32 / 4B | 水平增量 |
| 17 | wheelY | i32 / 4B | 垂直增量 |

统一符号为：wheelX>0 表示视口向右，wheelY>0 表示视口向下（内容分别向左、
向上移动）。采集/原生 API 的符号不自动成为 wire 标准，adapter 负责明确映射。

- detent120：±120 为一格，允许非 120 倍数的高分辨率增量；phase=None，至少
  一个轴非零。不将一格写死成固定像素数；目标系统滚动偏好由原生适配尊重。
- regionQ16_16：带符号 Q16.16 的绑定区域比例。65536 表示一个完整区域宽/高。
  RX 将采集到的连续位移按内容区域归一化；TX 按保存的 native 区域和单位恢复，
  不存在含糊的“RX 像素还是 TX 像素”。单位和方向是本版合同，不按宿主 API 的默认符号解释。
- Began 创建连续滚动，Changed 要求它存在；Ended/Cancelled 的两个 delta 为 0。
  连续滚动始终使用同一种 unit 和起始 anchor x/y；Move 可独立移动指针，Wheel
  provider 不应为投递 anchor 事件反复移动可见光标。不支持此语义就不声明该能力。
- 离散/连续滚动之间切换先结束或取消旧连续段；不将另一设备的滚动混入未结束
  手势。独立多设备滚动不在本版承诺内。
- 连续小数余量在适配层保留，终态/Cancel/close 时清零；计算用足够宽度并检查
  溢出，不静默截断。不能用隐式“像素转格”假装 native 保持语义。
- 独立 momentum/inertia 阶段本轮未定义，不声明完整惯性透传；未来按新明确
  记录/能力扩展。Mouse/Wheel 的事件类型不会因此切换成 Touch。

### 6.4 Modifiers 与 Cancel

Modifiers record 为 `eventKind=4:u8 | modifiers:u16`，共 3B。它只同步鼠标上下文
所需修饰键，解决 Ctrl/Shift 已释放但鼠标不动时没有下一条 Move 可传的问题。
普通 Move/Buttons/Wheel 仍携带当时的完整 modifiers，可更新上下文。

非零 modifierMask 意味着 producer 能可靠捕获修饰键变化/失焦取消，consumer
能安全表达；否则协商 mask=0 并发送零值。本版不转发普通文字/键盘按键。
终点仅释放自己合成的修饰键，不能释放本机物理键盘的按压。不能可靠捕获释放或安全注入的实现必须声明 modifierMask=0。

Cancel record 仅 `eventKind=3` 一个 byte：终止逻辑 Mouse 的按钮、连续滚动、
修饰键和余量，使用保存状态释放，不需要重复坐标/按钮字段。重复 Cancel 幂等。
它不取消 Touch；Touch Cancel 也不释放 Mouse。整流 close 才清理全部类型。

## 7. 排序、时间、跨类型与取消

每个新 streamId 从 seqId=1 开始，Touch/Mouse 共享一个严格递增序号域。重复或
缺口为当前输入故障；到 uint32 最大值后经控制 close 收敛，不能回绕。cursor 是
RX 单调微秒采样时间，同一 stream 非递减；不要求两机时钟同步，也不指示延迟播放。

```text
seq1 Touch：7 Down；8 Down
seq2 Mouse：Move（Touch 仍为 {7,8}）
seq3 Touch：7 Move；8 Move
seq4 Mouse：Buttons，Left 按下
seq5 Touch：7 Up；8 Move
seq6 Mouse：Buttons，Left 释放
seq7 Touch：8 Up
```

不因类型交错 reset/reopen。静止接触没有新采样时可以不发 Touch 包，不能仅按
无 Move 的时间虚构 Up。连接/操作活性 deadline 与输入事件间隔分开。

源设备消失由 RX 本地归属表决定：Touch 对应接触 Cancel、其余指仍在完整采样；
Mouse 逻辑归属无法继续时 Mouse Cancel。整个授权/来源失效则 close 全 context。

## 8. WebCast 的完整 AXTP 承载修正

当前生产 WebCast binary 只有裸 STREAM 与业务数据；它不会自动拥有外层 AXTP
magic/version/CRC。R2 必须让 input 走完整 Standard Framed，才能按上述方式删冗余。
承载 profile 为 AXTP-WEBRTC-DC，使用 STANDARD_FRAME；作为 experimental
集成声明，不把 profile 登记当作现有产品已实现或已通过网络/真机验证。

沿用已有 ordered/reliable control DataChannel：AXTP 流量统一为 binary
Standard Frames，既有非 AXTP 产品 text 消息保持原 handler。AXTP 的 CONTROL、
RPC、STREAM 不再另套私有 axtp-control JSON；输入也不接受旧裸 STREAM。
每个 AXTP binary DataChannel message 恰好承载一个完整 Standard Frame；需要分片
时各片分别发送为 message，接收端整消息校验 CRC/长度后重组，不能把片段当采样。
不新增一条 WebSocket 输入通道，不改变媒体 RTP/音视频生命周期。

| 阶段 | 流量/状态 | 输入权限 |
|---|---|---|
| DataChannel 可用且已关联认证媒体 peer | 仅说明承载可用 | 无 |
| 标准 CONTROL OPEN/ACCEPT | maxFrameSize、PayloadType、RPC encoding、heartbeat/ackMode | 无 |
| RPC Hello/Identify/Identified | 与既有配对/媒体身份绑定，进入 APP_READY | 无 |
| 查询能力/订阅状态/当前来源绑定 | 建立可验证控制上下文 | 无 |
| input.openStream 成功且 RX 确认当前请求 | 创建新的 Stream Context | 才允许有效新输入 |

RX 为输入 producer/logical client，TX 为消费 endpoint/logical server；WebRTC
DataChannel 创建者不因此改变逻辑角色。已有标准 session 可复用；channel open
不能替代 APP_READY，外层 frame 也不是认证。READY 不新增为必需第三次握手，
不启用逐帧应用 ACK。逻辑 input session 固定绑定当前配对媒体 peer 与专用控制 DC 实例；更换 peer/
重连即失效。若配对在媒体/signaling 已验证，Hello 可按现有 Core 宣告不另要求
password 认证，但只接受已绑定 peer；不能让另一个 DC 自报同名身份获得权限。
可复用的既有标准 session 必须满足相同角色、身份与承载要求，不能复用私有 JSON
session 或用 channel open 代替鉴权。本规则不改变 Core 的一般握手/认证语义。

## 9. 完整业务时序

### 9.1 发现、信任和正常开启

1. TX 发现 RX 候选，按现有指纹/配对和投屏码流程建立媒体会话。
2. RX 将媒体来源映射到完整 target；TX 准备授权来源及 sourceBindingId。内部
   布局、设备原始 ID 不放进输入包，两个方向的认证关联都要检查。
3. 标准输入承载 APP_READY；交换软件类型/约束，订阅流状态子集。
4. 本地现场 gate 和控制端 intent 都满足时，RX owner 发布 Opening 并发起 open。
5. TX 验证请求、当前绑定/权限、坐标映射与 native 独占域，准备 provider。
6. 成功只返回允许类型/约束及新 streamId。RX 检查当前 pending/generation、
   两个 gate 和目标未变，才发布 Active 并采集新生命周期。
7. Touch 采样整组、Mouse 单事件，按全流序号交错；完整验证后提交，不逐帧 ACK。

### 9.2 几何和质量变化

RX 的纯显示缩放/布局归一化可本地管理；受影响接触按本地策略取消。TX 的
capture region、方向/单位/来源身份或权限改变会使 context 失效：先停输入和
清理，再让新媒体/来源绑定实际应用，最后新 open。仅接受配置 intent 不代表
applied 几何已经改变；重建失败保留旧图也要重新验证映射，不能复活旧队列。

来源质量偏好 inherit/prefer1080p 与输入类型/开关独立。源可降级/拒绝，RX
放大画布不证明编码已升级；实际坐标语义未变不额外制造输入协商。

### 9.3 正常关闭与本机故障

RX 关闭：同步推进 generation/停止采集与入队 → input.closeStream → TX fence
并释放 → 成功 Closed/错误 Blocked → RX 完成状态发布。任一端本地停止都不以
网络请求成功为清理前置条件。

TX 权限/映射丢失或 native 失败：先本地 fence → 通用流状态通知 producer 停止
→ 清理确定的所有权 → 确认 Closed 或保持 Closing/Blocked → RX 通过 close/
状态恢复确认。部分 native 成功不重放前缀，旧异步完成不能改变新的 owner。

断线：两端分别本地取消和清理；重连先认证、快照和新绑定，新 streamId。
UI 的 accepted/effective、native submitted、清理结果和应用实际收到分开显示。

### 9.4 NA20/NT10

RX → NA20 → NT10 使用同一标准 Frame/STREAM/input data。NA20 不解析输入
记录、不改坐标、不转换手势、不新增 relay capability。逐跳地址/分片/CRC 可
按通用 AXTP 规则重建；路由来自开流建立的 session/stream table，不靠 deviceId。

本版 HID profile 要求 RX 与 NT10 保持同一端到端认证逻辑 session/Stream Context，
NA20 只做绑定链路的通用 frame 转发、逐跳寻址和重组。若产品实现终止为两个
独立 RPC session，必须另行实现并采纳通用 stream 路由映射；本版不声明该桥接
能力，不能把两边同号 ID 混用，也不能在 Touch parser 里翻译路由。

NT10 声明真实 native/HID 类型和功能，绑定本机输入区域，检测连接/会话失效并
自行释放。中继转发成功不证明 HID 生效。Mouse-only 终点不自动接受 native
Touch；显式 Touch→Mouse 是后续策略。固件和真实链路验收不由本轮模型代替。

## 10. 验证顺序与故障矩阵

```text
Frame 完整性/分片重组/长度预算
→ 认证/APP_READY + 已知当前 Stream Context
→ STREAM 归属/seq/cursor/data 预算
→ 已允许 inputType/精确记录形状
→ 整个 Touch 采样或 Mouse 转移及能力约束
→ 绑定/权限/generation 仍当前
→ native 执行，依据确定结果更新账本
```

| 情况 | 必须处理 | 恢复/隔离 |
|---|---|---|
| 无授权、未 APP_READY、未 open | 拒绝输入 | 不自动激活，不影响媒体/无关 RPC |
| 无共同类型/格式或动作能力不足 | NOT_SUPPORTED/实际收窄约束 | 不隐式转换，不伪称支持 |
| target/sourceBinding 过期 | INVALID_STATE/相应登记错误 | 取得当前绑定，不落到新来源 |
| 外来/已退役 streamId | 丢弃计数 | 不释放另一当前 owner |
| 当前流 CRC/长度/未知 type/kind/越界/跳序 | fence、整组拒绝、清理并终止 | 明确的新 open；不跳过后继续旧状态 |
| Touch 缺失活动 ID/重复 ID/orphan/超限 | 不提交任何前缀 | 清理该 context，不能猜成 Up/Down |
| Mouse Move 偷改按钮/非法位/非法滚动阶段 | 不注入隐含动作 | 故障终止，不用快照补造按钮边沿 |
| 队列超限 | 隔离旧队列并优先清理 | 不静默丢 Up/Cancel，不无限排队 |
| native 部分成功 | 保存确定前缀并清理 | 不自动重放；不确定则 Blocked |
| 清理失败 | 禁止新 owner/open/重新初始化绕过 | 成功 close 确认前不显示已释放 |
| 来源/几何变化 | 本地停输入、旧 ID 退役 | 新绑定/ID；纯 RX 变换按本地规则 |
| DataChannel/session/relay 失效 | 本地取消全部拥有状态 | 重连新 context，媒体分别处理 |

使用已登记 common/stream errors；细节可用 status.details 的 cleanupPending/
diagnosticReason，不私造数值错误。未知方法是 RPC_METHOD_NOT_FOUND，已注册
但本实现不可用是 NOT_SUPPORTED。控制去重、队列、操作和清理都有有限预算。

CRC 失败时整帧不可信，不能读取其中 streamId 并据此释放任意 owner。承载层按
既定错误策略处理；若因此无法保证活动输入的顺序/连续性，只能 fence 该已认证
链路所绑定的输入 context，再清理。不能让伪造坏帧跨会话取消其它来源。

## 11. 完整控制示例（AXTP RPC d block）

示例遵守本轮 registry；schema-local fieldId 见生成合同。JSON 属性顺序不影响语义。

### 11.1 open Request，op=7

```json
{
  "id": 1001,
  "method": "input.openStream",
  "params": {
    "controlVersion": 1,
    "runtimeInstanceId": "11111111-1111-4111-8111-111111111111",
    "target": {"sessionId":"source-1","sessionGeneration":"1","slotId":"webcast-0","bindingGeneration":"1","lifecycleGeneration":"1"},
    "sourceBindingId": "share-binding-5",
    "streamKind": "touchback",
    "format": "input-sample-v1",
    "inputTypes": ["touch", "mouse"],
    "maxDataBytes": 4096,
    "touch": {"maxContacts":10},
    "mouse": {"buttonsMask":31,"modifierMask":15,"allowChords":true,"wheelUnits":["detent120","regionQ16_16"],"continuousScroll":true},
    "commandId": "open-r2-1"
  }
}
```

### 11.2 成功 Response，op=8

```json
{
  "id": 1001,
  "status": {"ok":true,"code":0},
  "result": {
    "streamId":7,
    "format":"input-sample-v1",
    "acceptedInputTypes":["touch","mouse"],
    "maxDataBytes":4096,
    "touch":{"maxContacts":10},
    "mouse":{"buttonsMask":31,"modifierMask":15,"allowChords":true,"wheelUnits":["detent120","regionQ16_16"],"continuousScroll":true},
    "revision":"1",
    "accepted":true
  }
}
```

### 11.3 close Request 与成功 Response

```json
{
  "id":1002,
  "method":"input.closeStream",
  "params":{
    "controlVersion":1,
    "runtimeInstanceId":"11111111-1111-4111-8111-111111111111",
    "target":{"sessionId":"source-1","sessionGeneration":"1","slotId":"webcast-0","bindingGeneration":"1","lifecycleGeneration":"1"},
    "streamKind":"touchback",
    "streamId":7,
    "reason":"requested",
    "finalSeqId":7,
    "commandId":"close-r2-1"
  }
}
```

```json
{
  "id":1002,
  "status":{"ok":true,"code":0},
  "result":{"streamId":7,"closed":true,"alreadyClosed":false,"revision":"2"}
}
```

清理未确认时使用 input.closeStream 已登记的 UNAVAILABLE status，不带成功 result；
details 为 `{"cleanupPending":true,"diagnosticReason":"native_cleanup_unconfirmed"}`。

### 11.4 通用查询和状态事件（现有字段）

```json
{"id":1003,"method":"stream.getState","params":{"streamId":7}}
```

```json
{"event":"stream.stateChanged","data":{"streamId":7,"state":{"streamId":7,"state":"closing","lastSeqId":7,"reason":"unknown"}}}
```

上述 Event 是现有 op=6 的 d block；在 Standard Framed JSON 中，完整 RPC payload
为 `rpcEncoding=0x01` 一个 byte 后接 `{"sid":"当前认证会话","op":6,"d":上述对象}`。
Request/Response 同样包装为 sid/op/d，op 分别为 7/8；需要逻辑地址时使用现有
可选 m.src/m.dst，不在业务 d 中增加另一种路由 envelope。示例字段已对照现有
Core RPC 与 StreamStateChangedEvent schema；状态子集的要求由 AXTP-TYPED-INPUT-V1 profile 声明。

## 12. 正式样例、验证和下游任务

正式向量由生成器从 input_stream 事实生成到 `contract/test-vectors/input_stream.json`，
manifest 登记入口；`conformance/reference/input-stream.mjs` 执行字节/状态 oracle，
并与独立评审字节及 C++ codec 交叉核对。向量覆盖完整 Frame/CRC、Touch 1/2/5 指、
同采样 Up+持续指、Mouse 五类记录、混合类型、单位/阶段和非法状态。模型证据不
替代 native/系统应用的真机证据。

双指同采样 Down 的 data 示例（x/y 分别为 10000/20000、30000/40000）：

```text
01                         # inputType=Touch
00 07 00 27 10 4e 20       # contactId=7, Down, x=10000, y=20000
00 08 00 75 30 9c 40       # contactId=8, Down, x=30000, y=40000
```

data 共 15B；streamId=7、seqId=1、cursor=1000μs、逐跳地址 1→16 时，完整未分片帧：

```text
41 58 01 03 00 1f 01 10 00 01 00 01  # Frame header，PayloadLength=31
00 00 00 07 00 00 00 01              # STREAM streamId=7, seqId=1
00 00 00 00 00 00 03 e8              # STREAM cursor=1000
01 00 07 00 27 10 4e 20 00 08 00 75 30 9c 40
3c 56                                # CRC16-CCITT-FALSE，覆盖 header+payload
```

Mouse 左键按下且无修饰键、位置在归一中心的 data（12B）：

```text
02 01 00 00 00 00 00 01 80 00 80 00
# type=Mouse, kind=Buttons, modifiers=0, buttons=Left, x=y=32768
```

两个示例均不包含额外 input header。二进制记录不使用字段注释、分隔符或换行。

| 验证组 | 必须证明 |
|---|---|
| framing | 完整 Standard Frame、CRC16、分片后才进 STREAM、裸 STREAM 拒绝 |
| context | 非零新 ID、同 session 不复用、旧 context/回调不能改变新 owner |
| Touch | 7×N、共享时间、完整多指采样、terminal 后移除、ID映射/复用/设备取消 |
| Mouse | 各事件精确长度、完整按钮状态、组合转移、修饰键释放、滚动单位/阶段/anchor |
| mixed | Touch 保持时 Mouse 到达不 reset，类型各自取消不串状态 |
| readiness | native 不支持/无权限/错误来源/几何/第二 owner 拒绝 |
| failures | 全组验证、队列预算、部分提交、blocked cleanup、超时/晚结果/断线 |
| compatibility | 新 format 独立协商，旧字节仅负例，不进入当前有效协议 |
| physical | 两端提交和 fingerprint、设备/权限/DPI、真正 Touch 应用事件、Mouse 行为与 held-gesture 关闭/重开 |

specs/registry、生成器与正式 corpus 按本合同一致更新；Runtime/Axent 与两端须
绑定同一精确开发快照。Touch/Mouse decoder/conformance 同步；首版 RX 采集仍仅 Touch，
未实现的功能不能写进实际 capability。媒体/呈现控制和其它 domain 不顺带改语义。

## 13. 修订记录与已确认裁决

2026-11-05：用户确认按 R2 推进 TI-1。源基线 e986dcf，draft 破坏性修订，
替换 input/cast 的旧单模式合同；不变方法/事件/字段保持 ID，退役位不复用。

| 项目 | 本版固定规则 | 实施边界 |
|---|---|---|
| T1 | Touch 完整采样、7×N、包含本次终态 | RX 按真实 OS 帧聚合，不能任意 drain 成采样；native 容量决定上限 |
| M1 | 五种 Mouse 变体、完整按钮位图、先释放后按下 | 本版一只逻辑绝对鼠标，不含多鼠标/相对模式 |
| M2 | detent120/regionQ16_16，右/下为正、连续阶段和 anchor | 不支持动作在能力中收窄，不静默换单位/类型 |
| M3 | Modifiers 独立事件；可靠捕获/释放，否则 mask=0 | 不包括普通键盘/文本，不释放物理按键 |
| B1 | sourceBindingId 由认证媒体/设备集成提供，open 带回核对 | 缺少可信来源描述就拒绝；不新增 AXTP 查询/BIND 帧 |
| C1 | AXTP-WEBRTC-DC 完整 Standard Frame、标准 APP_READY | 绑定已有配对 peer；产品 text 分流，RTP 不变 |
| C2 | AXTP-TYPED-INPUT-V1 明确状态方法/事件子集 | profiles 发现，不暗示整个 stream.flowControl 全部实现 |
| C3 | HID 中继保持端到端逻辑 context，NA20 不解析输入 | 两独立 RPC session 的 bridge 不在本版支持声明内 |

本次是未发布开发合同，不修改正式 release tag/lock。Mouse 生产采集、TX 平台
接入及真实触摸注入的完成情况必须由后续里程碑分别取证，不能由本协议采纳代替。
