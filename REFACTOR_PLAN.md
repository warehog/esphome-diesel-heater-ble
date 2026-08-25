# Diesel Heater Refactor Plan

## Goal

Support multiple diesel-heater protocols through one stable ESPHome and Home Assistant interface while keeping the implementation small, readable, safe, and practical for a 4 MB ESP32 device.

The goal is not to create a generic device framework. Keep one heater component, one BLE implementation, one normalized state model, and a small set of explicit protocol variants.

## Known Facts and Evidence Limits

The code and protocol notes describe four response formats:

- AA55 plain (MD1)
- AA55 encrypted (MD2)
- AA66 plain (MD3)
- AA66 encrypted (MD4)

Treat these as four known **response formats** within one AA request family. The AirHeaterBLE 1.20 APK provides stronger evidence than the previous README reconstruction:

- Its one notification callback decodes all four formats.
- Both request-construction sites use the same frame without an MD1-MD4 branch. Every request is `[0xAA, 0x55, passkey/100, passkey%100, command, value_lo, value_hi, checksum]`.
- The passkey is configurable from decimal 1 through 9999 and defaults to 1234. The two transmitted fields are base-100 decimal groups, not a big-endian binary integer.
- The checksum is the low byte of the sum of bytes 2 through 6.
- Core commands 1-4 and clock command 10 are used independently of response format.
- Extended commands 11-20 are exposed by the app only for encrypted MD2 and MD4 responses.
- The app sends one request per second and retries the previous command while the response command byte does not acknowledge it.

The readable extraction is preserved in `doc/app-v1.20-protocol-extract.js`. The source APK and extracted `app-service.js` hashes must be recorded with protocol-test fixtures so later app versions can be compared reproducibly.

This supports a shared AA request encoder and a universal status probe for all four response formats. It does not replace raw-frame fixtures or hardware tests. Record support as **app-derived** until exercised on the corresponding heater variant.

The APK also leaves one important ambiguity: its encrypted decoder executes six eight-byte XOR blocks, but JavaScript arrays grow when missing indexes are assigned. This does not prove that encrypted wire frames are exactly 48 bytes. MD2 reads through byte 34 and MD4 through byte 35; determine actual accepted wire lengths from captures.

## APK-Confirmed Corrections to the Existing Code

The refactor must include regression tests for each of these concrete mismatches:

- `ControllerSelector` currently enables commands only for MD2 and MD3. The APK uses the shared core command path for MD1-MD4, so all four need core capabilities after a valid response is selected.
- Detection currently examines only raw byte 1. Require plain signatures `AA 55`/`AA 66` or encrypted signatures `DA 34`/`DA 07`, then confirm the decrypted header.
- The decrypt loop indexes 48 bytes unconditionally. Decrypt only validated received bytes and reject a frame shorter than the fields required by its detected variant.
- Plain case and cabin temperatures and encrypted case temperature require signed 16-bit conversion. Temperature offset requires signed 8-bit conversion.
- Encrypted RF433 state must be derived from decrypted byte 29. The current parser incorrectly reads raw encrypted byte 29.
- Encrypted target level and temperature require range validation and unit normalization. Plain target temperature is unavailable when the running mode does not carry it and must not retain stale data.
- Normalize heater power from the app-observed state semantics: running states 0 and 3 are off; other observed states are on. Do not compare the raw multi-valued state directly with a Boolean command.
- The request passkey is currently fixed at 1234. Add the validated decimal `passkey` option and construct its two base-100 fields for every request.
- `SetLevelRequest` currently subtracts one, while the app sends the displayed manual level 1-10 directly for command 4. Retain the direct encoding used by `HeaterControllerAA` and remove the contradictory class.
- Extended request classes confuse decimal command numbers with hexadecimal literals. Use decimal IDs 10-20 or clearly named constants with their correct byte values.
- Temperature-unit data is 0 for Celsius and 1 for Fahrenheit; altitude-unit data is 0 for meters and 1 for imperial. Existing Boolean helper names/encodings are inverted.
- RF433 shares command 17 with oil-pump type and uses values 20/21. Temperature offset is sent as a signed value through the request's 16-bit little-endian field.
- Preserve `rcv_cmd` as an acknowledgement field rather than exposing it as normalized heater state.

## Carleeno Fork Assessment

