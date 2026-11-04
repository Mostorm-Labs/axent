# Optional WebCast signaling backend state and events

This additive feature is locally adopted by the 2026-10-21 implementation request.
Registry maturity remains draft; no spec release or publication is implied.
The machine source is [cast/domain.yaml](../contract/registry/domains/cast/domain.yaml).
This feature leaves legacy UxPlay backend contracts and P0/P1 unchanged.

| Surface | Assignment | Contract |
|---|---|---|
| cast.getWebCastBackendState | method 0x161A, bit 25 | CastGetWebCastBackendStateParams → CastWebCastBackendSnapshot |
| cast.webcastBackendChanged | event 0x1613, bit 18 | CastWebCastBackendSnapshot |
| cast.webcastBackend | capability 0x160C | CastWebCastBackendCapability |
| cast.getStatus include webcastBackend | optional CastStatus field 0x10 | Same complete snapshot and barrier |
| getControlCapabilities.webcastBackend | optional field 0x0B | Explicit feature discovery |

## State meaning

`state.selectedBackend` is the backend currently offered for **new joins**:
`none`, `cloud` or `local`. It MUST agree with the actual offered room/code.
It does not move or rename an existing connection. Both backend observations
remain visible when a product retains local peers after cloud recovery.
Cloud preference, fallback delay, retry, listener lifetime, media admission,
audio and UI remain product policy; this contract does not mandate them.

`state.state` is disabled/starting/ready/failed/stopped. Ready iff selection is
not none. A selected cloud requires cloud.state=ready and transportConnected;
a selected local requires running local service and ready, connected local
signaling. Ready means authoritative room availability, never socket-open alone.
Historical numeric errors do not override current readiness.

`retainedPeerCount` includes peers still negotiating. `connectedPeerCount`
counts retained peers with an open data/control channel and cannot exceed the
retained count. Neither count proves media presentation, audible output or an
uninterrupted playback interval. Signaling failure can coexist with working P2P.

Local serviceState is disabled/dormant/running/failed/stopped. Dormant means
fallback is permitted but the owned listener has not started. servicePort MUST
be present iff running, in 1..65535. Start/error counters describe only this host
lifetime. Disabled WebCast reports enabled=false, overall disabled, no selection,
disabled cloud/local states, no listener port and zero retained/connected peers.
An enabled but stopped host reports stopped/no selection/no connected transports.

## Query and event synchronization

The query requires `{"controlVersion":1}`. Unknown versions return NOT_SUPPORTED;
malformed requests return INVALID_ARGUMENT; incomplete/sealed capture returns
UNAVAILABLE. An unsupported feature MUST NOT be advertised or simulated with an
empty successful response. Supported but disabled WebCast returns explicit state.
An empty/old capability descriptor never implies support.

The query, optional status section and event use the same full replacement:

```json
{
  "backendContext": {
    "controlVersion": 1,
    "runtimeInstanceId": "11111111-1111-4111-8111-111111111111",
    "backendRevision": "2",
    "backendEventSequence": "2"
  },
  "state": {
    "enabled": true,
    "state": "ready",
    "selectedBackend": "local",
    "cloud": {"state":"offline","transportConnected":false,"retainedPeerCount":1,"connectedPeerCount":1,"lastNetworkError":12002},
    "local": {"fallbackEnabled":true,"serviceState":"running","servicePort":78,"startAttempts":"1","signaling":{"state":"ready","transportConnected":true,"retainedPeerCount":0,"connectedPeerCount":0}}
  }
}
```

backendContext uses the same host runtimeInstanceId as control discovery, but
**independent** backendRevision/backendEventSequence. Counters are canonical
decimal uint64 strings in JSON and TLV, 0..18446744073709551615, no sign, whitespace,
leading zeros, floating-point conversion or rollover. New host lifetime changes
the UUID and resets its counters. Unsupported optional fields remain ignorable.

Meaningful committed state changes increment the backend revision and event
sequence and publish one complete replacement. Unchanged polling, sampled time
and frame counts MUST NOT create events. Event buffering is bounded; publication
failure/disconnect is recovered by snapshot, not unbounded history or retry.
This feature MUST NOT consume P0 eventSequence, enter P0 commit batches, change
P1 settingsEventSequence, or require old clients to count an unknown batch part.
Counter exhaustion stops publication and makes capture unavailable; no wrap.

Subscribe before the query, buffer bounded events, atomically install the query
state/barrier, discard buffered events through that barrier, then apply newer
contiguous complete replacements in the same epoch. Duplicate/older deliveries
do not undo state. A gap, changed epoch, invalid counter/context or buffer overflow
requires a fresh snapshot. A different control socket's late messages cannot
update the current mirror. Reconnect queries again; no command/media is replayed.
Shutdown can make queries unavailable; delivery of a final event is not promised.

`cast.getStatus` requires controlVersion=1 and include=["webcastBackend"] for
this optional section. It is **not** implied by unifiedControl. When combined
with P0/P1 sections it comes from the same host capture boundary and service
lock, retaining each stream's own barrier. An isolated backend change does not
change P0 control revision or settings revision.

## Privacy and compatibility

No code/password or hash, token, owner credential, URL with query credentials,
SDP/ICE, private key, raw exception text or private file path may appear here.
includeSensitive does not relax this rule. Error summaries are numeric only;
known fields carry no free-form endpoint/name material. The listener port is
not a claim that LAN routing/firewall/TLS trust or actual capture has passed.

All existing IDs, fields, enums, requiredness and legacy backend behavior remain.
The capability, method and event are optional. Unknown optional fields are ignored;
unknown versions/unsupported features are explicitly rejected. Protocol reference
checks establish contract consistency, not physical browser/media acceptance.
