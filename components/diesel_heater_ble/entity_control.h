#pragma once

#include <cmath>

#include "command_delivery.h"
#include "lifecycle.h"

namespace esphome {
namespace diesel_heater_ble {

// Entity-facing control policy. This is intentionally independent of ESPHome
// and AA implementation details: entities request normalized operations, while the
// selected protocol driver remains the only encoder.
enum class EntityOperation : uint8_t {
  POWER,
  MANUAL_LEVEL,
  TEMPERATURE_C,
  TEMPERATURE_OFFSET_C,
};

// The delivery FIFO preserves the source of a control so a final asynchronous
// failure can be routed back to climate automation without treating a poll or
// a standalone entity action as climate state.
enum class CommandOrigin : uint8_t {
  ENTITY,
  CLIMATE,
};

enum class ClimateStrategy : uint8_t {
  INTERNAL_THERMOSTAT,
  EXTERNAL_STAGED,
  MANUAL_LEVEL,
};

// Every public entity entry point reports why it did not submit work. QUEUED
// only means the delivery FIFO accepted a request; entities are updated only
// by a later confirmed heater response.
enum class EntityCommandResult : uint8_t {
  QUEUED,
  NO_CHANGE,
  UNSUPPORTED,
  CONNECTION_UNAVAILABLE,
  HANDLE_UNAVAILABLE,
  DRIVER_UNAVAILABLE,
  CAPABILITY_UNAVAILABLE,
  STATE_UNAVAILABLE,
  INVALID_VALUE,
  DELIVERY_QUEUE_FULL,
  DELIVERY_TRANSPORT_BLOCKED,
  DELIVERY_REJECTED,
};

constexpr bool entity_command_accepted(EntityCommandResult result) {
  return result == EntityCommandResult::QUEUED || result == EntityCommandResult::NO_CHANGE;
}

class EntityControlPolicy {
 public:
  static constexpr float kMinTemperatureC = 8.0f;
  static constexpr float kMaxTemperatureC = 36.0f;
  // The APK's raw parameter slider is integer -9..9 in the active reported
  // temperature unit. A static ESPHome number cannot change its traits when
  // the heater changes units, so this is the safe normalized-Celsius overlap.
  static constexpr float kMinTemperatureOffsetC = -5.0f;
  static constexpr float kMaxTemperatureOffsetC = 5.0f;
  static constexpr uint8_t kMinManualLevel = 1;
  static constexpr uint8_t kMaxManualLevel = 10;

  static constexpr Capabilities required_capabilities(EntityOperation operation) {
    switch (operation) {
      case EntityOperation::POWER:
        return capability_bits(Capability::POWER);
      case EntityOperation::MANUAL_LEVEL:
        return capability_bits(Capability::MODE) | Capability::MANUAL_LEVEL;
      case EntityOperation::TEMPERATURE_C:
        return capability_bits(Capability::MODE) | Capability::SET_TEMPERATURE;
      case EntityOperation::TEMPERATURE_OFFSET_C:
        return capability_bits(Capability::EXTENDED_SETTINGS);
    }
    return capability_bits(Capability::NONE);
  }

  static constexpr Capabilities required_capabilities(ClimateStrategy strategy) {
    switch (strategy) {
      case ClimateStrategy::INTERNAL_THERMOSTAT:
        return required_capabilities(EntityOperation::POWER) |
               required_capabilities(EntityOperation::TEMPERATURE_C);
      case ClimateStrategy::EXTERNAL_STAGED:
      case ClimateStrategy::MANUAL_LEVEL:
        return required_capabilities(EntityOperation::POWER) |
               required_capabilities(EntityOperation::MANUAL_LEVEL);
    }
    return capability_bits(Capability::NONE);
  }

