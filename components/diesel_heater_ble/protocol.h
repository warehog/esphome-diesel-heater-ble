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

// Per-connection response-driver policy. AUTO accepts the first complete,
// successfully parsed known response; an explicit format accepts only that
// matching format. Once selected, a connection cannot switch formats.
class ProtocolSelection {
 public:
  static constexpr uint8_t kMaxDiscoveryProbes = 5;

  explicit ProtocolSelection(ProtocolVariant configured = ProtocolVariant::UNKNOWN) : configured_(configured) {}

  void set_configured(ProtocolVariant configured) {
    this->configured_ = configured;
    this->reset();
  }
  void reset() {
    this->driver_ = nullptr;
    this->probes_sent_ = 0;
    this->invalid_response_seen_ = false;
  }

  const ProtocolDriver *driver() const { return this->driver_; }
  ProtocolVariant configured() const { return this->configured_; }
  bool should_probe() const { return this->driver_ == nullptr && this->probes_sent_ < kMaxDiscoveryProbes; }
  bool timed_out() const { return this->driver_ == nullptr && this->probes_sent_ >= kMaxDiscoveryProbes; }
  bool invalid_response_seen() const { return this->invalid_response_seen_; }
  void record_probe() {
    if (this->should_probe()) ++this->probes_sent_;
  }
  uint8_t probes_sent() const { return this->probes_sent_; }
  CommandBatch discovery_probe(uint16_t passkey) const;

  // State is replaced only after the configured/matching driver completes a
  // parse. `result` reports a mismatch or malformed packet without selection.
  bool parse(const std::vector<uint8_t> &raw, HeaterState &state, ParseResult &result);

 private:
  ProtocolVariant configured_{ProtocolVariant::UNKNOWN};
  const ProtocolDriver *driver_{nullptr};
  uint8_t probes_sent_{0};
  bool invalid_response_seen_{false};
};

}  // namespace diesel_heater_ble
}  // namespace esphome
