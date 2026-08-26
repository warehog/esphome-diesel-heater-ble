#include <cassert>
#include <cstdint>
#include <type_traits>

#include "contracts.h"

using esphome::diesel_heater_ble::Capability;
using esphome::diesel_heater_ble::CommandBatch;
using esphome::diesel_heater_ble::CommandFrame;
using esphome::diesel_heater_ble::CommandResult;
using esphome::diesel_heater_ble::NormalizedHeaterState;
using esphome::diesel_heater_ble::OperatingMode;
using esphome::diesel_heater_ble::ProtocolVariant;
using esphome::diesel_heater_ble::TimeValueUnit;
using esphome::diesel_heater_ble::capabilities_for;
using esphome::diesel_heater_ble::has_capability;
using esphome::diesel_heater_ble::is_normalized_operating_mode;
using esphome::diesel_heater_ble::kCommandBatchCapacity;
using esphome::diesel_heater_ble::kCommandFrameSize;
using esphome::diesel_heater_ble::operating_mode_from_raw;
using esphome::diesel_heater_ble::power_from_raw_running_state;

static_assert(sizeof(CommandFrame) == kCommandFrameSize, "command frame capacity changed");
static_assert(sizeof(CommandBatch) <= 32, "command batch is no longer a small stack value");
static_assert(std::is_trivially_copyable<CommandBatch>::value, "batch must own fixed storage");
static_assert(std::is_trivially_copyable<NormalizedHeaterState>::value, "state must be value-owned");
static_assert(std::is_same<decltype(NormalizedHeaterState::temperature_offset_c.value), float>::value,
              "temperature offset must preserve fractional Celsius values");

