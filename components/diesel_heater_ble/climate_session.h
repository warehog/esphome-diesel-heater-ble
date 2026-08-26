#pragma once

#include <cmath>
#include <cstdint>

#include "entity_control.h"

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

// Dependency-free transaction state used by the production climate component.
// The candidate remains private until every command in its sequence is
// accepted by the delivery FIFO. Publication is deliberately outside this
// class: only a parsed heater state may update climate's public fields.
struct ClimateAutomationIntent {
  bool heat_requested{false};
  bool auto_requested{true};
  float target_temperature_c{NAN};
  uint8_t level{1};
  uint32_t heat_transition_ms{0};
  uint32_t overshoot_start_ms{0};
  uint32_t level_update_ms{0};
};

class ClimateControlSession {
 public:
  const ClimateAutomationIntent &intent() const { return this->intent_; }
  bool transaction_active() const { return this->active_; }
  uint32_t transaction_id() const { return this->transaction_id_; }

  bool begin(const ClimateAutomationIntent &candidate) {
    if (this->active_) return false;
    this->snapshot_ = this->intent_;
    this->candidate_ = candidate;
    this->active_ = true;
    this->accepted_commands_ = 0;
    ++this->transaction_id_;
    if (this->transaction_id_ == 0) ++this->transaction_id_;
    return true;
  }

  // A rejected enqueue restores the previous automation intent immediately.
  bool accept(EntityCommandResult result) {
    if (!this->active_ || !entity_command_accepted(result)) {
      this->rollback_();
      return false;
    }
    // NO_CHANGE has no delivery operation to await, but it is still a safe
    // result for a transaction whose other members may have been queued.
    if (result == EntityCommandResult::QUEUED) ++this->accepted_commands_;
    return true;
  }

  // Commit to automation only after every sequence member was accepted. Keep
  // its rollback snapshot until delivery confirms every queued member.
  bool commit() {
    if (!this->active_) {
      this->rollback_();
      return false;
    }
    this->intent_ = this->candidate_;
    this->committed_ = true;
    if (this->accepted_commands_ == 0) this->finish_();
    return true;
  }

  void set_without_command(const ClimateAutomationIntent &intent) {
    if (!this->active_) this->intent_ = intent;
  }

  void delivery_completed(uint32_t transaction_id) {
    if (!this->active_ || transaction_id != this->transaction_id_ || !this->committed_) return;
    if (this->accepted_commands_ > 0) --this->accepted_commands_;
    if (this->accepted_commands_ == 0) this->finish_();
  }

  void delivery_failed(uint32_t transaction_id) {
    if (this->active_ && transaction_id == this->transaction_id_) this->rollback_();
  }

  void reset() {
    this->intent_ = ClimateAutomationIntent{};
    this->snapshot_ = ClimateAutomationIntent{};
    this->candidate_ = ClimateAutomationIntent{};
    this->active_ = false;
    this->committed_ = false;
    this->accepted_commands_ = 0;
  }

 private:
  void rollback_() {
    if (this->active_) this->intent_ = this->snapshot_;
    this->finish_();
  }
  void finish_() {
    this->active_ = false;
    this->committed_ = false;
    this->accepted_commands_ = 0;
  }

  ClimateAutomationIntent intent_{};
  ClimateAutomationIntent snapshot_{};
  ClimateAutomationIntent candidate_{};
  uint32_t transaction_id_{0};
  uint8_t accepted_commands_{0};
  bool active_{false};
  bool committed_{false};
};

// Dependency-free runtime seam shared by DieselHeaterClimate and host tests.
// It owns request transaction handling, external-sensor normalization, and the
// small confirmed-state projection that prevents local intent leaking into
// published climate fields.
struct ClimateConfirmedProjection {
  bool mode_heat{false};
  bool action_heating{false};
  bool target_available{false};
  float target_temperature_c{NAN};
};

class ClimateControlRuntime {
 public:
  const ClimateAutomationIntent &intent() const { return this->session_.intent(); }
  bool transaction_active() const { return this->session_.transaction_active(); }
  uint32_t transaction_id() const { return this->session_.transaction_id(); }
  bool begin(const ClimateAutomationIntent &candidate) { return this->session_.begin(candidate); }

  template<typename CancelPending> bool accept(EntityCommandResult result, CancelPending &&cancel_pending) {
    if (!entity_command_accepted(result)) cancel_pending(this->session_.transaction_id());
    return this->session_.accept(result);
  }

  bool commit() { return this->session_.commit(); }
  void delivery_completed(uint32_t transaction_id) { this->session_.delivery_completed(transaction_id); }
  void delivery_failed(uint32_t transaction_id) { this->session_.delivery_failed(transaction_id); }
  void set_without_command(const ClimateAutomationIntent &intent) { this->session_.set_without_command(intent); }

  bool set_external_temperature(float input, bool fahrenheit, uint32_t now) {
    if (!std::isfinite(input)) return false;
    this->external_temperature_c_ = fahrenheit ? (input - 32.0f) * (5.0f / 9.0f) : input;
    this->last_external_temperature_ms_ = now;
    return true;
  }
  float external_temperature_c() const { return this->external_temperature_c_; }
  uint32_t last_external_temperature_ms() const { return this->last_external_temperature_ms_; }
  bool external_sample_recent(uint32_t now, uint32_t timeout_ms) const {
    return this->last_external_temperature_ms_ != 0 &&
           static_cast<uint32_t>(now - this->last_external_temperature_ms_) <= timeout_ms;
  }

  static ClimateConfirmedProjection project_confirmed_state(bool power_valid, bool power, bool legacy_heating_valid,
                                                            bool legacy_heating, bool target_valid, float target_c) {
    ClimateConfirmedProjection projection;
    projection.mode_heat = power_valid && power;
    projection.action_heating = projection.mode_heat && legacy_heating_valid && legacy_heating;
    projection.target_available = target_valid && EntityControlPolicy::valid_temperature_c(target_c);
    if (projection.target_available) projection.target_temperature_c = target_c;
    return projection;
  }

  void reset() {
    this->session_.reset();
    this->external_temperature_c_ = NAN;
    this->last_external_temperature_ms_ = 0;
  }

 private:
  ClimateControlSession session_{};
  float external_temperature_c_{NAN};
  uint32_t last_external_temperature_ms_{0};
};

}  // namespace diesel_heater_ble
}  // namespace esphome
