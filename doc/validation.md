# Pre-refactor workspace bootstrap validation

This is the workspace bootstrap performed on 2026-08-25 after the root
`esphome.yaml` was switched to the local `components/diesel_heater_ble` source.
It is not the T01 full/minimal ESPHome baseline. Validation used Python 3.13.3,
ESPHome 2026.8.1,
ESP-IDF 5.5.5, and the ESP32 GCC 14.2.0 toolchain. The configuration uses the
repository's local `components/diesel_heater_ble` source.

The following commands completed successfully:

```sh
make bootstrap
make config
make compile
make verify
cd dist && sha256sum -c SHA256SUMS
```

The target is a classic 4 MB ESP32 using the `lolin32_lite` board definition,
DIO flash mode, and a 40 MHz flash frequency. ESPHome reports a 1,117,803-byte
application payload (60.9%). Its 1,117,920-byte padded OTA binary leaves 717,088
bytes in the 1,835,008-byte app partition. RAM usage is 68,228 of 124,580 bytes
(54.8%).

| Artifact | Size | SHA-256 |
| --- | ---: | --- |
| `dist/heater-adapter-ble.factory.bin` | 1,183,456 bytes | `91da073dbffa32bc1aceee37d04b9b6b937ecbcada4af9fb50122e80cdcda894` |
| `dist/heater-adapter-ble.ota.bin` | 1,117,920 bytes | `a53176a1eb695509de0f22f177acca40730535ab93d12a84166fd4bf7e1a2dfd` |

`make verify-artifacts` validates the OTA image metadata and confirms that the
merged factory image contains byte-identical build outputs at the offsets from
ESP-IDF's generated flash manifest:

| Offset | Segment |
| ---: | --- |
| `0x1000` | bootloader |
| `0x8000` | partition table |
| `0x9000` | initial OTA data |
| `0x10000` | application |

The factory image is ready for an initial serial flash only after confirming the
connected board is a 4 MB Lolin32 Lite-compatible ESP32. Set real Wi-Fi credentials
and the target heater MAC in ignored `secrets.yaml`, then rebuild; the reference
binary was built with placeholder Wi-Fi values. No physical board or heater was
available, so serial flashing, BLE interoperability, and on-device behavior remain
hardware validation steps. Use `make flash PORT=/dev/ttyUSB0` with the verified
serial port; this invokes ESPHome's normal upload flow and its generated flash
metadata.

## T00 protocol evidence baseline

This section is the evidence-only baseline for the refactor. It does not add
runtime support or establish hardware compatibility. Commands below were run
from the repository root on 2026-08-25.

### Reproducible source hashes

The APK hash identifies the user-supplied local file
`airheaterble-1-20.apk`; its original acquisition date is unavailable. The
source URL listed in `README.md` is `https://apkpure.com/airheaterble/com.clj.airheater`,
but this record does not claim that URL is stable or that the file was obtained
from it. Following the extraction instructions in `README.md`, its ZIP member
`assets/apps/__UNI__1471D01/www/app-service.js` was extracted into the
disposable `.analysis/` directory. The readable protocol-only extract was then
prepared as `doc/app-v1.20-protocol-extract.js`. The latter is a reformatted,
annotated derivative and is **not byte-identical** to the app-service member.

| Evidence item | Provenance | Bytes | SHA-256 |
| --- | --- | ---: | --- |
| `airheaterble-1-20.apk` | User-supplied local workspace file; acquisition date and source unavailable (README only lists a candidate URL) | 23001762 | `c3658487ac7fc0120c98d44d9465b85f2bfec43d440e69fd3c87a4fe7caa53fa` |
| `.analysis/apk/assets/apps/__UNI__1471D01/www/app-service.js` | Raw ZIP member extracted from the APK; disposable analysis output | 327550 | `cde1c0021fdf21c246408241d867b356f13b09cd3cf69129313f7577614829b2` |
| `doc/app-v1.20-protocol-extract.js` | Human-readable, protocol-only app-derived transcription | 6357 | `8024a65f2829f201759a5e8afac83d019eebfa487569e9b84b13cde778189bdb` |

The hashes are reproducible with:

```sh
mkdir -p .analysis/apk/assets/apps/__UNI__1471D01/www
unzip -p airheaterble-1-20.apk \
  assets/apps/__UNI__1471D01/www/app-service.js \
  > .analysis/apk/assets/apps/__UNI__1471D01/www/app-service.js
sha256sum airheaterble-1-20.apk \
  .analysis/apk/assets/apps/__UNI__1471D01/www/app-service.js \
  doc/app-v1.20-protocol-extract.js
wc -c .analysis/apk/assets/apps/__UNI__1471D01/www/app-service.js \
  doc/app-v1.20-protocol-extract.js
```

The raw APK and raw app-service file are source evidence. The readable extract
is app-derived evidence for control flow and constants; it is not a substitute
for raw notification captures or a hardware test.

### MD1-MD4 evidence inventory

