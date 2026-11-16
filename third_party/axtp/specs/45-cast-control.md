# Cast multi-presentation control v1

The [unpublished control refresh](49-cast-control-refresh.md) replaces the earlier Layout semantics and adds aggregate legacy operations and secret-free AirPlay observations.

This is the P0 semantic contract for optional `cast.presentation`,
`cast.audioPolicy` and `cast.controlOperations`. Machine shapes and identifiers
come from [cast registry](../contract/registry/domains/cast/domain.yaml).
Repository review input: `workspace/business/cast-unified-control-r1.md`
(maintainer-only material, excluded from release artifacts).
Product admission, layout geometry, audio policy, media teardown and queue defaults
remain host responsibilities. This contract adds no transport, CONTROL opcode,
media encoding or persistent replay endpoint. Optional P1 settings are adopted in
[the settings contract](46-cast-settings.md), with an independent event stream.

## Discovery and versioning

Reuse the existing WebSocket Hello → Identify → Identified session and sid/op/d
envelope. Subscribe to all six participating events before capturing a snapshot:
receiversChanged, sessionsChanged, presentationChanged, audioPolicyChanged,
controlOperationChanged and windowChanged in domain cast. Legacy window events
without controlContext do not participate. The control connection and media links
are independent. Query getControlCapabilities with Empty params before selecting
a control version. Every other new method MUST specify controlVersion=1.

Capabilities are optional on all AXTP devices. A v1 implementation MUST declare
the actual methods, events, statusSections and dynamic limits it supports; a
partial descriptor MUST NOT be interpreted as full v1 synchronization support.
Full P0 requires all three features, cast.status and cast.window, all four new
methods, getStatus, the six events and six snapshot sections below. For v1 the
presentation descriptor MUST declare atomic snapshots and strict targets true;
audioPolicy MUST support per-session enable and report the effective set;
operations MUST support both query and dedup. Protocols, modes and layout IDs are
declared sets, not product defaults. capacityChoices, when supplied, contain only
positive capacities no greater than maxPresentationCapacity.

Every CastControlLimits value MUST be finite, nonzero and within uint32.
Admission MUST enforce global and per-client pending bounds, request size and
retained-result capacity. Snapshot bytes and buffered event counts/bytes MUST be
bounded by the declared limits, including when the session/receiver arrays grow.
No implementation may promise a snapshot larger than maxSnapshotBytes; capture
failure returns UNAVAILABLE. BUSY reports admission/backpressure without mutation.
replayWindowMs measures service-monotonic elapsed time from initial admission;
pending records MUST remain queryable and deduplicated until terminal, and terminal
records MUST remain until at least admission + replayWindowMs. Querying/retrying
does not reset that deadline. Result space is reserved before admission, and live
guarantees MUST NOT be silently evicted. Buffer overflow forces resynchronization
or an explicit control disconnect; it cannot block or restart media receivers.

`device.getInfo.capability.features` may advertise the feature names; it does not
replace this dynamic query. Unsupported features/versions return NOT_SUPPORTED,
never SUCCESS with an empty result. An unknown method still returns
RPC_METHOD_NOT_FOUND. Both degrade without closing a healthy RPC session.
spec tags (baseline spec/v0.17.0), protocol metadata (1.0.0 / wire 1), downstream
runtime releases and controlVersion=1 are independent version spaces.

## Identity and encoding

runtimeInstanceId, clientInstanceId and commandId MUST be UUID text (36 characters,
canonical lower-case 8-4-4-4-12 hexadecimal form). The host allocates a fresh runtime
UUID at each start. IDs are opaque and confer no authorization; never derive them
from credentials. Session/receiver/operation IDs MUST be nonempty UTF-8 strings
of at most 128 bytes. Display name, MAC and array position are not identities.

Every generation, revision, eventSequence and batchId is a **string** containing
`0` or a nonzero digit followed by decimal digits, at most 20 characters, numeric
value ≤ 18446744073709551615. Signs, whitespace, fractional/exponent syntax,
leading zeros and numbers in place of strings are invalid. Compare/increment with
uint64 precision, never JavaScript Number. JSON and existing binary object codecs
MUST preserve the string type; TLV uses UTF-8 string bytes under the schema-local
field ID, never an undocumented uint64 field. Counters MUST NOT wrap within an
epoch; before exhaustion the service must stop admission and establish a new
runtime epoch and fresh snapshot. Unknown optional fields retain existing skip
policy. This does not change the global uint64 JSON rules or invent nullable.

