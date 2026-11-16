# AXTP Legacy Migration Dashboard

本文按旧协议来源追踪迁移状态。它是迁移管理视图，不是 AXTP runtime 实现合同。

稳定协议事实仍以 `contract/registry/**`、`contract/protocol/axtp.protocol.yaml`、`contract/generated/**`、`specs/**` 和 `conformance/**` 为准。

## 状态字段

| 字段 | 含义 |
|---|---|
| Source | 旧协议来源或协议族。 |
| Evidence | 原始证据或扫描材料。 |
| Classified | 是否已有 domain / feature 分类材料。 |
| Mapped to generated | 是否已映射到 generated 协议事实。未统计时写 `TBD`。 |
| Mapped to draft | 是否已映射到 `workspace/protocol/**` 草案。未统计时写 `TBD`。 |
| Blocked | 当前阻塞项。 |
| Adapter needed | 是否需要 legacy adapter 或兼容层。 |
| Next step | 下一步动作。 |

不要把 `Mapped to draft` 理解为 runtime 可实现。草案必须经过 review、registry 采纳和 generation 后才会成为合同。

## AXDP

| Source | Evidence | Classified | Mapped to generated | Mapped to draft | Blocked | Adapter needed | Next step |
|---|---|---|---|---|---|---|---|
| AXDP HID / source scan | [AXDP源码协议扫描清单](evidence/AXDP源码协议扫描清单.md)、[AXDP设备能力协议集.xlsx](evidence/AXDP设备能力协议集.xlsx) | [by-source/axdp_hid.md](classification/by-source/axdp_hid.md)、[classification CSV](classification/legacy-protocol-classification.csv) | TBD；已知 `audio.algorithm` 已 generated，但未在 dashboard 中做全量统计 | TBD；多项已指向 audio/device/system/video/camera/diagnostic 等草案 | 部分命令语义、默认值、授权、beam/DOA、产测行为需人工确认 | 是，尤其是 HID command 到 AXTP method/event 的兼容层 | 优先校准 P1 `device.*` / `system.*`，再处理 video/camera/diagnostic 批次。 |

## Rooms

| Source | Evidence | Classified | Mapped to generated | Mapped to draft | Blocked | Adapter needed | Next step |
|---|---|---|---|---|---|---|---|
| Rooms WebSocket JSON | [Rooms协议文档.md](evidence/Rooms协议文档.md)、[Rooms协议文档.pdf](evidence/Rooms协议文档.pdf) | [by-source/rooms_ws_json.md](classification/by-source/rooms_ws_json.md)、[classification CSV](classification/legacy-protocol-classification.csv) | TBD | TBD；room、output、video、signage、system 等草案已有候选 | 房间、输出布局、投屏 share、日程和设备管理语义需要按新 domain 复核 | 是，Rooms WS JSON 到 AXTP RPC/事件需要 adapter | 结合 [rooms migration plan](plans/rooms-protocol-migration-plan.md) 逐项确认 `room.*` 与 `output/video` 边界。 |

## VM33

| Source | Evidence | Classified | Mapped to generated | Mapped to draft | Blocked | Adapter needed | Next step |
|---|---|---|---|---|---|---|---|
| VM33 HTTP JSON | [VM33协议文档.md](evidence/VM33协议文档.md)、[VM33协议文档.pdf](evidence/VM33协议文档.pdf) | [by-source/vm33_http_json.md](classification/by-source/vm33_http_json.md)、[classification CSV](classification/legacy-protocol-classification.csv) | TBD | TBD；system/device/video/camera/network/signage 等已有候选草案 | 旧 HTTP namespace 与新 domain 不一一对应，部分 payload 字段需要设备侧确认 | 是，HTTP JSON 到 AXTP RPC 或 hybrid gateway 需要 adapter | 结合 [vm33 migration plan](plans/vm33-protocol-migration-plan.md) 先确认设备信息、系统生命周期、投屏/显示和 camera 配置。 |

## Signage

| Source | Evidence | Classified | Mapped to generated | Mapped to draft | Blocked | Adapter needed | Next step |
|---|---|---|---|---|---|---|---|
| NearHub Launcher Signage SDK / device management | [NearHub-Launcher数字标牌设备管理通用管理命令](evidence/NearHub-Launcher数字标牌设备管理通用管理命令.md)、[NearHub-Launcher设备管理命令](evidence/NearHub-Launcher设备管理命令.md) | [by-source/signage_sdk.md](classification/by-source/signage_sdk.md)、[classification CSV](classification/legacy-protocol-classification.csv) | TBD | 已映射；15 个已研发指令全部有草案覆盖（device.enrollment v0.3, signage.playlist v0.7, software.config v0.2, software.updatePolicy v0.2, system.lifecycle v0.8, device.info, network.*） | 无阻塞项。Schedule 已定域 system.lifecycle，外观已定域 software.config，注册已定域 device.enrollment，URL 刷新已合并 signage.playlist。 | 是，Launcher / Signage 命令到 AXTP RPC 需要 adapter | 转 Stage 20/30 完成草案评审和采纳；根据 flow gap 细化未研发指令的优先级。 |

## uxplay / cast

| Source | Evidence | Classified | Mapped to generated | Mapped to draft | Blocked | Adapter needed | Next step |
|---|---|---|---|---|---|---|---|
| uxplay / cast receiver WebSocket and product flows | [WEBSOCKET_PROTOCOL.md](evidence/WEBSOCKET_PROTOCOL.md)、[cast receiver business](../business/cast-receiver-uxplay.md)、[cast pairing business](../business/cast-rxtx-pairing.md)、[device streaming business](../business/device-streaming.md) | Not yet split into `classification/by-source/**`; related rows may exist in [classification CSV](classification/legacy-protocol-classification.csv) | TBD | TBD；video/audio stream、room/source、signage/cast-related 草案需复核 | 当前 cast / uxplay backend 控制和 AXTP audio/video stream 边界需要统一 | 是，uxplay backend control 到 AXTP domain method 需要 adapter | 先补 by-source 分类或 flow，再决定是否进入 `video.stream`、`audio.stream`、`room.source` 或未来 cast domain 草案。 |

## 迁移看板维护规则

- 不伪造统计数字；没有脚本统计前使用 `TBD`。
- 新增 evidence 后先补 `Evidence` 和 `Classified`，不要直接写 generated。
- 迁移候选进入草案时更新 `Mapped to draft`。
- 草案采纳并 generated 后，才更新 `Mapped to generated`。
- adapter 只能消费 generated 合同；草案期 adapter 应标记为 prototype 或 compatibility experiment。
