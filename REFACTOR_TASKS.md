# Diesel Heater Refactor Execution Tasks

## Purpose

This file converts `REFACTOR_PLAN.md` into an executable backlog. Every task is an independently reviewable unit with a fixed boundary, tests, validation, compilation, evidence, and a completion gate. A later task must not be used to excuse missing tests or a broken build in an earlier task.

The task numbers define dependency order, not permission to combine tasks. Combining adjacent tasks requires an explicit plan update explaining why their boundary can no longer be preserved.

## Agent Execution Protocol

Use at most three subagents alongside the root coordinator:

1. **Implementer:** the only agent allowed to edit files for the task.
2. **Reviewer:** read-only; reviews the completed diff, contracts, safety, and scope.
3. **Validator:** read-only except for disposable build output under `/tmp`; independently runs tests and compilation.

Never allow two agents to edit the shared workspace concurrently. The root coordinator assigns the task, resolves reviewer findings, verifies the final diff, and is the only agent that declares the task complete.

### Agent tiers

| Tier | Suggested model and effort | Use |
|---|---|---|
| H | Frontier/strong agent such as `gpt-5.6-sol`, high or xhigh effort | Protocol contracts, unsafe-input parsing, state machines, ownership, concurrency, and final protocol claims |
| M | Balanced agent such as `gpt-5.6-terra`, medium or high effort | ESPHome schemas, lifecycle integration, entity migration, optional features, and review |
| L | Fast agent such as `gpt-5.6-luna`, low or medium effort | Mechanical setup, command execution, cleanup searches, build matrices, and evidence collation |

Agent names are examples, not permanent requirements. Preserve the relative intelligence/effort assignment if available models change.

### Required task report

The implementer, reviewer, and validator reports must collectively record:

- files changed and why;
- tests added and their provenance;
- exact commands run and their results;
- ESPHome version, compiler/toolchain version, firmware size, and partition margin when applicable;
- hardware/capture evidence classified as `hardware-verified`, `capture-verified`, `app-derived`, `synthetic`, or `unavailable`;
- reviewer findings and how each was resolved;
- known limitations that remain outside the task boundary.

### Universal completion guard

A task is complete only when all of the following are true:

- The stated deliverables exist and excluded work did not leak into the diff.
- Task-specific unit tests pass.
- The entire existing host test suite passes.
- `git diff --check` passes.
- Both local full and minimal ESPHome configurations validate and compile, unless the task defines a different gate. T00 uses its evidence-only gate because the local configurations do not exist until T01.
- Required hardware validation passes for behavior that changed on a real heater.
- The reviewer reports no unresolved correctness or maintainability finding.
- The validator independently reproduces the required tests and builds.
- Generated `.esphome/`, CMake output, credentials, captures containing sensitive identifiers, and virtual-environment files are not committed.

If ESPHome or required hardware is unavailable, report the task as `blocked` or `code complete, validation pending`; do not mark it complete. Installing a missing toolchain or obtaining hardware access requires its own authorization and is not silently folded into an implementation task.

## Standard Validation Commands

Task T01 establishes the exact commands. Unless it deliberately changes the harness, every later task runs:

```bash
cmake -S tests -B /tmp/diesel-heater-host-tests
cmake --build /tmp/diesel-heater-host-tests --parallel
ctest --test-dir /tmp/diesel-heater-host-tests --output-on-failure
esphome config tests/esphome-local.yaml
esphome compile tests/esphome-local.yaml
esphome config tests/esphome-minimal.yaml
esphome compile tests/esphome-minimal.yaml
git diff --check
git status --short
```

Use a new disposable `/tmp` build directory when stale output could hide a dependency error. Configure the test YAML files to place ESPHome build output under distinct `/tmp` paths so compilation does not create a workspace `.esphome/` directory. The host harness must not download a test framework.

## T00 — Capture Baseline and Evidence Inventory

**Depends on:** Nothing
**Implementer:** L/medium
**Reviewer:** M/medium
**Validator:** L/low

### Objective

Create a reproducible record of available protocol evidence and toolchain state before runtime code changes. Firmware sizing is deferred to T01 because the checked-in example imports GitHub `main` and cannot measure the working tree.