| Variant | Response description | Hardware evidence | Capture evidence | Notification length/boundary | Command verification |
| --- | --- | --- | --- | --- | --- |
| MD1 | Plain `AA 55` | unavailable | unavailable | App/docs describe fields through byte 16; exact accepted wire length and notification boundary unavailable | app-derived: shared core request path; hardware result unavailable |
| MD2 | Encrypted, decrypted `AA 55` | unavailable; README reports ownership of the third protocol but provides no reproducible test record | unavailable | app-derived: decoder reads through byte 34 and runs six 8-byte XOR blocks; exact accepted wire length and notification boundary unknown | app-derived: shared core commands; extended UI gated to encrypted variants; hardware result unavailable |
| MD3 | Plain `AA 66` | unavailable | unavailable | App/docs describe fields through byte 17; exact accepted wire length and notification boundary unavailable | app-derived: shared core request path; hardware result unavailable |
| MD4 | Encrypted, decrypted `AA 66` | unavailable | unavailable | app-derived: decoder reads through byte 35 and runs six 8-byte XOR blocks; exact accepted wire length and notification boundary unknown | app-derived: shared core commands; extended UI gated to encrypted variants; hardware result unavailable |

No raw BLE captures, packet-boundary logs, command acknowledgements, or heater
fixtures are present in this workspace. Therefore no variant is marked
`hardware-verified` or `capture-verified`, and no response length is treated as
proven by the app's six-block decryption loop. No synthetic response fixtures
were added in T00.

### Toolchain baseline

The non-mutating version checks used for this evidence inventory reported:

```text
Python 3.13.3
ESPHome 2026.8.1
cmake version 3.31.6
GNU Make 4.4.1
c++ (Ubuntu 14.2.0-19ubuntu2) 14.2.0
```

The existing firmware bootstrap validation above is retained for toolchain
context, but it is not a working-tree refactor baseline: prior to T01 the
checked-in example imported the published GitHub component rather than a local
test configuration. T00 therefore records no local full/minimal compile size,
warning baseline, or partition measurement.

## T01 local build and host-test baseline

This is the working-tree baseline required before refactor runtime changes.
It was finalized on 2026-08-26 with the toolchain recorded above. Both test
configurations use only `type: local` external components at `../components`;
their configuration output resolves that path to this checkout. They contain no
`github://`, URL, or Git external-component source. Their builds are explicitly
placed under `/tmp`, so no generated ESPHome output is required in the working
tree. The test MAC `02:00:00:00:00:01` is a locally administered placeholder,
not a heater identifier.

`tests/esphome-local.yaml` exercises the core component plus every presently
supported entity platform and option: climate, 22 sensors, two numbers, power
switch, and four buttons. `tests/esphome-minimal.yaml` intentionally contains
only ESP32 BLE tracking, a BLE client, and the core heater component; it has no
climate, sensor, number, switch, button, or time platform.

The following clean-run commands completed successfully:

```sh
cmake -S tests -B /tmp/diesel-heater-host-tests
cmake --build /tmp/diesel-heater-host-tests --parallel
ctest --test-dir /tmp/diesel-heater-host-tests --output-on-failure
.venv/bin/esphome config tests/esphome-local.yaml
.venv/bin/esphome compile tests/esphome-local.yaml
.venv/bin/esphome config tests/esphome-minimal.yaml
.venv/bin/esphome compile tests/esphome-minimal.yaml
! rg -n -i 'github://|https?://|type: git|url:' \
  tests/esphome-local.yaml tests/esphome-minimal.yaml
git diff --check
git status --short
```

The dependency-free CMake target `protocol_request_smoke` compiled with
`-Wall -Wextra -Wpedantic` and passed CTest (1/1). It tests only the existing
request encoder so it can build without ESPHome headers and does not define any
new protocol behavior.

| Configuration | Image payload | Padded OTA artifact | App partition | Remaining app margin | Clean-build warnings |
| --- | ---: | ---: | ---: | ---: | --- |
| Full local entities | 660,551 bytes | 660,672 bytes | 1,835,008 bytes | 1,174,336 bytes (64%) | One existing `-Wformat` warning in `climate.cpp:121`: `%u` receives the `unsigned long` result of `millis()` subtraction. |
| Minimal core | 651,527 bytes | 651,648 bytes | 1,835,008 bytes | 1,183,360 bytes (64%) | The same existing `climate.cpp:121` `-Wformat` warning. `AUTO_LOAD` brings optional platform support into this baseline build even when no optional entity is configured. |

ESPHome compilation deliberately remains without `-Werror`. The full-build
format warning is baseline evidence for a later behavior-preserving cleanup;
T01 does not alter runtime code. Both factory and OTA artifacts were generated
under their respective `/tmp/diesel-heater-esphome-*` directories. ESPHome may
create ignored `tests/.esphome` metadata while reading a test configuration; no
firmware binaries are placed there.

The remote-source rejection command above completed with shell status 0 because
`rg` found no match (the leading `!` makes that expected no-match a success).
`git diff --check` completed with status 0 and no output, so it found no
whitespace errors. `git status --short` completed with status 0 and reported
only the intentional T01/bootstrap working-tree changes; no generated firmware
artifact was tracked.

## T06 lifecycle and command-admission validation

T06 was validated on 2026-08-26 using the T01 toolchain (ESPHome 2026.8.1,
ESP-IDF 5.5.5, and ESP32 GCC 14.2.0). The dependency-free
`lifecycle_admission_test` covers ordered rejection for connection, handle,
driver, capability, state, and value failures; each rejected path has zero
writes through the same dispatch gate used by `DieselHeaterBLE`. It also covers
discovery and idle disconnects, distinct exactly-once unavailable edges for
all 24 registered sensor/number identities, stale-publication suppression,
and rapid reconnect requiring a fresh complete response. ESPHome 2026.8.1
does not provide a native unavailable notification for this component's
switch or climate base classes; T06 clears their local state without claiming
a frontend unavailable publication.

