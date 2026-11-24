---
status: generated
contract: true
generated: true
domain: signage
feature: signage.playlist
registry: ../../../../contract/registry/domains/signage/domain.yaml
lastReviewed: 2026-09-15
---

# AXTP signage.playlist 协议草案

版本：v1.6（已采纳）

归属域：`signage`（DomainId `0x0D`）

Capability ID：`signage.playlist`（capability `0x0D01`）

适用范围：数字标牌播放列表全量同步、查询、恢复默认、播放项资源 URL 刷新和播放项整体刷新。

---

## 协议审核标记（人工复核）

| 标记 | 条目 | 审核结论 | 后续动作 |
|---|---|---|---|
| `[REVIEW-ADOPTED]` | `signage.playlist` capability | 本文已采纳（见下方「采纳记录」），machine 事实源为 `registry/domains/signage/domain.yaml`。 | generated 产物由 `50-generate-axtp-protocol` 重跑生效。 |
| `[REVIEW-RESOLVED]` | Legacy `slideshow` 类型废弃 | AXTP 不保留 `slideshow`，Adapter 映射为 `image`。 | — |
| `[REVIEW-RESOLVED]` | `signage.media` 合并 | 播放项 URL 刷新已从 `signage.media` 合并到本草案。 | — |
| `[REVIEW-ASK]` | `PlaylistConfigChangedEvent` payload | 是否总是携带完整 playlists 需产品确认。 | 采纳前确认。 |
| `[REVIEW-ASK]` | `ResetPlaylistConfigParams` scope | 是否支持 scoped reset 需产品确认。 | 采纳前确认。 |

---

## 采纳记录 (Adoption)

采纳状态：**已采纳（Stage 30 adopt-protocol-draft）**。采纳日期 2026-06-17，采纳版本 v1.3。本文业务语义、schema、错误码与 legacy 映射已冻结为正式提案；machine 事实源为 `registry/domains/signage/domain.yaml`，由 `50-generate-axtp-protocol` 生成 `protocol/axtp.protocol.yaml` 与 `docs/generated/*`。

### 已分配的正式 ID

| 类别 | ID | 名称 |
|---|---|---|
| capability | `0x0D01` | `signage.playlist` |
| method | `0x0D01` | `signage.getPlaylistCapabilities` |
| method | `0x0D02` | `signage.getPlaylistConfig` |
| method | `0x0D03` | `signage.setPlaylistConfig` |
| method | `0x0D04` | `signage.resetPlaylistConfig` |
| method | `0x0D05` | `signage.getPlaylistItemUrl` |
| method | `0x0D06` | `signage.getPlaylistItem` |
| event | `0x0D01` | `signage.playlistConfigChanged` |

signage domain 区段 `0x0D00-0x0DFF`（DomainId `0x0D`）；method/event/capability 各自独立 ID 命名空间；bitOffset 在 signage domain 内各自从 0 连续（methods 0–5，events 0）。

### 错误码决策（复用 common）

采纳决策：**候选业务码 `SIGNAGE_PLAYLIST_*` 不新增为独立业务码，统一复用 common 错误码**。

| 草案候选业务码 | 采纳后的正式错误码 | 说明 |
|---|---|---|
| `SIGNAGE_PLAYLIST_EMPTY` | `INVALID_ARGUMENT`（0x000A） | 空播放列表 / 空播放项数组 → 参数非法。 |
| `SIGNAGE_PLAYLIST_ITEM_NOT_FOUND` | `NOT_FOUND`（0x000C） | itemId 不存在。 |
| `SIGNAGE_PLAYLIST_URL_EXPIRED` | `NOT_FOUND`（0x000C） | 资源 URL 已过期不可刷新。 |

signage domain 错误码区段 `0x0D00-0x0DFF` 保持空，未来如需细粒度业务码走 `40-amend-adopted-protocol`。

### 其他固化决策

- `PlaylistConfigChangedEvent.playlists` 保持 optional（设备可省略以减小 payload，客户端省略时调 `getPlaylistConfig` 校准）。
- `PlaylistItem.sort` 在同一 playlist 内**强制唯一**，重复值由 `setPlaylistConfig` 返回 `INVALID_ARGUMENT`。
- `PlaylistItem.duration` 强制 `> 0`（`min: 1`）。
- `ResetPlaylistConfigParams` 无参（全量恢复默认）。
- 调度优先级/冲突/空状态属运行时播放行为，归 `signage.playback` 域，不进 playlist 配置 schema。

### Schema 名映射

registry YAML 中 schema 名加 `Signage`/`Playlist` 前缀以保证全局唯一（与 `AudioAlgorithmConfig`、`DeviceInfo` 等既有命名一致）。草案正文的 schema 名（裸名）与 registry schema 名对应关系：

| 草案 schema 名 | registry schema 名 |
|---|---|
| `PlaylistCapabilitiesParams` | `SignageGetPlaylistCapabilitiesParams` |
| `PlaylistCapabilitiesResult` | `SignagePlaylistCapabilitiesResult` |
| `GetPlaylistConfigParams` | `SignageGetPlaylistConfigParams` |
| `PlaylistConfigResult` | `SignagePlaylistConfigResult` |
| `SetPlaylistConfigParams` | `SignageSetPlaylistConfigParams` |
| `ResetPlaylistConfigParams` | `SignageResetPlaylistConfigParams` |
| `GetPlaylistItemUrlParams` | `SignageGetPlaylistItemUrlParams` |
| `GetPlaylistItemUrlResult` | `SignageGetPlaylistItemUrlResult` |
| `PlaylistConfigChangedEvent` | `SignagePlaylistConfigChangedEvent` |
| `Playlist` | `SignagePlaylist` |
| `PlaylistItem` | `SignagePlaylistItem` |
| `ImageItemSettings` | `SignageImageItemSettings` |
| `VideoItemSettings` | `SignageVideoItemSettings` |
| `WebsiteItemSettings` | `SignageWebsiteItemSettings` |
| `ClockItemSettings` | `SignageClockItemSettings` |
| `UnsplashItemSettings` | `SignageUnsplashItemSettings` |
| `PowerBiItemSettings` | `SignagePowerBiItemSettings` |
| `GetPlaylistItemParams` | `SignageGetPlaylistItemParams` |
| `GetPlaylistItemResult` | `SignageGetPlaylistItemResult` |
| `ClockEntry` | `SignagePlaylistClockEntry` |
| `UnsplashPhoto` | `SignagePlaylistUnsplashPhoto` |
| `UnsplashUser` | `SignagePlaylistUnsplashUser` |

### 命名与附录差异说明

`workspace/registry-planning/candidates/method-candidates.md` §6.13 与 `event-candidates.md` §5.14 为非规范性占位规划，其中 signage 条目（如 `0x0D01`=`signage.listMedia`、`signage.playlistChanged`）与本文评审后的正式名不一致。已采纳正式名以本文 `domain.feature` 治理为准；附录将在 `50-generate-axtp-protocol` 阶段同步刷新。

### 后续约束

- 未来未采纳事实必须先更新本草案，再重新运行 adoption。
- 已采纳语义变更（ID/schema/错误码/字段语义）必须走 `40-amend-adopted-protocol`，不得直接改 registry。
- generated 产物（`protocol/axtp.protocol.yaml`、`docs/generated/*`、legacy `registry-patches.generated.yaml`）不在本阶段手改，由 `50-generate-axtp-protocol` 重跑生效。

---

## Amendment History（修订记录）

本节记录 Stage 40 `40-amend-adopted-protocol` 阶段对已采纳事实的修订。machine 事实源为 `registry/domains/signage/domain.yaml`，修订均先改本草案、再同步 YAML、最后由 `50-generate-axtp-protocol` 重跑 generated 产物。

### A1 2026-09-15 · 补充枚举声明（枚举类型说明）

- **变更**：registry YAML 中 4 个 `type: enum` 字段补充正式 `enum:` 列表（此前枚举值仅存于 description 散文）：
  - `SignagePlaylistItem.type` → `[image, website, video, clock, unsplash]`
  - `SignagePlaylist.type` → `[default, scheduled]`
  - `SignagePlaylistConfigChangedEvent.reason` → `[set_config, reset_config]`
  - `SignageGetPlaylistItemUrlResult.type` → `[image, video, website, unsplash]`
- **同步**：2 处 `supportedItemTypes`（`SignagePlaylistCapabilitiesResult` / `SignagePlaylistCapability`）description 措辞对齐为「`SignagePlaylistItem.type` 枚举值的子集」。
- **理由**：specs/40-codec.md 规定 enum「合法值 MUST 已声明」；`enum:` 列表为仓库既有机制（sport/cast/stream/video 域在用），补齐后 generated protocol.md 的 Value Restrictions 列与 MCP schema JSON 才能携带枚举事实。
- **兼容性**：非破坏——仅补充声明，wire 语义不变。
- **YAML target**：`contract/registry/domains/signage/domain.yaml`。

### A2 2026-09-15 · 拆分 PlaylistItemSettings + variants 判别机制

- **变更 1（拆分）**：聚合类型 `PlaylistItemSettings`（registry `SignagePlaylistItemSettings`，9 字段平铺、全 optional）拆分为 5 个 per-type 独立 schema，每类型内 required 语义恢复：

  | 草案类型 | registry 类型 | 字段（粗体 = 该类型内必填） |
  |---|---|---|
  | `ImageItemSettings` | `SignageImageItemSettings` | **urls**, **delaySeconds**, expiresAt |
  | `VideoItemSettings` | `SignageVideoItemSettings` | **url**, expiresAt, muted |
  | `WebsiteItemSettings` | `SignageWebsiteItemSettings` | **url**, ignoreCertificateError, refreshIntervalSecs |
  | `ClockItemSettings` | `SignageClockItemSettings` | **clocks** |
  | `UnsplashItemSettings` | `SignageUnsplashItemSettings` | **photos**, **delaySeconds**, expiresAt |

- **变更 2（新机制）**：`PlaylistItem.settings` 与 `GetPlaylistItemUrlResult.settings` 字段改为 `type: object` + `variants: { discriminator: type, mapping: { <枚举值>: <settings schema> } }`，判别绑定成为机器事实（generator 校验：discriminator 必须为同 schema 内已声明枚举值的兄弟字段、mapping 全覆盖枚举值、value 必须为已注册 schema）。`GetPlaylistItemUrlResult` 的 mapping 不含 `clock`。specs/40-codec.md 新增规范性条款。
- **变更 3（删除）**：聚合类型 `SignagePlaylistItemSettings` 自 registry YAML 删除。
- **fieldId 重排**：settings 内字段 ID 由聚合布局改为每类型从 0x01 连续，新旧映射：

  | 聚合旧 ID | 字段 | image | video | website | clock | unsplash |
  |---|---|---|---|---|---|---|
  | 0x01 | urls | 0x01 | — | — | — | — |
  | 0x02 | delaySeconds | 0x02 | — | — | — | 0x02 |
  | 0x03 | expiresAt | 0x03 | 0x02 | — | — | 0x03 |
  | 0x04 | url | — | 0x01 | 0x01 | — | — |
  | 0x05 | muted | — | 0x03 | — | — | — |
  | 0x06 | ignoreCertificateError | — | — | 0x02 | — | — |
  | 0x07 | refreshIntervalSecs | — | — | 0x03 | — | — |
  | 0x08 | clocks | — | — | — | 0x01 | — |
  | 0x09 | photos | — | — | — | — | 0x01 |

- **兼容性判定**：draft 阶段 breaking correction。依据：本 capability 全部事实 status 为 `draft`（未达 stable/mvp）；零设备实现（Drafted only）；无 signage conformance/test vectors；无 SDK 消费者。spec/v0.10.0–v0.16.0 各 tag 的 generated 产物曾包含聚合类型，本修订后不再包含——历史 tag 不受影响。
- **不变量**：method/event/capability ID 与 bitOffset、全部 method/event schema 名、父级字段 ID（`SignagePlaylistItem.settings`=0x05、`SignageGetPlaylistItemUrlResult.settings`=0x02）、`SignagePlaylist` / `SignagePlaylistItem` / `SignagePlaylistClockEntry` 全部字段及 ID、`SignagePlaylistUnsplashPhoto` 类型名（其字段结构变更见 A3）、错误码决策均不变。
- **YAML target**：`contract/registry/domains/signage/domain.yaml`；机制实现于 `tooling/generators/src/`（variants 穿透 + 校验 + protocol.md 渲染）。

### A3 2026-09-15 · UnsplashPhoto user 嵌套对齐

- **变更**：`SignagePlaylistUnsplashPhoto` 的扁平字段 `userName`（0x02）/ `userLink`（0x03）改为嵌套对象字段 `user`（0x02，类型 `SignagePlaylistUnsplashUser`）；新类型 `SignagePlaylistUnsplashUser`（草案名 `UnsplashUser`）：`name`=0x01（max_length 128）、`link`=0x02（max_length 2048），约束从旧扁平字段平移。
- **性质**：采纳时草案↔registry 的结构偏差修正——草案 v0.3 引入 `unsplash` 起全部 JSON 示例即为嵌套 `user: { name, link }` 写法，v1.4 采纳时 registry 误写为扁平。本修订以草案嵌套语义为准（registry 向草案对齐），草案 JSON 示例零改动。
- **兼容性判定**：draft 阶段 breaking correction（同 A2 依据：零实现、无 conformance vectors、无 SDK 消费者）。fieldId 影响：photo 层 `userName`=0x02 / `userLink`=0x03 → `user`=0x02（类型内 `name`=0x01、`link`=0x02）。
- **不变量**：`photo.url`=0x01、`SignagePlaylistUnsplashPhoto` 类型名、`UnsplashItemSettings.photos` 引用及其余全部字段不变。
- **YAML target**：`contract/registry/domains/signage/domain.yaml`。

### A4 2026-09-15 · 新增 powerbi 播放项类型 + getPlaylistItem 播放项整体刷新方法

