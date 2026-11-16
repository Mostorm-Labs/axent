# C++ Legacy Adapter Plan (Generated)

## Goal

Provide a C++ legacy adapter skeleton that consumes `workspace/legacy-migration/generated/legacy-to-axtp-map.generated.yaml` and exposes AXDP HID and signage SDK traffic as AXTP v1 RPC/Event/STREAM operations.

## Components

- `LegacyProtocolDetector`: inspects incoming bytes or JSON and returns one of the generated `legacy_protocol` values.
- `LegacyCommandDecoder`: parses protocol-specific command value, sequence/correlation fields, legacy status and raw payload bytes.
- `LegacyToAxtpMapper`: resolves generated mapping entries and builds AXTP RPC requests, event emissions or stream-open instructions.
- `AxtpToLegacyMapper`: translates AXTP responses/events back into legacy response envelopes.
- `LegacyStatusMapper`: owns status mapping tables such as `0x00 -> SUCCESS`, `0x01 -> RPC_PARAM_INVALID`, `0x02 -> BUSY`.
- `LegacyStreamBridge`: moves firmware/file/log/media chunks over AXTP STREAM while keeping setup/control in RPC.
- `LegacyAdapter`: orchestrates detection, decode, mapping, AXTP dispatch and legacy response encoding.

## Header Sketch

```cpp
namespace axtp::legacy {

enum class LegacyProtocol {
  kAxdpHid,
  kVm33HttpJson,
  kSignageSdk,
  kUnknown,
};

enum class LegacyTargetKind {
  kRpcMethod,
  kEvent,
  kCapability,
  kStream,
};

struct LegacyEnvelope {
  LegacyProtocol protocol;
  std::string legacyId;
  std::string legacyName;
  std::uint32_t sequence = 0;
  std::vector<std::uint8_t> payload;
};

struct AxtpTarget {
  LegacyTargetKind kind;
  std::string methodName;
  std::string eventName;
  std::string capabilityName;
  std::string streamProfile;
};

class LegacyProtocolDetector {
 public:
  LegacyProtocol Detect(std::span<const std::uint8_t> bytes) const;
};

class LegacyCommandDecoder {
 public:
  Result<LegacyEnvelope> Decode(LegacyProtocol protocol, std::span<const std::uint8_t> bytes) const;
};

class LegacyToAxtpMapper {
 public:
  Result<AxtpTarget> Resolve(const LegacyEnvelope& envelope) const;
};

class AxtpToLegacyMapper {
 public:
  Result<std::vector<std::uint8_t>> EncodeResponse(const LegacyEnvelope& request, const Message& response) const;
  Result<std::vector<std::uint8_t>> EncodeEvent(const Message& event) const;
};

class LegacyStatusMapper {
 public:
  ErrorCode ToAxtp(LegacyProtocol protocol, std::int32_t legacyStatus) const;
  std::int32_t FromAxtp(LegacyProtocol protocol, ErrorCode error) const;
};

class LegacyStreamBridge {
 public:
  Result<std::uint32_t> Open(const LegacyEnvelope& envelope, const AxtpTarget& target);
  Result<void> PushChunk(std::uint32_t streamId, std::span<const std::uint8_t> chunk);
  Result<void> Close(std::uint32_t streamId);
};

class LegacyAdapter {
 public:
  Result<std::vector<std::uint8_t>> Handle(std::span<const std::uint8_t> incoming);
};

}  // namespace axtp::legacy
```

## Data Generation

- Generate compact lookup tables from `legacy-to-axtp-map.generated.yaml`.
- Keep generated tables separate from AXTP Core generated headers.
- Do not introduce new PayloadType constants.
- STREAM mappings must expose stream profile, setup method if any, and status mapping.

## Tests

- Run `test-vectors.generated.json` through `LegacyAdapter::Handle`.
- Assert existing Beta mappings target current generated AXTP method IDs.
- Assert firmware/file/log/media bulk vectors use STREAM.
- Assert unknown command and invalid payload vectors produce legacy adapter errors.
