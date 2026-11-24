# Framed control client provider

`axent::framed_control_client` supplies `axent/control/framed_client.hpp` for an
already authenticated, ordered, reliable, message-oriented byte carrier. It is
a protocol adapter, not a product host or a native input provider. Runtime types,
generated IDs, framing and RPC codecs stay private to its implementation.

## Ownership and integration

Create one `FramedClient` per authenticated transport session. All methods and
drains run on one serialized owner lane; the object is not thread safe. Identity,
authorization, routing, product RPC handlers, feature capability, transport send
and recovery policy belong to the caller. This profile uses a preauthenticated
peer; Hello requesting an additional authentication exchange is rejected.

Call `Start` once with a monotonic millisecond clock, Identify random seed and
event mask. The adapter sends CONTROL OPEN; ACCEPT, Hello, Identify and Identified
must complete before `Ready()`. The negotiated frame budget is available through
`MaxFrameBytes()`. `event_masks(names)` derives Identify subscription bytes from
the pinned generated event registry. Unknown names return no value.

Call `Consume` with exactly one complete Standard Frame, never bare payloads or
concatenated frames. Fragments are reassembled in order. Call `Tick` regularly
(more often than the negotiated heartbeat interval), even without traffic, to
enforce handshake, fragment, RPC and heartbeat deadlines. Use the same monotonic
clock for Start, Consume, Request and Tick.

Drain `TakeFrames` after operations and send each result once through the same
carrier. Drain `TakeMessages` to dispatch requests, responses and events. JSON
bodies use UTF-8 strings; the caller validates their semantic schema. Requests
must use strictly increasing nonzero IDs. `Respond` must match the incoming ID
and method. Successful response bodies are result objects; error bodies are
status details objects for the Runtime standard encoder. RPC timeout generates
a response message with `reason=RPC_TIMEOUT`; late replies for retired IDs are
discarded. Stream payloads do not enter this control-only client.

`Failed()` is terminal. Discard queued input and retire the session according to
the product's cleanup rules. Construct a new object for a new authenticated
session; do not reset it in place to bypass uncertain native cleanup.

## Resource bounds

- Standard frame budget: 64..4096 bytes, negotiated down by ACCEPT.
- One ordered fragment assembly: at most 64 KiB and 5 seconds.
- Handshake and pending RPC: 5 seconds.
- Heartbeat: negotiated interval, default 1 second, three intervals without
  confirmation fail the session.
- Outbound frames and undrained messages: 128 each; incoming and outgoing pending
  requests: 32 each. JSON nesting: at most 32 before the recursive decoder.
- Request/response JSON body: at most 32 KiB, object root.

Malformed length, CRC, fragment order, wrong session/role or unsupported encoding
fails the input session. No product/media policy is inferred from that failure.

## Verification

`framed_client_test` pairs the adapter with the actual Runtime AxtpCore and covers
handshake, request body/ID, timeout, fragment lifetime, depth, session and heartbeat
fences. `framed_client_probe` is a line-oriented console fixture for independent
language/carrier interoperability. It has no GUI or operating-system injection.
`control_contract_test` also exercises formal object-schema validation, allowing
products to validate capability DTOs without importing Runtime schema headers.
