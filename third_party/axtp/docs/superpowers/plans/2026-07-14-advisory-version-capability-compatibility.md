# Advisory Version And Capability Compatibility Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make `Hello.axtpVersion` diagnostic-only and guarantee cross-version feature degradation through profiles, capabilities, and canonical `NOT_SUPPORTED` behavior in the AXTP spec and every maintained runtime.

**Architecture:** The AXTP repository owns normative rules and shared machine-readable conformance cases. Each runtime consumes those cases through its existing conformance adapter, removes any version-based session gate, and distinguishes unknown methods from registered-but-unavailable methods. Runtime repositories are updated only after the shared cases are published at a fixed Spec commit.

**Tech Stack:** Markdown and YAML protocol contracts, Node.js/TypeScript generators, Vitest, C/CMake/CTest, C++/CMake/CTest, Dart, Python/pytest, GitHub Actions.

---

## File Map

### AXTP specification repository

- Modify `specs/20-core.md`: make `axtpVersion` advisory and define session behavior.
- Modify `specs/30-registry.md`: define capability lookup and canonical degradation errors.
- Modify `specs/40-codec.md`: make unknown optional-field handling explicit for cross-version peers.
- Modify `conformance/cases/session/hello_identify_identified.yaml`: stop requiring `axtpVersion` in the baseline Hello case; RPC envelope fields are specified in `specs/20-core.md`, not a registry schema file.
- Modify `contract/registry/error/error_code.yaml`: document canonical send behavior for `NOT_SUPPORTED` and compatibility-only receive behavior for older specific errors.
- Create `conformance/cases/session/axtp_version_advisory.yaml`: handshake version matrix.
- Create `conformance/cases/capability/registered_method_not_supported.yaml`: known-but-unavailable behavior.
- Create `conformance/cases/capability/session_survives_not_supported.yaml`: session remains usable after degradation.
- Create `conformance/cases/event/unknown_event_ignored.yaml`: unknown-event compatibility.
- Modify `conformance/cases/capability/unsupported_method.yaml`: reserve `RPC_METHOD_NOT_FOUND` for genuinely unknown methods.
- Modify `conformance/manifest.yaml` and `conformance/README.md`: register and document the matrix.
- Regenerate `contract/protocol/axtp.protocol.yaml`, `contract/generated/**`, and generator snapshots using the existing generator.

### Runtime repositories

- TypeScript: `src/core/handshake.ts`, `tests/core/handshake.test.ts`, `tests/sdk/clientConnectionErrors.test.ts`, `devtools/conformance/conformance.test.ts`.
- C++: `include/core/protocol/wire/websocket_json_rpc/outbound/json_rpc_encoder.hpp`, `include/core/runtime/core/rpc_dispatcher.hpp`, `tests/core/phase3_outbound_test.cpp`, `tests/core/phase4_core_test.cpp`, `devtools/conformance/conformance_runner.cpp`.
- C: `src/codec.c`, `src/broker.c`, `src/media_profile.c`, `tests/sdk_test.c`, `tests/media_profile_test.c`, and `devtools/conformance/conformance_runner.c`.
- Flutter: `lib/src/wire.dart`, `test/axtp_flutter_test.dart`, and `test/conformance/conformance_test.dart`.
- Python: `src/axtp_runtime/wire_io.py`, `src/axtp_runtime/broker.py`, `tests/test_runtime.py`, and `tests/conformance/test_conformance.py`.
- Mock server: `generated/node-mock-server/src/main.ts`, `generated/node-mock-server/src/smoke.ts`, `generated/cpp-mock-server/src/main.cpp`, `generated/cpp-mock-server/tests/smoke.cpp`, generator source `devtools/generators/src/emitters/mockServer.ts`, and `devtools/conformance/conformance_runner.mjs`.
- All runtimes: each repository's runtime profile and upgrade workflow only where required to include the new shared cases.

## Task 1: Make Advisory Version Behavior Normative

**Files:**
- Modify: `specs/20-core.md`
- Modify: `conformance/cases/session/hello_identify_identified.yaml`
- Test: `tooling/generators/src/__snapshots__/protocol.generated.md`

