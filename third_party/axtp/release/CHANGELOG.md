# AXTP Spec Changelog

This changelog records AXTP Spec releases published with `spec/vMAJOR.MINOR.PATCH` tags.

Current repository path note: conformance cases now live at the root `conformance/` directory. Older release entries may mention their historical paths.

## spec/v0.18.0

### Protocol

- Adds optional fenced multi-source cast control, atomic snapshots, operation tracking,
  Presentation and independent content adaptation, WebCast backend state, and
  scoped AirPlay and WebCast credential queries and events.
- Adds Presentation automatic entry and primary selection as independent settings.
  Prepared AirPlay access codes retain existing pending challenge generations.

### Registry

- Registers the multi-source control methods, events and capabilities alongside
  the existing cast methods and legacy aggregate notifications.

### Schemas

- Defines bounded command, settings, credential, backend and presentation objects;
  64-bit identities and revisions are encoded as decimal strings in JSON.

### Conformance

- Includes control and settings profiles, executable reference checks and JSON/TLV8
  vectors. Runtime and product implementations require their own verification.

### Migration

- The unpublished development Layout contract is replaced: `layoutMode` now means
  grid/presentation, `contentAdaptation` means fixed/auto-fit, and the former
  `presentationEnabled` field is removed. Consumers of development snapshots must
  migrate; the legacy aggregate cast interfaces retain their documented projection.
- Dedicated credential events can contain secrets and require redaction. Explicit
  sensitive queries must be handled separately from general status and diagnostics.

### Runtime Impact

- Runtime implementations must bind this exact spec tag and regenerate with their
  own generators. P0/P1 capabilities are optional and do not imply device support.

### Original P0/P1 scope

Optional concurrent cast control P0 and P1 settings extend the published v0.17.0
contract; protocol 1.0.0, wire 1 and product controlVersion=1 are independent.

- Adds five methods 0x1615–0x1619 (bits 20–24), six events 0x160D–0x1612
  (bits 12–17) and four capabilities 0x1608–0x160B in their separate ID spaces.
- Adds 33 object schemas (23 P0, 10 P1). Extends getStatus params with optional field 0x03,
  CastStatus with optional fields 0x0A–0x0F, windowChanged with optional field
  0x05 and CastStatusCapability with optional fields 0x03–0x04. All legacy IDs,
  field shapes and single-source semantics are preserved.
- Defines atomic six-section snapshots, full feature commit batches, strict
  runtime/session/slot/binding/lifecycle fences, bounded dedup/result retention,
  unknown outcomes and scoped owner cleanup in specs/45-cast-control.md.
- Generates protocol IR, references, MCP metadata and paired JSON/TLV8 decimal
  string boundary vectors. Introduces optional cast-control conformance C01–C11,
  fixtures and an executable reference oracle. The separate optional cast-settings
  profile adds C12/PS-01–PS-12 without expanding the P0 required set.
- Downstream consumers should use an approved exact spec tag/commit, regenerate
  runtime metadata/codecs with their own generators, then update Axent and NearCast
  in that order. This repository does not implement media or product policy.
- P1 adds field-only persistence before configuration/live application, truthful
  partial results, startup versus desired/adopted/stored projections and an
  independent settings stream. Optional fields append at CastControlCapabilities
  0x0A and CastControlOperation 0x0E; the new operation kind has no session target.
  Settings cursor JSON/TLV8 vectors preserve decimal strings. See
  `release/CAST_CONTROL_P0_RELEASE.md` for the local verification
  boundary and downstream migration contract.

## spec/v0.17.0

Signage playlist item typed settings, PowerBI item support, and full playlist item refresh, merged through PR #39.

### Protocol

- Declares the `SignagePlaylistItem.type` enum (image, website, video, clock, unsplash, powerbi) and splits playlist item settings into per-type `SignageImageItemSettings`, `SignageVideoItemSettings`, `SignageWebsiteItemSettings`, `SignageClockItemSettings`, `SignageUnsplashItemSettings`, and `SignagePowerBiItemSettings`, selected through the new `variants` discriminator binding on `PlaylistItem.settings`.
- Adds `signage.getPlaylistItem` to fetch the complete playlist item by `itemId`; the device calls it proactively when `settings.expiresAt` approaches expiry and replaces the local playlist item with the returned one.
- Non-URL resource types (`powerbi`) must use `signage.getPlaylistItem`; URL resource types may use it or `signage.getPlaylistItemUrl`, while `clock` and `powerbi` return `NOT_SUPPORTED` from URL refresh.

### Registry

- Registers `signage.getPlaylistItem` as `0x0D06` with method bit offset `5` in the signage domain.
- Updates the `signage.playlist` capability description to cover playlist item refresh alongside URL refresh; existing method, event, capability, error, and schema IDs are preserved.

### Schemas

- Adds `SignageGetPlaylistItemParams` (itemId selector) and `SignageGetPlaylistItemResult` (complete refreshed `SignagePlaylistItem`).
- Adds the `SignagePlaylistItem.type` enum and the six per-type settings schemas, replacing the merged settings object with a variant-selected structure.
- Extends the codec specification with the object-field `variants: { discriminator, mapping }` binding rule and its compatibility entry for adding a variant (new enum value plus mapping entry).

### Conformance

- Does not add conformance cases; the shared conformance manifest count is unchanged.
- Extends generator source and protocol validation with variant-binding consistency checks (discriminator resolves to a declared enum field in the same schema, mapping covers all enum values, and mapped values resolve to registered object schemas), covered by new unit tests.

### Migration

- Playlist item settings consumers must read the `type` discriminator and decode the matching settings variant instead of assuming a single merged settings layout; `powerbi` items require `signage.getPlaylistItem` for refresh.
- Existing runtimes that only implement URL refresh remain compatible for URL-capable item types; `clock` and `powerbi` items now explicitly return `NOT_SUPPORTED` from `signage.getPlaylistItemUrl`.

### Runtime Impact

- Runtime and SDK teams should bind to `spec/v0.17.0`, regenerate protocol metadata, and implement `signage.getPlaylistItem` with the per-type settings variants.
- No npm, pub, PyPI, Docker, or runtime package registry publish is part of this Spec release.

## spec/v0.16.0

Basketball device configuration controls for watermark display and event clip time windows.

### Protocol

- Adds `sport.getGoalShotWatermarkConfig` and `sport.setGoalShotWatermarkConfig` for per-device Goal/Shot watermark control.
- Adds `sport.getEventClipConfig` and `sport.setEventClipConfig` for configuring the pre-event and post-event clip window around `shot` / `goal` events.
- Requests target one connected device using `macAddress` and select the sport with `sportType`; set methods return operation status and the resulting configuration.

### Registry

- Registers the four methods as `0x1804`–`0x1807` with method bit offsets `3`–`6`.
- Extends the `sport.basketball` capability descriptor with optional `supportedConfigMethods` (`fieldId=0x05`) for capability-driven method discovery.
- Preserves all existing event-detection method, event, capability, and schema IDs.

### Schemas

- Adds watermark configuration fields for Goal/Shot display state and sport selection.
- Adds event clip configuration fields for pre-event and post-event seconds, with validation and atomic update semantics.
- Adds MAC-address device selectors and status-only set response behavior for app-to-device configuration calls.

### Conformance

- Adds four sport cases covering capability discovery, multi-device MAC routing, get/set round trips, status-only set responses, validation, and atomic updates.
- Expands the shared conformance manifest from 36 to 40 cases.
- Generator tests, source/protocol validation, generated drift checks, and conformance validation pass.

### Migration

- Existing runtimes remain compatible when they do not advertise the new configuration methods; unsupported calls continue to use normal capability/`NOT_SUPPORTED` handling.
- Runtime implementations should route configuration by `macAddress` and `sportType`, preserve unrelated device settings, and reject invalid clip windows without partial updates.

### Runtime Impact

- Runtime and SDK teams should bind to `spec/v0.16.0`, regenerate protocol metadata, and verify that `kMethodRegistry` (or equivalent) includes the four new `sport.*` methods.
- No npm, pub, PyPI, Docker, or runtime package registry publish is part of this Spec release.

## spec/v0.15.0

Object-RPC endpoint relay addressing, merged through PR #11 at authority commit `2d88ff4c369411d8e3afbb551886d423bd63bb82`.

### Protocol

