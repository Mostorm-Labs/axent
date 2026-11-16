# AXTP Legacy Migration Matrix

`AXTP_Legacy_Migration_Matrix.xlsx` is the working migration matrix for mapping legacy protocols to AXTP.

- [Rooms Protocol Migration Plan](rooms-protocol-migration-plan.md) defines the concrete Rooms WebSocket JSON compatibility strategy: keep legacy method/params, switch Hello to the Logical Server direction, adapt `status.{code,comment,result}` to `status.{code,msg,ok}`, and freeze the embedded parser boundary.
- [VM33 Protocol Migration Plan](vm33-protocol-migration-plan.md) defines the VM33 Pro strategy: keep old VM33 migration unchanged, build the new protocol parser and handler-injection framework first, then add AXTP business protocols for new or changed business logic.
- [Signage Protocol Migration Plan](signage-protocol-migration-plan.md) defines the digital signage strategy: fully switch the small signage SDK surface to AXTP, complete and adopt the current signage/common drafts, then move App, server, and firmware to generated protocol contracts.
- Each legacy protocol source has its own worksheet so reviewers can trace every row back to the original document.
- The main mapping sheets are simplified review tables. Use `AXTP Domain / 域` and `Feature / 业务对象` together to filter similar legacy rows.
- `Capability / Domain.Feature` is the capability candidate ID and must equal `domain.feature`; Excel does not allocate numeric IDs.
- Use `93_Feature分类参考` as the shared catalog from `specs/30-registry.md`.
- After review, confirmed protocol facts should be added to `contract/registry/**/*.yaml`; the Excel file is not the final protocol source of truth.
- Do not manually edit files under `contract/generated/`, `workspace/legacy-migration/generated/`, or runtime generated headers.
- Legacy-only compatibility behavior must stay in the legacy adapter layer and must not pollute AXTP Core.
