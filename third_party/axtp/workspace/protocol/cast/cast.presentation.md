---
status: generated
contract: true
generated: true
domain: cast
feature: cast.presentation
registry: ../../../contract/registry/domains/cast/domain.yaml
lastReviewed: 2026-10-20
---

# cast.presentation

[REVIEW-OK] P0 from [r1 input](../../business/cast-unified-control-r1.md).
Adopted into registry and emitted by the generator on 2026-10-20; registry
maturity remains draft. This human-authored adoption record is not generator output.
Adopt the exact target and atomic projection contract in
[Cast control](../../../specs/45-cast-control.md), with the
[F1–F6 flow](../../flows/cast-unified-control.md).

- `cast.getControlCapabilities`: Empty → CastControlCapabilities, method 0x1615,
  bitOffset 20; errors SUCCESS / NOT_SUPPORTED / UNAVAILABLE.
- `cast.stopPresentation`: CastStopPresentationParams → CastControlOperation,
  method 0x1616, bitOffset 21, also requires cast.controlOperations.
- `cast.receiversChanged`, `cast.sessionsChanged`, `cast.presentationChanged`:
  events 0x160D–0x160F, bitOffsets 12–14; section replacement plus required context.
- `cast.presentation`: capability 0x1608 → CastPresentationCapability.
- `cast.getStatus` appends optional controlVersion and five P0 sections;
  `cast.windowChanged` appends optional controlContext. Existing identifiers,
  selectors and required fields survive unchanged. The old generated feature
  documents are historical adoption records; this proposal records the amendment.

The review resolves ownership, version discovery, full snapshot capture, decimal
string fences, ID allocation, current versus historical errors, privacy and old
protocol coexistence. Source shapes live in domain.yaml; semantic conditions live
in specs/45-cast-control.md. No new core/profile requirement. P1 settings are adopted separately in cast.presentationSettings.md. Registry maturity remains draft until a separate maturity review.

Future semantic changes amend this adopted proposal before changing registry.

## 3. Methods

### 3.0 方法速览

| Method | Request | Response |
|---|---|---|
| cast.getControlCapabilities | Empty | CastControlCapabilities |
| cast.stopPresentation | CastStopPresentationParams | CastControlOperation |

### 3.1 `cast.getControlCapabilities`

#### 3.1.1 d block 示例

request:

```json
{
  "id": 101,
  "method": "cast.getControlCapabilities",
  "params": {}
}
```

Queued means admission/query state only; it does not prove media success.

success:

```json
{
  "id": 101,
  "status": {
    "ok": true,
    "code": 0
  },
  "result": {
    "supportedControlVersions": [
      1
    ],
    "runtimeInstanceId": "11111111-1111-4111-8111-111111111111",
    "methods": [
      "cast.getControlCapabilities",
      "cast.stopPresentation",
      "cast.setSessionAudioEnabled",
      "cast.getControlOperation",
      "cast.getStatus"
    ],
    "events": [
      "cast.receiversChanged",
      "cast.sessionsChanged",
      "cast.presentationChanged",
      "cast.audioPolicyChanged",
      "cast.controlOperationChanged",
      "cast.windowChanged"
    ],
    "statusSections": [
      "controlSync",
      "receivers",
      "sessions",
      "presentation",
      "audioPolicy",
      "window"
    ],
    "presentation": {
      "protocols": [
        "hid",
        "airplay",
        "webcast"
      ],
      "supportsAtomicSnapshot": true,
      "supportsStrictTargets": true,
      "maxPresentationCapacity": 8,
      "capacityChoices": [
        1,
        2,
        3,
        4,
        8
      ],
      "layoutModes": [
        "fixed-ratio",
        "auto-fit-content"
      ]
    },
    "audioPolicy": {
      "controlModes": [
        "automatic",
        "manual"
      ],
      "mixModes": [
        "exclusive",
        "mix"
      ],
      "supportsPerSessionEnable": true,
      "reportsEffectiveSet": true
    },
    "operations": {
      "supportsQuery": true,
      "supportsDedup": true,
      "limits": {
        "maxPendingOperations": 4,
        "maxPendingPerClient": 2,
        "maxRetainedResults": 8,
        "replayWindowMs": 1000,
        "maxEventBufferEvents": 8,
        "maxEventBufferBytes": 65536,
        "maxRequestBytes": 4096,
        "maxSnapshotBytes": 65536
      }
    }
  }
}
```

### 3.2 `cast.stopPresentation`

#### 3.2.1 d block 示例

request:

```json
{
  "id": 102,
  "method": "cast.stopPresentation",
  "params": {
    "controlVersion": 1,
    "runtimeInstanceId": "11111111-1111-4111-8111-111111111111",
    "clientInstanceId": "22222222-2222-4222-8222-222222222222",
    "commandId": "33333333-3333-4333-8333-333333333333",
    "target": {
      "session": {
        "id": "session-hid",
        "generation": "9007199254740993"
      },
      "slotId": 0,
      "bindingGeneration": "1",
      "lifecycleGeneration": "1"
    }
  }
}
```

Queued means admission/query state only; it does not prove media success.

success:

```json
{
  "id": 102,
  "status": {
    "ok": true,
    "code": 0
  },
  "result": {
    "operationId": "operation-1",
    "runtimeInstanceId": "11111111-1111-4111-8111-111111111111",
    "clientInstanceId": "22222222-2222-4222-8222-222222222222",
    "commandId": "33333333-3333-4333-8333-333333333333",
    "kind": "stopPresentation",
    "state": "queued",
    "submittedAt": "2026-10-20T08:00:00Z",
    "target": {
      "session": {
        "id": "session-hid",
        "generation": "9007199254740993"
      },
      "slotId": 0,
      "bindingGeneration": "1",
      "lifecycleGeneration": "1"
    }
  }
}
```


## Canvas extension for Touchback

The v0.18 unified `cast.presentationSettings` schemas remain authoritative. The
Touchback amendment adds optional append-only fields to
`CastPresentationSettingsPatch`, `CastPresentationSettingsValues`, and
`CastPresentationStartupSettings`: `canvasMode` (`adaptive` or `fixed`),
`baseWidth`, `baseHeight`, `maxWidth`, and `maxHeight`. Width and height are
positive `uint32` values bounded by 16384 per axis.

`CastPresentationSettingsCapability` may advertise `canvasModes`,
`canvasMaxWidth`, and `canvasMaxHeight`. A settings change fences the affected
input stream, releases active contacts/buttons, and opens a new input epoch with
new canvas and geometry tokens.
