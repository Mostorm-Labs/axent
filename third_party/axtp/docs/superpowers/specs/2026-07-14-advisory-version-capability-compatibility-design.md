# Advisory Version And Capability Compatibility Design

## Goal

AXTP sessions remain usable across protocol release differences. RPC `Hello.axtpVersion` is diagnostic metadata, not a session admission gate. Optional functionality degrades through transport profiles, generated capabilities, and the common `NOT_SUPPORTED` error instead of rejecting the whole session.

This design applies to the AXTP specification repository and all maintained consumers: C, C++, Flutter, Python, TypeScript, and the mock server.

## Version Semantics

`Hello.axtpVersion` remains an optional string for diagnostics and telemetry. It does not negotiate features and does not authorize or reject a session.

A receiver must continue the `Hello -> Identify -> Identified` sequence when `axtpVersion` is:

- absent;
- not valid SemVer;
- different only in patch or minor;
- different in major, including values such as `2.0.0`.

Implementations may classify the observed value as `missing`, `malformed`, `different-major`, `different-minor`, or `different-patch`. Such classifications may be exposed through logs, diagnostics, or telemetry, but must not change the session gate, retry policy, reconnect policy, or availability of unrelated RPC operations.

Frame Header `Version` remains a hard parsing boundary. A receiver may reject an unsupported frame version when it cannot safely interpret the frame layout. This wire parsing rule is independent of advisory RPC `axtpVersion` metadata.

Deprecated `protocolVersion`, `rpcVersion`, and `negotiatedRpcVersion` fields remain compatibility inputs only and do not become session admission gates.

## Profile And Capability Degradation

Completing a session establishes only the common RPC baseline. It does not imply that either endpoint supports every generated method, event, schema extension, encoding, or stream feature.

Profiles describe supported transport and wire behavior. Generated capabilities describe optional business features and their limits. A caller should inspect the relevant domain capability before using an optional method, subscribing to an optional event, or sending an optional feature-specific schema value. An absent capability is treated as unsupported.

A receiver must also enforce its own support state even when the caller skips discovery:

| Condition | Required behavior |
|---|---|
| Method name or ID is absent from the receiver's registry | Return `RPC_METHOD_NOT_FOUND`. |
| Method is registered but unavailable in the current runtime, device, profile, mode, or capability set | Return common `NOT_SUPPORTED`. |
| Method is supported but its arguments are malformed or outside declared limits | Return `INVALID_ARGUMENT`, `RPC_PARAM_INVALID`, or the registered validation error. |
| Optional schema field is unknown but structurally valid | Ignore the field and continue decoding. |
| Known field requests an unsupported optional feature | Return `NOT_SUPPORTED` from the containing method. |
| Event is unsupported or not subscribed | Do not emit it. |
| Unknown event is received | Ignore it or record diagnostics without invalidating the session. |
| Profile-specific operation is unavailable | Return `NOT_SUPPORTED` for that operation without invalidating unrelated session functionality. |

Existing `RPC_METHOD_NOT_SUPPORTED` and capability-specific unsupported errors remain valid compatibility inputs for receivers. New implementations send the common `NOT_SUPPORTED` error for a registered but unavailable operation so that one canonical error represents feature degradation.

Capability discovery continues to use generated capability facts and existing domain-specific capability methods. This design does not add a version-range exchange or a second capability registry.

## Cross-Version Conformance Matrix

The main repository provides normative, machine-readable cross-version cases. Runtime repositories consume the same cases according to their declared profiles.

### Handshake Matrix

| Server `axtpVersion` | Client local diagnostic version | Expected result |
|---|---|---|
| `1.0.0` | `1.1.0` | Session established; minor difference may be diagnosed. |
| `1.1.0` | `1.0.0` | Session established; minor difference may be diagnosed. |
| `1.0.1` | `1.0.0` | Session established; patch difference may be diagnosed. |
| `2.0.0` | `1.0.0` | Session established; major difference may be diagnosed. |
| malformed string | `1.0.0` | Session established; malformed value may be diagnosed. |
| absent | `1.0.0` | Session established; missing value may be diagnosed. |

### Capability Matrix

The shared cases cover:

1. A new client connected to an old server does not invoke a feature whose capability is absent.
2. If that client invokes a registered but unavailable method anyway, the server returns `NOT_SUPPORTED` and the session remains usable.
3. An old client connected to a new server ignores unknown optional schema fields.
4. A new server does not emit unsupported or unsubscribed events to an old client.
5. An unknown method returns `RPC_METHOD_NOT_FOUND`, distinct from a registered but unsupported method.
6. A profile mismatch degrades only the affected operation with `NOT_SUPPORTED`.
7. Receiving an unknown event does not close or invalidate the session.

## Repository Changes

The AXTP repository will:

- make the advisory `axtpVersion` behavior normative in the core specification;
- clarify profile, capability, schema, method, and event degradation rules;
- align error registry guidance around canonical `NOT_SUPPORTED` sending behavior;
- add machine-readable handshake and capability matrix cases;
- update generated protocol artifacts and conformance documentation.

Runtime repositories will:

- remove `axtpVersion` session rejection behavior;
- retain optional version diagnostics without coupling them to connection state;
- ensure registered but unavailable operations return `NOT_SUPPORTED`;
- keep unknown method behavior as `RPC_METHOD_NOT_FOUND`;
- ignore valid unknown optional fields and non-actionable unknown events;
- run the shared matrix for every declared profile supported by that runtime.

TypeScript will remove its `isAxtpVersionCompatible` admission check and update SDK reconnect tests so version differences no longer produce deterministic connection failures. C++ will stop treating its Spec release version as a compatibility gate and will align its Hello metadata with the advisory semantics. C, Flutter, Python, and the mock server will add equivalent behavior or explicit evidence that their current session path does not gate on `axtpVersion`.

## CI And Upgrade Flow

Each runtime upgrade workflow runs its applicable cross-version matrix after generated artifacts and version metadata are refreshed, and before creating an upgrade pull request. A runtime may skip only cases belonging to an undeclared profile; it may not skip advisory-version cases for a profile it claims to support.

Failure reporting identifies the matrix row, local runtime version, peer version value, profile, capability declaration, request, response code, and whether the session remained usable.

## Success Criteria

- Every maintained runtime establishes a session for all handshake matrix rows on each supported RPC profile.
- No runtime changes retry or reconnect behavior solely because of `axtpVersion`.
- Registered but unavailable operations return `NOT_SUPPORTED` and leave the session usable.
- Unknown methods continue to return `RPC_METHOD_NOT_FOUND`.
- Unknown optional schema fields and events do not invalidate a session.
- All runtime upgrade workflows execute the applicable shared cross-version cases before opening an upgrade pull request.
