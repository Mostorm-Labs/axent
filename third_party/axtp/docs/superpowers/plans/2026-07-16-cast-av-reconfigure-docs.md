# Cast AV Reconfigure Documentation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the video-only `cast.setVideoStreamParams` reconfiguration semantics with a complete audio/video close-and-reopen transaction using new stream IDs and a new synchronization group.

**Architecture:** Keep the existing cast, video, and audio RPC surface. Treat an active Nearcast session as one AV pair: close both downstream streams, wait for a close barrier, allocate a new `syncGroupId`, open video with the requested encoder targets and audio with its previous media/timing settings, then report `applied` only after both streams and the NT10 encoder are effective. Conformance YAML is updated first so the required documentation semantics are executable and reviewable.

**Tech Stack:** Markdown protocol documents, Mermaid sequence diagrams, YAML conformance cases, Node.js documentation checks, AXTP conformance validation scripts.

---

### Task 1: Make active reconfiguration conformance require a full AV replacement

**Files:**
- Modify: `conformance/cases/stream/video_stream_params_active_reconfigure.yaml`
- Modify: `conformance/README.md`

- [ ] **Step 1: Replace the video-only active-stream fixture with an AV pair**

Use an explicit old AV context:

```yaml
given:
  source:
    active_video_stream:
      streamId: 4097
      syncGroupId: cast-av-17
  audio:
    active_stream:
      streamId: 8193
      source: wireless_cast_audio
      codec: aac
      transportFormat: adts
      streamProfile: media.audio
      syncGroupId: cast-av-17
      audioPtsMode: derivedFromSeq
      timebase: 48000
      samplesPerPacket: 1024
```

- [ ] **Step 2: Add the two-stream close barrier**

Represent one canonical allowed order while asserting that neither open occurs before both close results:

```yaml
- id: close_old_audio
  direction: server_to_client
  rpc:
    op: REQUEST
    requestId: 631
    method: audio.closeStream
    params:
      streamId: 8193

- id: close_old_video
  direction: server_to_client
  rpc:
    op: REQUEST
    requestId: 632
    method: video.closeStream
    params:
      streamId: 4097
      reason: encodingReconfigure
```

Add successful terminal results for both requests. Keep the prose assertion order-neutral: both close calls may be sent in either order, but both results must be `closed` before any new open.

- [ ] **Step 3: Open a new AV pair**

Use `cast-av-18` for both new streams. The video request carries the requested encoder targets; the audio request restores the previous audio media and timing context:

```yaml
- id: open_new_video
  rpc:
    op: REQUEST
    requestId: 633
    method: video.openStream
    params:
      source: wireless_cast_video
      peerRole: transmitter
      codec: h264
      streamProfile: media.video
      syncGroupId: cast-av-18
      castSessionId: cast-session-17
      frameRate: 30
      bitrateKbps: 6000

- id: open_new_audio
  rpc:
    op: REQUEST
    requestId: 634
    method: audio.openStream
    params:
      source: wireless_cast_audio
      peerRole: transmitter
      codec: aac
      transportFormat: adts
      streamProfile: media.audio
      syncGroupId: cast-av-18
      castSessionId: cast-session-17
      audioPtsMode: derivedFromSeq
      timebase: 48000
      samplesPerPacket: 1024
```

Expect new video streamId `4098` and new audio streamId `8194`.

- [ ] **Step 4: Replace obsolete assertions**

Remove:

```yaml
- audio.streamId == 8193 and audio.syncGroupId == cast-av-17
- video reconfiguration does not restart the audio stream
```

Add assertions that old video/audio stream IDs are terminal, both new IDs differ from the old IDs, old `cast-av-17` is retired, both new streams share `cast-av-18`, and the applied state occurs only after both open results.

- [ ] **Step 5: Update the conformance catalog wording**

Change `stream.video_stream_params_active_reconfigure` in `conformance/README.md` to state:

```text
active AV streams close as a pair, both reach terminal closed, then video and audio reopen with new streamIds and one new syncGroupId; video.openStream carries the new encoder targets.
```

- [ ] **Step 6: Run the conformance source validator**

Run:

```bash
tooling/scripts/validate-conformance.sh
```

Expected: exit code `0`, with all conformance cases reported valid.

### Task 2: Make idle open and rollback operate on complete AV pairs

**Files:**
- Modify: `conformance/cases/stream/video_stream_params_idle_open.yaml`
- Modify: `conformance/cases/stream/video_stream_params_rollback.yaml`

- [ ] **Step 1: Extend idle open to create both media streams**

Keep the no-close assertion, allocate a new `syncGroupId`, and add `audio.openStream` after the video request:

```yaml
params:
  source: wireless_cast_audio
  peerRole: transmitter
  codec: aac
  transportFormat: adts
  streamProfile: media.audio
  syncGroupId: cast-av-idle-1
  castSessionId: cast-session-idle
  audioPtsMode: derivedFromSeq
  timebase: 48000
  samplesPerPacket: 1024
```

Assert that video and audio receive nonzero, distinct stream IDs and share `cast-av-idle-1`.

- [ ] **Step 2: Add the old audio stream to both rollback scenarios**

For each scenario, add an active audio stream with its source, codec, transport format, timing context, streamId, and the old `cast-av-17` synchronization group.

- [ ] **Step 3: Close both old streams before opening replacements**

For each rollback scenario, add `audio.closeStream(oldAudioStreamId)` and its terminal result alongside the existing `video.closeStream(reason=encodingReconfigure)`. Assert both old IDs are terminal before the first new open.

- [ ] **Step 4: Model failed replacement as an AV transaction**

Give the attempted replacement a new synchronization group such as `cast-av-18`. If one open fails, add close steps for any new stream that already opened; the scenario must not leave an audio-only or video-only pair active.

