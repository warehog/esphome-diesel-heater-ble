#include <cassert>
#include <cstdint>
#include <array>

#include "protocol_aa.h"

using esphome::diesel_heater_ble::AACommand;
using esphome::diesel_heater_ble::AAProtocolDriver;
using esphome::diesel_heater_ble::CommandFrame;
using esphome::diesel_heater_ble::CommandResult;
using esphome::diesel_heater_ble::HeaterState;
using esphome::diesel_heater_ble::OperatingMode;
using esphome::diesel_heater_ble::ProtocolRegistry;
using esphome::diesel_heater_ble::ProtocolVariant;
using esphome::diesel_heater_ble::TemperatureUnit;

int main() {
  const CommandFrame expected_status{{0xAA, 0x55, 0x0C, 0x22, 0x01, 0x00, 0x00, 0x2F}};
  assert(AAProtocolDriver::encode(1234, AACommand::STATUS, 0) == expected_status);
  assert((AAProtocolDriver::encode(1, AACommand::POWER, 1) ==
          CommandFrame{{0xAA, 0x55, 0x00, 0x01, 0x03, 0x01, 0x00, 0x05}}));
  assert((AAProtocolDriver::encode(9999, AACommand::VALUE, 255) ==
          CommandFrame{{0xAA, 0x55, 0x63, 0x63, 0x04, 0xFF, 0x00, 0xC9}}));
  assert((AAProtocolDriver::encode(1234, AACommand::CLOCK, 1439) ==
          CommandFrame{{0xAA, 0x55, 0x0C, 0x22, 0x0A, 0x9F, 0x05, 0xDC}}));
  assert((AAProtocolDriver::encode(1234, AACommand::TEMPERATURE_OFFSET, -2) ==
          CommandFrame{{0xAA, 0x55, 0x0C, 0x22, 0x14, 0xFE, 0xFF, 0x3F}}));
  assert((AAProtocolDriver::encode(9999, AACommand::TEMPERATURE_OFFSET, 256) ==
          CommandFrame{{0xAA, 0x55, 0x63, 0x63, 0x14, 0x00, 0x01, 0xDB}}));
  for (uint8_t command = 1; command <= 4; ++command)
    assert(AAProtocolDriver::encode(1234, static_cast<AACommand>(command), 0)[4] == command);
  for (uint8_t command = 10; command <= 20; ++command)
    assert(AAProtocolDriver::encode(1234, static_cast<AACommand>(command), 0)[4] == command);
  assert(AAProtocolDriver::encode(1234, AACommand::AUTOMATIC_START_TIME, 0)[4] == 11);
  assert(AAProtocolDriver::encode(1234, AACommand::AUTOMATIC_RUN_TIME, 0)[4] == 12);
  assert(AAProtocolDriver::encode(1234, AACommand::AUTOMATIC_ENABLED, 0)[4] == 13);
  assert(AAProtocolDriver::encode(1234, AACommand::LANGUAGE, 0)[4] == 14);
  assert(AAProtocolDriver::encode(1234, AACommand::TEMPERATURE_UNIT, 0)[4] == 15);
  assert(AAProtocolDriver::encode(1234, AACommand::TANK_VOLUME, 0)[4] == 16);
  assert(AAProtocolDriver::encode(1234, AACommand::OIL_PUMP_OR_RF433, 0)[4] == 17);
  assert(AAProtocolDriver::encode(1234, AACommand::AUTOMATIC_HEATING, 0)[4] == 18);
  assert(AAProtocolDriver::encode(1234, AACommand::ALTITUDE_UNIT, 0)[4] == 19);
  assert(AAProtocolDriver::encode(1234, AACommand::TEMPERATURE_OFFSET, 0)[4] == 20);
  const CommandFrame wrapped = AAProtocolDriver::encode(9999, static_cast<AACommand>(255), -1);
  assert(wrapped[7] == static_cast<uint8_t>(0x63 + 0x63 + 0xFF + 0xFF + 0xFF));

  const auto *driver = ProtocolRegistry::find(ProtocolVariant::MD2_AA55_ENCRYPTED);
  assert(driver != nullptr);
  HeaterState state{};
  assert(driver->power(state, 1234, true).result() == CommandResult::STATE_UNAVAILABLE);
  state.valid = true;
  state.operating_mode = {OperatingMode::THERMOSTAT, true};
  state.target_temperature_c = {20.0f, true};
  state.reported_temperature_unit = {TemperatureUnit::CELSIUS, true};
  auto celsius = driver->temperature_c(state, 1234, 21.0f);
  assert(celsius.sendable() && celsius.size() == 1 && celsius.frame_at(0)->at(5) == 21);
  state.reported_temperature_unit = {TemperatureUnit::FAHRENHEIT, true};
  state.target_temperature_c = {21.0f, true};
  auto fahrenheit = driver->temperature_c(state, 1234, 20.0f);
  assert(fahrenheit.sendable() && fahrenheit.size() == 1 && fahrenheit.frame_at(0)->at(5) == 68);
  state.target_temperature_c = {20.0f, true};
  auto fahrenheit_low = driver->temperature_c(state, 1234, 8.0f);
  assert(fahrenheit_low.sendable() && fahrenheit_low.frame_at(0)->at(5) == 46);
  auto fahrenheit_high = driver->temperature_c(state, 1234, 36.0f);
  assert(fahrenheit_high.sendable() && fahrenheit_high.frame_at(0)->at(5) == 97);
  assert(driver->temperature_c(state, 1234, 7.9f).result() == CommandResult::INVALID_VALUE);
  assert(driver->temperature_c(state, 1234, 36.1f).result() == CommandResult::INVALID_VALUE);
  state.operating_mode = {OperatingMode::MANUAL_LEVEL, true};
  auto mode_then_temperature = driver->temperature_c(state, 1234, 20.0f);
  assert(mode_then_temperature.sendable() && mode_then_temperature.size() == 2);
  assert(mode_then_temperature.frame_at(0)->at(4) == 2 && mode_then_temperature.frame_at(0)->at(5) == 2);
  assert(mode_then_temperature.frame_at(1)->at(4) == 4 && mode_then_temperature.frame_at(1)->at(5) == 68);
  return 0;
}
