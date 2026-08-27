#include <cassert>
#include <cstdint>
#include <vector>

#include "protocol.h"
#include "protocol_aa.h"

using esphome::diesel_heater_ble::AAProtocolDriver;
using esphome::diesel_heater_ble::CommandResult;
using esphome::diesel_heater_ble::HeaterState;
using esphome::diesel_heater_ble::ParseResult;
using esphome::diesel_heater_ble::ProtocolSelection;
using esphome::diesel_heater_ble::ProtocolVariant;

namespace {

std::vector<uint8_t> xor_crypt(std::vector<uint8_t> data) {
  constexpr uint8_t key[] = {'p', 'a', 's', 's', 'w', 'o', 'r', 'd'};
  for (std::size_t index = 0; index < data.size(); ++index) data[index] ^= key[index % 8];
  return data;
}

std::vector<uint8_t> frame(ProtocolVariant variant) {
  const bool encrypted = AAProtocolDriver::is_encrypted(variant);
  std::vector<uint8_t> data(AAProtocolDriver::documented_complete_span(variant), 0);
  data[0] = 0xAA;
  data[1] = AAProtocolDriver::expected_plain_header_2(variant);
  data[3] = 1;
  data[8] = 1;
  data[9] = 5;
  data[10] = 5;
  if (variant == ProtocolVariant::MD3_AA66_PLAIN) data[17] = 1;
  if (variant == ProtocolVariant::MD4_AA66_ENCRYPTED) data[35] = 1;
  return encrypted ? xor_crypt(data) : data;
}

HeaterState sentinel() {
  HeaterState state{};
  state.valid = true;
  state.variant = ProtocolVariant::MD3_AA66_PLAIN;
  return state;
}

void test_auto_selects_each_complete_variant() {
  for (const ProtocolVariant variant : {ProtocolVariant::MD1_AA55_PLAIN, ProtocolVariant::MD2_AA55_ENCRYPTED,
                                        ProtocolVariant::MD3_AA66_PLAIN, ProtocolVariant::MD4_AA66_ENCRYPTED}) {
    ProtocolSelection selection;
    HeaterState state{};
    ParseResult result{};
    assert(selection.parse(frame(variant), state, result));
    assert(result == ParseResult::OK);
    assert(selection.driver() != nullptr && selection.driver()->variant() == variant);
    assert(state.valid && state.variant == variant);
  }
}

void test_explicit_selection_and_mismatch_rejection() {
  for (const ProtocolVariant variant : {ProtocolVariant::MD1_AA55_PLAIN, ProtocolVariant::MD2_AA55_ENCRYPTED,
                                        ProtocolVariant::MD3_AA66_PLAIN, ProtocolVariant::MD4_AA66_ENCRYPTED}) {
    ProtocolSelection selection(variant);
    HeaterState state{};
    ParseResult result{};
    assert(selection.parse(frame(variant), state, result));
    assert(selection.driver() != nullptr && selection.driver()->variant() == variant);

    ProtocolSelection mismatch(variant);
    HeaterState preserved = sentinel();
    const HeaterState before = preserved;
    const ProtocolVariant other = variant == ProtocolVariant::MD1_AA55_PLAIN ? ProtocolVariant::MD3_AA66_PLAIN
                                                                               : ProtocolVariant::MD1_AA55_PLAIN;
    assert(!mismatch.parse(frame(other), preserved, result));
    assert(result == ParseResult::UNKNOWN_PROTOCOL);
    assert(mismatch.driver() == nullptr);
    assert(preserved.valid == before.valid && preserved.variant == before.variant);
  }
}

void test_invalid_or_incomplete_response_never_selects() {
  ProtocolSelection selection;
  HeaterState state = sentinel();
  ParseResult result{};
  assert(!selection.parse({0xAA}, state, result));
  assert(result == ParseResult::INCOMPLETE && selection.driver() == nullptr);
  std::vector<uint8_t> malformed = frame(ProtocolVariant::MD1_AA55_PLAIN);
  malformed[1] = 0;
  assert(!selection.parse(malformed, state, result));
  assert(result == ParseResult::INVALID_FRAME && selection.driver() == nullptr);
}

void test_selection_is_stable_until_connection_reset() {
  ProtocolSelection selection;
  HeaterState state{};
  ParseResult result{};
  assert(selection.parse(frame(ProtocolVariant::MD1_AA55_PLAIN), state, result));
  assert(!selection.parse(frame(ProtocolVariant::MD3_AA66_PLAIN), state, result));
  assert(result == ParseResult::UNKNOWN_PROTOCOL);
  assert(selection.driver()->variant() == ProtocolVariant::MD1_AA55_PLAIN);
  selection.reset();
  assert(selection.driver() == nullptr);
  assert(selection.parse(frame(ProtocolVariant::MD3_AA66_PLAIN), state, result));
  assert(selection.driver()->variant() == ProtocolVariant::MD3_AA66_PLAIN);
}

void test_configured_probe_and_bad_passkey_timeout_state() {
  ProtocolSelection selection;
  const auto probe = selection.discovery_probe(4321);
  assert(probe.sendable() && probe.size() == 1);
  const auto *bytes = probe.frame_at(0);
  assert((*bytes)[0] == 0xAA && (*bytes)[1] == 0x55);
  assert((*bytes)[2] == 43 && (*bytes)[3] == 21);
  assert((*bytes)[4] == 1 && (*bytes)[5] == 0 && (*bytes)[6] == 0);
  while (selection.should_probe()) selection.record_probe();
  assert(selection.probes_sent() == ProtocolSelection::kMaxDiscoveryProbes);
  assert(selection.timed_out());  // A wrong-but-valid physical passkey receives no response.
  assert(!selection.invalid_response_seen());
  assert(selection.discovery_probe(4321).result() == CommandResult::NO_CHANGE);
  assert(!selection.discovery_probe(4321).sendable());
  assert(selection.discovery_probe(0).result() == CommandResult::INVALID_VALUE);
}

void test_invalid_response_timeout_classification_resets_per_connection() {
  ProtocolSelection selection;
  HeaterState state{};
  ParseResult result{};
  assert(!selection.parse({0xAA}, state, result));
  assert(selection.invalid_response_seen());
  while (selection.should_probe()) selection.record_probe();
  assert(selection.timed_out());  // Terminal outcome is invalid-response, not no-response.
  selection.reset();
  assert(!selection.invalid_response_seen());
  assert(!selection.timed_out());
}

}  // namespace

int main() {
  test_auto_selects_each_complete_variant();
  test_explicit_selection_and_mismatch_rejection();
  test_invalid_or_incomplete_response_never_selects();
  test_selection_is_stable_until_connection_reset();
  test_configured_probe_and_bad_passkey_timeout_state();
  test_invalid_response_timeout_classification_resets_per_connection();
  return 0;
}