- Adds optional object-RPC endpoint addressing metadata for a caller-selected destination endpoint and a relay-selected source endpoint, while preserving operation on peers that omit endpoint metadata.
- Defines deterministic endpoint identity keys and endpoint-ID assignment so a relay can address child or virtual endpoints consistently.
- Defines single-destination RPC relay and event fanout semantics, including source endpoint metadata for relayed events.
- Clarifies the endpoint-relay codec representation in the core and codec specifications.
- Incorporates the previously merged cast AV reconfiguration documentation: active NT10 encoder parameter changes rebuild the complete audio/video pair with new stream IDs and a new shared sync group.

### Registry

- Does not add or renumber generated method, event, capability, error, or schema IDs.
- Keeps endpoint relay metadata optional and compatible with implementations that do not advertise the endpoint-relay profile.

### Schemas

- Extends the conformance case schema to express endpoint metadata and relay addressing expectations without changing existing generated business schemas.

### Conformance

- Adds the `endpoint-relay` profile and cases for endpoint metadata compatibility, single-destination RPC relay addressing, and event fanout addressing.
- Registers endpoint relay requirements in the conformance-level documentation and manifest.
- Updates the video-stream parameter reconfiguration cases to reflect full audio/video replacement and rollback semantics.

### Migration

- Existing object-RPC implementations remain valid when endpoint metadata is absent; relays and endpoints that support the optional profile must use the defined deterministic identity and addressing rules.
- Runtime implementations of `cast.setVideoStreamParams` must treat active reconfiguration as complete AV-pair replacement, invalidating old stream IDs and sync group IDs.

### Runtime Impact

- Runtime and SDK teams that implement object-RPC endpoint relays should bind to `spec/v0.15.0`, add the optional endpoint-relay profile, and run the new relay conformance cases.
- Runtime implementations of NT10 encoder parameter control should follow the clarified AV replacement lifecycle before advertising active video reconfiguration support.
- No npm, pub, PyPI, Docker, or runtime package registry publish is part of this Spec release.

## spec/v0.14.0

Cross-sport event detection registry and VM33PRO basketball shot/goal event contract.

### Protocol

- Adds the `sport` domain and the shared `sport.eventDetection` control surface for capability discovery, per-sport detection toggles, runtime state, and unified business events.
- Adds `sport.getEventDetectionCapabilities`, `sport.getEventDetectionConfig`, and `sport.setEventDetectionConfig`.
- Adds `sport.eventDetectionStateChanged` and `sport.eventDetected` for device-to-App WebSocket event delivery.
- Fixes the MVP lifecycle policy for VM33PRO: one active sport at a time, persistent detection toggles, factory-reset default disabled, and no new events after the detector reaches `disabled`.

### Registry

- Registers `sport` at high-byte `0x18` with method IDs `0x1801`-`0x1803`, event IDs `0x1801`-`0x1802`, and capability IDs `0x1801`-`0x1802`.
- Adds `sport.basketball` as a specialization capability with `shot` and `goal` event types.
- Reuses common errors; no basketball-specific error code or legacy mapping is added.

### Schemas

- Adds cross-sport capability, state, state-change, and unified event envelope schemas.
- Adds `SportEventDetails`, `SportBasketballShotDetails`, and `SportBasketballGoalDetails`.
- Requires `eventId`, `sequence`, `occurredAt`, and `details` in detected-event payloads; `confidence` and `trainingSessionId` remain optional.
- Requires every basketball `goal` to reference `shotId`; one `shot` produces at most one `goal`.

### Conformance

- Generator source validation, Protocol IR validation, generated drift checks, and the generator test suite pass for the expanded registry.
- No new hand-written conformance vectors are added in this release; MVP does not promise disconnect replay, history queries, or exactly-once delivery.

### Migration

- Existing runtimes remain compatible when they do not advertise `sport.eventDetection` or `sport.basketball`.
- Basketball-capable runtimes should use the shared event-detection methods and route `sport.eventDetected` by `sportType` and `eventType`.
- Any future replay/history capability or legacy adapter requires a separate reviewed amendment.

### Runtime Impact

- Runtime and SDK teams should bind to `spec/v0.14.0`, regenerate protocol metadata, implement or explicitly degrade the new sport capabilities, and run the generated validation/conformance checks.
- Existing CONTROL/RPC/STREAM wire layout and previously registered domains remain backward compatible with `spec/v0.13.0`.
- No npm, pub, PyPI, Docker, or runtime package registry publish is part of this Spec release.

## spec/v0.13.0

NT10 source video encoder parameter control through the Nearcast cast flow-control surface.

### Protocol

- Adds `cast.setVideoStreamParams` under `cast.flowControl` so Nearcast can configure the current cast session's NT10/source encoder frame rate and bitrate.
- Defines coordinated active-stream replacement as `video.closeStream(reason=encodingReconfigure)` followed by `video.openStream(peerRole=transmitter)` with a new `streamId`; idle sources open directly.
- Keeps audio running, preserves the existing `syncGroupId`, and defines rollback to the previous parameters and stream when the replacement open fails.
- Defines parameter precedence as explicit `video.openStream` values, then current-session cast values, then source/profile defaults; omitted set fields remain unchanged and only `resetFields` clears them.

### Registry

- Adds `cast.setVideoStreamParams` as method `0x1614`, `bitOffset=19`, bound to the existing `cast.flowControl` capability and `cast.flowControlChanged` event.
- Extends cast flow-control capability and state metadata with video parameter support, active reconfiguration support, desired/effective encoder values, reconfiguration state, stream identifiers, rollback status, and diagnostics.
- Extends `video.stream` source, open, state, event, and close-reason facts with encoder frame-rate/bitrate capabilities and `encodingReconfigure` lifecycle metadata.
- Promotes `MEDIA_BITRATE_UNSUPPORTED` to the MVP error set and exposes it from the relevant cast/video methods.

### Schemas

- Adds `CastSetVideoStreamParamsParams`, `CastSetVideoStreamParamsResult`, and `CastVideoStreamParamsState`.
- Extends `CastFlowControlState`, `CastFlowControlChangedEvent`, `CastFlowControlCapability`, `VideoStreamSource`, `VideoStreamState`, and `VideoStreamStateChangedEvent` without changing existing field IDs.
- Treats zero encoder frame rate or bitrate as invalid; callers omit fields to preserve current values and use `resetFields` to restore source/profile defaults.

### Conformance

- Adds capability degradation coverage proving unsupported encoder control returns `NOT_SUPPORTED` without closing the AXTP session.
- Adds idle-open, active close/open replacement, explicit-open precedence, rollback, validation, and concurrent-`BUSY` cases.
- Expands the shared conformance manifest from 30 to 36 cases.

### Migration

- Existing runtimes remain compatible when they do not advertise `cast.flowControl.supportsVideoStreamParams`; registered-but-unavailable calls return `NOT_SUPPORTED`.
- Implementations that advertise the new fields must preserve session-local values, reject unsupported profile values with typed media errors, and serialize active reconfiguration operations.

### Runtime Impact

- `axtp-cpp-runtime` should bind to `spec/v0.13.0`, regenerate protocol metadata, implement or explicitly degrade `cast.setVideoStreamParams`, and run the new conformance cases.
- Existing CONTROL/RPC/STREAM framing and previously registered method, event, capability, and schema IDs remain backward compatible with `spec/v0.12.0`.
- No npm, pub, PyPI, Docker, or runtime package registry publish is part of this Spec release.

## spec/v0.12.0

Cast receiver audio delay compensation and event envelope clarification.

### Protocol

- Adds `cast.setAudioDelay` so receivers can configure local cast audio playback delay compensation, with `0` disabling compensation.
- Documents the intended receiver behavior: audio delay shifts local output timing only, does not change media cursors, and does not replace AV drift correction.
- Clarifies that JSON RPC event payload data does not repeat `sid`, while every event message still carries `sid` in the outer `sid/op/d` envelope.

### Registry

- Extends the generated `cast` domain with `cast.setAudioDelay` as method `0x1613`, `bitOffset=18`, request schema `CastSetAudioDelayParams`, response schema `CastAudioState`, and event `cast.audioChanged`.
- Updates `cast.audioChanged` metadata so delay changes are part of the event trigger surface.
- Preserves existing cast method IDs, event IDs, field IDs, and bit offsets for previously generated facts.

### Schemas