- **变更 1（新类型）**：`PlaylistItem.type` 枚举新增 `powerbi`（线上字面值小写），新增 per-type settings schema `PowerBiItemSettings`（registry `SignagePowerBiItemSettings`，5 字段全必填：`groupId` / `dashboardId` / `embedUrl` / `token` / `expiresAt`），`PlaylistItem.settings` 的 `variants.mapping` 追加 `powerbi` 项。
- **变更 2（新方法）**：新增 `signage.getPlaylistItem`（method `0x0D06`，bitOffset 5，Device → Server）。params `{ itemId }`，result `{ item: PlaylistItem }`（与 `setPlaylistConfig` / `getPlaylistConfig` 的 `items` 元素同构）。行为契约：设备检测到 `settings.expiresAt` 临期时主动调用，用返回 `item` 整体替换本地播放项；非 URL 型资源（powerbi）MUST 使用本方法；URL 型资源 MAY 沿用 `getPlaylistItemUrl` 或统一使用本方法。读取方法，不触发事件。
- **变更 3（能力边界澄清）**：`getPlaylistItemUrl` 不支持 powerbi（embed token 属非 URL 型资源，返回 `NOT_SUPPORTED`，与 `clock` 同类排除）；`GetPlaylistItemUrlResult.type` 枚举保持 4 值不变，仅描述注明。
- **token 安全语义**：`token` 为 AAD access token（PowerBI audience `https://analysis.windows.net/powerbi/api`，约 60 分钟有效，tokenType=Aad 嵌入）；敏感凭证，仅经 RPC 通道传输，不得进资源 URL、不得写日志。`expiresAt` 必填恒有值且 MUST > 0（与其他类型的可选 `expiresAt`、0 表示永不过期的语义不同）。**扩散面**：token 会随 `getPlaylistConfig` 响应与 `playlistConfigChanged` 事件的可选 `playlists` 载荷重复下发，事件扇出还可复制到多个订阅 session——relay、订阅方与 SDK 必须将含 token 的载荷按敏感数据处理（不落日志、不进 debug dump）。
- **理由**：同步 device-sdk 新增的 powerbi 播放项与 `GetPlaylistItem` 指令（device-sdk commit 6e64f388，2026-09-10，首次发布于 1.5.0；本次对照契约版本 v1.6.0）。
- **兼容性**：draft 阶段非破坏扩展——纯新增枚举值 + variant + 方法；既有 method/event/capability ID、bitOffset、fieldId、错误码决策全部不变。对旧解码端的兼容依赖 specs/40-codec.md 的 unknown-enum policy（unknown enum 值为诊断保留，strict business validation MAY 在 decode 后拒绝）：不支持 `powerbi` 的对端收到含 powerbi 项的 `setPlaylistConfig` 可能整单拒绝，发送方 SHOULD 经 `supportedItemTypes` 能力协商避免向不支持的设备下发 powerbi 项。
- **不变量**：0x0D01-0x0D05 方法与 0x0D01 事件的 ID / bitOffset / schema 名；既有 5 种类型的全部 settings 字段；`GetPlaylistItemUrlResult` 枚举（4 值）；错误码复用 common 决策。
- **Legacy**：`getPlaylistItem` 无 legacy 对应（NearHub 证据源仅 3 命令），按 AXTP-only 新增登记于 §9.0；`powerbi` 类型同 `unsplash` 属 AXTP-only。
- **Capability 决策**：`SignagePlaylistCapability` 不新增 `supportsItemRefresh` 字段——device-sdk 能力面无此确认事实（仅有 `supportsUrlRefresh`），Stage 40 只写确认事实；方法可用性按标准回退（不支持时返回 `NOT_SUPPORTED`）。如产品需要独立开关，走后续 amendment。
- **YAML target**：`contract/registry/domains/signage/domain.yaml`。

### Follow-up 登记（非本次范围）

- 全仓库约 130 个 `type: enum` 字段缺 `enum:` 列表（audio 37 / network 37 / video 25 / device 14 / firmware 8 / software 5），generator 已加警告级报告；后续全域补全后再将硬校验写入 50-tooling validate-sources MUST 清单。
- `switch:`（按枚举值条件约束任意字段）if-then 机制登记为未来扩展（可将 `Playlist` 的 scheduled 条件必填从 prose 升级为结构化）。
- TLV8 单字段 value 上限 255 字节（specs/40-codec.md）与 signage 长字符串字段的张力（`token` max 8192、`embedUrl` / URL 类字段 max 2048）——按 `recommended_encoding: tlv` 实现 powerbi 刷新通道时这些字段无法单字段编解码，需要 codec 层的 TLV16 或分片策略（属未来 codec 扩展，本次不改 recommended_encoding 声明；实现方在 TLV 路径上应退回 JSON 编码）。
- 敏感字段结构化标记机制（区别于字段 description 散文约束，如 `sensitive: true` 或 secretRef 引用模式）登记为未来扩展——`PowerBiItemSettings.token` 为首个明文凭证字段，会随配置查询 / 事件载荷扩散，当前仅靠 prose 安全语义约束。

---

**变更历史：**

- **v1.6** — Stage 40 `40-amend-adopted-protocol` 修订（见「Amendment History」A4）：(1) `PlaylistItem.type` 新增 `powerbi`（PowerBI Dashboard 嵌入），新增 per-type schema `PowerBiItemSettings`（5 字段全必填：`groupId` / `dashboardId` / `embedUrl` / `token` / `expiresAt`；`token` 为 AAD access token 敏感凭证、仅经 RPC 通道传输且不得写日志，`expiresAt` 恒有值）；(2) 新增 `signage.getPlaylistItem`（`0x0D06` / bitOffset 5）播放项整体刷新方法，params `{ itemId }`、result `{ item }`，设备在资源临期时主动调用并整项替换本地播放项，powerbi MUST 走此通道；(3) `getPlaylistItemUrl` 明确不支持 powerbi（返回 `NOT_SUPPORTED`，与 `clock` 同类排除），`GetPlaylistItemUrlResult` 枚举保持 4 值；(4) 兼容性：draft 阶段非破坏纯新增，既有 ID / bitOffset / fieldId / 错误码不变；(5) 依据 device-sdk v1.6.0 契约同步；generated 产物由 `50-generate-axtp-protocol` 重跑生效。
- **v1.5** — Stage 40 `40-amend-adopted-protocol` 修订（见「Amendment History」A1/A2/A3）：(1) A1——registry YAML 4 个 enum 字段补正式 `enum:` 列表（`PlaylistItem.type` / `Playlist.type` / 事件 `reason` / `GetPlaylistItemUrlResult.type`），2 处 `supportedItemTypes` 措辞对齐；(2) A2——聚合类型 `PlaylistItemSettings` 拆分为 5 个 per-type settings schema（`ImageItemSettings` 等，每类型内 required 语义恢复、fieldId 从 0x01 连续重排），`settings` 字段新增 `variants: {discriminator: type, mapping}` 判别绑定机制（generator 新机制 + specs/40-codec.md 规范性条款），聚合类型删除；(3) A3——`UnsplashPhoto` 的扁平 `userName`/`userLink` 对齐为嵌套 `user` 对象（新类型 `UnsplashUser`），修正采纳时草案↔registry 结构偏差；(4) 兼容性判定为 draft 阶段 breaking correction（零实现、无 conformance vectors）；method/event/capability ID、bitOffset、父级字段 ID 全部不变；(5) generated 产物由 `50-generate-axtp-protocol` 重跑生效。
- **v1.4** — Stage 30 adopt-protocol-draft 采纳冻结（业务语义、schema、错误码、legacy 映射均不变）：(1) 新增 `## 采纳记录 (Adoption)` 节，记录已分配 ID（capability `0x0D01` / methods `0x0D01-0x0D05` / event `0x0D01`）、schema 名映射、命名与附录差异说明、后续约束；(2) 错误码决策——候选业务码 `SIGNAGE_PLAYLIST_*` 不新增，统一复用 common（`INVALID_ARGUMENT` / `NOT_FOUND`），signage 错误区段 `0x0D00-0x0DFF` 保持空；(3) 固化 `playlists` 可选、`sort` 同 playlist 内唯一、`duration > 0`、reset 无参；(4) 方法/事件速览表补 methodId/eventId 列并标记 `[REVIEW-ADOPTED]`，§8 错误表、§0/§10 readiness 同步更新；(5) registry YAML 已写入 `registry/domains/signage/domain.yaml`，generated 产物待 `50-generate-axtp-protocol` 重跑。
- **v1.3** — 对齐 20-draft-business-protocol skill 与 `system.lifecycle.md` 范式（业务语义、schema、错误码、legacy 映射均不变）：(1) 在 §0 速读结论后新增 `## JSON 示例约定` 节（RPC envelope 速查 + op=6/7/8 表）；(2) 将原集中式第 7 节 14 个 JSON 示例迁移到各 method/event 小节，每个 method 补齐 Request / Success Response / Error Response `d block` 内联示例 + 错误表 + 规则，每个 event 补齐 Event `d block` 示例 + 客户端处理建议表 + 规则；(3) 第 7 节改为 `## 7. 交互流程示例 Flow Examples`，只保留端到端 flow；(4) method/event 子标题改为带编号风格（`3.x.1`…`4.1.1`…）。Item 标题统一标注 `op=7` / `op=8` / `op=6`。
- **v1.2** — 格式修复与 per-method 结构补全：(1) 修复变更历史版本号顺序错乱，统一为倒序（原 v0.2–v0.7 为正序、v0.9/v0.8 为倒序，与顶部 v1.1/v1.0 倒序不一致）；(2) 为 `setPlaylistConfig` / `resetPlaylistConfig` 补 per-method「可能触发的事件」明细表，对齐标准 per-method 结构；(3) 统一 `setPlaylistConfig` 的 Result Schema 表述为「无 result body，仅返回标准 success status」。
- **v1.1** — 完善内部一致性与播放语义：(1) §5 capability 描述符表补 `supportsReset`，与 `PlaylistCapabilitiesResult`（§3.1/§6.3）三处字段对齐；(2) §8 增补 `NOT_FOUND`（0x000C）common 码与「错误码双轨制说明」，澄清候选业务码与 JSON 示例借用 common 码的对应关系；(3) 新增 §2.1「播放调度语义」，定义 default/scheduled 共存优先级、scheduled 重叠冲突与空状态；(4) §6.5 补 `duration = 0` 与 `sort` 重复排序语义；(5) §6.5 website `ignoreCertificateError` 加安全敏感 `[REVIEW-ASK]`；(6) 新增 §7.14 `SIGNAGE_PLAYLIST_URL_EXPIRED` 失败示例；(7) §4.1 说明事件 payload schema 与 flow 文档表述差异；(8) §12 补本次新增 `[REVIEW-ASK]` 项。
- **v1.0** — 结构完善：(1) 标题后新增元数据块（版本、归属域、Capability ID、适用范围），对齐 `device.enrollment` / `software.config` 等成熟草案模式；(2) 新增顶层"协议审核标记（人工复核）"节，提供快速概览；(3) 修复 §6.5 `Playlist` schema 条件必填规则块中断表格的格式问题。
- **v0.9** — Schema 精度提升：(1) `Playlist` schema 条件必填字段（`startDate`/`endDate`/`startTime`/`endTime`/`days`）从 prose 注释升级为正式条件必填规则块（与 `device.enrollment` 模式一致）；(2) `PlaylistItem.settings` 类型列从 `object` 更正为 `PlaylistItemSettings`。Legacy 映射改进：(3) §9.1 `SetPlaylistConfig` 新增 `slideshow` → `image` settings 字段级转换子表；(4) §9.3 `GetPlaylistItemUrl` 新增 `unsplash` AXTP-only 标注和 `slideshow` 响应转换细节；(5) §9.4 adapter 模式表新增 `GetPlaylistItemUrl` 和 `slideshow` → `image` 字段转换示例表。
- **v0.8** — 对齐仓库错误约定：业务语义校验错误由 `RPC_PARAM_INVALID`（曾误标 0x0002）统一改为 `INVALID_ARGUMENT`（0x000A，common），与 audio/software/device 草案一致；补全 §8 错误总表（新增 `INTERNAL_ERROR`、`PERMISSION_DENIED`，标注业务错误落点 0x0600-0x15FF）；为 `setPlaylistConfig` / `getPlaylistItemUrl` 增补方法级错误表；Legacy §9.0 标注 AXTP-only 新增能力、§9.3 细化各类型 URL 刷新分支。
- **v0.7** — 按 20-draft-business-protocol skill 规范重组全文：添加 frontmatter 和速读结论；补齐缺失 schema（`PlaylistCapabilitiesParams`/`Result`、`GetPlaylistConfigParams`、`PlaylistConfigResult`、`ResetPlaylistConfigParams`、`PlaylistConfigChangedEvent`）；新增 per-method detail blocks 和 per-event detail block；新增 Capability 字段表；扩展 Legacy 映射为 9.0-9.5 逐命令字段映射模式；补充 8 个 JSON 示例场景。`setPlaylistConfig` 响应简化为仅返回成功 status（无 result body）；`resetPlaylistConfig` 响应改为返回重置后的 `PlaylistConfigResult`；废弃 `slideshow` 播放项类型，Adapter 映射为 `image`。
- **v0.6** — 关闭 `scheduled` 类型时间区间约束决策——采用分条件约束。
- **v0.5** — 关闭三个核心设计决策：(1) 播放列表全量替换确认为硬替换，不引入版本号或 soft-delete；(2) Unsplash 等带过期时间播放项的 URL 刷新确认为设备端主动 Pull 模式；(3) `clock` 类型播放项调用 `getPlaylistItemUrl` 时服务端返回 `NOT_SUPPORTED`（0x0003）。
- **v0.4** — `GetPlaylistItemUrlResult` 从顶层互斥字段（`url` / `urls` / `photos`）模式重构为 `type` + `settings` 显式类型判别模式，与 `PlaylistItem` 结构统一。
- **v0.3** — 播放项类型新增 `unsplash`（Unsplash 图库幻灯片），新增 `UnsplashPhoto` schema；`signage.getPlaylistItemUrl` 返回结果新增 `photos` 字段。
- **v0.2** — 新增 `signage.getPlaylistItemUrl` 方法（原 `signage.media` URL 刷新功能），补充候选 schemas 和 JSON 示例。

---

## 0. 速读结论

| 项目 | 内容 |
|---|---|
| 这个能力做什么 | 数字标牌播放列表全量同步、查询、恢复默认、播放项资源 URL 刷新和播放项整体刷新。 |
| 当前状态 | draft（已采纳；见下方「采纳记录」） |
| 是否可直接实现 | 是（采纳后）。machine 事实源为 `registry/domains/signage/domain.yaml`，实现以 generated 为准。 |
| 主要交互 | RPC + EVENT |
| 是否使用 STREAM | 否 |
| Registry readiness | adopted |
| Conformance | needed |
| 主要未决问题 | 调度优先级/冲突/空状态（归 `signage.playback` 域，不进 playlist schema）；`ignoreCertificateError` 安全策略为产品定义。 |

---

## JSON 示例约定

本文中的 JSON 示例默认 RPC Session 已进入 `APP_READY`，`sid` 已由 Server 分配。Hello、Identify、Identified 属于 RPC Session 规范，不在每篇业务 feature 草案中重复。

示例使用 AXTP RPC JSON envelope。除本节的 envelope 速查外，后续 method/event/flow 示例默认只展示 RPC `d` 数据块，并在小节标题中标明对应 `op`：

```json
{ "sid": "12345678", "op": 7, "d": {} }
```

