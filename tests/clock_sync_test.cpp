#include <cassert>
#include <cstdint>
#include <vector>

#include "clock_sync.h"
#include "command_delivery.h"
#include "protocol.h"

using esphome::diesel_heater_ble::ClockSyncRuntime;
using esphome::diesel_heater_ble::ClockTime;
using esphome::diesel_heater_ble::CommandBatch;
using esphome::diesel_heater_ble::CommandDelivery;
using esphome::diesel_heater_ble::CommandFrame;
using esphome::diesel_heater_ble::DeliveryEnqueueResult;
using esphome::diesel_heater_ble::ProtocolRegistry;
using esphome::diesel_heater_ble::ProtocolVariant;

namespace {

struct Transport {
  std::vector<CommandFrame> writes{};
  bool write(const CommandFrame &frame) {
    this->writes.push_back(frame);
    return true;
  }
};

CommandBatch status_batch() {
  const auto *driver = ProtocolRegistry::find(ProtocolVariant::MD1_AA55_PLAIN);
  assert(driver != nullptr);
  return driver->status(1234);
}

void test_minutes_since_midnight_and_clock_encoding() {
  uint16_t minutes = 9999;
  assert(!ClockSyncRuntime::minutes_since_midnight({false, 0, 0}, minutes));
  assert(ClockSyncRuntime::minutes_since_midnight({true, 0, 0}, minutes) && minutes == 0);
  assert(ClockSyncRuntime::minutes_since_midnight({true, 4, 15}, minutes) && minutes == 255);
  assert(ClockSyncRuntime::minutes_since_midnight({true, 4, 16}, minutes) && minutes == 256);
  assert(ClockSyncRuntime::minutes_since_midnight({true, 23, 59}, minutes) && minutes == 1439);
  assert(!ClockSyncRuntime::minutes_since_midnight({true, 24, 0}, minutes));
  assert(!ClockSyncRuntime::minutes_since_midnight({true, 0, 60}, minutes));

  const auto *driver = ProtocolRegistry::find(ProtocolVariant::MD1_AA55_PLAIN);
  assert(driver != nullptr);
  const auto zero = driver->clock(1234, 0);
  const auto before_boundary = driver->clock(1234, 255);
  const auto after_boundary = driver->clock(1234, 256);
  const auto end_of_day = driver->clock(1234, 1439);
  assert(zero.sendable() && zero.frame_at(0)->at(4) == 10 && zero.frame_at(0)->at(5) == 0 && zero.frame_at(0)->at(6) == 0);
  assert(before_boundary.sendable() && before_boundary.frame_at(0)->at(5) == 255 && before_boundary.frame_at(0)->at(6) == 0);
  assert(after_boundary.sendable() && after_boundary.frame_at(0)->at(5) == 0 && after_boundary.frame_at(0)->at(6) == 1);
  assert(end_of_day.sendable() && end_of_day.frame_at(0)->at(5) == 0x9f && end_of_day.frame_at(0)->at(6) == 0x05);
  assert(!driver->clock(1234, 1440).sendable());
}

void test_late_validity_acknowledgement_retry_rate_and_reconnect() {
  ClockSyncRuntime sync;
  const ClockTime invalid{false, 4, 16};
  const ClockTime valid{true, 4, 16};
  uint16_t minutes = 0;

  // A source becoming valid after connection produces one initial attempt.
  assert(!sync.should_enqueue(0, true, true, invalid));
  assert(!sync.should_enqueue(0, false, true, valid));
  assert(sync.should_enqueue(0, true, true, valid));
  assert(ClockSyncRuntime::minutes_since_midnight(valid, minutes) && minutes == 256);
  sync.queued(0);
  assert(sync.in_flight());
  assert(!sync.should_enqueue(1, true, true, valid));
  assert(!sync.initial_sync_complete());  // Write submission is not success.

  // An acknowledgement failure is nonfatal but cannot cause a tight retry loop.
  sync.delivery_completed(10, false);
  assert(!sync.initial_sync_complete());
  assert(!sync.should_enqueue(ClockSyncRuntime::kRetryIntervalMs - 1, true, true, valid));
  assert(sync.should_enqueue(ClockSyncRuntime::kRetryIntervalMs, true, true, valid));
  sync.queued(ClockSyncRuntime::kRetryIntervalMs);
  assert(!sync.initial_sync_complete());
  sync.delivery_completed(ClockSyncRuntime::kRetryIntervalMs + 7, true);
  assert(sync.initial_sync_complete());
  assert(sync.last_success_ms() == ClockSyncRuntime::kRetryIntervalMs + 7);

  // Only a delivered acknowledgement starts the conservative periodic timer.
  assert(!sync.should_enqueue(sync.last_success_ms() + ClockSyncRuntime::kPeriodicSyncIntervalMs - 1, true, true, valid));
  assert(sync.should_enqueue(sync.last_success_ms() + ClockSyncRuntime::kPeriodicSyncIntervalMs, true, true, valid));
  sync.reset_connection();
  assert(!sync.initial_sync_complete());
  assert(sync.should_enqueue(1, true, true, valid));
}

void test_periodic_failure_uses_retry_interval_then_success_resumes_periodic_schedule() {
  ClockSyncRuntime sync;
  const ClockTime valid{true, 4, 16};

  // Establish the initial successful synchronization.
  assert(sync.should_enqueue(0, true, true, valid));
  sync.queued(0);
  sync.delivery_completed(1, true);
  assert(sync.initial_sync_complete());

  // The due periodic attempt fails.  Its old successful timestamp is already
  // expired, but that failure may only retry after the retry interval.
  const uint32_t periodic_due = 1 + ClockSyncRuntime::kPeriodicSyncIntervalMs;
  assert(sync.should_enqueue(periodic_due, true, true, valid));
  sync.queued(periodic_due);
  sync.delivery_completed(periodic_due, false);
  assert(!sync.should_enqueue(periodic_due + ClockSyncRuntime::kRetryIntervalMs - 1, true, true, valid));
  assert(sync.should_enqueue(periodic_due + ClockSyncRuntime::kRetryIntervalMs, true, true, valid));

  // A successful retry returns the scheduler to the conservative hourly rate.
  const uint32_t retry_due = periodic_due + ClockSyncRuntime::kRetryIntervalMs;
  sync.queued(retry_due);
  sync.delivery_completed(retry_due + 1, true);
  const uint32_t retry_success = retry_due + 1;
  assert(!sync.should_enqueue(retry_success + ClockSyncRuntime::kPeriodicSyncIntervalMs - 1, true, true, valid));
  assert(sync.should_enqueue(retry_success + ClockSyncRuntime::kPeriodicSyncIntervalMs, true, true, valid));
}

void test_status_and_clock_never_overlap() {
  ClockSyncRuntime sync;
  CommandDelivery delivery;
  Transport transport;
  const ClockTime valid{true, 4, 16};
  const auto *driver = ProtocolRegistry::find(ProtocolVariant::MD1_AA55_PLAIN);
  assert(driver != nullptr);

  assert(delivery.enqueue_poll(status_batch()) == DeliveryEnqueueResult::QUEUED);
  delivery.tick(0, [&transport](const CommandFrame &frame) { return transport.write(frame); });
  assert(transport.writes.size() == 1 && transport.writes[0][4] == 1);
  assert(!sync.should_enqueue(0, true, delivery.empty(), valid));
  delivery.on_write_complete(0, true);
  delivery.on_response(1, true, 1, false);
  assert(delivery.empty());

  assert(sync.should_enqueue(1, true, delivery.empty(), valid));
  const uint16_t minutes = 256;
  assert(delivery.enqueue_control(driver->clock(1234, minutes), 0xffffffffU) == DeliveryEnqueueResult::QUEUED);
  sync.queued(1);
  delivery.tick(1, [&transport](const CommandFrame &frame) { return transport.write(frame); });
  assert(transport.writes.size() == 2 && transport.writes[1][4] == 10);
  delivery.on_write_complete(1, true);
  assert(!sync.initial_sync_complete());
  delivery.on_response(2, true, 10, false);
  esphome::diesel_heater_ble::DeliveryEvent event;
  assert(delivery.consume_event(event) && event.tag == 0xffffffffU && event.success);
  sync.delivery_completed(2, event.success);
  assert(sync.initial_sync_complete() && sync.last_success_ms() == 2);
}

}  // namespace

int main() {
  test_minutes_since_midnight_and_clock_encoding();
  test_late_validity_acknowledgement_retry_rate_and_reconnect();
  test_periodic_failure_uses_retry_interval_then_success_resumes_periodic_schedule();
  test_status_and_clock_never_overlap();
  return 0;
}
