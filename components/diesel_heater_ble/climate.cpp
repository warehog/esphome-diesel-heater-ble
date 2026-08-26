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
static constexpr float CLIMATE_MIN_TARGET_C = EntityControlPolicy::kMinTemperatureC;
static constexpr float CLIMATE_MAX_TARGET_C = EntityControlPolicy::kMaxTemperatureC;
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
  if (this->heater_ == nullptr) return false;
  const bool was_using_external = this->using_external_;
  const HeaterState state = this->heater_->get_state();
  const bool is_heating = state.valid && state.power.valid && state.power.value;
  const uint32_t timeout = is_heating ? this->ext_temp_timeout_heating_ms_ : this->ext_temp_timeout_ms_;
  const bool has_recent_sample = this->control_runtime_.external_sample_recent(now, timeout);
  this->using_external_ = has_recent_sample;
  return this->using_external_ != was_using_external;
}

bool DieselHeaterClimate::climate_strategy_ready_(ClimateStrategy strategy, const char *context) const {
  if (this->heater_ == nullptr) return false;
  const EntityCommandResult result = this->heater_->admit_climate_strategy(strategy);
  if (result == EntityCommandResult::QUEUED) return true;
  ESP_LOGD(TAG, "%s skipped, command admission: %u", context, static_cast<unsigned>(result));
  return false;
}

float DieselHeaterClimate::control_target_temperature_() const {
  if (EntityControlPolicy::valid_temperature_c(this->control_runtime_.intent().target_temperature_c))
    return this->control_runtime_.intent().target_temperature_c;
  if (EntityControlPolicy::valid_temperature_c(this->target_temperature)) return this->target_temperature;
  return CLIMATE_DEFAULT_TARGET_C;
}

bool DieselHeaterClimate::begin_control_(const ClimateAutomationIntent &candidate) {
  if (this->control_runtime_.begin(candidate)) return true;
  ESP_LOGD(TAG, "Climate control skipped while a prior request is awaiting delivery");
  return false;
}

bool DieselHeaterClimate::accept_control_(EntityCommandResult result) {
  return this->control_runtime_.accept(result, [this](uint32_t transaction_id) {
    if (this->heater_ != nullptr) this->heater_->cancel_pending_climate_transaction(transaction_id);
  });
}

void DieselHeaterClimate::commit_control_() { this->control_runtime_.commit(); }

void DieselHeaterClimate::on_control_delivery(uint32_t transaction_id, bool success) {
  if (success)
    this->control_runtime_.delivery_completed(transaction_id);
  else
    this->control_runtime_.delivery_failed(transaction_id);
}

void DieselHeaterClimate::sync_from_heater(const HeaterState &state) {
  const bool using_external = this->using_external_;

  if (using_external) {
    this->current_temperature = this->control_runtime_.external_temperature_c();
  } else if (state.cabin_temperature_c.valid) {
    this->current_temperature = state.cabin_temperature_c.value;
  } else {
    this->current_temperature = NAN;
  }

  const ClimateConfirmedProjection projection = ClimateControlRuntime::project_confirmed_state(
      state.power.valid, state.power.value, state.legacy_ui_heating.valid, state.legacy_ui_heating.value,
      state.target_temperature_c.valid, state.target_temperature_c.value);
  this->target_temperature = projection.target_available ? projection.target_temperature_c : NAN;
  const bool heater_on = projection.mode_heat;

  // Keep automation intent aligned with confirmed device power, except in
  // external auto where the device intentionally cycles on/off under control.
  if (!(using_external && this->control_runtime_.intent().auto_requested)) {
    ClimateAutomationIntent intent = this->control_runtime_.intent();
    intent.heat_requested = heater_on;
    this->control_runtime_.set_without_command(intent);
  }

  // Public climate state is a projection of this parsed heater response only.
  // Local automation intent may decide future commands but never a published
  // mode, target, or action.
  this->mode = projection.mode_heat ? climate::CLIMATE_MODE_HEAT : climate::CLIMATE_MODE_OFF;
  if (this->mode == climate::CLIMATE_MODE_OFF) {
    this->action = climate::CLIMATE_ACTION_OFF;
  } else {
    this->action = projection.action_heating
                       ? climate::CLIMATE_ACTION_HEATING
                       : climate::CLIMATE_ACTION_IDLE;
  }

  // Keep user control preference synced with real heater mode, except when running
  // the external-auto strategy that intentionally uses manual backend levels.
  if (state.operating_mode.valid && state.operating_mode.value == OperatingMode::THERMOSTAT) {
    ClimateAutomationIntent intent = this->control_runtime_.intent();
    intent.auto_requested = true;
    this->control_runtime_.set_without_command(intent);
  } else if (state.operating_mode.valid && state.operating_mode.value == OperatingMode::MANUAL_LEVEL &&
             !(using_external && this->control_runtime_.intent().auto_requested)) {
    ClimateAutomationIntent intent = this->control_runtime_.intent();
    intent.auto_requested = false;
    this->control_runtime_.set_without_command(intent);
  }

  if (this->control_runtime_.intent().auto_requested) {
    this->fan_session_.set_auto();
    this->set_fan_mode_(climate::CLIMATE_FAN_AUTO);
    this->clear_custom_fan_mode_();
  } else if (state.manual_level.valid) {
    ClimateAutomationIntent intent = this->control_runtime_.intent();
    intent.level = state.manual_level.value;
    this->control_runtime_.set_without_command(intent);
    this->fan_session_.set_manual();
    this->set_custom_fan_mode_(manual_level_mode_name(state.manual_level.value));
  } else {
    this->fan_session_.clear();
    this->fan_mode.reset();
    this->clear_custom_fan_mode_();
  }
}

