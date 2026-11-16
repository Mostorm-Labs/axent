# Generated Legacy Migration Candidates

This directory contains generated legacy migration candidates. These files are
review inputs only.

They are not AXTP registry facts, runtime contracts, release artifact contents,
or patches that can be applied directly.

Important boundaries:

- `registry-patches.generated.yaml` may mention candidate paths such as
  `contract/registry/domains/<domain>/domain.yaml` that do not currently exist.
- Missing `contract/registry/domains/**` paths in this directory mean "candidate
  destination if adopted", not "current source path".
- Confirmed facts must first go through `workspace/protocol/**` review and then
  be adopted into `contract/registry/**` by the normal Stage 30 / Stage 50 flow.
- Do not treat this directory as generated runtime output. Runtime repos should
  consume `contract/**`, `specs/**`, `conformance/**`, and release artifacts.