| op | 名称 | 用途 |
|---:|---|---|
| `6` | Event | 设备向客户端推送事件。 |
| `7` | Request | 客户端调用业务 method。 |
| `8` | RequestResponse | 设备返回业务 method 结果或错误。 |

本文中的 `sid="12345678"`、`id`（取 `1`–`14` 等示例值）、`intent=1` 均为示例值。正式 methodId、eventId、fieldId、errorCode、intent bit 由 registry 采纳后分配。

业务草案不得使用 JSON-RPC 2.0 外层格式作为 AXTP wire 示例；不要在 AXTP 示例中写 `jsonrpc`、JSON-RPC 外层 `id/method/params`，或把 JSON-RPC envelope 当作 AXTP envelope。

---

## 1. 功能说明

`signage.playlist` 定义数字标牌播放列表的全量同步、查询、恢复默认、播放项资源 URL 刷新和播放项整体刷新。

本文落实 `workspace/flows/signage-device-management.md` 中对 legacy `SetPlaylistConfig` / `GetPlaylistConfig` / `GetPlaylistItemUrl` 的最终定域。本文已采纳（v1.4），method / event / capability ID 与 bitOffset 已由 registry 分配（见上方「采纳记录」与各方法小节）。

**需求来源**：NearHub Launcher 数字标牌设备管理 — 播放列表管理。

**目标用户**：运维人员（通过云端管理控制台）、设备标牌播放器服务。

**目标行为**：云端全量同步播放列表到设备；设备查询当前播放列表；设备检测到资源 URL 即将过期时主动请求刷新获取新 URL；设备检测到非 URL 型资源（powerbi embed token）临期时经 `getPlaylistItem` 整项刷新。

**当前实现程度**：Drafted only — 无已有 registry/generated 事实。

---

## 2. 能力边界

| 类型 | 内容 |
|---|---|
| 包含 | 播放列表全量同步（硬替换）、查询、恢复默认、播放项资源 URL 刷新、播放项整体刷新（`getPlaylistItem`）、播放列表配置变更事件。 |
| 包含 | `default` 和 `scheduled` 两种播放列表类型。 |
| 包含 | 6 种播放项类型：`image`、`website`、`video`、`clock`、`unsplash`、`powerbi`。 |
| 不包含 | 播放控制（`signage.playback`）—— 播放/暂停/跳转归播放控制域。 |
| 不包含 | 设备外观配置（`software.config` target: `"launcher"`）。 |
| 不包含 | 系统调度（`system.lifecycle`）—— 定时重启/关机归系统生命周期域。 |
| 数据面 | 不使用 STREAM；所有操作都是 RPC request/response + event。 |

### 2.1 播放调度语义

`signage.playlist` 负责**下发与存储**播放列表配置，**不定义运行时播放控制**（播放/暂停/跳转归 `signage.playback` 域）。但配置层面的调度语义影响设备在多播放列表共存时的选择，需明确：

| 场景 | 推荐默认行为 | 状态 |
|---|---|---|
| `default` 与 `scheduled` 共存 | `scheduled` 命中当前时间窗口（日期 + 星期 + 时段）时优先播放；无 `scheduled` 命中时回落 `default`。 | `[REVIEW-ASK]` 产品确认调度优先级。 |
| 多个 `scheduled` 时间区间重叠 | 行为未定义；建议设备择一播放（按 `Playlist.id` 或数组顺序稳定选择），冲突不视为错误、不返回 `INVALID_ARGUMENT`。 | `[REVIEW-ASK]` 产品确认冲突策略。 |
| 无任何播放列表 | 设备显示空状态（黑屏或出厂画面），不报错。 | `[REVIEW-ASK]` 产品确认空状态呈现。 |

> `default` 类型播放列表的 `startDate`/`endDate`/`startTime`/`endTime`/`days` 字段不可设置（见 §6.5 条件必填规则），即 `default` 始终生效、不参与时间窗口匹配。

---

## 3. 方法

方法 ID、bitOffset 和 schema fieldId 已由 registry 采纳时分配（methods `0x0D01-0x0D06` / bitOffset 0–5，事件 `0x0D01`），machine 事实源为 `contract/registry/domains/signage/domain.yaml`。

### 3.0 方法速览

| Method | methodId | 调用类型 | 用途 | Params Schema | Result Schema | 是否触发事件 | 状态 |
|---|---|---|---|---|---|---|---|
| `signage.getPlaylistCapabilities` | `0x0D01` | query | 查询播放列表能力范围。 | `PlaylistCapabilitiesParams` | `PlaylistCapabilitiesResult` | 否 | [REVIEW-ADOPTED] |
| `signage.getPlaylistConfig` | `0x0D02` | query | 查询当前播放列表配置。 | `GetPlaylistConfigParams` | `PlaylistConfigResult` | 否 | [REVIEW-ADOPTED] |
| `signage.setPlaylistConfig` | `0x0D03` | command | 全量替换播放列表配置。 | `SetPlaylistConfigParams` | *(无 result body，仅返回标准 success status)* | 是：`playlistConfigChanged` | [REVIEW-ADOPTED] |
| `signage.resetPlaylistConfig` | `0x0D04` | action | 恢复默认播放列表配置。 | `ResetPlaylistConfigParams` | `PlaylistConfigResult` | 是：`playlistConfigChanged` | [REVIEW-ADOPTED] |
| `signage.getPlaylistItemUrl` | `0x0D05` | query | 按播放项 ID 获取最新资源 URL（URL 刷新）。 | `GetPlaylistItemUrlParams` | `GetPlaylistItemUrlResult` | 否 | [REVIEW-ADOPTED] |
| `signage.getPlaylistItem` | `0x0D06` | query | 按播放项 ID 获取完整播放项（整项刷新，powerbi 等非 URL 型资源的刷新通道）。 | `GetPlaylistItemParams` | `GetPlaylistItemResult` | 否 | [REVIEW-ADOPTED] |

### 3.1 `signage.getPlaylistCapabilities`

**用途**：查询 `signage.playlist` 能力范围，包括支持的播放项类型、数量限制和功能开关。

| 项 | 内容 |
|---|---|
| 调用类型 | query |
| Params Schema | `PlaylistCapabilitiesParams` |
| Result Schema | `PlaylistCapabilitiesResult` |
| 是否触发事件 | 否 |
| 幂等性 / 异步性 | 幂等；同步返回当前能力快照。 |
| 常见错误 | `NOT_SUPPORTED`, `INTERNAL_ERROR` |

#### 3.1.1 请求参数 Params：`PlaylistCapabilitiesParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| *(无必填参数)* | | | | | 读取完整能力范围，无需过滤参数。 |

#### 3.1.2 返回结果 Result：`PlaylistCapabilitiesResult`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `supportedItemTypes` | string[] | yes | `image`, `website`, `video`, `clock`, `unsplash`, `powerbi` | none | 支持的播放项类型。 |
| `maxPlaylists` | uint32 | no | product-defined | omitted | 最大播放列表数量。 |
| `maxItemsPerPlaylist` | uint32 | no | product-defined | omitted | 每个播放列表最大播放项数量。 |
| `supportsScheduledPlaylist` | boolean | yes | `true`, `false` | none | 是否支持 `scheduled` 类型播放列表。 |
| `supportsUrlRefresh` | boolean | yes | `true`, `false` | none | 是否支持播放项资源 URL 刷新（`getPlaylistItemUrl`）。 |
| `supportsReset` | boolean | yes | `true`, `false` | none | 是否支持恢复默认播放列表（`resetPlaylistConfig`）。 |

#### 3.1.3 d block 示例

request:

```json
{
  "id": 1,
  "method": "signage.getPlaylistCapabilities",
  "params": {}
}
```

success:

```json
{
  "id": 1,
  "status": {
    "ok": true,
    "code": 0
  },
  "result": {
    "supportedItemTypes": ["image", "website", "video", "clock", "unsplash", "powerbi"],
    "maxPlaylists": 10,
    "maxItemsPerPlaylist": 50,
    "supportsScheduledPlaylist": true,
    "supportsUrlRefresh": true,
    "supportsReset": true
  }
}
```

读法：request 为空，返回设备支持的播放项类型集合、数量限制与功能开关；`id` 由调用方分配且非零。success 中 `supportedItemTypes` 告诉调用方该设备支持哪些播放项类型，`supportsUrlRefresh: true` 表示设备具备 URL 刷新能力（URL 型资源主动调用 `getPlaylistItemUrl`，也可统一改用 `getPlaylistItem`，见 §3.6）。

#### 3.1.4 可能触发的事件

| Event | 触发条件 | Payload Schema | 客户端处理建议 |
|---|---|---|---|
| 无 | query method 不应因查询触发状态变化事件。 | none | 无需处理。 |

#### 3.1.5 错误

| 错误 | 场景 | 返回建议 |
|---|---|---|
| `NOT_SUPPORTED` | 设备不支持 `signage.playlist` 能力。 | 返回 unsupported capability。 |
| `INTERNAL_ERROR` | 设备读取能力信息失败。 | 返回内部错误，调用方可稍后重试。 |

#### 3.1.6 规则

- Request MUST 使用 `op=7`。
- Success / Error Response MUST 使用 `op=8`，并回显 Request 的 `d.id`。
- 草案阶段不得分配正式 methodId、bitOffset 或 fieldId。

### 3.2 `signage.getPlaylistConfig`

**用途**：查询当前播放列表配置。返回设备当前持有的完整播放列表数组。

| 项 | 内容 |
|---|---|
| 调用类型 | query |
| Params Schema | `GetPlaylistConfigParams` |
| Result Schema | `PlaylistConfigResult` |
| 是否触发事件 | 否 |
| 幂等性 / 异步性 | 幂等；同步返回当前配置快照。 |
| 常见错误 | `NOT_SUPPORTED`, `INTERNAL_ERROR` |

#### 3.2.1 请求参数 Params：`GetPlaylistConfigParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| *(无必填参数)* | | | | | 读取完整配置，无需过滤参数。 |

#### 3.2.2 返回结果 Result：`PlaylistConfigResult`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `playlists` | `Playlist[]` | yes | see `Playlist` schema | none | 当前播放列表配置数组。空数组表示无播放列表。 |

#### 3.2.3 d block 示例

request:

```json
{
  "id": 2,
  "method": "signage.getPlaylistConfig",
  "params": {}
}
```

success:

```json
{
  "id": 2,
  "status": {
    "ok": true,
    "code": 0
  },
  "result": {
    "playlists": []
  }
}
```

读法：request 为空，返回设备当前持有的完整 `playlists` 数组（含所有 `default` / `scheduled` 列表及其播放项）；success 中空 `playlists` 表示设备当前无播放列表（显示空状态），不视为错误。

#### 3.2.4 可能触发的事件

| Event | 触发条件 | Payload Schema | 客户端处理建议 |
|---|---|---|---|
| 无 | query method 不应因查询触发状态变化事件。 | none | 无需处理。 |

#### 3.2.5 错误

| 错误 | 场景 | 返回建议 |
|---|---|---|
| `NOT_SUPPORTED` | 设备不支持 `signage.playlist` 能力。 | 返回 unsupported capability。 |
| `INTERNAL_ERROR` | 设备读取播放列表配置失败。 | 返回内部错误，调用方可稍后重试。 |

#### 3.2.6 规则

- Request MUST 使用 `op=7`。
- Success / Error Response MUST 使用 `op=8`，并回显 Request 的 `d.id`。
- 草案阶段不得分配正式 methodId、bitOffset 或 fieldId。

### 3.3 `signage.setPlaylistConfig`

**用途**：全量替换播放列表配置。服务端是唯一权威，设备不编辑播放列表。第二次全量下发删除旧配置中未出现的播放项。

| 项 | 内容 |
|---|---|
| 调用类型 | command |
| Params Schema | `SetPlaylistConfigParams` |
| Result Schema | *(无 result body，仅返回标准 success status)* |
| 是否触发事件 | 是，全量替换成功后触发 `signage.playlistConfigChanged`。 |
| 幂等性 / 异步性 | 建议幂等；连续相同参数调用产生相同终态，可不重复触发事件。每次调用仍是硬替换。 |
| 常见错误 | `INVALID_ARGUMENT`, `NOT_SUPPORTED`, `PERMISSION_DENIED`, `SIGNAGE_PLAYLIST_EMPTY` |

#### 3.3.1 请求参数 Params：`SetPlaylistConfigParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `playlists` | `Playlist[]` | yes | see `Playlist` schema | none | 播放列表配置数组。非空。 |

#### 3.3.2 返回结果 Result

全量替换成功后**无 result body**，仅返回标准 success status。设备随后触发 `signage.playlistConfigChanged`。

#### 3.3.3 d block 示例

request:

主示例（`default` 类型播放列表，含 image / video / website / unsplash 四种播放项）：

```json
{
  "id": 3,
  "method": "signage.setPlaylistConfig",
  "params": {
    "playlists": [
      {
        "id": "550e8400-e29b-41d4-a716-446655440000",
        "type": "default",
        "items": [
          {
            "id": "6ba7b810-9dad-11d1-80b4-00c04fd430c8",
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
            "id": "6ba7b811-9dad-11d1-80b4-00c04fd430c8",
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
            "id": "6ba7b812-9dad-11d1-80b4-00c04fd430c8",
            "type": "website",
            "duration": 300,
            "sort": 2,
            "settings": {
              "url": "https://example.com",
              "ignoreCertificateError": false,
              "refreshIntervalSecs": 300
            }
          },
          {
            "id": "6ba7b813-9dad-11d1-80b4-00c04fd430c8",
            "type": "unsplash",
            "duration": 60,
            "sort": 3,
            "settings": {
              "photos": [
                {
                  "url": "https://images.unsplash.example.com/photo-1",
                  "user": { "name": "Alice Photographer", "link": "https://unsplash.example.com/@alice" }
                },
                {
                  "url": "https://images.unsplash.example.com/photo-2",
                  "user": { "name": "Bob Photographer", "link": "https://unsplash.example.com/@bob" }
                }
              ],
              "delaySeconds": 10,
              "expiresAt": 1704240000
            }
          }
        ]
      }
    ]
  }
}
```

补充 request 示例 A — `default` + `scheduled` 共存，展示 scheduled 时间约束（周一到周五 09:00–18:00）：

