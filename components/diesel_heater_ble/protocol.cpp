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

bool ProtocolSelection::parse(const std::vector<uint8_t> &raw, HeaterState &state, ParseResult &result) {
  const ProtocolDriver *candidate = this->driver_;
  if (candidate == nullptr) {
    candidate = this->configured_ == ProtocolVariant::UNKNOWN
                    ? ProtocolRegistry::find(AAProtocolDriver::detect_variant(raw))
                    : ProtocolRegistry::find(this->configured_);
  }
  if (candidate == nullptr) {
    HeaterState ignored_state{};
    const ProtocolDriver *ignored = ProtocolRegistry::parse_and_find(raw, ignored_state, result);
    (void) ignored;
    this->invalid_response_seen_ = true;
    return false;
  }

  HeaterState parsed{};
  result = candidate->parse(raw, parsed);
  if (result != ParseResult::OK) {
    this->invalid_response_seen_ = true;
    return false;
  }
  if (this->driver_ != nullptr && this->driver_ != candidate) {
    result = ParseResult::UNKNOWN_PROTOCOL;
    this->invalid_response_seen_ = true;
    return false;
  }
  this->driver_ = candidate;
  state = parsed;
  return true;
}

CommandBatch ProtocolSelection::discovery_probe(uint16_t passkey) const {
  if (!AAProtocolDriver::valid_passkey(passkey)) return CommandBatch::with_result(CommandResult::INVALID_VALUE);
  if (!this->should_probe()) return CommandBatch::with_result(CommandResult::NO_CHANGE);
  CommandBatch probe = CommandBatch::with_result(CommandResult::READY);
  probe.append(AAProtocolDriver::encode(passkey, AACommand::STATUS, 0));
  return probe;
}

}  // namespace diesel_heater_ble
}  // namespace esphome
