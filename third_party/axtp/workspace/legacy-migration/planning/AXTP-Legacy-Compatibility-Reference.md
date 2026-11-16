# AXTP Legacy Compatibility Reference

> Status: Legacy Source
> Scope: Old protocol mapping material extracted from legacy registry documents

## 1. Purpose

This file collects legacy compatibility material that should not live in v1 Core specs or Registry/Capability Types specs meta specs.

Normative migration rules live in:

```text
specs/50-tooling.md
```

Original legacy registry documents are retained as raw evidence at:

```text
workspace/legacy-migration/evidence/
```

## 2. Mapping Categories

| Legacy object | AXTP destination |
|---|---|
| CmdValue | `methods[].legacy.cmdValue` |
| Legacy method name | `methods[].legacy.name` |
| Legacy payload format | `methods[].legacy.payloadFormat` or runtime legacy adapter metadata |
| Legacy status | `errors[].code` through a legacy error mapping |
| Legacy event | `events[]` plus adapter metadata |
| Legacy capability / feature bit | Normalize to `domain.feature`, then expose through v1 `capability.supportedMethods` or future Capability Model |
| Legacy stream bytes | RPC stream setup plus `PayloadType=STREAM` |

## 3. Current Preserved Mappings

| AXTP method | Legacy command | Legacy name | Payload format |
|---|---:|---|---|
| `device.getInfo` | `0x000B0002` | `BetaDeviceInfo` | fixed_struct |
| `display.setBrightness` | `0x000B0042` | `BetaBrightnessSet` | fixed_struct |

## 4. Legacy Error Mapping Rule

Legacy status values must map to AXTP `errors[].code`:

```yaml
legacyErrorMappings:
  - source: AXDP_HID
    legacyStatus: 0x00
    axtpErrorName: SUCCESS
  - source: AXDP_HID
    legacyStatus: 0x01
    axtpErrorName: RPC_PARAM_INVALID
  - source: AXDP_HID
    legacyStatus: 0x02
    axtpErrorName: BUSY
```

Unmapped legacy status values should use a future legacy adapter error, not a frame/control error.

## 5. Domain-Feature Normalization

Legacy compatibility must use the new source planning names as primary names. Existing registry YAML aliases remain pending migration.

| Legacy / old AXTP name | Target source name |
|---|---|
| `firmware.begin` | `firmware.beginOta` |
| `firmware.end` | `firmware.commitOtaBatch` |
| `firmware.verify` | `firmware.verifyOtaFiles` |
| `firmware.apply` | `firmware.installOta` |
| `firmware.updateProgress` | `firmware.otaProgressReported` |
| `firmware.updateCompleted` | `firmware.otaStateChanged` |
| `firmware.updateFailed` | `firmware.otaResultReported` |
| `network.softAp` | `network.ap` |
| `stream.hidMedia` | `video.stream` / `audio.recording` / `stream.flowControl` |
| `display.brightnessMin / display.brightnessMax / display.brightnessStep` | `display.brightness` capability fields |

## 6. Promotion Rule

Legacy material is only promoted to `contract/registry/**/*.yaml` or `contract/registry/domains/**/*.yaml` after it has:

1. A stable AXTP method/event/error/type/profile mapping.
2. A clear owner domain.
3. A schema or explicit runtime legacy adapter conversion rule.
4. Conformance tests or adapter test vectors.
