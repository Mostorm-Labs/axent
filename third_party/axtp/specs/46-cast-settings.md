# Cast presentation settings v1

This optional P1 contract extends [P0 control](45-cast-control.md) through
`cast.presentationSettings`, still using controlVersion=1. Registry shapes and
IDs come from [cast domain](../contract/registry/domains/cast/domain.yaml).
Review inputs are the revised r1 §9 and UNIFIED_PRESENTATION_SETTINGS_PLAN.md
(2026-10-20). Their earlier P0-only scope is superseded by the explicit combined
P0/P1 implementation request. All new registry entries remain draft until a
separate maturity/release decision. No automatic restart interface is introduced.

The optional [meeting extension](48-cast-meeting-presentation.md) adds five separately advertised preference keys. The seven base keys and transaction rules below use the [current control refresh](49-cast-control-refresh.md).

## Surface and capability conditions

setPresentationSettings accepts CastSetPresentationSettingsParams and returns
CastControlOperation. It requires presentationSettings and controlOperations;
getControlOperation recovers its original result. presentationSettingsChanged
carries a complete CastPresentationSettingsState and independent settingsContext.
getStatus appends optional presentationSettings; getControlCapabilities appends
optional presentationSettings capability. Supporting P1 requires all these
bindings, the descriptor and status section, not merely a method name.

The seven base setting keys are layoutMode, contentAdaptation, audioControlMode,
audioMixMode, capacity, enabledProtocols and profileHint. Patch MUST contain exactly one key, including
when its value equals the configured value. Empty, multi-key, unknown-key and
wrong-type patches return INVALID_ARGUMENT. persist is a required boolean.
No request implicitly saves another field's temporary modification. Global
settings operations have no presentation target.

Capability fields is a unique subset of these keys. liveFields and
restartOnlyFields MUST be disjoint, unique and together equal fields. The
partition declares behavior; AXTP does not make NearCast's first policy global.
StartupSettings permits all supported optional keys, but activeStartup MUST contain
exactly the supported restart-only keys. NearCast's first profile declares layout, content adaptation
and audio modes live, capacity/protocol/profile restart-only. Another host may
declare a different partition without inventing a new startup schema.

Conditional descriptor requirements:

| Supported key | Required declaration and validation |
|---|---|
| capacity | Nonempty unique positive uint32 capacityChoices; requested value must be a member. |
| layoutMode | Nonempty unique layoutModes containing grid and/or presentation. |
| contentAdaptation | Nonempty unique contentAdaptations containing fixed and/or auto-fit. |
| profileHint | Nonempty unique profileHints of nonempty opaque identifiers (max 64 bytes); these are product profiles, not wire/header profiles. |
| enabledProtocols | Required uint32 minEnabledProtocols, no larger than the supported presentation.protocols set. Values are a duplicate-free subset of hid/airplay/webcast advertised there. Empty requires an explicit minimum of 0 and a host capable of keeping control/settings available independently. |
| audioControlMode / audioMixMode | Modes come from audioPolicy.controlModes / mixModes; do not infer omitted modes from defaults. |

Unsupported capability/key/mode returns NOT_SUPPORTED. A supported numeric or
protocol-set constraint violation returns OUT_OF_RANGE. Unknown enum values are
unsupported modes; malformed types remain INVALID_ARGUMENT. Current deployment
locks are reported through lockedFields and field issues and rechecked at
execution. A locked write returns INVALID_STATE / SETTINGS_LOCKED. AXTP does not
declare historical CLI arguments permanently locked or prescribe startup priority.
No product capacity list, filesystem path or default profile is a global limit.

## Settings facts and revisions

| State field | Required meaning |
|---|---|
| settingsRevision | Canonical decimal uint64 string, monotonic within runtimeInstanceId, used for optimistic concurrency. |
| settingsEventSequence | Independent publication sequence/barrier, canonical decimal uint64 string. |
| configured | Current desired values, exactly the supported keys, including temporary and pending values. |
| applied | Confirmed adopted policy/configuration values; unknown fields absent with applicationIssues, never filled from desired values. Audio policy is not proof of sound. |
| activeStartup | Restart-only intent accepted for this running combination after explicit startup overrides; not effective capacity or the previous file. |
| pendingRestart | Exactly restartOnlyFields where configured differs from activeStartup. |
| persistedValues | Optional complete known startup projection for supported fields; omit when unknown/invalid/unavailable. |
| persisted | True iff configured equals known persistedValues; false when unknown. It does not claim a disk write occurred. |
| unsavedFields | Supported fields differing from known persistedValues; all supported keys when stored values are unknown. |
| unappliedFields | Live fields differing from confirmed applied policy; absent applied values also count. Layout animation progress does not count. |
| storageState | absent / loaded / saved / invalid / unavailable. |
| storeRevision | Optional canonical decimal uint64 valid store version; absent without a valid known file. |
| lockedFields | Current unique subset of supported keys that cannot be changed. |
| applicationIssues | Field issues: limitedByValidation / limitedByHardLimit / liveApplyFailed / startupFailed / locked / unknown. Reason is a bounded safe summary, statusCode an optional standard numeric code. |
| appliedControlRevision | Optional P0 policy revision lower bound associated with confirmed adopted policy, not the settings version. |
| persistError | Optional bounded sanitized storage diagnostic, with no raw request, credential or private path. |

