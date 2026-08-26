#pragma once

#include "esphome/core/log.h"
#include <cinttypes>
#include "state.h"
#include "messages.h"

namespace esphome {
namespace diesel_heater_ble {

class HeaterController {
 public:
  virtual std::vector<Request> gen_power_command(const HeaterState &state, bool power) = 0;
  virtual std::vector<Request> gen_level_command(const HeaterState &state, uint8_t value) = 0;
  virtual std::vector<Request> gen_level_up_command(const HeaterState &state) = 0;
  virtual std::vector<Request> gen_level_down_command(const HeaterState &state) = 0;
  virtual std::vector<Request> gen_temp_command(const HeaterState &state, uint8_t value) = 0;
  virtual std::vector<Request> gen_temp_up_command(const HeaterState &state) = 0;
  virtual std::vector<Request> gen_temp_down_command(const HeaterState &state) = 0;
  virtual std::vector<Request> get_manual_mode_command(const HeaterState &state) = 0;
  virtual std::vector<Request> get_auto_mode_command(const HeaterState &state) = 0;
};

class HeaterControllerAA : public HeaterController {
 // It's assumed that AA55 and AA66 have the same command structure, whether encrypted or not.
 // However specific heater controller classes are below for implementing differences as needed.

 public:
  std::vector<Request> change_mode_command(const HeaterState &state, OperatingMode target_mode) {
    std::vector<Request> requests;
    if (!state.valid) {
      return requests;
    }
    if (!state.operating_mode.valid || state.operating_mode.value != target_mode) {
      const uint8_t raw_target_mode = target_mode == OperatingMode::MANUAL_LEVEL ? 1 : 2;
      requests.emplace_back(Request(0x02, raw_target_mode, 0x00));
    }
    return requests;
  }

 public:
  std::vector<Request> gen_power_command(const HeaterState &state, bool power) override {
    std::vector<Request> requests;
    if (state.valid && state.power.valid && state.power.value != power) {
      requests.push_back(Request(0x03, power ? 0x01 : 0x00, 0x00));
    }
    return requests;
  }

  std::vector<Request> gen_level_command(const HeaterState &state, uint8_t value) override {
    auto requests = get_manual_mode_command(state);
    if (state.valid && state.manual_level.valid && state.manual_level.value != value) {
      requests.push_back(Request(0x04, value, 0x00));
    }
    return requests;
  }

  std::vector<Request> gen_level_up_command(const HeaterState &state) override {
    auto requests = get_manual_mode_command(state);
    if (state.valid && state.manual_level.valid && state.manual_level.value <= 9) {
      requests.push_back(Request(0x04, state.manual_level.value + 1, 0x00));
    }
    return requests;
  }

  std::vector<Request> gen_level_down_command(const HeaterState &state) override {
    auto requests = get_manual_mode_command(state);
    if (state.valid && state.manual_level.valid && state.manual_level.value > 1) {
      requests.push_back(Request(0x04, state.manual_level.value - 1, 0x00));
    }
    return requests;
  }

  std::vector<Request> gen_temp_command(const HeaterState &state, uint8_t value) override {
    // T04 owns conversion of normalized Celsius input to an on-wire
    // Fahrenheit value. Do not send an incorrectly interpreted value through
    // this legacy controller in the interim.
    if (!state.valid || !state.target_temperature_c.valid ||
        (state.reported_temperature_unit.valid && state.reported_temperature_unit.value == TemperatureUnit::FAHRENHEIT)) {
      return {};
    }
    auto requests = get_auto_mode_command(state);
    if (state.target_temperature_c.value != value) {
      requests.push_back(Request(0x04, value, 0x00));
    }
    return requests;
  }

  std::vector<Request> gen_temp_up_command(const HeaterState &state) override {
    if (!state.valid || !state.target_temperature_c.valid ||
        (state.reported_temperature_unit.valid && state.reported_temperature_unit.value == TemperatureUnit::FAHRENHEIT)) {
      return {};
    }
    auto requests = get_auto_mode_command(state);
    if (state.target_temperature_c.value < 36.0f) {
      requests.push_back(Request(0x04, static_cast<uint8_t>(state.target_temperature_c.value + 1.0f), 0x00));
    }
    return requests;
  }

  std::vector<Request> gen_temp_down_command(const HeaterState &state) override {
    if (!state.valid || !state.target_temperature_c.valid ||
        (state.reported_temperature_unit.valid && state.reported_temperature_unit.value == TemperatureUnit::FAHRENHEIT)) {
      return {};
    }
    auto requests = get_auto_mode_command(state);
    if (state.target_temperature_c.value > 8.0f) {
      requests.push_back(Request(0x04, static_cast<uint8_t>(state.target_temperature_c.value - 1.0f), 0x00));
    }
    return requests;
  }

  std::vector<Request> get_manual_mode_command(const HeaterState &state) override {
    return change_mode_command(state, OperatingMode::MANUAL_LEVEL);
  }

  std::vector<Request> get_auto_mode_command(const HeaterState &state) override {
    return change_mode_command(state, OperatingMode::THERMOSTAT);
  }
};

class HeaterControllerAA55E : public HeaterControllerAA {
  // Specific implementation for AA55 Encrypted if needed in future.
};

class HeaterControllerAA66 : public HeaterControllerAA {
  // Specific implementation for AA66 if needed in future.
};

class ControllerSelector {
 public:
  static HeaterController *get_controller(ProtocolVariant variant) {
    // Preserve the legacy controller availability through T03. T04 owns the
    // all-variant driver registry and static ownership migration; T03 merely
    // calls this selector after a successful atomic parse.
    switch (variant) {
      case ProtocolVariant::MD2_AA55_ENCRYPTED:
        return new HeaterControllerAA55E();
      case ProtocolVariant::MD3_AA66_PLAIN:
        return new HeaterControllerAA66();
      default:
        return nullptr;
    }
  }
};

}  // namespace diesel_heater_ble
}  // namespace esphome