  static bool valid_manual_level(float level) {
    return std::isfinite(level) && level >= kMinManualLevel && level <= kMaxManualLevel && std::floor(level) == level;
  }
  static bool valid_temperature_c(float temperature_c) {
    return std::isfinite(temperature_c) && temperature_c >= kMinTemperatureC && temperature_c <= kMaxTemperatureC;
  }
  static bool valid_temperature_offset_c(float temperature_offset_c) {
    return std::isfinite(temperature_offset_c) && temperature_offset_c >= kMinTemperatureOffsetC &&
           temperature_offset_c <= kMaxTemperatureOffsetC && std::floor(temperature_offset_c) == temperature_offset_c;
  }
  static bool valid(EntityOperation operation, float value) {
    switch (operation) {
      case EntityOperation::POWER:
        return true;
      case EntityOperation::MANUAL_LEVEL:
        return valid_manual_level(value);
      case EntityOperation::TEMPERATURE_C:
        return valid_temperature_c(value);
      case EntityOperation::TEMPERATURE_OFFSET_C:
        return valid_temperature_offset_c(value);
    }
    return false;
  }

  static EntityCommandResult admit(const LifecycleSession &lifecycle, EntityOperation operation, float value = 0.0f) {
    return from_admission(lifecycle.admit(required_capabilities(operation), valid(operation, value)));
  }
  static EntityCommandResult admit(const LifecycleSession &lifecycle, ClimateStrategy strategy) {
    return from_admission(lifecycle.admit(required_capabilities(strategy), true));
  }

  static EntityCommandResult from_admission(CommandAdmission result) {
    switch (result) {
      case CommandAdmission::READY:
        return EntityCommandResult::QUEUED;
      case CommandAdmission::CONNECTION_UNAVAILABLE:
        return EntityCommandResult::CONNECTION_UNAVAILABLE;
      case CommandAdmission::HANDLE_UNAVAILABLE:
        return EntityCommandResult::HANDLE_UNAVAILABLE;
      case CommandAdmission::DRIVER_UNAVAILABLE:
        return EntityCommandResult::DRIVER_UNAVAILABLE;
      case CommandAdmission::CAPABILITY_UNAVAILABLE:
        return EntityCommandResult::CAPABILITY_UNAVAILABLE;
      case CommandAdmission::STATE_UNAVAILABLE:
        return EntityCommandResult::STATE_UNAVAILABLE;
      case CommandAdmission::INVALID_VALUE:
        return EntityCommandResult::INVALID_VALUE;
    }
    return EntityCommandResult::DELIVERY_REJECTED;
  }

  static EntityCommandResult from_batch(CommandResult result) {
    switch (result) {
      case CommandResult::READY:
        return EntityCommandResult::QUEUED;
      case CommandResult::NO_CHANGE:
        return EntityCommandResult::NO_CHANGE;
      case CommandResult::UNSUPPORTED:
        return EntityCommandResult::UNSUPPORTED;
      case CommandResult::STATE_UNAVAILABLE:
        return EntityCommandResult::STATE_UNAVAILABLE;
      case CommandResult::INVALID_VALUE:
        return EntityCommandResult::INVALID_VALUE;
    }
    return EntityCommandResult::DELIVERY_REJECTED;
  }

  static EntityCommandResult from_delivery(DeliveryEnqueueResult result) {
    switch (result) {
      case DeliveryEnqueueResult::QUEUED:
        return EntityCommandResult::QUEUED;
      case DeliveryEnqueueResult::QUEUE_FULL:
        return EntityCommandResult::DELIVERY_QUEUE_FULL;
      case DeliveryEnqueueResult::TRANSPORT_BLOCKED:
        return EntityCommandResult::DELIVERY_TRANSPORT_BLOCKED;
      case DeliveryEnqueueResult::INVALID_BATCH:
      case DeliveryEnqueueResult::POLL_SUPPRESSED:
        return EntityCommandResult::DELIVERY_REJECTED;
    }
    return EntityCommandResult::DELIVERY_REJECTED;
  }
};

}  // namespace diesel_heater_ble
}  // namespace esphome
