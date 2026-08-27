#include "protocol_aa.h"

#include <cmath>

namespace esphome {
namespace diesel_heater_ble {

ProtocolVariant AAProtocolDriver::detect_variant(const std::vector<uint8_t> &raw) {
  if (raw.size() < 2) return ProtocolVariant::UNKNOWN;
  if (raw[0] == 0xAA && raw[1] == 0x55) return ProtocolVariant::MD1_AA55_PLAIN;
  if (raw[0] == 0xAA && raw[1] == 0x66) return ProtocolVariant::MD3_AA66_PLAIN;
  if (raw[0] == 0xDA && raw[1] == 0x34) return ProtocolVariant::MD2_AA55_ENCRYPTED;
  if (raw[0] == 0xDA && raw[1] == 0x07) return ProtocolVariant::MD4_AA66_ENCRYPTED;
  return ProtocolVariant::UNKNOWN;
}

std::vector<uint8_t> AAProtocolDriver::decrypt(const std::vector<uint8_t> &raw) {
  std::vector<uint8_t> decrypted = raw;
  if (!is_encrypted(detect_variant(raw))) return decrypted;
  for (std::size_t index = 0; index < decrypted.size(); ++index)
    decrypted[index] ^= kXorKey[index % kXorKey.size()];
  return decrypted;
}

bool AAProtocolDriver::headers_match(const std::vector<uint8_t> &raw, const std::vector<uint8_t> &decrypted,
                                     ProtocolVariant variant) {
  if (variant == ProtocolVariant::UNKNOWN || raw.size() < 2 || decrypted.size() < 2) return false;
  const uint8_t second = expected_plain_header_2(variant);
  const uint8_t first_raw = is_encrypted(variant) ? static_cast<uint8_t>(0xAA ^ kXorKey[0]) : 0xAA;
  const uint8_t second_raw = is_encrypted(variant) ? static_cast<uint8_t>(second ^ kXorKey[1]) : second;
  return raw[0] == first_raw && raw[1] == second_raw && decrypted[0] == 0xAA && decrypted[1] == second;
}

uint8_t AAProtocolDriver::decoded_byte_(const std::vector<uint8_t> &data, std::size_t index, bool encrypted) {
  const uint8_t byte = data[index];
  return encrypted ? static_cast<uint8_t>(byte ^ kXorKey[index % kXorKey.size()]) : byte;
}

bool AAProtocolDriver::headers_match_(const std::vector<uint8_t> &raw, ProtocolVariant variant) {
  if (variant == ProtocolVariant::UNKNOWN || raw.size() < 2) return false;
  const bool encrypted = is_encrypted(variant);
  const uint8_t second = expected_plain_header_2(variant);
  const uint8_t first_raw = encrypted ? static_cast<uint8_t>(0xAA ^ kXorKey[0]) : 0xAA;
  const uint8_t second_raw = encrypted ? static_cast<uint8_t>(second ^ kXorKey[1]) : second;
  return raw[0] == first_raw && raw[1] == second_raw && decoded_byte_(raw, 0, encrypted) == 0xAA &&
         decoded_byte_(raw, 1, encrypted) == second;
}

uint16_t AAProtocolDriver::read_u16_le_(const std::vector<uint8_t> &data, std::size_t index, bool encrypted) {
  return static_cast<uint16_t>(decoded_byte_(data, index, encrypted)) |
         (static_cast<uint16_t>(decoded_byte_(data, index + 1, encrypted)) << 8U);
}
uint16_t AAProtocolDriver::read_u16_be_(const std::vector<uint8_t> &data, std::size_t index, bool encrypted) {
  return (static_cast<uint16_t>(decoded_byte_(data, index, encrypted)) << 8U) |
         static_cast<uint16_t>(decoded_byte_(data, index + 1, encrypted));
}
int16_t AAProtocolDriver::signed16_(uint16_t value) {
  return value <= 0x7FFF ? static_cast<int16_t>(value) : static_cast<int16_t>(static_cast<int32_t>(value) - 0x10000);
}
int16_t AAProtocolDriver::read_i16_le_(const std::vector<uint8_t> &data, std::size_t index, bool encrypted) {
  return signed16_(read_u16_le_(data, index, encrypted));
}
int16_t AAProtocolDriver::read_i16_be_(const std::vector<uint8_t> &data, std::size_t index, bool encrypted) {
  return signed16_(read_u16_be_(data, index, encrypted));
}
int8_t AAProtocolDriver::signed8_(uint8_t value) {
  return value <= 0x7F ? static_cast<int8_t>(value) : static_cast<int8_t>(static_cast<int16_t>(value) - 0x100);
}
void AAProtocolDriver::set_manual_level_(HeaterState &state, uint8_t level) {
  if (level >= 1 && level <= 10) state.manual_level = {level, true};
}
void AAProtocolDriver::set_celsius_target_(HeaterState &state, float temperature_c) {
  if (temperature_c >= 8.0f && temperature_c <= 36.0f) state.target_temperature_c = {temperature_c, true};
}

void AAProtocolDriver::parse_plain_fields_(const std::vector<uint8_t> &data, HeaterState &state) {
  state.altitude_m = {static_cast<float>(read_u16_le_(data, 6, false)), true};
  state.supply_voltage_v = {static_cast<float>(read_u16_le_(data, 11, false)) / 10.0f, true};
  state.case_temperature_c = {static_cast<float>(read_i16_le_(data, 13, false)), true};
  state.cabin_temperature_c = {static_cast<float>(read_i16_le_(data, 15, false)), true};
  switch (decoded_byte_(data, 8, false)) {
    case 0: if (decoded_byte_(data, 10, false) < 10) set_manual_level_(state, decoded_byte_(data, 10, false) + 1); break;
    case 1: set_manual_level_(state, decoded_byte_(data, 9, false)); break;
    case 2:
      if (decoded_byte_(data, 10, false) < 10) set_manual_level_(state, decoded_byte_(data, 10, false) + 1);
      set_celsius_target_(state, static_cast<float>(decoded_byte_(data, 9, false)));
      break;
    default: break;
  }
}

void AAProtocolDriver::parse_encrypted_fields_(const std::vector<uint8_t> &data, HeaterState &state) {
  const uint8_t temp_unit = decoded_byte_(data, 27, true);
  const uint8_t altitude_unit = decoded_byte_(data, 30, true);
  if (temp_unit == 0) {
    state.reported_temperature_unit = {TemperatureUnit::CELSIUS, true};
    set_celsius_target_(state, static_cast<float>(decoded_byte_(data, 9, true)));
    state.temperature_offset_c = {static_cast<float>(signed8_(decoded_byte_(data, 34, true))), true};
  } else if (temp_unit == 1) {
    state.reported_temperature_unit = {TemperatureUnit::FAHRENHEIT, true};
    const uint8_t temp_f = decoded_byte_(data, 9, true);
    if (temp_f >= 40 && temp_f <= 99) state.target_temperature_c = {(temp_f - 32.0f) * (5.0f / 9.0f), true};
    state.temperature_offset_c = {static_cast<float>(signed8_(decoded_byte_(data, 34, true))) * (5.0f / 9.0f), true};
  }
  const float altitude = static_cast<float>(read_u16_be_(data, 6, true)) / 10.0f;
  if (altitude_unit == 0) {
    state.reported_altitude_unit = {AltitudeUnit::METRES, true}; state.altitude_m = {altitude, true};
  } else if (altitude_unit == 1) {
    state.reported_altitude_unit = {AltitudeUnit::IMPERIAL, true}; state.altitude_m = {altitude * 0.3048f, true};
  }
  set_manual_level_(state, decoded_byte_(data, 10, true));
  state.supply_voltage_v = {static_cast<float>(read_u16_be_(data, 11, true)) / 10.0f, true};
  state.case_temperature_c = {static_cast<float>(read_i16_be_(data, 13, true)), true};
  state.cabin_temperature_c = {static_cast<float>(read_i16_be_(data, 32, true)) / 10.0f, true};
  state.start_time = {read_u16_be_(data, 19, true), TimeValueUnit::PROTOCOL_TICKS, true};
  state.auto_time = {read_u16_be_(data, 21, true), TimeValueUnit::PROTOCOL_TICKS, true};
  state.run_time = {read_u16_be_(data, 23, true), TimeValueUnit::PROTOCOL_TICKS, true};
  const uint8_t auto_start_stop = decoded_byte_(data, 25, true);
  if (auto_start_stop <= 1) state.automatic_start_stop_enabled = {auto_start_stop == 1, true};
  const uint8_t language = decoded_byte_(data, 26, true);
  if (language <= 5) state.language = {language, true};
  state.tank_volume = {decoded_byte_(data, 28, true), true};
  const uint8_t pump = decoded_byte_(data, 29, true);
  state.oil_pump_type = {pump, true};
  if (pump == 20) state.rf433_enabled = {false, true};
  else if (pump == 21) state.rf433_enabled = {true, true};
  const uint8_t automatic_heating = decoded_byte_(data, 31, true);
  if (automatic_heating <= 1) state.automatic_heating_enabled = {automatic_heating == 1, true};
}

ParseResult AAProtocolDriver::parse(const std::vector<uint8_t> &raw, HeaterState &state) const {
  const ProtocolVariant detected = detect_variant(raw);
  if (detected == ProtocolVariant::UNKNOWN) {
    if (raw.size() < 2) return ParseResult::INCOMPLETE;
    return (raw[0] == 0xAA || raw[0] == 0xDA) ? ParseResult::INVALID_FRAME : ParseResult::UNKNOWN_PROTOCOL;
  }
  if (detected != this->variant_) return ParseResult::UNKNOWN_PROTOCOL;
  if (raw.size() < minimum_safe_read_span(detected) || raw.size() < documented_complete_span(detected))
    return ParseResult::INCOMPLETE;
  if (!headers_match_(raw, detected)) return ParseResult::INVALID_FRAME;

  HeaterState parsed{};
  parsed.valid = true;
  parsed.variant = detected;
  const bool encrypted = is_encrypted(detected);
  parsed.acknowledged_command = {decoded_byte_(raw, 2, encrypted), true};
  parsed.raw_running_state = {decoded_byte_(raw, 3, encrypted), true};
  parsed.power = {power_from_raw_running_state(parsed.raw_running_state.value), true};
  parsed.raw_running_step = {decoded_byte_(raw, 5, encrypted), true};
  parsed.legacy_ui_heating = {legacy_ui_heating_from_raw_running_step(parsed.raw_running_step.value), true};
  parsed.raw_operating_mode = {decoded_byte_(raw, 8, encrypted), true};
  const OperatingMode mode = operating_mode_from_raw(parsed.raw_operating_mode.value);
  if (is_normalized_operating_mode(mode)) parsed.operating_mode = {mode, true};
  if (detected == ProtocolVariant::MD1_AA55_PLAIN || detected == ProtocolVariant::MD2_AA55_ENCRYPTED)
    parsed.error_code = {decoded_byte_(raw, 4, encrypted), true};
  else if (detected == ProtocolVariant::MD3_AA66_PLAIN)
    parsed.error_code = {decoded_byte_(raw, 17, encrypted), true};
  else
    parsed.error_code = {decoded_byte_(raw, 35, encrypted), true};
  if (encrypted) parse_encrypted_fields_(raw, parsed); else parse_plain_fields_(raw, parsed);
  state = parsed;
  return ParseResult::OK;
}

CommandFrame AAProtocolDriver::encode(uint16_t passkey, AACommand command, int16_t value) {
  const uint16_t raw_value = static_cast<uint16_t>(value);
  CommandFrame frame{{0xAA, 0x55, static_cast<uint8_t>(passkey / 100), static_cast<uint8_t>(passkey % 100),
                      static_cast<uint8_t>(command), static_cast<uint8_t>(raw_value & 0xFF),
                      static_cast<uint8_t>(raw_value >> 8), 0}};
  uint8_t checksum = 0;
  for (std::size_t index = 2; index <= 6; ++index) checksum = static_cast<uint8_t>(checksum + frame[index]);
  frame[7] = checksum;
  return frame;
}

CommandBatch AAProtocolDriver::command_(uint16_t passkey, AACommand command, int16_t value) {
  if (!valid_passkey(passkey)) return CommandBatch::with_result(CommandResult::INVALID_VALUE);
  CommandBatch batch = CommandBatch::with_result(CommandResult::READY);
  batch.append(encode(passkey, command, value));
  return batch;
}
CommandBatch AAProtocolDriver::status(uint16_t passkey) const { return command_(passkey, AACommand::STATUS, 0); }
CommandBatch AAProtocolDriver::clock(uint16_t passkey, uint16_t minutes_since_midnight) const {
  if (minutes_since_midnight >= 24U * 60U) return CommandBatch::with_result(CommandResult::INVALID_VALUE);
  return command_(passkey, AACommand::CLOCK, static_cast<int16_t>(minutes_since_midnight));
}
CommandBatch AAProtocolDriver::power(const HeaterState &state, uint16_t passkey, bool enabled) const {
  if (!state.valid || !state.power.valid) return CommandBatch::with_result(CommandResult::STATE_UNAVAILABLE);
  if (state.power.value == enabled) return CommandBatch::with_result(CommandResult::NO_CHANGE);
  return command_(passkey, AACommand::POWER, enabled ? 1 : 0);
}

CommandBatch AAProtocolDriver::mode_then_value_(const HeaterState &state, uint16_t passkey, OperatingMode required_mode,
                                                 AACommand command, int16_t value, bool value_matches) {
  if (!state.valid) return CommandBatch::with_result(CommandResult::STATE_UNAVAILABLE);
  if (!valid_passkey(passkey)) return CommandBatch::with_result(CommandResult::INVALID_VALUE);
  const bool mode_matches = state.operating_mode.valid && state.operating_mode.value == required_mode;
  if (mode_matches && value_matches) return CommandBatch::with_result(CommandResult::NO_CHANGE);
  CommandBatch batch = CommandBatch::with_result(CommandResult::READY);
  if (!mode_matches) batch.append(encode(passkey, AACommand::MODE, required_mode == OperatingMode::MANUAL_LEVEL ? 1 : 2));
  if (!value_matches || !mode_matches) batch.append(encode(passkey, command, value));
  return batch;
}
CommandBatch AAProtocolDriver::manual_level(const HeaterState &state, uint16_t passkey, uint8_t level) const {
  if (level < 1 || level > 10) return CommandBatch::with_result(CommandResult::INVALID_VALUE);
  const bool matches = state.manual_level.valid && state.manual_level.value == level;
  return mode_then_value_(state, passkey, OperatingMode::MANUAL_LEVEL, AACommand::VALUE, level, matches);
}
CommandBatch AAProtocolDriver::temperature_c(const HeaterState &state, uint16_t passkey, float temperature_c) const {
  if (!std::isfinite(temperature_c) || temperature_c < 8.0f || temperature_c > 36.0f)
    return CommandBatch::with_result(CommandResult::INVALID_VALUE);
  int16_t wire_temperature = static_cast<int16_t>(std::lround(temperature_c));
  if (state.reported_temperature_unit.valid && state.reported_temperature_unit.value == TemperatureUnit::FAHRENHEIT)
    wire_temperature = static_cast<int16_t>(std::lround(temperature_c * 9.0f / 5.0f + 32.0f));
  if (wire_temperature < 0 || wire_temperature > 255) return CommandBatch::with_result(CommandResult::INVALID_VALUE);
  const bool matches = state.target_temperature_c.valid && std::fabs(state.target_temperature_c.value - temperature_c) < 0.01f;
  return mode_then_value_(state, passkey, OperatingMode::THERMOSTAT, AACommand::VALUE, wire_temperature, matches);
}

CommandBatch AAProtocolDriver::temperature_offset_c(const HeaterState &state, uint16_t passkey,
                                                     float temperature_offset_c) const {
  // App-derived parameter UI: the raw control is an integer slider from -9
  // through 9. The entity policy supplies the smaller, unit-safe normalized
  // Celsius interval; retain this boundary check for non-entity callers.
  if (!has_capability(this->capabilities(), Capability::EXTENDED_SETTINGS))
    return CommandBatch::with_result(CommandResult::UNSUPPORTED);
  if (!state.valid || !state.reported_temperature_unit.valid)
    return CommandBatch::with_result(CommandResult::STATE_UNAVAILABLE);
  if (!std::isfinite(temperature_offset_c) || temperature_offset_c < -5.0f || temperature_offset_c > 5.0f ||
      std::floor(temperature_offset_c) != temperature_offset_c)
    return CommandBatch::with_result(CommandResult::INVALID_VALUE);

  int16_t wire_offset = static_cast<int16_t>(temperature_offset_c);
  if (state.reported_temperature_unit.value == TemperatureUnit::FAHRENHEIT)
    wire_offset = static_cast<int16_t>(std::lround(temperature_offset_c * 9.0f / 5.0f));
  if (wire_offset < -9 || wire_offset > 9) return CommandBatch::with_result(CommandResult::INVALID_VALUE);

  const bool matches = state.temperature_offset_c.valid &&
                       std::fabs(state.temperature_offset_c.value - temperature_offset_c) < 0.01f;
  if (matches) return CommandBatch::with_result(CommandResult::NO_CHANGE);
  return command_(passkey, AACommand::TEMPERATURE_OFFSET, wire_offset);
}

}  // namespace diesel_heater_ble
}  // namespace esphome
