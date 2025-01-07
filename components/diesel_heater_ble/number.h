#pragma once

#include "esphome/components/number/number.h"
#include "esphome/core/component.h"

#include "heater.h"

namespace esphome {
namespace diesel_heater_ble {

class PowerLevelNumber : public number::Number, public Parented<DieselHeaterBLE> {
 public:
  PowerLevelNumber() = default;

 protected:
  void control(float value) override { this->parent_->on_power_level_number(value); }
};

class SetTempNumber : public number::Number, public Parented<DieselHeaterBLE> {
 public:
  SetTempNumber() = default;

 protected:
  void control(float value) override { this->parent_->on_temp_number(value); }
};

}  // namespace diesel_heater_ble
}  // namespace esphome