The following commands completed successfully:

```sh
cmake -S tests -B /tmp/diesel-heater-host-tests
cmake --build /tmp/diesel-heater-host-tests --parallel
ctest --test-dir /tmp/diesel-heater-host-tests --output-on-failure
cmake -S tests -B /tmp/diesel-heater-host-asan -DENABLE_SANITIZERS=ON
cmake --build /tmp/diesel-heater-host-asan --parallel
ctest --test-dir /tmp/diesel-heater-host-asan --output-on-failure
.venv/bin/esphome config tests/esphome-local.yaml
.venv/bin/esphome compile tests/esphome-local.yaml
.venv/bin/esphome config tests/esphome-minimal.yaml
.venv/bin/esphome compile tests/esphome-minimal.yaml
git diff --check
```

Both host suites passed 5/5 tests; the sanitizer suite reported no ASan/UBSan
finding. Full local compilation produced a 664,391-byte image with 1,170,496
bytes of application-partition headroom. Minimal compilation produced a
654,699-byte image with 1,180,192 bytes of headroom. No compatible heater or
BLE adapter is available in this workspace, so disconnect/reconnect and
unavailable-state behavior are synthetic/host-tested; hardware verification
remains unavailable.

## T02 protocol, state, capability, and command contracts

T02 was implemented on 2026-08-26. It adds the dependency-free,
currently-unused `components/diesel_heater_ble/contracts.h` and a host-only
`protocol_contracts_test`. The live `HeaterState`, `ResponseParser`, legacy
controllers, request encoder, and entity consumers remain unchanged; there is
no bridge because nothing in the runtime consumes these contracts yet.

The header defines the four app-described response variants, explicit parse and
command outcomes, a fixed capability bitmask, an eight-byte `CommandFrame`, a
two-frame fixed `CommandBatch`, and `NormalizedHeaterState`. Its universal
controls are a Boolean power state, an explicit manual-level/thermostat mode,
manual level, and target temperature. Power maps raw running states 0 and 3 to
off and every other raw state to on; the raw byte remains a valid diagnostic.
Raw mode and step values remain valid diagnostic fields; no universal meaning
is claimed for raw running-step codes.
Only raw modes 1 and 2 produce the
normalized manual-level and thermostat modes; other raw modes are unavailable
as controls.

Temperatures are Celsius, voltage is volts, and altitude is metres. Temperature
offset is a signed `float` Celsius delta so a Fahrenheit-source offset can be
converted by multiplying by 5/9 without rounding. Every common and extended
observation has explicit validity, including error code; start/auto/run timer
values; automatic start/stop; language; tank volume; pump type; RF433;
temperature and altitude units; and automatic heating. The extracted app does
not establish the physical unit of the three reported timer values, so they
carry the explicit `PROTOCOL_TICKS` unit until evidence supports a concrete
minutes unit. A default or invalidated state has `valid == false` and all of
those fields unavailable.

`CommandBatch` owns two `std::array` frames and is trivially-copyable, so
contract construction does not allocate per notification. Its count and frame
storage are private. A transport must use `sendable()`, which requires `READY`
and a non-empty bounded batch; `READY` with no frame is not sendable. Two
frames cover the current mode-then-value operation; bounded delivery and any
larger future operation remain outside this task.

The capability matrix is **app-derived** from
`doc/app-v1.20-protocol-extract.js`: MD1--MD4 expose status, power, mode,
manual level, set temperature, and clock; only encrypted MD2/MD4 expose the
extended-settings flag. The absence of extended settings for MD1/MD3 means
unavailable through the supported app UI, not that heater firmware rejects an
otherwise unverified request.

The following commands completed successfully with ESPHome 2026.8.1, ESP-IDF
5.5.5, ESP32 GCC 14.2.0, and CMake 3.31.6:

```sh
cmake -S tests -B /tmp/diesel-heater-host-tests
cmake --build /tmp/diesel-heater-host-tests --parallel
ctest --test-dir /tmp/diesel-heater-host-tests --output-on-failure
.venv/bin/esphome config tests/esphome-local.yaml
.venv/bin/esphome compile tests/esphome-local.yaml
.venv/bin/esphome config tests/esphome-minimal.yaml
.venv/bin/esphome compile tests/esphome-minimal.yaml
git diff --check
```

The host build uses `-Wall -Wextra -Wpedantic`; both registered tests passed
(2/2), including default and invalidated validity for every state observation,
the power/mode mapping, fractional negative Celsius offsets, MD1--MD4
capabilities, and the fixed batch's nonready, READY-empty, full, and overflow
behavior. The ESPHome gates both validated and compiled. Because the additive
header is not included by live runtime code, their image sizes are unchanged
from T01: full local entities used 660,551 bytes with 1,174,336 bytes of
app-partition margin, and the minimal core used 651,527 bytes with 1,183,360
bytes of margin. No new warning was emitted by the host build; the existing
`climate.cpp:121` format warning remains baseline-only ESPHome evidence.