Times are bounded UTC RFC3339 strings (a trailing Z, optional fractional seconds).
They are display metadata, not a client-clock ordering/expiry oracle.
protocol is hid / airplay / webcast / unknown only in the new structures.
Existing protocol/backend enums retain their original values and semantics.

A complete exact target comprises params.runtimeInstanceId and target.session
(id/generation), slotId, bindingGeneration and lifecycleGeneration. The session ID
and generation belong to the shared runtime, not a protocol-local session. The
mutation owner MUST check all fences atomically immediately before changing state,
including after queueing. Removed sessions immediately invalidate their targets;
no occupied ghost slot may survive. Old-epoch commands MUST NOT be rewritten and
automatically replayed into a new epoch.

## Atomic snapshots and feature commits

getStatus appends optional controlVersion; old selectors keep their meaning.
`include: [unifiedControl]` with version 1 expands to controlSync, receivers,
sessions, presentation, audioPolicy and window. A supporting service MUST capture
and return this entire group atomically, including the original required receiver
and sampledAt. Failure to capture returns UNAVAILABLE. Unknown controlVersion or
unsupported group returns NOT_SUPPORTED. Individual new section selectors require
version 1; a v1 projection includes controlSync for its captured sections. No
selector rewrites the old session/audio/pinCode/backend/flowControl summaries.
Omitting controlVersion preserves the old contract. A new client MUST validate the
group and cursor even after SUCCESS, because old services may ignore new params.

controlSync is the capture revision R and event barrier S in runtime epoch E.
The snapshot includes every published commit through S and cannot split a commit
group. Commit capture and event publication/barrier assignment MUST be serialized
by the host. Physical provider observedAt values may differ; this is one logical
control capture, not a claim of simultaneous physical sampling.

Each new feature event contains its section's complete replacement after commit,
plus required context; operationChanged contains one record. windowChanged keeps
all its old required fields and adds optional controlContext. No statusChanged
event is introduced, and getStatus does not create continuous events.

Within an epoch eventSequence is contiguous across only participating events,
starting at 1 (snapshot barrier may be 0). One logical commit uses a single
revision and a batch of partCount≥1 events. batchId is its first sequence;
partIndex is 0..partCount-1, eventSequence=batchId+partIndex without overflow.
All parts share epoch/version/revision/batchId/partCount, with each affected
section represented once. Parts use increasing sequence order on the connection.
Operations may be published in a later independent batch referencing an earlier
appliedRevision; an operation-only update need not advance the presentation
revision. Sequence, not revision alone, orders publication. Revision never regresses.
Unpublished intermediate states may be coalesced without reserving sequence IDs;
an announced batch MUST NOT silently lose a part.

The client buffers within declared limits, installs a validated snapshot atomically,
discards same-epoch events ≤ S, then applies only complete groups from S+1.
Identical duplicate sequences are ignored. Conflicting duplicates, gaps,
out-of-order parts, incomplete groups (bounded wait), invalid references, unknown
required version or buffer overflow require a new snapshot. No partial group may
be exposed to the application. Epoch change discards old targets, buffered events
and unresolved operation caches and restarts discovery/sync. Reconnection does not
assume durable event history or undo media state.

## State consistency

- occupied=true requires target, matching slotId, bindingGeneration and current
  lifecycleGeneration. occupied=false omits target (null is invalid).
- Occupied targets reference current sessions; slot IDs and session references are
  unique in their collections. enabledSessions/effectiveAudioSessions are sets of
  current session references. Neither set proves audible output.
- configuredCapacity is positive. effectiveCapacity and appliedCapacity may be 0
  and are distinct from configured capacity, started and presence of media.
  validationAvailable=true requires positive validatedCapacity; false omits it.
  Layout mode is one of the capability's declared IDs. The protocol does not
  prescribe capacity-policy calculations or slot geometry.
- Session receiverState, presentationEligibility and audioReadiness describe
  distinct facts. Connection/authentication, no frames, a static image or silence
  do not imply presentation, disconnection or effective audio. lastError is
  historical; a retained error does not override a receiver's current ready state.
- Unknown optional readiness/capacity is absent; never fabricate false, zero or
  null as a validated observation. Old summaries must remain truthful to their
  old schemas, or explicitly report unsupported where applicable; never label
  WebCast as AirPlay/UxPlay.

## Commands and asynchronous records

