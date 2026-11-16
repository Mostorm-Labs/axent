# AXTP Legacy Migration Plan (Generated)

## Summary

This generated plan migrates AXDP HID and signage SDK protocol material into an AXTP v1 compatible compatibility layer. It does not modify AXTP Core. Registry changes are emitted as candidate patches only.

## Source Coverage

- Source protocols: axdp_hid, signage_sdk
- Total legacy mappings: 266
- RPC method mappings: 252
- Event mappings: 4
- Capability mappings: 0
- STREAM mappings: 10
- Existing AXTP registry entries reused: 3

## Invariants

- AXTP v1 Standard Frame Header remains unchanged.
- PayloadType remains CONTROL=0x01, RPC=0x02, STREAM=0x03.
- Legacy compatibility is implemented by adapter code and generated mapping tables, not by AXTP Core changes.
- Bulk or continuous legacy data is mapped to STREAM. RPC is used only for setup, control and finalization.

## Migration Matrix

| Legacy source | Legacy object | Destination | Promotion | Notes |
|---|---|---|---|---|
| `axdp_hid` | `CommonSetExposureCompensationParam` / `0XC012A` | `rpc_method:legacy.setExposureCompensationParam` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonGetExposureCompensationParam` / `0XC012B` | `rpc_method:legacy.setExposureCompensationParam` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonSetBacklightCompensationParam` / `0XC012C` | `rpc_method:legacy.setBacklightCompensationParam` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonGetBacklightCompensationParam` / `0XC012D` | `rpc_method:legacy.getBacklightCompensationParam` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonSetHighlightSuppressionParam` / `0XC012E` | `rpc_method:legacy.setHighlightSuppressionParam` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonGetHighlightSuppressionParam` / `0XC012F` | `rpc_method:legacy.getHighlightSuppressionParam` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonSetShutterParam` / `0XC0130` | `rpc_method:legacy.setShutterParam` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonGetShutterParam` / `0XC0131` | `rpc_method:legacy.getShutterParam` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonSetImageStyle` / `0XC0132` | `rpc_method:video.setImageStyle` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonGetImageStyle` / `0XC0133` | `rpc_method:video.getImageStyle` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonSetPiPMode` / `0XC0134` | `rpc_method:video.setPiPMode` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonGetPiPMode` / `0XC0135` | `rpc_method:video.getPiPMode` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonSetROIDisplayState` / `0XC0136` | `rpc_method:display.setRoiDisplayState` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonGetROIDisplayState` / `0XC0137` | `rpc_method:display.getRoiDisplayState` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonSetAudioMuteState` / `0XC0138` | `rpc_method:audio.setAudioMuteState` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonGetAudioMuteState` / `0XC0139` | `rpc_method:audio.getAudioMuteState` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonSetDefaultConfigJson` / `0XC013A` | `rpc_method:legacy.setDefaultConfigJson` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonGetDefaultConfigJson` / `0XC013B` | `rpc_method:legacy.getDefaultConfigJson` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonSetRTSPStreamAddr` / `0XC013C` | `stream:video.setRtspStreamAddr` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonGetRTSPStreamAddr` / `0XC013D` | `stream:video.getRtspStreamAddr` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonGetLogData` / `0XC013E` | `stream:log.getLogData` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonGetMediaStatus` / `0XC013F` | `rpc_method:device.getMediaStatus` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonTestSDCardState` / `0XC0140` | `rpc_method:storage.testSdCardState` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonTestWIFIState` / `0XC0141` | `rpc_method:network.testWifiState` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonTestBluetoothState` / `0XC0142` | `rpc_method:network.testBluetoothState` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonTestBadFlashBlock` / `0XC0143` | `rpc_method:device.testBadFlashBlock` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonSetVerticalScreenMode` / `0XC0144` | `rpc_method:display.setVerticalScreenMode` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonGetVerticalScreenMode` / `0XC0145` | `rpc_method:display.getVerticalScreenMode` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonSetAutoFocusState` / `0XC0146` | `rpc_method:legacy.setAutoFocusState` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonGetAutoFocusState` / `0XC0147` | `rpc_method:legacy.getAutoFocusState` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonGetBluetoothMacAddr` / `0XC0148` | `rpc_method:network.getBluetoothMacAddr` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `BetaDeviceInfo` / `0x000B0002` | `rpc_method:device.getInfo` | `existing_registry_mapping` | Authoritative existing mapping retained. |
| `axdp_hid` | `BetaBrightnessSet` / `0x000B0042` | `rpc_method:display.setBrightness` | `existing_registry_mapping` | Authoritative existing mapping retained. |
| `axdp_hid` | `AlphaUpgradeInfo` / `0xA0001` | `rpc_method:firmware.startUpgrade` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `AlphaUpgradeData` / `0xA0002` | `stream:firmware.alphaUpgradeData` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `AlphaDeviceInfo` / `0xA0003` | `rpc_method:device.getDeviceInfo` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `AlphaDeviceType` / `0xA0004` | `rpc_method:device.alphaDeviceType` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonMultiChannelAudioRecordStart` / `0xA200063` | `rpc_method:audio.commonMultiChannelAudioRecordStart` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonMultiChannelAudioRecordStop` / `0xA200064` | `rpc_method:audio.commonMultiChannelAudioRecordStop` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonMultiChannelAudioRecordData` / `0xA200065` | `stream:audio.commonMultiChannelAudioRecordData` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonSetDebugJson` / `0xA500115` | `rpc_method:legacy.commonSetDebugJson` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonGetDebugJson` / `0xA500116` | `rpc_method:legacy.commonGetDebugJson` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `BetaDeviceReset` / `0xB0001` | `rpc_method:system.resetDevice` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `BetaDeviceInfo` / `0xB0002` | `rpc_method:device.getDeviceInfoSyncGetDeviceInfo` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `BetaStartUpgrade` / `0xB0003` | `rpc_method:firmware.startUpgrade` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `BetaStopUpgrade` / `0xB0004` | `rpc_method:firmware.betaStopUpgrade` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `BetaUpgradeInfo` / `0xB0005` | `rpc_method:firmware.betaUpgradeInfo` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `BetaUpgradeData` / `0xB0006` | `stream:firmware.betaUpgradeData` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `BetaUpgradeInfoEx` / `0xB0011` | `rpc_method:firmware.betaUpgradeInfoEx` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `BetaUpgradeDataEx` / `0xB0012` | `stream:firmware.betaUpgradeDataEx` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonSetVideoMode` / `0xC0021` | `rpc_method:audio.setVideoMode` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonGetVideoMode` / `0xC0022` | `rpc_method:video.getVideoMode` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonGetPeopleNumber` / `0xC0023` | `rpc_method:legacy.getPeopleNumber` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonSetMicUsed` / `0xC0024` | `rpc_method:audio.setMicUsed` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonAudioRecord` / `0xC0025` | `rpc_method:audio.startAudioRecord` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonSetTestResult` / `0xC0026` | `rpc_method:diagnostic.setTestResult` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonGetTestResult` / `0xC0027` | `rpc_method:diagnostic.getTestResult` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonSetEncryptedInfo` / `0xC0028` | `rpc_method:device.setEncryptedInfo` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonGetEncryptedInfo` / `0xC0029` | `rpc_method:device.getEncryptedInfo` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonGetGyroSlopeAngle` / `0xC002A` | `rpc_method:legacy.getGyroSlopeAngle` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonSetUacState` / `0xC002B` | `rpc_method:audio.setUacState` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonGetUacState` / `0xC002C` | `rpc_method:audio.getUacState` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonTestAudioConsistency` / `0xC002D` | `rpc_method:audio.testAudioConsistency` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonTestNetworkPort` / `0xC002E` | `rpc_method:network.testNetworkPort` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonSetSpeakerTrackMode` / `0xC0031` | `rpc_method:audio.setSpeakerTrackMode` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonGetSpeakerTrackMode` / `0xC0032` | `rpc_method:audio.getSpeakerTrackMode` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonSetMirrorState` / `0xC0033` | `rpc_method:legacy.setMirrorState` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonGetMirrorState` / `0xC0034` | `rpc_method:legacy.getMirrorState` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonSetSpeakerTrackDelay` / `0xC0035` | `rpc_method:audio.setSpeakerTrackDelay` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonGetSpeakerTrackDelay` / `0xC0036` | `rpc_method:audio.getSpeakerTrackDelay` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonSetSplitScreenNumber` / `0xC0037` | `rpc_method:display.setSplitScreenNumber` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonGetSplitScreenNumber` / `0xC0038` | `rpc_method:display.getSplitScreenNumber` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonSetUsbName` / `0xC0039` | `rpc_method:device.setUsbName` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonGetUsbName` / `0xC003A` | `rpc_method:device.getUsbName` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonSetUsbPid` / `0xC003B` | `rpc_method:legacy.setUsbPid` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonGetUsbPid` / `0xC003C` | `rpc_method:legacy.getUsbPid` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonSetUsbVid` / `0xC003D` | `rpc_method:legacy.setUsbVid` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonGetUsbVid` / `0xC003E` | `rpc_method:legacy.getUsbVid` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonSetReboot` / `0xC003F` | `rpc_method:system.setReboot` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonSetPowerLineFreq` / `0xC0040` | `rpc_method:legacy.setPowerLineFreq` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonGetPowerLineFreq` / `0xC0041` | `rpc_method:legacy.getPowerLineFreq` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonGetDeviceUniqueId` / `0xC0042` | `rpc_method:device.getDeviceUniqueId` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonSetDeviceUniqueId` / `0xC0043` | `rpc_method:device.setDeviceUniqueId` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonGetAlgAuthContent` / `0xC0044` | `rpc_method:auth.getAlgAuthContent` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonSetAlgAuthContent` / `0xC0045` | `rpc_method:auth.setAlgAuthContent` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonSetWdrState` / `0xC0046` | `rpc_method:legacy.setWdrState` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonGetWdrState` / `0xC0047` | `rpc_method:legacy.getWdrState` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonSetOsdMirrorState` / `0xC0048` | `rpc_method:video.setOsdMirrorState` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonGetOsdMirrorState` / `0xC0049` | `rpc_method:video.getOsdMirrorState` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonSetAFCalibration` / `0xC004A` | `rpc_method:legacy.setAfCalibration` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonStartAudioTest` / `0xC004B` | `rpc_method:audio.startAudioTest` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonSetFlipState` / `0xC004C` | `rpc_method:network.setFlipState` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonGetFlipState` / `0xC004D` | `rpc_method:network.getFlipState` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonGetNoiseEliminationLevel` / `0xC004E` | `rpc_method:legacy.getNoiseEliminationLevel` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonSetNoiseEliminationLevel` / `0xC004F` | `rpc_method:legacy.setNoiseEliminationLevel` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonGetBootDetect` / `0xC0050` | `rpc_method:legacy.getBootDetect` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonGetReverberationEliminationLevel` / `0xC0051` | `rpc_method:legacy.getReverberationEliminationLevel` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonSetReverberationEliminationLevel` / `0xC0052` | `rpc_method:legacy.setReverberationEliminationLevel` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonGetEchoEliminationLevel` / `0xC0053` | `rpc_method:legacy.getEchoEliminationLevel` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonSetEchoEliminationLevel` / `0xC0054` | `rpc_method:legacy.setEchoEliminationLevel` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonGetRecordEqParams` / `0xC0055` | `rpc_method:storage.getRecordEqParams` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonSetRecordEqParams` / `0xC0056` | `rpc_method:storage.setRecordEqParams` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonGetDefaultNoiseEliminationLevel` / `0xC0057` | `rpc_method:legacy.getDefaultNoiseEliminationLevel` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonSetDefaultNoiseEliminationLevel` / `0xC0058` | `rpc_method:legacy.setDefaultNoiseEliminationLevel` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonGetDefaultReverberationEliminationLevel` / `0xC0059` | `rpc_method:legacy.getDefaultReverberationEliminationLevel` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonSetDefaultReverberationEliminationLevel` / `0xC005A` | `rpc_method:legacy.setDefaultReverberationEliminationLevel` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonGetDefaultEchoEliminationLevel` / `0xC005B` | `rpc_method:legacy.getDefaultEchoEliminationLevel` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonSetDefaultEchoEliminationLevel` / `0xC005C` | `rpc_method:legacy.setDefaultEchoEliminationLevel` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonGetDefaultRecordEqParams` / `0xC005D` | `rpc_method:storage.getDefaultRecordEqParams` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonSetDefaultRecordEqParams` / `0xC005F` | `rpc_method:storage.setDefaultRecordEqParams` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonSetAlgoParamsReset` / `0xC0060` | `rpc_method:system.setAlgoParamsReset` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonGetMuteLightEnhancement` / `0xC0061` | `rpc_method:display.getMuteLightEnhancement` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonSetMuteLightEnhancement` / `0xC0062` | `rpc_method:display.setMuteLightEnhancement` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonAudioRecordStart` / `0xC0063` | `rpc_method:audio.audioRecordStart` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonAudioRecordStop` / `0xC0064` | `rpc_method:audio.audioRecordStop` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonAudioRecordData` / `0xC0065` | `stream:audio.onAudioRecordData` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonGetAFCalibration` / `0xC0066` | `rpc_method:legacy.getAfCalibration` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonGetDDRCapacity` / `0xC0067` | `rpc_method:network.getDdrCapacity` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonSetPanTiltZoom` / `0xC0068` | `rpc_method:video.setPanTiltZoom` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonGetPanTiltZoom` / `0xC0069` | `rpc_method:video.getPanTiltZoom` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonGetDumpInfo` / `0xC006A` | `rpc_method:device.getDumpInfo` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonSetAlgoEnable` / `0xC006B` | `rpc_method:legacy.setAlgoEnable` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonGetAlgoEnable` / `0xC006C` | `rpc_method:legacy.getAlgoEnable` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonSetHidCall` / `0xC006D` | `rpc_method:input.setHidCall` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonGetHidCall` / `0xC006E` | `rpc_method:input.getHidCall` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonSetUsbSpeedMode` / `0xC006F` | `rpc_method:legacy.setUsbSpeedMode` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonGetUsbSpeedMode` / `0xC0070` | `rpc_method:legacy.getUsbSpeedMode` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonGetBootInfo` / `0xC0071` | `rpc_method:device.getBootInfo` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonSetBootErase` / `0xC0072` | `rpc_method:legacy.setBootErase` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonSetPrivacyEnable` / `0xC0073` | `rpc_method:legacy.setPrivacyEnable` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonGetPrivacyEnable` / `0xC0074` | `rpc_method:legacy.getPrivacyEnable` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonSetNDIState` / `0xC0101` | `rpc_method:video.setNdiState` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonGetNDIState` / `0xC0102` | `rpc_method:video.getNdiState` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonGetIPConfig` / `0xC0103` | `rpc_method:network.getIpConfig` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonSetDHCPState` / `0xC0104` | `rpc_method:legacy.setDhcpState` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonGetDHCPState` / `0xC0105` | `rpc_method:legacy.getDhcpState` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonSetIPAddress` / `0xC0106` | `rpc_method:network.getIpAddress` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonGetIPAddress` / `0xC0107` | `rpc_method:network.getIpAddress` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonSetNetMask` / `0xC0108` | `rpc_method:network.setNetMask` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonGetNetMask` / `0xC0109` | `rpc_method:network.getNetMask` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonSetGateway` / `0xC010A` | `rpc_method:legacy.setGateway` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonGetGateway` / `0xC010B` | `rpc_method:legacy.getGateway` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonSetMacAddress` / `0xC010C` | `rpc_method:legacy.setMacAddress` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonGetMacAddress` / `0xC010D` | `rpc_method:legacy.getMacAddress` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonSetLensCenter` / `0xC010E` | `rpc_method:legacy.setLensCenter` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonSetConfigJson` / `0xC010F` | `rpc_method:device.setConfigJson` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonGetConfigJson` / `0xC0110` | `rpc_method:device.getConfigJson` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonSetFbfParamsStart` / `0xC0111` | `rpc_method:legacy.setFbfParamsStart` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonSetFbfParamsData` / `0xC0112` | `stream:storage.setFbfParamsData` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonSetFbfParamsStop` / `0xC0113` | `rpc_method:legacy.setFbfParamsStop` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonGetFbfParams` / `0xC0114` | `rpc_method:legacy.getFbfParams` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonGetRegionTracking` / `0xC0115` | `rpc_method:legacy.getRegionTracking` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonSetRegionTracking` / `0xC0116` | `rpc_method:legacy.setRegionTracking` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonPauseAiAlgThrd` / `0xC0117` | `rpc_method:legacy.pauseAiAlgThrd` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonContinueAiAlgThrd` / `0xC0118` | `rpc_method:legacy.continueAiAlgThrd` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonSetNoTargetStrategyState` / `0xC0119` | `rpc_method:firmware.setNoTargetStrategyState` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonGetNoTargetStrategyState` / `0xC011A` | `rpc_method:firmware.getNoTargetStrategyState` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonSetStartupPosition` / `0xC011B` | `rpc_method:legacy.setStartupPosition` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonGetStartupPosition` / `0xC011C` | `rpc_method:legacy.getStartupPosition` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonSetMenuBarLanguage` / `0xC011D` | `rpc_method:display.setMenuBarLanguage` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonGetMenuBarLanguage` / `0xC011E` | `rpc_method:display.getMenuBarLanguage` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonGetRS232TestResult` / `0xC011F` | `rpc_method:diagnostic.getRS232TestResult` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonTestKey` / `0xC0120` | `rpc_method:input.testKey` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonTestLed` / `0xC0121` | `rpc_method:diagnostic.testLed` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonSetDebugJson` / `0xC0122` | `rpc_method:legacy.setDebugJson` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonGetDebugJson` / `0xC0123` | `rpc_method:legacy.getDebugJson` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonSetViscaUdpPort` / `0xC0124` | `rpc_method:network.setViscaUdpPort` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonGetViscaUdpPort` / `0xC0125` | `rpc_method:network.getViscaUdpPort` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonSetHorTrackingStrategy` / `0xC0126` | `rpc_method:legacy.setHorTrackingStrategy` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonGetHorTrackingStrategy` / `0xC0127` | `rpc_method:legacy.getHorTrackingStrategy` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonSetVerTrackingStrategy` / `0xC0128` | `rpc_method:legacy.setVerTrackingStrategy` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonGetVerTrackingStrategy` / `0xC0129` | `rpc_method:legacy.getVerTrackingStrategy` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonGetReverberationSuppressionParam` / `0xC0149` | `rpc_method:legacy.getReverberationSuppressionParam` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonSetReverberationSuppressionParam` / `0xC014A` | `rpc_method:legacy.setReverberationSuppressionParam` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonSetManualFocusStatus` / `0xC014B` | `rpc_method:legacy.setManualFocusStatus` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonGetManualFocusStatus` / `0xC014C` | `rpc_method:legacy.getManualFocusStatus` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonGetWifiSignalIntensity` / `0xC014D` | `rpc_method:network.getWifiSignalIntensity` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonSetCropCircleX` / `0xC014F` | `rpc_method:diagnostic.setCropCircleX` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonSetCropCircleY` / `0xC0150` | `rpc_method:diagnostic.setCropCircleY` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonSetAudioEnergyThreshold` / `0xC0151` | `rpc_method:audio.setAudioEnergyThreshold` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonGetAudioEnergy` / `0xC0152` | `rpc_method:audio.getAudioEnergy` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonSetDeviceLoaderState` / `0xC0153` | `rpc_method:device.setDeviceLoaderState` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonSetBTBQBMODE` / `0xC0154` | `rpc_method:network.setBtbqbmode` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonSetBTRestore` / `0xC0155` | `rpc_method:network.setBtRestore` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonSetBTName` / `0xC0156` | `rpc_method:network.setBtName` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonGetBTName` / `0xC0157` | `rpc_method:network.getBtName` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonGetBatteryCap` / `0xC0158` | `rpc_method:network.getBatteryCap` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonAudioInputDetect` / `0xC0159` | `rpc_method:audio.audioInputDetect` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonSetPairBluetoothMacAddr` / `0xC015A` | `rpc_method:network.setPairBluetoothMacAddr` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonGetPairBluetoothMacAddr` / `0xC015B` | `rpc_method:network.getPairBluetoothMacAddr` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonSetRadius` / `0xC015C` | `rpc_method:diagnostic.setRadius` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonSNErase` / `0xC015D` | `rpc_method:legacy.eraseSn` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonGetCpuUniqueId` / `0xC015E` | `rpc_method:device.getCpuUniqueId` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonSetManualFocusPosition` / `0xC015F` | `rpc_method:legacy.setManualFocusPosition` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonGetManualFocusPosition` / `0xC0160` | `rpc_method:legacy.getManualFocusPosition` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonPTZinfoErase` / `0xC0161` | `rpc_method:device.erasePtZinfoErase` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonGetExternalSpeakerConfigJson` / `0xC0162` | `rpc_method:audio.getExternalSpeakerConfigJson` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonSetExternalSpeakerConfigJson` / `0xC0163` | `rpc_method:audio.setExternalSpeakerConfigJson` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonGetConfigJsonSubIndex` / `0xC0164` | `rpc_method:legacy.getConfigJsonSubIndex` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonSetConfigJsonSubIndex` / `0xC0165` | `rpc_method:legacy.setConfigJsonSubIndex` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonGetExternalDeviceInfo` / `0xC0166` | `rpc_method:device.getExternalDeviceInfo` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonSetExternalDeviceInfo` / `0xC0166` | `rpc_method:device.setExternalDeviceInfo` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonGetDefaultMuteKeyStatus` / `0xC0175` | `rpc_method:input.getDefaultMuteKeyStatus` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonSetDefaultMuteKeyStatus` / `0xC0176` | `rpc_method:input.setDefaultMuteKeyStatus` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonTransferJrpcData` / `0xC0178` | `rpc_method:legacy.jsonrpcCall` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonGetTripsStatus` / `0xC0179` | `rpc_method:network.getTipsStatus` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonSetTipsStatus` / `0xC017A` | `rpc_method:network.setTipsStatus` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonStartTestKVM` / `0xC017B` | `stream:device.startTestKvm` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonGetPositionNumberJson` / `0xC017C` | `rpc_method:legacy.getPositionNumberJson` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonSetPositionNumberJson` / `0xC017D` | `rpc_method:legacy.setPositionNumberJson` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonGetAudioTypeVol` / `0xC017E` | `rpc_method:audio.getAudioTypeVol` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonSetAudioDoaConfig` / `0xC017F` | `rpc_method:audio.commonSetAudioDoaConfig` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonGetAudioDoaConfig` / `0xC0180` | `rpc_method:audio.commonGetAudioDoaConfig` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonGetDanteDevicelic` / `0xC0181` | `rpc_method:audio.commonGetDanteDevicelic` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonGetDanteManufacturer` / `0xC0182` | `rpc_method:audio.commonGetDanteManufacturer` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonSetDanteDevicelic` / `0xC0183` | `rpc_method:audio.commonSetDanteDevicelic` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonSetDanteManufacturer` / `0xC0184` | `rpc_method:audio.commonSetDanteManufacturer` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonQueryRecordResponeStatus` / `0xC0185` | `rpc_method:audio.commonQueryRecordResponeStatus` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonSetConfigJsonBySn` / `0xC0186` | `rpc_method:device.commonSetConfigJsonBySn` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonGetConfigJsonBySn` / `0xC0187` | `rpc_method:device.commonGetConfigJsonBySn` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonSetVM33state` / `0xC0188` | `rpc_method:legacy.commonSetVM33state` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonProductTestJsonByParam` / `0xC0189` | `rpc_method:diagnostic.commonProductTestJsonByParam` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonGetSightAngle` / `0xC0190` | `rpc_method:device.commonGetSightAngle` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonSetSightAngle` / `0xC0191` | `rpc_method:device.commonSetSightAngle` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonGetAllChannelScanState` / `0xC0192` | `rpc_method:legacy.commonGetAllChannelScanState` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonSetAllChannelScanState` / `0xC0193` | `rpc_method:legacy.commonSetAllChannelScanState` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonTestAging` / `0xC0194` | `rpc_method:device.commonTestAging` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonGetScreenEntitlementId` / `0xC0195` | `rpc_method:display.commonGetScreenEntitlementId` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonSetScreenEntitlementId` / `0xC0196` | `rpc_method:display.commonSetScreenEntitlementId` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonAudioBeamReport` / `0xC0201` | `rpc_method:audio.audioBeamReport` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonSetTipsPlay` / `0xC20006B` | `rpc_method:network.setTipsPlay` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `CommonGetTipsPlay` / `0xC20006C` | `rpc_method:network.getTipsPlay` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `axdp_hid` | `UnknownCode` / `0xCFFFF` | `rpc_method:legacy.unknownCode` | `candidate_registry_patch` | Source file: current AXDP capability protocol set |
| `signage_sdk` | `FormatSd` / `NearHub-Launcher数字标牌设备管理通用管理命令:FormatSd` | `rpc_method:storage.formatSd` | `candidate_registry_patch` | Source status: 未研发 |
| `signage_sdk` | `GetAppearanceConfig` / `NearHub-Launcher数字标牌设备管理通用管理命令:GetAppearanceConfig` | `rpc_method:appearance.getConfig` | `candidate_registry_patch` | Source status: 未研发 |
| `signage_sdk` | `GetBindCode` / `NearHub-Launcher数字标牌设备管理通用管理命令:GetBindCode` | `rpc_method:binding.getCode` | `candidate_registry_patch` | Source status: 研发中 |
| `signage_sdk` | `GetBindConfig` / `NearHub-Launcher数字标牌设备管理通用管理命令:GetBindConfig` | `rpc_method:binding.getConfig` | `candidate_registry_patch` | Source status: 未研发 |
| `signage_sdk` | `GetDeviceInfo` / `NearHub-Launcher数字标牌设备管理通用管理命令:GetDeviceInfo` | `rpc_method:device.getInfo` | `candidate_registry_patch` | Source status: 已研发 |
| `signage_sdk` | `GetLineInPreGain` / `NearHub-Launcher数字标牌设备管理通用管理命令:GetLineInPreGain` | `rpc_method:audio.getLineInPreGain` | `candidate_registry_patch` | Source status: 未研发 |
| `signage_sdk` | `GetLineOutVolume` / `NearHub-Launcher数字标牌设备管理通用管理命令:GetLineOutVolume` | `rpc_method:audio.getLineOutVolume` | `candidate_registry_patch` | Source status: 未研发 |
| `signage_sdk` | `GetNetworkInfo` / `NearHub-Launcher数字标牌设备管理通用管理命令:GetNetworkInfo` | `rpc_method:network.getInfo` | `candidate_registry_patch` | Source status: 未研发 |
| `signage_sdk` | `GetPlaylistConfig` / `NearHub-Launcher数字标牌设备管理通用管理命令:GetPlaylistConfig` | `rpc_method:signage.getPlaylistConfig` | `candidate_registry_patch` | Source status: 未研发 |
| `signage_sdk` | `GetPlaylistItemUrl` / `NearHub-Launcher数字标牌设备管理通用管理命令:GetPlaylistItemUrl` | `rpc_method:signage.getPlaylistItemUrl` | `candidate_registry_patch` | Source status: 未研发 |
| `signage_sdk` | `GetSDInfo` / `NearHub-Launcher数字标牌设备管理通用管理命令:GetSDInfo` | `rpc_method:storage.getSdInfo` | `candidate_registry_patch` | Source status: 未研发 |
| `signage_sdk` | `GetScheduleConfig` / `NearHub-Launcher数字标牌设备管理通用管理命令:GetScheduleConfig` | `rpc_method:schedule.getConfig` | `candidate_registry_patch` | Source status: 未研发 |
| `signage_sdk` | `GetUpdateConfig` / `NearHub-Launcher数字标牌设备管理通用管理命令:GetUpdateConfig` | `rpc_method:update.getConfig` | `candidate_registry_patch` | Source status: 未研发 |
| `signage_sdk` | `KeepAlive` / `NearHub-Launcher数字标牌设备管理通用管理命令:KeepAlive` | `rpc_method:system.keepAlive` | `candidate_registry_patch` | Source status: 已研发 |
| `signage_sdk` | `KeepAlive` / `NearHub-Launcher数字标牌设备管理通用管理命令:KeepAlive` | `event:keepalive.keepAliveReceived` | `candidate_registry_patch` | Source status: 已研发 |
| `signage_sdk` | `NotifyLogUploadResult` / `NearHub-Launcher数字标牌设备管理通用管理命令:NotifyLogUploadResult` | `rpc_method:log.notifyUploadResult` | `candidate_registry_patch` | Source status: 未研发 |
| `signage_sdk` | `OnBindState` / `NearHub-Launcher数字标牌设备管理通用管理命令:OnBindState` | `event:setbindconfig.bindState` | `candidate_registry_patch` | Source status: 研发中 |
| `signage_sdk` | `OnTelemetryReport` / `NearHub-Launcher数字标牌设备管理通用管理命令:OnTelemetryReport` | `event:onbindstate.telemetryReport` | `candidate_registry_patch` | Source status: 已研发 |
| `signage_sdk` | `RemoteUpgrade` / `NearHub-Launcher数字标牌设备管理通用管理命令:RemoteUpgrade` | `rpc_method:firmware.upgradeByUrl` | `candidate_registry_patch` | Source status: 未研发 |
| `signage_sdk` | `RequestLogUpload` / `NearHub-Launcher数字标牌设备管理通用管理命令:RequestLogUpload` | `rpc_method:log.requestUpload` | `candidate_registry_patch` | Source status: 未研发 |
| `signage_sdk` | `ResetConfig` / `NearHub-Launcher数字标牌设备管理通用管理命令:ResetConfig` | `rpc_method:system.resetConfig` | `candidate_registry_patch` | Source status: 未研发 |
| `signage_sdk` | `SetAppearanceConfig` / `NearHub-Launcher数字标牌设备管理通用管理命令:SetAppearanceConfig` | `rpc_method:appearance.setConfig` | `candidate_registry_patch` | Source status: 未研发 |
| `signage_sdk` | `SetBindConfig` / `NearHub-Launcher数字标牌设备管理通用管理命令:SetBindConfig` | `rpc_method:binding.setConfig` | `candidate_registry_patch` | Source status: 未研发 |
| `signage_sdk` | `SetDeviceName` / `NearHub-Launcher数字标牌设备管理通用管理命令:SetDeviceName` | `rpc_method:device.setName` | `candidate_registry_patch` | Source status: 未研发 |
| `signage_sdk` | `SetLineInPreGain` / `NearHub-Launcher数字标牌设备管理通用管理命令:SetLineInPreGain` | `rpc_method:audio.setLineInPreGain` | `candidate_registry_patch` | Source status: 未研发 |
| `signage_sdk` | `SetLineOutVolume` / `NearHub-Launcher数字标牌设备管理通用管理命令:SetLineOutVolume` | `rpc_method:audio.setLineOutVolume` | `candidate_registry_patch` | Source status: 未研发 |
| `signage_sdk` | `SetPlaylistConfig` / `NearHub-Launcher数字标牌设备管理通用管理命令:SetPlaylistConfig` | `rpc_method:signage.setPlaylistConfig` | `candidate_registry_patch` | Source status: 未研发 |
| ... | ... | ... | ... | 6 additional mappings are listed in `legacy-to-axtp-map.generated.yaml`. |

## Candidate Registry Patch Summary

- Methods to add: 245
- Events to add: 4
- Capabilities to add: 168
- Schemas to add: 494
- Legacy mappings to add or merge: 262

## Promotion Decisions

- Existing mappings such as `BetaDeviceInfo -> device.getInfo` and `BetaBrightnessSet -> display.setBrightness` remain authoritative.
- AXDP commands are retained as adapter mappings unless they target an already registered AXTP method.
- Firmware/file/log/media/raw streams are represented as STREAM profiles and must not place large chunks into RPC bodies.

## Unresolved Source Issues

- `migration/legacy-sources/extracted` and `migration/legacy-sources/normalized` were absent; this run bootstrapped directly from current raw legacy files.
- Several legacy rows lack detailed request/response field definitions. Generated schemas therefore preserve legacy payload bytes plus status metadata until normalized field-level schemas are available.

## Implementation Order

1. Review `legacy-to-axtp-map.generated.yaml` for mapping correctness.
2. Review `registry-patches.generated.yaml` domain-by-domain before applying any patch to source registries.
3. Implement the C++ legacy adapter using the generated mapping table as data, not hard-coded switch statements.
4. Run generated test vectors against the adapter and AXTP runtime boundary.