```json
{
  "id": 4,
  "method": "signage.setPlaylistConfig",
  "params": {
    "playlists": [
      {
        "id": "550e8400-e29b-41d4-a716-446655440000",
        "type": "default",
        "items": [
          {
            "id": "6ba7b810-9dad-11d1-80b4-00c04fd430c8",
            "type": "image",
            "duration": 60,
            "sort": 0,
            "settings": {
              "urls": ["https://example.com/resource/default-bg.jpg"],
              "delaySeconds": 5,
              "expiresAt": null
            }
          }
        ]
      },
      {
        "id": "660e8400-e29b-41d4-a716-446655440001",
        "type": "scheduled",
        "startDate": "2026-01-01",
        "endDate": "2026-12-31",
        "startTime": "09:00:00",
        "endTime": "18:00:00",
        "days": [1, 2, 3, 4, 5],
        "items": [
          {
            "id": "7ba7b810-9dad-11d1-80b4-00c04fd430c8",
            "type": "video",
            "duration": 300,
            "sort": 0,
            "settings": {
              "url": "https://example.com/resource/work-hours-content.mp4",
              "expiresAt": 1735689600,
              "muted": true
            }
          }
        ]
      }
    ]
  }
}
```

补充 request 示例 B — `clock` 类型播放项（无远程 URL 资源，`settings` 只含 `clocks`）：

```json
{
  "id": 5,
  "method": "signage.setPlaylistConfig",
  "params": {
    "playlists": [
      {
        "id": "550e8400-e29b-41d4-a716-446655440000",
        "type": "default",
        "items": [
          {
            "id": "8ba7b810-9dad-11d1-80b4-00c04fd430c8",
            "type": "clock",
            "duration": 600,
            "sort": 0,
            "settings": {
              "clocks": [
                { "timezone": "Asia/Shanghai", "label": "北京" },
                { "timezone": "America/New_York", "label": "纽约" },
                { "timezone": "Europe/London", "label": "伦敦" }
              ]
            }
          }
        ]
      }
    ]
  }
}
```

补充 request 示例 C — `powerbi` 类型播放项（PowerBI Dashboard 嵌入；`settings` 5 字段全必填，`token` 为敏感凭证，`expiresAt` 恒有值）：

```json
{
  "id": 17,
  "method": "signage.setPlaylistConfig",
  "params": {
    "playlists": [
      {
        "id": "550e8400-e29b-41d4-a716-446655440000",
        "type": "default",
        "items": [
          {
            "id": "9ba7b810-9dad-11d1-80b4-00c04fd430c8",
            "type": "powerbi",
            "duration": 600,
            "sort": 0,
            "settings": {
              "groupId": "f089354e-3a18-4d43-8db5-3c58a1a0c9f2",
              "dashboardId": "69ff5868-12d6-4f1c-8c2f-7b6b6bc11d4b",
              "embedUrl": "https://app.powerbi.com/dashboardEmbed?dashboardId=69ff5868-12d6-4f1c-8c2f-7b6b6bc11d4b&groupId=f089354e-3a18-4d43-8db5-3c58a1a0c9f2",
              "token": "eyJhbGciOiJSUzI1NiIs...<aad-access-token>",
              "expiresAt": 1704153600
            }
          }
        ]
      }
    ]
  }
}
```

success:

```json
{
  "id": 3,
  "status": {
    "ok": true,
    "code": 0
  }
}
```

error:

失败示例 A — `scheduled` 时间约束违反（`startDate == endDate` 但 `startTime > endTime`）：

```json
{
  "id": 12,
  "status": {
    "ok": false,
    "code": 10,
    "msg": "Invalid scheduled time range: startTime must be <= endTime when startDate equals endDate.",
    "details": {
      "field": "playlists[1].startTime"
    }
  }
}
```

失败示例 B — `playlists` 为空数组（候选业务码 `SIGNAGE_PLAYLIST_EMPTY`，示例借用 common `INVALID_ARGUMENT` 并写入 `candidateError`，见 §8 双轨制说明）：

```json
{
  "id": 13,
  "status": {
    "ok": false,
    "code": 10,
    "msg": "Playlists array must not be empty.",
    "details": {
      "candidateError": "SIGNAGE_PLAYLIST_EMPTY"
    }
  }
}
```

读法：request 中 `startDate` < `endDate`、`startTime < endTime` 落在同一天内（不跨午夜），`days` 指定周一到周五生效，`scheduled` 命中时间窗口时优先播放否则回落 `default`（见 §2.1）；`clock` 类型不依赖远程 URL，`getPlaylistItemUrl` 对 `clock` 返回 `NOT_SUPPORTED`；`powerbi` 类型的 `token` 为敏感凭证（AAD access token，约 60 分钟有效），仅经 RPC 通道传输，不得写入资源 URL 或日志，设备在 `expiresAt` 临期时经 `getPlaylistItem` 整项刷新（见 §3.6）。success 只有 `status`、不携带业务 `result`，调用方通过 `getPlaylistConfig` 或等待 `playlistConfigChanged` 事件确认落地。error 失败响应使用 `op=8` 且不携带业务 `result`，校验失败不落地、不触发事件；`code: 10` 对应 common `INVALID_ARGUMENT`（0x000A）。

#### 3.3.4 可能触发的事件

| Event | 触发条件 | Payload Schema | 客户端处理建议 |
|---|---|---|---|
| `signage.playlistConfigChanged` | `setPlaylistConfig` 全量替换成功。 | `PlaylistConfigChangedEvent` | 标记本地播放列表缓存失效；需要完整配置时调用 `signage.getPlaylistConfig` 校准。 |

该方法成功后 SHOULD 触发 `signage.playlistConfigChanged`，事件 `d` block 示例（`reason: "set_config"`）：

```json
{
  "event": "signage.playlistConfigChanged",
  "intent": 1,
  "data": {
    "reason": "set_config",
    "playlists": [
      {
        "id": "550e8400-e29b-41d4-a716-446655440000",
        "type": "default",
        "items": [
          {
            "id": "6ba7b810-9dad-11d1-80b4-00c04fd430c8",
            "type": "image",
            "duration": 60,
            "sort": 0,
            "settings": {
              "urls": ["https://example.com/resource/file-1.jpg"],
              "delaySeconds": 5,
              "expiresAt": 1704067200
            }
          }
        ]
      }
    ]
  }
}
```

读法：`reason` 标明变更由 `setPlaylistConfig` 触发。`playlists` 可选携带变更后完整配置，客户端可直接使用，也可省略后通过 `getPlaylistConfig` 获取（见 §4.1）。

#### 3.3.5 错误

| Error | Code | 触发条件 |
|---|---|---|
| `SUCCESS` | 0x0000 | 全量替换成功，触发 `playlistConfigChanged`。 |
| `INVALID_ARGUMENT` | 0x000A | scheduled 时间区间约束违反（见 `Playlist` schema）；播放项 `settings` 字段非法（含 `powerbi` 的 5 个必填字段缺失或超长）。 |
| `SIGNAGE_PLAYLIST_EMPTY` | TBD | `playlists` 为空数组，或某播放列表 `items` 为空。 |
| `NOT_SUPPORTED` | 0x0003 | 设备不支持 `signage.playlist` 能力或当前模式不可写。 |
| `PERMISSION_DENIED` | 0x0009 | 调用方无权修改播放列表配置。 |

#### 3.3.6 规则

- Request MUST 使用 `op=7`。
- Success / Error Response MUST 使用 `op=8`，并回显 Request 的 `d.id`。
- 失败响应 MUST NOT 携带业务 `result`；校验失败不触发 `playlistConfigChanged`。
- 状态未实际变化时，method MAY 成功返回且 MAY 不重复触发事件。
- 草案阶段不得分配正式 methodId、bitOffset 或 fieldId。

### 3.4 `signage.resetPlaylistConfig`

**用途**：恢复默认播放列表配置。成功后设备回到出厂默认播放列表状态。

| 项 | 内容 |
|---|---|
| 调用类型 | action |
| Params Schema | `ResetPlaylistConfigParams` |
| Result Schema | `PlaylistConfigResult` |
| 是否触发事件 | 是，恢复默认成功后触发 `signage.playlistConfigChanged`（`reason: "reset_config"`）。 |
| 幂等性 / 异步性 | 幂等；重复执行重置到相同默认状态。 |
| 常见错误 | `NOT_SUPPORTED`, `INTERNAL_ERROR` |

#### 3.4.1 请求参数 Params：`ResetPlaylistConfigParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| *(无必填参数)* | | | | | 恢复默认配置。`[REVIEW-ASK]` 是否支持 scoped reset（如只重置 scheduled 播放列表）需产品确认。 |

#### 3.4.2 返回结果 Result：`PlaylistConfigResult`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `playlists` | `Playlist[]` | yes | see `Playlist` | none | 重置后的默认播放列表配置数组。 |

#### 3.4.3 d block 示例

request:

```json
{
  "id": 6,
  "method": "signage.resetPlaylistConfig",
  "params": {}
}
```

success:

```json
{
  "id": 6,
  "status": {
    "ok": true,
    "code": 0
  },
  "result": {
    "playlists": []
  }
}
```

读法：request 为空，设备恢复出厂默认播放列表配置（本例默认为空播放列表，由产品定义）；success 返回重置后的默认播放列表配置（此处为空数组，表示出厂默认无播放列表）。

#### 3.4.4 可能触发的事件

| Event | 触发条件 | Payload Schema | 客户端处理建议 |
|---|---|---|---|
| `signage.playlistConfigChanged` | `resetPlaylistConfig` 恢复默认成功。 | `PlaylistConfigChangedEvent` | payload `reason` 为 `reset_config`；标记本地缓存失效，按需调用 `signage.getPlaylistConfig` 校准。 |

该方法成功后 SHOULD 触发 `signage.playlistConfigChanged`，事件 `d` block 示例（`reason: "reset_config"`）：

```json
{
  "event": "signage.playlistConfigChanged",
  "intent": 1,
  "data": {
    "reason": "reset_config",
    "playlists": []
  }
}
```

读法：客户端收到 `reason: "reset_config"` 后应丢弃本地播放列表缓存，用返回的 `playlists` 或重新 `getPlaylistConfig` 校准。

#### 3.4.5 错误

| 错误 | 场景 | 返回建议 |
|---|---|---|
| `NOT_SUPPORTED` | 设备不支持 `signage.playlist` 能力，或 capability `supportsReset: false`。 | 返回 unsupported capability / operation。 |
| `INTERNAL_ERROR` | 设备恢复默认失败。 | 返回内部错误，调用方可稍后重试。 |

#### 3.4.6 规则

- Request MUST 使用 `op=7`。
- Success / Error Response MUST 使用 `op=8`，并回显 Request 的 `d.id`。
- 失败响应 MUST NOT 携带业务 `result`；失败不触发 `playlistConfigChanged`。
- 草案阶段不得分配正式 methodId、bitOffset 或 fieldId。

### 3.5 `signage.getPlaylistItemUrl`

**用途**：按播放项 ID 获取最新资源 URL（URL 刷新）。设备检测到资源 URL 即将过期时主动调用此方法。`clock` 类型播放项无 URL 资源、`powerbi` 类型播放项携带非 URL 型 embed token 资源，调用此方法均返回 `NOT_SUPPORTED`（powerbi 改用 `getPlaylistItem` 整项刷新，见 §3.6）。

| 项 | 内容 |
|---|---|
| 调用类型 | query |
| Params Schema | `GetPlaylistItemUrlParams` |
| Result Schema | `GetPlaylistItemUrlResult` |
| 是否触发事件 | 否 |
| 幂等性 / 异步性 | 幂等；返回当前有效 URL，同一 itemId 在有效期内返回相同结果。 |
| 常见错误 | `NOT_SUPPORTED`, `SIGNAGE_PLAYLIST_ITEM_NOT_FOUND`, `SIGNAGE_PLAYLIST_URL_EXPIRED` |

#### 3.5.1 请求参数 Params：`GetPlaylistItemUrlParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `itemId` | string (UUID) | yes | UUID format | none | 播放项唯一标识。 |

#### 3.5.2 返回结果 Result：`GetPlaylistItemUrlResult`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `type` | enum | yes | `image`, `video`, `website`, `unsplash` | none | 播放项类型。判别 `settings` 的 variant schema。`clock` 与 `powerbi` 类型不涉及 URL 资源刷新（`powerbi` 走 `getPlaylistItem`，见 §3.6）。 |
| `settings` | per-type settings | yes | 由 `type` 经 `variants` 判别 | none | 刷新后的完整设置。设备可直接用此值替换本地缓存的 `settings`。 |

`settings` 字段携带 `variants: { discriminator: type, mapping: {...} }`，按 `type` 值对应 settings schema（见 §6.5）：

| `type` | settings schema |
|---|---|
| `image` | `ImageItemSettings`（registry `SignageImageItemSettings`） |
| `video` | `VideoItemSettings`（registry `SignageVideoItemSettings`） |
| `website` | `WebsiteItemSettings`（registry `SignageWebsiteItemSettings`） |
| `unsplash` | `UnsplashItemSettings`（registry `SignageUnsplashItemSettings`） |

#### 3.5.3 d block 示例

request:

```json
{
  "id": 7,
  "method": "signage.getPlaylistItemUrl",
  "params": {
    "itemId": "6ba7b810-9dad-11d1-80b4-00c04fd430c8"
  }
}
```

success:

主示例（`image` 类型）：

```json
{
  "id": 7,
  "status": {
    "ok": true,
    "code": 0
  },
  "result": {
    "type": "image",
    "settings": {
      "urls": ["https://example.com/resource/file-1-new.jpg"],
      "delaySeconds": 5,
      "expiresAt": 1704153600
    }
  }
}
```

补充 success 示例（video / website / unsplash 类型）：

```json
{
  "id": 8,
  "status": { "ok": true, "code": 0 },
  "result": {
    "type": "video",
    "settings": {
      "url": "https://example.com/resource/video-1-new.mp4",
      "expiresAt": 1704153600,
      "muted": false
    }
  }
}
```

```json
{
  "id": 9,
  "status": { "ok": true, "code": 0 },
  "result": {
    "type": "website",
    "settings": {
      "url": "https://example.com/page",
      "ignoreCertificateError": false,
      "refreshIntervalSecs": 300
    }
  }
}
```

```json
{
  "id": 10,
  "status": { "ok": true, "code": 0 },
  "result": {
    "type": "unsplash",
    "settings": {
      "photos": [
        { "url": "https://images.unsplash.example.com/photo-1-new", "user": { "name": "Alice Photographer", "link": "https://unsplash.example.com/@alice" } },
        { "url": "https://images.unsplash.example.com/photo-2-new", "user": { "name": "Bob Photographer", "link": "https://unsplash.example.com/@bob" } }
      ],
      "delaySeconds": 10,
      "expiresAt": 1704326400
    }
  }
}
```

error:

失败示例 A — `clock` 类型不支持 URL 刷新：

```json
{
  "id": 11,
  "status": {
    "ok": false,
    "code": 3,
    "msg": "URL refresh is not supported for clock type playlist items."
  }
}
```

失败示例 B — `itemId` 不存在（候选业务码 `SIGNAGE_PLAYLIST_ITEM_NOT_FOUND`，示例借用 common `NOT_FOUND` 并写入 `candidateError`）：