No heater hardware or raw frame capture was available. This task therefore
does not claim hardware verification, accepted response lengths, parser
correctness, protocol selection, command delivery, or entity behavior. T03
must adopt the state contract atomically while making the existing parser safe;
T04 must replace the legacy request/controller path and remove any transition
code if one becomes necessary.

## T03 safe, atomic legacy parsing (code complete, hardware validation pending)

T03 was implemented on 2026-08-26. `ResponseParser` keeps its legacy entry
point but now classifies results with `ParseResult`, checks both raw and
decrypted two-byte headers, decodes only received encrypted bytes, parses a
value-initialized `NormalizedHeaterState`, and commits it only on `OK`. The
runtime's `HeaterState` name is now a narrow alias for that final normalized
contract; all existing climate, sensor, number, switch, and legacy-controller
consumers use validity-guarded normalized fields rather than uninitialized raw
members. The former climate running-step interpretation is retained only as
the explicitly named `legacy_ui_heating` compatibility field; it is not an
app-derived or universal protocol meaning. Raw state/mode/step sensors remain
diagnostic-only.

Safe-read spans are **app-derived** from the documented highest field index:
MD1 17, MD3 18, MD2 35, and MD4 36 bytes. They protect every indexed read but
are not acceptance lengths. Separately, the README/app documentation supplies
a conservative 20-byte plain / 48-byte encrypted completeness policy; the
parser returns `INCOMPLETE` below that policy. No capture establishes an exact
wire-frame length, a notification boundary, checksum, or terminator. Neither
the safe spans nor the 20/48 policy may be promoted to a hardware
compatibility claim.

`response_parser_safety_test` uses in-source **synthetic** vectors only. It
tests every prefix from zero through one byte below the documented 20/48
completeness policy; full-length malformed raw-header and decrypted-header
mismatch checks; encrypted short-input handling; no mutation after every
failure; MD1--MD4 error offsets; signed 16-bit case/cabin temperatures; signed
8-bit offset; voltage/altitude scaling; Fahrenheit Celsius normalization;
decrypted RF433 state; range invalidation; and plain non-thermostat target
temperature invalidation. The vectors prove memory and state-transition safety,
not real-wire lengths or interoperability.

Fresh standard host CTest (3/3) and a clean ASan/UBSan CTest run (3/3) passed
with CMake 3.31.6 and GNU C++ 14.2.0. Both local ESPHome configurations also
validated and compiled with ESPHome 2026.8.1 / ESP-IDF 5.5.5 / ESP32 GCC
14.2.0, with no compiler warnings emitted by either build:

| Configuration | Image payload | Padded binary | Remaining app-partition margin |
| --- | ---: | ---: | ---: |
| Full local entities | 662,291 bytes | 662,400 bytes | 1,172,717 bytes |
| Minimal core | 653,163 bytes | 653,280 bytes | 1,181,845 bytes |

The app partition is 1,835,008 bytes. Host sanitizer support is enabled by the
`ENABLE_SANITIZERS` CMake option when the compiler is GNU or Clang.

The legacy request/controller architecture remains intentionally in place for
T04. T03 preserves its existing MD2/MD3 controller-activation boundary: a
successfully parsed MD1/MD4 response does not activate a controller or commit a
new runtime-published state. Until T04 performs the dedicated
Celsius-to-Fahrenheit command encoding work, the legacy controller rejects
Fahrenheit temperature writes rather than send a normalized Celsius value as
an on-wire Fahrenheit value. This is a deliberately scoped T03 safety
regression, not a new command policy. No hardware, raw BLE capture, or device
state-publication test was available.

## T04 AA-family driver cutover (code complete, hardware validation pending)

T04 replaces the live request/controller/parser route with `ProtocolDriver`,
`AAProtocolDriver`, and one static MD1--MD4 registry. A response activates its
driver only after that driver's complete parse succeeds. The runtime retains a
single app-derived status probe before activation; entity control requests
return a safe unavailable result until a driver and valid state exist. There
is no heap allocation for driver selection, no legacy compatibility adapter,
and no remaining `messages.h`, `controllers.h`, `ResponseParser`, request
subclass, or controller-selector include in component or host-test C++.

The shared encoder accepts the default decimal passkey 1234, writes its two
base-100 fields, writes every value as a signed 16-bit little-endian bit
pattern, and computes the byte-wrapping checksum from bytes 2--6. Its command
identifiers are decimal: 10 clock; 11 automatic start time; 12 automatic run
time; 13 automatic enabled; 14 language; 15 temperature unit; 16 tank volume;
17 oil-pump type/RF433; 18 automatic heating; 19 altitude unit; and 20
temperature offset. Command construction converts normalized Celsius to
Fahrenheit only when the selected response state reports Fahrenheit; command
queueing, configurable passkeys, and configuration policy remain T05--T07
work.

The host suite remains entirely **synthetic**. It reruns every T03 parser
safety test through the registry and adds byte-exact encoder checks for the
default status frame, passkeys 1/9999, values 255/256/1439, signed negative
offsets, checksum wraparound, and command IDs 1--4 and 10--20. It also tests
unselected-state rejection plus Celsius/Fahrenheit command conversion and
mode-then-temperature batching. These tests verify code behavior, not a
captured wire protocol or a heater command acknowledgement.

