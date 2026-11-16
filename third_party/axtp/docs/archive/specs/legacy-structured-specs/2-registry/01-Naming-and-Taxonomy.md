# 2-registry/01《AXTP 命名与分类规范》

> 状态： AXTP v1 规范性治理规范
> 范围：domain、feature、method/event/capability 命名与 registry 准入分类规则
> 权威边界：本文定义命名和分类规则；不分配 methodId、eventId、errorCode、profileId、capabilityId 或 fieldId。

## 文档目的

本文档定义 AXTP 业务模块进入正式 registry 前必须使用的统一分类语言。核心目标是让每个 method、event、schema、capability 和 profile 都能追溯到清晰的 `domain.feature`，避免把字段名、旧协议命令、transport 细节或 codec 细节误建模为业务能力。

## 范围

本文档覆盖 `domain`、`feature`、method name、event name、capability name 的归属和命名规则，以及草案进入 registry 前的 taxonomy review。本文档不覆盖任何数字编号分配，也不定义 frame、PayloadType、CONTROL、RPC、STREAM 的 wire format。

## 规范规则

1. `domain.feature` 是 AXTP 的业务能力分类单位。正式 method、event、schema、capability 或 profile MUST 能说明自己属于哪个 `domain.feature`。
2. `domain` MUST 表示稳定业务边界，例如 `audio`、`video`、`camera`、`display`、`network`、`storage`、`file`、`log`、`diagnostic`、`device`、`system`、`input`、`output`、`room`、`signage`、`auth`、`privacy`、`capability`。
3. `feature` MUST 表示可评审、可测试、可演进的能力块。字段名、单个 UI 控件、旧协议命令号、encoding、transport profile MUST NOT 直接作为 feature。
4. Method name MUST 使用 `domain.actionObject` 或已采纳的 `domain.verbNoun` 形式，并且 domain 前缀 MUST 与 registry 条目的 `domain` 一致。
5. Event name MUST 表示状态变化、结果、进度或上报语义，SHOULD 使用 `Changed`、`Completed`、`Failed`、`Progressed` 等后缀。Event 不替代 RPC Response。
6. Capability name SHOULD 使用 `domain.feature`，并描述设备在当前固件、配置、会话和鉴权状态下可用的业务能力。
7. `stream` domain 只用于公共 STREAM 数据面或流控能力。业务流能力 MUST 归属到业务 domain，例如 `video.stream`、`audio.stream`、`file.transfer`。
8. 新增 domain 或 feature MUST 能追溯到 `workspace/business`、`workspace/flows` 或 `workspace/protocol` 的评审输入；不能仅因为 legacy 命令存在而创建。
9. stable 名称不得复用为新语义。废弃名称 MUST 通过 registry 状态表达，并保留兼容解释。

## Registry / Schema / Tooling 模型

AXTP 的命名链路如下：

```text
workspace/business or workspace/flows
  -> workspace/protocol/<domain>/<domain.feature>.md
  -> contract/registry/domains/<domain>/domain.yaml or core registry YAML
  -> contract/protocol/axtp.protocol.yaml
  -> contract/generated/* and runtime generated artifacts
```

`domain.feature` 不直接改变 wire format。它是 registry review、generated 文档分组、capability 归属、conformance case 组织和业务责任边界的共同语言。

| 业务形态 | 推荐 feature | 常见 method/event 形态 |
|---|---|---|
| 配置型能力 | `domain.<feature>` | `get<Feature>Capabilities`、`get<Feature>Config`、`set<Feature>Config`、`reset<Feature>Config`、`<feature>ConfigChanged` |
| 状态型能力 | `domain.state` 或更具体 feature | `get<State>`、`<state>Changed` |
| 动作型能力 | `domain.<operation>` | `start<Operation>`、`stop<Operation>`、`apply<Operation>`、`<operation>Completed` |
| 流型能力 | 业务 domain 下的 stream feature | RPC method 建立/关闭流，STREAM 承载连续数据 |

## 校验规则

Registry review 和 generator validation SHOULD 至少检查：

1. method/event/capability 的 domain 前缀与 registry `domain` 字段一致；
2. feature 不以字段名、枚举值、codec、transport、UI 控件或 legacy CmdValue 命名；
3. 业务 method、event、schema、error 和 capability 能追溯到同一 `domain.feature`；
4. 新增 domain 不与已有 domain 职责重叠；
5. 新增 feature 的粒度足以独立测试，且不会把多个无关能力混在一个 capability 中；
6. STREAM 相关能力没有把业务分类塞进公共 `stream` domain；
7. generated docs 中的分组与 registry YAML 中的 domain/feature 一致。

## 兼容规则

1. 已发布的 stable domain、feature、method name、event name 和 capability name MUST NOT 改变语义。
2. 重命名 SHOULD 通过新增名称加旧名称 deprecated 的方式完成，不得静默替换。
3. legacy 命令映射 MAY 记录在 registry 条目的 `legacy` 元数据或 `workspace/legacy-migration` 中，但 legacy 名称不得污染正式 AXTP 命名。
4. 新增 feature SHOULD 默认向后兼容；若需要 breaking change，必须进入版本治理并同步 generated/conformance。

## 实现要求

- Runtime MUST 消费 registry YAML 生成的 Protocol IR、generated reference 或绑定的 release artifact，不得从本文件推断当前可调用 method 列表。
- Generator MUST 将 registry 中的 domain/name/schema/capability 引用不一致视为 validation failure。
- 协议维护者在采纳草案前 MUST 完成 taxonomy review，并在草案或审计记录中说明 domain.feature 选择理由。
- Conformance case SHOULD 按 `domain.feature` 组织，覆盖 method 成功路径、错误模型和事件上报。

## 示例

正例：

```text
output.routing
output.layout
room.scene
signage.schedule
system.state
```

反例：

```text
brightness              # 缺少 domain
output.hdmi1            # 把接口字段当 feature
tlv.audio               # 把 codec 当 feature
stream.videoFrame       # 把业务流塞进公共 stream domain
legacy.cmd_0x42         # 把旧命令号当正式能力名
```

## 非目标 / 未来

本文档不维护完整业务能力清单。历史规划表、候选表或当前 generated 清单不应放回本文正文；当前正式实现事实以 `contract/registry/**/*.yaml`、`contract/protocol/axtp.protocol.yaml` 和 `contract/generated/**` 为准。未来可增加独立 taxonomy appendix 保存已评审但尚未进入 registry 的候选清单；runtime 仍不得从 appendix 实现协议。
