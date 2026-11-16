---
status: generated
contract: true
generated: true
domain: cast
feature: cast.webcastBackend
registry: ../../../contract/registry/domains/cast/domain.yaml
lastReviewed: 2026-10-21
---

# cast.webcastBackend

The user explicitly authorizes full state/event implementation and specification
changes. [The normative contract](../../../specs/47-cast-webcast-backend.md)
adopts independent backend snapshots/events. Registry maturity remains draft;
no release/tag or change to legacy cast.backend is implied.

The new query and event share CastWebCastBackendSnapshot. Selection for new joins
and independently retained cloud/local connections are separate observations.
The independent cursor preserves P0 event batches and P1 settings synchronization.
No product fallback/selection/media policy enters a protocol runtime or Axent Core.
