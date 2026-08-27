#pragma once

#include <cstdint>

namespace esphome {
namespace diesel_heater_ble {

// Dependency-free clock scheduling policy.  Production adapts an ESPHome
// RealTimeClock to ClockTime; host tests use the same policy without ESPHome.
struct ClockTime {
  bool valid{false};
  uint8_t hour{0};
  uint8_t minute{0};
};

class ClockSyncRuntime {
 public:
  // The protocol has no timezone evidence.  The supplied RealTimeClock owns
  // that decision; this component sends its local hour and minute unchanged.
  static constexpr uint32_t kPeriodicSyncIntervalMs = 60U * 60U * 1000U;
  static constexpr uint32_t kRetryIntervalMs = 60U * 1000U;

  static bool minutes_since_midnight(ClockTime time, uint16_t &minutes) {
    if (!time.valid || time.hour >= 24 || time.minute >= 60) return false;
    minutes = static_cast<uint16_t>(time.hour) * 60U + time.minute;
    return true;
  }

  // `ready` represents an established connection, selected driver, and
  // successfully parsed state. `delivery_idle` keeps clock writes from
  // overlapping a status request or user control operation.
  bool should_enqueue(uint32_t now, bool ready, bool delivery_idle, ClockTime time) const {
    uint16_t ignored_minutes = 0;
    if (!ready || !delivery_idle || this->in_flight_ || !minutes_since_midnight(time, ignored_minutes)) return false;
    if (!this->last_attempt_valid_) return true;
    // A failed periodic delivery must not fall straight through to the expired
    // successful-sync timestamp and enqueue on every loop.  Rate-limit every
    // failure from its queue time, just as an initial-sync failure is limited.
    if (this->last_delivery_failed_ || !this->initial_sync_complete_)
      return elapsed_(now, this->last_attempt_ms_) >= kRetryIntervalMs;
    return elapsed_(now, this->last_success_ms_) >= kPeriodicSyncIntervalMs;
  }

  void queued(uint32_t now) {
    this->in_flight_ = true;
    this->last_delivery_failed_ = false;
    this->last_attempt_ms_ = now;
    this->last_attempt_valid_ = true;
  }

  // Call only for the tagged command-delivery event.  In particular, a write
  // completion must not update the successful-sync timestamp.
  void delivery_completed(uint32_t now, bool success) {
    if (!this->in_flight_) return;
    this->in_flight_ = false;
    if (!success) {
      this->last_delivery_failed_ = true;
      return;
    }
    this->last_delivery_failed_ = false;
    this->initial_sync_complete_ = true;
    this->last_success_ms_ = now;
  }

  // Each BLE connection receives an initial synchronization after its first
  // valid parsed state. A disconnect also makes any in-flight request stale.
  void reset_connection() {
    this->initial_sync_complete_ = false;
    this->in_flight_ = false;
    this->last_delivery_failed_ = false;
    this->last_attempt_valid_ = false;
    this->last_attempt_ms_ = 0;
    this->last_success_ms_ = 0;
  }

  bool in_flight() const { return this->in_flight_; }
  bool initial_sync_complete() const { return this->initial_sync_complete_; }
  bool last_attempt_valid() const { return this->last_attempt_valid_; }
  uint32_t last_success_ms() const { return this->last_success_ms_; }

 private:
  static uint32_t elapsed_(uint32_t now, uint32_t then) { return now - then; }

  bool in_flight_{false};
  bool last_delivery_failed_{false};
  bool initial_sync_complete_{false};
  bool last_attempt_valid_{false};
  uint32_t last_attempt_ms_{0};
  uint32_t last_success_ms_{0};
};

}  // namespace diesel_heater_ble
}  // namespace esphome
