# Security Policy

AXTP defines protocol contracts that runtime and SDK repositories consume. A
security issue may be a weakness in the wire protocol, generated schema,
conformance expectations, release artifact, or migration guidance.

## Supported Scope

Security review covers the current `main` branch and the latest published
`spec/v*` release tag.

Historical files under `docs/legacy-migration/evidence/**` are migration input
only and are not supported runtime contracts.

## Reporting A Vulnerability

Do not open a public issue for exploitable protocol flaws, credential exposure,
release-artifact tampering, or conformance bypasses.

Use GitHub private vulnerability reporting for `Mostorm-Labs/axtp`. If private
reporting is unavailable, contact the Mostorm Labs maintainers through the
established private project channel.

Please include:

- The affected spec tag or commit SHA.
- The affected transport path, method, event, schema, or artifact path.
- Reproduction steps or a minimal malformed frame/request when possible.
- Expected impact on runtimes, SDKs, devices, or release consumers.

Maintainers should acknowledge receipt, triage impact, prepare fixes privately
when needed, and publish release notes once downstream consumers have an update
path.
