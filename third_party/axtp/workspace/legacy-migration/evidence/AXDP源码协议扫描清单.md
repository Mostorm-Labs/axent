# AXDP 老协议源码扫描清单

> 来源仓库: https://gitee.com/auditoryworks_hamedal/axdp  
> 扫描版本: `c8a1068b11ecd3dcabd69fcac6ef6e0d60a55ea4`  
> 远端校验: 2026-06-01 已确认 `origin/main` 仍指向该 commit  
> 说明: 本文按源码实现反推协议，不以 Excel 为准；响应字段以 `src/message_recv_handler.cpp` 中实际读取和 callback 使用为准。

## 扫描范围

- 命令枚举: `src/protocol_defines.h` 的 `UniCmd`, 共 201 条: Alpha 4 条, Beta 8 条, Common 189 条。
- 发包实现: `src/message_send_handler.cpp`, `src/message_send_handler_ex.cpp`, 以及升级/FBF helper 调用链。
- 收包解析: `src/message_recv_handler.cpp`, `include/axdp_api.h`, `include/axdp_api_ex.h` 的 callback。
- HID 公共报告: `src/human_interface_handler.cpp`。

## AXDP 帧格式与约定

- HID 私有 report 通常为 `0x05`; Alpha 早期实现中也存在 report id `0` 兼容路径。
- AXDP frame: `magic(0xFFA5,2B) + version(0x0001,2B) + dst(2B) + src(2B) + cmd(2B) + payload_len(2B) + crc(2B) + payload(N)`。
- 头部字段由 `MessageBuilder::pack` 写为网络序/大端; CRC 字段先置 0, 使用 CRC-16/XMODEM 计算。
- `UniCmd` 的高位用作协议族 mask: Alpha=`0xA0000`, Beta=`0xB0000`, Common=`0xC0000`; 实际 wire cmd 是低 16 位。
- 设备响应的 wire cmd 通常为请求 cmd + `0x80` 的 16 位结果; 接收侧再按协议族 mask 还原为 `UniCmd`。例如 `0x0021 -> 0x00A1`。
- 常用目标地址: `0x0002` 主设备, `0xFFFF` 广播, `0x4000` 子索引配置。
- `result(1/2/4B)` 是接收侧 `convert()` 的统一记法: payload 长度为 4/2/1 时分别按 BE `uint32/uint16/uint8` 取值；其他长度 result 默认为 0。
- `case 空` 表示 `message_recv_handler.cpp` 中存在对应 `case`，但 case 内没有读取 payload，也没有触发 callback；设备即使返回 ACK，当前库也会丢弃。

## 接收侧读取模式总览

- 已发送/已解析: 92 条，接收侧实际读取了 `result`、固定字段或 raw payload，并触发 callback 或驱动内部流程。
- 已发送/case空: 45 条，发送 API 存在，但响应 `case` 为空，当前库不使用设备回复内容。
- 仅响应/case空: 34 条，枚举和响应 `case` 存在，但没有发送封装，也没有读取响应 payload。
- 仅响应/解析未回调: 14 条，接收侧读了字段或 raw payload，但公开 callback 被注释掉。
- 仅发送/无接收case: 7 条，发送 API 存在，但 `message_recv_handler.cpp` 没有对应响应 `case`。
- 仅响应/已解析: 3 条，当前库只处理设备上报或响应，没有找到主机发送封装。
- 已发送/解析未回调: 3 条，发送和解析都有，但最终 callback 被注释掉。
- 仅枚举: 3 条，只在 `UniCmd` 中出现，未找到发送封装和响应解析。

## 完整命令清单

