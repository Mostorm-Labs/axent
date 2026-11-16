# Touchback product control draft

The four former receiver-private input-policy/master routes are registered as
`cast.getInputMode` (0x1620), `cast.setInputMode` (0x1621),
`cast.getTouchbackMaster` (0x1622), `cast.setTouchbackMaster` (0x1623).
`cast.touchbackMasterChanged` is event 0x1616. These remain unpublished draft
facts. The retired 0x161E / method bit 29 is never reused.

These methods control receiver product policy; `input.openStream` negotiates
actual accepted types and STREAM carries input-sample-v1 bytes, unchanged.
Input mode is native or touch-to-mouse interpretation, never an on-wire mode.
Gesture scrollSpeed defaults to 1; physical wheel input does not use it.

New mutations require controlVersion 1 and the current runtimeInstanceId from
cast.getControlCapabilities. A stale runtime is rejected before side effects.
commandId is a nonempty bounded key, deduplicated for five minutes (1024 entries)
across touchback methods; changed method/content conflicts. A successful retry
returns current observed state without replaying the mutation. Beyond retention,
clients must query state and create a new command for new intent.

setInputMode requires at least one policy option and a live exact target. A
successful response can still report switching. cast.touchbackChanged invalidates
the queried input-policy state through its target revision. Query it again on
that event, reconnect or an unknown mutation outcome; never guess defaults.

Master state is runtime-scoped. Disabling stops sampling, clears queued input and
begins closing every owner. Success confirms the logical gate; target lifecycle
reports cleanup, including cleanup_blocked. Enabling does not restore target
permission or replay held input. Read cast.getTouchbackMaster on connection and
replace its state on cast.touchbackMasterChanged; reconnect always re-queries.
Only advertise supported methods/events. Receivers without these controls do not
pretend support. Removed private names are not standard aliases.
