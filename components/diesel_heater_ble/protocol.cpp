#include "protocol.h"

#include "protocol_aa.h"

namespace esphome {
namespace diesel_heater_ble {
namespace {
const AAProtocolDriver kDrivers[] = {
    AAProtocolDriver(ProtocolVariant::MD1_AA55_PLAIN), AAProtocolDriver(ProtocolVariant::MD2_AA55_ENCRYPTED),
    AAProtocolDriver(ProtocolVariant::MD3_AA66_PLAIN), AAProtocolDriver(ProtocolVariant::MD4_AA66_ENCRYPTED),
};
}  // namespace

const ProtocolDriver *ProtocolRegistry::find(ProtocolVariant variant) {
  for (const auto &driver : kDrivers) if (driver.variant() == variant) return &driver;
  return nullptr;
}

const ProtocolDriver *ProtocolRegistry::parse_and_find(const std::vector<uint8_t> &raw, HeaterState &state,
                                                        ParseResult &result) {
  const ProtocolVariant variant = AAProtocolDriver::detect_variant(raw);
  const ProtocolDriver *driver = find(variant);
  if (driver == nullptr) {
    if (variant == ProtocolVariant::UNKNOWN)
      result = raw.size() < 2 ? ParseResult::INCOMPLETE :
               ((raw[0] == 0xAA || raw[0] == 0xDA) ? ParseResult::INVALID_FRAME : ParseResult::UNKNOWN_PROTOCOL);
    else
      result = ParseResult::UNKNOWN_PROTOCOL;
    return nullptr;
  }
  result = driver->parse(raw, state);
  return result == ParseResult::OK ? driver : nullptr;
}

}  // namespace diesel_heater_ble
}  // namespace esphome