void DieselHeaterClimate::on_heater_disconnect() {
  // A reconnect is a new heater session. Do not let locally retained external
  // automation intent send a command before that session has confirmed state.
  this->using_external_ = false;
  this->control_runtime_.reset();
  this->current_temperature = NAN;
  this->target_temperature = NAN;
  this->mode = climate::CLIMATE_MODE_OFF;
  this->action = climate::CLIMATE_ACTION_OFF;
  this->fan_session_.clear();
  this->fan_mode.reset();
  this->clear_custom_fan_mode_();
  // Climate has no native unavailable publication in the supported ESPHome
  // API. Clearing its local state prevents stale values from being republished.
  this->set_has_state(false);
}

void DieselHeaterClimate::loop() {
  if (this->heater_ == nullptr) return;
  const uint32_t now = millis();
  const bool external_usage_changed = this->refresh_using_external_(now);

  if (external_usage_changed && !this->using_external_) {
    ESP_LOGW(TAG, "External sensor timed out (%lu ms), falling back to built-in controls",
             static_cast<unsigned long>(now - this->control_runtime_.last_external_temperature_ms()));
    if (this->control_runtime_.intent().auto_requested && this->control_runtime_.intent().heat_requested &&
        this->climate_strategy_ready_(ClimateStrategy::INTERNAL_THERMOSTAT, "External fallback")) {
      const ClimateAutomationIntent candidate = this->control_runtime_.intent();
      if (!this->begin_control_(candidate)) return;
      const uint32_t transaction_id = this->control_runtime_.transaction_id();
      if (!this->accept_control_(this->heater_->on_power_switch(true, transaction_id))) return;
      if (!this->accept_control_(this->heater_->on_temp_number(this->control_target_temperature_(), transaction_id))) return;
      this->commit_control_();
    }
  }

  if (this->control_runtime_.intent().heat_requested && this->control_runtime_.intent().auto_requested && this->using_external_) {
    this->apply_external_control_(false, external_usage_changed);
  }
}

void DieselHeaterClimate::set_external_temperature(float temp) {
  ESP_LOGD(TAG, "External temperature update: %.1f", temp);

  if (!this->using_external_) {
    ESP_LOGI(TAG, "External sensor active, using staged external auto when selected");
  }

  const uint32_t now = millis();
  if (!this->control_runtime_.set_external_temperature(temp, this->ext_temp_fahrenheit_, now)) return;
  this->using_external_ = true;
}