- Adds `CastSetAudioDelayParams.audioDelayMs:uint32` with range `0..1000`.
- Adds optional `CastAudioState.audioDelayMs` with default `250`.
- Adds `CastAudioCapability.supportsAudioDelay`, `defaultAudioDelayMs`, and `maxAudioDelayMs` capability fields.

### Conformance

- Does not add new hand-written conformance cases in this release.
- Generator build, generator tests, source validation, protocol validation, and generated protocol output checks cover the new registry and schema facts.

### Migration

- Cast runtimes may keep existing behavior if they do not advertise or implement audio delay support.
- Cast receivers that implement `cast.audio` should expose the configured delay through `CastAudioState.audioDelayMs` and clear runtime PCM delay buffers on stream open, close, device reopen, or format change.
- Event consumers should continue reading RPC `sid` from the outer event envelope, not from event `data`.

### Runtime Impact

- Runtime and SDK teams should regenerate protocol metadata from `spec/v0.12.0` before exposing `cast.setAudioDelay` or the new cast audio capability fields.
- Existing CONTROL/RPC/STREAM wire layout, generated domains outside `cast`, shared errors, and previously generated cast facts remain backward compatible with `spec/v0.11.2`.
- No npm, pub, PyPI, Docker, or runtime package registry publish is part of this Spec release.

## spec/v0.11.2

Core RPC `sid` receiver compatibility patch.

### Protocol

- Clarifies that AXTP-native Logical Servers should continue generating `sid` values as non-zero `uint32` rendered as 8-character hex strings in JSON object encodings.
- Loosens JSON / CBOR / MSGPACK receiver requirements so assigned `sid` values are treated as opaque non-empty strings, allowing legacy or external session strings that are not 8-character hex.
- Keeps JSON_BINARY `sid` as a 4-byte Big-Endian `uint32`, with `0` reserved for the unassigned state.

### Registry

- Does not change registry YAML, generated method/event/capability facts, schema facts, profile facts, or stable IDs.

### Schemas

- Does not change business schema definitions.

### Conformance

- Does not add new hand-written conformance cases in this release.
- Existing generator validation, protocol validation, generated drift checks, release artifact checks, and Markdown/link checks cover the clarified core text.

### Migration

- JSON runtimes and SDKs that previously rejected assigned `sid` strings only because they were not 8-character hex should preserve and match the returned non-empty string instead.
- AXTP-native servers may continue producing 8-character hex `sid` strings such as `"00000003"`.

### Runtime Impact

- Runtime and SDK teams are not required to regenerate generated business protocol metadata for this patch, because machine-readable registry facts are unchanged.
- Existing CONTROL/RPC/STREAM wire layout, generated domains, shared errors, and business contract facts remain unchanged from `spec/v0.11.1`.
- No npm, pub, PyPI, Docker, or runtime package registry publish is part of this Spec release.

## spec/v0.11.1

Core RPC envelope clarification patch.

### Protocol

- Corrects the core RPC JSON RequestResponse example so `d.status` is an object carrying `status.ok` and `status.code` instead of a numeric shortcut.
- Clarifies that JSON / CBOR / MSGPACK RequestResponse envelopes MUST carry `status.ok` and `status.code`, while JSON_BINARY uses its fixed-header `statusCode:uint16` with the same semantics.
- Restores the explicit `sid` generation and encoding rules in the current core spec: `sid` is a `uint32`, JSON forms are fixed 8-character hex strings after Identified, `0` is reserved, and examples such as `"00000003"` are valid non-zero session IDs.

### Registry

- Does not change registry YAML, generated method/event/capability facts, schema facts, profile facts, or stable IDs.

### Schemas

- Does not change business schema definitions.

### Conformance

- Does not add new hand-written conformance cases in this release.
- Existing generator validation, protocol validation, generated drift checks, release artifact checks, and Markdown/link checks cover the corrected core text.

### Migration

- Runtime implementations that already use `status.ok` / `status.code` and fixed 8-character hex `sid` values do not need code changes.
- Implementations that copied the erroneous `status: 0` JSON example should emit the standard status object form.

### Runtime Impact

- Runtime and SDK teams are not required to regenerate generated business protocol metadata for this patch, because machine-readable registry facts are unchanged.
- Existing CONTROL/RPC/STREAM wire layout, generated domains, shared errors, and `cast` contract facts remain unchanged from `spec/v0.11.0`.
- No npm, pub, PyPI, Docker, or runtime package registry publish is part of this Spec release.

## spec/v0.11.0

Cast receiver snapshot status and window schema simplification.

### Protocol

- Keeps AirPlay receiver name control in the `cast.session` surface through the existing `cast.getAirPlayName` and `cast.setAirPlayName` methods.
- Refines `cast.status` into a current snapshot query only: `cast.getStatus` remains the reconnect, initial-load, and event-loss recovery calibration entry point.
- Removes the draft `cast.statusChanged` aggregate event so continuous updates are represented by feature-specific cast events and clients can call `cast.getStatus` when they need to resynchronize.
- Simplifies `CastWindowState` by removing `previousNormalBounds` and `restoredBounds`; `bounds` is the single protocol field for the current window rectangle.
- Adds schema reference tables with requiredness, field IDs, types, constraints, and defaults to the generated cast protocol draft documents.

### Registry

- Updates `contract/registry/domains/cast/domain.yaml` and generated protocol outputs to remove one draft event, reducing the generated cast method/event facts from 31 to 30.
- Removes `CastStatusChangedEvent` and `supportsStatusChangedEvent` from the generated cast status contract.
- Preserves existing cast method IDs, event IDs, field IDs, and bit offsets for the retained generated surface; removed window field IDs `0x08` and `0x09` are not reused.

### Schemas

- Updates `CastStatus`, `CastGetStatusParams`, receiver/session/PIN status summaries, and `CastStatusCapability` descriptions to use snapshot terminology.
- Updates `CastWindowState` examples and schema references so get/set/event results all report the current window state shape through `bounds`.
- Keeps sensitive status redaction behavior as a draft review item while retaining `redacted` markers in the current snapshot schema.

### Conformance

- Does not add new hand-written conformance cases in this release.
- Generator validation, protocol validation, generated drift checks, protocol status checks, release artifact checks, and protocol snapshot tests cover the cast registry and generated output changes.

### Migration

- Cast runtimes and SDKs that previously consumed the draft `cast.statusChanged` event should stop advertising or subscribing to that event and use feature-specific events plus `cast.getStatus` for resynchronization.
- Cast clients should treat `CastWindowState.bounds` as the current rectangle and remove dependencies on `previousNormalBounds` or `restoredBounds`.

### Runtime Impact

- Runtime and SDK teams should regenerate protocol metadata from `spec/v0.11.0` before exposing the revised cast status and window surfaces.
- Existing non-cast behavior, CONTROL/RPC/STREAM wire layout, shared errors, and generated domains outside `cast` are unchanged from `spec/v0.10.0`.
- No npm, pub, PyPI, Docker, or runtime package registry publish is part of this Spec release.

## spec/v0.10.0

Signage, software, and device enrollment business protocol adoption, plus generator-driven domain registry expansion.

### Protocol

- Adds the `signage` business domain as a generated AXTP protocol surface for digital signage playlist synchronization, query, reset, and playlist item URL refresh.
- Adds generated signage methods for playlist capabilities, playlist config get/set/reset, and playlist item URL refresh.
- Adds the `software` business domain as a generated AXTP protocol surface for device software configuration and update policy control.
- Adds generated software methods for software config get/set/reset and update policy get/set/reset.
- Adds the `device.enrollment` protocol surface into the `device` domain as a generated AXTP protocol surface for pairing code and device enrollment state.
- Adds generated device methods for pairing code retrieval and enrollment state get/set.
- Adds generated events for signage playlist config changes, software config changes, software update policy changes, and device enrollment state changes.

### Registry

- Adds generator-driven domain registry source files `contract/registry/domains/signage/domain.yaml`, `contract/registry/domains/software/domain.yaml`, and `contract/registry/domains/device/domain.yaml`, with 15 new methods, 4 new events, and 5 new capabilities across the signage, software, and device enrollment surfaces.
- Extends `contract/registry/core/domain_registry.yaml` with the high-byte-to-domain map covering `device` (`0x01`), `signage` (`0x0D`), and `software` (`0x0E`), and maps the generator/validator high byte to each new domain.
- Adds new error codes `ENROLLMENT_CODE_EXPIRED` and `ENROLLMENT_CODE_ALREADY_USED` for device enrollment pairing flow.
- Promotes the domain registry to a generator-driven model so domain YAML sources feed generator validation and generated artifacts directly.
- Marks the signage, software, and device enrollment protocol drafts as generated and updates product domain status/health summaries to reflect the new generated method/event facts.

