#pragma once

#include "esphome/components/button/button.h"
#include "esphome/core/log.h"
#include "heater.h"
#include "messages.h"

namespace esphome {
namespace diesel_heater_ble {

class LevelUpButton : public button::Button, public Parented<DieselHeaterBLE> {
 public:
  LevelUpButton() = default;

 protected:
  void press_action() override { this->parent_->on_level_up_button_press(); }
};

class LevelDownButton : public button::Button, public Parented<DieselHeaterBLE> {
 public:
  LevelDownButton() = default;

 protected:
  void press_action() override { this->parent_->on_level_down_button_press(); }
};

class TempUpButton : public button::Button, public Parented<DieselHeaterBLE> {
 public:
  TempUpButton() = default;

 protected:
  void press_action() override { this->parent_->on_temp_up_button_press(); }
};

class TempDownButton : public button::Button, public Parented<DieselHeaterBLE> {
 public:
  TempDownButton() = default;

 protected:
  void press_action() override { this->parent_->on_temp_down_button_press(); }
};

}  // namespace diesel_heater_ble
}  // namespace esphome
