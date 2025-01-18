# esphome-diesel-heater-ble
ESPHome External component that allows controll of bluetooth enabled popular chinese diesel heaters.

# What does this implement/fix?

# Adding Support for Bluetooth Low Energy-Enabled Diesel Heaters (Webasto)

There have been many attempts to control these devices, but none of them seemed to fit my needs. These solutions were either standalone, like [iotmaestro/vevor-heater-ble](https://github.com/iotmaestro/vevor-heater-ble), or based on ESPHome without fully integrating with it, such as [spin877/Bruciatore_BLE](https://github.com/spin877/Bruciatore_BLE/blob/main/ESP32-VevorBLE.yaml).

So, I delved deep into reverse-engineering the entire protocol, and this code is the result of that effort.

## What has been done?
- Created the `diesel_heater_ble` component, which implements the BLE protocol to communicate with the heater controller board.
- Added sensors for all possible settings retrieved from the controller.
- Introduced switches, buttons, and number controls for the most significant features, including:
  - Power switch
  - Level-up button
  - Level-down button
  - Temperature-up button
  - Temperature-down button
  - Level set (number)
  - Temperature set (number)
- Enabled extensibility for this component to support future versions of controllers.

## Key Insights
One of the major discoveries was that there is no single protocol for communicating with all BLE-enabled controllers. Most existing documentation, repositories, and code focus on one of at least four protocol versions (apparently the oldest one).

## Known Protocols
Protocols are identified by the first two bytes of the response:
- **0xAA 0x55**: The most basic protocol with a 20-byte response frame.
- **0xAA 0x66**: Very similar to the previous one, with a slightly different order of bytes in the response.
- **0xAA 0x55 (encrypted)**: A newer version with a 48-byte encrypted data frame.
- **0xAA 0x66 (encrypted)**: Another newer version with a 48-byte encrypted data frame and altered byte order.

Each protocol uses slightly different formats for requests and responses. From the controller’s perspective, incoming data (requests from the mobile app) is "plain text," while responses are encrypted only in the most recent protocol versions.

### IMPORTANT: Current Implementation
This code implements parsing for all protocol versions’ responses but can only send commands using the **0xAA 0x55 (encrypted)** protocol.  
I own a heater that uses this third protocol version, so it is fully implemented in this PR. For other protocols, I would appreciate assistance from owners of devices using those versions.

## Framework Selection
The BLE stack takes up a significant amount of flash memory. Together with several sensors, buttons, etc., I exceeded the 4MB flash capacity of my ESP32 Lolin Lite.  
I discovered that using the ESP-IDF framework results in a smaller binary compared to the Arduino framework. Therefore, unless you have an 8MB board, ESP-IDF is the only viable solution.

## TODO
- Implement `HeaterController_*` classes to support other types of controllers.
- Add a climate component for unification.
- Refactor the code to allow configurations without buttons, switches, or numbers. Currently, if no button/switch/sensor/number is defined in the YAML, the C++ compiler throws an error about missing headers (e.g., when no button is configured, `esphome/components/button/button.h` cannot be found).