### Schemas

- Adds generated schemas for signage playlist config, capabilities, and item URL refresh, including playlist item descriptor and validation constraints.
- Adds generated schemas for software config payloads and update policy descriptors.
- Adds generated schemas for device enrollment pairing code, enrollment state, and enrollment state change payloads.
- Keeps open product policies such as playlist rotation rules, software config value bounds, and update policy scheduling out of the generated contract until they are amended explicitly.

### Conformance

- Does not add new hand-written conformance cases in this release.
- Generator validation, protocol validation, generated drift checks, protocol status checks, and protocol snapshot tests cover the new generated registry surface.

### Migration

- Keeps signage, software, and device enrollment legacy mapping notes in `workspace/protocol/**` drafts as non-contract review material until each legacy alias is confirmed.
- Updates `workspace/legacy-migration/MIGRATION_DASHBOARD.md` to reflect the new signage, software, and device enrollment adoption status.

### Runtime Impact

- Runtime and SDK teams should regenerate protocol metadata from `spec/v0.10.0` before exposing signage, software, or device enrollment controls.
- Existing non-signage, non-software, and non-enrollment behavior, CONTROL/RPC/STREAM wire layout, shared errors, and previously generated domains are unchanged and remain backward compatible with `spec/v0.9.0`.
- No npm, pub, PyPI, Docker, or runtime package registry publish is part of this Spec release.

## spec/v0.9.0

Cast receiver protocol registry and generated contract expansion.

### Protocol

- Adds the `cast` business domain as a generated AXTP protocol surface for AirPlay/UxPlay receiver control.
- Adds generated cast methods for session state, AirPlay receiver name, local audio playback/mute, PIN protection, window state, backend status/restart, receiver-local flow control, and aggregate status.
- Adds generated cast events for incoming session, phase/state changes, started/stopped/failed sessions, audio/PIN/window/backend/flow-control/status changes, and PIN authentication notifications.

### Registry

- Registers the `0x16xx` business domain range for `cast` and maps the generator/validator high byte to the `cast` domain.
- Adds `contract/registry/domains/cast/domain.yaml` with 18 methods, 13 events, 7 capabilities, and the supporting cast schema set.
- Marks the cast protocol drafts as generated and updates product domain status/health summaries to reflect 31 generated cast method/event facts.

### Schemas

- Adds cast receiver phase and session schemas, including `receiverPhase` states for incoming, authenticating, stream starting, streaming, rendering, interrupted, stopping, ended, and failed.
- Adds schemas for PIN visibility/redaction, local audio state, window bounds/mode, backend state/errors, flow-control policy/statistics, and aggregate cast status.
- Keeps open product policies such as PIN format, AirPlay name constraints, no-window behavior, and status throttling out of the generated contract until they are amended explicitly.

### Conformance

- Does not add new conformance cases in this release.
- Generator validation, protocol validation, generated drift checks, protocol status checks, and protocol snapshot tests cover the new generated registry surface.

### Migration

- Existing runtimes remain compatible with earlier `spec/v0.8.x` contracts if they do not advertise or implement cast capabilities.
- Runtimes that want AirPlay/UxPlay receiver control should bind to `spec/v0.9.0` and implement the generated `cast.*` methods, events, capabilities, and schema references from `contract/generated/protocol.json`.

### Runtime Impact

- Runtime and SDK teams should regenerate protocol metadata from `spec/v0.9.0` before exposing cast receiver controls.
- Existing non-cast behavior, CONTROL/RPC/STREAM wire layout, shared errors, and previously generated domains are unchanged.
- No npm, pub, PyPI, Docker, or runtime package registry publish is part of this Spec release.

## spec/v0.8.10

Frontstage language consistency and release artifact safety patch.

### Protocol

- Keeps AXTP wire, CONTROL, RPC, STREAM, method, event, schema, capability, profile, and error semantics unchanged from `spec/v0.8.9`.
- Does not change registry YAML, Protocol IR, generated references, MCP artifacts, test vectors, or conformance case behavior.

### Documentation

- Normalizes frontstage documentation to Chinese-first wording across the docs reading route, role guides, product domain status, roadmap, and protocol draft health report while preserving protocol terms, paths, IDs, and implementation anchors.
- Refreshes `docs/product/protocol-draft-health.md` as a release artifact-safe product summary that exposes domain-level health counts without listing backstage `workspace/protocol/**` draft paths.
- Keeps file-level draft health analysis available through the local report script `--json` mode for maintainers who need backstage queues.

### Release Governance

- Adds `tooling/scripts/check-frontstage-language.mjs` to guard frontstage Chinese-first navigation language and prevent concrete backstage workspace paths from leaking into release artifact product pages.
- Wires the frontstage language checker into PR validation, spec release validation, local release validation, and release artifact dry-run checks.
- Extends release artifact dry-run validation so artifact-local frontstage docs are checked after the package is built.

### Runtime Impact

- Runtime and SDK teams do not need wire-level, registry, SDK API, generated metadata, or conformance behavior changes for this release.
- Runtime upgrade workflows should bind to the exact `spec/v0.8.10` tag and consume the release artifact as before.
- No npm, pub, PyPI, Docker, or runtime package registry publish is part of this Spec release.

## spec/v0.8.9

Protocol draft field readability, Chinese frontstage documentation, and governance patch.

### Protocol

- Keeps AXTP wire, CONTROL, RPC, STREAM, method, event, schema, capability, profile, and error semantics unchanged from `spec/v0.8.8`.
- Does not change registry YAML, Protocol IR, generated references, MCP artifacts, test vectors, or conformance case behavior.

### Documentation

- Converts the root README, docs reading route, and formal human-readable `specs/**` corpus to Chinese-first wording while preserving protocol keywords, wire examples, registry IDs, field names, paths, and validator anchor phrases.
- Removes generic protocol draft field placeholders such as repeated `sections`, `constraints`, empty `state` result descriptions, and repeated adoption-placeholder wording while preserving every method request/success example.
- Rewrites draft result tables so `target`, `state`, `sampledAt`, and formerly vague result rows carry sample values, concrete field summaries, or field-specific descriptions.
- Moves repeated schema-adoption guidance into `workspace/protocol/draft-conventions.md` so draft bodies stay focused on feature-specific params, results, events, capability fields, legacy mapping, and review questions.
- Refreshes `docs/product/protocol-draft-health.md` with `Generic field placeholders` tracking so maintainers can see that method examples remain complete and placeholder text stays at zero.

### Release Governance

- Extends the protocol draft noise checker to reject generic target/state/sampledAt rows, repeated schema-adoption reminders, fallback result descriptions, and "see success example" field descriptions.
- Keeps release artifact contents, manifest paths, runtime dispatch payloads, and release automation behavior unchanged from `spec/v0.8.8`.

### Runtime Impact

- Runtime and SDK teams do not need wire-level, registry, SDK API, generated metadata, or conformance behavior changes for this release.
- Runtime upgrade workflows should bind to the exact `spec/v0.8.9` tag and consume the release artifact as before.
- No npm, pub, PyPI, Docker, or runtime package registry publish is part of this Spec release.

## spec/v0.8.8

Protocol draft readability and template hygiene patch.

### Protocol

- Keeps AXTP wire, CONTROL, RPC, STREAM, method, event, schema, capability, profile, and error semantics unchanged from `spec/v0.8.7`.
- Does not change registry YAML, Protocol IR, generated references, MCP artifacts, test vectors, or conformance case behavior.

### Documentation

- Trims repeated capability-section pointers and generic common-error rows from protocol drafts while keeping method request/success examples, feature-specific error notes, state machines, legacy mapping notes, and review markers.
- Normalizes remaining protocol draft examples by replacing old English error headings, generic failure reasons, and template placeholder values with compact Chinese headings and stable sample values.
- Updates `workspace/protocol/draft-conventions.md` and the Stage 20 protocol draft template so new drafts keep feature-specific JSON examples but do not copy common capability or error boilerplate.
- Refreshes `docs/product/protocol-draft-health.md` so product and protocol maintainers can see the current draft line counts, JSON example coverage, and review marker density.

### Release Governance