void DieselHeaterClimate::apply_external_control_(bool force_start_request, bool bypass_timers) {
  if (this->heater_ == nullptr || !this->climate_strategy_ready_(ClimateStrategy::EXTERNAL_STAGED, "External control") ||
      !std::isfinite(this->control_runtime_.external_temperature_c()) || this->control_runtime_.transaction_active()) {
    return;
  }

  const uint32_t now = millis();
  const ClimateAutomationIntent current = this->control_runtime_.intent();

  if (!bypass_timers &&
      static_cast<uint32_t>(now - current.level_update_ms) < this->ext_level_update_interval_ms_) {
    return;
  }

  const float target_temperature = this->control_target_temperature_();
  const float error = target_temperature - this->control_runtime_.external_temperature_c();
  const bool min_on_satisfied =
      bypass_timers || current.heat_transition_ms == 0 ||
      static_cast<uint32_t>(now - current.heat_transition_ms) >= this->ext_min_on_time_ms_;
  const bool min_off_satisfied =
      bypass_timers || current.heat_transition_ms == 0 ||
      static_cast<uint32_t>(now - current.heat_transition_ms) >= this->ext_min_off_time_ms_;

  const HeaterState state = this->heater_->get_state();
  bool heater_on = state.valid && state.power.valid && state.power.value;

  if (!heater_on) {
    const bool allowed_by_error = force_start_request ? (error > 0.0f) : (error >= this->ext_auto_on_error_c_);
    if (!allowed_by_error || !min_off_satisfied) {
      return;
    }

    ESP_LOGD(TAG, "External auto start: ext=%.1f target=%.1f error=%.2f", this->control_runtime_.external_temperature_c(),
             target_temperature, error);
    ClimateAutomationIntent candidate = current;
    candidate.heat_requested = true;
    candidate.heat_transition_ms = now;
    candidate.overshoot_start_ms = 0;
    candidate.level_update_ms = now;
    const uint8_t level = this->stage_level_for_error_(error);
    candidate.level = level;
    if (!this->begin_control_(candidate)) return;
    const uint32_t transaction_id = this->control_runtime_.transaction_id();
    if (!this->accept_control_(this->heater_->on_power_switch(true, transaction_id))) return;
    // Begin with an explicit manual level so a heater that starts at an old
    // level cannot briefly run an unintended stage. The strategy preflight
    // has already required both power and manual capabilities.
    if (!this->accept_control_(this->heater_->on_power_level_number(level, transaction_id))) return;
    this->commit_control_();
    return;
  }

  // If running, check virtual off condition with hysteresis + dwell timers.
  if (error <= this->ext_off_error_c_) {
    if (current.overshoot_start_ms == 0) {
      ClimateAutomationIntent timer_started = current;
      timer_started.overshoot_start_ms = now;
      this->control_runtime_.set_without_command(timer_started);
      return;
    }
  } else if (current.overshoot_start_ms != 0) {
    ClimateAutomationIntent timer_cleared = current;
    timer_cleared.overshoot_start_ms = 0;
    this->control_runtime_.set_without_command(timer_cleared);
    return;
  }

  const bool overshoot_hold_satisfied =
      (current.overshoot_start_ms != 0) &&
    (bypass_timers || static_cast<uint32_t>(now - current.overshoot_start_ms) >= this->ext_overshoot_hold_ms_);

  if (heater_on && min_on_satisfied && overshoot_hold_satisfied) {
    ESP_LOGD(TAG, "External auto idle: ext=%.1f target=%.1f error=%.2f", this->control_runtime_.external_temperature_c(),
             target_temperature, error);
    ClimateAutomationIntent candidate = current;
    candidate.heat_transition_ms = now;
    candidate.overshoot_start_ms = 0;
    candidate.level_update_ms = now;
    if (!this->begin_control_(candidate)) return;
    const uint32_t transaction_id = this->control_runtime_.transaction_id();
    if (!this->accept_control_(this->heater_->on_power_switch(false, transaction_id))) return;
    this->commit_control_();
    return;
  }

  if (!heater_on) {
    return;
  }

  const uint8_t level = this->stage_level_for_error_(error);
  if (current.level != level) {
    ESP_LOGD(TAG, "External staged control: ext=%.1f target=%.1f error=%.2f -> level %u", this->control_runtime_.external_temperature_c(),
             target_temperature, error, level);
  }

  ClimateAutomationIntent candidate = current;
  candidate.level = level;
  candidate.level_update_ms = now;
  if (!this->begin_control_(candidate)) return;
  const uint32_t transaction_id = this->control_runtime_.transaction_id();
  if (!this->accept_control_(this->heater_->on_power_level_number(level, transaction_id))) return;
  this->commit_control_();
}

