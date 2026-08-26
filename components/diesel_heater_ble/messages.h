#pragma once

#include <array>
#include <cstdint>
#include <cstddef>
#include <vector>
#include "state.h"

namespace esphome {
namespace diesel_heater_ble {

class ResponseParser {
 public:
  // These spans are the highest documented app/docs field index plus one.
  // They are app-derived safe-read requirements, not captured complete-frame
  // lengths or proof of BLE notification boundaries.
  static constexpr std::size_t minimum_safe_read_span(ProtocolVariant variant) {
    switch (variant) {
      case ProtocolVariant::MD1_AA55_PLAIN:
        return 17;
      case ProtocolVariant::MD2_AA55_ENCRYPTED:
        return 35;
      case ProtocolVariant::MD3_AA66_PLAIN:
        return 18;
      case ProtocolVariant::MD4_AA66_ENCRYPTED:
        return 36;
      case ProtocolVariant::UNKNOWN:
        return 0;
    }
    return 0;
  }

  // The repository documentation describes 20-byte plain and 48-byte
  // encrypted responses. This is an app/docs-derived completeness policy, not
  // capture proof of a wire-frame length or BLE notification boundary. It is
  // deliberately separate from minimum_safe_read_span(): safe spans establish
  // which indexed reads are safe; this policy determines when the legacy
  // single-notification parser may publish a state.
  static constexpr std::size_t documented_complete_span(ProtocolVariant variant) {
    switch (variant) {
      case ProtocolVariant::MD1_AA55_PLAIN:
      case ProtocolVariant::MD3_AA66_PLAIN:
        return 20;
      case ProtocolVariant::MD2_AA55_ENCRYPTED:
      case ProtocolVariant::MD4_AA66_ENCRYPTED:
        return 48;
      case ProtocolVariant::UNKNOWN:
        return 0;
    }
    return 0;
  }

  static constexpr bool is_encrypted(ProtocolVariant variant) {
    return variant == ProtocolVariant::MD2_AA55_ENCRYPTED || variant == ProtocolVariant::MD4_AA66_ENCRYPTED;
  }

  static constexpr uint8_t expected_plain_header_2(ProtocolVariant variant) {
    switch (variant) {
      case ProtocolVariant::MD1_AA55_PLAIN:
      case ProtocolVariant::MD2_AA55_ENCRYPTED:
        return 0x55;
      case ProtocolVariant::MD3_AA66_PLAIN:
      case ProtocolVariant::MD4_AA66_ENCRYPTED:
        return 0x66;
      case ProtocolVariant::UNKNOWN:
        return 0;
    }
    return 0;
  }

  static ProtocolVariant detect_variant(const std::vector<uint8_t> &raw) {
    if (raw.size() < 2) {
      return ProtocolVariant::UNKNOWN;
    }

    if (raw[0] == 0xAA && raw[1] == 0x55) {
      return ProtocolVariant::MD1_AA55_PLAIN;
    }
    if (raw[0] == 0xAA && raw[1] == 0x66) {
      return ProtocolVariant::MD3_AA66_PLAIN;
    }
    if (raw[0] == 0xDA && raw[1] == 0x34) {
      return ProtocolVariant::MD2_AA55_ENCRYPTED;
    }
    if (raw[0] == 0xDA && raw[1] == 0x07) {
      return ProtocolVariant::MD4_AA66_ENCRYPTED;
    }
    return ProtocolVariant::UNKNOWN;
  }

  static std::vector<uint8_t> decrypt(const std::vector<uint8_t> &raw) {
    std::vector<uint8_t> decrypted = raw;
    const ProtocolVariant variant = detect_variant(raw);
    if (!is_encrypted(variant)) {
      return decrypted;
    }

    // This public test utility XORs only bytes which were actually received.
    // Production parsing uses decoded_byte_() directly, so it does not make a
    // per-notification decrypted-vector allocation.
    for (std::size_t index = 0; index < decrypted.size(); ++index) {
      decrypted[index] ^= kXorKey[index % kXorKey.size()];
    }
    return decrypted;
  }