### In scope

- Add `doc/validation.md` with toolchain versions and baseline results.
- Add `tests/fixtures/README.md` defining fixture provenance and privacy requirements.
- Record hashes and provenance for `airheaterble-1-20.apk` and `.analysis/apk/assets/apps/__UNI__1471D01/www/app-service.js`, matching `doc/app-v1.20-protocol-extract.js`. The disposable `.analysis/` path must not become a committed build dependency.
- Inventory known MD1-MD4 hardware, captures, notification lengths/boundaries, and command verification status.
- Record why the checked-in remote-source YAML is not valid working-tree build evidence. Record unavailable measurements as unavailable, never as zero.

### Out of scope

- Runtime changes, parser fixes, new configuration keys, invented response fixtures, and protocol-support claims.

### Tests and validation

- Verify recorded SHA-256 values against the workspace files.
- Run non-mutating toolchain/version checks; do not present a compile of the remote component as a local baseline.
- Confirm every fixture entry identifies its source as real capture, app-derived, or synthetic.

### Completion gate

The evidence inventory is reproducible by another agent, unknown evidence is explicitly marked unknown, and no source behavior changed.

## T01 — Add Local Build and Host-Test Harness

**Depends on:** T00
**Implementer:** M/medium
**Reviewer:** H/high
**Validator:** L/medium

### Objective

Make local component changes testable without importing GitHub `main` and provide a dependency-free native C++ test target.

### In scope

- Add `tests/esphome-local.yaml` with all supported entities and local `components/` source.
- Add `tests/esphome-minimal.yaml` with BLE and the heater component but no optional entity platforms.
- Add `tests/CMakeLists.txt` and a standard-assertion C++ smoke test.
- Document the standard commands and keep build output under `/tmp` or ignored paths.
- Compile protocol-only host targets with `-Wall -Wextra -Wpedantic`, review warnings without imposing `-Werror` on ESPHome builds, and register every executable with CTest.
- Before any runtime change, record working-tree full/minimal warnings, firmware image size, and remaining application-partition margin in `doc/validation.md`.

### Out of scope

- Runtime behavior, protocol contracts, parsing changes, external test-framework downloads, and firmware-size optimization.

### Tests and validation

- Configure, build, and run the host smoke test from a clean build directory.
- Run `esphome config` and `esphome compile` for both YAML files.
- Assert that the test YAML contains no `github://` or other remote external-component source, and inspect configuration logs to prove the local component is used.

### Completion gate

Both local configurations compile, the native smoke test passes, working-tree baseline sizes are recorded, and no generated artifacts are tracked.

## T02 — Define Protocol, State, Capability, and Command Contracts

**Depends on:** T01
**Implementer:** H/high
**Reviewer:** H/xhigh
**Validator:** M/medium

### Objective

Introduce additive definitions for the minimal stable types needed by later parsing and command work without changing runtime behavior or replacing legacy state yet.

### In scope

- Define `ProtocolVariant`, `ParseResult`, `CommandResult`, capability flags, fixed command frames/batches, and initialized normalized state.
- Define field-validity semantics, signed/unit invariants, and static ownership expectations. T03, not this task, performs the atomic state-consumer cutover.
- Add compile-time and unit assertions for initialization, capability mapping, fixed capacities, and invalid-state behavior.
- Provide an adapter if legacy code needs it to compile.

### Out of scope

- Moving parsing, enabling variants, sending new commands, queueing, YAML schema changes, and entity behavior changes.

### Tests and validation

- Test default state initialization and unavailable fields.
- Test capability combinations for MD1-MD4 as app-derived evidence.
- Review stack/heap implications and prohibit per-notification allocation in the contract.
- Run the standard host and ESPHome gates.

### Completion gate

Contracts are documented, compiled, unit-tested, unused or bridged without runtime drift, and approved by the high-effort reviewer.

## T03 — Make Legacy Parsing Safe and Atomic

**Depends on:** T02
**Implementer:** H/xhigh
**Reviewer:** H/xhigh
**Validator:** M/high

### Objective

