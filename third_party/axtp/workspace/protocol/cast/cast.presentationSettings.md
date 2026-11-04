---
status: generated
contract: true
generated: true
domain: cast
feature: cast.presentationSettings
registry: ../../../contract/registry/domains/cast/domain.yaml
lastReviewed: 2026-10-20
---

# cast.presentationSettings

[REVIEW-OK] The user's combined P0/P1 request adopts the revised r1 §9 and
UNIFIED_PRESENTATION_SETTINGS_PLAN.md, not the superseded r0 draft. The
registry and generator now contain the complete P1 surface. This document is a
human-authored adoption record; registry maturity remains draft. The
[business reconciliation](../../business/cast-unified-control-r1.md) records
compatibility decisions; the [normative contract](../../../specs/46-cast-settings.md)
defines the full settings state, independent stream and partial results.

| Registry item | New assignment | Binding |
|---|---|---|
| cast.setPresentationSettings | method 0x1619, bitOffset 24 | CastSetPresentationSettingsParams → CastControlOperation; presentationSettings + controlOperations |
| cast.presentationSettingsChanged | event 0x1612, bitOffset 17 | CastPresentationSettingsChangedEvent; presentationSettings only |
| cast.presentationSettings | capability 0x160B | CastPresentationSettingsCapability |

Ten new schemas cover params, patch, values, startup intent, state, field issue,
capability, operation result, settings event context and the event. Optional
append-only fields: CastStatus.presentationSettings 0x0F,
CastControlCapabilities.presentationSettings 0x0A,
CastControlOperation.settingsResult 0x0E. The operation kind enum adds
setPresentationSettings while preserving both P0 values and their target rules.
New registry maturity is draft. No Core/MVP profile becomes mandatory.

The first host fixture declares three live and three restart-only keys. The
protocol keeps this a capability partition; startup schema supports all optional
keys, conditioned on that partition. Version conflicts reject; same-intent retry
deduplicates first. A successful save does not prove live adoption or restart.
The [P1 flow](../../flows/cast-presentation-settings.md) and C12/PS-01–PS-12
prove the protocol guarantees using substitute owners.

## 3. Methods

### 3.0 方法速览

| Method | Request | Response |
|---|---|---|
| cast.setPresentationSettings | CastSetPresentationSettingsParams | CastControlOperation |

### 3.1 `cast.setPresentationSettings`

#### 3.1.1 d block 示例

request:

```json
{"id":201,"method":"cast.setPresentationSettings","params":{"controlVersion":1,"runtimeInstanceId":"11111111-1111-4111-8111-111111111111","clientInstanceId":"22222222-2222-4222-8222-222222222222","commandId":"44444444-4444-4444-8444-444444444444","expectedSettingsRevision":"7","patch":{"capacity":4},"persist":true}}
```

success:

```json
{"id":201,"status":{"ok":true,"code":0},"result":{"operationId":"settings-op-1","runtimeInstanceId":"11111111-1111-4111-8111-111111111111","clientInstanceId":"22222222-2222-4222-8222-222222222222","commandId":"44444444-4444-4444-8444-444444444444","kind":"setPresentationSettings","state":"applied","submittedAt":"2026-10-20T08:00:00Z","completedAt":"2026-10-20T08:00:01Z","statusCode":0,"outcomeReason":"SETTINGS_PENDING_RESTART","settingsResult":{"field":"capacity","settingsRevision":"8","configurationStatus":"committed","applicationStatus":"pendingRestart","persistenceStatus":"stored"}}}
```

This accepts the next-start intent without restarting software or changing current
slots. Future amendments update this human-authored adoption record before source.