```json
{
  "id": 7,
  "status": {
    "ok": false,
    "code": 12,
    "msg": "Playlist item not found.",
    "details": {
      "candidateError": "SIGNAGE_PLAYLIST_ITEM_NOT_FOUND"
    }
  }
}
```

失败示例 C — 资源 URL 已过期且无法刷新（候选业务码 `SIGNAGE_PLAYLIST_URL_EXPIRED`，示例借用 common `NOT_FOUND` 并写入 `candidateError`）：

```json
{
  "id": 14,
  "status": {
    "ok": false,
    "code": 12,
    "msg": "Playlist item URL has expired and cannot be refreshed.",
    "details": {
      "candidateError": "SIGNAGE_PLAYLIST_URL_EXPIRED"
    }
  }
}
```

读法：request 中设备检测到 `itemId` 对应播放项的 URL 即将过期时主动调用；success 中设备用返回的 `settings` 直接替换本地缓存的该 item `settings`，不同 `type` 对应不同字段集合（见上方子集表）；error 中设备收到 `SIGNAGE_PLAYLIST_URL_EXPIRED` 类错误应停止对该 `itemId` 重试，并通过重新 `getPlaylistConfig` 或等待 `playlistConfigChanged` 获取更新后的配置，`code: 12` 对应 common `NOT_FOUND`（0x000C），失败响应不携带业务 `result`。

#### 3.5.4 可能触发的事件

| Event | 触发条件 | Payload Schema | 客户端处理建议 |
|---|---|---|---|
| 无 | query method 不应因查询触发状态变化事件。 | none | 无需处理。 |

#### 3.5.5 错误

| Error | Code | 触发条件 |
|---|---|---|
| `SUCCESS` | 0x0000 | 返回刷新后的 `type` + `settings`。 |
| `NOT_SUPPORTED` | 0x0003 | 播放项类型为 `clock`（无远程 URL 资源）或 `powerbi`（非 URL 型 embed token 资源，改用 `getPlaylistItem`）。 |
| `SIGNAGE_PLAYLIST_ITEM_NOT_FOUND` | TBD | `itemId` 不存在于当前播放列表中。 |
| `SIGNAGE_PLAYLIST_URL_EXPIRED` | TBD | 资源 URL 已不可用且无法刷新。 |

#### 3.5.6 规则

- Request MUST 使用 `op=7`。
- Success / Error Response MUST 使用 `op=8`，并回显 Request 的 `d.id`。
- `clock` 与 `powerbi` 类型调用此方法 MUST 返回 `NOT_SUPPORTED`，不返回 `settings`（`powerbi` 的 embed token 刷新走 `getPlaylistItem`，见 §3.6）。
- URL 型资源（image / video / website / unsplash）MAY 使用本方法或 `getPlaylistItem` 二选一刷新；非 URL 型资源（powerbi）MUST 使用 `getPlaylistItem`。
- 草案阶段不得分配正式 methodId、bitOffset 或 fieldId。

### 3.6 `signage.getPlaylistItem`

**用途**：按播放项 ID 获取完整播放项（整项刷新）。设备检测到 `settings.expiresAt` 临期时主动调用，用返回的 `item` **整体替换**本地对应播放项。`powerbi` 等非 URL 型资源（embed token）MUST 使用本方法刷新；URL 型资源 MAY 沿用 `getPlaylistItemUrl` 或统一使用本方法。

| 项 | 内容 |
|---|---|
| 调用类型 | query |
| Params Schema | `GetPlaylistItemParams` |
| Result Schema | `GetPlaylistItemResult` |
| 是否触发事件 | 否 |
| 幂等性 / 异步性 | 读取语义幂等、不触发事件；但返回的 `token` / `expiresAt` 每次可能不同（服务端按需铸发新 token），调用方不得对响应做缓存或去重。 |
| 常见错误 | `NOT_SUPPORTED`, `SIGNAGE_PLAYLIST_ITEM_NOT_FOUND`, `INTERNAL_ERROR` |

#### 3.6.1 请求参数 Params：`GetPlaylistItemParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `itemId` | string (UUID) | yes | UUID format | none | 播放项唯一标识。与 `getPlaylistItemUrl` 使用同一标识空间。 |

#### 3.6.2 返回结果 Result：`GetPlaylistItemResult`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `item` | `PlaylistItem` | yes | see `PlaylistItem` schema | none | 刷新后的完整播放项，与 `setPlaylistConfig` / `getPlaylistConfig` 的 `items` 元素同构。设备用此值整体替换本地对应播放项。 |

#### 3.6.3 d block 示例

request:

```json
{
  "id": 15,
  "method": "signage.getPlaylistItem",
  "params": {
    "itemId": "9ba7b810-9dad-11d1-80b4-00c04fd430c8"
  }
}
```

success:

主示例（`powerbi` 整项刷新——返回含新 token 的完整播放项）：

```json
{
  "id": 15,
  "status": {
    "ok": true,
    "code": 0
  },
  "result": {
    "item": {
      "id": "9ba7b810-9dad-11d1-80b4-00c04fd430c8",
      "type": "powerbi",
      "duration": 600,
      "sort": 0,
      "settings": {
        "groupId": "f089354e-3a18-4d43-8db5-3c58a1a0c9f2",
        "dashboardId": "69ff5868-12d6-4f1c-8c2f-7b6b6bc11d4b",
        "embedUrl": "https://app.powerbi.com/dashboardEmbed?dashboardId=69ff5868-12d6-4f1c-8c2f-7b6b6bc11d4b&groupId=f089354e-3a18-4d43-8db5-3c58a1a0c9f2",
        "token": "eyJhbGciOiJSUzI1NiIs...<new-aad-access-token>",
        "expiresAt": 1704157200
      }
    }
  }
}
```

error:

失败示例 — `itemId` 不存在（候选业务码 `SIGNAGE_PLAYLIST_ITEM_NOT_FOUND`，示例借用 common `NOT_FOUND` 并写入 `candidateError`）：

```json
{
  "id": 15,
  "status": {
    "ok": false,
    "code": 12,
    "msg": "Playlist item not found.",
    "details": {
      "candidateError": "SIGNAGE_PLAYLIST_ITEM_NOT_FOUND"
    }
  }
}
```

读法：request 中设备检测到 `itemId` 对应播放项的 `settings.expiresAt` 临期时主动调用；success 中设备用返回的 `item` 整体替换本地播放项（`id` / `type` / `duration` / `sort` / `settings` 全部以返回值为准，不做字段级合并），`powerbi` 的 `embedUrl` 稳定不变、实际刷新的是 `token` 与 `expiresAt`；error 中 `code: 12` 对应 common `NOT_FOUND`（0x000C），设备应停止对该 `itemId` 重试，通过重新 `getPlaylistConfig` 或等待 `playlistConfigChanged` 校准（与 §3.5.3 同策略）。

#### 3.6.4 可能触发的事件

| Event | 触发条件 | Payload Schema | 客户端处理建议 |
|---|---|---|---|
| 无 | query method 不应因查询触发状态变化事件。 | none | 无需处理。 |

#### 3.6.5 错误

| Error | Code | 触发条件 |
|---|---|---|
| `SUCCESS` | 0x0000 | 返回刷新后的完整 `item`。 |
| `NOT_SUPPORTED` | 0x0003 | 对端不支持 `signage.playlist` 能力。 |
| `SIGNAGE_PLAYLIST_ITEM_NOT_FOUND` | TBD | `itemId` 不存在于当前播放列表中（通常表示条目已被全量替换删除）。设备应停止对该 `itemId` 重试，改走全量校准。 |
| `INTERNAL_ERROR` | 0x000E | 对端生成刷新资源失败（如 PowerBI embed token 签发失败、上游 OAuth 暂不可用）。条目本身仍可能有效，设备 SHOULD 稍后重试，不得据此丢弃本地播放项。 |

#### 3.6.6 规则

- Request MUST 使用 `op=7`。
- Success / Error Response MUST 使用 `op=8`，并回显 Request 的 `d.id`。
- 设备检测到 `settings.expiresAt` 临期时 SHOULD 主动调用本方法，并用返回的 `item` 整体替换本地对应播放项。
- `powerbi` 类型播放项 MUST 使用本方法刷新（embed token 属非 URL 型资源，`getPlaylistItemUrl` 返回 `NOT_SUPPORTED`）。
- URL 型资源（image / video / website / unsplash）MAY 使用本方法或 `getPlaylistItemUrl` 二选一刷新。
- `result.item.id` MUST 等于 `params.itemId`；设备若收到不一致的响应，SHOULD 丢弃本次结果并调用 `getPlaylistConfig` 校准（云端可能已删除并以新 UUID 重建条目）。
- 本方法为读取方法，MUST NOT 触发 `playlistConfigChanged` 事件。

---

## 4. 事件

### 4.0 事件速览

| Event | eventId | 触发条件 | Payload Schema | 客户端处理建议 | 状态 |
|---|---|---|---|---|---|
| `signage.playlistConfigChanged` | `0x0D01` | `setPlaylistConfig` 或 `resetPlaylistConfig` 成功改变播放列表配置后发出。 | `PlaylistConfigChangedEvent` | 用事件通知触发 UI 刷新或本地缓存更新；如需完整配置，调用 `getPlaylistConfig` 校准。 | [REVIEW-ADOPTED] |

### 4.1 `signage.playlistConfigChanged`

**触发条件**：

- `setPlaylistConfig` 全量替换成功。
- `resetPlaylistConfig` 恢复默认成功。

#### 4.1.1 Payload：`PlaylistConfigChangedEvent`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `reason` | string (enum) | yes | `set_config`, `reset_config` | none | 变更原因。`set_config` 由 `setPlaylistConfig` 触发；`reset_config` 由 `resetPlaylistConfig` 触发。 |
| `playlists` | `Playlist[]` | no | see `Playlist` schema | omitted | 变更后的完整播放列表。设备可选择省略以减小 payload，客户端调用 `getPlaylistConfig` 获取。`[REVIEW-ASK]` 是否总是携带完整 playlists 需产品确认。 |

#### 4.1.2 d block 示例

```json
{
  "event": "signage.playlistConfigChanged",
  "intent": 1,
  "data": {
    "reason": "set_config",
    "playlists": [
      {
        "id": "550e8400-e29b-41d4-a716-446655440000",
        "type": "default",
        "items": [
          {
            "id": "6ba7b810-9dad-11d1-80b4-00c04fd430c8",
            "type": "image",
            "duration": 60,
            "sort": 0,
            "settings": {
              "urls": ["https://example.com/resource/file-1.jpg"],
              "delaySeconds": 5,
              "expiresAt": 1704067200
            }
          }
        ]
      }
    ]
  }
}
```

读法：`reason` 标明变更来源；`playlists` 为变更后完整配置，客户端可直接使用，也可省略后通过 `getPlaylistConfig` 获取。

#### 4.1.3 客户端处理建议

| 场景 | 建议 |
|---|---|
| payload 携带完整 `playlists` | 可直接更新 UI 或本地缓存。 |
| payload 省略 `playlists` | 调用 `signage.getPlaylistConfig` 校准完整配置。 |
| event 丢失或重连 | 重连后主动调用 `signage.getPlaylistConfig` 校准。 |
| 多端同时控制 | 以最新一次 `setPlaylistConfig` 为权威；本地缓存按事件失效后重新拉取。 |

#### 4.1.4 规则

- Event MUST 使用 `op=6`。
- Event MUST NOT 携带 `d.id`。
- Event payload MUST 放在 `d.data` 中。
- 失败的 `setPlaylistConfig` / `resetPlaylistConfig` MUST NOT 触发本事件。

> **事件 payload schema 说明**：本草案为 `playlistConfigChanged` 定义独立 payload schema `PlaylistConfigChangedEvent`（含 `reason` + 可选 `playlists`）。`workspace/flows/signage-device-management.md` §7 将该事件 payload 记为 `PlaylistConfigResult` 属场景级简化表述；**正式 payload schema 以本草案为准**。如需同步 flow 文档表述，转 10-plan-protocol-flow（不在 20-draft-business-protocol 边界内）。

---

## 5. Capability

Capability name: `signage.playlist`。

| 能力字段 | 类型 | 必填 | 取值范围 / 枚举 | 说明 |
|---|---|---:|---|---|
| `capability` | string | yes | fixed `signage.playlist` | capability 名称。 |
| `supportedItemTypes` | string[] | yes | `image`, `website`, `video`, `clock`, `unsplash`, `powerbi` | 支持的播放项类型。 |
| `maxPlaylists` | uint32 | no | product-defined | 最大播放列表数量。 |
| `maxItemsPerPlaylist` | uint32 | no | product-defined | 每个播放列表最大播放项数量。 |
| `supportsScheduledPlaylist` | boolean | yes | `true`, `false` | 是否支持 `scheduled` 类型播放列表。 |
| `supportsUrlRefresh` | boolean | yes | `true`, `false` | 是否支持播放项资源 URL 刷新。 |
| `supportsReset` | boolean | yes | `true`, `false` | 是否支持恢复默认播放列表（`resetPlaylistConfig`）。 |

---

## 6. 字段 / Schemas

### 6.1 Schema 层级速览

```text
请求 Schemas:
  PlaylistCapabilitiesParams  → getPlaylistCapabilities
  GetPlaylistConfigParams     → getPlaylistConfig
  SetPlaylistConfigParams     → setPlaylistConfig
  ResetPlaylistConfigParams   → resetPlaylistConfig
  GetPlaylistItemUrlParams    → getPlaylistItemUrl
  GetPlaylistItemParams       → getPlaylistItem

响应 Schemas:
  PlaylistCapabilitiesResult  ← getPlaylistCapabilities
  PlaylistConfigResult        ← getPlaylistConfig / resetPlaylistConfig
  GetPlaylistItemUrlResult    ← getPlaylistItemUrl
  GetPlaylistItemResult       ← getPlaylistItem

事件 Payload Schemas:
  PlaylistConfigChangedEvent  ← playlistConfigChanged

共享对象:
  Playlist, PlaylistItem
  ImageItemSettings, VideoItemSettings, WebsiteItemSettings, ClockItemSettings, UnsplashItemSettings, PowerBiItemSettings
    （PlaylistItem.settings / GetPlaylistItemUrlResult.settings 经 variants 按 type 判别；
      GetPlaylistItemUrlResult 的判别不含 clock / powerbi）
  ClockEntry, UnsplashPhoto, UnsplashUser
```

### 6.2 请求 Schemas

#### `SetPlaylistConfigParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `playlists` | `Playlist[]` | yes | see `Playlist` | none | 播放列表配置数组。非空。 |

#### `GetPlaylistItemUrlParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `itemId` | string (UUID) | yes | UUID format | none | 播放项唯一标识。 |