AXTP request id pairs a single RPC response. (runtimeInstanceId, clientInstanceId,
commandId) deduplicates one user intent. operationId identifies an admitted
record, never a media session. Validate shape, supported version/capability and
epoch before dedup lookup. Same key and same parsed canonical command (including
method, target, enabled/reason and defined optional presence) MUST return the
original record without executing twice. JSON object key order and unknown
optional fields do not affect equality. Same key with different intent returns
INVALID_ARGUMENT / COMMAND_ID_CONFLICT. Never use a new key merely because a
response or event was lost. Authentication is host policy, independent of IDs.

Writes return SUCCESS + an admitted queued or terminal CastControlOperation;
queued means admission, not successful media effect. Synchronous validation,
stale runtime or full queues can fail before admission without creating a record.
Stale targets at the mutation owner produce a terminal rejected record with
INVALID_STATE / STALE_TARGET (or NOT_PRESENTED for an unoccupied presentation),
no appliedRevision and no mutation of any replacement. A pre-admission stale
rejection may instead use RPC INVALID_STATE. Per-session records MUST retain
their original target. P1 global settings records instead require settingsResult,
omit target and use their own settings version; see the settings contract.

Transitions are queued → applied | rejected | failed; terminals are immutable.
Each terminal MUST carry completedAt, standard numeric statusCode and bounded
outcomeReason. applied uses SUCCESS; rejected/failed use a non-success applicable
standard statusCode. A P0 logical commit requires appliedRevision, including a failed
partial execution; without commit it MUST be absent. Rejected means no mutation;
failed distinguishes owner error or partial execution and never pretends rollback.
The record may be terminal in the first response; its event may arrive first.

stopPresentation applied means the exact presentation was logically withdrawn and
the cleanup request handed to its protocol owner. ownerCleanup tracks only that
target's cleanup (notApplicable / pending / complete / failed), not receiver exit.
Terminal record immutability means the value is the cleanup observation at
terminalization; later cleanup state is observed through normal feature state,
not by rewriting the terminal record. Other sessions/shared graph remain intact.
An unavailable owner before mutation rejects/fails without commit; failure after
withdrawal is failed with appliedRevision / PARTIAL_EXECUTION and truthful cleanup.

setSessionAudioEnabled applies an explicit enabled boolean, never toggle. Repeating
the same value cannot reverse intent or reset mode. applied means intent committed;
the independently reported effective set may differ. Exclusive mode can alter
other audio participation, without stopping their sessions/video. Unsupported
modes/features return NOT_SUPPORTED and cannot be simulated as success.

getControlOperation queries the original dedup key in the same epoch. TIMEOUT is
not cancellation or rollback; NOT_FOUND means unknown outcome, especially after
retention expiry. After uncertainty, query the original operation and/or sync
current state; never infer not-executed from missing history. No cross-restart
exactly-once guarantee. Shutdown rejects new work, invalidates old epoch targets
and reports unresolved results as incomplete/unknown; teardown order is host-owned.

## Errors, privacy and compatibility

| Surface | Applicable RPC status names |
|---|---|
| getControlCapabilities | SUCCESS, NOT_SUPPORTED, UNAVAILABLE |
| precise writes | SUCCESS, NOT_SUPPORTED, INVALID_ARGUMENT, INVALID_STATE, BUSY, UNAVAILABLE, INTERNAL_ERROR |
| getControlOperation | SUCCESS, NOT_SUPPORTED, INVALID_ARGUMENT, INVALID_STATE, NOT_FOUND, UNAVAILABLE |
| getStatus | existing set plus NOT_SUPPORTED; UNAVAILABLE covers incomplete capture |

outcomeReason is a bounded business string, not a numerical error registry. P0
standard reasons: STALE_TARGET, NOT_PRESENTED, COMMAND_ID_CONFLICT, INTENT_APPLIED,
PRESENTATION_WITHDRAWN, OWNER_UNAVAILABLE, PARTIAL_EXECUTION, CONTROL_FAILED.
Unknown reason strings are display/diagnostic data; clients rely on state/code.
P1 settings reason strings and partial results are defined in 46-cast-settings.md.

New capabilities, snapshots, feature events and failures MUST NOT expose PINs,
passwords or their hashes, tokens, SDP/ICE, private keys or authentication secrets,
including through free-text names, lastError and outcomeReason. includeSensitive
does not authorize secrets in new sections. Existing authorized legacy PIN queries
retain their contract. Logs/diagnostics redact secrets as well.

All old IDs, bitOffsets, field IDs, requiredness and single-source semantics
remain. New fields are optional in old shapes and conditional only in the explicit
v1 projection. New clients show insufficient capability on old services rather
than inferring multi-session truth from activeProtocol/counts. New entries remain
draft; a release tag and downstream lock update require the release workflow.
