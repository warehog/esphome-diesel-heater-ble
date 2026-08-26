#pragma once

#if defined(USE_ESP32) && defined(USE_SWITCH)

#include "esphome/components/switch/switch.h"
#include "esphome/core/log.h"
#include "heater.h"

namespace esphome {
namespace diesel_heater_ble {

class PowerSwitch : public switch_::Switch, public Parented<DieselHeaterBLE> {
 public:
  PowerSwitch() = default;

 protected:
  void write_state(bool state) override { this->parent_->on_power_switch(state); }
};

}  // namespace diesel_heater_ble
}  // namespace esphome

#endif  // USE_ESP32 && USE_SWITCH