#### `GetPlaylistItemParams`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `itemId` | string (UUID) | yes | UUID format | none | 播放项唯一标识。与 `getPlaylistItemUrl` 使用同一标识空间。 |

`PlaylistCapabilitiesParams`、`GetPlaylistConfigParams`、`ResetPlaylistConfigParams`：无必填参数，详见各方法 per-method 字段表。

### 6.3 响应 Schemas

#### `PlaylistCapabilitiesResult`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `supportedItemTypes` | string[] | yes | `image`, `website`, `video`, `clock`, `unsplash`, `powerbi` | none | 支持的播放项类型。 |
| `maxPlaylists` | uint32 | no | product-defined | omitted | 最大播放列表数。 |
| `maxItemsPerPlaylist` | uint32 | no | product-defined | omitted | 每列表最大播放项数。 |
| `supportsScheduledPlaylist` | boolean | yes | `true`, `false` | none | 是否支持 scheduled 类型。 |
| `supportsUrlRefresh` | boolean | yes | `true`, `false` | none | 是否支持 URL 刷新。 |
| `supportsReset` | boolean | yes | `true`, `false` | none | 是否支持恢复默认。 |

#### `PlaylistConfigResult`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `playlists` | `Playlist[]` | yes | see `Playlist` | none | 当前播放列表配置数组。 |

#### `GetPlaylistItemUrlResult`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `type` | enum | yes | `image`, `video`, `website`, `unsplash` | none | 播放项类型。判别 `settings` 的 variant schema。 |
| `settings` | per-type settings | yes | 由 `type` 经 `variants` 判别 | none | 刷新后的完整设置。按 `type` 对应 §6.5 的 per-type settings schema。 |

#### `GetPlaylistItemResult`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `item` | `PlaylistItem` | yes | see `PlaylistItem` | none | 刷新后的完整播放项，与 `setPlaylistConfig` / `getPlaylistConfig` 的 `items` 元素同构。设备用此值整体替换本地对应播放项。 |

### 6.4 事件 Payload Schemas

#### `PlaylistConfigChangedEvent`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `reason` | string (enum) | yes | `set_config`, `reset_config` | none | 变更原因。 |
| `playlists` | `Playlist[]` | no | see `Playlist` | omitted | 变更后的完整播放列表。可省略。 |

### 6.5 共享对象 Schemas

#### `Playlist`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `id` | string (UUID) | yes | UUID format | none | 播放列表唯一标识。 |
| `type` | enum | yes | `default`, `scheduled` | none | 播放列表类型。`default` 为默认播放列表，`scheduled` 为定时播放列表。 |
| `startDate` | string (date) | conditional | YYYY-MM-DD | omitted | 开始日期。`startDate <= endDate`。 |
| `endDate` | string (date) | conditional | YYYY-MM-DD | omitted | 结束日期。`startDate <= endDate`。 |
| `startTime` | string (time) | conditional | HH:mm:ss | omitted | 开始时间。**当 `startDate == endDate` 时，`startTime <= endTime`。** |
| `endTime` | string (time) | conditional | HH:mm:ss | omitted | 结束时间。**当 `startDate == endDate` 时 `startTime <= endTime`；当 `startDate < endDate` 时允许跨午夜（`startTime > endTime`），语义为 D 日 startTime → D+1 日 endTime。** |
| `days` | uint8[] | conditional | 1-7, 1=周一 | omitted | 生效星期。 |
| `items` | `PlaylistItem[]` | yes | see `PlaylistItem` | none | 播放项数组。非空。 |

> **条件必填规则**：
> - `type: "default"` — `startDate`、`endDate`、`startTime`、`endTime`、`days` 均不可设置（省略或 `null`）。
> - `type: "scheduled"` — `startDate`、`endDate`、`startTime`、`endTime`、`days` 全部必填。附加约束：`startDate <= endDate`；当 `startDate == endDate` 时 `startTime <= endTime`；当 `startDate < endDate` 时允许跨午夜（`startTime > endTime`）；`days` 非空且每项在 1-7 范围内。

#### `PlaylistItem`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `id` | string (UUID) | yes | UUID format | none | 播放项唯一标识。 |
| `type` | enum | yes | `image`, `website`, `video`, `clock`, `unsplash`, `powerbi` | none | 播放项类型。该值经 `settings` 的 `variants` 判别选择对应 settings schema。 |
| `duration` | uint32 | yes | 0-86400（`0` 语义见说明） | 60 | 单次播放时长（秒）。`0` 语义未定义——推荐禁止 `0`（要求 `> 0`），或允许 `0` 表示「直到下一次列表循环或切换」。`[REVIEW-ASK]` 采纳前确认。 |
| `sort` | uint32 | yes | 非负整数 | 0 | 播放顺序，按 `sort` 升序排列。相同 `sort` 值时的相对顺序未定义——建议按 `PlaylistItem.id` 稳定排序。`[REVIEW-ASK]` 采纳前确认。 |
| `settings` | per-type settings | yes | 由 `type` 经 `variants` 判别 | none | 播放项设置。字段携带 `variants: { discriminator: type, mapping: { image: ImageItemSettings, website: WebsiteItemSettings, video: VideoItemSettings, clock: ClockItemSettings, unsplash: UnsplashItemSettings, powerbi: PowerBiItemSettings } }`（registry 名加 `Signage` 前缀）。 |

#### `ImageItemSettings`

> registry 名：`SignageImageItemSettings`。`image` 类型播放项的 settings schema（经 `PlaylistItem.settings` 的 `variants` 判别）。fieldId 从 0x01 连续重排（见 Amendment History A2）。

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `urls` | string[] | yes | non-empty URL array | none | 图片 URL 数组。 |
| `delaySeconds` | uint32 | yes | > 0 | 5 | 每个图片显示时长（秒）。 |
| `expiresAt` | uint64 | no | Unix timestamp | `null` | URL 过期时间。`null` 表示永不过期。 |

#### `VideoItemSettings`

> registry 名：`SignageVideoItemSettings`。`video` 类型播放项的 settings schema。fieldId 从 0x01 连续重排（见 Amendment History A2）。

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `url` | string | yes | valid URL | none | 视频 URL。 |
| `expiresAt` | uint64 | no | Unix timestamp | `null` | URL 过期时间。`null` 表示永不过期。 |
| `muted` | boolean | no | `true`, `false` | `false` | 是否静音播放。 |

#### `WebsiteItemSettings`

> registry 名：`SignageWebsiteItemSettings`。`website` 类型播放项的 settings schema。fieldId 从 0x01 连续重排（见 Amendment History A2）。

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `url` | string | yes | valid URL | none | 网站 URL。 |
| `ignoreCertificateError` | boolean | no | `true`, `false` | `false` | 忽略 TLS 证书错误。`[REVIEW-ASK]` 安全敏感字段——允许跳过证书校验有中间人风险，需明确何时允许、调用方权限要求与默认策略；采纳前由安全/架构确认。 |
| `refreshIntervalSecs` | uint32 | no | > 0 | `null` | 刷新间隔秒数。`null` 表示不刷新。 |

#### `ClockItemSettings`

> registry 名：`SignageClockItemSettings`。`clock` 类型播放项的 settings schema。fieldId 从 0x01 连续重排（见 Amendment History A2）。

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `clocks` | `ClockEntry[]` | yes | non-empty | none | 时钟列表。 |

#### `ClockEntry`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `timezone` | string (IANA) | yes | IANA timezone | none | 时区标识。 |
| `label` | string | yes | non-empty | none | 城市标签。 |

#### `UnsplashItemSettings`

> registry 名：`SignageUnsplashItemSettings`。`unsplash` 类型播放项的 settings schema。fieldId 从 0x01 连续重排（见 Amendment History A2）。

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `photos` | `UnsplashPhoto[]` | yes | non-empty | none | Unsplash 图片列表（含摄影师信息）。 |
| `delaySeconds` | uint32 | yes | > 0 | 5 | 播放间隔秒数。 |
| `expiresAt` | uint64 | no | Unix timestamp | `null` | URL 过期时间。`null` 表示永不过期。 |

#### `UnsplashPhoto`

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `url` | string | yes | valid URL | none | 图片 URL。 |
| `user` | `UnsplashUser` | yes | see `UnsplashUser` | none | 摄影师信息（嵌套对象，registry 名 `SignagePlaylistUnsplashUser`）。 |

#### `UnsplashUser`

> registry 名：`SignagePlaylistUnsplashUser`。Unsplash 照片的摄影师署名对象（v1.5 A3 由扁平 `userName`/`userLink` 对齐为嵌套，见 Amendment History A3）。

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `name` | string | yes | non-empty | none | 摄影师名称。 |
| `link` | string | yes | valid URL | none | 摄影师 Unsplash 主页链接。 |

#### `PowerBiItemSettings`

> registry 名：`SignagePowerBiItemSettings`。`powerbi` 类型播放项的 settings schema（v1.6 A4 新增，经 `PlaylistItem.settings` 的 `variants` 判别）。5 字段**全必填**——与其他类型不同，`powerbi` 无可省略字段，`expiresAt` 恒有值（AAD token 必然过期）。`token` 为敏感凭证：仅经 RPC 通道传输，不得进资源 URL、不得写日志。

| 字段名 | 类型 | 必填 | 取值范围 / 枚举 | 默认值 | 说明 |
|---|---|---:|---|---|---|
| `groupId` | string (GUID) | yes | GUID format | none | PowerBI workspace（group）ID。 |
| `dashboardId` | string (GUID) | yes | GUID format | none | PowerBI Dashboard ID。 |
| `embedUrl` | string | yes | valid URL | none | Dashboard embed URL。稳定不变（token 刷新不改变此值），设备可缓存复用。 |
| `token` | string | yes | AAD access token | none | AAD access token（PowerBI audience，tokenType=Aad 嵌入）。敏感凭证，仅经 RPC 通道传输，不得写日志。约 60 分钟有效。 |
| `expiresAt` | uint64 | yes | Unix timestamp | none | token 过期时间。**恒有值**（与其他类型 `expiresAt` 可空不同）。设备在临期时经 `getPlaylistItem` 刷新。 |

---

## 7. 交互流程示例 Flow Examples

本章只展示多个 method/event 组成的端到端业务流程。单个 method 的 request / success / error `d block` 示例见第 3 章；单个 event 的 `d block` 示例见第 4 章。

### 7.1 场景：能力发现 → 全量同步 → 配置变更事件

设备上线（RPC Session 已 `APP_READY`）后，云端先查询能力，再全量下发播放列表；设备成功落地后发出配置变更事件，调用方据此刷新本地缓存。

#### Step 1. 能力发现：`signage.getPlaylistCapabilities` 请求 (op=7)

```json
{
  "id": 201,
  "method": "signage.getPlaylistCapabilities",
  "params": {}
}
```

#### Step 2. 能力发现：成功响应 (op=8)

```json
{
  "id": 201,
  "status": { "ok": true, "code": 0 },
  "result": {
    "supportedItemTypes": ["image", "website", "video", "clock", "unsplash", "powerbi"],
    "supportsUrlRefresh": true,
    "supportsReset": true
  }
}
```

#### Step 3. 全量同步：`signage.setPlaylistConfig` 请求 (op=7)

```json
{
  "id": 202,
  "method": "signage.setPlaylistConfig",
  "params": {
    "playlists": [
      {
        "id": "550e8400-e29b-41d4-a716-446655440000",
        "type": "default",
        "items": [
          {
            "id": "6ba7b810-9dad-11d1-80b4-00c04fd430c8",
            "type": "image",
            "duration": 60,
            "sort": 0,
            "settings": {
              "urls": ["https://example.com/resource/file-1.jpg"],
              "delaySeconds": 5,
              "expiresAt": 1704067200
            }
          }
        ]
      }
    ]
  }
}
```

#### Step 4. 全量同步：成功响应 (op=8)

```json
{
  "id": 202,
  "status": { "ok": true, "code": 0 }
}
```

#### Step 5. 配置落地通知：`signage.playlistConfigChanged` 事件 (op=6)

```json
{
  "event": "signage.playlistConfigChanged",
  "intent": 1,
  "data": {
    "reason": "set_config"
  }
}
```

读法：`setPlaylistConfig` 成功仅返回 success status（无 result body），调用方通过本事件确认落地；若事件 payload 省略 `playlists`，调用方应调用 `signage.getPlaylistConfig` 校准。校验失败（如 §3.3.3 scheduled 约束 / 空列表 error 示例）不会走到 Step 4/5，直接返回错误且不触发事件。

### 7.2 场景：资源 URL 即将过期，设备主动 Pull 刷新

设备检测到某 image 播放项 URL 即将过期，主动调用 `signage.getPlaylistItemUrl` 拉取新 URL；若资源已不可用，设备改走全量校准。

#### Step 1. URL 刷新：`signage.getPlaylistItemUrl` 请求 (op=7)

```json
{
  "id": 203,
  "method": "signage.getPlaylistItemUrl",
  "params": {
    "itemId": "6ba7b810-9dad-11d1-80b4-00c04fd430c8"
  }
}
```

#### Step 2. URL 刷新：成功响应 (op=8)

```json
{
  "id": 203,
  "status": { "ok": true, "code": 0 },
  "result": {
    "type": "image",
    "settings": {
      "urls": ["https://example.com/resource/file-1-new.jpg"],
      "delaySeconds": 5,
      "expiresAt": 1704153600
    }
  }
}
```

#### Step 3（失败分支）. 资源已过期：错误响应 (op=8)

```json
{
  "id": 203,
  "status": {
    "ok": false,
    "code": 12,
    "msg": "Playlist item URL has expired and cannot be refreshed.",
    "details": {
      "candidateError": "SIGNAGE_PLAYLIST_URL_EXPIRED"
    }
  }
}
```

读法：成功时设备用返回的 `settings` 替换本地缓存继续播放；失败时设备停止对该 `itemId` 重试，改为调用 `signage.getPlaylistConfig`（或等待 `playlistConfigChanged`）获取云端最新配置。`code: 12` 借用 common `NOT_FOUND`（0x000C），候选业务码 `SIGNAGE_PLAYLIST_URL_EXPIRED` 采纳时二选一（见 §8）。

### 7.3 场景：powerbi embed token 临期，设备主动整项刷新

设备检测到某 `powerbi` 播放项的 `settings.expiresAt`（AAD token 约 60 分钟有效）临期，主动调用 `signage.getPlaylistItem` 获取含新 token 的完整播放项并整体替换本地缓存。`powerbi` 等非 URL 型资源 MUST 使用本方法（`getPlaylistItemUrl` 返回 `NOT_SUPPORTED`）。

#### Step 1. 整项刷新：`signage.getPlaylistItem` 请求 (op=7)

```json
{
  "id": 204,
  "method": "signage.getPlaylistItem",
  "params": {
    "itemId": "9ba7b810-9dad-11d1-80b4-00c04fd430c8"
  }
}
```