Remove current memory-safety and stale-state hazards before architectural cutover while preserving the existing runtime entry points.

### In scope

- Validate both raw header bytes and decrypted headers.
- Check every indexed read against the variant's required safe read span. Do not treat the highest accessed index or a synthetic frame as proof of an accepted complete wire length.
- Decrypt only received bytes.
- Parse into a temporary state and commit only on `ParseResult::OK`.
- Correct signed case/cabin temperatures, signed temperature offset, decrypted RF433 access, running-state normalization, and per-mode field validity.
- Make the normalized state contract final, remove superseded raw/uninitialized state fields, and update every state-reading consumer atomically; no consumer may keep interpreting a normalized field as its former raw value.
- Add exhaustive short-prefix and no-mutation tests.

### Out of scope

- New protocol files, passkey configuration, controller selection changes, command encoding, queueing, and entity redesign beyond consuming corrected state.

### Tests and validation

- Test every prefix from zero through one byte below each minimum required length.
- Test wrong first/second headers, encrypted-header mismatch, unsupported messages, and malformed frames.
- Test signed extrema, byte order, scaling, field invalidation, and preservation of the previous valid state on every failure.
- Specifically verify that a plain non-temperature mode invalidates target temperature rather than retaining an earlier value.
- Validate response checksums, terminators, or invariants only where evidence defines them; document unknown validation rather than inventing a rule.
- Run a clean sanitizer build with `cmake -S tests -B /tmp/diesel-heater-host-asan -DENABLE_SANITIZERS=ON`, build it, and run CTest. If unsupported, record the compiler and exact failure instead of silently skipping it.
- Run the standard host and ESPHome gates and hardware state-publication checks on available variants.

### Completion gate

No indexed read is unchecked, no failure mutates published state, corrected values match evidence, and all current supported behavior still compiles and operates.

## T04 — Cut Over to the AA-Family Protocol Driver

**Depends on:** T03
**Implementer:** H/xhigh
**Reviewer:** H/xhigh
**Validator:** M/high

### Objective

Atomically replace the legacy parser/controller/request architecture with one AA-family driver and one request encoder, without leaving competing live paths.

### In scope

- Add `protocol.h/.cpp` and `protocol_aa.h/.cpp` using the contracts from T02.
- Move MD1-MD4 detection/parsing and shared AA request construction into the driver.
- Implement passkey-aware, 16-bit little-endian request encoding and decimal command constants.
- Use statically owned variant instances; remove heap allocation.
- Migrate all current runtime parser, command construction, and entity-facing command callers in the same task. Entities may continue calling `DieselHeaterBLE`, but no caller may retain a legacy request/controller type.
- Use the final static driver registry and successful-parse auto activation with default passkey 1234; do not add a temporary replacement selector before T05 exposes configuration policy.
- Return safe `STATE_UNAVAILABLE`/`UNSUPPORTED` results when no driver has been selected so the cutover cannot introduce a pre-detection crash.
- Add Celsius/Fahrenheit command conversion and range tests at the driver boundary.
- Delete `messages.h`, `controllers.h`, `ControllerSelector`, contradictory request subclasses, and their unused fields after migration.
- Delete every adapter introduced by T02/T03 before completing the cutover.

### Out of scope

- New YAML protocol/passkey keys, explicit selection, asynchronous command queueing, clock scheduling, optional-platform cleanup, and new extended entities.

### Tests and validation

- Re-run every T03 parser test unchanged against the new driver.
- Test exact status request `1234, 1, 0 -> AA 55 0C 22 01 00 00 2F`.
- Test passkey 1/9999, values 255/256/1439, checksum wraparound, and command IDs 1-4 and 10-20.
- Test signed 16-bit value encoding, including a negative temperature-offset command.
- Use `rg` to prove no live legacy symbol or include remains.
- Run the standard host/ESPHome gates and compare firmware size with the T01 working-tree baseline.
- Hardware-test status, power, mode, level, and temperature construction on every available variant without promoting unavailable variants beyond their evidence label.

### Completion gate

There is exactly one live parser path, one AA encoder, one permanent static registry, no driver allocation leak, no migration adapter or legacy compatibility path, and behavior is byte-for-byte covered by tests.