All collections are bounded by advertised request/snapshot/event limits. Fields
not supported by this instance are omitted from Values; every present key must
be supported. Comparison of protocol arrays is set-based; request ordering does
not create a different intent. Absent unknown values are not JSON null.
Revision/sequence/store fields use the existing P0 decimal-string and uint64
rules in JSON and TLV UTF-8, including >2^53; counters never wrap in an epoch.

settingsRevision advances when configured/applied/persistence/locks/issues facts
are committed. A genuine no-op with the same configuration and storage metadata
does not advance it. Saving metadata or a new sanitized failure observation can
advance it without changing configuration. Publishing a cursor, counting media
frames, temporary per-session audio selection or layout interpolation does not
advance it. A restart uses a fresh runtime UUID; versions are not comparable
across epochs.

For example, desired capacity 9, activeStartup 9 and confirmed applied capacity 4
with a limitation issue MUST NOT leave capacity in pendingRestart. Changing the
desired value to 2 does require restart. Unknown validated capacity still follows
P0 validationAvailable/optional validatedCapacity; it does not falsify the current
running applied capacity. Adopted protocol enablement does not prove receiver Ready.
P0 presentation.configuredCapacity remains the running runtime's configured input;
P1 configured.capacity can instead be a next-start intent while activeStartup
records what this runtime accepted. A combined snapshot preserves that distinction.

With no file a host may expose known build defaults as persistedValues with
storageState=absent and no storeRevision. Equality may make persisted=true, while
the operation's persistenceStatus still distinguishes an explicit write.

## Ordered mutation and partial results

The command carries runtimeInstanceId, clientInstanceId, commandId,
expectedSettingsRevision, one-field patch and persist. Validate epoch, shape and
authorization, then look up the same-intent dedup key **before** comparing the
expected settings revision. Same-key same canonical request returns the original
record, even after its own commit advanced the revision. The expected revision
and persist choice are part of intent equality. Same key with any different known
intent returns INVALID_ARGUMENT / COMMAND_ID_CONFLICT.

At the settings owner recheck expectedSettingsRevision, capability and locks,
including after queueing. A stale revision rejects with INVALID_STATE /
SETTINGS_REVISION_CONFLICT and the observed settings revision. The client resyncs
and requires a new explicit user intent; it cannot silently replace the expected
revision, issue a new key after timeout, or replay into a new epoch.

CastControlOperation.kind adds setPresentationSettings. settingsResult is optional
in the common shape and REQUIRED for this kind, including queued/rejected/failed.
The old per-session kinds still require their original exact target. Settings
records MUST omit target and ownerCleanup. settingsResult contains:

| Field | Values |
|---|---|
| field | The unique requested setting key. |
| settingsRevision | Most recently confirmed settings revision; terminal records carry the version at completion/check. |
| configurationStatus | notCommitted / unchanged / committed. |
| applicationStatus | notAttempted / unchanged / applied / pendingRestart / failed. |
| persistenceStatus | notRequested / pending / unchanged / stored / failed. |
| appliedControlRevision | Optional associated P0 policy revision. |

queued records before commit use notCommitted/notAttempted and pending for a
requested save, or notRequested. No new public executing state is added: progress
may remain queued. Terminal rules remain queued → applied/rejected/failed and
immutable terminal records with completedAt/statusCode/outcomeReason. P1 applied
means requested configuration and applicable save/live steps completed. A
restart-only applied result may report applicationStatus=pendingRestart; it does
not claim the software restarted. P1 settingsRevision never fabricates a P0
appliedRevision. Only an actual P0 logical policy commit may populate the latter.

persist=false:

1. Validate, then commit only this configured field.
2. Apply a live field through its existing owner, capture confirmed policy, or
   compute pendingRestart without touching current media for a restart-only key.
3. Report state/result and recompute unsaved/unapplied sets. Stored values stay.
4. On live failure retain truthful desired/applied differences; do not compensate
   by reversing audio mode, which could change selected sources.

persist=true:

1. Validate and reserve one settings write transaction. On a storage worker save
   only this field's explicit override; do not serialize the whole desired object.
2. After store success commit the desired/stored projections, then attempt live
   apply or report pending restart. No media/global host lock spans store waiting.
3. Store failure leaves configured/applied unchanged and does not invoke live
   application. Report failed/notCommitted/notAttempted/failed, with a sanitized
   failure fact which may independently advance settingsRevision.
