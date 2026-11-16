# AXTP Legacy Compatibility Layer (Generated)

## Boundary

The compatibility layer sits outside AXTP Core. It accepts the legacy protocols covered by the generated map (AXDP HID and signage SDK), translates them into AXTP RPC/Event/STREAM operations, and translates responses back to the legacy caller.

The layer must not change the AXTP v1 Frame Header, STREAM header, CONTROL opcodes, RPC operation values or PayloadType registry.

## Request Translation

- Decode the legacy envelope and identify `legacy_protocol`, `legacy_id` or Class/Method alias.
- Resolve the mapping from `legacy-to-axtp-map.generated.yaml`.
- For `rpc_method`, build an AXTP RPC request with the mapped method name and schema.
- For `stream`, call the mapped setup/finalization RPC when present, then move chunk or continuous data over `PayloadType=STREAM`.
- Preserve raw legacy payload bytes in adapter diagnostics when a field-level schema is not yet available.

## Response Translation

- Map AXTP `SUCCESS` to the legacy success status for the source protocol.
- Map `RPC_PARAM_INVALID`, `BUSY` and adapter validation failures to the source protocol status vocabulary.
- For unmapped legacy status values, return `LEGACY_STATUS_UNMAPPED` internally and the nearest legacy failure code externally.
- Preserve the original request correlation field, such as an AXDP command value or JSON `Seq`.

## Event Translation

- Legacy notify/subscribe events are normalized into AXTP Event Registry names.
- AXTP event payloads include normalized fields when known and raw legacy payload bytes when only an alias is available.
- Event fan-out and subscription filtering remain adapter behavior.

## Capability Projection

- Legacy support matrices and configuration-name rows project into candidate AXTP capabilities.
- `capability.supportedMethods` is generated from mapped methods exposed by the adapter for the current device model.
- Conflicts between legacy feature bits and AXTP capability declarations produce `LEGACY_CAPABILITY_CONFLICT`.

## STREAM Bridging

- Firmware data, file transfer, log upload, media, KVM, raw audio/video and state-sync bursts use STREAM.
- RPC bodies may carry only setup metadata, small control parameters, result summaries and task IDs.
- STREAM uses the AXTP v1 16-byte header: `streamId:uint32`, `seqId:uint32`, `cursor:uint64`.
- WebSocket Unframed JSON compatibility may expose setup/control RPCs but must not carry STREAM bytes.

## Failure Behavior

- Unknown legacy command: `LEGACY_CMD_UNMAPPED`.
- Malformed payload: `LEGACY_PAYLOAD_INVALID`.
- Payload shorter than required fixed layout: `LEGACY_PAYLOAD_TOO_SHORT`.
- Payload longer than supported fixed layout: `LEGACY_PAYLOAD_TOO_LONG`.
- Unsupported legacy field: `LEGACY_FIELD_UNSUPPORTED`.
- Adapter timeout while waiting for a legacy response: `LEGACY_RESPONSE_TIMEOUT`.

## Generated Coverage

- Total mappings: 266
- STREAM mappings: 10
- Event mappings: 4
- Capability mappings: 0
