# Local cast-control adapter development

The runtime submodule now binds the released AXTP `spec/v0.18.0` and its exact
commit through runtime `v0.18.0.1`. The earlier local snapshot was development
evidence only and is not the release input.

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

Initialize the submodules recursively and check the recorded gitlink commits.
Release-lock validation remains separate from development snapshot verification.
