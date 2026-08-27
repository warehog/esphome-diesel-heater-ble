#pragma once

#include "contracts.h"

namespace esphome {
namespace diesel_heater_ble {

// Runtime consumers use the initialized, normalized state contract. No raw
// field alias is provided.
using HeaterState = NormalizedHeaterState;

}  // namespace diesel_heater_ble
}  // namespace esphome
