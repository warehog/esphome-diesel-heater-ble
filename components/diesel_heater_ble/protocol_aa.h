#pragma once

#include <array>
#include <cstddef>

#include "protocol.h"

namespace esphome {
namespace diesel_heater_ble {

constexpr uint16_t kDefaultAAPasskey = 1234;

// Decimal IDs from the AirHeaterBLE 1.20 app. Keep the wire byte explicit so
// no hexadecimal-looking source literal can accidentally change its meaning.
enum class AACommand : uint8_t {
  STATUS = 1,
  MODE = 2,
  POWER = 3,
  VALUE = 4,
  CLOCK = 10,
  AUTOMATIC_START_TIME = 11,
  AUTOMATIC_RUN_TIME = 12,
  AUTOMATIC_ENABLED = 13,
  LANGUAGE = 14,
  TEMPERATURE_UNIT = 15,
  TANK_VOLUME = 16,
  OIL_PUMP_OR_RF433 = 17,
  AUTOMATIC_HEATING = 18,
  ALTITUDE_UNIT = 19,
  TEMPERATURE_OFFSET = 20,
};

class AAProtocolDriver final : public ProtocolDriver {
 public:
  explicit constexpr AAProtocolDriver(ProtocolVariant variant) : variant_(variant) {}

  ProtocolVariant variant() const override { return this->variant_; }
  Capabilities capabilities() const override { return capabilities_for(this->variant_); }
  ParseResult parse(const std::vector<uint8_t> &raw, HeaterState &state) const override;

  CommandBatch status(uint16_t passkey) const override;
  CommandBatch clock(uint16_t passkey, uint16_t minutes_since_midnight) const override;
  CommandBatch power(const HeaterState &state, uint16_t passkey, bool enabled) const override;
  CommandBatch manual_level(const HeaterState &state, uint16_t passkey, uint8_t level) const override;
  CommandBatch temperature_c(const HeaterState &state, uint16_t passkey, float temperature_c) const override;

  static constexpr std::size_t minimum_safe_read_span(ProtocolVariant variant) {
    switch (variant) {
      case ProtocolVariant::MD1_AA55_PLAIN: return 17;
      case ProtocolVariant::MD2_AA55_ENCRYPTED: return 35;
      case ProtocolVariant::MD3_AA66_PLAIN: return 18;
      case ProtocolVariant::MD4_AA66_ENCRYPTED: return 36;
      case ProtocolVariant::UNKNOWN: return 0;
    }
    return 0;
  }
  static constexpr std::size_t documented_complete_span(ProtocolVariant variant) {
    switch (variant) {
      case ProtocolVariant::MD1_AA55_PLAIN:
      case ProtocolVariant::MD3_AA66_PLAIN: return 20;
      case ProtocolVariant::MD2_AA55_ENCRYPTED:
      case ProtocolVariant::MD4_AA66_ENCRYPTED: return 48;
      case ProtocolVariant::UNKNOWN: return 0;
    }
    return 0;
  }
  static constexpr bool is_encrypted(ProtocolVariant variant) {
    return variant == ProtocolVariant::MD2_AA55_ENCRYPTED || variant == ProtocolVariant::MD4_AA66_ENCRYPTED;
  }
  static constexpr uint8_t expected_plain_header_2(ProtocolVariant variant) {
    switch (variant) {
      case ProtocolVariant::MD1_AA55_PLAIN:
      case ProtocolVariant::MD2_AA55_ENCRYPTED: return 0x55;
      case ProtocolVariant::MD3_AA66_PLAIN:
      case ProtocolVariant::MD4_AA66_ENCRYPTED: return 0x66;
      case ProtocolVariant::UNKNOWN: return 0;
    }
    return 0;
  }
  static ProtocolVariant detect_variant(const std::vector<uint8_t> &raw);
  static std::vector<uint8_t> decrypt(const std::vector<uint8_t> &raw);
  static bool headers_match(const std::vector<uint8_t> &raw, const std::vector<uint8_t> &decrypted,
                            ProtocolVariant variant);

  // Fixed AA encoder test seam. Values are written as a complete signed
  // sixteen-bit little-endian field; its bit pattern is preserved for values
  // such as temperature offsets.
  static CommandFrame encode(uint16_t passkey, AACommand command, int16_t value);
  static bool valid_passkey(uint16_t passkey) { return passkey >= 1 && passkey <= 9999; }

 private:
  static constexpr std::array<uint8_t, 8> kXorKey{{'p', 'a', 's', 's', 'w', 'o', 'r', 'd'}};

  static uint8_t decoded_byte_(const std::vector<uint8_t> &data, std::size_t index, bool encrypted);
  static bool headers_match_(const std::vector<uint8_t> &raw, ProtocolVariant variant);
  static uint16_t read_u16_le_(const std::vector<uint8_t> &data, std::size_t index, bool encrypted);
  static uint16_t read_u16_be_(const std::vector<uint8_t> &data, std::size_t index, bool encrypted);
  static int16_t read_i16_le_(const std::vector<uint8_t> &data, std::size_t index, bool encrypted);
  static int16_t read_i16_be_(const std::vector<uint8_t> &data, std::size_t index, bool encrypted);
  static int16_t signed16_(uint16_t value);
  static int8_t signed8_(uint8_t value);
  static void set_manual_level_(HeaterState &state, uint8_t level);
  static void set_celsius_target_(HeaterState &state, float temperature_c);
  static void parse_plain_fields_(const std::vector<uint8_t> &data, HeaterState &state);
  static void parse_encrypted_fields_(const std::vector<uint8_t> &data, HeaterState &state);
  static CommandBatch command_(uint16_t passkey, AACommand command, int16_t value);
  static CommandBatch mode_then_value_(const HeaterState &state, uint16_t passkey, OperatingMode required_mode,
                                       AACommand command, int16_t value, bool value_matches);

  ProtocolVariant variant_;
};

static_assert(AAProtocolDriver::documented_complete_span(ProtocolVariant::MD1_AA55_PLAIN) >=
                  AAProtocolDriver::minimum_safe_read_span(ProtocolVariant::MD1_AA55_PLAIN),
              "MD1 completeness policy must cover every safe read");
static_assert(AAProtocolDriver::documented_complete_span(ProtocolVariant::MD2_AA55_ENCRYPTED) >=
                  AAProtocolDriver::minimum_safe_read_span(ProtocolVariant::MD2_AA55_ENCRYPTED),
              "MD2 completeness policy must cover every safe read");
static_assert(AAProtocolDriver::documented_complete_span(ProtocolVariant::MD3_AA66_PLAIN) >=
                  AAProtocolDriver::minimum_safe_read_span(ProtocolVariant::MD3_AA66_PLAIN),
              "MD3 completeness policy must cover every safe read");
static_assert(AAProtocolDriver::documented_complete_span(ProtocolVariant::MD4_AA66_ENCRYPTED) >=
                  AAProtocolDriver::minimum_safe_read_span(ProtocolVariant::MD4_AA66_ENCRYPTED),
              "MD4 completeness policy must cover every safe read");

}  // namespace diesel_heater_ble
}  // namespace esphome
