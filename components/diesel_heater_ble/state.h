#pragma once

#include "contracts.h"

namespace esphome {
namespace diesel_heater_ble {

// T03 completes the state cutover. Retaining this name avoids a gratuitous
// public API rename while ensuring every runtime consumer sees only the
// initialized, normalized contract. No legacy raw-field alias is provided.
using HeaterState = NormalizedHeaterState;

}  // namespace diesel_heater_ble
}  // namespace esphome