The following commands passed with ESPHome 2026.8.1, ESP-IDF 5.5.5, ESP32 GCC
14.2.0, CMake 3.31.6, and GNU C++ 14.2.0:

```sh
cmake -S tests -B /tmp/diesel-heater-host-tests-t04
cmake --build /tmp/diesel-heater-host-tests-t04 --parallel
ctest --test-dir /tmp/diesel-heater-host-tests-t04 --output-on-failure
cmake -S tests -B /tmp/diesel-heater-host-asan-t04 -DENABLE_SANITIZERS=ON
cmake --build /tmp/diesel-heater-host-asan-t04 --parallel
ctest --test-dir /tmp/diesel-heater-host-asan-t04 --output-on-failure
.venv/bin/esphome config tests/esphome-local.yaml
.venv/bin/esphome compile tests/esphome-local.yaml
.venv/bin/esphome config tests/esphome-minimal.yaml
.venv/bin/esphome compile tests/esphome-minimal.yaml
git diff --check
```

Both CTest runs passed 3/3. Both ESPHome configurations validated and compiled
without compiler warnings. The 1,835,008-byte app partition measurements are:

| Configuration | Image payload | Padded OTA binary | Remaining app-partition margin |
| --- | ---: | ---: | ---: |
| Full local entities | 662,359 bytes | 662,480 bytes | 1,172,649 bytes |
| Minimal core | 653,163 bytes | 653,280 bytes | 1,181,845 bytes |

No hardware, real BLE captures, write acknowledgements, or state-publication
checks were available. MD1--MD4 support remains app-derived; hardware command
and response validation is pending.

## T05 explicit configuration and safe selection (code complete, hardware validation pending)

`diesel_heater_ble` now validates `protocol` as `auto`, `aa55_plain`,
`aa55_encrypted`, `aa66_plain`, or `aa66_encrypted`, and validates decimal
`passkey` from 1 through 9999. The configured passkey is used for the shared
status discovery probe (command 1, value 0) and every post-selection command.
Auto mode activates only after one complete successful parse. Explicit mode
uses only its configured parser, and a selected format cannot change until the
BLE connection ends. Discovery sends at most five probes per connection and
emits at most one initial parse-failure log plus one terminal log. The terminal
log distinguishes no response (including a possible wrong passkey) from one or
more received but invalid, malformed, or configured-format-mismatched responses.

`protocol_selection_test` is synthetic and covers all auto/explicit formats,
mismatches, malformed/incomplete input, selection stability, configured probe
bytes, both terminal discovery classifications, and their per-connection reset.
It does not establish hardware passkey behavior or protocol interoperability.

ESPHome 2026.8.1 accepted both auto fixtures and all four explicit
configuration fixtures; the invalid protocol and passkeys 0/10000 fixtures
failed schema validation as expected. The normal and ASan host suites passed
4/4. A five-build firmware matrix (local auto plus the four explicit formats)
completed with ESP-IDF 5.5.5 and ESP32 GCC 14.2.0. Its padded OTA outputs were
662,880 bytes (auto), 653,680 bytes (AA55 plain), 653,664 bytes (AA55
encrypted), 653,664 bytes (AA66 plain), and 653,648 bytes (AA66 encrypted).
Generated `main.cpp` was inspected for each build and contains the expected
distinct `set_protocol(ProtocolVariant::...)` and `set_passkey(...)` calls.
After adding terminal-outcome classification, both normal and ASan host suites
again passed 4/4, and the full plus minimal ESPHome configurations compiled.
Hardware discovery for every variant remains unavailable.

## T07 serialized command delivery (code complete, hardware validation pending)

`CommandDelivery` is a fixed four-operation FIFO with no dynamic allocation.
It permits exactly one submitted GATT write, waits for
`ESP_GATTC_WRITE_CHAR_EVT` before accepting an acknowledgement, and advances a
two-frame mode/value batch only after its current frame receives either a
matching parsed response-command byte or a matching fresh parsed state. Fresh
state is accepted only while that frame is awaiting acknowledgement and only
when its notification timestamp is at or after GATT write completion.

The dispatcher uses a 1,500 ms acknowledgement timeout and two retries; the
third failed attempt deterministically records `ACK_TIMEOUT` or
`WRITE_FAILED` and drops the operation. A submitted GATT write also has a
1,000 ms completion deadline. Because a missing write-completion event leaves
the controller outcome unknown, `WRITE_COMPLETION_TIMEOUT` fails closed:
queued work is cleared, all new work is rejected, and no retry/write occurs
until disconnect resets the connection generation. An explicit GATT write
failure is known not to be pending and may use the normal bounded retry path.
Queue overflow, invalid batches, blocked transport, and suppressed status
polls return named delivery results and are rate-limited in the runtime log.
Only one poll can exist; later polls are suppressed while it is pending or
awaiting acknowledgement, and a control then sits directly behind that poll.
An unsent idle poll is removed when a control arrives. Disconnect clears every
queued and in-flight operation for that connection generation.

