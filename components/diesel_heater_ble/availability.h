#pragma once

#include <cstdint>

namespace esphome {
namespace diesel_heater_ble {

// Sensor and number support an explicit unknown publication in the ESPHome
// version used by this component. Switch and climate do not, so they are not
// represented here; their local state is cleared separately on disconnect.
enum class AvailabilityEntity : uint8_t {
  RUNNING_STATE,
  ERROR_CODE,
  RUNNING_STEP,
  ALTITUDE,
  RUNNING_MODE,
  SET_LEVEL,
  SET_TEMP,
  SUPPLY_VOLTAGE,
  CASE_TEMP,
  CAB_TEMP,
  START_TIME,
  AUTO_TIME,
  RUN_TIME,
  IS_AUTO,
  LANGUAGE,
  TEMP_OFFSET,
  TANK_VOLUME,
  OIL_PUMP_TYPE,
  RF433_ON_OFF,
  TEMP_UNIT,
  ALTITUDE_UNIT,
  AUTOMATIC_HEATING,
  POWER_LEVEL_NUMBER,
  SET_TEMP_NUMBER,
  COUNT,
};

using AvailabilityMask = uint32_t;
constexpr AvailabilityMask availability_bit(AvailabilityEntity entity) {
  return AvailabilityMask{1} << static_cast<uint8_t>(entity);
}
constexpr uint8_t kAvailabilityEntityCount = static_cast<uint8_t>(AvailabilityEntity::COUNT);

class AvailabilityGeneration {
 public:
  void register_entity(AvailabilityEntity entity) { this->registered_ |= availability_bit(entity); }
  AvailabilityMask registered() const { return this->registered_; }

  // Fresh parsed state may be published only through the current generation.
  void mark_published() { this->published_ = this->registered_; }
  bool may_publish() const { return this->published_ != 0; }

  // Returns each registered entity at most once. The caller emits the returned
  // identities as unavailable, then no stale entity publication is permitted
  // until mark_published() starts a new confirmed generation.
  AvailabilityMask disconnect() {
    const AvailabilityMask pending = this->published_;
    this->published_ = 0;
    return pending;
  }

 private:
  AvailabilityMask registered_{0};
  AvailabilityMask published_{0};
};

}  // namespace diesel_heater_ble
}  // namespace esphome
