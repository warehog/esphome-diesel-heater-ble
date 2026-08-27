#include <cassert>

#include "climate_session.h"

using esphome::diesel_heater_ble::ClimateAutomationIntent;
using esphome::diesel_heater_ble::ClimateConfirmedProjection;
using esphome::diesel_heater_ble::ClimateControlRuntime;
using esphome::diesel_heater_ble::EntityCommandResult;

namespace {

ClimateAutomationIntent changed_intent(const ClimateAutomationIntent &base) {
  ClimateAutomationIntent candidate = base;
  candidate.heat_requested = true;
  candidate.auto_requested = false;
  candidate.target_temperature_c = 23.0f;
  candidate.level = 6;
  candidate.heat_transition_ms = 100;
  candidate.overshoot_start_ms = 50;
  candidate.level_update_ms = 100;
  return candidate;
}

void accept_and_commit(ClimateControlRuntime &session, const ClimateAutomationIntent &candidate, unsigned commands) {
  assert(session.begin(candidate));
  for (unsigned command = 0; command < commands; ++command)
    assert(session.accept(EntityCommandResult::QUEUED, [](uint32_t) {}));
  assert(session.commit());
}

void complete(ClimateControlRuntime &session) {
  const uint32_t id = session.transaction_id();
  while (session.transaction_active()) session.delivery_completed(id);
}

void test_every_runtime_climate_path_waits_for_its_delivery() {
  // OFF and manual-level changes enqueue one command; heat, fan-auto,
  // target, external start, and external fallback all enqueue power + value.
  for (const unsigned commands : {1U, 2U}) {
    ClimateControlRuntime session;
    const ClimateAutomationIntent candidate = changed_intent(session.intent());
    accept_and_commit(session, candidate, commands);
    assert(session.intent().target_temperature_c == 23.0f);
    assert(session.transaction_active());
    complete(session);
    assert(!session.transaction_active());
  }
}

void test_rejection_and_partial_enqueue_restore_intent_and_timers() {
  ClimateControlRuntime session;
  ClimateAutomationIntent baseline = session.intent();
  baseline.target_temperature_c = 20.0f;
  baseline.level = 2;
  baseline.level_update_ms = 7;
  session.set_without_command(baseline);

  const ClimateAutomationIntent candidate = changed_intent(baseline);
  assert(session.begin(candidate));
  assert(session.accept(EntityCommandResult::QUEUED, [](uint32_t) {}));
  uint32_t cancelled_id = 0;
  assert(!session.accept(EntityCommandResult::DELIVERY_QUEUE_FULL,
                         [&cancelled_id](uint32_t id) { cancelled_id = id; }));
  assert(cancelled_id != 0);
  assert(!session.transaction_active());
  assert(session.intent().target_temperature_c == 20.0f);
  assert(session.intent().level == 2);
  assert(session.intent().level_update_ms == 7);
}

void test_async_timeout_and_failure_roll_back_then_a_new_request_recovers() {
  ClimateControlRuntime session;
  ClimateAutomationIntent baseline = session.intent();
  baseline.heat_requested = true;
  baseline.level = 3;
  session.set_without_command(baseline);

  accept_and_commit(session, changed_intent(baseline), 2);
  const uint32_t failed_id = session.transaction_id();
  session.delivery_completed(failed_id);  // First command was confirmed.
  session.delivery_failed(failed_id);     // Second command timed out or failed.
  assert(!session.transaction_active());
  assert(session.intent().level == 3);
  assert(session.intent().target_temperature_c != 23.0f);

  ClimateAutomationIntent recovery = baseline;
  recovery.level = 4;
  accept_and_commit(session, recovery, 1);
  complete(session);
  assert(session.intent().level == 4);
}

void test_tagged_disconnect_failure_uses_the_same_runtime_rollback() {
  ClimateControlRuntime runtime;
  ClimateAutomationIntent baseline = runtime.intent();
  baseline.level = 2;
  runtime.set_without_command(baseline);
  ClimateAutomationIntent candidate = baseline;
  candidate.level = 8;
  accept_and_commit(runtime, candidate, 2);
  runtime.delivery_failed(runtime.transaction_id());  // Delivery disconnect event.
  assert(!runtime.transaction_active());
  assert(runtime.intent().level == 2);
}

void test_command_free_intent_never_claims_confirmed_entity_state() {
  ClimateControlRuntime session;
  ClimateAutomationIntent requested = session.intent();
  requested.target_temperature_c = 26.0f;
  requested.auto_requested = true;
  assert(session.begin(requested));
  assert(session.commit());
  // The session intentionally exposes automation intent only. Production
  // climate mode/target/action are assigned solely in sync_from_heater().
  assert(session.intent().target_temperature_c == 26.0f);
  assert(!session.transaction_active());
}

void test_external_input_admission_and_confirmed_projection_use_runtime_seam() {
  ClimateControlRuntime runtime;
  assert(!runtime.set_external_temperature(NAN, false, 1));
  assert(runtime.set_external_temperature(68.0f, true, 10));
  assert(runtime.external_temperature_c() > 19.9f && runtime.external_temperature_c() < 20.1f);
  assert(runtime.external_sample_recent(20, 20));
  assert(!runtime.external_sample_recent(31, 20));

  ClimateAutomationIntent candidate = runtime.intent();
  candidate.heat_requested = true;
  assert(runtime.begin(candidate));
  uint32_t cancelled_id = 0;
  assert(!runtime.accept(EntityCommandResult::CONNECTION_UNAVAILABLE,
                         [&cancelled_id](uint32_t id) { cancelled_id = id; }));
  assert(cancelled_id != 0 && !runtime.intent().heat_requested);

  assert(runtime.begin(candidate));
  assert(!runtime.accept(EntityCommandResult::STATE_UNAVAILABLE, [](uint32_t) {}));

  const ClimateConfirmedProjection off = ClimateControlRuntime::project_confirmed_state(
      false, true, true, true, true, 22.0f);
  assert(!off.mode_heat && !off.action_heating && off.target_available);
  const ClimateConfirmedProjection heat = ClimateControlRuntime::project_confirmed_state(
      true, true, true, true, true, 22.0f);
  assert(heat.mode_heat && heat.action_heating && heat.target_temperature_c == 22.0f);
  const ClimateConfirmedProjection invalid_target = ClimateControlRuntime::project_confirmed_state(
      true, true, false, false, true, 50.0f);
  assert(!invalid_target.target_available);
}

}  // namespace

int main() {
  test_every_runtime_climate_path_waits_for_its_delivery();
  test_rejection_and_partial_enqueue_restore_intent_and_timers();
  test_async_timeout_and_failure_roll_back_then_a_new_request_recovers();
  test_tagged_disconnect_failure_uses_the_same_runtime_rollback();
  test_command_free_intent_never_claims_confirmed_entity_state();
  test_external_input_admission_and_confirmed_projection_use_runtime_seam();
  return 0;
}
