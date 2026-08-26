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