- Extends the protocol draft noise checker to reject repeated capability pointers, generic common-error rows, old `op=7/8` English headings, placeholder failure reasons, and template sample placeholders.
- Keeps release artifact contents, manifest paths, runtime dispatch payloads, and release automation behavior unchanged from `spec/v0.8.7`.

### Runtime Impact

- Runtime and SDK teams do not need wire-level, registry, SDK API, generated metadata, or conformance behavior changes for this release.
- Runtime upgrade workflows should bind to the exact `spec/v0.8.8` tag and consume the release artifact as before.
- No npm, pub, PyPI, Docker, or runtime package registry publish is part of this Spec release.

## spec/v0.8.7

Release artifact governance and backstage documentation slimming patch.

### Protocol

- Keeps AXTP wire, CONTROL, RPC, STREAM, method, event, schema, capability, profile, and error semantics unchanged from `spec/v0.8.6`.
- Does not change registry YAML, Protocol IR, generated references, MCP artifacts, test vectors, or conformance case behavior.

### Documentation

- Slims the frontstage `docs/guides/core-protocol-flow.md` into a runtime startup-flow summary and moves the long packet, authentication, and STREAM reference into `workspace/runtime/core-protocol-flow.md`.
- Removes repeated request/response/error example boilerplate, generic flow examples, and fixed registry/conformance status tables from protocol drafts while preserving feature-specific fields, payload examples, state machines, legacy mapping notes, and review markers.
- Expands `workspace/protocol/draft-conventions.md` as the shared home for common method examples, response envelope rules, generic flow guidance, and registry/conformance status conventions.

### Release Governance

- Adds `tooling/release/artifact-contract.json` as the shared release artifact contract for required paths and excluded repository-only materials.
- Updates release artifact build and check scripts to read the same artifact contract, reducing duplicated required-path lists.
- Validates the release artifact and artifact-local Markdown links in the `spec/v*` release workflow before creating or updating the GitHub Release.
- Extends local release validation so artifact dry-run, artifact link check, and protocol draft noise checks run before tagging.

### Runtime Impact

- Runtime and SDK teams do not need wire-level, registry, SDK API, generated metadata, or conformance behavior changes for this release.
- Runtime upgrade workflows should bind to the exact `spec/v0.8.7` tag and consume the release artifact as before.
- No npm, pub, PyPI, Docker, or runtime package registry publish is part of this Spec release.

## spec/v0.8.6

Protocol draft governance and runtime dispatch consolidation patch.

### Protocol

- Keeps AXTP wire, CONTROL, RPC, STREAM, method, event, schema, capability, profile, and error semantics unchanged from `spec/v0.8.5`.
- Does not change registry YAML, Protocol IR, generated references, MCP artifacts, test vectors, or conformance case behavior.

### Documentation

- Expands `workspace/protocol/draft-conventions.md` as the shared home for capability discovery, placeholder error, generic test matrix, and common review-question guidance.
- Removes repeated template boilerplate from protocol drafts while preserving feature-specific methods, events, schemas, legacy mapping notes, review markers, and draft status.
- Clarifies that Chinese release docs are the maintainer-facing primary reading path, while English release docs remain for runtime repositories, release artifacts, and cross-repo automation.

### Release Governance

- Adds a protocol draft noise checker to PR validation, spec release validation, and local release validation so repeated template boilerplate does not re-enter drafts.
- Consolidates runtime repository dispatch configuration into a single shared list and replaces duplicated workflow dispatch logic with a shared script.
- Keeps the release artifact contents and path contract unchanged from `spec/v0.8.5`.

### Runtime Impact

- Runtime and SDK teams do not need wire-level, registry, SDK API, generated metadata, or conformance behavior changes for this release.
- Runtime upgrade workflows should observe the same `axtp_spec_released` payload shape as previous releases, now produced by shared release tooling.
- No npm, pub, PyPI, Docker, or runtime package registry publish is part of this Spec release.

## spec/v0.8.5

Reader experience and governance patch.

### Protocol

- Keeps AXTP wire, CONTROL, RPC, STREAM, method, event, schema, capability, profile, and error semantics unchanged from `spec/v0.8.4`.
- Does not change registry YAML, Protocol IR, generated references, MCP artifacts, test vectors, or conformance case behavior.

### Documentation

- Clarifies the public docs reading route so Release owner remains a front-door role while `release/` is described as an operation area rather than a runtime behavior contract.
- Converts the product roadmap to Chinese-first wording for the current product and architecture audience.
- Consolidates repeated protocol draft JSON `d` block boilerplate into `workspace/protocol/draft-conventions.md` and removes duplicated boilerplate lines from workspace drafts without changing feature-specific draft content.
- Cleans lifecycle skill instructions by removing repeated spec path entries and replacing the old "08 rules" wording with the current `specs/30-registry.md` reference.

### Release Governance

- Adds the plain-text repository path checker to both PR validation and spec release validation so stale hidden paths are caught by CI.
- Keeps the release artifact contents and path contract unchanged from `spec/v0.8.4`.

### Runtime Impact

- Runtime and SDK teams do not need wire-level, registry, SDK API, generated metadata, or conformance behavior changes for this release.
- Runtime upgrade workflows can treat this as a documentation/governance patch while continuing to bind to the exact `spec/v0.8.5` tag.
- No npm, pub, PyPI, Docker, or runtime package registry publish is part of this Spec release.

## spec/v0.8.4

Chinese human-readable specs patch.

### Protocol

- Keeps AXTP wire, CONTROL, RPC, STREAM, method, event, schema, capability, profile, and error semantics unchanged from `spec/v0.8.3`.
- Keeps the flattened numbered specs layout introduced in `spec/v0.8.1`.
- Translates the formal human-readable `specs/**` documents to Chinese while preserving implementation anchors, wire examples, registry IDs, field names, paths, and RFC-style keywords.

### Release Governance

- Keeps the release artifact contents unchanged from `spec/v0.8.3`.
- Uses this patch tag to publish the Chinese `specs/**` contract text without requiring generated artifact or registry changes.

### Runtime Impact

- Runtime and SDK teams do not need wire-level, registry, SDK API, or conformance behavior changes for this release.
- Tooling that validates protocol invariants can continue reading `specs/20-core.md` and `specs/40-codec.md`; the required invariant phrases remain present.
- No npm, pub, PyPI, Docker, or runtime package registry publish is part of this Spec release.

## spec/v0.8.3

Runtime dispatch compatibility verification patch.

### Protocol

- Keeps AXTP wire, CONTROL, RPC, STREAM, method, event, schema, capability, profile, and error semantics unchanged from `spec/v0.8.2`.
- Keeps the flattened numbered specs layout and machine contract paths introduced across `spec/v0.8.0` through `spec/v0.8.2`.

### Release Governance

- Publishes a fresh spec tag after C, Python, TypeScript, Flutter, and mock-server runtime generators were updated to consume `contract/**` and flattened `specs/**` paths.
- Uses this tag to re-run automatic `axtp_spec_released` dispatch across runtime repositories after the runtime-side workflow fixes.
- Keeps the release artifact contents unchanged from `spec/v0.8.2`.

### Runtime Impact

- Runtime and SDK teams can bind to `spec/v0.8.3` as the verified patch tag for cross-runtime dispatch compatibility with the current spec repository layout.
- Implementations already compatible with `spec/v0.8.2` do not need wire-level, registry, SDK API, or conformance behavior changes for this release.
- No npm, pub, PyPI, Docker, or runtime package registry publish is part of this Spec release.

## spec/v0.8.2

Runtime dispatch CI verification patch.

### Protocol

- Keeps AXTP wire, CONTROL, RPC, STREAM, method, event, schema, capability, profile, and error semantics unchanged from `spec/v0.8.1`.
- Keeps the flattened numbered specs layout introduced in `spec/v0.8.1`.

### Release Governance

- Publishes a fresh spec tag after cpp-runtime CI was updated to checkout runtime submodules for CMake checks.
- Verifies the automatic `axtp_spec_released` dispatch path against the cpp-runtime upgrade workflow after the flattened specs path fix.
- Keeps the release artifact contents unchanged from `spec/v0.8.1`.

### Runtime Impact

- Runtime and SDK teams can bind to `spec/v0.8.2` as the verified patch tag for the flattened spec layout and cpp-runtime dispatch path.
- Implementations already compatible with `spec/v0.8.1` do not need wire-level, registry, SDK API, or conformance behavior changes for this release.
- No npm, pub, PyPI, Docker, or runtime package registry publish is part of this Spec release.

