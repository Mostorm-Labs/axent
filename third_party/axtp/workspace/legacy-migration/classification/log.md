# log Legacy Classification

本文件从 `legacy-protocol-classification.csv` 按 domain 切分生成，用于后续撰写 `contract/registry/domains/log/domain.yaml` 前的人工审查。

| Source | File | Line | Type | Legacy Wire | Command ID | Class | Method | Event | Config Name | Capability | AXTP Method | AXTP Event | Protocol Doc | Confidence | Notes |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| axdp_hid | workspace/legacy-migration/evidence/AXDP源码协议扫描清单.md | 185 | command | CommonGetLogData | 0xC013E / 0x013E -> 0x01BE |  | CommonGetLogData |  |  | log.export | log.getExportState |  | workspace/protocol/log/log.export.md | high | 日志打包、上传、导出结果归 log.export。 |
| signage_sdk | workspace/legacy-migration/evidence/NearHub-Launcher数字标牌设备管理通用管理命令.md | 756 | method | RequestLogUpload |  |  | RequestLogUpload |  |  | log.export | log.createExport |  | workspace/protocol/log/log.export.md | high | 日志打包、上传、导出结果归 log.export。 |
| signage_sdk | workspace/legacy-migration/evidence/NearHub-Launcher数字标牌设备管理通用管理命令.md | 767 | method | NotifyLogUploadResult |  |  | NotifyLogUploadResult |  |  | log.export |  | log.exportStateChanged | workspace/protocol/log/log.export.md | high | 日志打包、上传、导出结果归 log.export。 |
