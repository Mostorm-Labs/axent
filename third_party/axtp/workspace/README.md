# AXTP Maintainer Workspace

> Backend workspace: do not start here from the main docs. Use the role guides first, and open this directory only when changing protocol facts or tracing source intent.

`workspace/` groups materials that help maintain the AXTP spec lifecycle but are not runtime implementation contracts.

| Directory | Purpose | Runtime contract |
|---|---|---:|
| [business/](business/README.md) | Product requirements, PRD source notes, customer context, and open questions. | No |
| [flows/](flows/README.md) | Scenario-level interaction plans and protocol coverage analysis. | No |
| [protocol/](protocol/README.md) | Human-written protocol drafts and review records before adoption. | No, unless adopted and generated |
| [legacy-migration/](legacy-migration/README.md) | Old protocol evidence, classification, adapter planning, and generated migration aids. | No |

Release operations live in [../release/](../release/README.md). Stable implementation facts remain in `../specs/`, `../contract/registry/**`, `../contract/protocol/axtp.protocol.yaml`, `../contract/generated/**`, and `../conformance/**`.
