# Legacy Protocol Domain-Feature Classification

本目录是 AXDP / VM33 / Rooms / Signage legacy intake 的逐条分类结果，不是 AXTP registry 事实源。分类依据为 `specs/30-registry.md`、`specs/50-tooling.md`，并对照 `workspace/protocol` 下已成型的业务协议文档。

生成脚本：`tooling/legacy_classification/classify_legacy_protocols.py`

CSV 明细：`workspace/legacy-migration/classification/legacy-protocol-classification.csv`

## 字段说明

- `legacy_command_name` / `legacy_class` / `legacy_method` / `legacy_event` / `legacy_config_name` 分开展示旧协议原始字段。
- `target_domain` / `target_feature` / `target_capability` 表示按 Naming and Taxonomy spec 归类后的能力块。
- `target_axtp_method` / `target_axtp_event` 表示建议落到的 AXTP method/event；没有正式协议文档的条目会标明 taxonomy/spec candidate。
- `target_protocol_doc` 优先指向 `workspace/protocol` 中已有设计文档；为空表示该 domain.feature 还需要补业务协议文档或 domain YAML。

## 分类原则

- 旧协议先归类到 `domain.feature`，再映射 method/event/schema adapter。
- `Config / State / Mode / Scan / Connection` 默认不是 feature；它们进入 method、event 或 schema 字段。
- `stream` 只承担公共流控和数据面；文件、固件、视频、音频、日志流归各自业务域。
- 泛 `ConfigJson` 或缺少具体 payload/Name 的条目标为 `adapter_only` 或 `needs_split`，不能直接进入正式 capability。
- 本清单不修改 `workspace/legacy-migration/plans/`、`contract/registry/`、`contract/protocol/axtp.protocol.yaml` 或 generated artifacts。

## 按 Source 查看

| Source | Markdown |
| --- | --- |
| axdp_hid | by-source/axdp_hid.md |
| rooms_ws_json | by-source/rooms_ws_json.md |
| signage_sdk | by-source/signage_sdk.md |
| vm33_http_json | by-source/vm33_http_json.md |

## 覆盖统计

| Source | Entries |
| --- | --- |
| axdp_hid | 204 |
| rooms_ws_json | 98 |
| signage_sdk | 31 |
| vm33_http_json | 156 |

## 分类置信度

| Confidence | Entries |
| --- | --- |
| high | 296 |
| low | 3 |
| medium | 190 |

## Target RPC Kind

| Target RPC Kind | Entries |
| --- | --- |
| adapter_only | 22 |
| event | 13 |
| event_candidate | 1 |
| event_subscription | 5 |
| method | 402 |
| stream_owner_method | 46 |

## Domain.Feature 汇总

| Capability | Entries |
| --- | --- |
| audio.algorithm | 36 |
| video.framing | 22 |
| vendor.genericConfig | 22 |
| firmware.ota | 20 |
| network.wifi | 20 |
| output.layout | 20 |
| audio.volume | 16 |
| system.lifecycle | 15 |
| network.ip | 14 |
| video.stream | 14 |
| input.hid | 12 |
| file.transfer | 12 |
| audio.input | 11 |
| room.source | 11 |
| camera.focus | 10 |
| video.osd | 9 |
| system.initialization | 8 |
| diagnostic.networkTest | 8 |
| video.layout | 8 |
| audio.eq | 8 |
| camera.zoom | 8 |
| device.childDevice | 8 |
| device.info | 7 |
| device.state | 7 |
| video.recording | 7 |
| storage.recording | 7 |
| system.time | 7 |
| device.identity | 6 |
| video.ndi | 6 |
| camera.image | 6 |
| room.layout | 6 |
| diagnostic.manufacturing | 5 |
| video.outputTransform | 5 |
| storage.sdCard | 5 |
| audio.recording | 4 |
| diagnostic.selfTest | 4 |
| diagnostic.audioTest | 4 |
| camera.exposure | 4 |
| network.interface | 4 |
| diagnostic.inputTest | 4 |
| network.bluetooth | 4 |
| audio.dante | 4 |
| output.source | 4 |
| auth.session | 4 |
| network.ap | 4 |
| network.serviceEndpoint | 3 |
| video.rtsp | 3 |
| log.export | 3 |
| diagnostic.storageTest | 3 |
| audio.mixer | 3 |
| device.indicator | 3 |
| auth.token | 3 |
| audio.uac | 2 |
| camera.calibration | 2 |
| storage.disk | 2 |
| privacy.state | 2 |
| device.power | 2 |
| room.participant | 2 |
| camera.ptz | 2 |
| input.key | 2 |
| audio.playback | 2 |
| diagnostic.videoTest | 2 |
| diagnostic.kvmTest | 2 |
| signage.playlist | 2 |
| signage.osd | 2 |
| firmware.updatePolicy | 2 |
| signage.schedule | 2 |
| room.schedule | 2 |
| sensor.motion | 1 |
| diagnostic.report | 1 |
| input.kvm | 1 |
| sensor.telemetry | 1 |
| signage.media | 1 |
| device.inventory | 1 |

## 需要人工复核的条目

