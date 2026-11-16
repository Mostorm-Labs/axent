# P1 settings control flow

Reviewed combined scope, 2026-10-20. [Normative settings contract](../../specs/46-cast-settings.md)
and [adoption](../protocol/cast/cast.presentationSettings.md) own message semantics.

## S1: Discovery and two-barrier synchronization

Subscribe P0 participating events and presentationSettingsChanged. Discover the
optional descriptor and all method/event/status bindings. Request unifiedControl
and presentationSettings explicitly. Capture both at one host commit boundary,
install both models, discard each stream's events through its own barrier.
Do not compare the two sequence spaces. A P0-only request/event model stays valid.
An early settings event waits for appliedControlRevision in P0 or a new combined
snapshot. Missing/invalid fields, gaps, overflow or epoch replacement resynchronize.

## S2: One-field command and owner ordering

```mermaid
sequenceDiagram
  participant C as Controller
  participant H as Control service
  participant O as Settings owner
  participant S as Substitute or host store
  participant P as Policy owner
  C->>H: setPresentationSettings(identity, expectedRevision, one-field patch, persist)
  H->>H: Epoch/shape/permission, then same-intent dedup
  H-->>C: Admitted queued operation with settingsResult
  H->>O: Recheck revision, capability and lock
  alt stale or locked
    O-->>H: Rejected, no configuration mutation
  else persist=true
    O->>S: Reserve and save only this field
    Note over H,P: P0 commands, capture and media continue
    alt storage failure or external change
      S-->>O: Failed or changed store; retain current policy
      O-->>H: Truthful failure result, refreshed diagnostic/store observation
    else stored
      S-->>O: New known store projection/revision
      O->>O: Commit configured field and storage facts
      O->>P: Apply if live; capture structured outcome
    end
  else persist=false
    O->>O: Commit configured field only
    O->>P: Apply if live, otherwise compute pendingRestart
  end
  O-->>H: Complete settings state and per-stage result
  H-->>C: Independent settings event; RPC/query terminal result
```

No save failure may modify desired/applied values or call a live setter. Store
success followed by apply failure retains saved/desired facts and reports partial
failure without reverse-mode compensation. Same configured value with unapplied
policy can be explicitly retried under a new key/latest revision. Same confirmed
mode does not reset per-session selection. Restart-only writes change intent,
not sessions/slots/graphs. Reverting to activeStartup clears pendingRestart even
when applied capacity is limited by capability.

## S3: Loss, store conflict and exit

Lost response → same original command/query, dedup before revision comparison.
Same key changed intent → INVALID_ARGUMENT. Another queued commit → revision
conflict. Another writer/store edit → BUSY or SETTINGS_STORE_CHANGED without
overwriting foreign data. Invalid/unknown-version stores are preserved. Crash
after save can load the saved field in a fresh epoch; old results are unknown and
are never automatically replayed. Shutdown rejects new writes and fences late
store callbacks. No file watch, exit-time implicit save, receiver observation or
automatic restart is introduced. PS-01–PS-12 instrument these host boundaries;
real disk/media implementation and product acceptance remain downstream.