## T05 — Add Explicit Configuration and Safe Auto Selection

**Depends on:** T04
**Implementer:** M/high
**Reviewer:** H/high
**Validator:** M/medium

### Objective

Add validated `protocol` and `passkey` configuration and select a response driver only from a successful complete parse.

### In scope

- Add `protocol: auto|aa55_plain|aa55_encrypted|aa66_plain|aa66_encrypted`.
- Add decimal `passkey` validation from 1 through 9999.
- Use the shared command-1/value-0 request as the auto-discovery probe.
- Require a valid matching response before activation.
- Reject explicit-protocol mismatches without silently switching.
- Bound discovery polling and failure logs.

### Out of scope

- Compile-time driver exclusion, command queueing, entity-capability presentation, and clock synchronization.

### Tests and validation

- Host-test auto selection, every explicit variant, mismatch rejection, malformed/incomplete frames, bad-passkey timeout state, and selection stability per connection.
- Test that discovery-probe bytes contain the configured passkey, command 1, and value 0.
- Run configuration validation for auto and all four explicit variants, plus negative configurations for an invalid protocol and passkeys 0/10000.
- Compile auto and all four explicit variants; if generated source is demonstrably identical, document that evidence before reducing redundant firmware builds.
- Hardware-test discovery for each available heater variant.

### Completion gate

No invalid frame can select a driver, auto has a bounded bootstrap path, explicit selection is deterministic, and configuration errors fail before compilation.

## T06 — Harden Disconnect, Reconnect, and Command Admission

**Depends on:** T05
**Implementer:** M/high
**Reviewer:** H/high
**Validator:** M/high

### Objective

Make lifecycle transitions safe before introducing asynchronous command delivery.

### In scope

- Centralize command precondition checks for connection, handle, selected driver, capability, state validity, and value range.
- On disconnect clear selection, state, handles, discovery/lifecycle work, and automation intent that could send stale commands. T07 separately adds queue and in-flight cleanup when those objects exist.
- Publish unavailable entity state where ESPHome supports it.
- Ensure reconnect requires fresh protocol/state confirmation.
- Rate-limit rejection and lifecycle logs without revealing the passkey.

### Out of scope

- FIFO implementation, acknowledgement retries, entity API redesign, clock sync, and optional-platform compile cleanup.

### Tests and validation

- Exercise disconnect during discovery, valid idle state, command admission, and reconnect.
- Test every command rejection reason and ensure it performs no BLE write.
- Verify stale state is never republished after reconnect.
- Verify each registered entity is marked unavailable exactly once, emits no stale `publish_state` afterward, and recovers after rapid disconnect/reconnect.
- Run the standard gates and hardware reconnect/unavailable-state tests.

### Completion gate

Every public command fails safely outside a valid session, disconnect leaves no actionable stale state, and reconnect begins from a clean state.

## T07 — Implement Serialized and Acknowledged Command Delivery

**Depends on:** T06
**Implementer:** H/xhigh
**Reviewer:** H/xhigh
**Validator:** H/high

### Objective

Replace immediate/back-to-back BLE writes with a bounded deterministic state machine.

### In scope

- Add a fixed-capacity FIFO and exactly one in-flight GATT write.
- Wait for `ESP_GATTC_WRITE_CHAR_EVT` before awaiting controller acknowledgement.
- Match `response_command` or validated fresh state to the in-flight operation.
- Sequence dependent mode-then-value batches.
- Define bounded timeout, retry, queue-full, stale-acknowledgement, write-failure, status-poll suppression, and disconnect behavior.
- Expose deterministic failure results to callers/logs.

### Out of scope

- New entity features, clock synchronization, transport abstraction, unbounded storage, and speculative UART support.

### Tests and validation

- Use a deterministic fake BLE transport/clock.
- Test one-in-flight enforcement, matching/nonmatching/stale acknowledgements, retries, final timeout, write failure, overflow, dependent ordering, poll fairness, millis wraparound, and disconnect clearing.
- Prove that write completion alone never dequeues a command, an acknowledgement received before write completion cannot advance it, and fresh-state acknowledgement is tied to the in-flight command and timestamp.
- Run the standard gates.
- Hardware-test multi-frame mode/level and mode/temperature commands, reconnect during a command, and timeout recovery.

