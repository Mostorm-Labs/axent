# Contributing

AXTP is a specification repository. Changes must preserve the source-of-truth
chain from `registry/**` to `protocol/axtp.protocol.yaml`, `docs/generated/**`,
tooling outputs, conformance cases, and release artifacts.

## Source Of Truth

- Edit `registry/**` for machine-readable protocol facts.
- Edit `docs/specs/**`, `docs/protocol/**`, `docs/conformance/**`, and
  `docs/release/**` for human governance and review materials.
- Do not hand-edit `protocol/axtp.protocol.yaml`, `docs/generated/**`,
  `tooling/mcp/**`, or `tooling/test-vectors/**`; regenerate them.
- Treat `docs/legacy-migration/evidence/**` as historical input, not a runtime
  contract.

## Local Setup

```sh
pnpm --dir generators install --frozen-lockfile
pnpm --dir generators build
```

## Before Opening A Change

Run the same checks that CI runs:

```sh
pnpm --dir generators lint
pnpm --dir generators test
pnpm --dir generators validate
pnpm --dir generators validate:sources
pnpm --dir generators validate:protocol
scripts/validate-conformance.sh
scripts/check-generated-drift.sh
scripts/check-links.mjs
scripts/check-protocol-status.mjs
scripts/check-release-artifact.sh 0.0.0
```

If registry facts changed, run:

```sh
pnpm --dir generators generate
```

Commit the generated outputs together with the source change.

## Review Expectations

- Protocol changes should state the affected domain, capability, methods,
  events, schemas, errors, and conformance impact.
- Release changes should state the artifact contract paths that were added,
  removed, or renamed.
- Documentation changes should keep local Markdown links valid.
- Security-sensitive behavior should avoid public exploit details; follow
  `SECURITY.md`.
