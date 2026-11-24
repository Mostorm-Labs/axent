# Typed Input v1 contract and adapter

Classification: JSON-free generic input contract plus private AXTP protocol
adapter. No product gate, route policy, capture geometry or native OS injection
has been added to Axent Core.

Use target `axent::input_contract` and public header
`axent/input/input_stream.hpp`. It exposes Touch contacts, Mouse events, sample
packets, negotiated limits, opaque authenticated source-binding identity and
full-frame encode/decode operations. Public headers expose no Runtime types,
generated IDs or JSON. The adapter implementation privately uses Runtime codecs.

`encode_data`/`decode_data` operate on input data. `encode_frames` adds the full
AXTP Standard Frame and STREAM16; `FrameDecoder::consume` accepts one complete
STREAM frame and reassembles before producing a sample. Incomplete fragments
produce no packet; bad CRC/shape/budget returns failure. The caller must reset
on context/channel loss and fence its bound owner on current-flow errors.

Touch is `inputType(1B)+record(7B)×N`, one full sample with terminal contacts.
Mouse is an explicitly typed event with variant-specific fields. The adapter
never guesses type from buttons and never silently converts Touch/Mouse.
It does not authorize or inject anything. Products/endpoint owners validate
actual accepted types, sequence/cursor, target/source binding, local generation,
native readiness and ownership before executing through their native adapter.
Public default limits are codec bounds, not a statement of device capability.

The vendored `third_party/axtp` input is the exact unpublished snapshot of AXTP
`48144141775da6c35e579d2498d88c0b0449ed61`, version `0.18.0-dev.feac6fa2a4b9`,
SHA-256 `feac6fa2a4b9c927708c077746166259ffeb7d5f06dc6972b29f1fc30954c2d0`.
Its manifest includes business, flow, protocol, specs, generated contracts and
conformance. `.gitattributes` preserves those fingerprinted bytes on checkout.
The Runtime submodule consumes the same binding; its released spec lock is
unchanged. Rebuild the snapshot from canonical committed spec input rather than
editing vendored documents/generated files independently.

`input_stream_test` verifies the public contract, 7×N samples, signed Mouse
wheel fields, exact failure behavior, complete framing, fragmentation and CRC.
Runtime additionally cross-checks the formal JS vectors. Run the Axent boundary
test, affected tests and a clean recursive checkout/build when upgrading the pin.