The fork at `carleeno/esphome-diesel-heater-ble` was compared commit-by-commit with upstream `main`. Its AA66 controller, heater-class detection, scaling, unavailable-entity, and climate changes are already present upstream through merged pull requests. The only additional runtime feature on the fork's `main` is heater-clock synchronization from an ESPHome `RealTimeClock`.

Adopt the clock-sync feature **in adapted form**, not by cherry-picking the fork commits:

- The concept and command are valid: the APK sends decimal command 10 with minutes since midnight for MD1-MD4.
- Accept any ESPHome `time::RealTimeClock`; do not couple the component specifically to Home Assistant time.
- Make `time_id` optional. A heater configured without clock synchronization must not load or require the time platform.
- Encode minutes since midnight as the request's complete 16-bit little-endian value. The fork passes it through the current `uint8_t` request constructor, truncating every time after 04:15.
- Enqueue clock synchronization through the same serialized, acknowledged command path as other commands. The fork calls it immediately after a status write and can overlap GATT operations.
- Synchronize once after connection, protocol selection, valid heater state, and valid wall clock, then at a conservative bounded interval such as hourly. The APK refreshes every minute, but that frequency is not required without drift measurements.
- Update the last-successful-sync timestamp only after command acknowledgement or confirmed state, not merely after attempting a write.
- Treat clock sync failure as non-fatal and rate-limit logs and retries.

Do not adopt the fork's README replacement or example source change: they remove the protocol-analysis documentation and point users at the fork rather than the local/upstream component.

## Target Architecture

```text
climate / sensor / switch / number / button
                       │
                       ▼
               DieselHeaterBLE API
                       │
                       ▼
          selected/candidate protocol driver
                       │
                       ▼
       bounded command delivery + BLE lifecycle
```

`DieselHeaterBLE` remains the main ESPHome runtime component. It owns:

- the BLE connection lifecycle and characteristic handle;
- receive buffering if hardware demonstrates fragmented or combined notifications;
- protocol discovery and selection;
- the last complete normalized `HeaterState`;
- command admission and serialized GATT delivery;
- entity publication and unavailable-state handling.

Protocol drivers own only protocol-specific detection, validation, decoding, request encoding, value conversion, and capabilities. Do not add separate transport, service, event-bus, generic packet-mapping, or device frameworks.

## Proposed File Layout

```text
components/diesel_heater_ble/
  __init__.py  sensor.py  climate.py  switch.py  number.py  button.py
  heater.h    heater.cpp
  state.h
  protocol.h  protocol.cpp
  protocol_aa.h  protocol_aa.cpp
doc/
  app-v1.20-protocol-extract.js
tests/
  protocol_aa_test.cpp
  fixtures/
  esphome-local.yaml
  esphome-minimal.yaml
```

`messages.h` and `controllers.h` should be removed after their responsibilities have moved into the protocol files. Do not retain parallel old and new command paths.

The four formats should share one readable AA-family request encoder and may share one response implementation parameterized by a small explicit variant descriptor. Branch explicitly for response header, encryption, validated wire length, field offsets, byte order, scaling, and capabilities. Do not create a generic field-map engine or macro-based parser.

## Core Data Contracts

### Parse result

A Boolean parser result is insufficient. Use an explicit result:

```cpp
enum class ParseResult : uint8_t {
  OK,
  INCOMPLETE,
  UNKNOWN_PROTOCOL,
  INVALID_FRAME,
  UNSUPPORTED_MESSAGE,
};
```

Required behavior:

- Never index a frame until the relevant length has been checked.
- Detection must validate both header bytes. For encrypted frames, validate the complete encrypted header signature and then the decrypted header.
- Determine exact or minimum accepted lengths from real fixtures. Do not infer a 48-byte requirement solely from the current loop or accept 35/36 bytes solely from the highest documented field.
- Validate any known command byte, checksum, terminator, or invariant. Document fields for which no validation rule is known.
- Parse into a zero-initialized temporary state. Replace the published state only after `ParseResult::OK`; no error path may partially mutate the last valid state.
- Distinguish a recognized but short frame from a complete malformed frame.

If BLE captures show that a logical frame can span notifications or that one notification can contain multiple frames, add a small bounded receive accumulator owned by `DieselHeaterBLE`. Do not add buffering speculatively; first preserve the raw notification boundaries in test evidence.

### Normalized heater state

`HeaterState` is the contract consumed by entities. Define and test these invariants:

- Value-initialize every member and start with `valid == false`.
- Represent temperatures exposed to ESPHome in degrees Celsius, including conversion from Fahrenheit-speaking variants.
- Use signed types for signed protocol values such as case temperature and temperature offset.
- Normalize voltage, altitude, level, power, and temperature scaling in the driver.
- Read the encrypted temperature-unit field before normalizing that frame's target temperature. The app consults the previously stored unit, but the ESPHome implementation must not reproduce that first-frame ordering bug.
- Convert Fahrenheit target values to Celsius for entities and convert Celsius commands back to the heater's reported unit. Add explicit rounding and range tests for the app-observed Fahrenheit limits.
- Represent unavailable fields explicitly with validity flags or a small optional-field type; never reuse stale values or silently substitute zero.
- Keep raw protocol class, command byte, and variant-only diagnostics outside the universal control fields.
- Document the universal meanings of power, operating mode, level, and running step. If meanings differ, keep the raw value diagnostic-only and expose a separate normalized value.

On disconnect, invalidate the state and publish unavailable states where the ESPHome entity type supports it. Climate automation must not act on stale state.

### Capabilities and command results

Use a small capability bitmask rather than a capability framework. At minimum distinguish:

- status polling;
- power control;
- manual level control;
- thermostat/set-temperature control;
- mode changes required by level or temperature commands.
- clock synchronization;
- extended encrypted-format settings.

The initial capability matrix is evidence-backed as follows:

| Capability | MD1 | MD2 | MD3 | MD4 | Evidence |
|---|---:|---:|---:|---:|---|
| Status, power, mode, level, temperature, clock | Yes | Yes | Yes | Yes | Shared APK UI and request encoder |
| Auto times, language, units, tank, pump/RF433, automatic heating, offset | No | Yes | No | Yes | APK parameter UI is gated to MD2/MD4 |

Treat `No` as unavailable through the supported app, not proof that the firmware rejects the command. Do not expose it until stronger evidence exists.

An empty command list must not ambiguously mean unsupported, invalid, or no change. Command building returns an explicit status, for example:

```cpp
enum class CommandResult : uint8_t {
  READY,
  NO_CHANGE,
  UNSUPPORTED,
  STATE_UNAVAILABLE,
  INVALID_VALUE,
};
```

The climate entity is available only when the selected protocol supports the operations its current strategy needs. Its assumptions—Celsius range, levels 1 through 10, mode values, and heating-step meanings—must come from the normalized contract or declared capabilities, not raw AA bytes.

## Protocol Boundary

`Request` currently hard-codes passkey 1234 and represents the two value bytes as unrelated arguments. The APK confirms that the AA55 eight-byte request envelope is common to all four response variants, but the encoder must accept the configured passkey and a 16-bit little-endian value. Keep the AA request encoder inside the AA-family implementation and return fully encoded frames across the generic boundary:

```cpp
using CommandFrame = std::vector<uint8_t>;

struct CommandBatch {
  CommandResult result;
  std::vector<CommandFrame> frames;
};

class HeaterProtocol {
 public:
  virtual ~HeaterProtocol() = default;
  virtual ProtocolVariant variant() const = 0;
  virtual uint32_t capabilities() const = 0;
  virtual ParseResult parse(const std::vector<uint8_t> &frame, HeaterState &state) const = 0;
  virtual CommandBatch status_request() const = 0;
  virtual CommandBatch set_power(const HeaterState &, bool) const = 0;
  virtual CommandBatch set_level(const HeaterState &, uint8_t) const = 0;
  virtual CommandBatch set_temperature(const HeaterState &, float celsius) const = 0;
  virtual CommandBatch set_clock(uint16_t minutes_since_midnight) const = 0;
};
```

This is a behavioral sketch, not a requirement to allocate nested vectors on the device. Fixed-capacity storage is preferable if measurements show heap churn or code-size cost. Preserve the result and ownership semantics if the concrete representation is simplified.

Use statically owned driver instances, a tagged union, or another leak-free fixed set. Do not allocate a new driver on every connection.

All Home Assistant entities call `DieselHeaterBLE` command methods. They must not include protocol headers, raw mode values, offsets, encryption keys, checksums, or controller subclasses.

## Protocol Selection and Discovery

Add a validated YAML option:

```yaml
diesel_heater_ble:
  protocol: auto  # auto, aa55_plain, aa55_encrypted, aa66_plain, aa66_encrypted
  passkey: 1234   # decimal 1-9999; transmitted as two base-100 fields
  time_id: ha_time  # optional; accepts any ESPHome RealTimeClock
```

### Explicit selection

- Select the requested driver when the BLE connection is established.
- Require a complete successfully parsed matching response before enabling state-dependent commands.
- Reject and log a response that belongs to a different variant; never silently switch away from the configured protocol.
- Structure code generation so explicit selection can exclude unused drivers later. A runtime enum alone is not assumed to provide firmware-size exclusion.

### Auto selection

The APK establishes one status request for the entire AA family: command 1, value 0, using the configured passkey. Auto mode therefore has a safe bootstrap request without selecting a response variant first:

1. Accept and validate unsolicited responses if the device sends them.
2. Otherwise send the shared AA status request at the bounded polling interval.
3. Activate a response variant only after its decoder returns `ParseResult::OK` for a complete response.
4. Keep the selected driver fixed for the connection. Clear it on disconnect.
5. Bound probe rate and attempts; log a concise failure after all evidence-backed probes are exhausted.

An incorrect passkey produces no valid response in the app's connection flow. Discovery failure must distinguish timeout/no response from malformed or unknown response data without logging the passkey.

## Command Admission and BLE Delivery

Every command entry point must check, in order:

1. BLE is established and the characteristic handle is valid.
2. A protocol is selected.
3. The required capability is present.
4. Required state fields are valid and current.
5. The requested value is within the normalized range.

Log rejected commands with a rate-limited reason. Do not dereference a null driver and do not optimistically publish a requested value as confirmed heater state.

Some existing operations generate a mode command followed by a value command. The APK avoids overlapping writes by sending at one-second intervals and uses `rcv_cmd` to retry the last non-status command until acknowledged. The component does not need to copy that latency, but it must preserve both layers of ordering:

1. Do not submit a second GATT write until `ESP_GATTC_WRITE_CHAR_EVT` completes the first.
2. Do not advance a dependent command batch until the controller returns the expected response command or fresh state proves the operation succeeded.

Use a small bounded FIFO with bounded acknowledgement timeout and retry count. Define overflow, disconnect, stale acknowledgement, and final failure behavior. Status polling must not overtake a control command, and no queue may grow without bound.

Use decimal command IDs from the APK source. The currently unused specialized request classes mistakenly encode several decimal IDs as hexadecimal literals: for example app command 13 is `0x0D`, not `0x13`, and app command 20 is `0x14`, not `0x20`. Remove or correct all such classes during migration. Also preserve a full 16-bit value for clock and timer commands.

On disconnect:

- clear the selected/candidate protocol and discovery state;
- clear receive and command buffers;
- invalidate the state and characteristic handle;
- cancel retries and pending automation actions;
- publish entities unavailable where supported.

## ESPHome Entity and Code-Size Rules

- Keep one implementation of each entity platform.
- Preserve existing YAML names and entity behavior unless a documented safety correction requires a breaking change.
- Remove unconditional loading of all optional entity platforms from the core component.
- Avoid including sensor, number, switch, button, and climate headers from the core header when forward declarations and conditional compilation suffice.
- Compile both a full example and a minimal BLE-only configuration to prevent optional-platform coupling from returning.
- Prefer explicit parsing over macros and metaprogramming.
- Avoid dynamic allocation in polling and notification hot paths.
- Record firmware binary size before the refactor and after each architectural step. Report both total size and remaining application-partition margin.
- Test `protocol: auto` and every explicit selection. Confirm whether explicit variants actually remove unused code before relying on that as a size optimization.
- Add UART or other transports only after BLE behavior is stable and supported by hardware evidence.

## Validation Strategy

### Fixtures and host tests

Before changing behavior, add anonymized raw frames captured from real devices where available. Each fixture records variant, wire length, source, expected normalized fields, and whether command behavior was hardware-tested.

For every variant, test:

- a representative valid status frame;
- every accepted frame length;
- every prefix length from zero to one byte shorter than the minimum;
- incorrect first and second header bytes;
- malformed encrypted data and decrypted-header mismatch;
- invalid checksum or terminator when applicable;
- signed temperatures and offsets;
- byte order and scaling;
- unsupported message types;
- no mutation of the previous state on any failure.

Do not invent a representative frame to make a test pass. A variant without a real fixture remains explicitly unverified.

