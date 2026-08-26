#include <cassert>
#include <cstdint>
#include <vector>

#include "command_delivery.h"

using esphome::diesel_heater_ble::CommandBatch;
using esphome::diesel_heater_ble::CommandDelivery;
using esphome::diesel_heater_ble::CommandFrame;
using esphome::diesel_heater_ble::CommandResult;
using esphome::diesel_heater_ble::DeliveryEnqueueResult;
using esphome::diesel_heater_ble::DeliveryEvent;
using esphome::diesel_heater_ble::DeliveryFailure;
using esphome::diesel_heater_ble::DeliveryPhase;

namespace {

struct FakeClock {
  uint32_t now{0};
};

struct FakeTransport {
  bool accept_writes{true};
  std::vector<CommandFrame> writes{};

  bool write(const CommandFrame &frame) {
    if (!this->accept_writes) return false;
    this->writes.push_back(frame);
    return true;
  }
};

CommandBatch batch(std::initializer_list<uint8_t> commands) {
  CommandBatch result = CommandBatch::with_result(CommandResult::READY);
  for (const uint8_t command : commands) {
    CommandFrame frame{};
    frame[4] = command;
    assert(result.append(frame));
  }
  return result;
}

void tick(CommandDelivery &delivery, FakeClock &clock, FakeTransport &transport) {
  delivery.tick(clock.now, [&transport](const CommandFrame &frame) { return transport.write(frame); });
}

void complete(CommandDelivery &delivery, FakeClock &clock, bool success = true) {
  delivery.on_write_complete(clock.now, success);
}

void acknowledge(CommandDelivery &delivery, FakeClock &clock, uint8_t command) {
  delivery.on_response(clock.now, true, command, false);
}

void test_one_write_waits_for_completion_and_acknowledgement() {
  CommandDelivery delivery;
  FakeClock clock;
  FakeTransport transport;
  assert(delivery.enqueue_control(batch({3})) == DeliveryEnqueueResult::QUEUED);
  assert(delivery.enqueue_control(batch({4})) == DeliveryEnqueueResult::QUEUED);
  tick(delivery, clock, transport);
  assert(transport.writes.size() == 1 && transport.writes[0][4] == 3);
  assert(delivery.has_in_flight_write());
  tick(delivery, clock, transport);
  assert(transport.writes.size() == 1);  // Exactly one submitted GATT write.

  acknowledge(delivery, clock, 3);  // A response before WRITE_CHAR_EVT is stale.
  assert(delivery.queued_operations() == 2 && delivery.phase() == DeliveryPhase::WRITE_PENDING);
  complete(delivery, clock);
  assert(delivery.awaiting_acknowledgement());
  assert(delivery.queued_operations() == 2);  // Completion alone never dequeues.
  tick(delivery, clock, transport);
  assert(transport.writes.size() == 1);
  acknowledge(delivery, clock, 4);
  assert(delivery.queued_operations() == 2);  // Nonmatching acknowledgement is ignored.
  acknowledge(delivery, clock, 3);
  assert(delivery.queued_operations() == 1 && delivery.phase() == DeliveryPhase::IDLE);
  tick(delivery, clock, transport);
  assert(transport.writes.size() == 2 && transport.writes[1][4] == 4);
}

void test_fresh_state_is_command_and_timestamp_bound() {
  CommandDelivery delivery;
  FakeClock clock{100};
  FakeTransport transport;
  assert(delivery.enqueue_control(batch({4})) == DeliveryEnqueueResult::QUEUED);
  tick(delivery, clock, transport);
  complete(delivery, clock);
  delivery.on_response(99, false, 0, true);  // State observed before the GATT completion is stale.
  assert(delivery.awaiting_acknowledgement());
  delivery.on_response(100, false, 0, false);  // A fresh but nonmatching state cannot advance it.
  assert(delivery.awaiting_acknowledgement());
  delivery.on_response(101, false, 0, true);
  assert(delivery.empty() && delivery.phase() == DeliveryPhase::IDLE);
}

void test_retries_timeout_and_write_failure_are_bounded() {
  CommandDelivery delivery;
  FakeClock clock;
  FakeTransport transport;
  assert(delivery.enqueue_control(batch({3})) == DeliveryEnqueueResult::QUEUED);
  for (uint8_t attempt = 0; attempt <= CommandDelivery::kMaxRetries; ++attempt) {
    tick(delivery, clock, transport);
    complete(delivery, clock);
    clock.now += CommandDelivery::kAcknowledgementTimeoutMs;
    tick(delivery, clock, transport);
  }
  assert(delivery.empty());
  assert(delivery.last_failure() == DeliveryFailure::ACK_TIMEOUT);
  assert(transport.writes.size() == CommandDelivery::kMaxRetries + 1);

  CommandDelivery write_failure;
  FakeTransport failed_transport;
  failed_transport.accept_writes = false;
  for (uint8_t attempt = 0; attempt <= CommandDelivery::kMaxRetries; ++attempt) {
    if (attempt == 0) assert(write_failure.enqueue_control(batch({3})) == DeliveryEnqueueResult::QUEUED);
    tick(write_failure, clock, failed_transport);
  }
  assert(write_failure.empty());
  assert(write_failure.last_failure() == DeliveryFailure::WRITE_FAILED);
  assert(failed_transport.writes.empty());

  CommandDelivery completion_failure;
  FakeTransport accepted_transport;
  assert(completion_failure.enqueue_control(batch({3})) == DeliveryEnqueueResult::QUEUED);
  tick(completion_failure, clock, accepted_transport);
  complete(completion_failure, clock, false);
  assert(completion_failure.phase() == DeliveryPhase::IDLE && completion_failure.retries() == 1);
}

void test_overflow_dependent_ordering_and_poll_fairness() {
  CommandDelivery delivery;
  FakeClock clock;
  FakeTransport transport;
  for (std::size_t index = 0; index < CommandDelivery::kQueueCapacity; ++index)
    assert(delivery.enqueue_control(batch({static_cast<uint8_t>(index + 1)})) == DeliveryEnqueueResult::QUEUED);
  assert(delivery.enqueue_control(batch({9})) == DeliveryEnqueueResult::QUEUE_FULL);
  assert(delivery.enqueue_poll(batch({1})) == DeliveryEnqueueResult::POLL_SUPPRESSED);

  CommandDelivery dependent;
  assert(dependent.enqueue_control(batch({2, 4})) == DeliveryEnqueueResult::QUEUED);
  tick(dependent, clock, transport);
  assert(transport.writes.back()[4] == 2);
  complete(dependent, clock);
  acknowledge(dependent, clock, 2);
  tick(dependent, clock, transport);
  assert(transport.writes.back()[4] == 4);  // Value follows the acknowledged mode frame only.

  CommandDelivery polls;
  assert(polls.enqueue_poll(batch({1})) == DeliveryEnqueueResult::QUEUED);
  assert(polls.enqueue_control(batch({3})) == DeliveryEnqueueResult::QUEUED);
  tick(polls, clock, transport);
  assert(transport.writes.back()[4] == 3);  // A pending poll never overtakes a new control.

  CommandDelivery submitted_poll;
  assert(submitted_poll.enqueue_poll(batch({1})) == DeliveryEnqueueResult::QUEUED);
  tick(submitted_poll, clock, transport);
  assert(submitted_poll.phase() == DeliveryPhase::WRITE_PENDING);
  assert(submitted_poll.enqueue_poll(batch({1})) == DeliveryEnqueueResult::POLL_SUPPRESSED);
  assert(submitted_poll.enqueue_poll(batch({1})) == DeliveryEnqueueResult::POLL_SUPPRESSED);
  assert(submitted_poll.enqueue_control(batch({3})) == DeliveryEnqueueResult::QUEUED);
  assert(submitted_poll.queued_operations() == 2);  // Control sits directly behind the one poll.
  complete(submitted_poll, clock);
  assert(submitted_poll.enqueue_poll(batch({1})) == DeliveryEnqueueResult::POLL_SUPPRESSED);
  acknowledge(submitted_poll, clock, 1);
  tick(submitted_poll, clock, transport);
  assert(transport.writes.back()[4] == 3);

  CommandDelivery awaiting_poll;
  assert(awaiting_poll.enqueue_poll(batch({1})) == DeliveryEnqueueResult::QUEUED);
  tick(awaiting_poll, clock, transport);
  complete(awaiting_poll, clock);
  assert(awaiting_poll.phase() == DeliveryPhase::AWAITING_ACK);
  assert(awaiting_poll.enqueue_poll(batch({1})) == DeliveryEnqueueResult::POLL_SUPPRESSED);
  assert(awaiting_poll.enqueue_poll(batch({1})) == DeliveryEnqueueResult::POLL_SUPPRESSED);
  assert(awaiting_poll.enqueue_control(batch({4})) == DeliveryEnqueueResult::QUEUED);
  assert(awaiting_poll.queued_operations() == 2);
  acknowledge(awaiting_poll, clock, 1);
  tick(awaiting_poll, clock, transport);
  assert(transport.writes.back()[4] == 4);
}

void test_missing_write_completion_blocks_without_retry_and_disconnect_recovers() {
  CommandDelivery delivery;
  FakeClock clock{0xfffffff0u};
  FakeTransport transport;
  assert(delivery.enqueue_control(batch({3})) == DeliveryEnqueueResult::QUEUED);
  tick(delivery, clock, transport);
  assert(delivery.has_in_flight_write() && transport.writes.size() == 1);
  assert(delivery.enqueue_control(batch({4})) == DeliveryEnqueueResult::QUEUED);
  clock.now = static_cast<uint32_t>(clock.now + CommandDelivery::kWriteCompletionTimeoutMs);
  tick(delivery, clock, transport);
  assert(delivery.transport_blocked() && delivery.empty());
  assert(delivery.last_failure() == DeliveryFailure::WRITE_COMPLETION_TIMEOUT);
  assert(delivery.enqueue_control(batch({3})) == DeliveryEnqueueResult::TRANSPORT_BLOCKED);
  assert(delivery.enqueue_poll(batch({1})) == DeliveryEnqueueResult::TRANSPORT_BLOCKED);
  tick(delivery, clock, transport);
  assert(transport.writes.size() == 1);  // No retry can overlap an unknown pending write.

  delivery.disconnect();
  assert(delivery.empty() && delivery.phase() == DeliveryPhase::IDLE);
  assert(delivery.enqueue_control(batch({4})) == DeliveryEnqueueResult::QUEUED);
  tick(delivery, clock, transport);
  assert(transport.writes.size() == 2);  // A fresh connection generation recovers.
}

void test_millis_wraparound_and_disconnect_clear() {
  CommandDelivery delivery;
  FakeClock clock{0xfffffff0u};
  FakeTransport transport;
  assert(delivery.enqueue_control(batch({3})) == DeliveryEnqueueResult::QUEUED);
  tick(delivery, clock, transport);
  complete(delivery, clock);
  clock.now = 0x00000020u;
  tick(delivery, clock, transport);
  assert(delivery.awaiting_acknowledgement());
  clock.now = static_cast<uint32_t>(0xfffffff0u + CommandDelivery::kAcknowledgementTimeoutMs);
  tick(delivery, clock, transport);
  assert(delivery.phase() == DeliveryPhase::WRITE_PENDING && delivery.retries() == 1);

  assert(delivery.enqueue_control(batch({4})) == DeliveryEnqueueResult::QUEUED);
  delivery.disconnect();
  assert(delivery.empty() && delivery.phase() == DeliveryPhase::IDLE);
  assert(delivery.last_failure() == DeliveryFailure::DISCONNECTED);
  tick(delivery, clock, transport);
  assert(transport.writes.size() == 2);  // Disconnect did not submit retained work.
}

void test_tagged_controls_report_completion_and_async_failure() {
  CommandDelivery delivery;
  FakeClock clock;
  FakeTransport transport;
  assert(delivery.enqueue_control(batch({3}), 41) == DeliveryEnqueueResult::QUEUED);
  tick(delivery, clock, transport);
  complete(delivery, clock);
  acknowledge(delivery, clock, 3);
  DeliveryEvent event;
  assert(delivery.consume_event(event));
  assert(event.tag == 41 && event.success);

  assert(delivery.enqueue_control(batch({4}), 42) == DeliveryEnqueueResult::QUEUED);
  for (uint8_t attempt = 0; attempt <= CommandDelivery::kMaxRetries; ++attempt) {
    tick(delivery, clock, transport);
    complete(delivery, clock);
    clock.now += CommandDelivery::kAcknowledgementTimeoutMs;
    tick(delivery, clock, transport);
  }
  assert(delivery.consume_event(event));
  assert(event.tag == 42 && !event.success);
}

void test_partial_tagged_enqueue_can_be_cancelled_before_any_write() {
  CommandDelivery delivery;
  FakeClock clock;
  FakeTransport transport;
  assert(delivery.enqueue_control(batch({1})) == DeliveryEnqueueResult::QUEUED);
  assert(delivery.enqueue_control(batch({2})) == DeliveryEnqueueResult::QUEUED);
  assert(delivery.enqueue_control(batch({3})) == DeliveryEnqueueResult::QUEUED);
  assert(delivery.enqueue_control(batch({4}), 70) == DeliveryEnqueueResult::QUEUED);
  assert(delivery.enqueue_control(batch({5}), 70) == DeliveryEnqueueResult::QUEUE_FULL);
  assert(delivery.cancel_pending_tag(70) == 1);
  assert(delivery.queued_operations() == 3);
  tick(delivery, clock, transport);
  assert(transport.writes.size() == 1 && transport.writes[0][4] == 1);
}

void test_terminal_tagged_failure_cancels_later_members_but_keeps_unrelated_work() {
  CommandDelivery delivery;
  FakeClock clock;
  FakeTransport failed_transport;
  failed_transport.accept_writes = false;
  assert(delivery.enqueue_control(batch({3}), 71) == DeliveryEnqueueResult::QUEUED);
  assert(delivery.enqueue_control(batch({4}), 71) == DeliveryEnqueueResult::QUEUED);
  assert(delivery.enqueue_control(batch({5})) == DeliveryEnqueueResult::QUEUED);
  for (uint8_t attempt = 0; attempt <= CommandDelivery::kMaxRetries; ++attempt)
    tick(delivery, clock, failed_transport);
  DeliveryEvent event;
  assert(delivery.consume_event(event));
  assert(event.tag == 71 && !event.success);
  assert(!delivery.consume_event(event));  // Later member has no second failure event.
  assert(delivery.queued_operations() == 1);
  FakeTransport transport;
  tick(delivery, clock, transport);
  assert(transport.writes.size() == 1 && transport.writes[0][4] == 5);
}

void test_tagged_ack_timeout_write_timeout_and_disconnect_each_report_one_failure() {
  CommandDelivery ack_timeout;
  FakeClock clock;
  FakeTransport transport;
  assert(ack_timeout.enqueue_control(batch({3}), 72) == DeliveryEnqueueResult::QUEUED);
  assert(ack_timeout.enqueue_control(batch({4}), 72) == DeliveryEnqueueResult::QUEUED);
  for (uint8_t attempt = 0; attempt <= CommandDelivery::kMaxRetries; ++attempt) {
    tick(ack_timeout, clock, transport);
    complete(ack_timeout, clock);
    clock.now += CommandDelivery::kAcknowledgementTimeoutMs;
    tick(ack_timeout, clock, transport);
  }
  DeliveryEvent event;
  assert(ack_timeout.consume_event(event) && event.tag == 72 && !event.success);
  assert(!ack_timeout.consume_event(event));
  assert(ack_timeout.queued_operations() == 0);

  CommandDelivery write_timeout;
  FakeClock timeout_clock;
  assert(write_timeout.enqueue_control(batch({3}), 73) == DeliveryEnqueueResult::QUEUED);
  tick(write_timeout, timeout_clock, transport);
  timeout_clock.now += CommandDelivery::kWriteCompletionTimeoutMs;
  tick(write_timeout, timeout_clock, transport);
  assert(write_timeout.consume_event(event) && event.tag == 73 && !event.success);

  CommandDelivery disconnected;
  assert(disconnected.enqueue_control(batch({3}), 74) == DeliveryEnqueueResult::QUEUED);
  assert(disconnected.enqueue_control(batch({4}), 74) == DeliveryEnqueueResult::QUEUED);
  disconnected.disconnect();
  assert(disconnected.consume_event(event) && event.tag == 74 && !event.success);
  assert(!disconnected.consume_event(event));
}

void test_tagged_success_events_are_one_per_accepted_operation() {
  CommandDelivery delivery;
  FakeClock clock;
  FakeTransport transport;
  assert(delivery.enqueue_control(batch({3}), 75) == DeliveryEnqueueResult::QUEUED);
  assert(delivery.enqueue_control(batch({4}), 75) == DeliveryEnqueueResult::QUEUED);
  for (const uint8_t command : {3, 4}) {
    tick(delivery, clock, transport);
    complete(delivery, clock);
    acknowledge(delivery, clock, command);
  }
  DeliveryEvent event;
  assert(delivery.consume_event(event) && event.tag == 75 && event.success);
  assert(delivery.consume_event(event) && event.tag == 75 && event.success);
  assert(!delivery.consume_event(event));
}

}  // namespace

int main() {
  test_one_write_waits_for_completion_and_acknowledgement();
  test_fresh_state_is_command_and_timestamp_bound();
  test_retries_timeout_and_write_failure_are_bounded();
  test_overflow_dependent_ordering_and_poll_fairness();
  test_missing_write_completion_blocks_without_retry_and_disconnect_recovers();
  test_millis_wraparound_and_disconnect_clear();
  test_tagged_controls_report_completion_and_async_failure();
  test_partial_tagged_enqueue_can_be_cancelled_before_any_write();
  test_terminal_tagged_failure_cancels_later_members_but_keeps_unrelated_work();
  test_tagged_ack_timeout_write_timeout_and_disconnect_each_report_one_failure();
  test_tagged_success_events_are_one_per_accepted_operation();
  return 0;
}
