#pragma once

#ifdef USE_ESP32

#include "esphome/components/climate/climate.h"
#include "esphome/core/component.h"

namespace esphome {
namespace diesel_heater_ble {

class DieselHeaterBLE;
struct HeaterState;

class DieselHeaterClimate : public climate::Climate, public Component {
 public:
  DieselHeaterClimate() = default;
  ~DieselHeaterClimate() = default;

  void setup() override;
  void loop() override;
  void sync_from_heater(const HeaterState &state);

  void set_heater(DieselHeaterBLE *heater) { this->heater_ = heater; }

  /// Called from a sensor's on_value lambda to supply an external temperature reading.
  /// When called, the component uses staged proportional control to drive manual heater levels
  /// instead of relying on the heater's built-in thermostat.
  void set_external_temperature(float temp);

  /// How long to wait without an external reading before falling back to the heater's
  /// built-in auto mode. Configured via external_temperature_timeout in YAML.
  void set_external_temperature_timeout(uint32_t timeout_ms) { this->ext_temp_timeout_ms_ = timeout_ms; }
  void set_external_temperature_timeout_heating(uint32_t timeout_ms) { this->ext_temp_timeout_heating_ms_ = timeout_ms; }

  /// When true, incoming external temperatures are converted from °F to °C before use.
  void set_external_temperature_fahrenheit(bool use_f) { this->ext_temp_fahrenheit_ = use_f; }
  void set_external_stage_gain(float gain) { this->ext_stage_gain_ = gain; }
  void set_external_off_error(float error_c) { this->ext_off_error_c_ = error_c; }
  void set_external_auto_on_error(float error_c) { this->ext_auto_on_error_c_ = error_c; }
  void set_external_min_on_time(uint32_t ms) { this->ext_min_on_time_ms_ = ms; }
  void set_external_min_off_time(uint32_t ms) { this->ext_min_off_time_ms_ = ms; }
  void set_external_overshoot_hold(uint32_t ms) { this->ext_overshoot_hold_ms_ = ms; }
  void set_external_level_update_interval(uint32_t ms) { this->ext_level_update_interval_ms_ = ms; }

 protected:
  void control(const climate::ClimateCall &call) override;
  climate::ClimateTraits traits() override;
  uint8_t stage_level_for_error_(float error_c) const;

  /// Run staged external-auto control; force_start_request is used for explicit
  /// user HEAT requests and bypass_timers is used for user-driven changes.
  void apply_external_control_(bool force_start_request, bool bypass_timers);
  bool refresh_using_external_(uint32_t now);
  bool is_heating_step_(uint8_t running_step) const { return running_step == 2 || running_step == 3; }

  DieselHeaterBLE *heater_;

  // External temperature control state
  float external_temperature_{NAN};
  uint32_t ext_temp_timeout_ms_{2 * 60 * 60 * 1000};  // default 2 hours; override via YAML
  uint32_t ext_temp_timeout_heating_ms_{15 * 60 * 1000};  // default 15 minutes; override via YAML
  uint32_t last_ext_temp_ms_{0};                 // 0 = never received
  uint32_t last_level_update_ms_{0};
  bool ext_temp_fahrenheit_{false};
  // Primary state axes:
  // using_external_: external sensor freshness/availability state.
  // auto_requested_: user intent for auto vs manual-level control.
  bool using_external_{false};
  bool auto_requested_{true};
  // Separate climate on/off intent so mode persistence is decoupled from auto/manual preference.
  bool climate_heat_requested_{false};

  uint8_t last_commanded_level_{1};
  uint32_t last_heat_transition_ms_{0};
  uint32_t overshoot_start_ms_{0};

  // Staged external-auto parameters.
  float ext_stage_gain_{2.0f};
  float ext_off_error_c_{-0.5f};
  float ext_auto_on_error_c_{1.0f};
  uint32_t ext_min_on_time_ms_{10 * 60 * 1000};
  uint32_t ext_min_off_time_ms_{20 * 60 * 1000};
  uint32_t ext_overshoot_hold_ms_{3 * 60 * 1000};
  uint32_t ext_level_update_interval_ms_{10 * 1000};
};

}  // namespace diesel_heater_ble
}  // namespace esphome

#endif  // USE_ESP32