  // Kept public as a small test seam: parser tests can prove that raw and
  // decrypted headers are independently checked without accepting a
  // pre-decrypted frame through the production parse entry point.
  static bool headers_match(const std::vector<uint8_t> &raw, const std::vector<uint8_t> &decrypted,
                            ProtocolVariant variant) {
    if (variant == ProtocolVariant::UNKNOWN || raw.size() < 2 || decrypted.size() < 2) {
      return false;
    }

    const uint8_t plain_header_2 = expected_plain_header_2(variant);
    const uint8_t raw_header_1 = is_encrypted(variant) ? static_cast<uint8_t>(0xAA ^ kXorKey[0]) : 0xAA;
    const uint8_t raw_header_2 =
        is_encrypted(variant) ? static_cast<uint8_t>(plain_header_2 ^ kXorKey[1]) : plain_header_2;
    return raw[0] == raw_header_1 && raw[1] == raw_header_2 && decrypted[0] == 0xAA &&
           decrypted[1] == plain_header_2;
  }

  static ParseResult parse(const std::vector<uint8_t> &raw, HeaterState &state) {
    const ProtocolVariant variant = detect_variant(raw);
    if (variant == ProtocolVariant::UNKNOWN) {
      if (raw.size() < 2) {
        return ParseResult::INCOMPLETE;
      }
      // AA and DA are the documented plain/encrypted first header bytes.
      // Their unrecognized second byte is a malformed AA-family header, not
      // an unsupported protocol. Other two-byte prefixes remain unsupported.
      return (raw[0] == 0xAA || raw[0] == 0xDA) ? ParseResult::INVALID_FRAME
                                                : ParseResult::UNKNOWN_PROTOCOL;
    }

    // The safe-span check protects every indexed read below. The documented
    // completeness policy is intentionally checked separately: it is not
    // evidence that the lower safe span is an acceptable wire length.
    if (raw.size() < minimum_safe_read_span(variant)) {
      return ParseResult::INCOMPLETE;
    }
    if (raw.size() < documented_complete_span(variant)) {
      return ParseResult::INCOMPLETE;
    }
    if (!headers_match_(raw, variant)) {
      return ParseResult::INVALID_FRAME;
    }

    HeaterState parsed{};
    parsed.valid = true;
    parsed.variant = variant;
    const bool encrypted = is_encrypted(variant);
    parsed.acknowledged_command = {decoded_byte_(raw, 2, encrypted), true};
    parsed.raw_running_state = {decoded_byte_(raw, 3, encrypted), true};
    parsed.power = {power_from_raw_running_state(parsed.raw_running_state.value), true};
    parsed.raw_running_step = {decoded_byte_(raw, 5, encrypted), true};
    parsed.legacy_ui_heating = {legacy_ui_heating_from_raw_running_step(parsed.raw_running_step.value), true};
    parsed.raw_operating_mode = {decoded_byte_(raw, 8, encrypted), true};
    const OperatingMode mode = operating_mode_from_raw(parsed.raw_operating_mode.value);
    if (is_normalized_operating_mode(mode)) {
      parsed.operating_mode = {mode, true};
    }

    if (variant == ProtocolVariant::MD1_AA55_PLAIN || variant == ProtocolVariant::MD2_AA55_ENCRYPTED) {
      parsed.error_code = {decoded_byte_(raw, 4, encrypted), true};
    } else if (variant == ProtocolVariant::MD3_AA66_PLAIN) {
      parsed.error_code = {decoded_byte_(raw, 17, encrypted), true};
    } else {
      parsed.error_code = {decoded_byte_(raw, 35, encrypted), true};
    }

    if (encrypted) {
      parse_encrypted_fields_(raw, parsed);
    } else {
      parse_plain_fields_(raw, parsed);
    }

    // All reads above are covered by minimum_safe_read_span(). Only this
    // complete temporary replacement publishes a newly decoded state.
    state = parsed;
    return ParseResult::OK;
  }

