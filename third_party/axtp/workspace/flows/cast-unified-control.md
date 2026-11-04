# Concurrent cast control — F1–F6

Reviewed P0 flow, 2026-10-20. [Business input](../business/cast-unified-control-r1.md),
[normative contract](../../specs/45-cast-control.md),
[presentation proposal](../protocol/cast/cast.presentation.md).
Actors: controller, AXTP RPC host, atomic control owner, protocol-specific media
owner. Only host/reference-model observations are protocol conformance evidence;
HWND, GStreamer and human listening tests belong to the product.

The combined P1 extension is adopted in
[settings flow](cast-presentation-settings.md); P0-only clients retain the same
six-section group and sequence. P1 clients request both projections explicitly
and use two independent snapshot barriers from one host commit boundary.

## F1: Connect and synchronize

```mermaid
sequenceDiagram
  participant C as Controller
  participant H as AXTP host
  participant O as Control owner
  C->>H: WS connect, Hello / Identify / Identified
  C->>H: Subscribe six participating cast events
  C->>H: getControlCapabilities({})
  H-->>C: versions, epoch, supported surfaces, bounded limits
  C->>H: getStatus(controlVersion=1, include=[unifiedControl])
  H->>O: Capture complete logical projection and barrier
  O-->>H: Epoch E, revision R, sequence S, six sections
  H-->>C: CastStatus with original required fields
  Note over C: Atomic replace; discard buffered sequence ≤ S
  H-->>C: Complete batches from S+1
```

Missing feature/partial capability/unknown version: NOT_SUPPORTED or client marks
insufficient support, with a healthy session. Missing any section/cursor despite
SUCCESS: do not install v1. Capture unavailable/too large: UNAVAILABLE. Bound
pre-snapshot buffering by advertised event count/bytes. No getPresentation method.

## F2: Feature batches, gaps and reconnect

One commit that changes sessions, presentation and audioPolicy emits three full
section replacements, same revision and batchId, partIndex 0/1/2, partCount 3,
sequences batchId..batchId+2. Controller exposes only the complete group.
Identical duplicate sequences are ignored; gap, reorder, conflicting duplicate,
missing part, overflow or invalid target reference discards the incomplete model
and restarts F1 capture. Window joins via controlContext; legacy events have no
sequence effect. Reconnect captures current joins/removals/generations even when
events were lost. Epoch change clears old targets and unresolved-operation caches;
never substitute a fresh epoch into old commands. Slow control consumers cannot
restart or block media. Operation-only batches may keep the current revision.

## F3: Withdraw one precise presentation

```mermaid
sequenceDiagram
  participant C as Controller
  participant H as AXTP host
  participant O as Mutation owner
  participant P as Target protocol owner
  C->>H: stopPresentation(version, epoch, client, command, target)
  H->>H: Validate; dedup; reserve result + bounded queue
  H-->>C: CastControlOperation(queued)
  H->>O: Apply original command
  O->>O: Atomic session/slot/binding/lifecycle fence check
  alt stale or not presented
    O-->>H: rejected, INVALID_STATE, no commit
  else target current
    O->>O: Withdraw exact presentation
    O->>P: Scoped cleanup request
    O-->>H: applied revision or truthful partial failure
  end
  H-->>C: Affected full feature batch; terminal operation batch
```

HID retention/parking, AirPlay scoped disconnect and WebCast leave/kick remain
product adaptations. No global stop, fake receiverPhase, graph teardown or cleanup
of other owners. Recheck after queueing so slot reuse cannot affect a new target.

## F4: Audio intent

setSessionAudioEnabled uses the same fences/admission flow, explicit enabled.
Commit selected intent; report enabled/effective sets independently. Same-value
commands remain idempotent. Exclusive participation changes are legitimate audio
policy; other video/session state stays intact. Unsupported per-session mode is
NOT_SUPPORTED. No claim of human-audible output follows from applied.

## F5: Loss, retry, expiry and pressure

Lost RPC response → same-key same-value retry returns the original record.
Event-before-response → correlate by command/operation identity, not arrival order.
Same-key changed method/target/intent → INVALID_ARGUMENT. Queue/retention full →
BUSY before mutation. Reserve result storage before accepting, retain all live
guarantees, and use host monotonic time. TIMEOUT → query original key or sync;
NOT_FOUND after expiry → unknown outcome. Never generate a fresh key to replay an
uncertain withdrawal. No infinite replay log or cross-restart exactly-once claim.

## F6: Shutdown and failures

Seal interaction and reject new commands; expose pending work as incomplete or
unknown. Invalidated epochs/targets cannot mutate a restarted owner. Partial
execution reports failed plus appliedRevision, unlike rejected with no mutation.
Protocol callback joins, providers, windows and shared-graph exit order are outside
the AXTP control contract. Do not leak secrets through any failure description.