#### Step 2. 整项刷新：成功响应 (op=8)

```json
{
  "id": 204,
  "status": { "ok": true, "code": 0 },
  "result": {
    "item": {
      "id": "9ba7b810-9dad-11d1-80b4-00c04fd430c8",
      "type": "powerbi",
      "duration": 600,
      "sort": 0,
      "settings": {
        "groupId": "f089354e-3a18-4d43-8db5-3c58a1a0c9f2",
        "dashboardId": "69ff5868-12d6-4f1c-8c2f-7b6b6bc11d4b",
        "embedUrl": "https://app.powerbi.com/dashboardEmbed?dashboardId=69ff5868-12d6-4f1c-8c2f-7b6b6bc11d4b&groupId=f089354e-3a18-4d43-8db5-3c58a1a0c9f2",
        "token": "eyJhbGciOiJSUzI1NiIs...<new-aad-access-token>",
        "expiresAt": 1704157200
      }
    }
  }
}
```

读法：设备用返回的 `item` **整体替换**本地对应播放项（`id` / `type` / `duration` / `sort` / `settings` 全部以返回值为准，不做字段级合并）；`embedUrl` 稳定不变可缓存复用，实际变化的是 `token` 与 `expiresAt`。若 `itemId` 已不存在（如配置已被全量替换删除），设备收到 `NOT_FOUND` 后应停止重试，改经 `getPlaylistConfig` / `playlistConfigChanged` 校准（与 §7.2 失败分支同策略）。

---

## 8. 错误

| Error | Code | 类别 | 说明 | Review |
|---|---|---|---|---|
| `INVALID_ARGUMENT` | 0x000A | common | 参数校验失败：scheduled 时间约束违反、播放项 `settings` 字段非法、播放列表数组为空、播放项数组为空、`sort` 在同一 playlist 内重复。 | [REVIEW-ADOPTED] |
| `NOT_SUPPORTED` | 0x0003 | common | 操作不支持当前播放项类型（如 `clock` 或 `powerbi` 调用 `getPlaylistItemUrl`）或设备不支持 `signage.playlist` 能力。 | [REVIEW-ADOPTED] |
| `PERMISSION_DENIED` | 0x0009 | common | 调用方无权修改播放列表配置（`setPlaylistConfig`）。 | [REVIEW-ADOPTED] |
| `INTERNAL_ERROR` | 0x000E | common | 设备内部错误（`getPlaylistCapabilities` / `getPlaylistConfig` / `resetPlaylistConfig` 执行失败）；对 `getPlaylistItem` 亦承载对端生成刷新资源失败（如 PowerBI embed token 签发失败，设备 SHOULD 稍后重试）。 | [REVIEW-ADOPTED] |
| `NOT_FOUND` | 0x000C | common | 指定资源不存在：`getPlaylistItemUrl` / `getPlaylistItem` 的 `itemId` 不存在于当前播放列表、资源 URL 已过期且无法刷新。 | [REVIEW-ADOPTED] |

> 采纳决策：候选业务码 `SIGNAGE_PLAYLIST_EMPTY` / `SIGNAGE_PLAYLIST_ITEM_NOT_FOUND` / `SIGNAGE_PLAYLIST_URL_EXPIRED` **不新增为独立业务码，统一复用 common 错误码**（见上方「采纳记录」错误码决策表）。signage domain 错误码区段 `0x0D00-0x0DFF` 保持空；§3.3.3 / §3.5.3 / §3.6.3 JSON 示例中的 `candidateError` 占位以 common 码为准（空列表→`INVALID_ARGUMENT`，item 不存在/URL 过期→`NOT_FOUND`）。

> 通用错误码数值取自 `registry/error/error_code.yaml`。业务域错误 `SIGNAGE_PLAYLIST_*` 落点在业务域区段 `0x0600-0x15FF`，编号 `TBD after adoption`，由 registry 采纳时分配。

> **错误码双轨制说明**：上表 `SIGNAGE_PLAYLIST_ITEM_NOT_FOUND` / `SIGNAGE_PLAYLIST_EMPTY` / `SIGNAGE_PLAYLIST_URL_EXPIRED` 为**候选业务错误码**，数值 `TBD after adoption`。在候选码尚未分配数值前，本文 JSON 示例（§3.3.3 / §3.5.3 / §3.6.3）按 20-draft-business-protocol 约定借用**语义最近的 common 错误码**（`NOT_FOUND` 0x000C、`INVALID_ARGUMENT` 0x000A），并将候选名写入 `status.details.candidateError`。采纳阶段需对每个候选码做二选一决定：**新增为独立业务码**（落入 `0x0600-0x15FF`），还是**直接复用对应 common 码**（减少 registry 膨胀）。见 §12 待确认问题。

---

## 9. Legacy 待映射

### 9.0 Legacy 映射总览

| Legacy entry | 证据源 / 状态 | Direction | AXTP target | 说明 |
|---|---|---|---|---|
| `SetPlaylistConfig` | 通用管理命令 · 已研发 | Server → Device | `signage.setPlaylistConfig` | 逐字段映射见 9.1。 |
| `GetPlaylistConfig` | 通用管理命令 · 已研发 | Server ↔ Device | `signage.getPlaylistConfig` | 逐字段映射见 9.2。 |
| `GetPlaylistItemUrl` | 通用管理命令 · 已研发 | Device → Server | `signage.getPlaylistItemUrl` | 逐字段映射见 9.3。 |

> **证据源文件**：`workspace/legacy-migration/evidence/NearHub-Launcher数字标牌设备管理通用管理命令.md`。

> **AXTP-only 新增（无 legacy 对应）**：`signage.getPlaylistCapabilities`（能力查询）、`signage.resetPlaylistConfig`（恢复默认）、`signage.getPlaylistItem`（播放项整项刷新，v1.6 A4 新增）、`unsplash` / `powerbi` 播放项类型、`signage.playlistConfigChanged` 事件均为 AXTP 新增能力，legacy NearHub Launcher 无对应命令。迁移所有者据此判断完整 delta——legacy 侧仅有 `SetPlaylistConfig` / `GetPlaylistConfig` / `GetPlaylistItemUrl` 三个命令可映射，其余为纯新增。

### 9.1 `SetPlaylistConfig` → `signage.setPlaylistConfig`

Legacy Server → Device，请求 `{ playlists: [...] }`，响应 `{ ok: true }`。

**请求参数映射**：

| Legacy 字段 | 类型 | AXTP 字段 | 类型 | 转换说明 |
|---|---|---|---|---|
| `playlists` | Array | `playlists` | `Playlist[]` | 直传。AXTP 增加 UUID format、非空、scheduled 时间约束等校验规则。 |
| `playlists[].id` | String | `id` | string (UUID) | 直传。AXTP 增加 UUID format 约束。 |
| `playlists[].type` | String (`default`/`scheduled`) | `type` | enum | 直传。枚举值不变。 |
| `playlists[].startDate` | String (date) | `startDate` | string (date) | 直传。仅 `scheduled` 类型。 |
| `playlists[].endDate` | String (date) | `endDate` | string (date) | 直传。AXTP 新增分条件约束（见 v0.6 决策）。 |
| `playlists[].startTime` | String (time) | `startTime` | string (time) | 直传。 |
| `playlists[].endTime` | String (time) | `endTime` | string (time) | 直传。 |
| `playlists[].days` | Array (1-7) | `days` | uint8[] | 直传。1=周一，语义不变。 |
| `playlists[].items` | Array | `items` | `PlaylistItem[]` | 直传。AXTP 增加非空约束。 |
| `playlists[].items[].id` | String | `id` | string (UUID) | 直传。 |
| `playlists[].items[].type` | String (`image`/`slideshow`/`website`/`video`/`clock`) | `type` | enum | AXTP 新增 `unsplash`，废弃 `slideshow`。Legacy `slideshow` 由 adapter 映射为 `image`。AXTP 另新增 `powerbi`（v1.6 A4，AXTP-only，legacy 无此类型，Adapter 不需处理）。 |
| `playlists[].items[].duration` | Number | `duration` | uint32 | 直传。默认 60，范围 0-86400。 |
| `playlists[].items[].sort` | Number | `sort` | uint32 | 直传。默认 0。 |
| `playlists[].items[].settings` | Object | `settings` | per-type settings（`ImageItemSettings` 等 6 种，由 `type` 经 `variants` 判别；含 AXTP-only 的 `PowerBiItemSettings`，Adapter 不需处理） | 按 `type` 区分结构。字段 1:1 映射。 |
| *(none)* | — | *(AXTP 无额外字段)* | — | — |

**Legacy `slideshow` 类型废弃说明** `[REVIEW-RESOLVED]`：AXTP 不保留 `slideshow` 播放项类型。Legacy `slideshow` 由 Adapter 映射为 AXTP `image` 类型（settings schema 为 `ImageItemSettings`）。以下子表展示 `settings` 字段级转换：

| Legacy `slideshow` 字段 | 类型 | AXTP `image` 字段 | 类型 | 转换规则 |
|---|---|---|---|---|
| `settings.url` | String | `settings.urls` | string[] | `settings.url` 包装为单元素数组：`"<url>"` → `["<url>"]`。 |
| *(none)* | — | `settings.delaySeconds` | uint32 | AXTP 新增。Adapter 使用默认值 `5`。 |
| `settings.expiresAt` | Number / null | `settings.expiresAt` | uint64 / null | 直传。类型从 Number 隐式转为 uint64。`null` 表示永不过期。 |

Legacy `slideshow` 的 `type` 字段替换为 `"image"`。`duration`、`sort`、`id` 字段直传不变。

**响应结果映射**：

| Legacy 字段 | 类型 | AXTP 字段 | 类型 | 转换说明 |
|---|---|---|---|---|
| `ok` | boolean | *(无 result body)* | — | **结构变更**。Legacy `{ ok: true }` 映射到 AXTP 标准 success status（`status: { ok: true, code: 0 }`），无 result body。 |

> **Adapter 说明**：Adapter 将 Legacy `{ ok: true }` 映射为 AXTP 标准 success status。setPlaylistConfig 无 result body。请求参数结构 1:1 兼容，无需字段转换。

### 9.2 `GetPlaylistConfig` → `signage.getPlaylistConfig`

Legacy Server ↔ Device，请求无参数，响应 `{ playlists: [...] }`。

**请求参数映射**：

| Legacy 字段 | 类型 | AXTP 字段 | 类型 | 转换说明 |
|---|---|---|---|---|
| *(none)* | — | *(none)* | — | 双方均无请求参数。 |

**响应结果映射**：

| Legacy 字段 | 类型 | AXTP 字段 | 类型 | 转换说明 |
|---|---|---|---|---|
| `playlists` | Array | `playlists` | `Playlist[]` | 直传。结构一致，嵌套对象 1:1 映射。 |

> **Adapter 说明**：Legacy 响应为顶层 `playlists[]`。AXTP 响应包装在 `result` 对象内 `{ playlists: [...] }`。Adapter 需处理外层包装差异。字段内容 1:1 兼容，无需转换。

### 9.3 `GetPlaylistItemUrl` → `signage.getPlaylistItemUrl`

Legacy Device → Server，请求 `{ itemId }`，响应 `{ url / urls, expiresAt }`。

**请求参数映射**：

| Legacy 字段 | 类型 | AXTP 字段 | 类型 | 转换说明 |
|---|---|---|---|---|
| `itemId` | String (UUID) | `itemId` | string (UUID) | 1:1 直传。 |

**响应结果映射**：

| Legacy 字段 | 类型 | AXTP 字段 | 类型 | 转换说明 |
|---|---|---|---|---|
| `url` | String | `type` + `settings.url` | enum + string | **Breaking change**。Legacy 顶层 `url` → AXTP `type: "video"/"website"` + `settings.url`。Adapter 必须根据 item 类型推断 `type` 并将顶层 `url` 移入 `settings.url`。 |
| `urls` | String[] | `type` + `settings.urls` | enum + string[] | **Breaking change**。Legacy 顶层 `urls` → AXTP `type: "image"` + `settings.urls`。Adapter 必须将顶层 `urls` 移入 `settings.urls`。 |
| `expiresAt` | Number (Unix timestamp) | `settings.expiresAt` | uint64 | **Breaking change**。Legacy 顶层 `expiresAt` → AXTP `settings.expiresAt`（嵌套在对应类型 settings 内）。 |
| *(none)* | — | `type` | enum | AXTP 新增。Legacy 响应无 `type` 字段，Adapter 必须从当前播放列表配置中查找 `itemId` 对应的 `type` 并填充。 |
| *(none)* | — | `type: "unsplash"` + `settings.photos` / `settings.delaySeconds` / `settings.expiresAt` | enum + `UnsplashPhoto[]` + uint32 + uint64 | **AXTP-only**。Legacy `GetPlaylistItemUrl` 无 `unsplash` 类型响应。AXTP 新增此类型，Adapter 不需处理。 |

> **Adapter 说明**：这是三个 legacy 命令中**结构变化最大**的。v0.4 将响应从顶层互斥字段（`url` / `urls` / `expiresAt`）重构为 `type` + `settings` 显式类型判别模式（v1.5 起判别由 `variants` 机制承载，见 Amendment History A2）。Adapter 需要：(1) 从设备当前播放列表配置中查找 `itemId` 对应的播放项 `type`；(2) 将旧顶层字段按类型移入 `settings` 子对象：
> - **image**（`ImageItemSettings`）：顶层 `urls[]` + `expiresAt` → `settings.urls` + `settings.expiresAt`；`delaySeconds` 取设备当前配置原值（legacy 刷新响应不含此字段）。
> - **video**（`VideoItemSettings`）/ **website**（`WebsiteItemSettings`）：顶层 `url` + `expiresAt` → `settings.url` + `settings.expiresAt`（website 保留 `ignoreCertificateError` / `refreshIntervalSecs`，video 保留 `muted`）。
> - **slideshow**：legacy `GetPlaylistItemUrl` 响应 `url`（String）按 `image` 策略包装为 `ImageItemSettings.urls`（单元素数组 `["<url>"]`），补充 `delaySeconds`（默认 `5`）和 `expiresAt`（从响应顶层 `expiresAt` 移入）。Adapter 还需将推断的 `type` 从 `"slideshow"` 替换为 `"image"`。
> - **unsplash**（`UnsplashItemSettings`）：Legacy 无此类型。AXTP 直接返回 `type: "unsplash"` + `settings`。Adapter 不需处理。
> - **clock**：不涉及刷新，设备不应调用 `getPlaylistItemUrl`。
> - **powerbi**（`PowerBiItemSettings`）：Legacy 无此类型（AXTP-only，v1.6 A4）。AXTP 对 powerbi 调用 `getPlaylistItemUrl` 返回 `NOT_SUPPORTED`——Adapter 不得把该错误映射为带空 `url` 的 legacy 成功响应（会静默弄坏设备侧 token 刷新）；powerbi 的 embed token 刷新走 `signage.getPlaylistItem`（无 legacy 对应，Adapter 不需处理）。

