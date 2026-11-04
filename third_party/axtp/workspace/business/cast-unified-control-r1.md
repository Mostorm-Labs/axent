# Cast unified control r1 — combined P0/P1 adoption input

Date: 2026-10-20. Source: NearCast `AXTP_UNIFIED_CONTROL_EXTENSION_REQUEST.md`
r1 (2026-10-20), sections 3–12, and UNIFIED_PRESENTATION_SETTINGS_PLAN.md.
The user's combined implementation request authorizes P0/P1 in
`Mostorm-Labs/axtp` only. The source document's
historical statements about stopping checks or future work are context, not
execution instructions. Baseline: `a2a550313bcf570361f1bfe9872851141b847520`
(`spec/v0.17.0`); its cast surface matches the product's older spec/v0.15.0 lock.

Users need to discover concurrent HID/AirPlay/WebCast receivers and sessions,
withdraw one exact presentation, set one session's audio intent, and recover
after event loss, timeout or reconnection without affecting a replacement slot.
The host owns state capture, mutation, media cleanup, admission and audio policy.
AXTP owns the observable contract, identities, limits, synchronization and errors.

P0 includes four methods, five events, three optional capabilities, additive
getStatus/windowChanged fields, F1–F6, generated metadata and C01–C11 conformance.
The follow-up user request explicitly extends this work to full P1 settings and
C12 / PS-01–PS-12 protocol guarantees. It supersedes the earlier P0-only scope.
The latest local `UNIFIED_PRESENTATION_SETTINGS_PLAN.md` and revised r1 §9 are
the P1 authority; the capability matrix and App plan explain host ownership.
Their historical proposed/not-authorized statements do not override this request.
No facts are taken from the superseded r0 wire draft.
No transport, CONTROL handshake, media codec, global uint64 rule, runtime code,
submodule pointer, product queue default or layout algorithm changes are in scope.

Review: exact public shapes and compatibility are resolved in the four feature
proposals, [P0 contract](../../specs/45-cast-control.md) and
[P1 contract](../../specs/46-cast-settings.md). New entries
remain registry `draft`; adoption into source is distinct from release maturity.
No legacy numeric mapping is invented. No new MVP requirement is introduced.

## Execution closure

Repository: github / Mostorm-Labs/axtp. Task anchor: the baseline above, ancestor
of the result. Local reviewable worktree is the requested result boundary;
publishing a release and downstream integration are separate work.

```yaml
EXECUTION_CLOSURE_CONTRACT:
  implementation:
    required_changes: [preserved P0 registry and schemas, complete P1 settings schemas and bindings, normative P0/P1 flows and failure branches, review records and examples, generated artifacts, C01-C12 and PS-01-PS-12 protocol conformance, unreleased migration and verification notes]
    forbidden_changes: [hand-edited generated output, legacy or P0 ID reuse, downstream repository changes, product media or disk implementation, release publication, automatic receiver observation or restart]
  tests:
    required:
      - {id: sources, command_or_oracle: build/lint/validate/validate-sources/validate-protocol, expected_result: exit zero, blocking_reason: source and output contract}
      - {id: conformance, command_or_oracle: generator tests and validate-conformance, expected_result: exit zero, blocking_reason: executable reference evidence and case linkage}
      - {id: reproducibility, command_or_oracle: check-generated-drift and diff-check, expected_result: exit zero, blocking_reason: generated outputs reproducible}
      - {id: compatibility, command_or_oracle: frozen baseline identifier and field comparison, expected_result: no changed or removed legacy facts, blocking_reason: old protocol compatibility}
  hosted_verification: {required: [], optional: [hosted CI after publication]}
  evidence:
    blocking: [local command results, executable conformance, source diff]
    corroborative: [release artifact dry run]
  terminal_success:
    all_of: [P0 and P1 surfaces generated, independent settings synchronization, C01-C12 and PS-01-PS-12 linked and validated, required checks pass, downstream unexecuted work explicitly handed off]
  terminal_blockers:
    explicit_classes: [AUTHORITY_CONFLICT, MISSING_REQUIRED_INPUT, ENVIRONMENT_BLOCKER, FROZEN_VERIFICATION_FAILURE, NEW_HIGH_IMPACT_FAILURE_MODE]
  return_policy: {continue_until_terminal_state: true}
```

## P1 review reconciliation

The initial P0 source is frozen in conformance/fixtures/cast-control/p0-baseline.json.
Its four methods, five events, three capabilities and all existing field IDs are
retained. New P1 IDs append after P0 in each independent registry space.

- The short early r1 settings state is replaced by the detailed PS plan: desired,
  applied, startup intent and known stored values are distinct; save precedes
  configuration/live application when persist=true.
- The new settings event has its own cursor and barrier. P0-only unifiedControl
  still has exactly six required sections; settings is explicitly requested.
- P1 global operations have no session/slot target and do not fabricate a P0
  appliedRevision. They use required-on-kind settingsResult. P0 operations keep
  their exact target/revision rules. P1 progress/results use RPC/query; their
  settings notifications do not become P0 operation event sequence entries.
- StartupSettings admits the same six optional setting keys as Values, conditioned
  on capability.restartOnlyFields. The PS plan names three startup keys for
  NearCast's first implementation; permitting the other keys in the schema lets
  another implementation declare a different live/restart partition without
  hard-coding NearCast's policy globally. The NearCast-shaped fixture uses the
  original three restart-only keys.
- Disk paths, file algorithms, default choices and physical media acceptance stay
  host-owned; the conformance store and policy owners are substitutes.