## spec/v0.8.1

Specs readability and runtime dispatch verification patch.

### Protocol

- Keeps AXTP wire, CONTROL, RPC, STREAM, method, event, schema, capability, profile, and error semantics unchanged from `spec/v0.8.0`.
- Renames the formal human-readable specs into numbered flat files under `specs/`: `00-glossary.md`, `10-contract.md`, `20-core.md`, `30-registry.md`, `40-codec.md`, and `50-tooling.md`.
- Preserves CONTROL OPEN/ACCEPT, RPC, and STREAM examples in `specs/20-core.md` so runtime teams still have executable wire guidance after the docs slimming pass.

### Documentation

- Archives the previous multi-directory specs tree under `docs/archive/specs/legacy-structured-specs/**` for comparison instead of dropping the old material outright.
- Moves candidate registry planning material to `workspace/registry-planning/**`, keeping it out of the default reader path and release artifact.
- Updates active docs and tooling references from the old nested specs paths to the new `specs/*.md` contract paths.

### Release Governance

- Keeps the default release artifact focused on current consumer contracts and excludes archived specs and registry-planning workspace material.
- Validates the flattened specs layout through link checks, plain-text path checks, generated drift checks, conformance validation, and release artifact dry-run.
- Verifies the runtime upgrade path after fixing cpp-runtime tooling to consume `contract/protocol/axtp.protocol.yaml` and the flattened `specs/20-core.md` / `specs/40-codec.md` documentation facts.

### Runtime Impact

- Runtime and SDK teams should bind to `spec/v0.8.1` if they need the flattened `specs/**` layout or the verified cpp-runtime dispatch path.
- Implementations already compatible with `spec/v0.8.0` do not need wire-level, registry, SDK API, or conformance behavior changes for this release.
- Tooling that validates human-readable spec facts should read `specs/20-core.md` and `specs/40-codec.md`, with old nested paths treated only as historical archive material.
- No npm, pub, PyPI, Docker, or runtime package registry publish is part of this Spec release.

## spec/v0.8.0

Repository contract layout and release artifact boundary release.

### Protocol

- Keeps AXTP wire, session, RPC, STREAM, method, event, schema, and error semantics unchanged from `spec/v0.7.0`.
- Promotes the current machine-readable contract paths as the implementation authority: `contract/protocol/axtp.protocol.yaml`, `contract/generated/**`, `contract/registry/**`, `contract/mcp/**`, and `contract/test-vectors/**`.
- Keeps human-readable normative specs under root `specs/**` so runtime and SDK teams can separate implementation rules from maintainer workbench material.

### Registry

- Moves registry source facts to `contract/registry/**` and updates generator, validation, release, and workflow references to consume that path.
- Does not add, remove, or renumber adopted registry methods, events, capabilities, profiles, schemas, or error codes in this release.
- Updates protocol metadata references so generated artifacts point to the root `workspace/**` maintainer inputs rather than removed docs-hosted workspace paths.

### Schemas

- Refreshes generated protocol JSON/YAML metadata and generator snapshots for the new contract/workspace path model.
- Keeps generated Protocol IR structure and field semantics compatible with `spec/v0.7.0`; changes are path and release-boundary metadata only.
- Keeps MCP and test-vector artifacts inside `contract/` as part of the runtime-consumable release contract.

### Conformance

- Keeps conformance as a root-level testing entry point at `conformance/**`, independent from maintainer `workspace/**`.
- Adds plain-text path validation for maintainer draft and legacy evidence references so stale hidden paths fail before release.
- Extends protocol status validation to check generated and draft counts in `docs/product/domain-status.md`.

### Migration

- Moves maintainer workbench material to root `workspace/**`.
- Moves release operation documents to root `release/**`.
- Keeps historical audits in `docs/archive/**` and keeps legacy evidence in `workspace/legacy-migration/evidence/**`, but excludes those maintainer-only inputs from the default release artifact.

### Release Governance

- Tightens the default release artifact to include only `contract/`, `specs/`, `conformance/`, `docs/guides/`, `docs/product/`, root `release/`, and top-level release metadata.
- Excludes `workspace/business`, `workspace/flows`, `workspace/protocol`, `workspace/legacy-migration`, lifecycle skills, release templates, and legacy evidence from the published artifact.
- Updates release scripts, manifest template, CODEOWNERS, CI references, and lifecycle skill instructions to the new `contract/`, `tooling/`, `workspace/`, `conformance/`, and `release/` paths.

### Runtime Impact

- Runtime and SDK teams should bind to `spec/v0.8.0` when they need the new release artifact layout and root `conformance/**` packaging.
- Implementations already compatible with `spec/v0.7.0` protocol facts do not need wire-level or API behavior changes for this release.
- Tooling that previously consumed `docs/generated/**`, `protocol/axtp.protocol.yaml`, `registry/**`, `tooling/mcp/**`, or `tooling/test-vectors/**` from the spec repository must update to the `contract/**` paths.
- No npm, pub, PyPI, Docker, or runtime package registry publish is part of this Spec release.

## spec/v0.7.0

Release contract, generated artifact governance, and Protocol IR fidelity release.

### Protocol

- Promotes capability metadata into top-level Protocol IR so runtimes and SDK generators can consume capability ids, status, type, and schema bindings directly.
- Preserves field defaults, enum values, schema references, repeated metadata, and array item typing in generated Protocol IR and JSON references.
- Converts generated-domain JSON-array fields from opaque `bytes` placeholders into first-class `array<T>` schema facts for device, firmware, network, audio stream, and video stream contracts.

### Registry

- Updates adopted domain YAML sources to express object arrays and scalar arrays with `type: array` plus `schema` or `array.item_type`.
- Adds source validation for array item type references so malformed array declarations fail before generated artifacts drift.

### Schemas

- Refreshes `contract/protocol/axtp.protocol.yaml`, `contract/generated/protocol.md`, `contract/generated/protocol.json`, MCP schema output, and generator snapshots from the richer contract/registry/source model.
- Keeps the remaining opaque `bytes` field only where the value is intentionally a polymorphic JSON selector rather than a simple array.

### Conformance

- Replaces loose conformance case validation with AJV-backed JSON Schema validation for case and result files.
- Adds cross-reference checks for manifest levels, profiles, fixtures, methods, events, errors, and capabilities.
- Keeps existing conformance behavior intact while making case authoring failures fail in CI.

### Release Governance

- Defines the release artifact as an explicit runtime-consumable contract containing `contract/protocol/`, `contract/generated/`, `contract/registry/`, `specs/`, `conformance/`, `contract/mcp`, `contract/test-vectors`, and `LICENSE`.
- Excludes `.DS_Store`, legacy evidence files, and repository-only lifecycle skills from the published archive.
- Adds generated drift, Markdown link, protocol status, and release artifact dry-run checks to CI and release validation.
- Adds repository governance entry points: `LICENSE`, `CONTRIBUTING.md`, `SECURITY.md`, and `CODEOWNERS`.

### Documentation

- Updates the protocol domain matrix to match generated method/event counts.
- Fixes generated protocol draft status text for adopted device, network, audio stream, video stream, and firmware update features.
- Fixes local Markdown links and clarifies renamed or merged draft paths.

### Runtime Impact

- Runtime and SDK teams should bind to `spec/v0.7.0` and regenerate from `contract/protocol/axtp.protocol.yaml` or `contract/generated/protocol.json`.
- Generators should treat `array<T>`, field defaults, enum values, and top-level capability records as authoritative.
- Implementations that previously treated generated JSON-array fields as opaque bytes should update bindings to typed arrays where the new IR declares `type: array`.
- No npm, pub, PyPI, Docker, or runtime package registry publish is part of this Spec release.

## spec/v0.6.1

Core session field cleanup and CONTROL negotiation clarification release.

### Protocol

- Makes RPC `axtpVersion` the AXTP v1 compatibility authority and demotes `rpcVersion` / `negotiatedRpcVersion` to rc1 transition compatibility only.
- Adds required Identify `randomSeed:uint32` so devices without reliable clocks or hardware RNG can mix caller-provided entropy into RPC `sid` generation.
- Clarifies CONTROL `protocolVersion` as Deprecated/Transition and keeps Frame Header `Version` plus RPC `axtpVersion` as the primary version boundaries.
- Clarifies CONTROL sizing: `maxFrameSize` is the complete Standard Frame size limit, while `maxPayloadSize` is deprecated/reserved and `mtu` is profile-specific optional.