`command_delivery_test` is entirely **synthetic** and shares the production
dispatcher seam through a deterministic fake clock and callback transport. It
covers one-write enforcement; pre-completion, nonmatching, stale, and matching
acknowledgements; state acknowledgement identity/timestamp checks; retries,
final timeout, immediate and completion write failures; a missing completion
event that blocks transport without retry until disconnect; FIFO overflow;
dependent mode/value ordering; repeated polling while a poll is pending or
awaiting acknowledgement; `uint32_t` millis wraparound; and disconnect
clearing. It proves implementation behavior only; it is not evidence of real
controller acknowledgement timing or protocol interoperability.

The following completed successfully on 2026-08-26 with CMake 3.31.6, GNU C++
14.2.0, ESPHome 2026.8.1, ESP-IDF 5.5.5, and ESP32 GCC 14.2.0:

```sh
cmake -S tests -B /tmp/diesel-heater-host-tests-t07
cmake --build /tmp/diesel-heater-host-tests-t07 --parallel
ctest --test-dir /tmp/diesel-heater-host-tests-t07 --output-on-failure
cmake -S tests -B /tmp/diesel-heater-host-asan-t07 -DENABLE_SANITIZERS=ON
cmake --build /tmp/diesel-heater-host-asan-t07 --parallel
ctest --test-dir /tmp/diesel-heater-host-asan-t07 --output-on-failure
.venv/bin/esphome config tests/esphome-local.yaml
.venv/bin/esphome compile tests/esphome-local.yaml
.venv/bin/esphome config tests/esphome-minimal.yaml
.venv/bin/esphome compile tests/esphome-minimal.yaml
git diff --check
```

Both normal and ASan/UBSan host suites passed 6/6. The full local build
reported 665,639 bytes of image payload, 665,760 bytes padded, and 1,169,248
bytes of application-partition headroom. The minimal build reported 655,791
bytes of image payload, 655,904 bytes padded, and 1,179,104 bytes of headroom.
No compatible heater or BLE adapter was available: multi-frame level/
temperature commands, reconnect during a command, and timeout recovery remain
hardware-validation pending.

## T08 capability-aware entity and climate audit (code complete, hardware validation pending)

`entity_control.h` is the dependency-free runtime policy shared by entity
entry points and host tests. It defines normalized manual-level (1--10) and
temperature (8--36 °C) ranges, required capabilities for power, manual-level,
and temperature operations, and the complete capability sets required by
internal thermostat, external staged, and manual climate strategies. Entity
calls now return explicit outcomes for lifecycle rejection, unavailable
capabilities or state, invalid values, unsupported driver batches, and
delivery rejection. A queued request is not a state update: switch, number,
and climate state remain owned by the next confirmed parsed response.

Climate requests now use `ClimateControlRuntime`, a dependency-free runtime
seam around `ClimateControlSession`. It normalizes external samples, snapshots
automation intent and external-control timers, commits a candidate only after
every command has been accepted by the delivery FIFO, and receives tagged
completion/failure events from `CommandDelivery`. If a later synchronous
enqueue is rejected, matching queued work is removed before rollback. A later
write failure, acknowledgement timeout, transport timeout, or disconnect
discards all remaining same-transaction work and restores that snapshot. This
applies to mode, fan-auto, manual-level, target, external start/stop/staging,
and the external-to-internal fallback. Climate mode, target, and action remain
projections of a confirmed parsed heater state; automation intent is never
published optimistically.

Non-finite external sensor samples and disconnected or unconfirmed sessions
therefore produce no control write. The test-only static guard now also scans
`entity_control.h` and `climate_session.h`, case-insensitively rejecting direct
entity includes of protocol implementation headers and AA headers/checksums,
raw identifiers, packet/frame, or wire details.

The following completed successfully on 2026-08-26 with CMake 3.31.6, GNU C++
14.2.0, ESPHome 2026.8.1, ESP-IDF 5.5.5, and ESP32 GCC 14.2.0:

```sh
cmake -S tests -B /tmp/diesel-heater-host-tests-t08
cmake --build /tmp/diesel-heater-host-tests-t08 --parallel
ctest --test-dir /tmp/diesel-heater-host-tests-t08 --output-on-failure
cmake -S tests -B /tmp/diesel-heater-host-asan-t08 -DENABLE_SANITIZERS=ON
cmake --build /tmp/diesel-heater-host-asan-t08 --parallel
ctest --test-dir /tmp/diesel-heater-host-asan-t08 --output-on-failure
.venv/bin/esphome config tests/esphome-local.yaml
.venv/bin/esphome compile tests/esphome-local.yaml
.venv/bin/esphome config tests/esphome-minimal.yaml
.venv/bin/esphome compile tests/esphome-minimal.yaml
git diff --check
```

Both host suites passed 9/9, including the static guard, runtime-seam coverage
for external input/admission/confirmed projection, and tagged FIFO coverage
for partial enqueue cancellation, terminal write/ack/transport/disconnect
failure, timeout rollback, and recovery; ASan/UBSan reported no finding. The
full local build reported 667,763 bytes of image payload and 1,167,136 bytes
of app-partition headroom. The minimal build reported 656,319 bytes and
1,178,576 bytes of headroom. All entity/control, external-automation,
and confirmed-publication evidence is synthetic or compile-derived; no heater
or BLE adapter was available, so hardware publication/control/fallback checks
remain unavailable.

## T09 optional-platform decoupling and size evidence (code complete, hardware validation pending)

