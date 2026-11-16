# privacy Legacy Classification

本文件从 `legacy-protocol-classification.csv` 按 domain 切分生成，用于后续撰写 `contract/registry/domains/privacy/domain.yaml` 前的人工审查。

| Source | File | Line | Type | Legacy Wire | Command ID | Class | Method | Event | Config Name | Capability | AXTP Method | AXTP Event | Protocol Doc | Confidence | Notes |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| axdp_hid | workspace/legacy-migration/evidence/AXDP源码协议扫描清单.md | 132 | command | CommonSetPrivacyEnable | 0xC0073 / 0x0073 -> 0x00F3 |  | CommonSetPrivacyEnable |  |  | privacy.state | privacy.setModeConfig |  | workspace/protocol/privacy/privacy.state.md | high | 隐私开关/状态归 privacy.state。 |
| axdp_hid | workspace/legacy-migration/evidence/AXDP源码协议扫描清单.md | 133 | command | CommonGetPrivacyEnable | 0xC0074 / 0x0074 -> 0x00F4 |  | CommonGetPrivacyEnable |  |  | privacy.state | privacy.getState |  | workspace/protocol/privacy/privacy.state.md | high | 隐私开关/状态归 privacy.state。 |