void DieselHeaterClimate::setup() {
  if (this->heater_ == nullptr) {
    ESP_LOGE(TAG, "Heater not set for climate component!");
    return;
  }

  this->set_supported_custom_fan_modes(MANUAL_LEVEL_MODES);

  // The heater component publishes the climate only after a complete parsed
  // response confirms the current connection generation.
  this->set_has_state(false);
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
    ClimateAutomationIntent candidate = this->control_runtime_.intent();

    if (new_mode == climate::CLIMATE_MODE_OFF) {
      candidate.heat_requested = false;
      if (!this->begin_control_(candidate)) return;
      const uint32_t transaction_id = this->control_runtime_.transaction_id();
      if (!this->accept_control_(this->heater_->on_power_switch(false, transaction_id))) return;
      this->commit_control_();
    } else {
      candidate.heat_requested = true;
      if (candidate.auto_requested && this->using_external_) {
        if (!this->climate_strategy_ready_(ClimateStrategy::EXTERNAL_STAGED, "External heat request")) return;
        if (!this->begin_control_(candidate)) return;
        this->commit_control_();
        this->apply_external_control_(true, true);
      } else if (candidate.auto_requested) {
        if (!this->climate_strategy_ready_(ClimateStrategy::INTERNAL_THERMOSTAT, "Thermostat heat request")) return;
        if (!this->begin_control_(candidate)) return;
        const uint32_t transaction_id = this->control_runtime_.transaction_id();
        if (!this->accept_control_(this->heater_->on_power_switch(true, transaction_id))) return;
        if (!this->accept_control_(this->heater_->on_temp_number(this->control_target_temperature_(), transaction_id))) return;
        this->commit_control_();
      } else {
        if (!this->climate_strategy_ready_(ClimateStrategy::MANUAL_LEVEL, "Manual heat request")) return;
        if (!this->begin_control_(candidate)) return;
        const uint32_t transaction_id = this->control_runtime_.transaction_id();
        if (!this->accept_control_(this->heater_->on_power_switch(true, transaction_id))) return;
        this->commit_control_();
      }
    }
  }

  if (call.get_fan_mode().has_value()) {
    auto fan_mode = *call.get_fan_mode();
    if (fan_mode == climate::CLIMATE_FAN_AUTO) {
      ClimateAutomationIntent candidate = this->control_runtime_.intent();
      candidate.auto_requested = true;
      if (candidate.heat_requested) {
        if (this->using_external_) {
          if (!this->climate_strategy_ready_(ClimateStrategy::EXTERNAL_STAGED, "External auto request")) return;
          if (!this->begin_control_(candidate)) return;
          this->commit_control_();
          this->apply_external_control_(false, true);
        } else {
          if (!this->climate_strategy_ready_(ClimateStrategy::INTERNAL_THERMOSTAT, "Thermostat auto request")) return;
          if (!this->begin_control_(candidate)) return;
          const uint32_t transaction_id = this->control_runtime_.transaction_id();
          if (!this->accept_control_(this->heater_->on_power_switch(true, transaction_id))) return;
          if (!this->accept_control_(this->heater_->on_temp_number(this->control_target_temperature_(), transaction_id))) return;
          this->commit_control_();
        }
      } else {
        if (!this->begin_control_(candidate)) return;
        this->commit_control_();
      }
    }
  }

  if (call.has_custom_fan_mode()) {
    auto custom_mode = call.get_custom_fan_mode();
    uint8_t level = 0;
    if (parse_manual_level(custom_mode, level)) {
      ClimateAutomationIntent candidate = this->control_runtime_.intent();
      candidate.auto_requested = false;
      candidate.level = level;
      if (candidate.heat_requested) {
        if (!this->climate_strategy_ready_(ClimateStrategy::MANUAL_LEVEL, "Manual level request")) return;
        if (!this->begin_control_(candidate)) return;
        const uint32_t transaction_id = this->control_runtime_.transaction_id();
        if (!this->accept_control_(this->heater_->on_power_switch(true, transaction_id))) return;
        if (!this->accept_control_(this->heater_->on_power_level_number(level, transaction_id))) return;
        this->commit_control_();
      } else {
        if (!this->begin_control_(candidate)) return;
        this->commit_control_();
      }
      ESP_LOGD(TAG, "Manual level override: %u", level);
    } else {
      ESP_LOGW(TAG, "Invalid manual level fan mode: %.*s", static_cast<int>(custom_mode.size()), custom_mode.c_str());
    }
  }

  if (call.get_target_temperature().has_value()) {
    float new_temp = *call.get_target_temperature();
    if (!EntityControlPolicy::valid_temperature_c(new_temp)) {
      ESP_LOGW(TAG, "Rejected target temperature outside normalized Celsius range: %.1f", new_temp);
      return;
    }
    ClimateAutomationIntent candidate = this->control_runtime_.intent();
    candidate.target_temperature_c = new_temp;
    candidate.auto_requested = true;

    if (!candidate.heat_requested) {
      ESP_LOGD(TAG, "Target request %.1f while OFF: awaiting confirmed heater state", new_temp);
      if (!this->begin_control_(candidate)) return;
      this->commit_control_();
      return;
    }

    if (candidate.auto_requested) {
      if (this->using_external_) {
        if (!this->climate_strategy_ready_(ClimateStrategy::EXTERNAL_STAGED, "External target request")) return;
        if (!this->begin_control_(candidate)) return;
        this->commit_control_();
        ESP_LOGD(TAG, "Target request %.1f in staged external auto", new_temp);
        this->apply_external_control_(false, true);
      } else {
        if (!this->climate_strategy_ready_(ClimateStrategy::INTERNAL_THERMOSTAT, "Thermostat target request")) return;
        if (!this->begin_control_(candidate)) return;
        ESP_LOGD(TAG, "Target request %.1f in internal auto", new_temp);
        const uint32_t transaction_id = this->control_runtime_.transaction_id();
        if (!this->accept_control_(this->heater_->on_power_switch(true, transaction_id))) return;
        if (!this->accept_control_(this->heater_->on_temp_number(new_temp, transaction_id))) return;
        this->commit_control_();
      }
    }
  }
}

}  // namespace diesel_heater_ble
}  // namespace esphome

#endif  // USE_ESP32
