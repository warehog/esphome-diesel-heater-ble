#include <cassert>
#include <array>
#include <cstdint>

#include "availability.h"
#include "climate_session.h"
#include "command_dispatch.h"
#include "lifecycle.h"

using esphome::diesel_heater_ble::Capability;
using esphome::diesel_heater_ble::Capabilities;
using esphome::diesel_heater_ble::CommandAdmission;
using esphome::diesel_heater_ble::CommandBatch;
using esphome::diesel_heater_ble::CommandDispatchGate;
using esphome::diesel_heater_ble::CommandFrame;
using esphome::diesel_heater_ble::DispatchResult;
using esphome::diesel_heater_ble::AvailabilityEntity;
using esphome::diesel_heater_ble::AvailabilityGeneration;
using esphome::diesel_heater_ble::AvailabilityMask;
using esphome::diesel_heater_ble::ClimateFanSession;
using esphome::diesel_heater_ble::LifecycleSession;
using esphome::diesel_heater_ble::capability_bits;
using esphome::diesel_heater_ble::availability_bit;
using esphome::diesel_heater_ble::kAvailabilityEntityCount;

namespace {

constexpr Capabilities kPower = capability_bits(Capability::POWER);
constexpr Capabilities kLevel = capability_bits(Capability::MODE) | Capability::MANUAL_LEVEL;
void assert_rejected_without_write(const LifecycleSession &session, Capabilities required, bool value_valid,
                                   CommandAdmission expected) {
  unsigned writes = 0;
  const DispatchResult dispatch = CommandDispatchGate::dispatch(
      session.admit(required, value_valid), nullptr, [&writes](const CommandFrame &) { ++writes; });
  assert(session.admit(required, value_valid) == expected);
  assert(dispatch == DispatchResult::ADMISSION_REJECTED);
  assert(writes == 0);
}

void test_every_admission_rejection_is_ordered_and_write_free() {
  LifecycleSession session;
  assert_rejected_without_write(session, kPower, true, CommandAdmission::CONNECTION_UNAVAILABLE);

  session.set_established();
  assert_rejected_without_write(session, kPower, true, CommandAdmission::HANDLE_UNAVAILABLE);

  session.set_characteristic_handle(0x0042);
  assert_rejected_without_write(session, kPower, true, CommandAdmission::DRIVER_UNAVAILABLE);

  session.confirm_response(kPower);
  assert(session.admit(kPower, true) == CommandAdmission::READY);
  assert_rejected_without_write(session, kLevel, true, CommandAdmission::CAPABILITY_UNAVAILABLE);
  assert_rejected_without_write(session, kPower, false, CommandAdmission::INVALID_VALUE);

  LifecycleSession no_state;
  no_state.set_established();
  no_state.set_characteristic_handle(0x0042);
  no_state.select_driver(kPower);
  assert_rejected_without_write(no_state, kPower, true, CommandAdmission::STATE_UNAVAILABLE);

  CommandBatch invalid_batch{};
  unsigned writes = 0;
  assert(CommandDispatchGate::dispatch(CommandAdmission::READY, &invalid_batch,
                                      [&writes](const CommandFrame &) { ++writes; }) == DispatchResult::BATCH_REJECTED);
  assert(writes == 0);

  CommandBatch ready_batch = CommandBatch::with_result(esphome::diesel_heater_ble::CommandResult::READY);
  CommandFrame first{};
  CommandFrame second{};
  first[0] = 1;
  second[0] = 2;
  assert(ready_batch.append(first));
  assert(ready_batch.append(second));
  std::array<uint8_t, 2> written{};
  assert(CommandDispatchGate::dispatch(CommandAdmission::READY, &ready_batch, [&written](const CommandFrame &frame) {
           written[frame[0] - 1] = frame[0];
         }) == DispatchResult::WRITTEN);
  assert(written[0] == 1 && written[1] == 2);
}

void test_disconnect_clears_discovery_idle_and_entity_generation() {
  LifecycleSession session;
  session.set_characteristic_handle(0x0042);  // Discovery has found a handle but is not connected.
  assert(!session.disconnect());
  assert(session.admit(kPower, true) == CommandAdmission::CONNECTION_UNAVAILABLE);
  assert(session.handle() == 0);

  session.begin_connection();
  session.set_characteristic_handle(0x0042);
  session.set_established();
  session.confirm_response(kPower);
  assert(session.can_publish_state());
  session.mark_entities_published();
  assert(session.disconnect());
  assert(!session.disconnect());  // Exactly one unavailable edge.
  assert(!session.can_publish_state());
  assert(session.admit(kPower, true) == CommandAdmission::CONNECTION_UNAVAILABLE);
}

void test_reconnect_needs_fresh_response_and_recovers_after_rapid_disconnect() {
  LifecycleSession session;
  AvailabilityGeneration availability;
  assert(kAvailabilityEntityCount == 24);
  for (uint8_t index = 0; index < kAvailabilityEntityCount; ++index) {
    availability.register_entity(static_cast<AvailabilityEntity>(index));
  }
  const AvailabilityMask all_entities = availability.registered();
  assert(all_entities == ((AvailabilityMask{1} << kAvailabilityEntityCount) - 1));
  session.begin_connection();
  session.set_characteristic_handle(0x0042);
  session.set_established();
  session.confirm_response(kPower);
  session.mark_entities_published();
  availability.mark_published();
  assert(session.disconnect());
  const AvailabilityMask first_disconnect = availability.disconnect();
  assert(first_disconnect == all_entities);
  std::array<unsigned, kAvailabilityEntityCount> unavailable_counts{};
  for (uint8_t index = 0; index < kAvailabilityEntityCount; ++index) {
    const AvailabilityEntity entity = static_cast<AvailabilityEntity>(index);
    if ((first_disconnect & availability_bit(entity)) != 0) ++unavailable_counts[index];
    assert(unavailable_counts[index] == 1);  // Every identity exactly once.
  }
  assert(!availability.may_publish());
  assert(availability.disconnect() == 0);  // No duplicate unavailable publish.

  session.begin_connection();
  session.set_characteristic_handle(0x0042);
  session.set_established();
  assert(!session.can_publish_state());
  assert_rejected_without_write(session, kPower, true, CommandAdmission::DRIVER_UNAVAILABLE);
  assert(!availability.may_publish());  // No stale publication before fresh response.
  session.confirm_response(kPower);
  assert(session.can_publish_state());
  session.mark_entities_published();
  availability.mark_published();
  assert(session.disconnect());
  const AvailabilityMask second_disconnect = availability.disconnect();
  assert(second_disconnect == all_entities);  // Fresh generation recovers every identity.
}

void test_climate_fan_session_clears_on_disconnect_and_missing_manual_level() {
  ClimateFanSession session;
  session.set_manual();
  assert(!session.fan_mode_set() && session.custom_fan_mode_set());
  session.clear();  // Runtime also resets Climate::fan_mode and custom_fan_mode_.
  assert(!session.fan_mode_set() && !session.custom_fan_mode_set());
  session.set_manual();
  session.clear();  // A fresh parsed state without manual_level must not retain it.
  assert(!session.fan_mode_set() && !session.custom_fan_mode_set());
}

}  // namespace

int main() {
  test_every_admission_rejection_is_ordered_and_write_free();
  test_disconnect_clears_discovery_idle_and_entity_generation();
  test_reconnect_needs_fresh_response_and_recovers_after_rapid_disconnect();
  test_climate_fan_session_clears_on_disconnect_and_missing_manual_level();
  return 0;
}