Command tests assert exact encoded bytes independently for each evidence-backed variant. Unsupported variants assert `CommandResult::UNSUPPORTED`.

At minimum, request tests must cover:

- default passkey/status: `1234, 1, 0 -> AA 55 0C 22 01 00 00 2F`;
- passkey boundaries 1 and 9999 and invalid 0/10000 configurations;
- 16-bit little-endian values and checksum wraparound;
- exact decimal command IDs 1-4 and 10-20;
- clock values `00:00 -> 0`, `04:15 -> 255`, `04:16 -> 256`, and `23:59 -> 1439`, including both value bytes and checksum;
- configurations with no time component, a valid time source, and a time source that is initially invalid;
- clock scheduling after connection and acknowledgement, without overlapping the status poll;
- core capability availability for MD1-MD4 and extended capability availability only for MD2/MD4;
- response-command acknowledgement, timeout, bounded retry, and dependent mode/value ordering.

### ESPHome configurations

The checked-in `esphome.yaml` currently references GitHub `main`, so it does not validate working-tree changes. Add local test YAML that loads `components/` from this checkout. Validate at least:

- the full example with all entities;
- a minimal component with no optional entity platforms;
- `protocol: auto`;
- each explicit protocol value;
- invalid protocol configuration.

For each reviewable step run, when the toolchain is available:

```bash
esphome config tests/esphome-local.yaml
esphome compile tests/esphome-local.yaml
esphome config tests/esphome-minimal.yaml
esphome compile tests/esphome-minimal.yaml
```

Hardware validation for an affected variant covers detection, normalized entity state, command bytes, multi-frame command ordering, reconnect, unavailable states, timeouts, and safe fallback. Clearly label untested variants in the pull request and protocol documentation.

## Refactor Sequence

Each step must compile independently and must not mix unrelated behavior changes.

1. **Capture the baseline.** Record full and minimal firmware sizes, current warnings, known hardware variants, raw notification boundaries, and command evidence.
2. **Make validation local.** Add local full/minimal ESPHome configs and a host parser-test target without changing runtime behavior.
3. **Define contracts.** Add parse results, protocol variants, normalized state validity, capabilities, and command results.
4. **Make parsing safe and atomic.** Add length/header checks, signed conversions, temporary-state parsing, negative tests, and verified fixtures for the current parser.
5. **Separate AA protocol behavior.** Move parsing and request encoding from `messages.h` and `controllers.h` into the AA-family driver without claiming new command support.
6. **Implement selection and discovery.** Add explicit configuration, evidence-backed auto probes, successful-parse activation, and mismatch handling.
7. **Harden lifecycle behavior.** Reset driver/state/buffers on disconnect, mark entities unavailable, and safely reject premature commands.
8. **Serialize commands where required.** Test existing multi-frame operations and add bounded acknowledgement-driven delivery if back-to-back writes are not reliable.
9. **Add optional clock synchronization.** Adapt the useful Carleeno feature using the full 16-bit command value, optional generic time source, serialized delivery, and acknowledgement-based scheduling.
10. **Migrate entities to the universal API.** Remove raw protocol knowledge from climate, number, switch, and button paths; enforce capabilities and normalized ranges.
11. **Remove obsolete paths.** Delete the old parser/controller/request types and unused lifecycle fields after all callers migrate.
12. **Verify size and optional builds.** Compile the full/minimal and auto/explicit matrix, with and without a time source, compare firmware sizes with the baseline, and remove accidental optional-platform coupling.
13. **Expand only from evidence.** Enable commands for additional variants only with exact-frame tests and hardware or capture evidence.

## Completion Criteria

The refactor is complete only when:

- no parser performs an unchecked indexed read;
- invalid or incomplete input cannot mutate published state or select a driver;
- auto discovery has a documented bootstrap path and bounded failure behavior;
- every command is either byte-tested for that variant or explicitly unsupported;
- multi-frame commands cannot issue overlapping GATT writes;
- disconnect makes stale state unavailable and prevents automation commands;
- entities depend only on normalized state and `DieselHeaterBLE` methods;
- full and minimal local configurations compile;
- clock synchronization compiles away when omitted and sends the correct 16-bit value when configured;
- all fixture and negative parser tests pass;
- firmware size and remaining partition margin are documented;
- protocol documentation distinguishes verified behavior from assumptions and untested hardware.
