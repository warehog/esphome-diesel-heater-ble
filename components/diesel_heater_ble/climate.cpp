#include "climate.h"
#include "heater.h"

#include <algorithm>
#include <cmath>
#include <string>

#ifdef USE_ESP32

namespace esphome {
namespace diesel_heater_ble {

static const char *TAG = "diesel_heater_climate";
static const char *const MANUAL_LEVEL_MODES[] = {"Level 1", "Level 2", "Level 3", "Level 4", "Level 5",
                                                  "Level 6", "Level 7", "Level 8", "Level 9", "Level 10"};
static constexpr float CLIMATE_MIN_TARGET_C = 8.0f;
static constexpr float CLIMATE_MAX_TARGET_C = 36.0f;
static constexpr float CLIMATE_DEFAULT_TARGET_C = 20.0f;

static bool parse_manual_level(StringRef mode, uint8_t &level) {
  std::string mode_str(mode.c_str(), mode.size());
  if (mode_str.rfind("Level ", 0) == 0) {
    mode_str = mode_str.substr(6);
  }
  if (mode_str.empty()) {
    return false;
  }
  for (char c : mode_str) {
    if (c < '0' || c > '9') {
      return false;
    }
  }
  int value = std::stoi(mode_str);
  if (value < 1 || value > 10) {
    return false;
  }
  level = static_cast<uint8_t>(value);
  return true;
}

static const char *manual_level_mode_name(uint8_t level) {
  if (level < 1 || level > 10) {
    return MANUAL_LEVEL_MODES[0];
  }
  return MANUAL_LEVEL_MODES[level - 1];
}

uint8_t DieselHeaterClimate::stage_level_for_error_(float error_c) const {
  // Proportional starts with the auto-on threshold as an offset, reducing overshoots from higher levels when it turns on
  int stage = static_cast<int>(std::floor((error_c - this->ext_auto_on_error_c_) * this->ext_stage_gain_));
  return static_cast<uint8_t>(std::clamp(stage, 1, 10));
}

bool DieselHeaterClimate::refresh_using_external_(uint32_t now) {
  const bool was_using_external = this->using_external_;
  const bool has_recent_sample =
      this->last_ext_temp_ms_ != 0 &&
      static_cast<uint32_t>(now - this->last_ext_temp_ms_) <= this->ext_temp_timeout_ms_;
  this->using_external_ = has_recent_sample;
  return this->using_external_ != was_using_external;
}

void DieselHeaterClimate::sync_from_heater(const HeaterState &state) {
  const bool using_external = this->using_external_;

  if (using_external) {
    this->current_temperature = this->external_temperature_;
  } else {
    this->current_temperature = state.cabtemp;
  }

  // Allow heater-side target updates when not in external strategy, so remote
  // control changes are reflected while preserving high-resolution external control.
  if (!using_external && state.settemp >= CLIMATE_MIN_TARGET_C && state.settemp <= CLIMATE_MAX_TARGET_C) {
    this->target_temperature = state.settemp;
  }

  if (std::isnan(this->target_temperature) || this->target_temperature < CLIMATE_MIN_TARGET_C ||
      this->target_temperature > CLIMATE_MAX_TARGET_C) {
    this->target_temperature = CLIMATE_DEFAULT_TARGET_C;
  }

  const bool heater_on = state.runningstate != 0;

  // Mirror device on/off state into our intent, except in external auto where
  // the device intentionally cycles on/off under our control.
  if (!(using_external && this->auto_requested_)) {
    this->climate_heat_requested_ = heater_on;
  }

  this->mode = (heater_on || this->climate_heat_requested_) ? climate::CLIMATE_MODE_HEAT : climate::CLIMATE_MODE_OFF;
  this->action = this->is_heating_step_(state.runningstep) ? climate::CLIMATE_ACTION_HEATING : climate::CLIMATE_ACTION_IDLE;

  // Keep user control preference synced with real heater mode, except when running
  // the external-auto strategy that intentionally uses manual backend levels.
  if (state.runningmode == 2) {
    this->auto_requested_ = true;
  } else if (state.runningmode == 1 && !(using_external && this->auto_requested_)) {
    this->auto_requested_ = false;
  }

  if (this->auto_requested_) {
    this->set_fan_mode_(climate::CLIMATE_FAN_AUTO);
    this->clear_custom_fan_mode_();
  } else {
    this->set_custom_fan_mode_(manual_level_mode_name(static_cast<uint8_t>(state.setlevel)));
  }
}

void DieselHeaterClimate::loop() {
  const uint32_t now = millis();
  const bool external_usage_changed = this->refresh_using_external_(now);

  if (external_usage_changed && !this->using_external_) {
    ESP_LOGW(TAG, "External sensor timed out (%u ms), falling back to built-in controls", this->ext_temp_timeout_ms_);
    if (this->auto_requested_ && this->climate_heat_requested_) {
      this->heater_->on_power_switch(true);
      this->heater_->on_temp_number(this->target_temperature);
    }
  }

  if (this->climate_heat_requested_ && this->auto_requested_ && this->using_external_) {
    this->apply_external_control_(false, external_usage_changed);
  }
}

void DieselHeaterClimate::set_external_temperature(float temp) {
  if (std::isnan(temp)) {
    return;
  }

  ESP_LOGD(TAG, "External temperature update: %.1f", temp);

  const float temp_c = this->ext_temp_fahrenheit_ ? (temp - 32.0f) * (5.0f / 9.0f) : temp;

  if (!this->using_external_) {
    ESP_LOGI(TAG, "External sensor active (%.1f°C), using staged external auto when selected", temp_c);
  }

  const uint32_t now = millis();
  this->external_temperature_ = temp_c;
  this->last_ext_temp_ms_ = now;
  this->using_external_ = true;
}

void DieselHeaterClimate::apply_external_control_(bool force_start_request, bool bypass_timers) {
  if (std::isnan(this->external_temperature_) || std::isnan(this->target_temperature)) {
    return;
  }

  const uint32_t now = millis();

  if (!bypass_timers &&
      static_cast<uint32_t>(now - this->last_level_update_ms_) < this->ext_level_update_interval_ms_) {
    return;
  }

  const float error = this->target_temperature - this->external_temperature_;
  const bool min_on_satisfied =
      bypass_timers || this->last_heat_transition_ms_ == 0 ||
      static_cast<uint32_t>(now - this->last_heat_transition_ms_) >= this->ext_min_on_time_ms_;
  const bool min_off_satisfied =
      bypass_timers || this->last_heat_transition_ms_ == 0 ||
      static_cast<uint32_t>(now - this->last_heat_transition_ms_) >= this->ext_min_off_time_ms_;

  bool heater_on = this->heater_->get_state().runningstate != 0;

  if (!heater_on) {
    const bool allowed_by_error = force_start_request ? (error > 0.0f) : (error >= this->ext_auto_on_error_c_);
    if (!allowed_by_error || !min_off_satisfied) {
      this->last_level_update_ms_ = now;
      return;
    }

    ESP_LOGD(TAG, "External auto start: ext=%.1f target=%.1f error=%.2f", this->external_temperature_,
             this->target_temperature, error);
    this->heater_->on_power_switch(true);
    heater_on = true;
    this->last_heat_transition_ms_ = now;
    this->overshoot_start_ms_ = 0;
  }

  // If running, check virtual off condition with hysteresis + dwell timers.
  if (error <= this->ext_off_error_c_) {
    if (this->overshoot_start_ms_ == 0) {
      this->overshoot_start_ms_ = now;
    }
  } else {
    this->overshoot_start_ms_ = 0;
  }

  const bool overshoot_hold_satisfied =
      (this->overshoot_start_ms_ != 0) &&
    (bypass_timers || static_cast<uint32_t>(now - this->overshoot_start_ms_) >= this->ext_overshoot_hold_ms_);

  if (heater_on && min_on_satisfied && overshoot_hold_satisfied) {
    ESP_LOGD(TAG, "External auto idle: ext=%.1f target=%.1f error=%.2f", this->external_temperature_,
             this->target_temperature, error);
    this->heater_->on_power_switch(false);
    this->last_heat_transition_ms_ = now;
    this->overshoot_start_ms_ = 0;
    this->last_level_update_ms_ = now;
    return;
  }

  if (!heater_on) {
    this->last_level_update_ms_ = now;
    return;
  }

  const uint8_t level = this->stage_level_for_error_(error);
  if (this->last_commanded_level_ != level) {
    ESP_LOGD(TAG, "External staged control: ext=%.1f target=%.1f error=%.2f -> level %u", this->external_temperature_,
             this->target_temperature, error, level);
  }

  this->heater_->on_power_level_number(level);
  this->last_commanded_level_ = level;
  this->last_level_update_ms_ = now;
}

void DieselHeaterClimate::setup() {
  if (this->heater_ == nullptr) {
    ESP_LOGE(TAG, "Heater not set for climate component!");
    return;
  }

  this->set_supported_custom_fan_modes(MANUAL_LEVEL_MODES);

  HeaterState state = this->heater_->get_state();
  this->sync_from_heater(state);

  ESP_LOGD(TAG, "Climate initialized. Mode: %d, Temp: %.1f, Running: %d, Running mode: %d", static_cast<int>(this->mode),
           this->target_temperature, state.runningstate, state.runningmode);

  this->publish_state();
}

climate::ClimateTraits DieselHeaterClimate::traits() {
  auto traits = climate::ClimateTraits();

  traits.add_feature_flags(climate::CLIMATE_SUPPORTS_CURRENT_TEMPERATURE | climate::CLIMATE_SUPPORTS_ACTION);
  traits.set_supported_modes({
      climate::CLIMATE_MODE_OFF,
      climate::CLIMATE_MODE_HEAT,
  });
  traits.set_supported_fan_modes({
      climate::CLIMATE_FAN_AUTO,
  });
  traits.set_visual_min_temperature(CLIMATE_MIN_TARGET_C);
  traits.set_visual_max_temperature(CLIMATE_MAX_TARGET_C);
  traits.set_visual_temperature_step(1.0f);

  return traits;
}

void DieselHeaterClimate::control(const climate::ClimateCall &call) {
  if (this->heater_ == nullptr) {
    ESP_LOGW(TAG, "Heater not set for climate component");
    return;
  }

  if (call.get_mode().has_value()) {
    climate::ClimateMode new_mode = *call.get_mode();
    ESP_LOGD(TAG, "Mode change requested: %d", static_cast<int>(new_mode));

    if (new_mode == climate::CLIMATE_MODE_OFF) {
      this->climate_heat_requested_ = false;
      this->mode = climate::CLIMATE_MODE_OFF;
      this->heater_->on_power_switch(false);
    } else {
      this->climate_heat_requested_ = true;
      this->mode = climate::CLIMATE_MODE_HEAT;

      if (this->auto_requested_ && this->using_external_) {
        this->apply_external_control_(true, true);
      } else if (this->auto_requested_) {
        this->heater_->on_power_switch(true);
        this->heater_->on_temp_number(this->target_temperature);
      } else {
        this->heater_->on_power_switch(true);
      }
    }
  }

  if (call.get_fan_mode().has_value()) {
    auto fan_mode = *call.get_fan_mode();
    if (fan_mode == climate::CLIMATE_FAN_AUTO) {
      this->auto_requested_ = true;

      if (this->climate_heat_requested_) {
        if (this->using_external_) {
          this->apply_external_control_(false, true);
        } else {
          this->heater_->on_power_switch(true);
          this->heater_->on_temp_number(this->target_temperature);
        }
      }
    }
  }

  if (call.has_custom_fan_mode()) {
    auto custom_mode = call.get_custom_fan_mode();
    uint8_t level = 0;
    if (parse_manual_level(custom_mode, level)) {
      this->auto_requested_ = false;
      if (this->climate_heat_requested_) {
        this->heater_->on_power_switch(true);
        this->heater_->on_power_level_number(level);
      }
      this->last_commanded_level_ = level;
      ESP_LOGD(TAG, "Manual level override: %u", level);
    } else {
      ESP_LOGW(TAG, "Invalid manual level fan mode: %.*s", static_cast<int>(custom_mode.size()), custom_mode.c_str());
    }
  }

  if (call.get_target_temperature().has_value()) {
    float new_temp = *call.get_target_temperature();
    this->target_temperature = std::clamp(new_temp, CLIMATE_MIN_TARGET_C, CLIMATE_MAX_TARGET_C);
    this->auto_requested_ = true;

    if (!this->climate_heat_requested_) {
      ESP_LOGD(TAG, "Target update %.1f while OFF: storing only, not waking heater", this->target_temperature);
      this->publish_state();
      return;
    }

    if (this->auto_requested_) {
      if (this->using_external_) {
        ESP_LOGD(TAG, "Target update %.1f in staged external auto", this->target_temperature);
        this->apply_external_control_(false, true);
      } else {
        ESP_LOGD(TAG, "Target update %.1f in internal auto", this->target_temperature);
        this->heater_->on_power_switch(true);
        this->heater_->on_temp_number(this->target_temperature);
      }
    }
  }

  this->publish_state();
}

}  // namespace diesel_heater_ble
}  // namespace esphome

#endif  // USE_ESP32
