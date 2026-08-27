// Optional platform macros alone must not pull ESPHome platform headers into a
// non-ESP32 translation unit. Include the actual headers and implementation
// sources to make the preprocessor boundary part of the host test suite.
#define USE_BUTTON
#define USE_CLIMATE
#define USE_NUMBER
#define USE_SENSOR
#define USE_SWITCH

#include "../components/diesel_heater_ble/heater.h"
#include "../components/diesel_heater_ble/button.h"
#include "../components/diesel_heater_ble/climate.h"
#include "../components/diesel_heater_ble/number.h"
#include "../components/diesel_heater_ble/switch.h"

#include "../components/diesel_heater_ble/heater.cpp"
#include "../components/diesel_heater_ble/climate.cpp"

int main() { return 0; }
