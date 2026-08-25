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
