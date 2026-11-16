---
name: generate-axtp-protocol
description: Stage 50 generation skill for accepted AXTP registry YAML facts. Use when specs and contract/registry/domain YAML are already updated and the user asks to generate, regenerate, build, emit, refresh, validate, or publish AXTP protocol artifacts. Runs the Generator pipeline from YAML to Protocol IR, generated docs, tooling JSON, and test vectors.
---

# Generate AXTP Protocol

Stage 50. Run the deterministic AXTP generation stage after `adopt-protocol-draft` or `amend-adopted-protocol` has updated YAML facts.

## Boundaries

- Input is `contract/registry/**/*.yaml` and `contract/registry/domains/**/*.yaml`.
- Registry/Profile/Capability Types specs are governance context, not the machine input for generation.
- Do not infer new protocol facts from `workspace/protocol/**` or `specs/**` during generation.
- Do not hand-edit generated outputs.
- If validation fails because source YAML or Generator logic is wrong, stop and report the source issue unless the user asked you to fix it.

## Required Context

Read only what is needed:

```text
specs/50-tooling.md
contract/registry/**/*.yaml
contract/registry/domains/**/*.yaml
tooling/generators/package.json
```

If validation errors mention wire facts, also read the relevant specs:

```text
specs/20-core.md
specs/30-registry.md
specs/50-tooling.md
specs/40-codec.md
```

## Workflow

### 1. Confirm Inputs

Check the working tree and identify source changes:

```bash
git status --short
git diff --name-only -- contract/registry specs workspace/protocol tooling/generators
```

If only rough drafts changed and no YAML was adopted, do not generate. Route back to `adopt-protocol-draft`.

If only an adopted proposal or specs changed but the corresponding YAML facts were not amended, do not generate. Route to `amend-adopted-protocol` unless the change is documentation-only and intentionally does not affect generated output.

### 2. Run Generator Pipeline

Run:

```bash
pnpm --dir tooling/generators build
pnpm --dir tooling/generators test
pnpm --dir tooling/generators validate:sources
pnpm --dir tooling/generators generate
pnpm --dir tooling/generators validate:protocol
git diff --check
```

If pnpm blocks script execution because dependency build approval is pending, use and report:

```bash
pnpm --dir tooling/generators --config.verify-deps-before-run=false build
pnpm --dir tooling/generators --config.verify-deps-before-run=false test
pnpm --dir tooling/generators --config.verify-deps-before-run=false validate:sources
pnpm --dir tooling/generators --config.verify-deps-before-run=false generate
pnpm --dir tooling/generators --config.verify-deps-before-run=false validate:protocol
git diff --check
```

### 3. Verify Outputs

Expected generated outputs include:

```text
contract/protocol/axtp.protocol.yaml
contract/generated/protocol.md
contract/generated/protocol.json
contract/generated/*_registry.generated.md
contract/mcp/*.generated.json
contract/test-vectors/*
tooling/generators/src/__snapshots__/*
```

Use `git diff --name-only` and targeted `rg`/JSON checks to confirm adopted method/event/capability/schema names appear in Protocol IR and generated docs.

### 4. Handle Failures

- If `validate:sources` fails, the source YAML or specs alignment is wrong. Report the exact source facts to fix; do not patch generated files.
- If `generate` changes unexpected files, report them and inspect Generator configuration before proceeding.
- If `validate:protocol` fails, compare Protocol IR, generated docs, and relevant specs; fix source facts or Generator logic only if requested.
- If tests fail due to snapshots, inspect whether generated output changed intentionally before updating snapshots with the repo's Vitest snapshot workflow.

## Final Report

Report:

- Source YAML/spec inputs detected.
- Commands run and results.
- Generated files changed.
- Any validation failures or skipped checks.
- Confirmation that generated files were produced by Generator and not manually edited.