- [ ] **Step 1: Write a failing source assertion**

Add a conformance validation assertion that the baseline Hello case permits an absent `axtpVersion`, while the version matrix carries explicit present values. The assertion must fail while the baseline case still requires the field.

- [ ] **Step 2: Run the focused validation and confirm failure**

Run: `tooling/scripts/validate-conformance.sh`

Expected: failure naming the old required Hello version expectation.

- [ ] **Step 3: Update the normative core text**

State explicitly:

```text
Hello.axtpVersion is optional advisory metadata. A receiver MUST NOT reject,
delay, retry, reconnect, or otherwise change session admission because the
field is absent, malformed, or differs in major, minor, or patch.
```

Keep unsupported Frame Header `Version` rejection as a separate wire parsing rule.

- [ ] **Step 4: Update the machine source and regenerate**

Remove the required version assertion from the baseline Hello case, add the diagnostic wording to generated-facing documentation, and run:

```bash
pnpm --dir tooling/generators build
pnpm --dir tooling/generators test
pnpm --dir tooling/generators validate:sources
pnpm --dir tooling/generators generate
pnpm --dir tooling/generators validate:protocol
```

Expected: all commands pass and generated protocol artifacts contain the advisory wording.

- [ ] **Step 5: Commit**

```bash
git add specs/20-core.md conformance/cases/session/hello_identify_identified.yaml contract/protocol contract/generated tooling/generators/src/__snapshots__
git commit -m "Define axtpVersion as advisory metadata"
```

## Task 2: Define Canonical Capability Degradation

**Files:**
- Modify: `specs/30-registry.md`
- Modify: `specs/40-codec.md`
- Modify: `contract/registry/error/error_code.yaml`
- Test: generator protocol validation tests

- [ ] **Step 1: Add failing registry validation assertions**

Assert that the generated descriptions preserve these distinct cases:

```text
unknown method -> RPC_METHOD_NOT_FOUND
registered but unavailable -> NOT_SUPPORTED
malformed supported request -> validation error
```

- [ ] **Step 2: Run protocol validation and confirm failure**

Run: `pnpm --dir tooling/generators validate:protocol`

Expected: failure until the error source and normative text agree.

- [ ] **Step 3: Add normative degradation rules**

Document caller capability discovery, receiver-side enforcement, unknown optional-field skipping, unknown-event tolerance, and the requirement that a degraded operation leave the session usable.

- [ ] **Step 4: Clarify error registry descriptions**

Keep old specific errors decodable, but document `NOT_SUPPORTED` as the canonical error new senders use for a registered method unavailable under the current runtime/device/profile/capability set.

- [ ] **Step 5: Regenerate and verify**

Run the full generator pipeline from Task 1 and `git diff --check`.

Expected: all pass; no unrelated generated drift.

- [ ] **Step 6: Commit**

```bash
git add specs/30-registry.md specs/40-codec.md contract/registry/error contract/protocol contract/generated tooling/generators/src/__snapshots__
git commit -m "Define capability based compatibility degradation"
```

## Task 3: Add Shared Cross-Version Conformance Cases

**Files:**
- Create: `conformance/cases/session/axtp_version_advisory.yaml`
- Create: `conformance/cases/capability/registered_method_not_supported.yaml`
- Create: `conformance/cases/capability/session_survives_not_supported.yaml`
- Create: `conformance/cases/event/unknown_event_ignored.yaml`
- Modify: `conformance/cases/capability/unsupported_method.yaml`
- Modify: `conformance/manifest.yaml`
- Modify: `conformance/README.md`

- [ ] **Step 1: Add the handshake matrix case**

Represent rows for `1.0.0 -> 1.1.0`, `1.1.0 -> 1.0.0`, patch difference, `2.0.0`, malformed, and absent values. Every row must assert that Identify is sent and the session can reach Identified.

- [ ] **Step 2: Run conformance validation and confirm unsupported rows fail**