### Completion gate

No execution path overlaps GATT writes, no queue grows unbounded, dependent commands preserve order, and hardware evidence confirms reliable delivery on available variants.

## T08 — Audit Capability-Aware Entity and Climate Behavior

**Depends on:** T07
**Implementer:** M/high
**Reviewer:** H/high
**Validator:** M/high

### Objective

Complete the entity-level behavior audit after the T04 API cutover: make climate, number, switch, and button behavior consistently use normalized state, admission results, and declared capabilities.

### In scope

- Verify all controls already route through the T04 `DieselHeaterBLE` API; remove any remaining entity-layer protocol knowledge discovered by the static guard.
- Enforce normalized Celsius, level, power, and mode ranges.
- Make climate strategies require the capabilities they use.
- Handle unsupported operations explicitly instead of treating an empty batch as success.
- Publish confirmed heater state only; do not optimistically overwrite it.
- Test external-temperature automation against invalid/disconnected state.

### Out of scope

- New entities for extended settings, clock scheduling, parser changes, and optional-platform header cleanup.

### Tests and validation

- Test every entity action for supported, unsupported, invalid-range, stale-state, and disconnected cases.
- Test entity-level normalized Celsius behavior and the climate's mode-plus-value sequences; protocol-unit conversion itself is owned and tested by T04.
- Test that rejected, failed, or timed-out commands never update entity state optimistically.
- Add a static `rg`/review guard proving entity sources do not include `protocol*.h`, `messages.h`, or `controllers.h` and do not encode AA headers, raw commands, modes, or checksums.
- Compile the full entity configuration and run hardware publication/control/fallback checks.
- Run the standard gates.

### Completion gate

No entity includes protocol headers or raw mode/checksum knowledge, all unsupported behavior is visible and safe, and climate automation cannot bypass command admission.

## T09 — Remove Optional-Platform Coupling and Measure Size

**Depends on:** T08
**Implementer:** M/medium
**Reviewer:** H/high
**Validator:** L/medium

### Objective

Ensure unused ESPHome entity platforms are neither required nor accidentally compiled and measure the result.

### In scope

- Remove unconditional `AUTO_LOAD` of optional platforms from the core component.
- Replace unnecessary core-header includes with forward declarations and appropriate compile guards.
- Ensure each platform loads only its own dependencies.
- Compile full and minimal configurations and record binary-size deltas and partition margin.
- Investigate, but do not assume, compile-time exclusion for explicit protocol selection.

### Out of scope

- Changing entity behavior, adding protocols/transports, and speculative metaprogramming solely for size.

### Tests and validation

- Compile minimal BLE-only configuration with none of climate/sensor/number/switch/button/time.
- Keep the no-time minimal build as a formal checked-in fixture, not an ad hoc edit of another YAML file.
- Compile the full configuration.
- Record the exact build-map path and queried protocol symbols before claiming code exclusion; report exclusion as unproven if the necessary map/symbol evidence is unavailable.
- Run the host suite and record size changes against the T01 working-tree baseline.

### Completion gate

Minimal builds do not require optional headers/platforms, full builds retain behavior, and every size claim has compiler evidence.

## T10 — Add Optional Heater-Clock Synchronization

**Depends on:** T07, T09
**Implementer:** M/high
**Reviewer:** H/high
**Validator:** M/high

### Objective

Adapt the useful Carleeno fork feature without its truncation, mandatory dependency, or overlapping-write defects.

### In scope

- Add optional `time_id` accepting any ESPHome `RealTimeClock`.
- Encode command 10 with full minutes-since-midnight as a 16-bit value.
- Enqueue an initial sync only after valid time, connection, selected protocol, and valid state.
- Add conservative periodic resynchronization through the T07 dispatcher.
- Update successful-sync state only after acknowledgement.
- Make failure non-fatal and rate-limited.

### Out of scope

