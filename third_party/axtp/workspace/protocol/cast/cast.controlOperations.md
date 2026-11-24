---
status: generated
contract: true
generated: true
domain: cast
feature: cast.controlOperations
registry: ../../../contract/registry/domains/cast/domain.yaml
lastReviewed: 2026-10-20
---

# cast.controlOperations

[REVIEW-OK] Adopt P0 from [r1 input](../../business/cast-unified-control-r1.md).
Adopted into registry and emitted by the generator on 2026-10-20; this is a
human-authored adoption record, with registry maturity still draft.
`cast.getControlOperation` (0x1618, bitOffset 23) queries by controlVersion,
runtimeInstanceId, clientInstanceId and commandId, returning CastControlOperation.
`cast.controlOperationChanged` (0x1611, bitOffset 16) carries one record and required
context. Capability cast.controlOperations (0x160A) declares bounded query,
deduplication and retention guarantees through CastControlOperationsCapability.

RPC id, deduplication key and operationId have separate identities. Admission
reserves retention; no guaranteed record is evicted to admit work. Terminal
rejection, logical commit and partial failure remain distinguishable. NOT_FOUND
after expiry and timeout both mean unknown outcome; neither authorizes fresh-key
replay. No guarantee crosses a runtime restart. P1 adds the global settings kind with optional-on-P0 settingsResult; see cast.presentationSettings.md.
Exact errors and terminal conditions are in [normative contract](../../../specs/45-cast-control.md);
[F3/F5/F6](../../flows/cast-unified-control.md) and C05/C06/C08 prove the behaviors.
Registry maturity remains draft; no new numerical error space or legacy mapping.

## 3. Methods

### 3.0 方法速览

| Method | Request | Response |
|---|---|---|
| cast.getControlOperation | CastGetControlOperationParams | CastControlOperation |

### 3.1 `cast.getControlOperation`

#### 3.1.1 d block 示例

request:

```json
{
  "id": 104,
  "method": "cast.getControlOperation",
  "params": {
    "controlVersion": 1,
    "runtimeInstanceId": "11111111-1111-4111-8111-111111111111",
    "clientInstanceId": "22222222-2222-4222-8222-222222222222",
    "commandId": "33333333-3333-4333-8333-333333333333"
  }
}
```

Queued means admission/query state only; it does not prove media success.

success:

```json
{
  "id": 104,
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
