# AXTP Spec Generator

This generator is owned by the AXTP spec repository.

It generates spec repository artifacts only:

- `contract/protocol/axtp.protocol.yaml`
- `contract/generated/*`
- `contract/mcp/*.generated.json`
- `contract/test-vectors/*`

Runtime repositories maintain their own generators for runtime-specific outputs.

```bash
pnpm --dir tooling/generators install
pnpm --dir tooling/generators build
pnpm --dir tooling/generators test
pnpm --dir tooling/generators validate:sources
pnpm --dir tooling/generators generate
pnpm --dir tooling/generators validate:protocol
```