- Requiring Home Assistant time, mandatory time platform loading, timezone policy changes, and copying the fork commits or README.

### Tests and validation

- Test absent/invalid time and values `00:00 -> 0`, `04:15 -> 255`, `04:16 -> 256`, and `23:59 -> 1439`.
- Validate rejection of an invalid or non-`RealTimeClock` `time_id`, plus successful configuration/compilation with a generic non-Home-Assistant time platform.
- Test a time source that becomes valid after connection and ensure it triggers exactly one initial synchronization.
- Test acknowledgement-only success timestamp, retry/rate limit, reconnect, and no status-write overlap.
- Compile full/minimal configurations both with and without `time_id`.
- Hardware-verify heater clock behavior on an available device.

### Completion gate

Clock sync is correct across the byte boundary, uses the normal command state machine, remains optional at compile time, and cannot degrade heater control when it fails.

## T11 — Final Documentation, Cleanup, and Complete Build Matrix

**Depends on:** T10
**Implementer:** L/medium
**Reviewer:** M/high
**Validator:** M/high

### Objective

Prove the refactor is complete, remove residual scaffolding, and publish reproducible support status and size results.

### In scope

- Audit for adapters and migration scaffolding that earlier completion gates required to be absent; finding one reopens its owning task. Remove only unrelated dead includes or documentation duplication owned by final qualification.
- Update `README.md`, `doc/protocols.md`, `doc/validation.md`, example YAML, and evidence statuses.
- Run the complete full/minimal × auto/explicit × with/without-time matrix.
- Record warnings, binary size, partition margin, and hardware results for each available variant.
- Search for unchecked frame access, raw protocol knowledge in entities, heap driver allocation, and legacy symbols.

### Out of scope

- New protocols, new transports, new extended-setting entities, and unrelated cleanup.

### Tests and validation

- Run all host tests from a clean build directory.
- Compile full/minimal auto configurations, all four explicit variants, and with/without time where applicable.
- Run scoped searches over `components/diesel_heater_ble/*.{h,cpp}` for `ResponseParser|Request|HeaterController|ControllerSelector|messages\.h|controllers\.h`, dynamic driver allocation matching `new .*Protocol|new .*Controller`, and entity-layer protocol constants such as `0xAA|0x55|0x66`. Review expected protocol-driver matches rather than searching documentation.
- Run hardware reconnect, command-ordering, timeout, unavailable-state, climate fallback, and clock checks.
- Compare final size with the T01 working-tree baseline and explain every material regression.

### Completion gate

All completion criteria in `REFACTOR_PLAN.md` are evidenced, documentation distinguishes app-derived from hardware-verified behavior, the complete matrix passes, and no migration scaffolding remains.

## T12 — Evidence-Gated Capability Expansion

**Depends on:** T11 for the initial refactor; repeatable afterward
**Implementer:** H/high
**Reviewer:** H/xhigh
**Validator:** M/high

### Objective

Add one previously unavailable protocol capability at a time without converting assumptions into support claims.

### In scope

- Exactly one command/capability or one new response variant per execution.
- Add capture/APK/hardware provenance, exact frame tests, capability change, documentation, and hardware result.
- Extend normalized state only when a universal or explicitly diagnostic field is justified.

### Out of scope

- Bundled speculative commands, unrelated UI work, generic transport frameworks, and promotion based only on synthetic tests.

### Tests and validation

- Test exact request bytes and all affected response fields and failure cases.
- Run the standard gates plus the configuration variants affected by the capability.
- Require hardware validation before marking the capability `hardware-verified`; otherwise retain the weaker evidence label.

### Completion gate

The single capability is isolated, fully tested, correctly labelled, documented, compiled, and independently reviewed without weakening existing variants.

## Dependency Summary

```text
T00 → T01 → T02 → T03 → T04 → T05 → T06 → T07 → T08 → T09 → T10 → T11
                                                                    │
                                                                    └→ T12 (repeatable)
```

T00 evidence collection and T01 harness design may be researched in parallel, but T01 cannot complete until T00 defines evidence and validation recording. All runtime tasks remain sequential because agents share the workspace and each task establishes contracts consumed by the next.