| Source | File | Line | Legacy Entry | Candidate | Target | Question |
| --- | --- | --- | --- | --- | --- | --- |
| axdp_hid | workspace/legacy-migration/evidence/AXDP源码协议扫描清单.md | 148 | 0xC010F / 0x010F -> 0x018F / CommonSetConfigJson | vendor.genericConfig | (split required) | 补充 JSON schema 或 Config Name 后再拆成正式能力。 |
| axdp_hid | workspace/legacy-migration/evidence/AXDP源码协议扫描清单.md | 149 | 0xC0110 / 0x0110 -> 0x0190 / CommonGetConfigJson | vendor.genericConfig | (split required) | 补充 JSON schema 或 Config Name 后再拆成正式能力。 |
| axdp_hid | workspace/legacy-migration/evidence/AXDP源码协议扫描清单.md | 167 | 0xC0122 / 0x0122 -> 0x01A2 / CommonSetDebugJson | vendor.genericConfig | (split required) | 补充 JSON schema 或 Config Name 后再拆成正式能力。 |
| axdp_hid | workspace/legacy-migration/evidence/AXDP源码协议扫描清单.md | 168 | 0xC0123 / 0x0123 -> 0x01A3 / CommonGetDebugJson | vendor.genericConfig | (split required) | 补充 JSON schema 或 Config Name 后再拆成正式能力。 |
| axdp_hid | workspace/legacy-migration/evidence/AXDP源码协议扫描清单.md | 181 | 0xC013A / 0x013A -> 0x01BA / CommonSetDefaultConfigJson | vendor.genericConfig | (split required) | 补充 JSON schema 或 Config Name 后再拆成正式能力。 |
| axdp_hid | workspace/legacy-migration/evidence/AXDP源码协议扫描清单.md | 182 | 0xC013B / 0x013B -> 0x01BB / CommonGetDefaultConfigJson | vendor.genericConfig | (split required) | 补充 JSON schema 或 Config Name 后再拆成正式能力。 |
| axdp_hid | workspace/legacy-migration/evidence/AXDP源码协议扫描清单.md | 206 | 0xC0162 / 0x0162 -> 0x01E2 / CommonGetExternalSpeakerConfigJson | vendor.genericConfig | (split required) | 补充 JSON schema 或 Config Name 后再拆成正式能力。 |
| axdp_hid | workspace/legacy-migration/evidence/AXDP源码协议扫描清单.md | 207 | 0xC0163 / 0x0163 -> 0x01E3 / CommonSetExternalSpeakerConfigJson | vendor.genericConfig | (split required) | 补充 JSON schema 或 Config Name 后再拆成正式能力。 |
| axdp_hid | workspace/legacy-migration/evidence/AXDP源码协议扫描清单.md | 208 | 0xC0164 / 0x0164 -> 0x01E4 / CommonGetConfigJsonSubIndex | vendor.genericConfig | (split required) | 补充 JSON schema 或 Config Name 后再拆成正式能力。 |
| axdp_hid | workspace/legacy-migration/evidence/AXDP源码协议扫描清单.md | 209 | 0xC0165 / 0x0165 -> 0x01E5 / CommonSetConfigJsonSubIndex | vendor.genericConfig | (split required) | 补充 JSON schema 或 Config Name 后再拆成正式能力。 |
| axdp_hid | workspace/legacy-migration/evidence/AXDP源码协议扫描清单.md | 221 | 0xC0186 / 0x0186 -> 0x0206 / CommonSetConfigJsonBySn | vendor.genericConfig | (split required) | 补充 JSON schema 或 Config Name 后再拆成正式能力。 |
| axdp_hid | workspace/legacy-migration/evidence/AXDP源码协议扫描清单.md | 222 | 0xC0187 / 0x0187 -> 0x0207 / CommonGetConfigJsonBySn | vendor.genericConfig | (split required) | 补充 JSON schema 或 Config Name 后再拆成正式能力。 |
| signage_sdk | workspace/legacy-migration/evidence/NearHub-Launcher数字标牌设备管理通用管理命令.md | 353 | OnTelemetryReport | sensor.telemetry | sensor.telemetryReported | 确认遥测字段集合后再定 feature。 |
| vm33_http_json | workspace/legacy-migration/evidence/VM33协议文档.md | 69 | Config.Set | vendor.genericConfig | (split required) | 按具体 Config Name 生成 method/capability，泛方法不进入正式 AXTP method。 |
| vm33_http_json | workspace/legacy-migration/evidence/VM33协议文档.md | 83 | Config.MultiSet | vendor.genericConfig | (split required) | 按具体 Config Name 生成 method/capability，泛方法不进入正式 AXTP method。 |
| vm33_http_json | workspace/legacy-migration/evidence/VM33协议文档.md | 143 | Config.Get | vendor.genericConfig | (split required) | 按具体 Config Name 生成 method/capability，泛方法不进入正式 AXTP method。 |
| vm33_http_json | workspace/legacy-migration/evidence/VM33协议文档.md | 152 | Config.MultiGet | vendor.genericConfig | (split required) | 按具体 Config Name 生成 method/capability，泛方法不进入正式 AXTP method。 |
| vm33_http_json | workspace/legacy-migration/evidence/VM33协议文档.md | 157 | Config.MultiGet:Camera / Name=Camera | camera.image | camera.getImageConfig | 补充 Camera 配置字段后确认是否拆到 exposure/focus/whiteBalance。 |
| vm33_http_json | workspace/legacy-migration/evidence/VM33协议文档.md | 196 | Config.MultiGet:camera / Name=camera | camera.image | camera.getImageConfig | 补充 Camera 配置字段后确认是否拆到 exposure/focus/whiteBalance。 |
| vm33_http_json | workspace/legacy-migration/evidence/VM33协议文档.md | 330 | Config.SetMulti | vendor.genericConfig | (split required) | 按具体 Config Name 生成 method/capability，泛方法不进入正式 AXTP method。 |
| vm33_http_json | workspace/legacy-migration/evidence/VM33协议文档.md | 382 | Config.GetMulti | vendor.genericConfig | (split required) | 按具体 Config Name 生成 method/capability，泛方法不进入正式 AXTP method。 |
| vm33_http_json | workspace/legacy-migration/evidence/VM33协议文档.md | 431 | Config.Restore | vendor.genericConfig | (split required) | 按具体 Config Name 生成 method/capability，泛方法不进入正式 AXTP method。 |
| vm33_http_json | workspace/legacy-migration/evidence/VM33协议文档.md | 463 | Config.RestoreMulti | vendor.genericConfig | (split required) | 按具体 Config Name 生成 method/capability，泛方法不进入正式 AXTP method。 |
| vm33_http_json | workspace/legacy-migration/evidence/VM33协议文档.md | 536 | Config.Subscribe | vendor.genericConfig | (split required) | 按具体 Config Name 生成 method/capability，泛方法不进入正式 AXTP method。 |
| vm33_http_json | workspace/legacy-migration/evidence/VM33协议文档.md | 578 | Config.UnSubscribe | vendor.genericConfig | (split required) | 按具体 Config Name 生成 method/capability，泛方法不进入正式 AXTP method。 |