### 9.4 Adapter 通用转换模式

以下转换模式在多个 legacy 命令中共享：

| 模式 | 说明 | 涉及命令 |
|---|---|---|
| **`{ ok: true }` 响应** | Legacy Set 指令统一返回 `{ "ok": true }`。AXTP 使用标准 success status（`status: { ok: true, code: 0 }`）加 typed result 对象。Adapter 将 `ok: true` 映射为 AXTP success status。 | `SetPlaylistConfig` |
| **顶层 url/urls 互斥 → type+settings** | Legacy `GetPlaylistItemUrl` 响应使用顶层互斥字段（`url` 或 `urls`）。AXTP 使用 `type` + `settings` 显式类型判别模式。Adapter 需推断 `type` 并重组字段结构。 | `GetPlaylistItemUrl` |
| **Legacy `slideshow` → AXTP `image`** `[REVIEW-RESOLVED]` | AXTP 废弃 `slideshow` 类型。Adapter 将 `slideshow` 的 `settings.url`（单 URL）包装为 `image` 的 `settings.urls`（单元素数组 `["<url>"]`）。 | `SetPlaylistConfig`, `GetPlaylistConfig`, `GetPlaylistItemUrl` |

> **`slideshow` → `image` 字段转换示例**：
>
> | Legacy `slideshow` | AXTP `image` | 说明 |
> |---|---|---|
> | `type: "slideshow"` | `type: "image"` | 类型替换。 |
> | `settings.url: "https://..."` | `settings.urls: ["https://..."]` | String → 单元素 string[]。 |
> | *(absent)* | `settings.delaySeconds: 5` | AXTP 新增必填字段，默认值 5。 |
> | `settings.expiresAt: 1704153600` | `settings.expiresAt: 1704153600` | 直传。`null` → `null`。 |
> | `id`, `duration`, `sort` | `id`, `duration`, `sort` | 直传不变。 |

### 9.5 Generated Legacy 文件交叉引用

以下 generated 文件当前包含 signage 播放列表相关映射，采纳阶段需同步更新。

**`workspace/legacy-migration/generated/legacy-to-axtp-map.generated.yaml`** 当前映射：

| Legacy entry | 当前 target method | Status |
|---|---|---|
| `SetPlaylistConfig` | `signage.setPlaylistConfig` | draft |
| `GetPlaylistConfig` | `signage.getPlaylistConfig` | draft |
| `GetPlaylistItemUrl` | `signage.getPlaylistItemUrl` | draft |

**`workspace/legacy-migration/generated/registry-patches.generated.yaml`** 当前条目：

| 当前 ID | 当前名称 | 当前 domain | 当前 schema 名称 |
|---|---|---|---|
| Method `0x1D01` | `signage.getPlaylistConfig` | `signage` | `SignageGetPlaylistConfigRequest` / `Response` |
| Method `0x1D02` | `signage.getPlaylistItemUrl` | `signage` | `SignageGetPlaylistItemUrlRequest` / `Response` |
| Method `0x1D03` | `signage.setPlaylistConfig` | `signage` | `SignageSetPlaylistConfigRequest` / `Response` |
| Capability `0x1D01` | `signage.playlistConfig` | `signage` | — |
| Capability `0x1D02` | `signage.playlistItemUrl` | `signage` | — |

采纳阶段更新要点：

- Schema 名称应从 generated 占位名（`SignageGetPlaylistConfigRequest` 等）更新为与本文草案 schema 名一致（`GetPlaylistConfigParams`、`PlaylistConfigResult` 等）。
- Capability 应从拆分的 `signage.playlistConfig` / `signage.playlistItemUrl` 合并为统一的 `signage.playlist`。
- 新增方法 `signage.getPlaylistCapabilities`（ID `0x1D04` 或采纳时分配）、`signage.resetPlaylistConfig`（`0x1D05`）。
- 新增事件 `signage.playlistConfigChanged`。
- Adapter 名称（`GetplaylistconfigAdapter` 等）应同步更新。

> **注意**：以上 generated 文件不在本草案阶段修改。更新发生在 `adopt-protocol-draft`（Stage 30）阶段，当 registry YAML 写入并 Generator 重跑后自动生效。本 section 9 的映射信息作为采纳阶段的输入参考。

---

## 10. Registry / Conformance Status

| 项 | 状态 |
|---|---|
| Registry YAML | **adopted** — `registry/domains/signage/domain.yaml` 已写入（capability `0x0D01` / methods `0x0D01-0x0D06` / event `0x0D01`） |
| Generated docs | **已生成** — `contract/protocol/axtp.protocol.yaml` 与 `contract/generated/*` / `contract/mcp/*` 已由 `50-generate-axtp-protocol` 重跑并随本修订入库（真实产物路径为 `contract/generated/*`，草案早期表述中的 `docs/generated/*` 为旧路径）。 |
| Method / event IDs | 已分配：见上方「采纳记录」 |
| Conformance | 采纳后需覆盖：全量同步（含 powerbi 项）、查询、恢复默认、URL 刷新（4 种类型）、播放项整项刷新（powerbi token 刷新）、clock / powerbi 类型调 `getPlaylistItemUrl` 返回 NOT_SUPPORTED、scheduled 时间约束、空列表 / `sort` 重复 / item 不存在错误、事件触发。 |

---

## 11. 测试要点

| 类型 | 要点 |
|---|---|
| happy path | `getCapabilities` 返回支持类型和限制；`getConfig` 返回当前配置；`setConfig` 全量替换；`resetConfig` 恢复默认；`getItemUrl` 按 4 种类型返回刷新 URL；`getItem` 返回完整播放项。 |
| hard-replace | `setConfig` 第二次调用不出现的旧 item 被删除；设备不应保留旧 item。 |
| scheduled constraint | `startDate == endDate` 时 `startTime <= endTime`（单日约束违反返回 `INVALID_ARGUMENT`）；`startDate < endDate` 时允许跨午夜。 |
| URL refresh | `image`/`video`/`website`/`unsplash` 类型返回对应 `type`+`settings`；`clock` / `powerbi` 类型返回 `NOT_SUPPORTED`；不存在 item 返回 `SIGNAGE_PLAYLIST_ITEM_NOT_FOUND`。 |
| powerbi refresh | `setConfig` 含 powerbi 项（5 字段全必填）；token 临期时 `getItem` 返回新 `token` + `expiresAt` 的完整播放项，设备整项替换（`embedUrl` 不变）；`getItemUrl` 对 powerbi 返回 `NOT_SUPPORTED`。 |
| event | `setConfig`/`resetConfig` 成功后触发 `playlistConfigChanged`；失败请求不触发事件。 |
| error path | 空播放列表（`SIGNAGE_PLAYLIST_EMPTY`）、无效参数（`INVALID_ARGUMENT`）、不支持的操作（`NOT_SUPPORTED`）、资源 URL 已过期（`SIGNAGE_PLAYLIST_URL_EXPIRED`）。 |

---

## 12. 待确认问题

### 待解决问题

| 问题 | 影响 | 当前建议 | 状态 |
|---|---|---|---|
| `PlaylistCapabilitiesResult` 应包含哪些能力字段？ | schema / capability | 建议包含 `supportedItemTypes`、`maxPlaylists`、`maxItemsPerPlaylist`、`supportsScheduledPlaylist`、`supportsUrlRefresh`、`supportsReset`。 | open |
| `ResetPlaylistConfigParams` 是否支持 scoped reset（如只重置 scheduled 播放列表）？ | schema / method | 当前建议无参数，全量恢复默认。 | open |
| `PlaylistConfigChangedEvent` 是否应总是携带完整 `playlists`？还是仅通知变更？ | event payload | 建议可选携带 playlists，客户端可调用 `getConfig` 获取完整配置。 | open |
| `default` 与 `scheduled` 播放列表共存的调度优先级？ | behavior | 建议 scheduled 命中时间窗口时优先，无命中回落 default。 | open |
| 多个 `scheduled` 时间区间重叠的冲突处理？ | behavior | 建议行为未定义，设备按 id/顺序稳定择一，不视为错误。 | open |
| `PlaylistItem.duration = 0` 的语义？ | schema | 建议禁止 0（要求 `> 0`），或允许 0 表示「直到循环/切换」。 | open |
| `PlaylistItem.sort` 相同值时的排序规则？ | schema | 建议相同 sort 时按 id 稳定排序。 | open |
| `website.ignoreCertificateError` 的安全策略与权限要求？ | security | 允许跳过 TLS 校验有 MITM 风险，需明确默认策略与调用方权限。 | open |
| 候选业务码 `SIGNAGE_PLAYLIST_*` 是否复用 common 码（`NOT_FOUND`/`INVALID_ARGUMENT`）以减少 registry 膨胀？ | error registry | 草案阶段两套并存（候选业务码 TBD + JSON 借 common 码）；采纳时二选一。 | open |

### 已解决问题

| 问题 | 决策 | 版本 |
|---|---|---|
| `GetPlaylistItemUrlResult` 多态响应设计 | v0.4 已重构为 `type` + `settings` 显式类型判别模式。 | v0.4 |
| 播放列表全量替换语义 | 硬替换，不引入版本号或 soft-delete。第二次全量下发删除未出现的旧 item。 | v0.5 |
| `scheduled` 类型时间区间约束 | `startDate == endDate` 时 `startTime <= endTime`；`startDate < endDate` 时允许跨午夜。违反返回 `INVALID_ARGUMENT`（0x000A）。 | v0.6 |
| URL 过期刷新方式 | 设备端主动 Pull 模式。设备调用 `signage.getPlaylistItemUrl` 获取新 URL。服务端不推送。 | v0.5 |
| `clock` 类型播放项调用 `getPlaylistItemUrl` | 返回 `NOT_SUPPORTED`（0x0003）。`clock` 不依赖远程 URL 资源。 | v0.5 |
| powerbi 等非 URL 型资源的刷新通道 | `getPlaylistItem`（`0x0D06`）整项刷新；`getPlaylistItemUrl` 不支持 powerbi（返回 `NOT_SUPPORTED`）。 | v1.6 (A4) |

---

## 附录 A. 协议审核标记

| 标记 | 条目 | 审核结论 | 后续动作 |
|---|---|---|---|
| `[REVIEW-ADOPTED]` | `signage.playlist` capability | 本文已采纳（见「采纳记录」），按 Naming and Taxonomy spec 完成单 feature 治理。事实源为 `registry/domains/signage/domain.yaml`。 | generated 产物由 `50-generate-axtp-protocol` 重跑生效；后续语义变更走 `40-amend-adopted-protocol`。 |
| `[REVIEW-RESOLVED]` | Legacy `slideshow` 类型废弃 | AXTP 不保留 `slideshow`，Adapter 映射为 `image`（`url` → `urls` 单元素数组）。 | — |
| `[REVIEW-RESOLVED]` | `signage.media` 合并 | 播放项 URL 刷新功能已从 `signage.media` 合并到本草案。`signage.media` 草案可删除。 | — |

## 附录 B. 协议决策

| 决策点 | 结论 | 理由 |
|---|---|---|
| 新增/修改/复用 | Modify | 扩展现有 v0.1 草案，新增 URL 刷新方法和详细 schema。 |
| 控制面 | RPC method/event | 业务控制不进入 Frame Header。 |
| 数据面 | None | 播放列表是配置同步操作，不涉及连续数据传输。 |
| WebSocket | RPC-only | WebSocket Unframed JSON 不承载 STREAM。 |
| 文档结构重组 | v0.7 按 20-draft-business-protocol skill 规范重组 | 对齐 `device.enrollment` / `device.info` 等成熟草案的结构规范。 |

## 附录 C. Registry 草案输入

> **采纳状态**：本附录原为采纳前的草案输入占位。本文已于 v1.4 完成采纳，正式 domain YAML 已写入 `registry/domains/signage/domain.yaml`（含完整 method/event/error/capability/type 条目）。以下保留草案输入仅作审计对照；以 registry YAML 为 machine 事实源，schema 名映射见上方「采纳记录」。原草案输入中的候选业务码 `SIGNAGE_PLAYLIST_*` 与 `TBD after adoption` 已被「复用 common」决策与已分配 ID 取代。

草案输入（采纳前占位，已过时）：

```yaml
capabilities:
  - name: signage.playlist
    status: draft

methods:
  - name: signage.getPlaylistCapabilities
    id: TBD after adoption
    bitOffset: TBD after adoption
    requestSchema: PlaylistCapabilitiesParams
    responseSchema: PlaylistCapabilitiesResult
    capabilities:
      - signage.playlist
  # ... 其余 method 见 registry/domains/signage/domain.yaml
```

## 附录 D. 采纳检查清单

采纳日期：2026-06-17（Stage 30 adopt-protocol-draft）。

> **当前状态注记（v1.6 A4 后）**：以下清单中「method `0x0D01-0x0D05`，bitOffset 0–4」为 v1.4 采纳时点记录；v1.6 A4 新增 `signage.getPlaylistItem`（`0x0D06` / bitOffset 5）后方法集为 `0x0D01-0x0D06`（bitOffset 0–5），见「采纳记录」与 Amendment History。generator 产物已随本修订重跑入库。

- [x] 已确认 domain.feature 粒度和 method/event 命名。
- [x] 已确认 Domain/ID 规划和生成链路。（signage DomainId `0x0D`；capability/method/event ID 已分配）
- [x] 已确认 methodId、bitOffset、request/response schema。（method `0x0D01-0x0D05`，bitOffset 0–4）
- [x] 已确认 eventId、eventMasks bitOffset、event schema。（event `0x0D01`，bitOffset 0）
- [x] 已确认 errorCode 范围和错误归属。（复用 common：INVALID_ARGUMENT / NOT_SUPPORTED / PERMISSION_DENIED / INTERNAL_ERROR / NOT_FOUND；signage 区段保持空）
- [x] 已确认 schema fieldId、capabilityId、supportedMethods。
- [x] 已确认 `PlaylistCapabilitiesResult`、`PlaylistConfigResult`、`ResetPlaylistConfigParams`、`PlaylistConfigChangedEvent` 的 schema 字段。
- [x] 已确认 Legacy `slideshow` 类型已废弃，Adapter 映射为 `image`。
- [x] YAML 写入后 Generator 能完整生成 `protocol/axtp.protocol.yaml` 和 `docs/generated/*`。（`50-generate-axtp-protocol` 已重跑，产物位于 `contract/protocol/` 与 `contract/generated/*`、`contract/mcp/*`）
