#pragma once

#include <cstdint>

#include "contracts.h"

namespace esphome {
namespace diesel_heater_ble {

// Dependency-free lifecycle and command-admission seam. The BLE component
// owns the real GATT operations, while this value type owns the ordering of
// their preconditions and the state that must not survive a disconnect.
enum class CommandAdmission : uint8_t {
  READY,
  CONNECTION_UNAVAILABLE,
  HANDLE_UNAVAILABLE,
  DRIVER_UNAVAILABLE,
  CAPABILITY_UNAVAILABLE,
  STATE_UNAVAILABLE,
  INVALID_VALUE,
};

class LifecycleSession {
 public:
  void begin_connection() { this->reset_(); }

  // The characteristic can be discovered before notification registration,
  // but no traffic is admitted until notification registration succeeds.
  void set_characteristic_handle(uint16_t handle) { this->handle_ = handle; }
  void set_established() { this->connected_ = true; }

  // Selection and state confirmation are separate so admission can represent
  // an explicitly selected driver that has not yet produced a complete state.
  void select_driver(Capabilities capabilities) {
    this->driver_selected_ = true;
    this->capabilities_ = capabilities;
  }
  // A complete parsed response is the normal runtime path; it confirms both
  // the selected driver and current normalized state for this connection.
  void confirm_response(Capabilities capabilities) {
    this->select_driver(capabilities);
    this->state_confirmed_ = true;
  }

  // Returns true only once for a published entity generation. The caller uses
  // that edge to publish unavailable states exactly once on disconnect.
  void mark_entities_published() {
    if (this->can_publish_state()) this->entities_published_ = true;
  }

  bool disconnect() {
    const bool entities_were_published = this->entities_published_;
    this->reset_();
    return entities_were_published;
  }

  bool transport_ready() const { return this->connected_ && this->handle_ != 0; }
  bool can_publish_state() const {
    return this->transport_ready() && this->driver_selected_ && this->state_confirmed_;
  }
  bool connected() const { return this->connected_; }
  uint16_t handle() const { return this->handle_; }

  // Preconditions intentionally remain ordered. A caller gets the first
  // actionable failure and must not construct or write a command on failure.
  CommandAdmission admit(Capabilities required_capabilities, bool value_valid) const {
    if (!this->connected_) return CommandAdmission::CONNECTION_UNAVAILABLE;
    if (this->handle_ == 0) return CommandAdmission::HANDLE_UNAVAILABLE;
    if (!this->driver_selected_) return CommandAdmission::DRIVER_UNAVAILABLE;
    if ((this->capabilities_ & required_capabilities) != required_capabilities)
      return CommandAdmission::CAPABILITY_UNAVAILABLE;
    if (!this->state_confirmed_) return CommandAdmission::STATE_UNAVAILABLE;
    if (!value_valid) return CommandAdmission::INVALID_VALUE;
    return CommandAdmission::READY;
  }

 private:
  void reset_() {
    this->connected_ = false;
    this->handle_ = 0;
    this->driver_selected_ = false;
    this->state_confirmed_ = false;
    this->entities_published_ = false;
    this->capabilities_ = capability_bits(Capability::NONE);
  }

  bool connected_{false};
  uint16_t handle_{0};
  bool driver_selected_{false};
  bool state_confirmed_{false};
  bool entities_published_{false};
  Capabilities capabilities_{capability_bits(Capability::NONE)};
};

}  // namespace diesel_heater_ble
}  // namespace esphome
