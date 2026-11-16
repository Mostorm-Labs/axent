# auth Legacy Classification

本文件从 `legacy-protocol-classification.csv` 按 domain 切分生成，用于后续撰写 `contract/registry/domains/auth/domain.yaml` 前的人工审查。

| Source | File | Line | Type | Legacy Wire | Command ID | Class | Method | Event | Config Name | Capability | AXTP Method | AXTP Event | Protocol Doc | Confidence | Notes |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| signage_sdk | workspace/legacy-migration/evidence/NearHub-Launcher设备管理命令.md | 237 | method | GetBindCode |  |  | GetBindCode |  |  | auth.session | auth.getSessionState |  | workspace/protocol/auth/auth.session.md | medium | 设备绑定/绑定码/绑定状态归 auth.session 候选。 |
| signage_sdk | workspace/legacy-migration/evidence/NearHub-Launcher设备管理命令.md | 248 | event | OnBindState |  |  |  | OnBindState |  | auth.session |  | auth.sessionStateChanged | workspace/protocol/auth/auth.session.md | medium | 设备绑定/绑定码/绑定状态归 auth.session 候选。 |
| signage_sdk | workspace/legacy-migration/evidence/NearHub-Launcher数字标牌设备管理通用管理命令.md | 310 | method | GetBindConfig |  |  | GetBindConfig |  |  | auth.session | auth.getSessionState |  | workspace/protocol/auth/auth.session.md | medium | 设备绑定/绑定码/绑定状态归 auth.session 候选。 |
| signage_sdk | workspace/legacy-migration/evidence/NearHub-Launcher数字标牌设备管理通用管理命令.md | 323 | method | SetBindConfig |  |  | SetBindConfig |  |  | auth.session | auth.createSession |  | workspace/protocol/auth/auth.session.md | medium | 设备绑定/绑定码/绑定状态归 auth.session 候选。 |
| vm33_http_json | workspace/legacy-migration/evidence/VM33协议文档.md | 540 | config_name | Config.Subscribe:DevPasswd |  | Config | Subscribe |  | DevPasswd | auth.token |  | auth.tokenStateChanged | workspace/protocol/auth/auth.token.md | medium | 密码/token 属于 auth.token。 |
| vm33_http_json | workspace/legacy-migration/evidence/VM33协议文档.md | 582 | config_name | Config.UnSubscribe:DevPasswd |  | Config | UnSubscribe |  | DevPasswd | auth.token |  | auth.tokenStateChanged | workspace/protocol/auth/auth.token.md | medium | 密码/token 属于 auth.token。 |
| vm33_http_json | workspace/legacy-migration/evidence/VM33协议文档.md | 1277 | method | System.CheckPasswd |  | System | CheckPasswd |  |  | auth.token | auth.refreshToken |  | workspace/protocol/auth/auth.token.md | medium | 密码/token 属于 auth.token。 |