- [ ] **Step 5: Reopen both old media semantics during rollback**

Use another fresh group such as `cast-av-rollback-19`, the previous video frame rate/bitrate, and the previous audio media/timing context. Expect two new stream IDs. Report `rolledBack` only after both results succeed.

- [ ] **Step 6: Tighten rollback-failure assertions**

The failure scenario must assert:

```yaml
- no complete active AV pair remains after rollback failure
- no replacement audio-only or video-only stream remains active
- all streamIds created during the failed transaction are terminal
```

- [ ] **Step 7: Validate both modified cases**

Run:

```bash
tooling/scripts/validate-conformance.sh
```

Expected: exit code `0`; idle and rollback case IDs remain registered once each.

### Task 3: Update cast, video, and audio protocol semantics

**Files:**
- Modify: `workspace/protocol/cast/cast.flowControl.md`
- Modify: `workspace/protocol/video/video.stream.md`
- Modify: `workspace/protocol/audio/audio.stream.md`

- [ ] **Step 1: Rewrite the cast transaction rules**

In `cast.flowControl.md`, retain the existing state values but redefine them as an AV transaction:

```text
pending -> closing -> opening -> streaming
```

Document that `closing` means both `audio.closeStream` and `video.closeStream` have been sent and neither open may start before both terminal results. Document that `opening` covers both new open requests and `applied` is emitted only after the complete AV pair and NT10 target are effective.

- [ ] **Step 2: Define identifier replacement without expanding schemas**

State that both old stream IDs and the old syncGroupId are retired; the replacement uses two new stream IDs and one new syncGroupId. Clarify that `CastSetVideoStreamParamsResult.previousStreamId` and `activeStreamId` remain video IDs, while audio IDs are observed through audio RPC results/events.

- [ ] **Step 3: Rewrite video open/close and rollback sections**

In `video.stream.md`, replace every statement equivalent to “只重开视频” or “audio remains unchanged.” The active path must be:

```text
audio.closeStream + video.closeStream
-> wait for both terminal closed
-> video.openStream(new frameRate/bitrateKbps, new syncGroupId)
   + audio.openStream(previous audio context, same new syncGroupId)
-> wait for both new streamIds
-> encoder target effective / cast applied
```

Update the Nearcast example, transaction description, close semantics, and rollback section consistently.

- [ ] **Step 4: Add coordinated restart semantics to audio.stream**

Add a subsection explaining that `audio.closeStream` can participate in a cast encoder reconfiguration transaction even though only video encoder targets change. The audio request may omit `reason`; no new audio close-reason enum is introduced. Its old streamId and old syncGroupId become terminal, and the replacement audio open preserves source/codec/profile/timing fields while using a new streamId and the new AV syncGroupId.

- [ ] **Step 5: Define whole-pair failure handling**

Across all three documents, require that a partial replacement is closed before rollback. Rollback uses the previous encoder values and previous audio context but still creates two fresh stream IDs and another fresh syncGroupId. `rolledBack` requires both streams; `failed` means there is no complete active AV pair.

- [ ] **Step 6: Scan for contradictory protocol wording**

Run:

```bash
rg -n "只重开视频|音频.*保持不变|audio stream.*保持|does not restart the audio|audio及.*保持" \
  workspace/protocol/cast/cast.flowControl.md \
  workspace/protocol/video/video.stream.md \
  workspace/protocol/audio/audio.stream.md
```

Expected: no matches that prescribe video-only reconfiguration.

### Task 4: Update the Nearcast flow and run final documentation checks

**Files:**
- Modify: `workspace/flows/cast-receiver-uxplay.md`
- Modify: `conformance/README.md`
- Verify: `docs/superpowers/specs/2026-07-16-cast-av-reconfigure-design.md`

- [ ] **Step 1: Replace the flow sequence diagram**

Show these participants and barriers:

```text
Nearcast WS -> NA20: cast.setVideoStreamParams
NA20 -> Audio: audio.closeStream(oldAudioStreamId)
NA20 -> Video: video.closeStream(oldVideoStreamId, encodingReconfigure)
Audio/Video -> NA20: both closed
NA20: allocate new syncGroupId
NA20 -> Video: video.openStream(new params, new syncGroupId)
NA20 -> Audio: audio.openStream(previous audio context, new syncGroupId)
Audio/Video -> NA20: two new streamIds
NA20 -> NT10: encoder target effective
NA20 -> WS: sourceVideo.state=streaming/applied
```

The diagram may serialize calls for readability, but the accompanying rule must state that ordering inside each close/open pair is not normative; the close barrier and complete-AV success condition are normative.

- [ ] **Step 2: Update flow rules and review checks**

State that active Nearcast streaming always has both media streams open, reconfiguration is a full AV reopen, and UxPlay/AirPlay still returns `NOT_SUPPORTED` for this encoder-control sub-capability.

- [ ] **Step 3: Run conformance and documentation validation**

Run:

```bash
tooling/scripts/validate-conformance.sh
node tooling/scripts/check-links.mjs
node tooling/scripts/check-text-paths.mjs
node tooling/scripts/check-frontstage-language.mjs
node tooling/scripts/check-draft-noise.mjs
node tooling/scripts/check-protocol-status.mjs
git diff --check
```

Expected: every command exits `0`.

- [ ] **Step 4: Review the complete diff against the design**

Verify all of the following in the diff:

```text
both old media streams close
both close results form a barrier
both replacement streams get new streamIds
both replacement streams share a new syncGroupId
video.openStream carries frameRate/bitrateKbps
applied requires both streams plus effective NT10 parameters
rollback restores a complete AV pair with fresh identifiers
no schema ID or method/event/capability is added
```

Do not commit or push unless the user explicitly requests it.