| # | UniCmd | ID / req -> resp | 状态 | 输入 payload | 输出 payload / 接收侧字段长度 |
|---:|---|---|---|---|---|
| 1 | `AlphaUpgradeInfo` | `0xA0001` / `0x0001` -> `0x0081` | 已发送/已解析 | BlockInfo/兼容块信息: SubindexHeader(可选,38B) + flash_block:uint32 + slice_size:uint32 + slices_total:uint32 + block_size:uint32 + md5:16B | 读取 uint32 BE status(4B)；0 表示成功并开始连续发送 AlphaUpgradeData，否则 DFU Failed。 |
| 2 | `AlphaUpgradeData` | `0xA0002` / `0x0002` -> `0x0082` | 已发送/已解析 | SliceData: SubindexHeader(可选,38B) + slice_index:uint32 + 固件分片 bytes | 读取 uint32 BE upgrade_status(4B)：0xFFFFFFFF Success，0xFFFFEEEE Failed，其他状态仅结束 downing。 |
| 3 | `AlphaDeviceInfo` | `0xA0003` / `0x0003` -> `0x0083` | 已发送/已解析 | 无 payload, dst=0x0002 | 读取 `sizeof(DevInfoSt)=97B`：product_name[32]、software_ver[32]、serial_number[32]、dev_id:uint8；回调只拷 product/sn/sw。 |
| 4 | `AlphaDeviceType` | `0xA0004` / `0x0004` -> `0x0084` | 仅响应/已解析 | 源码无发送封装; 语义为查询类型 | 读取 `sizeof(DevTypeSt)=4B`：type:uint16 BE + count:uint16 BE；无公开 callback。 |
| 5 | `BetaDeviceReset` | `0xB0001` / `0x0001` -> `0x0081` | 已发送/已解析 | 无 payload, dst=broadcast | 读取 result(1/2/4B: len=4 读 BE uint32, len=2 读 BE uint16, len=1 读 uint8) -> result==0 回调 onResetDevice(Success)，否则 Failed。 |
| 6 | `BetaDeviceInfo` | `0xB0002` / `0x0002` -> `0x0082` | 已发送/已解析 | 无 payload, dst=broadcast | 变长：dev_count:uint16 BE；每设备 dev_type:uint16 BE + info_flag:uint32 BE + 若干 TLV(len:uint8 + value[len])，已知 product/hw/sw/sn/uid/index。 |
| 7 | `BetaStartUpgrade` | `0xB0003` / `0x0003` -> `0x0083` | 已发送/已解析 | 无 payload, dst=broadcast | 要求 len=4，读取 uint32 BE result；0 表示所有目标 ready 后发送 BetaUpgradeInfo，否则匹配 dst 时 DFU Failed。 |
| 8 | `BetaStopUpgrade` | `0xB0004` / `0x0004` -> `0x0084` | 已发送/已解析 | uint32 BE success=0, dst=broadcast | 不读取 payload；所有目标 src ready 后回调 DFU Success。 |
| 9 | `BetaUpgradeInfo` | `0xB0005` / `0x0005` -> `0x0085` | 已发送/已解析 | BlockInfo/兼容块信息: SubindexHeader(可选,38B) + block fields + md5:16B | 要求 len=4，读取 uint32 BE result；0 后发送第一片 BetaUpgradeData。 |
| 10 | `BetaUpgradeData` | `0xB0006` / `0x0006` -> `0x0086` | 已发送/已解析 | SliceData: SubindexHeader(可选,38B) + slice_index:uint32 + 固件分片 bytes | 要求 len=4；读取 uint32 但代码用 htonl(pv[0]) 比较 success/fail/requested_slice_index，驱动下一片或失败。 |
| 11 | `BetaUpgradeInfoEx` | `0xB0011` / `0x0011` -> `0x0091` | 已发送/已解析 | BlockInfo/兼容块信息, Ex 批量发送策略 | 读取 uint32 BE result(4B)；0 后本端循环发送所有 Ex 数据片，结束后 Verifying。 |
| 12 | `BetaUpgradeDataEx` | `0xB0012` / `0x0012` -> `0x0092` | 已发送/已解析 | SliceData, Ex 批量发送策略 | 读取 uint32 BE upgrade_status(4B)：0xFFFFFFFF Success，0xFFFFEEEE Failed。 |
| 13 | `CommonSetVideoMode` | `0xC0021` / `0x0021` -> `0x00A1` | 已发送/case空 | uint32 BE VideoMode(0 全景,1 智能跟踪) | case 空，未读取 payload；无 callback。 |
| 14 | `CommonGetVideoMode` | `0xC0022` / `0x0022` -> `0x00A2` | 已发送/已解析 | 无 payload | 读取 result(1/2/4B: len=4 读 BE uint32, len=2 读 BE uint16, len=1 读 uint8) -> onGetVideoMode(VideoMode(result))。 |
| 15 | `CommonGetPeopleNumber` | `0xC0023` / `0x0023` -> `0x00A3` | 仅响应/解析未回调 | 源码未找到发送封装; 通常无 payload | 读取 result(1/2/4B: len=4 读 BE uint32, len=2 读 BE uint16, len=1 读 uint8)，但 onGetPeopleCount 注释掉。 |
| 16 | `CommonSetMicUsed` | `0xC0024` / `0x0024` -> `0x00A4` | 已发送/case空 | uint32 BE mic_index/mask | case 空，未读取 payload；无 callback。 |
| 17 | `CommonAudioRecord` | `0xC0025` / `0x0025` -> `0x00A5` | 已发送/case空 | uint32 BE mic_mask, 工厂录音 | case 空，未读取 payload；无 callback。 |
| 18 | `CommonSetTestResult` | `0xC0026` / `0x0026` -> `0x00A6` | 已发送/case空 | uint32 BE test_index + uint32 BE test_result | case 空，未读取 payload；无 callback。 |
| 19 | `CommonGetTestResult` | `0xC0027` / `0x0027` -> `0x00A7` | 已发送/case空 | 源码发送无 payload; API 入参 test_index 未写入 payload | case 空，未读取 payload；无 callback。 |
| 20 | `CommonSetEncryptedInfo` | `0xC0028` / `0x0028` -> `0x00A8` | 已发送/case空 | uint32 BE flash_block + hwid/sn bytes | case 空，未读取 payload；无 callback。 |
| 21 | `CommonGetEncryptedInfo` | `0xC0029` / `0x0029` -> `0x00A9` | 已发送/case空 | 源码发送无 payload; API state 未写入 payload | case 空，未读取 payload；无 callback。 |
| 22 | `CommonGetGyroSlopeAngle` | `0xC002A` / `0x002A` -> `0x00AA` | 仅响应/已解析 | 源码未找到发送封装; 通常无 payload | 读取 result(1/2/4B: len=4 读 BE uint32, len=2 读 BE uint16, len=1 读 uint8) -> EventCallbackDelegateEx::onGetGyroSlopeAngle(result)。 |
| 23 | `CommonSetUacState` | `0xC002B` / `0x002B` -> `0x00AB` | 已发送/case空 | uint32 BE EnableState | case 空，未读取 payload；无 callback。 |
| 24 | `CommonGetUacState` | `0xC002C` / `0x002C` -> `0x00AC` | 已发送/已解析 | 无 payload | 读取 result(1/2/4B: len=4 读 BE uint32, len=2 读 BE uint16, len=1 读 uint8) -> onGetUacState(EnableState(result))。 |
| 25 | `CommonTestAudioConsistency` | `0xC002D` / `0x002D` -> `0x00AD` | 已发送/case空 | uint32 BE TestTaskCommand | case 空，未读取 payload；无 callback。 |
| 26 | `CommonTestNetworkPort` | `0xC002E` / `0x002E` -> `0x00AE` | 已发送/case空 | uint32 BE TestTaskCommand | case 空，未读取 payload；无 callback。 |
| 27 | `CommonSetVideoTrackMode` | `0xC0031` / `0x0031` -> `0x00B1` | 已发送/case空 | uint32 BE VideoTrackMode(0..6) | case 空，未读取 payload；无 callback。 |
| 28 | `CommonGetVideoTrackMode` | `0xC0032` / `0x0032` -> `0x00B2` | 已发送/已解析 | 无 payload | 读取 result(1/2/4B: len=4 读 BE uint32, len=2 读 BE uint16, len=1 读 uint8) -> onGetVideoTrackMode(VideoTrackMode(result))。 |
| 29 | `CommonSetMirrorState` | `0xC0033` / `0x0033` -> `0x00B3` | 已发送/case空 | uint32 BE EnableState | case 空，未读取 payload；无 callback。 |
| 30 | `CommonGetMirrorState` | `0xC0034` / `0x0034` -> `0x00B4` | 已发送/已解析 | 无 payload | 读取 result(1/2/4B: len=4 读 BE uint32, len=2 读 BE uint16, len=1 读 uint8) -> onGetMirrorState(EnableState(result))。 |
| 31 | `CommonSetSpeakerTrackDelay` | `0xC0035` / `0x0035` -> `0x00B5` | 已发送/case空 | uint32 BE delay_seconds | case 空，未读取 payload；无 callback。 |
| 32 | `CommonGetSpeakerTrackDelay` | `0xC0036` / `0x0036` -> `0x00B6` | 已发送/已解析 | 无 payload | 读取 result(1/2/4B: len=4 读 BE uint32, len=2 读 BE uint16, len=1 读 uint8) -> onGetSpeakerTrackDelay(result)。 |
| 33 | `CommonSetSplitScreenNumber` | `0xC0037` / `0x0037` -> `0x00B7` | 已发送/case空 | uint32 BE number | case 空，未读取 payload；无 callback。 |
| 34 | `CommonGetSplitScreenNumber` | `0xC0038` / `0x0038` -> `0x00B8` | 已发送/已解析 | 无 payload | 读取 result(1/2/4B: len=4 读 BE uint32, len=2 读 BE uint16, len=1 读 uint8) -> onGetSplitScreenNumber(result)。 |
| 35 | `CommonSetUsbName` | `0xC0039` / `0x0039` -> `0x00B9` | 仅响应/case空 | 源码未找到发送封装; payload 未明确 | case 空，未读取 payload；无 callback。 |
| 36 | `CommonGetUsbName` | `0xC003A` / `0x003A` -> `0x00BA` | 仅响应/case空 | 源码未找到发送封装; 通常无 payload | case 空，未读取 payload；无 callback。 |
| 37 | `CommonSetUsbPid` | `0xC003B` / `0x003B` -> `0x00BB` | 仅响应/case空 | 源码未找到发送封装; payload 未明确 | case 空，未读取 payload；无 callback。 |
| 38 | `CommonGetUsbPid` | `0xC003C` / `0x003C` -> `0x00BC` | 仅响应/case空 | 源码未找到发送封装; 通常无 payload | case 空，未读取 payload；无 callback。 |
| 39 | `CommonSetUsbVid` | `0xC003D` / `0x003D` -> `0x00BD` | 仅响应/case空 | 源码未找到发送封装; payload 未明确 | case 空，未读取 payload；无 callback。 |
| 40 | `CommonGetUsbVid` | `0xC003E` / `0x003E` -> `0x00BE` | 仅响应/case空 | 源码未找到发送封装; 通常无 payload | case 空，未读取 payload；无 callback。 |
| 41 | `CommonSetReboot` | `0xC003F` / `0x003F` -> `0x00BF` | 已发送/解析未回调 | 无 payload | 读取 result(1/2/4B: len=4 读 BE uint32, len=2 读 BE uint16, len=1 读 uint8)，但 onRebootDevice 注释掉；注释说明很多设备无回传。 |
| 42 | `CommonSetPowerLineFreq` | `0xC0040` / `0x0040` -> `0x00C0` | 已发送/case空 | uint8 PowerlineFreqType(1=50Hz,2=60Hz) | case 空，未读取 payload；无 callback。 |
| 43 | `CommonGetPowerLineFreq` | `0xC0041` / `0x0041` -> `0x00C1` | 已发送/已解析 | 无 payload | 读取 result(1/2/4B: len=4 读 BE uint32, len=2 读 BE uint16, len=1 读 uint8) -> onGetPowerLineFreq(PowerlineFreqType(result))。 |
| 44 | `CommonGetDeviceUniqueId` | `0xC0042` / `0x0042` -> `0x00C2` | 仅响应/case空 | 源码未找到发送封装; 通常无 payload | case 空，未读取 payload；无 callback。 |
| 45 | `CommonSetDeviceUniqueId` | `0xC0043` / `0x0043` -> `0x00C3` | 仅响应/case空 | 源码未找到发送封装; payload 未明确 | case 空，未读取 payload；无 callback。 |
| 46 | `CommonGetAlgAuthContent` | `0xC0044` / `0x0044` -> `0x00C4` | 仅响应/case空 | 源码未实现发送; 语义为获取算法授权内容 | case 空，未读取 payload；无 callback。 |
| 47 | `CommonSetAlgAuthContent` | `0xC0045` / `0x0045` -> `0x00C5` | 已发送/case空 | 语义为 auth_content bytes; 源码实际传 &auth_content 指针地址(疑似 bug) | case 空，未读取 payload；无 callback。 |
| 48 | `CommonSetWdrState` | `0xC0046` / `0x0046` -> `0x00C6` | 已发送/case空 | uint32 BE EnableState | case 空，未读取 payload；无 callback。 |
| 49 | `CommonGetWdrState` | `0xC0047` / `0x0047` -> `0x00C7` | 已发送/已解析 | 无 payload | 读取 result(1/2/4B: len=4 读 BE uint32, len=2 读 BE uint16, len=1 读 uint8) -> onGetWdrState(EnableState(result))。 |
| 50 | `CommonSetOsdMirrorState` | `0xC0048` / `0x0048` -> `0x00C8` | 已发送/case空 | uint32 BE EnableState | case 空，未读取 payload；无 callback。 |
| 51 | `CommonGetOsdMirrorState` | `0xC0049` / `0x0049` -> `0x00C9` | 已发送/已解析 | 无 payload | 读取 result(1/2/4B: len=4 读 BE uint32, len=2 读 BE uint16, len=1 读 uint8) -> onGetOsdMirrorState(EnableState(result))。 |
| 52 | `CommonSetAFCalibration` | `0xC004A` / `0x004A` -> `0x00CA` | 已发送/case空 | 无 payload | case 空，未读取 payload；无 callback。 |
| 53 | `CommonStartAudioTest` | `0xC004B` / `0x004B` -> `0x00CB` | 仅响应/case空 | 源码未找到发送封装; payload 未明确 | case 空，未读取 payload；无 callback。 |
| 54 | `CommonSetFlipState` | `0xC004C` / `0x004C` -> `0x00CC` | 已发送/case空 | uint32 BE EnableState | case 空，未读取 payload；无 callback。 |
| 55 | `CommonGetFlipState` | `0xC004D` / `0x004D` -> `0x00CD` | 已发送/已解析 | 无 payload | 读取 result(1/2/4B: len=4 读 BE uint32, len=2 读 BE uint16, len=1 读 uint8) -> onGetFlipState(EnableState(result))。 |
| 56 | `CommonGetNoiseSuppressionLevel` | `0xC004E` / `0x004E` -> `0x00CE` | 已发送/已解析 | 无 payload, dst=broadcast | 读取 result(1/2/4B: len=4 读 BE uint32, len=2 读 BE uint16, len=1 读 uint8)；仅 head.src()==2 时 onGetNoiseSuppressionLevel(DegreeLevel(result))。 |
| 57 | `CommonSetNoiseSuppressionLevel` | `0xC004F` / `0x004F` -> `0x00CF` | 已发送/case空 | uint8 DegreeLevel(0..4), dst=broadcast | case 空，未读取 payload；无 callback。 |
| 58 | `CommonGetBootDetect` | `0xC0050` / `0x0050` -> `0x00D0` | 仅响应/case空 | 源码未找到发送封装; 通常无 payload | case 空，未读取 payload；无 callback。 |
| 59 | `CommonGetReverberationSuppressionLevel` | `0xC0051` / `0x0051` -> `0x00D1` | 已发送/已解析 | 无 payload, dst=broadcast | 读取 result(1/2/4B: len=4 读 BE uint32, len=2 读 BE uint16, len=1 读 uint8)；仅 head.src()==2 时 onGetReverbrationSuppressionLevel(DegreeLevel(result))。 |
| 60 | `CommonSetReverberationSuppressionLevel` | `0xC0052` / `0x0052` -> `0x00D2` | 已发送/case空 | uint8 DegreeLevel(0..4), dst=broadcast | case 空，未读取 payload；无 callback。 |
| 61 | `CommonGetEchoCancellationLevel` | `0xC0053` / `0x0053` -> `0x00D3` | 已发送/已解析 | 无 payload, dst=broadcast | 读取 result(1/2/4B: len=4 读 BE uint32, len=2 读 BE uint16, len=1 读 uint8)；仅 head.src()==2 时 onGetEchoCancellationLevel(DegreeLevel(result))。 |
| 62 | `CommonSetEchoCancellationLevel` | `0xC0054` / `0x0054` -> `0x00D4` | 已发送/case空 | uint8 DegreeLevel(0..4), dst=broadcast | case 空，未读取 payload；无 callback。 |
| 63 | `CommonGetRecordEqParams` | `0xC0055` / `0x0055` -> `0x00D5` | 仅响应/case空 | 源码未找到发送封装; 通常无 payload | case 空，未读取 payload；无 callback。 |
| 64 | `CommonSetRecordEqParams` | `0xC0056` / `0x0056` -> `0x00D6` | 仅响应/case空 | 源码未找到发送封装; payload 未明确 | case 空，未读取 payload；无 callback。 |
| 65 | `CommonGetDefaultNoiseSuppressionLevel` | `0xC0057` / `0x0057` -> `0x00D7` | 仅响应/解析未回调 | 源码未找到发送封装; 通常无 payload | 读取 result(1/2/4B: len=4 读 BE uint32, len=2 读 BE uint16, len=1 读 uint8)，但默认降噪 callback 注释掉。 |
| 66 | `CommonSetDefaultNoiseSuppressionLevel` | `0xC0058` / `0x0058` -> `0x00D8` | 仅响应/case空 | 源码未找到发送封装; payload 未明确 | case 空，未读取 payload；无 callback。 |
| 67 | `CommonGetDefaultReverberationSuppressionLevel` | `0xC0059` / `0x0059` -> `0x00D9` | 仅响应/解析未回调 | 源码未找到发送封装; 通常无 payload | 读取 result(1/2/4B: len=4 读 BE uint32, len=2 读 BE uint16, len=1 读 uint8)，但默认混响抑制 callback 注释掉。 |
| 68 | `CommonSetDefaultReverberationSuppressionLevel` | `0xC005A` / `0x005A` -> `0x00DA` | 仅响应/case空 | 源码未找到发送封装; payload 未明确 | case 空，未读取 payload；无 callback。 |
| 69 | `CommonGetDefaultEchoCancellationLevel` | `0xC005B` / `0x005B` -> `0x00DB` | 仅响应/解析未回调 | 源码未找到发送封装; 通常无 payload | 读取 result(1/2/4B: len=4 读 BE uint32, len=2 读 BE uint16, len=1 读 uint8)，但默认回声消除 callback 注释掉。 |
| 70 | `CommonSetDefaultEchoCancellationLevel` | `0xC005C` / `0x005C` -> `0x00DC` | 仅响应/case空 | 源码未找到发送封装; payload 未明确 | case 空，未读取 payload；无 callback。 |
| 71 | `CommonGetDefaultRecordEqParams` | `0xC005D` / `0x005D` -> `0x00DD` | 仅响应/case空 | 源码未找到发送封装; 通常无 payload | case 空，未读取 payload；无 callback。 |
| 72 | `CommonSetDefaultRecordEqParams` | `0xC005F` / `0x005F` -> `0x00DF` | 仅响应/case空 | 源码未找到发送封装; payload 未明确 | case 空，未读取 payload；无 callback。 |
| 73 | `CommonResetAudioAlgorithmParams` | `0xC0060` / `0x0060` -> `0x00E0` | 已发送/解析未回调 | 无 payload, dst=broadcast | 读取 result(1/2/4B: len=4 读 BE uint32, len=2 读 BE uint16, len=1 读 uint8)，但 callback 注释掉。 |
| 74 | `CommonGetMuteLightEnhancement` | `0xC0061` / `0x0061` -> `0x00E1` | 已发送/已解析 | 无 payload, dst=broadcast | 读取 result(1/2/4B: len=4 读 BE uint32, len=2 读 BE uint16, len=1 读 uint8)；仅 head.src()==2 时 onGetMuteLightEnhancement(result)。 |
| 75 | `CommonSetMuteLightEnhancement` | `0xC0062` / `0x0062` -> `0x00E2` | 已发送/case空 | uint8 value, dst=broadcast | case 空，未读取 payload；无 callback。 |
| 76 | `CommonAudioRecordStart` | `0xC0063` / `0x0063` -> `0x00E3` | 已发送/已解析 | uint16 BE max_time_seconds, dst=broadcast | 至少 8B：support:uint8[0]、audio_type:uint8[1]、sample_rate:uint32 BE[2..5]、channel:uint8[6]、bit_width:uint8[7] -> onAudioRecordStart。 |
| 77 | `CommonAudioRecordStop` | `0xC0064` / `0x0064` -> `0x00E4` | 已发送/已解析 | 无 payload, dst=broadcast | 至少 2B：data_len:uint16 BE[0..1] -> onAudioRecordStopped(data_len)。 |
| 78 | `CommonAudioRecordData` | `0xBFFE5` / `0xFFE5` -> `0x0065` | 仅响应/已解析 | 设备上报/流数据; 主机无发送封装; 注: 0xC0065,下位机错误 | raw payload，长度=head.len() -> onAudioRecordData(data,len)。 |
| 79 | `CommonGetAFCalibration` | `0xC0066` / `0x0066` -> `0x00E6` | 已发送/case空 | 无 payload | case 空，未读取 payload；无 callback。 |
| 80 | `CommonGetDDRCapacity` | `0xC0067` / `0x0067` -> `0x00E7` | 已发送/case空 | 无 payload | case 空，未读取 payload；无 callback。 |
| 81 | `CommonSetPanTiltZoom` | `0xC0068` / `0x0068` -> `0x00E8` | 仅响应/case空 | 源码未找到发送封装; payload 未明确 | case 空，未读取 payload；无 callback。 |
| 82 | `CommonGetPanTiltZoom` | `0xC0069` / `0x0069` -> `0x00E9` | 仅响应/解析未回调 | 源码未找到发送封装; 通常无 payload | 至少 12B：pan:uint32 BE + tilt:uint32 BE + zoom:uint32 BE；callback 注释掉。 |
| 83 | `CommonGetDumpInfo` | `0xC006A` / `0x006A` -> `0x00EA` | 仅响应/解析未回调 | 源码未找到发送封装; 通常无 payload | raw payload，长度=head.len()；onGetDumpInfo 注释掉。 |
| 84 | `CommonSetAlgoEnable` | `0xC006B` / `0x006B` -> `0x00EB` | 仅响应/case空 | 源码未找到发送封装; payload 未明确 | case 空，未读取 payload；无 callback。 |
| 85 | `CommonGetAlgoEnable` | `0xC006C` / `0x006C` -> `0x00EC` | 仅响应/解析未回调 | 源码未找到发送封装; 通常无 payload | 读取 result(1/2/4B: len=4 读 BE uint32, len=2 读 BE uint16, len=1 读 uint8)；onGetAlgoEnable 注释掉。 |
| 86 | `CommonSetHidCall` | `0xC006D` / `0x006D` -> `0x00ED` | 已发送/解析未回调 | uint8 0/1, dst=broadcast | 读取 result(1/2/4B: len=4 读 BE uint32, len=2 读 BE uint16, len=1 读 uint8)；set 回调注释掉。 |
| 87 | `CommonGetHidCall` | `0xC006E` / `0x006E` -> `0x00EE` | 已发送/已解析 | 无 payload, dst=broadcast | 读取 result(1/2/4B: len=4 读 BE uint32, len=2 读 BE uint16, len=1 读 uint8)；result==1 -> Enabled，否则 Disabled；onGetHidCallState。 |
| 88 | `CommonSetUsbSpeedMode` | `0xC006F` / `0x006F` -> `0x00EF` | 已发送/case空 | uint8 value | case 空，未读取 payload；无 callback。 |
| 89 | `CommonGetUsbSpeedMode` | `0xC0070` / `0x0070` -> `0x00F0` | 已发送/已解析 | 无 payload | 读取 result(1/2/4B: len=4 读 BE uint32, len=2 读 BE uint16, len=1 读 uint8) -> onGetUsbSpeedMode(result)。 |
| 90 | `CommonGetBootInfo` | `0xC0071` / `0x0071` -> `0x00F1` | 仅响应/解析未回调 | 源码未找到发送封装; 通常无 payload | 读取 result(1/2/4B: len=4 读 BE uint32, len=2 读 BE uint16, len=1 读 uint8)；onGetBootInfo 注释掉。 |
| 91 | `CommonSetBootErase` | `0xC0072` / `0x0072` -> `0x00F2` | 仅响应/case空 | 源码未找到发送封装; payload 未明确 | case 空，未读取 payload；无 callback。 |
| 92 | `CommonSetPrivacyEnable` | `0xC0073` / `0x0073` -> `0x00F3` | 已发送/case空 | uint32 BE EnableState | case 空，未读取 payload；无 callback。 |
| 93 | `CommonGetPrivacyEnable` | `0xC0074` / `0x0074` -> `0x00F4` | 已发送/已解析 | 无 payload | 读取 result(1/2/4B: len=4 读 BE uint32, len=2 读 BE uint16, len=1 读 uint8) -> onGetPrivacyEnable(EnableState(result))。 |
| 94 | `CommonSetNDIState` | `0xC0101` / `0x0101` -> `0x0181` | 仅响应/case空 | 源码未找到发送封装; payload 未明确 | case 空，未读取 payload；无 callback。 |
| 95 | `CommonGetNDIState` | `0xC0102` / `0x0102` -> `0x0182` | 仅响应/解析未回调 | 源码未找到发送封装; 通常无 payload | 读取 result(1/2/4B: len=4 读 BE uint32, len=2 读 BE uint16, len=1 读 uint8)；onGetNDIState 注释掉。 |
| 96 | `CommonGetIPConfig` | `0xC0103` / `0x0103` -> `0x0183` | 已发送/已解析 | 无 payload | 读取 result(1/2/4B: len=4 读 BE uint32, len=2 读 BE uint16, len=1 读 uint8) -> onGetIPConfig(result)。 |
| 97 | `CommonSetDHCPState` | `0xC0104` / `0x0104` -> `0x0184` | 已发送/case空 | idx:uint8 + dhcp_state:uint8 | case 空，未读取 payload；无 callback。 |
| 98 | `CommonGetDHCPState` | `0xC0105` / `0x0105` -> `0x0185` | 已发送/已解析 | idx:uint8 | 至少 2B：idx:uint8[0] + dhcp_state:uint8[1] -> onGetDHCPState。 |
| 99 | `CommonSetIPAddress` | `0xC0106` / `0x0106` -> `0x0186` | 已发送/case空 | idx:uint8 + ip:uint32 raw memory copy(未 htonl) | case 空，未读取 payload；无 callback。 |
| 100 | `CommonGetIPAddress` | `0xC0107` / `0x0107` -> `0x0187` | 已发送/已解析 | idx:uint8 | 至少 5B：idx:uint8[0] + ip:uint32 raw[1..4]，未 ntohl -> onGetIPAddress。 |
| 101 | `CommonSetNetMask` | `0xC0108` / `0x0108` -> `0x0188` | 已发送/case空 | idx:uint8 + netmask:uint32 raw memory copy(未 htonl) | case 空，未读取 payload；无 callback。 |
| 102 | `CommonGetNetMask` | `0xC0109` / `0x0109` -> `0x0189` | 已发送/已解析 | idx:uint8 | 至少 5B：idx:uint8[0] + netmask:uint32 raw[1..4]，未 ntohl -> onGetNetMask。 |
| 103 | `CommonSetGateway` | `0xC010A` / `0x010A` -> `0x018A` | 已发送/case空 | idx:uint8 + gateway:uint32 raw memory copy(未 htonl) | case 空，未读取 payload；无 callback。 |
| 104 | `CommonGetGateway` | `0xC010B` / `0x010B` -> `0x018B` | 已发送/已解析 | idx:uint8 | 至少 5B：idx:uint8[0] + gateway:uint32 raw[1..4]，未 ntohl -> onGetGateway。 |
| 105 | `CommonSetMacAddress` | `0xC010C` / `0x010C` -> `0x018C` | 已发送/case空 | idx:uint8 + mac:6B(高字节到低字节手工展开) | case 空，未读取 payload；无 callback。 |
| 106 | `CommonGetMacAddress` | `0xC010D` / `0x010D` -> `0x018D` | 已发送/已解析 | idx:uint8 | 至少 7B：idx:uint8[0] + mac:6B[1..6]，memcpy 到 uint64 低 6B -> onGetMacAddress。 |
| 107 | `CommonSetLensCenter` | `0xC010E` / `0x010E` -> `0x018E` | 已发送/case空 | uint32 BE value | case 空，未读取 payload；无 callback。 |
| 108 | `CommonSetConfigJson` | `0xC010F` / `0x010F` -> `0x018F` | 已发送/已解析 | raw JSON bytes, dst 可变 | 读取 result(1/2/4B: len=4 读 BE uint32, len=2 读 BE uint16, len=1 读 uint8) -> onSetConfigJson(result, src)。 |
| 109 | `CommonGetConfigJson` | `0xC0110` / `0x0110` -> `0x0190` | 已发送/已解析 | 无 payload, dst 可变; 支持 sync/async | raw payload，长度=head.len()；async -> onGetConfigJson(data,len,src)，sync -> config_json_.assign(data,len)。 |
| 110 | `CommonSetFbfParamsStart` | `0xC0111` / `0x0111` -> `0x0191` | 已发送/已解析 | JSON {"md5":"...","size":N} | 变长字符串/JSON payload；用 strstr 查找 `"errno":0`，成功后进入 Transferring 并发送首个 data chunk。 |
| 111 | `CommonSetFbfParamsData` | `0xC0112` / `0x0112` -> `0x0192` | 已发送/已解析 | FBF 文件数据 chunk(raw bytes) | 变长字符串/JSON payload；用 strstr 查找 `"errno":0`，成功继续下一 chunk，失败触发 Failed 并 Stop。 |
| 112 | `CommonSetFbfParamsStop` | `0xC0113` / `0x0113` -> `0x0193` | 已发送/已解析 | 无 payload | 变长字符串/JSON payload；用 strstr 查找 `"errno":0`，成功 -> onSetFbfParamState(Success)，否则 Failed。 |
| 113 | `CommonGetFbfParams` | `0xC0114` / `0x0114` -> `0x0194` | 已发送/已解析 | 无 payload, dst 可变 | raw payload，长度=head.len() -> onGetFbfParam(data,len,src)。 |
| 114 | `CommonGetRegionTracking` | `0xC0115` / `0x0115` -> `0x0195` | 仅响应/解析未回调 | 源码未找到发送封装; 通常无 payload | raw payload，长度=head.len()；onGetConfigJson 注释掉。 |
| 115 | `CommonSetRegionTracking` | `0xC0116` / `0x0116` -> `0x0196` | 仅响应/case空 | 源码未找到发送封装; payload 未明确 | case 空，未读取 payload；无 callback。 |
| 116 | `CommonPauseAiAlgThrd` | `0xC0117` / `0x0117` -> `0x0197` | 仅响应/case空 | 源码未找到发送封装; 通常无 payload | case 空，未读取 payload；无 callback。 |
| 117 | `CommonContinueAiAlgThrd` | `0xC0118` / `0x0118` -> `0x0198` | 仅响应/case空 | 源码未找到发送封装; 通常无 payload | case 空，未读取 payload；无 callback。 |
| 118 | `CommonSetNoTargetStrategyState` | `0xC0119` / `0x0119` -> `0x0199` | 仅响应/case空 | 源码未找到发送封装; payload 未明确 | case 空，未读取 payload；无 callback。 |
| 119 | `CommonGetNoTargetStrategyState` | `0xC011A` / `0x011A` -> `0x019A` | 仅响应/解析未回调 | 源码未找到发送封装; 通常无 payload | 读取 result(1/2/4B: len=4 读 BE uint32, len=2 读 BE uint16, len=1 读 uint8)；onGetNoTargetStrategyState 注释掉。 |
| 120 | `CommonSetStartupPosition` | `0xC011B` / `0x011B` -> `0x019B` | 已发送/case空 | uint32 BE EnableState/value | case 空，未读取 payload；无 callback。 |
| 121 | `CommonGetStartupPosition` | `0xC011C` / `0x011C` -> `0x019C` | 已发送/已解析 | 无 payload | 读取 result(1/2/4B: len=4 读 BE uint32, len=2 读 BE uint16, len=1 读 uint8) -> onGetStartupPosition(result)。 |
| 122 | `CommonSetMenuBarLanguage` | `0xC011D` / `0x011D` -> `0x019D` | 仅响应/case空 | 源码未找到发送封装; payload 未明确 | case 空，未读取 payload；无 callback。 |
| 123 | `CommonGetMenuBarLanguage` | `0xC011E` / `0x011E` -> `0x019E` | 仅响应/解析未回调 | 源码未找到发送封装; 通常无 payload | 读取 result(1/2/4B: len=4 读 BE uint32, len=2 读 BE uint16, len=1 读 uint8)；onGetMenuBarLanguage 注释掉。 |
| 124 | `CommonGetRS232TestResult` | `0xC011F` / `0x011F` -> `0x019F` | 已发送/case空 | 无 payload | case 空，未读取 payload；无 callback。 |
| 125 | `CommonTestKey` | `0xC0120` / `0x0120` -> `0x01A0` | 已发送/case空 | uint32 BE flag(0 start,1 get result) | case 空，未读取 payload；无 callback。 |
| 126 | `CommonTestLed` | `0xC0121` / `0x0121` -> `0x01A1` | 已发送/case空 | uint32 BE flag(0 start,1 get result) | case 空，未读取 payload；无 callback。 |
| 127 | `CommonSetDebugJson` | `0xC0122` / `0x0122` -> `0x01A2` | 仅响应/case空 | 源码未找到发送封装; payload 未明确 | case 空，未读取 payload；无 callback。 |
| 128 | `CommonGetDebugJson` | `0xC0123` / `0x0123` -> `0x01A3` | 仅响应/case空 | 源码未找到发送封装; 通常无 payload | case 空，未读取 payload；无 callback。 |
| 129 | `CommonSetViscaUdpPort` | `0xC0124` / `0x0124` -> `0x01A4` | 已发送/case空 | uint32 BE port | case 空，未读取 payload；无 callback。 |
| 130 | `CommonGetViscaUdpPort` | `0xC0125` / `0x0125` -> `0x01A5` | 已发送/已解析 | 无 payload | 读取 result(1/2/4B: len=4 读 BE uint32, len=2 读 BE uint16, len=1 读 uint8) -> onGetViscaUdpPort(result)。 |
| 131 | `CommonSetHorTrackingStrategy` | `0xC0126` / `0x0126` -> `0x01A6` | 仅响应/case空 | 源码未找到发送封装; payload 未明确 | case 空，未读取 payload；无 callback。 |
| 132 | `CommonGetHorTrackingStrategy` | `0xC0127` / `0x0127` -> `0x01A7` | 仅响应/解析未回调 | 源码未找到发送封装; 通常无 payload | 读取 result(1/2/4B: len=4 读 BE uint32, len=2 读 BE uint16, len=1 读 uint8)；onGetHorTrackingStrategy 注释掉。 |
| 133 | `CommonSetVerTrackingStrategy` | `0xC0128` / `0x0128` -> `0x01A8` | 仅响应/case空 | 源码未找到发送封装; payload 未明确 | case 空，未读取 payload；无 callback。 |
| 134 | `CommonGetVerTrackingStrategy` | `0xC0129` / `0x0129` -> `0x01A9` | 仅响应/解析未回调 | 源码未找到发送封装; 通常无 payload | 读取 result(1/2/4B: len=4 读 BE uint32, len=2 读 BE uint16, len=1 读 uint8)；onGetVerTrackingStrategy 注释掉。 |
| 135 | `CommonSetImageStyle` | `0xC0132` / `0x0132` -> `0x01B2` | 仅发送/无接收case | uint32 BE style | 无接收 case；响应字段长度无法从 `message_recv_handler.cpp` 确认。 |
| 136 | `CommonGetImageStyle` | `0xC0133` / `0x0133` -> `0x01B3` | 已发送/已解析 | 无 payload | 读取 result(1/2/4B: len=4 读 BE uint32, len=2 读 BE uint16, len=1 读 uint8) -> onGetImageStyle(result)。 |
| 137 | `CommonSetPiPMode` | `0xC0134` / `0x0134` -> `0x01B4` | 仅发送/无接收case | uint32 BE EnableState/mode | 无接收 case；响应字段长度无法从 `message_recv_handler.cpp` 确认。 |
| 138 | `CommonGetPiPMode` | `0xC0135` / `0x0135` -> `0x01B5` | 已发送/已解析 | 无 payload | 读取 result(1/2/4B: len=4 读 BE uint32, len=2 读 BE uint16, len=1 读 uint8) -> onGetPiPMode(EnableState(result))。 |
| 139 | `CommonSetAudioMuteState` | `0xC0138` / `0x0138` -> `0x01B8` | 仅发送/无接收case | JSON {"mute":0\|1 } | 无接收 case；响应字段长度无法从 `message_recv_handler.cpp` 确认。 |
| 140 | `CommonGetAudioMuteState` | `0xC0139` / `0x0139` -> `0x01B9` | 已发送/已解析 | 无 payload | raw JSON/string，长度=head.len()；用 strstr 查找 `"mute":1` -> onGetAudioMuteState(bool,src)。 |
| 141 | `CommonSetDefaultConfigJson` | `0xC013A` / `0x013A` -> `0x01BA` | 已发送/case空 | 无 payload, dst 可变; 语义为恢复默认配置 | case 空，未读取 payload；无 callback。 |
| 142 | `CommonGetDefaultConfigJson` | `0xC013B` / `0x013B` -> `0x01BB` | 已发送/已解析 | 无 payload, dst 可变 | raw payload，长度=head.len() -> onGetDefaultConfigJson(data,len,src)。 |
| 143 | `CommonSetRTSPStreamURL` | `0xC013C` / `0x013C` -> `0x01BC` | 已发送/case空 | 语义为 URL bytes; 源码实际传 &url 指针地址(疑似 bug) | case 空，未读取 payload；无 callback。 |
| 144 | `CommonGetRTSPStreamURL` | `0xC013D` / `0x013D` -> `0x01BD` | 已发送/已解析 | 无 payload | raw payload，长度=head.len() -> onGetRtspStreamUrl(data,len)。 |
| 145 | `CommonGetLogData` | `0xC013E` / `0x013E` -> `0x01BE` | 仅响应/case空 | 源码未找到发送封装; 通常无 payload | case 空，未读取 payload；无 callback。 |
| 146 | `CommonGetStreamMediaStatus` | `0xC013F` / `0x013F` -> `0x01BF` | 仅响应/case空 | 源码未找到发送封装; 通常无 payload | case 空，未读取 payload；无 callback。 |
| 147 | `CommonTestSDCardState` | `0xC0140` / `0x0140` -> `0x01C0` | 已发送/已解析 | 无 payload | 读取 result(1/2/4B: len=4 读 BE uint32, len=2 读 BE uint16, len=1 读 uint8) -> convertResult(-1 Failed,0 Success,其他 Unknown) -> onSDCardTestState。 |
| 148 | `CommonTestWIFIState` | `0xC0141` / `0x0141` -> `0x01C1` | 已发送/已解析 | uint32 BE TestTaskCommand | 读取 result(1/2/4B: len=4 读 BE uint32, len=2 读 BE uint16, len=1 读 uint8) -> convertResult(-1/0/其他) -> onWifiTestState。 |
| 149 | `CommonTestBluetoothState` | `0xC0142` / `0x0142` -> `0x01C2` | 已发送/已解析 | uint32 BE TestTaskCommand | 读取 result(1/2/4B: len=4 读 BE uint32, len=2 读 BE uint16, len=1 读 uint8) -> convertResult(-1/0/其他) -> onBluetoothTestState。 |
| 150 | `CommonTestBadFlashBlock` | `0xC0143` / `0x0143` -> `0x01C3` | 已发送/已解析 | 无 payload | 读取 result(1/2/4B: len=4 读 BE uint32, len=2 读 BE uint16, len=1 读 uint8) -> convertResult(-1/0/其他) -> onBadFlashBlockTestState。 |
| 151 | `CommonSetVerticalScreenMode` | `0xC0144` / `0x0144` -> `0x01C4` | 已发送/已解析 | uint32 BE EnableState/value | 读取 result(1/2/4B: len=4 读 BE uint32, len=2 读 BE uint16, len=1 读 uint8) -> onSetVerticalScreenMode(EnableState(result))。 |
| 152 | `CommonGetVerticalScreenMode` | `0xC0145` / `0x0145` -> `0x01C5` | 已发送/已解析 | 无 payload | 读取 result(1/2/4B: len=4 读 BE uint32, len=2 读 BE uint16, len=1 读 uint8) -> onGetVerticalScreenMode(result)。 |
| 153 | `CommonSetAutoFocusState` | `0xC0146` / `0x0146` -> `0x01C6` | 已发送/已解析 | uint32 BE EnableState | 读取 result(1/2/4B: len=4 读 BE uint32, len=2 读 BE uint16, len=1 读 uint8) -> onSetAutoFocusState(EnableState(result))。 |
| 154 | `CommonGetAutoFocusState` | `0xC0147` / `0x0147` -> `0x01C7` | 已发送/已解析 | 无 payload | 读取 result(1/2/4B: len=4 读 BE uint32, len=2 读 BE uint16, len=1 读 uint8) -> onGetAutoFocusState(result)。 |
| 155 | `CommonGetBluetoothMacAddr` | `0xC0148` / `0x0148` -> `0x01C8` | 仅响应/case空 | 源码未找到发送封装; 通常无 payload | case 空，未读取 payload；无 callback。 |
| 156 | `CommonGetDereverationAlgParam` | `0xC0149` / `0x0149` -> `0x01C9` | 已发送/已解析 | 无 payload | 读取 result(1/2/4B: len=4 读 BE uint32, len=2 读 BE uint16, len=1 读 uint8) -> onGetDereverationAlgParam(result)。 |
| 157 | `CommonSetDereverationAlgParam` | `0xC014A` / `0x014A` -> `0x01CA` | 仅发送/无接收case | uint32 BE val(注释范围 0..100) | 无接收 case；响应字段长度无法从 `message_recv_handler.cpp` 确认。 |
| 158 | `CommonSetBlueToothBQBMode` | `0xC0154` / `0x0154` -> `0x01D4` | 仅枚举 | 源码未找到发送封装; payload 未明确 | 无接收 case；响应字段长度无法从 `message_recv_handler.cpp` 确认。 |
| 159 | `CommonSetBlueToothRestore` | `0xC0155` / `0x0155` -> `0x01D5` | 仅发送/无接收case | 无 payload | 无接收 case；响应字段长度无法从 `message_recv_handler.cpp` 确认。 |
| 160 | `CommonSetBlueToothName` | `0xC0156` / `0x0156` -> `0x01D6` | 仅枚举 | 源码未找到发送封装; payload 未明确 | 无接收 case；响应字段长度无法从 `message_recv_handler.cpp` 确认。 |
| 161 | `CommonGetBlueToothName` | `0xC0157` / `0x0157` -> `0x01D7` | 仅枚举 | 源码未找到发送封装; 通常无 payload | 无接收 case；响应字段长度无法从 `message_recv_handler.cpp` 确认。 |
| 162 | `CommonGetBatteryCap` | `0xC0158` / `0x0158` -> `0x01D8` | 已发送/已解析 | 无 payload | 读取 result(1/2/4B: len=4 读 BE uint32, len=2 读 BE uint16, len=1 读 uint8) -> onGetBatteryCap(result)。 |
| 163 | `CommonAudioInputDetect` | `0xC0159` / `0x0159` -> `0x01D9` | 已发送/已解析 | uint8 EnableState | raw payload，长度=head.len() -> onAudioInputDetect(data,len)。 |
| 164 | `CommonSetManualFocusPosition` | `0xC015F` / `0x015F` -> `0x01DF` | 已发送/已解析 | uint32 BE position | 读取 result(1/2/4B: len=4 读 BE uint32, len=2 读 BE uint16, len=1 读 uint8) -> onSetManualFocusPosition(EnableState(result))。 |
| 165 | `CommonGetManualFocusPosition` | `0xC0160` / `0x0160` -> `0x01E0` | 已发送/已解析 | 无 payload | 读取 result(1/2/4B: len=4 读 BE uint32, len=2 读 BE uint16, len=1 读 uint8) -> onGetManualFocusPosition(result)。 |
| 166 | `CommonGetExternalSpeakerConfigJson` | `0xC0162` / `0x0162` -> `0x01E2` | 已发送/已解析 | 无 payload, dst 可变 | raw payload，长度=head.len() -> onGetExternalSpeakerConfigJson(data,len,src)。 |
| 167 | `CommonSetExternalSpeakerConfigJson` | `0xC0163` / `0x0163` -> `0x01E3` | 已发送/case空 | raw JSON bytes, dst 可变 | case 内 callback 注释掉；未实际读取 payload。 |
| 168 | `CommonGetConfigJsonSubIndex` | `0xC0164` / `0x0164` -> `0x01E4` | 已发送/已解析 | SubindexHeader(magic=0,type=0x10,len=32,sub_idx[32]), dst=0x4000 | raw payload，长度=head.len() -> onGetConfigJsonSubIndex(data,len,src)。 |
| 169 | `CommonSetConfigJsonSubIndex` | `0xC0165` / `0x0165` -> `0x01E5` | 已发送/case空 | SubindexHeader 前缀 + raw JSON, dst=0x4000 | case 内 callback 注释掉；未实际读取 payload。 |
| 170 | `CommonGetExternalDeviceInfo` | `0xC0166` / `0x0166` -> `0x01E6` | 已发送/已解析 | 无 payload | raw payload，长度=head.len() -> onGetExternalDeviceInfo(data,len)。 |
| 171 | `CommonSetTailWiFiSSID` | `0xCFF36` / `0xFF36` -> `0xFFB6` | 已发送/已解析 | raw SSID bytes; wire cmd=0xFF36 | raw payload，长度=head.len() -> onGetWifi(payload,len)；注意这是 Set 命令响应回调。 |
| 172 | `CommonGetTailWiFiSSID` | `0xC0167` / `0x0167` -> `0x01E7` | 已发送/case空 | 无 payload | case 空，未读取 payload；无 callback。 |
| 173 | `CommonGetTipsStatus` | `0xC0179` / `0x0179` -> `0x01F9` | 已发送/已解析 | 无 payload | 读取 result(1/2/4B: len=4 读 BE uint32, len=2 读 BE uint16, len=1 读 uint8) -> onGetTipsStatus(result)。 |
| 174 | `CommonSetTipsStatus` | `0xC017A` / `0x017A` -> `0x01FA` | 已发送/已解析 | uint32 BE EnableState/value | 读取 result(1/2/4B: len=4 读 BE uint32, len=2 读 BE uint16, len=1 读 uint8) -> onSetTipsStatus(EnableState(result))。 |
| 175 | `CommonGetPositionNumberJson` | `0xC017C` / `0x017C` -> `0x01FC` | 已发送/已解析 | 无 payload, dst 可变 | raw payload，长度=head.len() -> onGetPositionNumberJson(data,len,src)。 |
| 176 | `CommonSetPositionNumberJson` | `0xC017D` / `0x017D` -> `0x01FD` | 仅发送/无接收case | raw JSON bytes, dst 可变 | 无接收 case；响应字段长度无法从 `message_recv_handler.cpp` 确认。 |
| 177 | `CommonGetDanteDevicelic` | `0xC0181` / `0x0181` -> `0x0201` | 已发送/已解析 | 无 payload | raw payload，长度=head.len() -> onGetDanteDevicelic(str,len)。 |
| 178 | `CommonGetDanteManufacturer` | `0xC0182` / `0x0182` -> `0x0202` | 已发送/已解析 | 无 payload | raw payload，长度=head.len() -> onGetDanteManufacturer(str,len)。 |
| 179 | `CommonSetDanteDevicelic` | `0xC0183` / `0x0183` -> `0x0203` | 已发送/已解析 | raw string bytes | 读取 result(1/2/4B: len=4 读 BE uint32, len=2 读 BE uint16, len=1 读 uint8) -> onSetDanteDevicelic(EnableState(result))。 |
| 180 | `CommonSetDanteManufacturer` | `0xC0184` / `0x0184` -> `0x0204` | 已发送/已解析 | raw string bytes | 读取 result(1/2/4B: len=4 读 BE uint32, len=2 读 BE uint16, len=1 读 uint8) -> onSetDanteManufacturer(EnableState(result))。 |
| 181 | `CommonSetConfigJsonBySn` | `0xC0186` / `0x0186` -> `0x0206` | 已发送/已解析 | raw JSON/config bytes | 读取 result(1/2/4B: len=4 读 BE uint32, len=2 读 BE uint16, len=1 读 uint8) -> onSetConfigJsonBySn(result)。 |
| 182 | `CommonGetConfigJsonBySn` | `0xC0187` / `0x0187` -> `0x0207` | 已发送/已解析 | raw selector bytes(通常含 SN/条件) | raw payload，长度=head.len() -> onGetConfigJsonBySn(data,len)。 |
| 183 | `CommonGetSightAngle` | `0xC0190` / `0x0190` -> `0x0210` | 已发送/已解析 | 无 payload | 读取 result(1/2/4B: len=4 读 BE uint32, len=2 读 BE uint16, len=1 读 uint8) -> onGetSightAngle(result)。 |
| 184 | `CommonSetSightAngle` | `0xC0191` / `0x0191` -> `0x0211` | 已发送/已解析 | uint32 BE angle | 读取 result(1/2/4B: len=4 读 BE uint32, len=2 读 BE uint16, len=1 读 uint8) -> onSetSightAngle(EnableState(result))。 |
| 185 | `CommonGetAllChannelScanState` | `0xC0192` / `0x0192` -> `0x0212` | 已发送/已解析 | 无 payload | 读取 result(1/2/4B: len=4 读 BE uint32, len=2 读 BE uint16, len=1 读 uint8) -> onGetAllChannelScanState(result)。 |
| 186 | `CommonSetAllChannelScanState` | `0xC0193` / `0x0193` -> `0x0213` | 已发送/已解析 | uint32 BE EnableState | 读取 result(1/2/4B: len=4 读 BE uint32, len=2 读 BE uint16, len=1 读 uint8) -> onSetAllChannelScanState(EnableState(result))。 |
| 187 | `CommonAudioBeamReport` | `0xC0201` / `0x0201` -> `0x0281` | 已发送/已解析 | uint8 EnableState; 响应体为 beam 上报 | 至少 4B：device_type:uint8[0] + device_index:uint8[1] + beam_index:uint8[2] + is_speech:uint8[3] -> onAudioBeamReport。 |
| 188 | `CommonSetFrameRateSwitch` | `0xC0202` / `0x0202` -> `0x0282` | 已发送/case空 | uint32 BE EnableState | case 空，未读取 payload；无 callback。 |
| 189 | `CommonGetFrameRateSwitch` | `0xC0203` / `0x0203` -> `0x0283` | 已发送/已解析 | 无 payload | 读取 result(1/2/4B: len=4 读 BE uint32, len=2 读 BE uint16, len=1 读 uint8) -> onGetFrameRateSwitch(result)。 |
| 190 | `CommonSetAutoShutDown` | `0xC0205` / `0x0205` -> `0x0285` | 已发送/已解析 | uint32 BE state | 读取 result(1/2/4B: len=4 读 BE uint32, len=2 读 BE uint16, len=1 读 uint8) -> onSetAutoShutDown(EnableState(result))。 |
| 191 | `CommonGetAutoShutDown` | `0xC0206` / `0x0206` -> `0x0286` | 已发送/已解析 | 无 payload | 读取 result(1/2/4B: len=4 读 BE uint32, len=2 读 BE uint16, len=1 读 uint8) -> onGetAutoShutDown(result)。 |
| 192 | `CommonSetDefaultVolume` | `0xC0207` / `0x0207` -> `0x0287` | 已发送/已解析 | uint32 BE gain/volume | 读取 result(1/2/4B: len=4 读 BE uint32, len=2 读 BE uint16, len=1 读 uint8) -> onSetDefaultVolume(EnableState(result))。 |
| 193 | `CommonGetDefaultVolume` | `0xC0208` / `0x0208` -> `0x0288` | 已发送/已解析 | 无 payload | 读取 result(1/2/4B: len=4 读 BE uint32, len=2 读 BE uint16, len=1 读 uint8) -> onGetDefaultVolume(result)。 |
| 194 | `CommonGetAudioEqParam` | `0xC0209` / `0x0209` -> `0x0289` | 已发送/已解析 | 无 payload | 读取 result(1/2/4B: len=4 读 BE uint32, len=2 读 BE uint16, len=1 读 uint8) -> ongetAudioEqParam(result)。 |
| 195 | `CommonSetAudioEqParam` | `0xC020A` / `0x020A` -> `0x028A` | 已发送/已解析 | uint32 BE eq packed value(协议文档示例曾按小端解释, 与实现存在差异) | 读取 result(1/2/4B: len=4 读 BE uint32, len=2 读 BE uint16, len=1 读 uint8) -> onSetAudioEqParam(EnableState(result))。 |
| 196 | `CommonSetAudioEqMode` | `0xC020B` / `0x020B` -> `0x028B` | 已发送/已解析 | uint32 BE mode | 读取 result(1/2/4B: len=4 读 BE uint32, len=2 读 BE uint16, len=1 读 uint8) -> onSetAudioEqMode(EnableState(result))。 |
| 197 | `CommonGetAudioEqMode` | `0xC020C` / `0x020C` -> `0x028C` | 已发送/已解析 | 无 payload | 读取 result(1/2/4B: len=4 读 BE uint32, len=2 读 BE uint16, len=1 读 uint8) -> ongetAudioEqMode(result)。 |
| 198 | `CommonSetWatermark` | `0xC020D` / `0x020D` -> `0x028D` | 仅发送/无接收case | uint32 BE EnableState | 无接收 case；响应字段长度无法从 `message_recv_handler.cpp` 确认。 |
| 199 | `CommonGetWatermark` | `0xC020E` / `0x020E` -> `0x028E` | 已发送/已解析 | 无 payload | 读取 result(1/2/4B: len=4 读 BE uint32, len=2 读 BE uint16, len=1 读 uint8) -> onGetWatermark(EnableState(result))。 |
| 200 | `CommonSetComboKey` | `0xC020F` / `0x020F` -> `0x028F` | 已发送/已解析 | uint32 BE EnableState/value | 读取 result(1/2/4B: len=4 读 BE uint32, len=2 读 BE uint16, len=1 读 uint8) -> onSetComboKey(EnableState(result))。 |
| 201 | `CommonGetComboKey` | `0xC0210` / `0x0210` -> `0x0290` | 已发送/已解析 | 无 payload | 读取 result(1/2/4B: len=4 读 BE uint32, len=2 读 BE uint16, len=1 读 uint8) -> onGetComboKey(result)。 |

