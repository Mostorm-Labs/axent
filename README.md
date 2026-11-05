# Axent

The [Typed Input v1 contract](docs/dev/TYPED_INPUT_V1.md) provides JSON-free
Touch/Mouse samples and private AXTP framing adapters for product hosts.

Axent is a device-control runtime core with a stable extension and host model.
It exposes AXTP-oriented control surfaces, manages device sessions, and routes
device commands through AXDP, AXTP, TEA, or mock adapters without accepting
product-specific workflow logic into Core.

Architecture guardrails:

- [Protocol-Bus Layer Boundaries](docs/architecture/LAYER_BOUNDARIES.md) — read before changing code
- [Axent Roadmap](docs/ROADMAP.md)
- [Axent Architecture](docs/architecture/AXENT_ARCHITECTURE.md)
- [Axent Host Model](docs/architecture/AXENT_HOST_MODEL.md)
- [Axent Extension Model](docs/architecture/AXENT_EXTENSION_MODEL.md)
- [Codex Guardrails](docs/dev/CODEX_GUARDRAILS.md)

## Axent Library And Daemon Model

`libaxent` is the current library target for the first-stage implementation. It
contains the core runtime and the present static adapter/host pieces while the
project moves toward narrower `core`, `extension`, `client`, `adapter`, and
`host` targets.

`axentd` is the default daemon host for `libaxent`. It is responsible for
owning physical devices on a PC so multiple frontend products do not compete
for the same HID/AXTP transport.

Products such as Nearcast should normally use the Axent client and extension
boundaries. Controlled embedded-host scenarios may link `libaxent`, but product
logic, rendering policy, and product state machines must remain outside Axent
Core.

Axent relays encoded media frames and metadata. It does not own Nearcast
rendering policy. Nearcast keeps `MediaCore`, H.264/AAC assembly, render clock,
late drop, catch-up, D3D11, WASAPI, overlay, and UI behavior.

## New Axent Mainline

The legacy implementation remains under `agent/` and is not used as the
architecture base for the new core.

New targets:

- `libaxent`: first-stage library target
- `axent::axtp_tooling`: shared AXTP control and diagnostic command runner
- `axtpctl`: canonical AXTP control and diagnostic CLI
- `axentd`: daemon and WebSocket control plane
- `axent`: local management CLI and future host launcher

Target host command model:

```bash
axent daemon
axentd
axent run nearcast
axent up --with daemon,nearcast
axent extension list
axent host list
axent doctor
```

Do not add ambiguous mixed-role commands such as `axent --daemon --nearcast`.

Developer smoke:

```bash
git submodule update --init --recursive
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure
build/axtpctl -t mock ping
build/axent status --offline
build/axentd --foreground
```

## AXTP Control Tool

`axtpctl` is the canonical AXTP control and diagnostic CLI maintained by Axent:

```bash
build/axtpctl -t mock ping
build/axtpctl list-methods
build/axtpctl -c network.getApConfig
build/axtpctl -t mock -o json firmware update --file firmware.bin --chunk-size 1024
```

`axent axtp` remains a compatibility alias and delegates to the same
`axent::axtp_tooling` implementation:

```bash
build/axent axtp -t mock ping
build/axent axtp -c network.getApConfig
```

The shared tooling defaults target the current NA20 development device:

```text
transport: hid
VID/PID: 0x0581:0x2582
usage page/usage: 0x81:0x82
timeout: 5000 ms
output: json
```

Use `-t mock` explicitly for offline examples and tests. No serial number is
configured by default; when multiple NA20 devices are attached, select one with
`--hid-serial <serial>` or `--hid-path <path>`.

JSON RPC calls preserve the AXTP `sid` / `op` / `d` response envelope. The `d`
object contains the request `id`, mandatory `status`, and a `result` only for a
successful response. Tool-only commands such as `ping`, `list-hid`, and
`firmware update` use their own diagnostic output schemas.

The tooling target does not link `libaxent` or Axent Core. Its firmware command
uses the JSON-free `axent::firmware` service and an Axent-owned private AXTP
transaction backend; it does not consume the cpp-runtime firmware profile.
Axent now owns its native TCP and IX WebSocket providers. Runtime SDK/wire, the
HID wrapper, and the compatibility TCP/WebSocket wrappers remain private
transitional dependencies while the ordered transport-provider migration continues.

## Firmware Update V1

`FirmwareUpdateService::run()` is the synchronous local firmware entry point.
It owns file validation and reading, MD5 calculation, maintenance leasing,
`beginUpdate -> STREAM -> finishUpdate`, typed results, and staged progress.
The public contract contains no JSON or cpp-runtime types.

An embedded host supplies the composed provider returned by
`AxentHost::maintenance_lease_provider()`. A maintenance lease fails fast with
Busy while the same device has a control, media, or maintenance lease, and it
releases automatically on every return or exception path. `axtpctl` and
`axent axtp` use the same service while preserving their command and output
contract.

V1 deliberately has no retry, resume, cancellation, or cross-process lock.
The real `axentd` firmware route remains unavailable; V1 does not expose remote
flashing.

## Logging And CLI

`axent` and `axentd` share Axent-native logging options:

```bash
build/axent --version
build/axent status --offline --log --debug
build/axentd --foreground --bind 127.0.0.1 --port 6060 --log --log-level debug
```

`--log` enables file logging, `--debug` maps to `--log-level debug`, and
`--log-dir` selects the log directory. The logger supports level filtering,
structured in-memory diagnostics, checkpoints, bounded memory records, and
rotating file segments without adding a third-party logging dependency.

## Real AXTP Adapter And Daemon Mode

`axentd` can host the first real AXTP HID adapter path with NA20-style defaults:

```bash
build/axentd --foreground --axtp-real --bind 127.0.0.1 --port 6060
```

Default NA20 HID selector values are VID `0x0581`, PID `0x2582`, usage page
`0x0081`, report id `0x05`, and automatic input/output report sizes. Override
them with `--hid-vid`, `--hid-pid`, `--hid-usage-page`, `--hid-usage`,
`--hid-report-id`, `--hid-input-report-size`, `--hid-output-report-size`,
`--hid-path`, and `--hid-serial`.

When no matching HID device is present, the real adapter simply discovers zero
devices and daemon startup continues. When a discovered AXTP device is selected,
the adapter lazily opens the HID transport, runs the AXTP app-ready handshake,
and forwards JSON control calls through the runtime SDK. This lets `axentd`
remain the single resource owner on a PC without making development machines
require hardware. Nearcast can either connect to this daemon through the Axent
client boundary or use a controlled embedded-host path; it must keep
`MediaCore` and renderer policy outside Axent.

The development profile binds WebSocket to LAN by default and has no
authentication. Use only on trusted networks until the hardening stage adds
authentication and authorization.
