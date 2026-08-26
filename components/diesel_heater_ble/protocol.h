#pragma once

#include <cstdint>
#include <vector>

#include "contracts.h"
#include "state.h"

namespace esphome {
namespace diesel_heater_ble {

// ProtocolDriver owns one response format and the AA-family commands that are
// valid after that format has been selected. Instances are static; callers do
// not own or allocate drivers.
class ProtocolDriver {
 public:
  virtual ~ProtocolDriver() = default;

  virtual ProtocolVariant variant() const = 0;
  virtual Capabilities capabilities() const = 0;
  virtual ParseResult parse(const std::vector<uint8_t> &raw, HeaterState &state) const = 0;

  virtual CommandBatch status(uint16_t passkey) const = 0;
  virtual CommandBatch power(const HeaterState &state, uint16_t passkey, bool enabled) const = 0;
  virtual CommandBatch manual_level(const HeaterState &state, uint16_t passkey, uint8_t level) const = 0;
  virtual CommandBatch temperature_c(const HeaterState &state, uint16_t passkey, float temperature_c) const = 0;
};

// This registry is the sole response-driver selection path. It contains a
// permanent static instance for every app-derived AA response format.
class ProtocolRegistry {
 public:
  static const ProtocolDriver *find(ProtocolVariant variant);
  static const ProtocolDriver *parse_and_find(const std::vector<uint8_t> &raw, HeaterState &state,
                                              ParseResult &result);
};

}  // namespace diesel_heater_ble
}  // namespace esphome
