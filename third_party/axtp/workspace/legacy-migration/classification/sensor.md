# sensor Legacy Classification

本文件从 `legacy-protocol-classification.csv` 按 domain 切分生成，用于后续撰写 `contract/registry/domains/sensor/domain.yaml` 前的人工审查。

| Source | File | Line | Type | Legacy Wire | Command ID | Class | Method | Event | Config Name | Capability | AXTP Method | AXTP Event | Protocol Doc | Confidence | Notes |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| axdp_hid | workspace/legacy-migration/evidence/AXDP源码协议扫描清单.md | 62 | command | CommonGetGyroSlopeAngle | 0xC002A / 0x002A -> 0x00AA |  | CommonGetGyroSlopeAngle |  |  | sensor.motion | sensor.getMotionState |  |  | medium | 陀螺仪/倾角属于传感器状态，候选为 sensor.motion。 |
| signage_sdk | workspace/legacy-migration/evidence/NearHub-Launcher数字标牌设备管理通用管理命令.md | 353 | event | OnTelemetryReport |  |  |  | OnTelemetryReport |  | sensor.telemetry |  | sensor.telemetryReported |  | low | 遥测上报可候选为 sensor.telemetry，需确认是否拆为 device.power/sensor.。 Open: 确认遥测字段集合后再定 feature。 |
