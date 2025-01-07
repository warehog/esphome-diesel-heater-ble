#pragma once

#include "esphome/core/log.h"
#include <cinttypes>
#include "state.h"
#include "messages.h"

namespace esphome {
namespace diesel_heater_ble {

class HeaterController {
 public:
  virtual std::vector<Request> gen_power_command(HeaterState state, bool power) = 0;
  virtual std::vector<Request> gen_level_command(HeaterState state, uint8_t value) = 0;
  virtual std::vector<Request> gen_level_up_command(HeaterState state) = 0;
  virtual std::vector<Request> gen_level_down_command(HeaterState state) = 0;
  virtual std::vector<Request> gen_temp_command(HeaterState state, uint8_t value) = 0;
  virtual std::vector<Request> gen_temp_up_command(HeaterState state) = 0;
  virtual std::vector<Request> gen_temp_down_command(HeaterState state) = 0;
  virtual std::vector<Request> get_manual_mode_command(HeaterState state) = 0;
  virtual std::vector<Request> get_auto_mode_command(HeaterState state) = 0;
};

class HeaterControllerAA55E : public HeaterController {
 public:
  std::vector<Request> change_mode_command(HeaterState state, uint8_t target_mode) {
    std::vector<Request> requests;
    if (state.runningmode != target_mode) {
      requests.emplace_back(Request(0x02, target_mode, 0x00));
    }
    return requests;
  }

 public:
  std::vector<Request> gen_power_command(HeaterState state, bool power) override {
    std::vector<Request> requests;
    if (state.runningstate != power) {
      requests.push_back(Request(0x03, power ? 0x01 : 0x00, 0x00));
    }
    return requests;
  }

  std::vector<Request> gen_level_command(HeaterState state, uint8_t value) override {
    auto requests = get_manual_mode_command(state);
    if (state.setlevel != value) {
      requests.push_back(Request(0x04, value, 0x00));
    }
    return requests;
  }

  std::vector<Request> gen_level_up_command(HeaterState state) override {
    auto requests = get_manual_mode_command(state);
    if (state.setlevel <= 9) {
      requests.push_back(Request(0x04, state.setlevel + 1, 0x00));
    }
    return requests;
  }

  std::vector<Request> gen_level_down_command(HeaterState state) override {
    auto requests = get_manual_mode_command(state);
    if (state.setlevel > 1) {
      requests.push_back(Request(0x04, state.setlevel - 1, 0x00));
    }
    return requests;
  }

  std::vector<Request> gen_temp_command(HeaterState state, uint8_t value) override {
    auto requests = get_auto_mode_command(state);
    if (state.settemp != value) {
      requests.push_back(Request(0x04, value, 0x00));
    }
    return requests;
  }

  std::vector<Request> gen_temp_up_command(HeaterState state) override {
    auto requests = get_auto_mode_command(state);
    if (state.tempunit == 0x00 && state.settemp < 36) {
      requests.push_back(Request(0x04, state.settemp + 1, 0x00));
    } else if (state.tempunit == 0x01 && state.settemp < 97) {
      requests.push_back(Request(0x04, state.settemp + 1, 0x00));
    }
    return requests;
  }

  std::vector<Request> gen_temp_down_command(HeaterState state) override {
    auto requests = get_auto_mode_command(state);
    if (state.tempunit == 0x00 && state.settemp > 8) {
      requests.push_back(Request(0x04, state.settemp - 1, 0x00));
    } else if (state.tempunit == 0x01 && state.settemp > 46) {
      requests.push_back(Request(0x04, state.settemp - 1, 0x00));
    }
    return requests;
  }

  std::vector<Request> get_manual_mode_command(HeaterState state) override { return change_mode_command(state, 0x01); }

  std::vector<Request> get_auto_mode_command(HeaterState state) override { return change_mode_command(state, 0x02); }
};

class ControllerSelector {
 public:
  static HeaterController *get_controller(HeaterClass heater_class) {
    switch (heater_class) {
      case HeaterClass::HEATER_AA_55_ENCRYPTED:
        return new HeaterControllerAA55E();
      default:
        return nullptr;
    }
  }
};

}  // namespace diesel_heater_ble
}  // namespace esphome