## 前 100 条样例

| Source | File | Line | Type | Legacy Wire | Class | Method | Event | Config Name | Capability | AXTP Method | AXTP Event | Protocol Doc | Confidence |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| axdp_hid | workspace/legacy-migration/evidence/AXDP源码协议扫描清单.md | 41 | command | AlphaUpgradeInfo |  | AlphaUpgradeInfo |  |  | firmware.ota | firmware.beginOta |  |  | high |
| axdp_hid | workspace/legacy-migration/evidence/AXDP源码协议扫描清单.md | 42 | command | AlphaUpgradeData |  | AlphaUpgradeData |  |  | firmware.ota | firmware.commitOtaBatch |  |  | high |
| axdp_hid | workspace/legacy-migration/evidence/AXDP源码协议扫描清单.md | 43 | command | AlphaDeviceInfo |  | AlphaDeviceInfo |  |  | device.identity | device.setIdentityConfig |  | workspace/protocol/device/device.info.md | high |
| axdp_hid | workspace/legacy-migration/evidence/AXDP源码协议扫描清单.md | 44 | command | AlphaDeviceType |  | AlphaDeviceType |  |  | device.identity | device.setIdentityConfig |  | workspace/protocol/device/device.info.md | high |
| axdp_hid | workspace/legacy-migration/evidence/AXDP源码协议扫描清单.md | 45 | command | BetaDeviceReset |  | BetaDeviceReset |  |  | system.initialization | system.reset |  | workspace/protocol/system/system.initialization.md | medium |
| axdp_hid | workspace/legacy-migration/evidence/AXDP源码协议扫描清单.md | 46 | command | BetaDeviceInfo |  | BetaDeviceInfo |  |  | device.identity | device.setIdentityConfig |  | workspace/protocol/device/device.info.md | high |
| axdp_hid | workspace/legacy-migration/evidence/AXDP源码协议扫描清单.md | 47 | command | BetaStartUpgrade |  | BetaStartUpgrade |  |  | firmware.ota | firmware.beginOta |  |  | high |
| axdp_hid | workspace/legacy-migration/evidence/AXDP源码协议扫描清单.md | 48 | command | BetaStopUpgrade |  | BetaStopUpgrade |  |  | firmware.ota | firmware.cancelOta |  |  | high |
| axdp_hid | workspace/legacy-migration/evidence/AXDP源码协议扫描清单.md | 49 | command | BetaUpgradeInfo |  | BetaUpgradeInfo |  |  | firmware.ota | firmware.beginOta |  |  | high |
| axdp_hid | workspace/legacy-migration/evidence/AXDP源码协议扫描清单.md | 50 | command | BetaUpgradeData |  | BetaUpgradeData |  |  | firmware.ota | firmware.commitOtaBatch |  |  | high |
| axdp_hid | workspace/legacy-migration/evidence/AXDP源码协议扫描清单.md | 51 | command | BetaUpgradeInfoEx |  | BetaUpgradeInfoEx |  |  | firmware.ota | firmware.beginOta |  |  | high |
| axdp_hid | workspace/legacy-migration/evidence/AXDP源码协议扫描清单.md | 52 | command | BetaUpgradeDataEx |  | BetaUpgradeDataEx |  |  | firmware.ota | firmware.commitOtaBatch |  |  | high |
| axdp_hid | workspace/legacy-migration/evidence/AXDP源码协议扫描清单.md | 53 | command | CommonSetVideoMode |  | CommonSetVideoMode |  |  | video.framing | video.setFramingMode |  | workspace/protocol/video/video.framing.md | medium |
| axdp_hid | workspace/legacy-migration/evidence/AXDP源码协议扫描清单.md | 54 | command | CommonGetVideoMode |  | CommonGetVideoMode |  |  | video.framing | video.getFramingMode |  | workspace/protocol/video/video.framing.md | medium |
| axdp_hid | workspace/legacy-migration/evidence/AXDP源码协议扫描清单.md | 55 | command | CommonGetPeopleNumber |  | CommonGetPeopleNumber |  |  | video.framing | video.getFramingConfig |  | workspace/protocol/video/video.framing.md | medium |
| axdp_hid | workspace/legacy-migration/evidence/AXDP源码协议扫描清单.md | 56 | command | CommonSetMicUsed |  | CommonSetMicUsed |  |  | audio.input | audio.setInputConfig |  | workspace/protocol/audio/audio.input.md | high |
| axdp_hid | workspace/legacy-migration/evidence/AXDP源码协议扫描清单.md | 57 | command | CommonAudioRecord |  | CommonAudioRecord |  |  | audio.recording | audio.startRecording |  | workspace/protocol/audio/audio.recording.md | high |
| axdp_hid | workspace/legacy-migration/evidence/AXDP源码协议扫描清单.md | 58 | command | CommonSetTestResult |  | CommonSetTestResult |  |  | diagnostic.selfTest | diagnostic.runSelfTest |  | workspace/protocol/diagnostic/diagnostic.selfTest.md | medium |
| axdp_hid | workspace/legacy-migration/evidence/AXDP源码协议扫描清单.md | 59 | command | CommonGetTestResult |  | CommonGetTestResult |  |  | diagnostic.selfTest | diagnostic.runSelfTest |  | workspace/protocol/diagnostic/diagnostic.selfTest.md | medium |
| axdp_hid | workspace/legacy-migration/evidence/AXDP源码协议扫描清单.md | 60 | command | CommonSetEncryptedInfo |  | CommonSetEncryptedInfo |  |  | diagnostic.manufacturing | diagnostic.setManufacturingData |  | workspace/protocol/diagnostic/diagnostic.manufacturing.md | high |
| axdp_hid | workspace/legacy-migration/evidence/AXDP源码协议扫描清单.md | 61 | command | CommonGetEncryptedInfo |  | CommonGetEncryptedInfo |  |  | device.info | device.getInfo |  | workspace/protocol/device/device.info.md | high |
| axdp_hid | workspace/legacy-migration/evidence/AXDP源码协议扫描清单.md | 62 | command | CommonGetGyroSlopeAngle |  | CommonGetGyroSlopeAngle |  |  | sensor.motion | sensor.getMotionState |  |  | medium |
| axdp_hid | workspace/legacy-migration/evidence/AXDP源码协议扫描清单.md | 63 | command | CommonSetUacState |  | CommonSetUacState |  |  | audio.uac | audio.setUacConfig |  | workspace/protocol/audio/audio.uac.md | high |
| axdp_hid | workspace/legacy-migration/evidence/AXDP源码协议扫描清单.md | 64 | command | CommonGetUacState |  | CommonGetUacState |  |  | audio.uac | audio.getUacState |  | workspace/protocol/audio/audio.uac.md | high |
| axdp_hid | workspace/legacy-migration/evidence/AXDP源码协议扫描清单.md | 65 | command | CommonTestAudioConsistency |  | CommonTestAudioConsistency |  |  | diagnostic.audioTest | diagnostic.runAudioTest |  | workspace/protocol/diagnostic/diagnostic.audioTest.md | high |
| axdp_hid | workspace/legacy-migration/evidence/AXDP源码协议扫描清单.md | 66 | command | CommonTestNetworkPort |  | CommonTestNetworkPort |  |  | diagnostic.networkTest | diagnostic.runNetworkTest |  | workspace/protocol/diagnostic/diagnostic.networkTest.md | high |
| axdp_hid | workspace/legacy-migration/evidence/AXDP源码协议扫描清单.md | 67 | command | CommonSetVideoTrackMode |  | CommonSetVideoTrackMode |  |  | video.framing | video.setFramingMode |  | workspace/protocol/video/video.framing.md | medium |
| axdp_hid | workspace/legacy-migration/evidence/AXDP源码协议扫描清单.md | 68 | command | CommonGetVideoTrackMode |  | CommonGetVideoTrackMode |  |  | video.framing | video.getFramingMode |  | workspace/protocol/video/video.framing.md | medium |
| axdp_hid | workspace/legacy-migration/evidence/AXDP源码协议扫描清单.md | 69 | command | CommonSetMirrorState |  | CommonSetMirrorState |  |  | video.outputTransform | video.setOutputTransform |  | workspace/protocol/video/video.outputTransform.md | high |
| axdp_hid | workspace/legacy-migration/evidence/AXDP源码协议扫描清单.md | 70 | command | CommonGetMirrorState |  | CommonGetMirrorState |  |  | video.outputTransform | video.getOutputTransform |  | workspace/protocol/video/video.outputTransform.md | high |
| axdp_hid | workspace/legacy-migration/evidence/AXDP源码协议扫描清单.md | 71 | command | CommonSetSpeakerTrackDelay |  | CommonSetSpeakerTrackDelay |  |  | video.framing | video.setFramingMode |  | workspace/protocol/video/video.framing.md | medium |
| axdp_hid | workspace/legacy-migration/evidence/AXDP源码协议扫描清单.md | 72 | command | CommonGetSpeakerTrackDelay |  | CommonGetSpeakerTrackDelay |  |  | video.framing | video.getFramingMode |  | workspace/protocol/video/video.framing.md | medium |
| axdp_hid | workspace/legacy-migration/evidence/AXDP源码协议扫描清单.md | 73 | command | CommonSetSplitScreenNumber |  | CommonSetSplitScreenNumber |  |  | video.layout | video.setLayoutConfig |  | workspace/protocol/video/video.layout.md | medium |
| axdp_hid | workspace/legacy-migration/evidence/AXDP源码协议扫描清单.md | 74 | command | CommonGetSplitScreenNumber |  | CommonGetSplitScreenNumber |  |  | video.layout | video.getLayoutConfig |  | workspace/protocol/video/video.layout.md | medium |
| axdp_hid | workspace/legacy-migration/evidence/AXDP源码协议扫描清单.md | 75 | command | CommonSetUsbName |  | CommonSetUsbName |  |  | input.hid | input.setHidConfig |  | workspace/protocol/input/input.hid.md | medium |
| axdp_hid | workspace/legacy-migration/evidence/AXDP源码协议扫描清单.md | 76 | command | CommonGetUsbName |  | CommonGetUsbName |  |  | input.hid | input.getHidConfig |  | workspace/protocol/input/input.hid.md | medium |
| axdp_hid | workspace/legacy-migration/evidence/AXDP源码协议扫描清单.md | 77 | command | CommonSetUsbPid |  | CommonSetUsbPid |  |  | input.hid | input.setHidConfig |  | workspace/protocol/input/input.hid.md | medium |
| axdp_hid | workspace/legacy-migration/evidence/AXDP源码协议扫描清单.md | 78 | command | CommonGetUsbPid |  | CommonGetUsbPid |  |  | input.hid | input.getHidConfig |  | workspace/protocol/input/input.hid.md | medium |
| axdp_hid | workspace/legacy-migration/evidence/AXDP源码协议扫描清单.md | 79 | command | CommonSetUsbVid |  | CommonSetUsbVid |  |  | input.hid | input.setHidConfig |  | workspace/protocol/input/input.hid.md | medium |
| axdp_hid | workspace/legacy-migration/evidence/AXDP源码协议扫描清单.md | 80 | command | CommonGetUsbVid |  | CommonGetUsbVid |  |  | input.hid | input.getHidConfig |  | workspace/protocol/input/input.hid.md | medium |
| axdp_hid | workspace/legacy-migration/evidence/AXDP源码协议扫描清单.md | 81 | command | CommonSetReboot |  | CommonSetReboot |  |  | system.lifecycle | system.reboot |  | workspace/protocol/system/system.lifecycle.md | high |
| axdp_hid | workspace/legacy-migration/evidence/AXDP源码协议扫描清单.md | 82 | command | CommonSetPowerLineFreq |  | CommonSetPowerLineFreq |  |  | camera.exposure | camera.setExposureConfig |  | workspace/protocol/camera/camera.exposure.md | high |
| axdp_hid | workspace/legacy-migration/evidence/AXDP源码协议扫描清单.md | 83 | command | CommonGetPowerLineFreq |  | CommonGetPowerLineFreq |  |  | camera.exposure | camera.getExposureConfig |  | workspace/protocol/camera/camera.exposure.md | high |
| axdp_hid | workspace/legacy-migration/evidence/AXDP源码协议扫描清单.md | 84 | command | CommonGetDeviceUniqueId |  | CommonGetDeviceUniqueId |  |  | device.identity | device.getIdentity |  | workspace/protocol/device/device.info.md | high |
| axdp_hid | workspace/legacy-migration/evidence/AXDP源码协议扫描清单.md | 85 | command | CommonSetDeviceUniqueId |  | CommonSetDeviceUniqueId |  |  | device.identity | device.setIdentityConfig |  | workspace/protocol/device/device.info.md | high |
| axdp_hid | workspace/legacy-migration/evidence/AXDP源码协议扫描清单.md | 86 | command | CommonGetAlgAuthContent |  | CommonGetAlgAuthContent |  |  | audio.algorithm | audio.getAlgorithmConfig |  | workspace/protocol/audio/audio.algorithm.md | medium |
| axdp_hid | workspace/legacy-migration/evidence/AXDP源码协议扫描清单.md | 87 | command | CommonSetAlgAuthContent |  | CommonSetAlgAuthContent |  |  | audio.algorithm | audio.setAlgorithmConfig |  | workspace/protocol/audio/audio.algorithm.md | medium |
| axdp_hid | workspace/legacy-migration/evidence/AXDP源码协议扫描清单.md | 88 | command | CommonSetWdrState |  | CommonSetWdrState |  |  | camera.exposure | camera.setExposureConfig |  | workspace/protocol/camera/camera.exposure.md | high |
| axdp_hid | workspace/legacy-migration/evidence/AXDP源码协议扫描清单.md | 89 | command | CommonGetWdrState |  | CommonGetWdrState |  |  | camera.exposure | camera.getExposureConfig |  | workspace/protocol/camera/camera.exposure.md | high |
| axdp_hid | workspace/legacy-migration/evidence/AXDP源码协议扫描清单.md | 90 | command | CommonSetOsdMirrorState |  | CommonSetOsdMirrorState |  |  | video.osd | video.setOsdConfig |  | workspace/protocol/video/video.osd.md | high |
| axdp_hid | workspace/legacy-migration/evidence/AXDP源码协议扫描清单.md | 91 | command | CommonGetOsdMirrorState |  | CommonGetOsdMirrorState |  |  | video.osd | video.getOsdConfig |  | workspace/protocol/video/video.osd.md | high |
| axdp_hid | workspace/legacy-migration/evidence/AXDP源码协议扫描清单.md | 92 | command | CommonSetAFCalibration |  | CommonSetAFCalibration |  |  | camera.calibration | camera.startCalibration |  | workspace/protocol/camera/camera.calibration.md | high |
| axdp_hid | workspace/legacy-migration/evidence/AXDP源码协议扫描清单.md | 93 | command | CommonStartAudioTest |  | CommonStartAudioTest |  |  | diagnostic.audioTest | diagnostic.runAudioTest |  | workspace/protocol/diagnostic/diagnostic.audioTest.md | high |
| axdp_hid | workspace/legacy-migration/evidence/AXDP源码协议扫描清单.md | 94 | command | CommonSetFlipState |  | CommonSetFlipState |  |  | video.outputTransform | video.setOutputTransform |  | workspace/protocol/video/video.outputTransform.md | high |
| axdp_hid | workspace/legacy-migration/evidence/AXDP源码协议扫描清单.md | 95 | command | CommonGetFlipState |  | CommonGetFlipState |  |  | video.outputTransform | video.getOutputTransform |  | workspace/protocol/video/video.outputTransform.md | high |
| axdp_hid | workspace/legacy-migration/evidence/AXDP源码协议扫描清单.md | 96 | command | CommonGetNoiseSuppressionLevel |  | CommonGetNoiseSuppressionLevel |  |  | audio.algorithm | audio.getAlgorithmConfig |  | workspace/protocol/audio/audio.algorithm.md | medium |
| axdp_hid | workspace/legacy-migration/evidence/AXDP源码协议扫描清单.md | 97 | command | CommonSetNoiseSuppressionLevel |  | CommonSetNoiseSuppressionLevel |  |  | audio.algorithm | audio.setAlgorithmConfig |  | workspace/protocol/audio/audio.algorithm.md | medium |
| axdp_hid | workspace/legacy-migration/evidence/AXDP源码协议扫描清单.md | 98 | command | CommonGetBootDetect |  | CommonGetBootDetect |  |  | system.initialization | system.getInitializationState |  | workspace/protocol/system/system.initialization.md | medium |
| axdp_hid | workspace/legacy-migration/evidence/AXDP源码协议扫描清单.md | 99 | command | CommonGetReverberationSuppressionLevel |  | CommonGetReverberationSuppressionLevel |  |  | audio.algorithm | audio.getAlgorithmConfig |  | workspace/protocol/audio/audio.algorithm.md | medium |
| axdp_hid | workspace/legacy-migration/evidence/AXDP源码协议扫描清单.md | 100 | command | CommonSetReverberationSuppressionLevel |  | CommonSetReverberationSuppressionLevel |  |  | audio.algorithm | audio.setAlgorithmConfig |  | workspace/protocol/audio/audio.algorithm.md | medium |
| axdp_hid | workspace/legacy-migration/evidence/AXDP源码协议扫描清单.md | 101 | command | CommonGetEchoCancellationLevel |  | CommonGetEchoCancellationLevel |  |  | audio.algorithm | audio.getAlgorithmConfig |  | workspace/protocol/audio/audio.algorithm.md | medium |
| axdp_hid | workspace/legacy-migration/evidence/AXDP源码协议扫描清单.md | 102 | command | CommonSetEchoCancellationLevel |  | CommonSetEchoCancellationLevel |  |  | audio.algorithm | audio.setAlgorithmConfig |  | workspace/protocol/audio/audio.algorithm.md | medium |
| axdp_hid | workspace/legacy-migration/evidence/AXDP源码协议扫描清单.md | 103 | command | CommonGetRecordEqParams |  | CommonGetRecordEqParams |  |  | audio.eq | audio.getEqConfig |  | workspace/protocol/audio/audio.eq.md | high |
| axdp_hid | workspace/legacy-migration/evidence/AXDP源码协议扫描清单.md | 104 | command | CommonSetRecordEqParams |  | CommonSetRecordEqParams |  |  | audio.eq | audio.setEqConfig |  | workspace/protocol/audio/audio.eq.md | high |
| axdp_hid | workspace/legacy-migration/evidence/AXDP源码协议扫描清单.md | 105 | command | CommonGetDefaultNoiseSuppressionLevel |  | CommonGetDefaultNoiseSuppressionLevel |  |  | audio.algorithm | audio.resetAlgorithmConfig |  | workspace/protocol/audio/audio.algorithm.md | medium |
| axdp_hid | workspace/legacy-migration/evidence/AXDP源码协议扫描清单.md | 106 | command | CommonSetDefaultNoiseSuppressionLevel |  | CommonSetDefaultNoiseSuppressionLevel |  |  | audio.algorithm | audio.resetAlgorithmConfig |  | workspace/protocol/audio/audio.algorithm.md | medium |
| axdp_hid | workspace/legacy-migration/evidence/AXDP源码协议扫描清单.md | 107 | command | CommonGetDefaultReverberationSuppressionLevel |  | CommonGetDefaultReverberationSuppressionLevel |  |  | audio.algorithm | audio.resetAlgorithmConfig |  | workspace/protocol/audio/audio.algorithm.md | medium |
| axdp_hid | workspace/legacy-migration/evidence/AXDP源码协议扫描清单.md | 108 | command | CommonSetDefaultReverberationSuppressionLevel |  | CommonSetDefaultReverberationSuppressionLevel |  |  | audio.algorithm | audio.resetAlgorithmConfig |  | workspace/protocol/audio/audio.algorithm.md | medium |
| axdp_hid | workspace/legacy-migration/evidence/AXDP源码协议扫描清单.md | 109 | command | CommonGetDefaultEchoCancellationLevel |  | CommonGetDefaultEchoCancellationLevel |  |  | audio.algorithm | audio.resetAlgorithmConfig |  | workspace/protocol/audio/audio.algorithm.md | medium |
| axdp_hid | workspace/legacy-migration/evidence/AXDP源码协议扫描清单.md | 110 | command | CommonSetDefaultEchoCancellationLevel |  | CommonSetDefaultEchoCancellationLevel |  |  | audio.algorithm | audio.resetAlgorithmConfig |  | workspace/protocol/audio/audio.algorithm.md | medium |
| axdp_hid | workspace/legacy-migration/evidence/AXDP源码协议扫描清单.md | 111 | command | CommonGetDefaultRecordEqParams |  | CommonGetDefaultRecordEqParams |  |  | audio.eq | audio.resetEqConfig |  | workspace/protocol/audio/audio.eq.md | high |
| axdp_hid | workspace/legacy-migration/evidence/AXDP源码协议扫描清单.md | 112 | command | CommonSetDefaultRecordEqParams |  | CommonSetDefaultRecordEqParams |  |  | audio.eq | audio.resetEqConfig |  | workspace/protocol/audio/audio.eq.md | high |
| axdp_hid | workspace/legacy-migration/evidence/AXDP源码协议扫描清单.md | 113 | command | CommonResetAudioAlgorithmParams |  | CommonResetAudioAlgorithmParams |  |  | audio.algorithm | audio.resetAlgorithmConfig |  | workspace/protocol/audio/audio.algorithm.md | medium |
| axdp_hid | workspace/legacy-migration/evidence/AXDP源码协议扫描清单.md | 114 | command | CommonGetMuteLightEnhancement |  | CommonGetMuteLightEnhancement |  |  | audio.volume | audio.getVolumeState |  | workspace/protocol/audio/audio.volume.md | high |
| axdp_hid | workspace/legacy-migration/evidence/AXDP源码协议扫描清单.md | 115 | command | CommonSetMuteLightEnhancement |  | CommonSetMuteLightEnhancement |  |  | audio.volume | audio.setVolumeConfig |  | workspace/protocol/audio/audio.volume.md | high |
| axdp_hid | workspace/legacy-migration/evidence/AXDP源码协议扫描清单.md | 116 | command | CommonAudioRecordStart |  | CommonAudioRecordStart |  |  | audio.recording | audio.startRecording |  | workspace/protocol/audio/audio.recording.md | high |
| axdp_hid | workspace/legacy-migration/evidence/AXDP源码协议扫描清单.md | 117 | command | CommonAudioRecordStop |  | CommonAudioRecordStop |  |  | audio.recording | audio.stopRecording |  | workspace/protocol/audio/audio.recording.md | high |
| axdp_hid | workspace/legacy-migration/evidence/AXDP源码协议扫描清单.md | 118 | command | CommonAudioRecordData |  | CommonAudioRecordData |  |  | audio.recording | audio.startRecording |  | workspace/protocol/audio/audio.recording.md | high |
| axdp_hid | workspace/legacy-migration/evidence/AXDP源码协议扫描清单.md | 119 | command | CommonGetAFCalibration |  | CommonGetAFCalibration |  |  | camera.calibration | camera.getCalibrationState |  | workspace/protocol/camera/camera.calibration.md | high |
| axdp_hid | workspace/legacy-migration/evidence/AXDP源码协议扫描清单.md | 120 | command | CommonGetDDRCapacity |  | CommonGetDDRCapacity |  |  | storage.disk | storage.getDiskState |  | workspace/protocol/storage/storage.disk.md | medium |
| axdp_hid | workspace/legacy-migration/evidence/AXDP源码协议扫描清单.md | 121 | command | CommonSetPanTiltZoom |  | CommonSetPanTiltZoom |  |  | camera.zoom | camera.setZoomConfig |  | workspace/protocol/camera/camera.zoom.md | high |
| axdp_hid | workspace/legacy-migration/evidence/AXDP源码协议扫描清单.md | 122 | command | CommonGetPanTiltZoom |  | CommonGetPanTiltZoom |  |  | camera.zoom | camera.getZoomState |  | workspace/protocol/camera/camera.zoom.md | high |
| axdp_hid | workspace/legacy-migration/evidence/AXDP源码协议扫描清单.md | 123 | command | CommonGetDumpInfo |  | CommonGetDumpInfo |  |  | diagnostic.report | diagnostic.getReportExportState |  | workspace/protocol/diagnostic/diagnostic.report.md | medium |
| axdp_hid | workspace/legacy-migration/evidence/AXDP源码协议扫描清单.md | 124 | command | CommonSetAlgoEnable |  | CommonSetAlgoEnable |  |  | audio.algorithm | audio.setAlgorithmConfig |  | workspace/protocol/audio/audio.algorithm.md | medium |
| axdp_hid | workspace/legacy-migration/evidence/AXDP源码协议扫描清单.md | 125 | command | CommonGetAlgoEnable |  | CommonGetAlgoEnable |  |  | audio.algorithm | audio.getAlgorithmConfig |  | workspace/protocol/audio/audio.algorithm.md | medium |
| axdp_hid | workspace/legacy-migration/evidence/AXDP源码协议扫描清单.md | 126 | command | CommonSetHidCall |  | CommonSetHidCall |  |  | input.hid | input.setHidConfig |  | workspace/protocol/input/input.hid.md | medium |
| axdp_hid | workspace/legacy-migration/evidence/AXDP源码协议扫描清单.md | 127 | command | CommonGetHidCall |  | CommonGetHidCall |  |  | input.hid | input.getHidConfig |  | workspace/protocol/input/input.hid.md | medium |
| axdp_hid | workspace/legacy-migration/evidence/AXDP源码协议扫描清单.md | 128 | command | CommonSetUsbSpeedMode |  | CommonSetUsbSpeedMode |  |  | input.hid | input.setHidConfig |  | workspace/protocol/input/input.hid.md | medium |
| axdp_hid | workspace/legacy-migration/evidence/AXDP源码协议扫描清单.md | 129 | command | CommonGetUsbSpeedMode |  | CommonGetUsbSpeedMode |  |  | input.hid | input.getHidConfig |  | workspace/protocol/input/input.hid.md | medium |
| axdp_hid | workspace/legacy-migration/evidence/AXDP源码协议扫描清单.md | 130 | command | CommonGetBootInfo |  | CommonGetBootInfo |  |  | system.initialization | system.getInitializationState |  | workspace/protocol/system/system.initialization.md | medium |
| axdp_hid | workspace/legacy-migration/evidence/AXDP源码协议扫描清单.md | 131 | command | CommonSetBootErase |  | CommonSetBootErase |  |  | system.initialization | system.startInitialization |  | workspace/protocol/system/system.initialization.md | medium |
| axdp_hid | workspace/legacy-migration/evidence/AXDP源码协议扫描清单.md | 132 | command | CommonSetPrivacyEnable |  | CommonSetPrivacyEnable |  |  | privacy.state | privacy.setModeConfig |  | workspace/protocol/privacy/privacy.state.md | high |
| axdp_hid | workspace/legacy-migration/evidence/AXDP源码协议扫描清单.md | 133 | command | CommonGetPrivacyEnable |  | CommonGetPrivacyEnable |  |  | privacy.state | privacy.getState |  | workspace/protocol/privacy/privacy.state.md | high |
| axdp_hid | workspace/legacy-migration/evidence/AXDP源码协议扫描清单.md | 134 | command | CommonSetNDIState |  | CommonSetNDIState |  |  | video.ndi | video.setNdiConfig |  | workspace/protocol/video/video.ndi.md | high |
| axdp_hid | workspace/legacy-migration/evidence/AXDP源码协议扫描清单.md | 135 | command | CommonGetNDIState |  | CommonGetNDIState |  |  | video.ndi | video.getNdiConfig |  | workspace/protocol/video/video.ndi.md | high |
| axdp_hid | workspace/legacy-migration/evidence/AXDP源码协议扫描清单.md | 136 | command | CommonGetIPConfig |  | CommonGetIPConfig |  |  | network.ip | network.getIpConfig |  | workspace/protocol/network/network.ip.md | high |
| axdp_hid | workspace/legacy-migration/evidence/AXDP源码协议扫描清单.md | 137 | command | CommonSetDHCPState |  | CommonSetDHCPState |  |  | network.ip | network.setIpConfig |  | workspace/protocol/network/network.ip.md | high |
| axdp_hid | workspace/legacy-migration/evidence/AXDP源码协议扫描清单.md | 138 | command | CommonGetDHCPState |  | CommonGetDHCPState |  |  | network.ip | network.getIpConfig |  | workspace/protocol/network/network.ip.md | high |
| axdp_hid | workspace/legacy-migration/evidence/AXDP源码协议扫描清单.md | 139 | command | CommonSetIPAddress |  | CommonSetIPAddress |  |  | network.ip | network.setIpConfig |  | workspace/protocol/network/network.ip.md | high |
| axdp_hid | workspace/legacy-migration/evidence/AXDP源码协议扫描清单.md | 140 | command | CommonGetIPAddress |  | CommonGetIPAddress |  |  | network.ip | network.getIpConfig |  | workspace/protocol/network/network.ip.md | high |