### Registry

- Updates core RPC op descriptions and protocol lifecycle metadata to reference `randomSeed` and AXTP version semantics.
- Updates CONTROL schema facts so `protocolVersion` is optional/deprecated, `mtu` is optional, and `maxFrameSize` has the minimum needed for the smallest CONTROL payload.

### Schemas

- Refreshes `contract/protocol/axtp.protocol.yaml`, generated protocol references, MCP schema output, and generator snapshots from the updated registry and schema facts.
- Keeps deprecated transition fields represented in generated artifacts for compatibility while removing them from new minimal handshake examples.

### Conformance

- Updates session and event conformance cases to require Identify `randomSeed:uint32`.
- Refreshes guides and testing quickstarts so WebSocket JSON and Standard Framed examples use the current Hello / Identify / Identified shape.

### Migration

- Updates legacy migration planning notes and cast receiver flow examples to use `randomSeed` and the revised RPC session startup shape.
- Leaves historical legacy evidence unchanged.

### Runtime Impact

- Runtime and SDK teams should bind to `spec/v0.6.1`, accept missing `rpcVersion` in AXTP v1 handshakes, require `randomSeed:uint32` in Identify, and generate `sid` by mixing that seed with local state.
- Standard Framed runtimes should treat `maxFrameSize` as the peer receive limit for complete frames and should not require CONTROL `protocolVersion`, `maxPayloadSize`, or core `mtu` in new v1 handshakes.
- No npm, pub, PyPI, Docker, or runtime package registry publish is part of this Spec release.

## spec/v0.6.0

Business registry expansion and generated protocol refresh release.

### Protocol

- Expands the generated Protocol IR from the previously adopted audio algorithm surface to 38 methods, 17 events, 154 errors, 135 schemas, and 2 profiles.
- Generates newly adopted business protocol surfaces for `device.info`, `network.interface`, `network.ip`, `network.wifi`, `network.ap`, `audio.stream`, `video.stream`, and the P0 subset of `firmware.update`.
- Refreshes `contract/protocol/axtp.protocol.yaml`, `contract/generated/protocol.md`, `contract/generated/protocol.json`, generated registry markdown, MCP JSON registry artifacts, and generator snapshots from contract/registry/domain YAML sources.
- Keeps AXTP core wire semantics unchanged from `spec/v0.5.1`, including Big-Endian / network byte order.

### Registry

- Adds domain registry source files for `device`, `network`, `video`, and `firmware`.
- Extends the existing `audio` domain registry with `audio.stream` methods, events, schemas, and capability metadata.
- Marks the adopted protocol drafts as generated while preserving unresolved `[REVIEW-ASK]` items as non-contract review notes.
- Keeps legacy mappings unadopted where field-level behavior remains unconfirmed.

### Schemas

- Adds generated schema coverage for device information snapshots, network interface/IP/Wi-Fi/AP configuration and state, audio/video stream control, stream state events, and firmware update sessions.
- Adds capability descriptors for `device.info`, `network.interface`, `network.ip`, `network.wifi`, `network.ap`, `audio.stream`, `video.stream`, and `firmware.update`.
- Keeps advanced firmware update flows such as cancel, resume, URL update, rollback, and external verify/install out of the generated P0 contract.

### Conformance

- Updates generator tests and protocol snapshots so validation tracks the expanded generated protocol model.
- Source and generated protocol validation now pass against the 38-method / 17-event registry model.
- No new hand-written conformance cases are added in this release; domain-specific conformance suites should follow the generated facts.

### Migration

- Preserves legacy review notes in the adopted protocol drafts but does not promote unconfirmed legacy aliases into stable registry mappings.
- Keeps adapter-only or uncertain legacy behavior outside the generated implementation contract.

### Runtime Impact

- Runtime and SDK teams should bind to `spec/v0.6.0`, regenerate from `contract/protocol/axtp.protocol.yaml` or `contract/generated/protocol.json`, and update method/event/capability registries accordingly.
- Existing `spec/v0.5.1` core wire behavior remains compatible; this release primarily expands generated business registry facts.
- Implementations should treat generated `draft` business methods as adopted registry facts for this snapshot, while using capability discovery before invoking optional device/network/stream/firmware features.
- No npm, pub, PyPI, Docker, or runtime package registry publish is part of this Spec release.

## spec/v0.5.1

Wire byte order metadata and runtime release hotfix.

### Protocol

- Adds machine-readable `wire` metadata to the Protocol IR with `byteOrder: big-endian`, `byteOrderAlias: network`, and `crcByteOrder: big-endian`.
- Emits the same wire byte order facts into `contract/generated/protocol.json` and `contract/generated/protocol.md`.
- Updates generator validation so Protocol IR and core specs must continue to agree on Big-Endian / network byte order.

### Conformance

- Updates Standard Framed test vectors to use Big-Endian / network byte order for frame length, message id, RPC method/event ids, and STREAM header fields.
- Adds a STREAM conformance byte-order assertion for `streamId:uint32`, `seqId:uint32`, and `cursor:uint64`.

### Runtime Impact

- Runtime repositories should bind to `spec/v0.5.1` rather than `spec/v0.5.0` for Standard Framed binary work.
- Runtime wire codecs must serialize and parse AXTP multi-byte wire integers using Big-Endian / network byte order.
- Runtime vendored generator templates and generated protocol references should no longer mention Little-Endian for AXTP wire integers.

## spec/v0.5.0

Documentation governance, business protocol draft consolidation, and AXTP network byte order alignment release.

### Protocol

- Establishes a clearer repository narrative and role-based documentation entry points for first-time readers, runtime/SDK implementers, conformance owners, protocol maintainers, product/architecture reviewers, legacy migration owners, and release owners.
- Adds a shared AXTP glossary and refactors `specs/**` into a more normative core/contract/registry/codec/tooling specification backbone.
- Rewrites core runtime specs for Frame/Payload, Transport Profiles, CONTROL, RPC, STREAM, and low-bandwidth degradation to separate required v1 behavior from optional/future behavior.
- Aligns AXTP multi-byte wire fields to Big-Endian / network byte order across specs, guides, and STREAM draft examples. Runtime implementations that followed earlier Little-Endian draft text must update frame, CONTROL, RPC JSON_BINARY, TLV, STREAM, and CRC byte handling.
- Updates flow and protocol-draft templates so business flows identify protocol coverage/gaps, and domain-feature drafts use readable method/event overview plus per-item schema expansion.
- Adds or refines draft business protocol documents for device information, system state/lifecycle/reset, device enrollment/child devices, signage management, software config/update policy, firmware update policy, and audio/video stream-related drafts.

### Registry

- No registry YAML source facts are changed in this release.
- Keeps adopted runtime contract facts sourced from `contract/registry/**`, `contract/protocol/axtp.protocol.yaml`, and `contract/generated/**`.
- Moves historical registry planning tables into non-normative appendix documents under `workspace/registry-planning/candidates/**`; runtimes must not implement from these appendix tables.
- Clarifies domain.feature naming, method/event/error/profile allocation rules, draft ID boundaries, and registry adoption readiness criteria.

### Schemas

- No adopted generated schema contract changes are introduced by this release.
- Refines protocol draft schema presentation for audio.algorithm, audio.eq, audio.volume, device.info, system.state, system.lifecycle, system.reset, signage, software, and related draft modules.
- Clarifies that draft methodId, eventId, errorCode, capabilityId, and fieldId values remain registry-assigned or intentionally unassigned until adoption.

### Conformance

- Expands conformance documentation into a clearer main entry, runtime checklist, support-level guidance, failure classification, and Phase 1 MVP expectations.
- Adds audit material and module review checklists for future business-domain conformance planning.
- Existing conformance cases remain the source for runtime validation; draft business protocol documents do not become conformance requirements until adopted and generated.

### Migration

- Adds and updates flow documents for device/system, signage device management, cast receiver/pairing, firmware update, audio/video streaming, and audio algorithm control scenarios.
- Preserves legacy migration evidence while moving new protocol decisions into flows and protocol drafts.
- Clarifies that legacy aliases and adapter-only behavior must not pollute formal domain.feature boundaries.

### Runtime Impact

