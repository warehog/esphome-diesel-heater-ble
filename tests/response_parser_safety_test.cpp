#include <cassert>
#include <cmath>
#include <cstdint>
#include <vector>

#include "messages.h"

using esphome::diesel_heater_ble::AltitudeUnit;
using esphome::diesel_heater_ble::HeaterState;
using esphome::diesel_heater_ble::OperatingMode;
using esphome::diesel_heater_ble::ParseResult;
using esphome::diesel_heater_ble::ProtocolVariant;
using esphome::diesel_heater_ble::ResponseParser;
using esphome::diesel_heater_ble::TemperatureUnit;

namespace {

// Every response vector in this file is synthetic. It is constructed from the
// app/docs field map to exercise parser bounds and normalization; it is not a
// captured wire frame and its length is not evidence of a wire-frame boundary.
std::vector<uint8_t> plain_frame(ProtocolVariant variant) {
  const std::size_t size = ResponseParser::documented_complete_span(variant);
  std::vector<uint8_t> data(size, 0);
  data[0] = 0xAA;
  data[1] = variant == ProtocolVariant::MD3_AA66_PLAIN ? 0x66 : 0x55;
  data[2] = 4;
  data[3] = 2;
  data[4] = 9;
  data[5] = 3;
  data[6] = 0xD2;
  data[7] = 0x04;  // 1234 metres, little endian.
  data[8] = 2;
  data[9] = 24;
  data[10] = 4;
  data[11] = 0x7E;
  data[12] = 0x00;  // 12.6 V, little endian.
  data[13] = 0x00;
  data[14] = 0x80;  // -32768 C, little endian.
  data[15] = 0xFF;
  data[16] = 0x7F;  // 32767 C, little endian.
  if (variant == ProtocolVariant::MD3_AA66_PLAIN) {
    data[17] = 7;
  }
  return data;
}

std::vector<uint8_t> xor_crypt(std::vector<uint8_t> data) {
  constexpr uint8_t key[] = {'p', 'a', 's', 's', 'w', 'o', 'r', 'd'};
  for (std::size_t index = 0; index < data.size(); ++index) {
    data[index] ^= key[index % (sizeof(key) / sizeof(key[0]))];
  }
  return data;
}

std::vector<uint8_t> encrypted_frame(ProtocolVariant variant) {
  const std::size_t size = ResponseParser::documented_complete_span(variant);
  std::vector<uint8_t> data(size, 0);
  data[0] = 0xAA;
  data[1] = variant == ProtocolVariant::MD4_AA66_ENCRYPTED ? 0x66 : 0x55;
  data[2] = 3;
  data[3] = 3;  // App-derived off state.
  data[4] = 8;
  data[5] = 4;
  data[6] = 0x01;
  data[7] = 0x2C;  // 30.0 metres, big endian tenths.
  data[8] = 1;
  data[9] = 68;    // Fahrenheit -> 20 C.
  data[10] = 5;
  data[11] = 0x00;
  data[12] = 0x7D;  // 12.5 V, big endian.
  data[13] = 0x80;
  data[14] = 0x00;  // -32768 C, big endian.
  data[19] = 0x01;
  data[20] = 0x02;
  data[21] = 0x03;
  data[22] = 0x04;
  data[23] = 0x05;
  data[24] = 0x06;
  data[25] = 1;
  data[26] = 5;
  data[27] = 1;
  data[28] = 10;
  data[29] = 21;
  data[30] = 0;
  data[31] = 1;
  data[32] = 0xFF;
  data[33] = 0x85;  // -12.3 C, big endian tenths.
  data[34] = 0xFE;  // -2 F delta -> -1.111... C.
  if (variant == ProtocolVariant::MD4_AA66_ENCRYPTED) {
    data[35] = 6;
  }
  return xor_crypt(data);
}

HeaterState sentinel_state() {
  HeaterState state{};
  state.valid = true;
  state.variant = ProtocolVariant::MD3_AA66_PLAIN;
  state.power = {true, true};
  state.target_temperature_c = {21.0f, true};
  state.case_temperature_c = {-5.0f, true};
  return state;
}

template<typename T> bool fields_equal(const esphome::diesel_heater_ble::StateField<T> &lhs,
                                       const esphome::diesel_heater_ble::StateField<T> &rhs) {
  return lhs.value == rhs.value && lhs.valid == rhs.valid;
}

bool timed_fields_equal(const esphome::diesel_heater_ble::TimedField &lhs,
                        const esphome::diesel_heater_ble::TimedField &rhs) {
  return lhs.value == rhs.value && lhs.unit == rhs.unit && lhs.valid == rhs.valid;
}

bool states_equal(const HeaterState &lhs, const HeaterState &rhs) {
  return lhs.valid == rhs.valid && lhs.variant == rhs.variant && fields_equal(lhs.power, rhs.power) &&
         fields_equal(lhs.operating_mode, rhs.operating_mode) && fields_equal(lhs.manual_level, rhs.manual_level) &&
         fields_equal(lhs.target_temperature_c, rhs.target_temperature_c) &&
         fields_equal(lhs.legacy_ui_heating, rhs.legacy_ui_heating) &&
         fields_equal(lhs.error_code, rhs.error_code) && fields_equal(lhs.case_temperature_c, rhs.case_temperature_c) &&
         fields_equal(lhs.cabin_temperature_c, rhs.cabin_temperature_c) &&
         fields_equal(lhs.supply_voltage_v, rhs.supply_voltage_v) && fields_equal(lhs.altitude_m, rhs.altitude_m) &&
         fields_equal(lhs.temperature_offset_c, rhs.temperature_offset_c) && timed_fields_equal(lhs.start_time, rhs.start_time) &&
         timed_fields_equal(lhs.auto_time, rhs.auto_time) && timed_fields_equal(lhs.run_time, rhs.run_time) &&
         fields_equal(lhs.automatic_start_stop_enabled, rhs.automatic_start_stop_enabled) &&
         fields_equal(lhs.language, rhs.language) && fields_equal(lhs.tank_volume, rhs.tank_volume) &&
         fields_equal(lhs.oil_pump_type, rhs.oil_pump_type) && fields_equal(lhs.rf433_enabled, rhs.rf433_enabled) &&
         fields_equal(lhs.reported_temperature_unit, rhs.reported_temperature_unit) &&
         fields_equal(lhs.reported_altitude_unit, rhs.reported_altitude_unit) &&
         fields_equal(lhs.automatic_heating_enabled, rhs.automatic_heating_enabled) &&
         fields_equal(lhs.acknowledged_command, rhs.acknowledged_command) &&
         fields_equal(lhs.raw_running_state, rhs.raw_running_state) &&
         fields_equal(lhs.raw_operating_mode, rhs.raw_operating_mode) &&
         fields_equal(lhs.raw_running_step, rhs.raw_running_step);
}

void assert_parse_failure_preserves_state(const std::vector<uint8_t> &raw, ParseResult expected_result) {
  HeaterState state = sentinel_state();
  const HeaterState before = state;
  assert(ResponseParser::parse(raw, state) == expected_result);
  assert(states_equal(state, before));
}

void test_all_short_prefixes_preserve_state() {
  for (const ProtocolVariant variant : {ProtocolVariant::MD1_AA55_PLAIN, ProtocolVariant::MD2_AA55_ENCRYPTED,
                                        ProtocolVariant::MD3_AA66_PLAIN, ProtocolVariant::MD4_AA66_ENCRYPTED}) {
    const std::vector<uint8_t> full = ResponseParser::is_encrypted(variant) ? encrypted_frame(variant) : plain_frame(variant);
    const std::size_t safe_span = ResponseParser::minimum_safe_read_span(variant);
    const std::size_t complete_span = ResponseParser::documented_complete_span(variant);
    assert(safe_span < complete_span);
    assert(full.size() == complete_span);
    for (std::size_t prefix_size = 0; prefix_size < complete_span; ++prefix_size) {
      const std::vector<uint8_t> prefix(full.begin(), full.begin() + prefix_size);
      assert_parse_failure_preserves_state(prefix, ParseResult::INCOMPLETE);
    }
  }
}

void test_header_validation_and_decryption() {
  assert_parse_failure_preserves_state({0xAA, 0x00}, ParseResult::INVALID_FRAME);
  assert_parse_failure_preserves_state({0xDA, 0x55}, ParseResult::INVALID_FRAME);
  assert_parse_failure_preserves_state({0xDA, 0x35}, ParseResult::INVALID_FRAME);
  assert_parse_failure_preserves_state({0x00, 0x55}, ParseResult::UNKNOWN_PROTOCOL);

  const std::vector<uint8_t> encrypted = encrypted_frame(ProtocolVariant::MD2_AA55_ENCRYPTED);
  const std::vector<uint8_t> decrypted = ResponseParser::decrypt(encrypted);
  assert(decrypted.size() == encrypted.size());
  assert(ResponseParser::headers_match(encrypted, decrypted, ProtocolVariant::MD2_AA55_ENCRYPTED));
  assert(!ResponseParser::headers_match(encrypted, decrypted, ProtocolVariant::MD4_AA66_ENCRYPTED));

  std::vector<uint8_t> wrong_decrypted = decrypted;
  wrong_decrypted[1] = 0x66;
  assert(!ResponseParser::headers_match(encrypted, wrong_decrypted, ProtocolVariant::MD2_AA55_ENCRYPTED));

  const std::vector<uint8_t> short_encrypted{0xDA, 0x34, 0x42};
  assert(ResponseParser::decrypt(short_encrypted).size() == short_encrypted.size());
}

void test_full_length_malformed_frames_preserve_state() {
  const std::vector<uint8_t> plain = plain_frame(ProtocolVariant::MD1_AA55_PLAIN);
  std::vector<uint8_t> plain_wrong_first_header = plain;
  plain_wrong_first_header[0] = 0xAB;
  assert_parse_failure_preserves_state(plain_wrong_first_header, ParseResult::UNKNOWN_PROTOCOL);
  std::vector<uint8_t> plain_wrong_second_header = plain;
  plain_wrong_second_header[1] = 0x00;
  assert_parse_failure_preserves_state(plain_wrong_second_header, ParseResult::INVALID_FRAME);

  const std::vector<uint8_t> encrypted = encrypted_frame(ProtocolVariant::MD2_AA55_ENCRYPTED);
  std::vector<uint8_t> encrypted_wrong_first_header = encrypted;
  encrypted_wrong_first_header[0] = 0xDB;
  assert_parse_failure_preserves_state(encrypted_wrong_first_header, ParseResult::UNKNOWN_PROTOCOL);
  std::vector<uint8_t> encrypted_wrong_second_header = encrypted;
  encrypted_wrong_second_header[1] = 0x35;  // Not the documented encrypted AA55 header byte 0x34.
  assert_parse_failure_preserves_state(encrypted_wrong_second_header, ParseResult::INVALID_FRAME);
}

void test_documented_completeness_policy() {
  assert(ResponseParser::minimum_safe_read_span(ProtocolVariant::MD1_AA55_PLAIN) == 17);
  assert(ResponseParser::minimum_safe_read_span(ProtocolVariant::MD3_AA66_PLAIN) == 18);
  assert(ResponseParser::minimum_safe_read_span(ProtocolVariant::MD2_AA55_ENCRYPTED) == 35);
  assert(ResponseParser::minimum_safe_read_span(ProtocolVariant::MD4_AA66_ENCRYPTED) == 36);
  assert(ResponseParser::documented_complete_span(ProtocolVariant::MD1_AA55_PLAIN) == 20);
  assert(ResponseParser::documented_complete_span(ProtocolVariant::MD3_AA66_PLAIN) == 20);
  assert(ResponseParser::documented_complete_span(ProtocolVariant::MD2_AA55_ENCRYPTED) == 48);
  assert(ResponseParser::documented_complete_span(ProtocolVariant::MD4_AA66_ENCRYPTED) == 48);
}

void test_plain_normalization_and_invalidation() {
  HeaterState state{};
  std::vector<uint8_t> plain = plain_frame(ProtocolVariant::MD1_AA55_PLAIN);
  assert(ResponseParser::parse(plain, state) == ParseResult::OK);
  assert(state.valid);
  assert(state.variant == ProtocolVariant::MD1_AA55_PLAIN);
  assert(state.acknowledged_command.valid && state.acknowledged_command.value == 4);
  assert(state.power.valid && state.power.value);
  assert(state.legacy_ui_heating.valid && state.legacy_ui_heating.value);
  assert(state.operating_mode.valid && state.operating_mode.value == OperatingMode::THERMOSTAT);
  assert(state.manual_level.valid && state.manual_level.value == 5);
  assert(state.target_temperature_c.valid && state.target_temperature_c.value == 24.0f);
  assert(state.altitude_m.valid && state.altitude_m.value == 1234.0f);
  assert(state.supply_voltage_v.valid && state.supply_voltage_v.value == 12.6f);
  assert(state.case_temperature_c.valid && state.case_temperature_c.value == -32768.0f);
  assert(state.cabin_temperature_c.valid && state.cabin_temperature_c.value == 32767.0f);
  assert(!state.reported_temperature_unit.valid);

  // A non-temperature plain mode has no target-temperature field. The parser
  // starts a fresh state, so a previous thermostat target cannot survive.
  plain[8] = 1;
  plain[9] = 6;
  assert(ResponseParser::parse(plain, state) == ParseResult::OK);
  assert(state.operating_mode.valid && state.operating_mode.value == OperatingMode::MANUAL_LEVEL);
  assert(state.manual_level.valid && state.manual_level.value == 6);
  assert(!state.target_temperature_c.valid);

  plain[8] = 0;
  plain[10] = 9;
  assert(ResponseParser::parse(plain, state) == ParseResult::OK);
  assert(!state.operating_mode.valid);
  assert(state.manual_level.valid && state.manual_level.value == 10);
  assert(!state.target_temperature_c.valid);
}

void test_encrypted_normalization_and_validity() {
  HeaterState state{};
  std::vector<uint8_t> encrypted = encrypted_frame(ProtocolVariant::MD2_AA55_ENCRYPTED);
  assert(ResponseParser::parse(encrypted, state) == ParseResult::OK);
  assert(state.valid);
  assert(state.variant == ProtocolVariant::MD2_AA55_ENCRYPTED);
  assert(state.error_code.valid && state.error_code.value == 8);
  assert(state.power.valid && !state.power.value);
  assert(state.legacy_ui_heating.valid && !state.legacy_ui_heating.value);
  assert(state.manual_level.valid && state.manual_level.value == 5);
  assert(state.target_temperature_c.valid);
  assert(std::fabs(state.target_temperature_c.value - 20.0f) < 0.001f);
  assert(state.reported_temperature_unit.valid && state.reported_temperature_unit.value == TemperatureUnit::FAHRENHEIT);
  assert(state.reported_altitude_unit.valid && state.reported_altitude_unit.value == AltitudeUnit::METRES);
  assert(state.altitude_m.valid && std::fabs(state.altitude_m.value - 30.0f) < 0.001f);
  assert(state.supply_voltage_v.valid && std::fabs(state.supply_voltage_v.value - 12.5f) < 0.001f);
  assert(state.case_temperature_c.valid && state.case_temperature_c.value == -32768.0f);
  assert(state.cabin_temperature_c.valid && std::fabs(state.cabin_temperature_c.value + 12.3f) < 0.001f);
  assert(state.temperature_offset_c.valid);
  assert(std::fabs(state.temperature_offset_c.value + (10.0f / 9.0f)) < 0.001f);
  assert(state.rf433_enabled.valid && state.rf433_enabled.value);
  assert(encrypted[29] != 21);  // RF433 must use the decoded byte, not ciphertext.
  assert(state.oil_pump_type.valid && state.oil_pump_type.value == 21);
  assert(state.start_time.valid && state.start_time.value == 0x0102);
  assert(state.auto_time.valid && state.auto_time.value == 0x0304);
  assert(state.run_time.valid && state.run_time.value == 0x0506);

  std::vector<uint8_t> decrypted = ResponseParser::decrypt(encrypted);
  decrypted[9] = 100;
  decrypted[10] = 0;
  decrypted[26] = 6;
  decrypted[27] = 2;
  decrypted[29] = 19;
  decrypted[30] = 2;
  decrypted[31] = 2;
  encrypted = xor_crypt(decrypted);
  assert(ResponseParser::parse(encrypted, state) == ParseResult::OK);
  assert(!state.target_temperature_c.valid);
  assert(!state.manual_level.valid);
  assert(!state.reported_temperature_unit.valid);
  assert(!state.temperature_offset_c.valid);
  assert(!state.reported_altitude_unit.valid);
  assert(!state.altitude_m.valid);
  assert(!state.language.valid);
  assert(!state.rf433_enabled.valid);
  assert(!state.automatic_heating_enabled.valid);

  decrypted = ResponseParser::decrypt(encrypted);
  decrypted[27] = 0;
  decrypted[34] = 0x80;
  decrypted[30] = 1;
  decrypted[6] = 0x00;
  decrypted[7] = 0x64;  // 10.0 imperial units -> 3.048 metres.
  decrypted[13] = 0x7F;
  decrypted[14] = 0xFF;  // 32767 C, big endian.
  decrypted[32] = 0x80;
  decrypted[33] = 0x00;  // -3276.8 C, big endian tenths.
  encrypted = xor_crypt(decrypted);
  assert(ResponseParser::parse(encrypted, state) == ParseResult::OK);
  assert(state.temperature_offset_c.valid && state.temperature_offset_c.value == -128.0f);
  assert(state.altitude_m.valid && std::fabs(state.altitude_m.value - 3.048f) < 0.001f);
  assert(state.case_temperature_c.valid && state.case_temperature_c.value == 32767.0f);
  assert(state.cabin_temperature_c.valid && std::fabs(state.cabin_temperature_c.value + 3276.8f) < 0.001f);

  encrypted = encrypted_frame(ProtocolVariant::MD2_AA55_ENCRYPTED);
  decrypted = ResponseParser::decrypt(encrypted);
  decrypted[9] = 40;
  encrypted = xor_crypt(decrypted);
  assert(ResponseParser::parse(encrypted, state) == ParseResult::OK);
  assert(state.target_temperature_c.valid && std::fabs(state.target_temperature_c.value - (40.0f - 32.0f) * (5.0f / 9.0f)) < 0.001f);
  decrypted = ResponseParser::decrypt(encrypted);
  decrypted[9] = 99;
  encrypted = xor_crypt(decrypted);
  assert(ResponseParser::parse(encrypted, state) == ParseResult::OK);
  assert(state.target_temperature_c.valid && std::fabs(state.target_temperature_c.value - (99.0f - 32.0f) * (5.0f / 9.0f)) < 0.001f);
}

void test_variant_transition_invalidates_extended_fields() {
  HeaterState state{};
  assert(ResponseParser::parse(encrypted_frame(ProtocolVariant::MD2_AA55_ENCRYPTED), state) == ParseResult::OK);
  assert(state.temperature_offset_c.valid);
  assert(state.rf433_enabled.valid);
  assert(state.reported_temperature_unit.valid);
  assert(state.start_time.valid);

  assert(ResponseParser::parse(plain_frame(ProtocolVariant::MD1_AA55_PLAIN), state) == ParseResult::OK);
  assert(!state.temperature_offset_c.valid);
  assert(!state.rf433_enabled.valid);
  assert(!state.reported_temperature_unit.valid);
  assert(!state.start_time.valid);
  assert(!state.automatic_heating_enabled.valid);
}

void test_variant_specific_error_offsets() {
  HeaterState state{};
  const std::vector<uint8_t> md3 = plain_frame(ProtocolVariant::MD3_AA66_PLAIN);
  assert(ResponseParser::parse(md3, state) == ParseResult::OK);
  assert(state.error_code.valid && state.error_code.value == 7);

  const std::vector<uint8_t> md4 = encrypted_frame(ProtocolVariant::MD4_AA66_ENCRYPTED);
  assert(ResponseParser::parse(md4, state) == ParseResult::OK);
  assert(state.error_code.valid && state.error_code.value == 6);
}

}  // namespace

int main() {
  test_all_short_prefixes_preserve_state();
  test_header_validation_and_decryption();
  test_full_length_malformed_frames_preserve_state();
  test_documented_completeness_policy();
  test_plain_normalization_and_invalidation();
  test_encrypted_normalization_and_validity();
  test_variant_transition_invalidates_extended_fields();
  test_variant_specific_error_offsets();
  return 0;
}