int main() {
  const NormalizedHeaterState initial{};
  assert(!initial.valid);
  assert(initial.variant == ProtocolVariant::UNKNOWN);
  assert(!initial.power.valid);
  assert(!initial.operating_mode.valid);
  assert(!initial.manual_level.valid);
  assert(!initial.target_temperature_c.valid);
  assert(!initial.error_code.valid);
  assert(!initial.case_temperature_c.valid);
  assert(!initial.cabin_temperature_c.valid);
  assert(!initial.supply_voltage_v.valid);
  assert(!initial.altitude_m.valid);
  assert(!initial.temperature_offset_c.valid);
  assert(initial.temperature_offset_c.value == 0.0f);
  assert(!initial.start_time.valid);
  assert(initial.start_time.unit == TimeValueUnit::PROTOCOL_TICKS);
  assert(!initial.auto_time.valid);
  assert(!initial.run_time.valid);
  assert(!initial.automatic_start_stop_enabled.valid);
  assert(!initial.language.valid);
  assert(!initial.tank_volume.valid);
  assert(!initial.oil_pump_type.valid);
  assert(!initial.rf433_enabled.valid);
  assert(!initial.reported_temperature_unit.valid);
  assert(!initial.reported_altitude_unit.valid);
  assert(!initial.automatic_heating_enabled.valid);
  assert(!initial.acknowledged_command.valid);
  assert(!initial.raw_running_state.valid);
  assert(!initial.raw_operating_mode.valid);
  assert(!initial.raw_running_step.valid);

  assert(!power_from_raw_running_state(0));
  assert(!power_from_raw_running_state(3));
  assert(power_from_raw_running_state(1));
  assert(power_from_raw_running_state(2));
  assert(power_from_raw_running_state(4));
  assert(power_from_raw_running_state(0xff));
  assert(operating_mode_from_raw(1) == OperatingMode::MANUAL_LEVEL);
  assert(operating_mode_from_raw(2) == OperatingMode::THERMOSTAT);
  assert(operating_mode_from_raw(0) == OperatingMode::UNKNOWN);
  assert(!is_normalized_operating_mode(OperatingMode::UNKNOWN));

  NormalizedHeaterState state{};
  state.valid = true;
  state.manual_level = {5, true};
  state.error_code = {42, true};
  state.temperature_offset_c = {-2.5f, true};
  state.start_time = {123, TimeValueUnit::MINUTES_SINCE_MIDNIGHT, true};
  state.auto_time = {45, TimeValueUnit::MINUTES_DURATION, true};
  state.run_time = {60, TimeValueUnit::MINUTES_DURATION, true};
  state.automatic_start_stop_enabled = {true, true};
  state.language = {2, true};
  state.tank_volume = {10, true};
  state.oil_pump_type = {4, true};
  state.rf433_enabled = {true, true};
  state.reported_temperature_unit = {esphome::diesel_heater_ble::TemperatureUnit::FAHRENHEIT, true};
  state.reported_altitude_unit = {esphome::diesel_heater_ble::AltitudeUnit::IMPERIAL, true};
  state.automatic_heating_enabled = {true, true};
  state.acknowledged_command = {1, true};
  state.raw_running_state = {2, true};
  state.raw_operating_mode = {1, true};
  state.raw_running_step = {3, true};
  state.invalidate();
  assert(!state.valid);
  assert(!state.manual_level.valid);
  assert(state.manual_level.value == 0);
  assert(!state.error_code.valid);
  assert(!state.temperature_offset_c.valid);
  assert(state.temperature_offset_c.value == 0.0f);
  assert(!state.start_time.valid);
  assert(state.start_time.value == 0);
  assert(state.start_time.unit == TimeValueUnit::PROTOCOL_TICKS);
  assert(!state.auto_time.valid);
  assert(!state.run_time.valid);
  assert(!state.automatic_start_stop_enabled.valid);
  assert(!state.language.valid);
  assert(!state.tank_volume.valid);
  assert(!state.oil_pump_type.valid);
  assert(!state.rf433_enabled.valid);
  assert(!state.reported_temperature_unit.valid);
  assert(!state.reported_altitude_unit.valid);
  assert(!state.automatic_heating_enabled.valid);
  assert(!state.acknowledged_command.valid);
  assert(!state.raw_running_state.valid);
  assert(!state.raw_operating_mode.valid);
  assert(!state.raw_running_step.valid);

  for (const ProtocolVariant variant : {ProtocolVariant::MD1_AA55_PLAIN, ProtocolVariant::MD2_AA55_ENCRYPTED,
                                        ProtocolVariant::MD3_AA66_PLAIN, ProtocolVariant::MD4_AA66_ENCRYPTED}) {
    const auto capabilities = capabilities_for(variant);
    assert(has_capability(capabilities, Capability::STATUS));
    assert(has_capability(capabilities, Capability::POWER));
    assert(has_capability(capabilities, Capability::MODE));
    assert(has_capability(capabilities, Capability::MANUAL_LEVEL));
    assert(has_capability(capabilities, Capability::SET_TEMPERATURE));
    assert(has_capability(capabilities, Capability::CLOCK));
  }
  assert(!has_capability(capabilities_for(ProtocolVariant::MD1_AA55_PLAIN), Capability::EXTENDED_SETTINGS));
  assert(has_capability(capabilities_for(ProtocolVariant::MD2_AA55_ENCRYPTED), Capability::EXTENDED_SETTINGS));
  assert(!has_capability(capabilities_for(ProtocolVariant::MD3_AA66_PLAIN), Capability::EXTENDED_SETTINGS));
  assert(has_capability(capabilities_for(ProtocolVariant::MD4_AA66_ENCRYPTED), Capability::EXTENDED_SETTINGS));
  assert(capabilities_for(ProtocolVariant::UNKNOWN) == 0);

  CommandBatch batch{};
  assert(batch.result() == CommandResult::STATE_UNAVAILABLE);
  assert(batch.empty());
  assert(!batch.ready());
  assert(!batch.sendable());
  assert(!batch.append(CommandFrame{}));

  CommandBatch ready_empty = CommandBatch::with_result(CommandResult::READY);
  assert(ready_empty.ready());
  assert(ready_empty.empty());
  assert(!ready_empty.sendable());
  for (std::size_t index = 0; index < kCommandBatchCapacity; ++index) {
    CommandFrame frame{};
    frame[0] = static_cast<uint8_t>(index);
    assert(ready_empty.append(frame));
  }
  assert(!ready_empty.empty());
  assert(ready_empty.sendable());
  assert(ready_empty.size() == kCommandBatchCapacity);
  assert(!ready_empty.append(CommandFrame{}));
  assert(ready_empty.frame_at(0) != nullptr);
  assert(ready_empty.frame_at(1) != nullptr);
  assert(ready_empty.frame_at(0)->at(0) == 0);
  assert(ready_empty.frame_at(1)->at(0) == 1);
  assert(ready_empty.frame_at(kCommandBatchCapacity) == nullptr);
  return 0;
}