Run: `tooling/scripts/validate-conformance.sh`

Expected: failure until the manifest/schema recognizes the new matrix case structure.

- [ ] **Step 3: Add capability degradation cases**

Use an adopted registry method with a fixture declaring its capability unavailable. Assert exact `NOT_SUPPORTED`, then issue a core supported request on the same session and assert success. Keep a separate vendor-unknown method case expecting exact `RPC_METHOD_NOT_FOUND`.

- [ ] **Step 4: Add schema/event tolerance cases**

Assert that an unknown structurally valid optional field and an unknown event do not close the session; follow each with a supported request proving liveness.

- [ ] **Step 5: Validate all conformance inputs**

Run:

```bash
tooling/scripts/validate-conformance.sh
pnpm --dir tooling/generators validate:protocol
git diff --check
```

Expected: all pass.

- [ ] **Step 6: Commit**

```bash
git add conformance
git commit -m "Add cross version compatibility matrix"
```

## Task 4: Publish A Fixed Spec Commit For Runtime Work

**Files:** none beyond Tasks 1-3

- [ ] **Step 1: Run the complete AXTP verification suite**

```bash
pnpm --dir tooling/generators build
pnpm --dir tooling/generators test
pnpm --dir tooling/generators validate:sources
pnpm --dir tooling/generators generate
pnpm --dir tooling/generators validate:protocol
tooling/scripts/validate-conformance.sh
git diff --check
```

Expected: zero failures and clean generated output.

- [ ] **Step 2: Push the reviewed spec branch**

Use a `codex/` feature branch and record its exact commit SHA. Runtime work must pin `AXTP_SPEC_PATH` or the lock file to this SHA until a new `spec/v*` release exists.

## Task 5: Remove TypeScript Version Admission

**Files:**
- Modify: `../axtp-ts-runtime/src/core/handshake.ts`
- Modify: `../axtp-ts-runtime/tests/core/handshake.test.ts`
- Modify: `../axtp-ts-runtime/tests/sdk/clientConnectionErrors.test.ts`
- Modify: `../axtp-ts-runtime/devtools/conformance/conformance.test.ts`

- [ ] **Step 1: Replace rejection tests with failing advisory matrix tests**

Parameterize Hello values:

```ts
it.each(["1.0.0", "1.1.0", "1.0.1", "2.0.0", "broken", ""])(
  "continues Identify for advisory axtpVersion %j",
  (version) => {
    const result = client.handle(helloMsg("", version));
    expect(result.error).toBeUndefined();
    expect(result.outbound?.op).toBe(RpcOp.Identify);
  }
);
```

Update real-WebSocket tests to expect connection progress rather than deterministic version failure.

- [ ] **Step 2: Run focused tests and confirm failure**

Run: `pnpm vitest run tests/core/handshake.test.ts tests/sdk/clientConnectionErrors.test.ts`

Expected: major, malformed, and absent rows fail under the old gate.

- [ ] **Step 3: Remove the admission check**

Delete `isAxtpVersionCompatible`. Store/classify the observed string only in existing diagnostics; always generate Identify from `handleHello` when role and state are valid.

- [ ] **Step 4: Add exact method degradation tests**

Assert registered-but-disabled dispatch returns `ErrorCode.NotSupported`, while an unknown method remains `RpcMethodNotFound`, and both leave a subsequent request usable.

- [ ] **Step 5: Run TypeScript verification**

```bash
pnpm build
pnpm test
pnpm lint
AXTP_SPEC_PATH=../axtp devtools/scripts/run-conformance.sh
```

Expected: all pass, including the shared matrix.

- [ ] **Step 6: Commit**

```bash
git add src tests devtools/conformance
git commit -m "Use capability degradation for cross version sessions"
```

## Task 6: Align C++ Hello And Degradation

**Files:**
- Modify: `../axtp-cpp-runtime/include/core/protocol/wire/websocket_json_rpc/outbound/json_rpc_encoder.hpp`
- Modify: `../axtp-cpp-runtime/include/core/runtime/core/rpc_dispatcher.hpp`
- Modify: `../axtp-cpp-runtime/tests/core/phase3_outbound_test.cpp`
- Modify: `../axtp-cpp-runtime/devtools/conformance/conformance_runner.cpp`