4. Stored success followed by live failure reports failed/committed/failed/stored
   (or unchanged configuration if already equal), retaining the actual adopted
   policy and associated control version if known. No disk/media atomic transaction
   or reverse-policy rollback is promised.

Saving the same known override reports persistenceStatus=unchanged. Saving a
default-derived value without an explicit override still writes that field.
Same-value confirmed live policies MUST NOT reset audio selection or re-run a
mode setter. If configured already matches but an earlier live apply failed, a
new command with the latest revision MUST be allowed to retry the unapplied policy.
Layout adoption can succeed while layoutSettling=true. Static/silent content and
graph faults alone do not identify a setter failure: use structured owner results.

## Store, shutdown and failure boundaries

The host owns file schema, path, atomic replacement and single-writer protection.
Protocol conformance uses a substitute store. Before write, protected revalidation
of the known store identity/revision/values detects external edits; observing an
external change refreshes stored-state facts but rejects overwriting it with
INVALID_STATE / SETTINGS_STORE_CHANGED. Atomic replacement alone is not a
cross-process compare-and-swap. A second writer or occupied transaction returns
BUSY or waits within declared bounds. Invalid, unknown higher-version or unavailable
stores are preserved, not silently rewritten; read-only safe defaults may run but
cannot masquerade as recovered/saved contents. A storeRevision cannot be fabricated.

Disk-full, permission and replacement failures preserve the prior effective
store. Pending storage does not block P0 target operations, provider polling,
snapshots or media. Explicit shutdown seals new writes and fences late callbacks
from a new epoch. A crash after storage but before receipt can load the new value
at next startup; the old operation remains unknown across restart. TIMEOUT is not
cancellation. Temporary unsaved values are not automatically saved on exit.
Returning configured to activeStartup cancels pending restart without restarting
or clearing sessions. No file watch changes live policy automatically.

## Independent streams and combined snapshots

P0 unifiedControl still requires exactly its original six sections. P1-aware
clients subscribe presentationSettingsChanged before requesting
getStatus(controlVersion=1, include=[unifiedControl,presentationSettings]). Both
sections and their respective barriers MUST be captured at the same host commit
boundary. Unsupported requested P1 returns NOT_SUPPORTED; incomplete capture or
size failure returns UNAVAILABLE. A settings-only query is allowed and includes
the required original receiver/sampledAt plus the complete settings state.
It also returns controlSync for runtime/version identity; that does not implicitly
request the other P0 sections or merge the two stream barriers.

CastSettingsEventContext requires controlVersion, runtimeInstanceId,
settingsRevision and settingsEventSequence. Its revision/sequence MUST equal the
payload settings fields. Every settings event is a complete replacement. The
settings sequence starts at 1 (snapshot barrier may be 0), is contiguous among
published settings events and shared among subscribers in an epoch. Snapshot
capture does not advance it. Coalescing unpublished changes does not reserve IDs.
Use independent bounded pre-snapshot buffering, barrier discards and duplicate
handling. Gap, conflicting duplicate, overflow, unknown required version or epoch
change forces resync; old epochs do not replay commands.

Settings events and P1 operation progress MUST NOT consume P0 eventSequence.
P1 operation results are returned/queryable through RPC; the existing
controlOperationChanged stream remains the P0 per-session operation stream.
Actual live-policy changes still publish the relevant P0 feature commits. Storage
metadata alone does not synthesize media events. P0 clients ignore the new event
without a false gap. Full combined conformance additionally requires P1 cases;
the existing P0 conformance level remains independently claimable.

There is no cross-stream arrival order. A client displaying applied modes with
effective audio/presentation waits for its P0 model to reach appliedControlRevision,
or captures a new combined snapshot. Neither stream's sequence is compared with
the other's. A settings event can arrive before the corresponding P0 feature group.

## Errors and privacy

setPresentationSettings binds SUCCESS, NOT_SUPPORTED, INVALID_ARGUMENT,
INVALID_STATE, OUT_OF_RANGE, BUSY, UNAVAILABLE and INTERNAL_ERROR. Async failures
use standard numeric statusCode and bounded string reasons, not a new error space:
SETTINGS_REVISION_CONFLICT, SETTINGS_LOCKED, SETTINGS_STORE_CHANGED,
SETTINGS_PERSIST_FAILED, SETTINGS_LIVE_APPLY_FAILED, SETTINGS_SAVED,
SETTINGS_PENDING_RESTART, SETTINGS_ACCEPTED, COMMAND_ID_CONFLICT.

Configuration/state/events/issues/results/store projections MUST contain only
supported setting values and safe metadata. No private keys/paths, passwords,
PINs or their hashes, tokens, SDP/ICE, session keys, slot/binding/lifecycle targets
or temporary per-session enabled lists belong in settings. Existing authorized
legacy credential queries keep their prior contract. C01–C12 and PS-01–PS-12
separate protocol/store/policy evidence from physical product/media acceptance.