 private:
  static constexpr std::array<uint8_t, 8> kXorKey{{'p', 'a', 's', 's', 'w', 'o', 'r', 'd'}};

  // parse() checks the variant safe span before calling this helper. This
  // helper is intentionally not a general unchecked reader.
  static uint8_t decoded_byte_(const std::vector<uint8_t> &data, std::size_t index, bool encrypted) {
    const uint8_t byte = data[index];
    return encrypted ? static_cast<uint8_t>(byte ^ kXorKey[index % kXorKey.size()]) : byte;
  }

  static bool headers_match_(const std::vector<uint8_t> &raw, ProtocolVariant variant) {
    if (variant == ProtocolVariant::UNKNOWN || raw.size() < 2) {
      return false;
    }
    const bool encrypted = is_encrypted(variant);
    const uint8_t plain_header_2 = expected_plain_header_2(variant);
    const uint8_t raw_header_1 = encrypted ? static_cast<uint8_t>(0xAA ^ kXorKey[0]) : 0xAA;
    const uint8_t raw_header_2 = encrypted ? static_cast<uint8_t>(plain_header_2 ^ kXorKey[1]) : plain_header_2;
    return raw[0] == raw_header_1 && raw[1] == raw_header_2 && decoded_byte_(raw, 0, encrypted) == 0xAA &&
           decoded_byte_(raw, 1, encrypted) == plain_header_2;
  }

  static uint16_t read_u16_le_(const std::vector<uint8_t> &data, std::size_t index, bool encrypted) {
    return static_cast<uint16_t>(decoded_byte_(data, index, encrypted)) |
           (static_cast<uint16_t>(decoded_byte_(data, index + 1, encrypted)) << 8U);
  }

  static uint16_t read_u16_be_(const std::vector<uint8_t> &data, std::size_t index, bool encrypted) {
    return (static_cast<uint16_t>(decoded_byte_(data, index, encrypted)) << 8U) |
           static_cast<uint16_t>(decoded_byte_(data, index + 1, encrypted));
  }

  static int16_t read_i16_le_(const std::vector<uint8_t> &data, std::size_t index, bool encrypted) {
    return signed16_(read_u16_le_(data, index, encrypted));
  }

  static int16_t read_i16_be_(const std::vector<uint8_t> &data, std::size_t index, bool encrypted) {
    return signed16_(read_u16_be_(data, index, encrypted));
  }

  static int16_t signed16_(uint16_t value) {
    return value <= 0x7FFF ? static_cast<int16_t>(value)
                           : static_cast<int16_t>(static_cast<int32_t>(value) - 0x10000);
  }

  static int8_t signed8_(uint8_t value) {
    return value <= 0x7F ? static_cast<int8_t>(value) : static_cast<int8_t>(static_cast<int16_t>(value) - 0x100);
  }

  static void set_manual_level_(HeaterState &state, uint8_t level) {
    if (level >= 1 && level <= 10) {
      state.manual_level = {level, true};
    }
  }

  static void set_celsius_target_(HeaterState &state, float temperature_c) {
    if (temperature_c >= 8.0f && temperature_c <= 36.0f) {
      state.target_temperature_c = {temperature_c, true};
    }
  }

  static void parse_plain_fields_(const std::vector<uint8_t> &data, HeaterState &state) {
    state.altitude_m = {static_cast<float>(read_u16_le_(data, 6, false)), true};
    state.supply_voltage_v = {static_cast<float>(read_u16_le_(data, 11, false)) / 10.0f, true};
    state.case_temperature_c = {static_cast<float>(read_i16_le_(data, 13, false)), true};
    state.cabin_temperature_c = {static_cast<float>(read_i16_le_(data, 15, false)), true};

    // The app/docs define the field layout by raw running mode. In particular,
    // target temperature is unavailable outside mode 2 and must not retain a
    // value from a preceding thermostat response.
    switch (decoded_byte_(data, 8, false)) {
      case 0:
        if (decoded_byte_(data, 10, false) < 10) {
          set_manual_level_(state, static_cast<uint8_t>(decoded_byte_(data, 10, false) + 1));
        }
        break;
      case 1:
        set_manual_level_(state, decoded_byte_(data, 9, false));
        break;
      case 2:
        if (decoded_byte_(data, 10, false) < 10) {
          set_manual_level_(state, static_cast<uint8_t>(decoded_byte_(data, 10, false) + 1));
        }
        set_celsius_target_(state, static_cast<float>(decoded_byte_(data, 9, false)));
        break;
      default:
        break;
    }
  }