- [ ] **Step 1: Add failing Hello and dispatcher tests**

Assert Hello emits advisory protocol metadata rather than using the runtime lock as an admission signal. Add dispatcher tests for exact `NOT_SUPPORTED`, exact `RPC_METHOD_NOT_FOUND`, and a successful follow-up request.

- [ ] **Step 2: Run focused C++ tests and confirm failure**

Run the existing CMake target containing `phase3_outbound_test` and dispatcher tests with `ctest --output-on-failure`.

- [ ] **Step 3: Implement minimal C++ behavior**

Do not add an inbound `axtpVersion` gate. Route registered-but-unavailable methods to `ErrorCode::NotSupported`; retain not-found only for absent registry entries.

- [ ] **Step 4: Run full C++ verification and shared conformance**

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
AXTP_SPEC_PATH=../axtp devtools/scripts/run-conformance.sh
```

Expected: all declared-profile cases pass.

- [ ] **Step 5: Commit**

```bash
git add include src tests devtools/conformance
git commit -m "Align C++ runtime with advisory version compatibility"
```

## Task 7: Align C Runtime

**Files:**
- Modify: `../axtp-c-runtime/src/broker.c`
- Modify: `../axtp-c-runtime/src/media_profile.c`
- Modify: `../axtp-c-runtime/tests/sdk_test.c`
- Modify: `../axtp-c-runtime/tests/media_profile_test.c`
- Modify: `../axtp-c-runtime/devtools/conformance/conformance_runner.c`

- [ ] **Step 1: Add failing exact-error and liveness tests**

Test unknown, registered-but-unavailable, and successful follow-up requests. Add all advisory Hello rows supported by the C runtime's declared RPC profile.

- [ ] **Step 2: Run focused C tests and confirm failure**

Run: `cmake -S . -B build && cmake --build build && ctest --test-dir build --output-on-failure`.

- [ ] **Step 3: Implement registry-aware degradation**

Return `NOT_SUPPORTED` only after registry lookup succeeds but support/profile lookup fails. Do not weaken Frame Header version validation in `src/codec.c`.

- [ ] **Step 4: Verify and commit**

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
AXTP_SPEC_PATH=../axtp devtools/scripts/run-conformance.sh
git add src include tests devtools/conformance
git commit -m "Add capability based compatibility to C runtime"
```

## Task 8: Align Flutter Runtime

**Files:**
- Modify: `../axtp-flutter-runtime/lib/src/wire.dart`
- Modify: matching handshake/dispatch tests under `../axtp-flutter-runtime/test/`
- Modify: `../axtp-flutter-runtime/test/conformance/conformance_test.dart`

- [ ] **Step 1: Add failing advisory and degradation tests**

Parameterize absent, malformed, minor, patch, and major-different Hello values. Add exact error and post-error liveness assertions.

- [ ] **Step 2: Run focused Dart tests and confirm failure**

Run: `dart test test/axtp_flutter_test.dart test/conformance/conformance_test.dart`.

- [ ] **Step 3: Remove any version gate and implement exact degradation**

Keep Hello metadata diagnostic-only. Preserve unknown-method lookup, and map registered-but-unavailable support checks to `NOT_SUPPORTED`.

- [ ] **Step 4: Verify and commit**

```bash
dart pub get
dart analyze lib test tool
dart test
AXTP_SPEC_PATH=../axtp devtools/scripts/run-conformance.sh
git add lib test
git commit -m "Use capability degradation in Flutter runtime"
```

## Task 9: Align Python Runtime

**Files:**
- Modify: `src/axtp_runtime/wire_io.py`
- Modify: `src/axtp_runtime/broker.py`
- Modify: `tests/test_runtime.py`
- Modify: `tests/conformance/test_conformance.py`

- [ ] **Step 1: Add failing matrix tests**

