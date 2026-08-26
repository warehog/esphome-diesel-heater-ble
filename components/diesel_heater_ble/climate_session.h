#pragma once

namespace esphome {
namespace diesel_heater_ble {

// Dependency-free mirror of the fan presentation owned by the climate base
// class. It makes the disconnect/fresh-state reset policy host-testable.
class ClimateFanSession {
 public:
  void set_auto() {
    this->fan_mode_set_ = true;
    this->custom_fan_mode_set_ = false;
  }
  void set_manual() {
    this->fan_mode_set_ = false;
    this->custom_fan_mode_set_ = true;
  }
  void clear() {
    this->fan_mode_set_ = false;
    this->custom_fan_mode_set_ = false;
  }
  bool fan_mode_set() const { return this->fan_mode_set_; }
  bool custom_fan_mode_set() const { return this->custom_fan_mode_set_; }

 private:
  bool fan_mode_set_{false};
  bool custom_fan_mode_set_{false};
};

}  // namespace diesel_heater_ble
}  // namespace esphome