  static void parse_encrypted_fields_(const std::vector<uint8_t> &data, HeaterState &state) {
    const uint8_t raw_temperature_unit = decoded_byte_(data, 27, true);
    const uint8_t raw_altitude_unit = decoded_byte_(data, 30, true);
    if (raw_temperature_unit == 0) {
      state.reported_temperature_unit = {TemperatureUnit::CELSIUS, true};
      set_celsius_target_(state, static_cast<float>(decoded_byte_(data, 9, true)));
      state.temperature_offset_c = {static_cast<float>(signed8_(decoded_byte_(data, 34, true))), true};
    } else if (raw_temperature_unit == 1) {
      state.reported_temperature_unit = {TemperatureUnit::FAHRENHEIT, true};
      const uint8_t target_temperature_f = decoded_byte_(data, 9, true);
      if (target_temperature_f >= 40 && target_temperature_f <= 99) {
        state.target_temperature_c = {(static_cast<float>(target_temperature_f) - 32.0f) * (5.0f / 9.0f), true};
      }
      state.temperature_offset_c = {static_cast<float>(signed8_(decoded_byte_(data, 34, true))) * (5.0f / 9.0f), true};
    }

    const float reported_altitude = static_cast<float>(read_u16_be_(data, 6, true)) / 10.0f;
    if (raw_altitude_unit == 0) {
      state.reported_altitude_unit = {AltitudeUnit::METRES, true};
      state.altitude_m = {reported_altitude, true};
    } else if (raw_altitude_unit == 1) {
      state.reported_altitude_unit = {AltitudeUnit::IMPERIAL, true};
      state.altitude_m = {reported_altitude * 0.3048f, true};
    }

    set_manual_level_(state, decoded_byte_(data, 10, true));
    state.supply_voltage_v = {static_cast<float>(read_u16_be_(data, 11, true)) / 10.0f, true};
    state.case_temperature_c = {static_cast<float>(read_i16_be_(data, 13, true)), true};
    state.cabin_temperature_c = {static_cast<float>(read_i16_be_(data, 32, true)) / 10.0f, true};
    state.start_time = {read_u16_be_(data, 19, true), TimeValueUnit::PROTOCOL_TICKS, true};
    state.auto_time = {read_u16_be_(data, 21, true), TimeValueUnit::PROTOCOL_TICKS, true};
    state.run_time = {read_u16_be_(data, 23, true), TimeValueUnit::PROTOCOL_TICKS, true};
    const uint8_t automatic_start_stop = decoded_byte_(data, 25, true);
    if (automatic_start_stop <= 1) {
      state.automatic_start_stop_enabled = {automatic_start_stop == 1, true};
    }
    const uint8_t language = decoded_byte_(data, 26, true);
    if (language <= 5) {
      state.language = {language, true};
    }
    state.tank_volume = {decoded_byte_(data, 28, true), true};
    const uint8_t oil_pump_type = decoded_byte_(data, 29, true);
    state.oil_pump_type = {oil_pump_type, true};
    if (oil_pump_type == 20) {
      state.rf433_enabled = {false, true};
    } else if (oil_pump_type == 21) {
      state.rf433_enabled = {true, true};
    }
    const uint8_t automatic_heating = decoded_byte_(data, 31, true);
    if (automatic_heating <= 1) {
      state.automatic_heating_enabled = {automatic_heating == 1, true};
    }
  }
};

static_assert(ResponseParser::documented_complete_span(ProtocolVariant::MD1_AA55_PLAIN) >=
                  ResponseParser::minimum_safe_read_span(ProtocolVariant::MD1_AA55_PLAIN),
              "MD1 completeness policy must cover every safe read");
static_assert(ResponseParser::documented_complete_span(ProtocolVariant::MD2_AA55_ENCRYPTED) >=
                  ResponseParser::minimum_safe_read_span(ProtocolVariant::MD2_AA55_ENCRYPTED),
              "MD2 completeness policy must cover every safe read");
static_assert(ResponseParser::documented_complete_span(ProtocolVariant::MD3_AA66_PLAIN) >=
                  ResponseParser::minimum_safe_read_span(ProtocolVariant::MD3_AA66_PLAIN),
              "MD3 completeness policy must cover every safe read");
static_assert(ResponseParser::documented_complete_span(ProtocolVariant::MD4_AA66_ENCRYPTED) >=
                  ResponseParser::minimum_safe_read_span(ProtocolVariant::MD4_AA66_ENCRYPTED),
              "MD4 completeness policy must cover every safe read");

class Request {
 public:
  uint8_t header_1 = 0xAA;
  uint8_t header_2 = 0x55;
  uint8_t password_1 = 0x0C;
  uint8_t password_2 = 0x22;
  uint8_t command;
  uint8_t data_1;
  uint8_t data_2;
  uint8_t checksum;