## HID 公共报告补充

| Report | 方向 | 输入 | 输出/事件 |
|---|---|---|---|
| `0x01` IN | 设备 -> 主机 | `buffer[1]` bit0 hookswitch, bit1 linebusy, bit2 mute, bit3 flash | `Answer`, `HangUp`, `Mute`, `Unmute` |
| `0x02` IN | 设备 -> 主机 | `buffer[1]=0x01/0x02` | `VolUp` / `VolDown` |
| `0x03` OUT | 主机 -> 设备 | `buffer[1]` bit0 offhook LED, bit2 mute LED | 无 AXDP 响应, 通过 `sendRaw` 写 HID report |

## 需要特别确认的实现差异

- `CommonAudioRecordData` 的枚举值是 `0xBFFE5`, 注释写明原应为 `0xC0065` 且“下位机错误”; 因此响应 wire cmd 会表现为 `0x0065`。
- `CommonSetTailWiFiSSID` 的完整 ID 是 `0xCFF36`, wire cmd 为 `0xFF36`, 不在常规 `0x01xx/0x02xx` 区段；接收侧把 Set 的响应 payload 当成 Wi-Fi SSID 回调给 `onGetWifi`。
- `CommonSetRTSPStreamURL` 与 `CommonSetAlgAuthContent` 源码传的是 `&url` / `&auth_content` 指针地址, 语义上应为字符串 bytes, 适配时需要按设备真实行为复核。
- 网络 IP/netmask/gateway 设置和获取都没有在接收侧 `ntohl`; 当前实现按本机原始 uint32 解释，跨平台或新协议迁移时需明确字节序。
- EQ 参数在 `AXDP_Protocol.md` 示例里按小端解释, 但当前发送实现用 `utils::htonl`; 接收侧又走 `result(1/2/4B)`，需要用真机或抓包确认最终约定。
- 多数 `Set*` 响应 case 只占位或无接收 case；新协议适配时建议统一补齐 ACK/result/event 三类响应模型。