The core component no longer unconditionally `AUTO_LOAD`s sensor, button,
number, switch, or climate. Each optional platform module declares only its
own ESPHome platform dependency. The core header now forward-declares optional
entity types, and the implementation/header code that names those types is
guarded by `defined(USE_ESP32) && defined(USE_*)`. Optional platform headers
are similarly self-guarded because ESPHome's generated umbrella header
discovers component headers even when the matching platform is not installed.

`tests/esphome-minimal.yaml` remains the checked-in formal BLE-only fixture:
it contains ESP32 BLE tracking, a BLE client, and `diesel_heater_ble`, but no
climate, sensor, number, switch, button, or time configuration. Its generated
`src/esphome/core/defines.h` has no `USE_CLIMATE`, `USE_SENSOR`, `USE_NUMBER`,
`USE_SWITCH`, `USE_BUTTON`, or `USE_TIME` define. The minimal ELF also has no
`DieselHeaterClimate`, `PowerSwitch`, `PowerLevelNumber`, `SetTempNumber`, or
heater button symbols, while the full local ELF contains each of those classes.
This is compiler/linker evidence that the fixture neither requires nor links
the component's optional entity-platform code.

The following completed successfully on 2026-08-26 with CMake 3.31.6, GNU C++
14.2.0, ESPHome 2026.8.1, ESP-IDF 5.5.5, and ESP32 GCC 14.2.0:

```sh
cmake -S tests -B /tmp/diesel-heater-host-tests
cmake --build /tmp/diesel-heater-host-tests --parallel
ctest --test-dir /tmp/diesel-heater-host-tests --output-on-failure
cmake -S tests -B /tmp/diesel-heater-host-asan -DENABLE_SANITIZERS=ON
cmake --build /tmp/diesel-heater-host-asan --parallel
ctest --test-dir /tmp/diesel-heater-host-asan --output-on-failure
.venv/bin/esphome config tests/esphome-minimal.yaml
.venv/bin/esphome compile tests/esphome-minimal.yaml
.venv/bin/esphome config tests/esphome-local.yaml
.venv/bin/esphome compile tests/esphome-local.yaml
```

Both host suites passed 10/10; ASan/UBSan reported no finding. The added
`optional_platform_non_esp_guard_test` defines every optional platform macro
without `USE_ESP32` and directly includes the real optional headers plus
`heater.cpp` and `climate.cpp`; it compiles without ESPHome headers, proving
that those macros cannot accidentally pull platform code into a non-ESP32
translation unit. The exact build maps queried were
`/tmp/diesel-heater-esphome-minimal/build/diesel-heater-minimal-test.map` and
`/tmp/diesel-heater-esphome-local/build/diesel-heater-local-test.map`.
`xtensa-esp32-elf-nm -C` queried both corresponding ELFs for
`DieselHeaterClimate|PowerSwitch|PowerLevelNumber|SetTempNumber|Level(Up|Down)Button|Temp(Up|Down)Button`.

| Configuration | T01 payload | T09 payload | Payload delta | T01 padded OTA | T09 padded OTA | Padded delta | T09 remaining app margin |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Full local entities | 660,551 | 668,135 | +7,584 | 660,672 | 668,256 | +7,584 | 1,166,752 |
| BLE-only minimal | 651,527 | 652,299 | +772 | 651,648 | 652,416 | +768 | 1,182,592 |

The T01 comparison spans the cumulative T02--T09 refactor, so these are exact
working-tree deltas rather than an attribution of every byte to optional
platform cleanup. The app partition remains 1,835,008 bytes. The full and
minimal padded OTA outputs above are respectively
`/tmp/diesel-heater-esphome-local/build/firmware.ota.bin` and
`/tmp/diesel-heater-esphome-minimal/build/firmware.ota.bin`.

Explicit-protocol compile-time exclusion is **unproven**. The queried minimal
auto map contains `protocol.cpp.obj`, `protocol_aa.cpp.obj`, and the 32-byte
static `kDrivers` storage at map line 56793; its ELF exports
`ProtocolRegistry::find(ProtocolVariant)`,
`ProtocolSelection::parse(...)`, `AAProtocolDriver::detect_variant(...)`,
and `AAProtocolDriver::parse(...)`. Those queried symbols characterize the
auto image, but are not evidence for a particular explicit configuration. The
current source uses a static four-entry `AAProtocolDriver` registry and runtime
`ProtocolVariant` selection; no explicit-protocol build was used to claim that
any driver code is excluded.
No hardware or BLE adapter was available, so hardware validation remains
pending.

## T10 optional heater-clock synchronization (code complete, hardware validation pending)

The dependency-free `clock_sync_test` covers invalid time, all 16-bit
minutes-since-midnight boundaries, acknowledgement-only success, reconnect,
and serialization with the normal status-command path. It also verifies that a
failed periodic attempt at its one-hour due time cannot enqueue again before
the 60-second retry interval, retries exactly at that interval, and returns to
the one-hour schedule after the retry succeeds. This preserves unsigned
wrap-safe elapsed-time arithmetic and prevents periodic delivery failures from
enqueuing once per loop.

On 2026-08-27, ESPHome 2026.8.1 / ESP-IDF 5.5.5 validation completed with:

