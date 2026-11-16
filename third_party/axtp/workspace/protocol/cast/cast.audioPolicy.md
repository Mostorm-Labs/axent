---
status: generated
contract: true
generated: true
domain: cast
feature: cast.audioPolicy
registry: ../../../contract/registry/domains/cast/domain.yaml
lastReviewed: 2026-10-20
---

# cast.audioPolicy

[REVIEW-OK] Adopt P0 from [r1 input](../../business/cast-unified-control-r1.md).
Adopted into registry and emitted by the generator on 2026-10-20; this is a
human-authored adoption record, with registry maturity still draft.
`cast.setSessionAudioEnabled` (0x1617, bitOffset 22) accepts the four explicit
command fields, exact target and boolean enabled; returns CastControlOperation.
It requires cast.audioPolicy and cast.controlOperations. `cast.audioPolicyChanged`
(0x1610, bitOffset 15) replaces only audioPolicy with required commit context.
Capability cast.audioPolicy (0x1609) uses CastAudioPolicyCapability.

Enabled intent and effective participation are distinct. Same-value writes are
idempotent; exclusive selection can change other audio participation without
stopping their video/session. No mode setter, listening claim or automatic policy
algorithm is adopted. Existing cast.audio and its single-source fields remain.
Errors are the exact-write common set in [normative contract](../../../specs/45-cast-control.md).
Source schema and field IDs are in domain.yaml; [F4](../../flows/cast-unified-control.md)
and C07 supply behavior evidence. Registry maturity remains draft.

## 3. Methods

### 3.0 方法速览

| Method | Request | Response |
|---|---|---|
| cast.setSessionAudioEnabled | CastSetSessionAudioEnabledParams | CastControlOperation |

### 3.1 `cast.setSessionAudioEnabled`

#### 3.1.1 d block 示例

request:

```json
{
  "id": 103,
  "method": "cast.setSessionAudioEnabled",
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
    },
    "enabled": true
  }
}
```

Queued means admission/query state only; it does not prove media success.

success:

```json
{
  "id": 103,
  "status": {
    "ok": true,
    "code": 0
  },
  "result": {
    "operationId": "operation-1",
    "runtimeInstanceId": "11111111-1111-4111-8111-111111111111",
    "clientInstanceId": "22222222-2222-4222-8222-222222222222",
    "commandId": "33333333-3333-4333-8333-333333333333",
    "kind": "setSessionAudioEnabled",
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