  Request(uint8_t command, uint8_t data1 = 0x00, uint8_t data2 = 0x00)
      : command(command), data_1(data1), data_2(data2) {
    calculateChecksum();
  }

  void calculateChecksum() { checksum = (password_1 + password_2 + command + data_1 + data_2) % 256; }

  std::vector<uint8_t> toBytes() const {
    return {header_1, header_2, password_1, password_2, command, data_1, data_2, checksum};
  }
};

// Specific Requests

class StatusRequest : public Request {
 public:
  StatusRequest() : Request(0x01) {}
};

class SetPowerRequest : public Request {
 public:
  SetPowerRequest(bool enable) : Request(0x03, enable ? 0x01 : 0x00, 0x00) {}
};

class SetTemperatureRequest : public Request {
 public:
  SetTemperatureRequest(uint8_t temperature) : Request(0x04, temperature, 0x00) {}
};

class SetLevelRequest : public Request {
 public:
  SetLevelRequest(uint8_t level) : Request(0x04, level - 1, 0x00) {}
};

class SetRunningModeRequest : public Request {
 public:
  SetRunningModeRequest(uint8_t mode) : Request(0x02, mode, 0x00) {}
};

class SetAutomaticStartStopRequest : public Request {
 public:
  SetAutomaticStartStopRequest(bool enable) : Request(0x13, enable ? 0x01 : 0x00, 0x00) {}
};

class SetLanguageRequest : public Request {
 public:
  SetLanguageRequest(uint8_t language_code) : Request(0x14, language_code, 0x00) {}
};

class SetTemperatureUnitRequest : public Request {
 public:
  SetTemperatureUnitRequest(bool is_celsius) : Request(0x15, is_celsius ? 0x01 : 0x00, 0x00) {}
};

class SetAltitudeUnitRequest : public Request {
 public:
  SetAltitudeUnitRequest(bool is_meters) : Request(0x16, is_meters ? 0x01 : 0x00, 0x00) {}
};

class SetTankVolumeRequest : public Request {
 public:
  SetTankVolumeRequest(uint8_t volume) : Request(0x17, volume, 0x00) {}
};

class SetOilPumpTypeRequest : public Request {
 public:
  SetOilPumpTypeRequest(uint8_t type) : Request(0x18, type, 0x00) {}
};

class SetTemperatureOffsetRequest : public Request {
 public:
  SetTemperatureOffsetRequest(uint8_t offset) : Request(0x20, offset, 0x00) {}
};

}  // namespace diesel_heater_ble
}  // namespace esphome