Use `pytest.mark.parametrize` for the six advisory values and separate exact-error/liveness tests.

- [ ] **Step 2: Run focused pytest and confirm failure**

Run: `python -m pytest tests/test_runtime.py tests/conformance/test_conformance.py -q`.

- [ ] **Step 3: Implement diagnostic-only version handling and degradation**

Do not reject Hello from `wire_io.py` or session code. Return the generated common `NOT_SUPPORTED` value after successful registry lookup and failed support lookup.

- [ ] **Step 4: Verify and commit**

```bash
python -m pip install -e '.[test]'
python -m pytest
AXTP_SPEC_PATH=../axtp devtools/scripts/run-conformance.sh
git add src tests
git commit -m "Add advisory version compatibility to Python runtime"
```

## Task 10: Align Mock Server

**Files:**
- Modify: `devtools/generators/src/emitters/mockServer.ts`
- Modify: `generated/node-mock-server/src/main.ts`
- Modify: `generated/node-mock-server/src/smoke.ts`
- Modify: `generated/cpp-mock-server/src/main.cpp`
- Modify: `generated/cpp-mock-server/tests/smoke.cpp`
- Modify: `devtools/conformance/conformance_runner.mjs`

- [ ] **Step 1: Add failing server matrix tests**

Test that all advisory Hello values proceed, registered disabled fixtures return exact `NOT_SUPPORTED`, unknown methods return exact `RPC_METHOD_NOT_FOUND`, and a follow-up request succeeds.

- [ ] **Step 2: Run focused tests and confirm failure**

Run: `pnpm --dir generated/node-mock-server test` and `cmake -S generated/cpp-mock-server -B build/generated-cpp-mock-server && cmake --build build/generated-cpp-mock-server && ctest --test-dir build/generated-cpp-mock-server --output-on-failure`.

- [ ] **Step 3: Implement server degradation**

Use the generated method registry before capability/profile support lookup; never close a WebSocket or TCP RPC session for feature unavailability.

- [ ] **Step 4: Verify and commit**

```bash
pnpm install --frozen-lockfile
pnpm build
pnpm test
AXTP_SPEC_PATH=../axtp devtools/scripts/run-conformance.sh
git add src generated tests devtools/conformance
git commit -m "Add cross version degradation to mock server"
```

## Task 11: Upgrade Workflow And Cross-Repository Verification

**Files:**
- Modify: each runtime's upgrade workflow and runtime profile file for C.
- Modify: each runtime's upgrade workflow and runtime profile file for C++.
- Modify: each runtime's upgrade workflow and runtime profile file for Flutter.
- Modify: each runtime's upgrade workflow and runtime profile file for TypeScript.
- Modify: each runtime's upgrade workflow and runtime profile file for Python.
- Modify: each runtime's upgrade workflow and runtime profile file for the mock server.

- [ ] **Step 1: Assert each declared RPC profile includes advisory cases**

Run every repository's `devtools/scripts/run-conformance.sh` against the same fixed AXTP commit. A profile may skip only cases for profiles it does not declare.

- [ ] **Step 2: Ensure upgrade workflows run conformance before PR creation**

For each workflow, verify the conformance step precedes `Create upgrade PR` and does not use `CONFORMANCE_ALLOW_INCOMPLETE=true` for the new required matrix.

- [ ] **Step 3: Run the complete per-repository verification commands**

Repeat Tasks 4-10 verification from clean checkouts. Expected: zero failures in every repository.

- [ ] **Step 4: Trigger dry-run or test dispatches**

Trigger the upgrade workflow against the fixed Spec ref in a non-release test path. Confirm each run reaches PR creation and reports the shared matrix rows.

- [ ] **Step 5: Review compatibility evidence**

Record, per runtime: supported profiles, six handshake rows, unknown method result, registered unsupported result, unknown optional field/event behavior, and post-error session liveness.

- [ ] **Step 6: Final commits and PRs**

Create one focused PR per repository. Do not combine runtime implementations into the Spec PR. Link every runtime PR to the Spec PR and include its verification evidence.
