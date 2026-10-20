# Local cast-control adapter development

The runtime submodule is a local, unpublished checkpoint. It consumes the exact
AXTP snapshot fingerprint
`683a051759b7b82b2c0c80323dd719c99cee0ed02a6d42ee447d54a24751686a`
through explicit `AXTP_SPEC_PATH` / `AXTP_SPEC_SOURCE_MODE=local-development`.
The runtime's formal release lock remains unchanged and rejects local input.
No Axent release or protocol spec/v0.18.0 publication is implied.

Axent exposes route-name dispatch, payload validation and lossless decimal
uint64 parsing through public contracts. Generated IDs, schema codec and
cpp-runtime remain private implementation dependencies. `protocol_spec_version`
provides exact handshake metadata without leaking runtime types.

Control event batches enter a bounded queue atomically. Overflow closes control
connections for resynchronization; it leaves the listener and media owners alive.
Concrete WebSocket ingress/outbound queues are bounded as well. Route handlers
do not acquire media/provider ownership or implement cast settings policy.

The private scripted-adapter factory now supplies an explicit empty USB
enumeration. Previously, injected transports still enumerated workstation USB
devices, so an unrelated physical NA20 could reject a fixture's logical serial.
The unchanged baseline reproduced this failure locally. Production factories
continue using native enumeration and the same allowlist, leases and retry rules.
No media retry behavior was changed to make tests pass.

Validation uses Windows targets `control_contract_test`,
`axtp_control_endpoint_test`, `websocket_transport_test`, existing adapter/host
tests, the dependency boundary test and full CTest. The NearCast product tests
exercise the actual host, precise ownership and settings store over this adapter;
the protocol reference model alone is not product evidence.

These commits have not been pushed. Reproduce from the local Axent object store,
initialize each submodule URL from the corresponding local checkout, and check
the recorded gitlink commits recursively. Before publication replace the local
runtime pin with an approved distributed checkpoint/release and run release-lock
validation separately from development snapshot verification.
