# Repository Guidelines

## Project Structure

This repository is an ESPHome external component for Bluetooth diesel heaters.

- `components/diesel_heater_ble/` contains the integration. Python modules define ESPHome schemas and code generation; matching `.h`/`.cpp` files implement the ESP32 runtime.
- `esphome.yaml` is a working example configuration, including the BLE client and exposed climate, sensor, number, switch, and button entities.
- `doc/protocols.md` documents supported heater protocols; `doc/` also contains explanatory images.
- There is currently no automated test directory. Do not commit generated `.esphome/`, `__pycache__/`, secrets, or virtual-environment files.

## Build, Test, and Development Commands

Run commands from the repository root with ESPHome installed (preferably in a local `.venv`):

```bash
esphome config esphome.yaml       # Validate YAML and component schemas
esphome compile esphome.yaml      # Generate and compile the ESP32 firmware
esphome run esphome.yaml          # Compile, flash, and show device logs
```

Use a local copy of the component while developing; update `external_components` only when testing the published GitHub source. Device-dependent behavior must be checked with a compatible heater and BLE adapter.

## Coding Style and Naming

Use four spaces in Python and two spaces in YAML. Follow ESPHome/Python conventions: `snake_case` for schema keys, functions, and variables; `PascalCase` for C++ classes; and `UPPER_SNAKE_CASE` for constants. Use two-space indentation in C++, braces on the same line, and keep protocol constants and packet parsing explicit. Preserve the existing separation between Python configuration glue and C++ runtime logic.

## Testing Guidelines

Before submitting changes, run `esphome config` and `esphome compile` against `esphome.yaml`. For protocol or state changes, test the affected heater class over BLE and verify entity state, reconnect behavior, timeouts, and safe fallback behavior. Document any hardware limitations or untested heater variants in the pull request.

## Commits and Pull Requests

Use short, imperative commit subjects (for example, `Fix heater class detection` or `Add climate component integration`). Keep unrelated changes separate. Pull requests should explain the behavior change, identify supported/untested hardware, include validation commands and results, and update `README.md` or `doc/protocols.md` when configuration or protocol behavior changes. Include logs or screenshots when they clarify Home Assistant or device behavior.

## Security and Configuration

Never commit real Wi-Fi credentials, BLE MAC addresses belonging to others, API keys, or generated secrets. Use placeholders in examples and keep private overrides in ignored files such as `secrets.yaml`.