```sh
cmake -S tests -B /tmp/diesel-heater-host-tests
cmake --build /tmp/diesel-heater-host-tests --parallel
ctest --test-dir /tmp/diesel-heater-host-tests --output-on-failure
cmake -S tests -B /tmp/diesel-heater-host-asan -DENABLE_SANITIZERS=ON
cmake --build /tmp/diesel-heater-host-asan --parallel
ASAN_OPTIONS=detect_leaks=1 ctest --test-dir /tmp/diesel-heater-host-asan --output-on-failure
.venv/bin/esphome config tests/esphome-local.yaml
.venv/bin/esphome config tests/esphome-minimal.yaml
.venv/bin/esphome config tests/esphome-local-time.yaml
.venv/bin/esphome config tests/esphome-minimal-time.yaml
.venv/bin/esphome compile tests/esphome-local.yaml
.venv/bin/esphome compile tests/esphome-minimal.yaml
.venv/bin/esphome compile tests/esphome-local-time.yaml
.venv/bin/esphome compile tests/esphome-minimal-time.yaml
```

Both host suites passed 11/11, with no ASan/UBSan findings. The full and
BLE-only fixtures compile both without `time_id` and with the generic SNTP
`RealTimeClock` fixture. The checked firmware output sizes are 668,320 bytes
(full without time), 652,544 bytes (minimal without time), 1,103,248 bytes
(full with time), and 1,087,344 bytes (minimal with time); the time-enabled
images retain 731,760 and 747,664 bytes, respectively, in the 1,835,008-byte
application partition. Schema-negative fixtures for an unknown `time_id` and
a non-clock `time_id` both correctly fail validation.

No compatible heater or BLE adapter was available, so on-device clock setting,
acknowledgement behavior, and reconnect behavior remain hardware-validation
pending.

## T11 final qualification (matrix pass reported by user; hardware validation pending)

The final source audit found no live legacy parser/controller symbols
(`ResponseParser`, `Request`, `HeaterController`, `ControllerSelector`,
`messages.h`, or `controllers.h`), no dynamic protocol-driver/controller
allocation, and no AA header/checksum/raw-mode knowledge in entity sources.
The only AA constants are in the protocol implementation and its protocol
tests. The remaining `HeaterState` alias and legacy UI heating label are
intentional compatibility/diagnostic names, not live migration adapters.

On 2026-08-27 the user ran the complete checked-in matrix successfully after
freeing disk space under `/tmp`:

- full and BLE-only minimal fixtures;
- `auto`, `aa55_plain`, `aa55_encrypted`, `aa66_plain`, and
  `aa66_encrypted` selection;
- each selection both without `time_id` and with the generic SNTP
  `RealTimeClock` fixture.

The 16 explicit-selection fixtures are under `tests/esphome-matrix-*.yaml`;
the four auto-selection cells are `tests/esphome-{local,minimal}{,-time}.yaml`.
This records a user-verified compile pass, not fabricated compiler output:
per-cell warning/size logs from that terminal session were not captured. The
reproducible T09 and T10 size measurements above remain the byte-level
evidence, and explicit-protocol compile-time driver exclusion remains
unproven. Hardware reconnect, command ordering, timeout recovery, unavailable
state, climate fallback, and clock checks remain unavailable because no
compatible heater or BLE adapter was supplied.

## T12 temperature-offset write (app-derived; hardware validation pending)

This T12 execution adds exactly one capability: the optional
`number.temperature_offset` control. It is admitted only after a valid
MD2/MD4 response grants `EXTENDED_SETTINGS`; MD1/MD3 remain unsupported.
The number has no optimistic publication path: it queues a tagged serialized
delivery operation, and its state is published only from a later parsed heater
response. A write failure, timeout, disconnect, unsupported variant, stale
state, or invalid value leaves the confirmed state unchanged.

The source is **app-derived** from the recorded AirHeaterBLE 1.20 APK and its
raw `app-service.js` member (hashes above). The app's response parsing and
capability evidence justify this component's encrypted MD2/MD4-only gate. Its
parameter template separately renders `tempoffset` when `tempoffset != 255`,
with an integer slider `min:-9,max:9`; plain-response parsing does not
establish encrypted-only template visibility. `tempoffset_change` sends that
integer as decimal command 20. The shared app encoder writes command data as
signed 16-bit little-endian, so for example passkey 1234 / offset -9 is
`AA 55 0C 22 14 F7 FF 38`.

The normalized component state is Celsius: its existing encrypted parser
reads raw signed offsets directly for reported Celsius and converts reported
Fahrenheit offsets by 5/9. The new static ESPHome number is deliberately the
unit-safe intersection `-5..5 °C`, integer step 1: direct Celsius values stay
within the observed `-9..9` raw range, and Fahrenheit conversion/rounding also
cannot exceed that range. This conservative static range is necessary because
ESPHome number traits cannot safely change with the heater's reported unit;
it does not claim that `-5..5 °C` is the app's full Celsius-mode UI range.

The request tests are **synthetic** and assert command 20's exact signed
little-endian bytes for Celsius and Fahrenheit normalization, plus invalid
range/fraction rejection and MD1 unsupported rejection. The entity-policy
test asserts `EXTENDED_SETTINGS` admission and the same conservative range.
The full entity fixture includes the optional number; the minimal fixture does
not load the number platform. No BLE capture, acknowledgement, or compatible
heater was available for this capability, so it is neither capture-verified
nor hardware-verified.
