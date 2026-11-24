# Cast 视频编码参数 AV 重建语义设计

## 目标

修正 `cast.setVideoStreamParams` 的活动流重配置语义。NT10 编码帧率或码率变化要求
Nearcast 关闭当前 cast session 的音频和视频 downstream stream，并将它们作为一组全新的
AV stream 重新打开。原有“只重开视频、音频保持运行”的语义不再成立。

## 协议范围

- 不新增 method、event、capability 或 schema ID。
- 继续使用 `cast.setVideoStreamParams`、`video.closeStream`、`audio.closeStream`、
  `video.openStream` 和 `audio.openStream`。
- `video.openStream` 继续携带新的 `frameRate`、`bitrateKbps`。
- 当前 Nearcast 投屏会话按一路 video 和一路 audio 组成 AV pair；活动投屏时两路均已打开。
- `CastSetVideoStreamParamsResult.activeStreamId` 继续表示 video streamId。audio streamId 通过
  `audio.openStream` result 和 `audio.streamStateChanged` 观察，不扩展 cast result schema。

## 活动流重配置事务

1. Nearcast 接受 `cast.setVideoStreamParams`，状态进入 `pending`。
2. Nearcast 分别调用 `audio.closeStream` 和
   `video.closeStream(reason=encodingReconfigure)`。两次 close 的发送顺序不作硬性规定。
3. 必须等待 audio 和 video 都进入 terminal `closed`；任一路未关闭前不得发送新的 open。
4. 旧 audio/video streamId 和旧 syncGroupId 全部作废。
5. Nearcast 分配全新的 syncGroupId。
6. Nearcast 调用：
   - `video.openStream`：使用原 source、codec、streamProfile 和 castSessionId，携带新的
     syncGroupId、frameRate、bitrateKbps。
   - `audio.openStream`：使用原 audio source、codec、streamProfile、timing 参数和
     castSessionId，携带同一个新的 syncGroupId。
7. 两次 open 的发送顺序不作硬性规定，但最终必须形成同一 syncGroupId 下的完整 AV pair。
8. video 和 audio 都返回新的 streamId，并且 NT10 编码参数确认 effective 后，cast 状态才进入
   `applied` / `streaming`。

因此该操作不是 video replacement，而是一次完整的 AV reopen。旧 streamId 不得接收后续媒体包，
旧 syncGroupId 不得继续关联新流。

## 无活动流

`idle` 严格表示当前不存在任何 audio 或 video downstream stream。只有完全无流时才保留现有 idle-open 语义：不发送 close，直接以新参数建立 video/audio AV pair；两路使用新 streamId 和同一个新 syncGroupId。若存在单边或其他 partial media state，必须先 cleanup/normalize，不能按 idle direct-open。

## 失败与回退

- 任一路 close 失败时，不得应用新编码参数；事务进入恢复流程。
- 任一路新 open 失败时，关闭本次已经打开的新流，禁止保留半开的 audio-only 或 video-only 状态。
- 恢复旧 session 编码参数后，重新建立完整 video/audio AV pair。回退也是一次全新打开，使用新的
  video/audio streamId 和新的 syncGroupId，不能复用本次失败或重配置前的标识符。
- 两路都恢复成功才报告 `rolledBack`；否则报告 `failed`，并明确当前没有完整 active AV pair。
- `NOT_SUPPORTED`、参数范围错误和 `BUSY` 不关闭 AXTP/cast session。

## 文档和 conformance 修改范围

- `workspace/protocol/cast/cast.flowControl.md`
- `workspace/protocol/video/video.stream.md`
- `workspace/protocol/audio/audio.stream.md`
- `workspace/flows/cast-receiver-uxplay.md`
- `conformance/README.md`
- `conformance/cases/stream/video_stream_params_active_reconfigure.yaml`
- `conformance/cases/stream/video_stream_params_idle_open.yaml`
- `conformance/cases/stream/video_stream_params_rollback.yaml`

Conformance 必须验证 close barrier、两个新 streamId、新 syncGroupId、完整 AV open、旧标识符作废、
半开流清理和整组 AV 回退。现有 precedence、validation 和 capability-not-supported case 保持语义不变。

## 验收标准

- 所有“只重开视频”或“audio stream 保持不变”的表述均被删除。
- 活动流 case 明确 audio/video 都 close 后才能 open。
- 新 video/audio 使用不同于旧值的 streamId，并共享不同于旧值的新 syncGroupId。
- `video.openStream` 仍携带目标 frameRate/bitrateKbps。
- `applied` 只在两路 open 成功且 NT10 参数 effective 后产生。
- 回退成功必须恢复完整 AV pair；回退失败不得声称存在完整 active stream。
