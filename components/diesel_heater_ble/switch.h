#pragma once

#include "esphome/components/switch/switch.h"
#include "esphome/core/log.h"
#include "heater.h"
#include "messages.h"

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
