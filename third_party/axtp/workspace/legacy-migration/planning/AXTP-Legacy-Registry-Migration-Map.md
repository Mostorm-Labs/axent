# AXTP Legacy Registry Migration Map

> Status: Migration Working Source
> Scope: Disposition map for legacy Registry/Capability Types specs registry documents

## 1. Rule Set

| Legacy content | Destination |
|---|---|
| Affects binary wire format | `specs/02`, `04`, `05`, `06`, `07` |
| Affects transport/header/profile | `02` / `03` |
| Affects OPEN / ACCEPT / READY | `04` |
| Affects Hello / Identify / RPC | `05` |
| Affects STREAM header / resume | `06` |
| Concrete business method/event/type | `contract/registry/**/*.yaml`, `contract/registry/domains/**/*.yaml`, or legacy evidence under `workspace/legacy-migration/evidence/` |
| Complete Capability Model | Not carried forward as a current v1 input; restart through `workspace/protocol/` RFC if needed |
| Registry entry meta model | `Registry/Capability Types specs` meta specs |
| Legacy compatibility reference | `workspace/legacy-migration/planning/AXTP-Legacy-Compatibility-Reference.md` |

## 2. Legacy 08 Disposition

| Section | Disposition |
|---|---|
| Registry positioning, single source of truth | Keep as governance in `08`; source of truth is now `contract/registry/**/*.yaml` and `contract/registry/domains/**/*.yaml` |
| Header and business decoupling | Migrated to `02` |
| ID width and byte order | Migrated to `02` |
| PayloadType | Migrated to `02` |
| Control Opcode | Migrated to `04` |
| RPC Encoding / RPC Operation | Migrated to `05` |
| Stream Profile boundary | Migrated to `06` |
| Domain / method / event / error / type allocation rules | Kept in `YAML mapping and Registry/Capability Types specs` meta specs |
| Legacy Mapping / Vendor Extension | Migrated to `07` and `workspace/legacy-migration/planning/AXTP-Legacy-Compatibility-Reference.md` |
| Domain-Scoped Mask | Split into `05` eventMasks and `12` supportedMethods bitmap |

## 3. Legacy 09-11 Disposition

| Source | Disposition |
|---|---|
| MethodId online position and JSON/Binary mapping | `05` and `09` |
| Method entry fields and stability rules | `09` |
| Complete method planning tables | Legacy evidence under `workspace/legacy-migration/evidence/`; current MVP facts in Source YAML |
| EventId online position and eventMasks | `05` and `10` |
| Event entry fields and eventId ranges | `10` |
| Complete event planning tables | Legacy evidence under `workspace/legacy-migration/evidence/`; current MVP facts in Source YAML |
| ErrorCode response/control/stream mappings | `04`, `05`, `06`, `11` |
| Complete error planning tables | Current set in Source YAML; future set in protocol source |
| Legacy status mapping | `workspace/legacy-migration/planning/AXTP-Legacy-Compatibility-Reference.md` |

## 4. Legacy 12-13 Disposition

| Source | Disposition |
|---|---|
| v1 `capability.supportedMethods` | `12` and Source YAML |
| Complete capability tree and query methods | Not carried forward as a current v1 input; restart through `workspace/protocol/` RFC if needed |
| Business capability registries | Protocol source only; not v1 Core |
| MVP payload/frame/control/RPC/stream decisions | Migrated to `02/03/04/05/06` |
| MVP method/event/error/profile facts | `contract/registry/**/*.yaml` and `contract/registry/domains/**/*.yaml` |
| MVP type/TLV subset | `12` and future type-system work |
| Legacy compatibility MVP | `07` and `workspace/legacy-migration/planning/AXTP-Legacy-Compatibility-Reference.md` |

## 5. Domain-Feature Migration Notes

Legacy commands and source planning tables must be normalized through the `domain.feature` taxonomy before mapping to method/event/schema adapters:

```text
legacy command -> domain.feature -> method/event -> schema adapter
```

| Old source name | Target source name | Reason |
|---|---|---|
| `firmware.begin` | `firmware.beginOta` | Bind method explicitly to `firmware.ota`. |
| `firmware.end` | `firmware.commitOtaBatch` | Express OTA batch commit instead of generic end. |
| `firmware.verify` | `firmware.verifyOtaFiles` | Express OTA file verification. |
| `firmware.apply` | `firmware.installOta` | Express install action under OTA feature. |
| `firmware.updateProgress` | `firmware.otaProgressReported` | Use progress reported event template. |
| `firmware.updateCompleted` | `firmware.otaStateChanged` | Completion is represented as OTA state transition. |
| `firmware.updateFailed` | `firmware.otaResultReported` | Failure is represented as OTA result report. |
| `stream.open` | Business-domain open stream method | `stream` domain no longer performs generic business stream creation. |
| `stream.hidMedia` | `video.stream` / `audio.recording` / `stream.flowControl` | HID is transport, media is business classification. |
| `display.brightnessMin/Max/Step` | `display.brightness` schema fields | Brightness range is feature metadata, not separate capability. |

## 6. Execution Notes

The source files under `workspace/legacy-migration/evidence/` and the planning notes under `workspace/legacy-migration/planning/` are retained as evidence, planning and migration reference. They are not normative unless their facts have been promoted into `contract/registry/**/*.yaml` or `contract/registry/domains/**/*.yaml`.