- Runtime and SDK teams should bind to `spec/v0.5.0`, the exact commit, or the generated release artifact; they must not depend on floating `main`.
- Standard Framed binary runtimes must treat AXTP multi-byte wire integers as Big-Endian / network byte order for new work against this release.
- WebSocket JSON behavior remains RPC-oriented; CONTROL and STREAM remain Standard Framed concerns.
- Business protocol documents under `workspace/protocol/**` are review drafts unless backed by registry YAML and generated artifacts.
- No npm, pub, PyPI, Docker, or runtime package registry publish is part of this Spec release.

## spec/v0.0.4

Spec repository purification, documentation workflow, and Phase 1 STREAM data-plane alignment release.

### Protocol

- Reorganizes the main repository as a protocol-first spec repository with root runtime leftovers removed and runtime source/docs moved out of the AXTP spec tree.
- Groups the formal `specs/` corpus into core, registry, codec, and tooling sections to make the protocol reading path easier to follow.
- Aligns Phase 1 CONTROL scope to `OPEN / ACCEPT`, `HEARTBEAT / HEARTBEAT_ACK`, and `CLOSE / CLOSE_ACK`; ACK/NACK remains reserved for future reliable-transfer profiles.
- Aligns Standard Framed transports to support CONTROL, RPC, and Phase 1 STREAM data packets for adopted audio/video media stream profiles.
- Clarifies RPC encoding order and semantics: `JSON`, `CBOR`, `MSGPACK`, and `JSON_BINARY`; `bodyEncoding` applies only to `JSON_BINARY` with `NONE`, `TLV8`, and `TLV16`.
- Adds and refines onboarding docs including `KICKOFF.md`, `ROADMAP.md`, core protocol flow, quickstart, and runtime MVP/conformance guidance.

### Registry

- Updates protocol metadata, transport capability declarations, control opcode status, RPC encoding facts, body encoding facts, and `protocol.payload.stream` capability status to match the Phase 1 decisions.
- Refreshes `contract/protocol/axtp.protocol.yaml`, generated protocol references, generator snapshots, and MCP/tooling registry outputs from the YAML source facts.
- Keeps adopted business methods limited to the current generated `audio.algorithm` surface while documenting that video/audio stream business methods must be adopted before becoming a published runtime contract.

### Schemas

- No breaking schema shape changes are introduced for adopted business methods.
- Refreshes generated schema references and snapshots as part of the source-to-generated validation pipeline.
- Clarifies `sid`, `requestId`, `rpcEncoding`, `bodyEncoding`, and STREAM header typing in the core specs and generated references.

### Conformance

- Moves conformance into the `docs/conformance/**` documentation hierarchy and keeps release artifacts compatible by packaging conformance at the artifact root.
- Updates conformance manifest and framed-binary/stream profiles so Standard Framed runtime MVP covers CONTROL open/heartbeat/close, RPC request matching, and media STREAM open/data/close semantics.
- Revises STREAM conformance cases to model media stream lifecycle through business-domain methods such as `video.openStream` / `video.closeStream` instead of requiring generic `stream.open` / `stream.close`.
- Keeps ACK/NACK strict retransmission outside Phase 1 conformance requirements.

### Migration

- Consolidates legacy evidence, classification, plans, generated outputs, and planning docs under `workspace/legacy-migration/**`.
- Updates legacy classification and migration tooling paths to the new docs layout.
- Preserves legacy migration guidance while keeping legacy adapters outside the AXTP Core wire contract.

### Runtime Impact

- Standard Framed runtimes should treat STREAM as a Phase 1 P0 data-plane requirement for adopted audio/video media stream profiles.
- WebSocket JSON remains RPC-only and does not carry CONTROL or STREAM payloads.
- Runtime conformance scripts should prefer `$AXTP_SPEC_PATH/docs/conformance` and may retain `$AXTP_SPEC_PATH/conformance` fallback for release artifact compatibility.
- Runtime and SDK teams should continue to bind to a fixed `spec/v0.0.4` tag or commit through their spec lock metadata; no runtime package release is part of this Spec release.

## spec/v0.0.3

Conformance foundation release for AXTP Spec and runtime/tool repositories.

### Protocol

- No wire protocol fact changes from `spec/v0.0.2`.
- Keeps current `spec/v0.0.2` method and event names, including `audio.getAlgorithmConfig`, `audio.setAlgorithmConfig`, `audio.getAlgorithmCapabilities`, and `audio.algorithmConfigChanged`.
- Keeps capability discovery out of the wire-level conformance surface because current protocol docs reserve `capability.getAll` for v2/P1.

### Registry

- No registry ID or method/event fact changes from `spec/v0.0.2`.
- Treats capability conformance as generated registry and capability metadata validation against the existing registry facts.

### Schemas

- Adds conformance case and result JSON schemas under `docs/conformance/schemas/`.
- Keeps existing protocol and business schema facts unchanged.

### Conformance

- Adds the first AXTP conformance manifest, profiles, fixtures, and YAML case suite.
- Adds 22 conformance cases covering core RPC/error behavior, session gating, framed binary handshakes, WebSocket JSON-RPC, events, streams, and capability metadata.
- Adds `tooling/scripts/validate-conformance.sh` and a `Validate conformance cases` CI workflow to validate case IDs, manifest references, profile coverage, schemas, and result shape.

### Migration

- No migration content changes from `spec/v0.0.2`.
- Existing legacy migration boundaries continue to apply.

### Runtime Impact

- Runtime/tool repositories should consume the shared `docs/conformance/` directory from the locked AXTP Spec checkout instead of defining their own case sources.
- Native runtime conformance runners should emit `conformance-results/result.json` using the shared result schema and explicitly report unsupported or skipped cases.
- No npm, pub, PyPI, Docker, or runtime package registry publish is part of this Spec release.

## spec/v0.0.2

Automation validation release for the AXTP Spec to runtime/tool upgrade flow.

### Protocol

- No protocol fact changes from `spec/v0.0.1`.
- Uses the current generated AXTP protocol snapshot to validate release artifact packaging.

### Registry

- No registry fact changes from `spec/v0.0.1`.
- Reuses the current method, event, error, capability, profile, and domain registry facts.

### Schemas

- No schema source changes from `spec/v0.0.1`.
- Reuses the current generated schema descriptors and metadata.

### Conformance

- Validates that the release workflow can build the Spec artifact and dispatch runtime/tool upgrades.
- Validates that runtime/tool repositories can regenerate artifacts from the released Spec tag.

### Migration

- No migration content changes from `spec/v0.0.1`.
- Reuses the current legacy classification and migration planning material.

### Runtime Impact

- Exercises the automatic `spec/v0.0.2` to runtime/tool `v0.0.2` synchronization path.
- Expected runtime/tool behavior is automated upgrade PR creation, generated manifest refresh, auto-merge after checks, `v0.0.2` tag creation, and GitHub Release creation.
- No npm, pub, PyPI, Docker, or runtime package registry publish is part of this Spec release.

## spec/v0.0.1

Initial tagged AXTP Spec release from the current repository snapshot.

### Protocol

- Publishes the current AXTP protocol specs as the first immutable Spec tag.
- Establishes AXTP v1 core wire documentation, transport profiles, RPC/CONTROL/STREAM specs, and compatibility/versioning guidance as the baseline documentation set.

### Registry

- Publishes current registry facts for core protocol metadata, RPC operations and encodings, control opcodes, errors, schemas, capabilities, profiles, and adopted domain registry entries.
- Keeps generated protocol facts sourced from `contract/registry/**/*.yaml` and `contract/registry/domains/**/*.yaml`.

### Schemas

- Publishes current generated schema descriptors and schema metadata for adopted AXTP methods, events, capabilities, and shared types.
- Keeps generated protocol outputs as machine-consumable artifacts derived from the YAML source facts.

### Conformance

- Publishes current generator tests, generated snapshots, and tooling test vectors as the baseline conformance material.
- Includes current C++/TypeScript/Flutter runtime generated references present in the repository snapshot.

### Migration

- Publishes current legacy classification and migration planning material for AXDP HID, Rooms, VM33, signage, firmware, and other legacy protocol mapping work.
- Keeps legacy compatibility behavior outside AXTP Core by documenting adapter and migration boundaries.

### Runtime Impact

- Establishes `spec/v0.0.1` as the first fixed AXTP Spec dependency target for runtime repositories.
- Runtime packages should record this tag through `AXTP_SPEC.lock.yaml` or package metadata instead of depending on `main`.
- No runtime package release is created by this Spec release.
