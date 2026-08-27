#include <cassert>

#include "entity_control.h"

using esphome::diesel_heater_ble::Capability;
using esphome::diesel_heater_ble::Capabilities;
using esphome::diesel_heater_ble::ClimateStrategy;
using esphome::diesel_heater_ble::CommandResult;
using esphome::diesel_heater_ble::DeliveryEnqueueResult;
using esphome::diesel_heater_ble::EntityCommandResult;
using esphome::diesel_heater_ble::EntityControlPolicy;
using esphome::diesel_heater_ble::EntityOperation;
using esphome::diesel_heater_ble::LifecycleSession;
using esphome::diesel_heater_ble::capability_bits;
using esphome::diesel_heater_ble::entity_command_accepted;

namespace {

constexpr Capabilities kPower = capability_bits(Capability::POWER);
constexpr Capabilities kManual = capability_bits(Capability::MODE) | Capability::MANUAL_LEVEL;
constexpr Capabilities kTemperature = capability_bits(Capability::MODE) | Capability::SET_TEMPERATURE;
constexpr Capabilities kTemperatureOffset = capability_bits(Capability::EXTENDED_SETTINGS);
constexpr Capabilities kAllEntityControls = kPower | kManual | kTemperature | kTemperatureOffset;

LifecycleSession ready_session(Capabilities capabilities = kAllEntityControls) {
  LifecycleSession session;
  session.begin_connection();
  session.set_characteristic_handle(0x0042);
  session.set_established();
  session.confirm_response(capabilities);
  return session;
}

void test_each_entity_operation_is_normalized_and_admitted() {
  LifecycleSession session = ready_session();
  assert(EntityControlPolicy::admit(session, EntityOperation::POWER) == EntityCommandResult::QUEUED);
  assert(EntityControlPolicy::admit(session, EntityOperation::MANUAL_LEVEL, 1.0f) == EntityCommandResult::QUEUED);
  assert(EntityControlPolicy::admit(session, EntityOperation::MANUAL_LEVEL, 10.0f) == EntityCommandResult::QUEUED);
  assert(EntityControlPolicy::admit(session, EntityOperation::TEMPERATURE_C, 8.0f) == EntityCommandResult::QUEUED);
  assert(EntityControlPolicy::admit(session, EntityOperation::TEMPERATURE_C, 36.0f) == EntityCommandResult::QUEUED);
  assert(EntityControlPolicy::admit(session, EntityOperation::TEMPERATURE_OFFSET_C, -5.0f) == EntityCommandResult::QUEUED);
  assert(EntityControlPolicy::admit(session, EntityOperation::TEMPERATURE_OFFSET_C, 5.0f) == EntityCommandResult::QUEUED);

  assert(EntityControlPolicy::admit(session, EntityOperation::MANUAL_LEVEL, 0.0f) == EntityCommandResult::INVALID_VALUE);
  assert(EntityControlPolicy::admit(session, EntityOperation::MANUAL_LEVEL, 10.5f) == EntityCommandResult::INVALID_VALUE);
  assert(EntityControlPolicy::admit(session, EntityOperation::TEMPERATURE_C, 7.99f) == EntityCommandResult::INVALID_VALUE);
  assert(EntityControlPolicy::admit(session, EntityOperation::TEMPERATURE_C, 36.01f) == EntityCommandResult::INVALID_VALUE);
  assert(EntityControlPolicy::admit(session, EntityOperation::TEMPERATURE_OFFSET_C, -5.01f) == EntityCommandResult::INVALID_VALUE);
  assert(EntityControlPolicy::admit(session, EntityOperation::TEMPERATURE_OFFSET_C, 5.01f) == EntityCommandResult::INVALID_VALUE);
  assert(EntityControlPolicy::admit(session, EntityOperation::TEMPERATURE_OFFSET_C, 0.5f) == EntityCommandResult::INVALID_VALUE);
}

void test_every_entity_operation_rejects_disconnected_stale_and_unsupported_state() {
  for (const EntityOperation operation : {EntityOperation::POWER, EntityOperation::MANUAL_LEVEL,
                                          EntityOperation::TEMPERATURE_C, EntityOperation::TEMPERATURE_OFFSET_C}) {
    const float value = operation == EntityOperation::MANUAL_LEVEL ? 5.0f
                        : operation == EntityOperation::TEMPERATURE_OFFSET_C ? 0.0f
                                                                           : 20.0f;
    LifecycleSession disconnected;
    assert(EntityControlPolicy::admit(disconnected, operation, value) == EntityCommandResult::CONNECTION_UNAVAILABLE);

    LifecycleSession stale;
    stale.begin_connection();
    stale.set_characteristic_handle(0x0042);
    stale.set_established();
    stale.select_driver(kAllEntityControls);
    assert(EntityControlPolicy::admit(stale, operation, value) == EntityCommandResult::STATE_UNAVAILABLE);
  }

  assert(EntityControlPolicy::admit(ready_session(kManual), EntityOperation::POWER) ==
         EntityCommandResult::CAPABILITY_UNAVAILABLE);
  assert(EntityControlPolicy::admit(ready_session(kPower | kTemperature), EntityOperation::MANUAL_LEVEL, 5.0f) ==
         EntityCommandResult::CAPABILITY_UNAVAILABLE);
  assert(EntityControlPolicy::admit(ready_session(kPower | kManual), EntityOperation::TEMPERATURE_C, 20.0f) ==
         EntityCommandResult::CAPABILITY_UNAVAILABLE);
  assert(EntityControlPolicy::admit(ready_session(kPower | kManual), EntityOperation::TEMPERATURE_OFFSET_C, 0.0f) ==
         EntityCommandResult::CAPABILITY_UNAVAILABLE);
}

void test_climate_sequences_are_capability_gated_before_their_first_action() {
  LifecycleSession all = ready_session();
  assert(EntityControlPolicy::admit(all, ClimateStrategy::INTERNAL_THERMOSTAT) == EntityCommandResult::QUEUED);
  assert(EntityControlPolicy::admit(all, ClimateStrategy::EXTERNAL_STAGED) == EntityCommandResult::QUEUED);
  assert(EntityControlPolicy::admit(all, ClimateStrategy::MANUAL_LEVEL) == EntityCommandResult::QUEUED);

  assert(EntityControlPolicy::admit(ready_session(kPower | kManual), ClimateStrategy::INTERNAL_THERMOSTAT) ==
         EntityCommandResult::CAPABILITY_UNAVAILABLE);
  assert(EntityControlPolicy::admit(ready_session(kPower | kTemperature), ClimateStrategy::EXTERNAL_STAGED) ==
         EntityCommandResult::CAPABILITY_UNAVAILABLE);
  assert(EntityControlPolicy::admit(ready_session(kPower), ClimateStrategy::MANUAL_LEVEL) ==
         EntityCommandResult::CAPABILITY_UNAVAILABLE);

  LifecycleSession disconnected;
  assert(EntityControlPolicy::admit(disconnected, ClimateStrategy::EXTERNAL_STAGED) ==
         EntityCommandResult::CONNECTION_UNAVAILABLE);
}

void test_non_sendable_batches_and_delivery_failures_are_explicit_and_non_confirming() {
  assert(EntityControlPolicy::from_batch(CommandResult::NO_CHANGE) == EntityCommandResult::NO_CHANGE);
  assert(EntityControlPolicy::from_batch(CommandResult::UNSUPPORTED) == EntityCommandResult::UNSUPPORTED);
  assert(EntityControlPolicy::from_batch(CommandResult::STATE_UNAVAILABLE) == EntityCommandResult::STATE_UNAVAILABLE);
  assert(EntityControlPolicy::from_batch(CommandResult::INVALID_VALUE) == EntityCommandResult::INVALID_VALUE);
  assert(EntityControlPolicy::from_delivery(DeliveryEnqueueResult::QUEUE_FULL) == EntityCommandResult::DELIVERY_QUEUE_FULL);
  assert(EntityControlPolicy::from_delivery(DeliveryEnqueueResult::TRANSPORT_BLOCKED) ==
         EntityCommandResult::DELIVERY_TRANSPORT_BLOCKED);
  assert(entity_command_accepted(EntityCommandResult::QUEUED));
  assert(entity_command_accepted(EntityCommandResult::NO_CHANGE));
  assert(!entity_command_accepted(EntityCommandResult::UNSUPPORTED));
  assert(!entity_command_accepted(EntityCommandResult::DELIVERY_QUEUE_FULL));
}

}  // namespace

int main() {
  test_each_entity_operation_is_normalized_and_admitted();
  test_every_entity_operation_rejects_disconnected_stale_and_unsupported_state();
  test_climate_sequences_are_capability_gated_before_their_first_action();
  test_non_sendable_batches_and_delivery_failures_are_explicit_and_non_confirming();
  return 0;
}
