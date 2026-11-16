# Cast control refresh (unpublished local development)

Adopted by explicit NearCast user acceptance on 2026-10-24. This replaces the
unpublished Layout/entry semantics in 45/46/48; no published release lock changes.
layoutMode is grid/presentation. contentAdaptation is fixed/auto-fit, independent
of layout and native fullscreen. presentationEnabled is removed. display values
are sidebar/grid/focus; meeting.view is omitted in Grid. Primary selection is
automatic/manual inside Presentation only. Complete source withdrawal returns
to Grid. Grid is not a view. presentationGridOnDoubleClick replaces the former
Split-named setting. NearCast defaults Presentation, Auto-fit, auto-entry on, Automatic primary
selection. The independent auto-entry preference and source-round suppression
are defined in 48-cast-meeting-presentation.md.

Untargeted legacy stopSession stops all connections existing at its finite owner
execution boundary, including authentication and data-only sharing requests.
It does not disable receivers or suppress genuinely later joins. An explicit
session selector never stops another session. Acceptance is not cleanup proof.
Untargeted setMuted is an independent global output gate that covers future
admissions and preserves per-session selection. Targeted audio keeps exact owner
validation. No input/readiness is not evidence of explicit mute.

Legacy lifecycle notifications are aggregate compatibility projections: the first
presentation starts the receiver, the last withdrawal stops it; an individual
failure/stop must not falsify the other presentations. New multi-session events
remain per-source and atomic. AirPlay authentication identities are distinct from
media sessions. Repeated password prompts use displayRequestId, never the secret
as identity. General multi-control snapshots/events/logs contain no credentials.
Explicit PIN queries and dedicated PIN events follow the credential channel below. Do not infer cancellation from ordinary connection teardown.

AirPlay rename is discovery-only and must preserve live media and reader lifetime.
The old name getter/setter use displayName and truthful publishState. Password
protection, capacity and protocol switches remain startup-only for this profile.
No implicit software restart, fixed-PIN setter or receiver persistence is added.

## Current usable code query and notification (2026-10-27 correction)

The existing cast.getPinCodeConfig, cast.pinCodeChanged and cast.pinCodeRequired
IDs are preserved. Optional protocol selects airplay (legacy default) or webcast.
includeSecret=true is canonical; includeSensitive=true remains a supported alias.
Default queries omit plaintext. Legacy cast.getStatus(includeSensitive=true) may
include its AirPlay pinCode section; unifiedControl/airplay/webcastBackend state
and events remain secret-free, even with includeSensitive.

AirPlay default scope=access returns the current prepared code even before any
authentication request. The challenges array retains each active request and its
exact attemptId/challengeId/displayRequestId and codeGeneration decimal uint64
strings. Optional attemptId selects scope=challenge and returns only that request. Consumption, replacement and
expiry clear the matching challenge and publish pinCodeChanged; every display
occurrence publishes pinCodeRequired. UI display leases do not expire a credential.
Transport loss is not proof of cancel, and must not rotate a retained password.

WebCast returns only the currently selected ready backend's code for new joins.
backend is cloud/local/none. Retained old-backend peers do not select the code.
Code rotation, availability loss/restoration, or backend switch publishes
pinCodeChanged with config.protocol=webcast, including a clear hasPinCode=false
when unavailable. No stale fallback code may be returned. Stop/start has no replay.

Dedicated PIN events may contain plaintext, carry redactionRequired=true and must
be redacted in diagnostic traffic, copy/export and persistent logs. An explicit
credential display may show the value in memory. Reconnect queries authoritative
current state; event delivery alone is not a durable credential history.

## Prepared AirPlay access code (explicit user acceptance, 2026-10-27)

Prepare at startup. A new request freezes the current access code and generation;
RTSP Digest and pre-verification pairing PIN use the same prepared generation.
Every actual media presentation admission rotates the access code once for future
requests. Authentication success alone, a connected transport, failed admission,
FLUSH, UI redisplay, duplicate observation or other protocols never rotate it.
There is no claim that an admitted buffer proves actual visual/audio quality.

Already pending requests retain their exact bound code, generation, nonce/SRP
state and original expiry across another source's rotation. Do not change them
in place. Failed-Digest limits and scoped retirement remain enforced. A bounded
failure-triggered private challenge replacement is distinct from the public
access-code generation. Generation identifiers never rely on the four digits
changing. Stale presentation identities cannot trigger another rotation.

scope=access pinCode is the code for new requests; scope=challenge pinCode is the
specified pending request's code. Query returns both the prepared access metadata
and active challenges when no selector is given. A retired challenge disappears
without making the still-available access code disappear. Rotation failure makes
new-join code unavailable, retaining already bound authentication/media ownership;
there is no fixed-password fallback. Dedicated pinCodeChanged publishes the scope
and codeGeneration. Merely preparing/changing a code must not open a password UI.
