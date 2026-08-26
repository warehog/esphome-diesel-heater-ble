#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "contracts.h"

namespace esphome {
namespace diesel_heater_ble {

// A bounded, allocation-free command delivery state machine. The caller owns
// the GATT transport and calls tick() with its monotonic uint32_t clock. This
// deliberately keeps the production transport and the host fake on the same
// small seam without introducing another transport implementation.
enum class DeliveryEnqueueResult : uint8_t {
  QUEUED,
  QUEUE_FULL,
  INVALID_BATCH,
  POLL_SUPPRESSED,
  TRANSPORT_BLOCKED,
};

enum class DeliveryFailure : uint8_t {
  NONE,
  WRITE_FAILED,
  WRITE_COMPLETION_TIMEOUT,
  ACK_TIMEOUT,
  DISCONNECTED,
};

enum class DeliveryPhase : uint8_t {
  IDLE,
  WRITE_PENDING,
  AWAITING_ACK,
  TRANSPORT_BLOCKED,
};

class CommandDelivery {
 public:
  static constexpr std::size_t kQueueCapacity = 4;
  static constexpr uint8_t kMaxRetries = 2;
  static constexpr uint32_t kWriteCompletionTimeoutMs = 1000;
  static constexpr uint32_t kAcknowledgementTimeoutMs = 1500;

  DeliveryEnqueueResult enqueue_control(const CommandBatch &batch) {
    // A not-yet-written poll has no user-visible effect, so a newly requested
    // control replaces it. An already submitted poll remains in flight and is
    // allowed to finish; no second write may overlap it.
    if (this->phase_ == DeliveryPhase::IDLE) this->remove_pending_polls_();
    return this->enqueue_(batch, false);
  }

  // Permit one poll only when no operation exists. This avoids a polling
  // backlog: while a poll is submitted or awaiting acknowledgement, later
  // polls are suppressed and a new control is placed directly behind it.
  DeliveryEnqueueResult enqueue_poll(const CommandBatch &batch) {
    if (this->phase_ == DeliveryPhase::TRANSPORT_BLOCKED) return DeliveryEnqueueResult::TRANSPORT_BLOCKED;
    if (!this->empty() || this->phase_ != DeliveryPhase::IDLE) return DeliveryEnqueueResult::POLL_SUPPRESSED;
    return this->enqueue_(batch, true);
  }

  template<typename Writer> void tick(uint32_t now, Writer &&writer) {
    // A submitted write with no completion has an unknown controller outcome.
    // Retrying it could overlap an eventual GATT operation, so fail closed and
    // reject all further work until the connection is reset.
    if (this->phase_ == DeliveryPhase::WRITE_PENDING &&
        elapsed_(now, this->write_started_at_) >= kWriteCompletionTimeoutMs) {
      this->block_transport_(DeliveryFailure::WRITE_COMPLETION_TIMEOUT);
    }
    if (this->phase_ == DeliveryPhase::AWAITING_ACK && elapsed_(now, this->awaiting_since_) >= kAcknowledgementTimeoutMs) {
      this->retry_or_fail_(DeliveryFailure::ACK_TIMEOUT);
    }
    if (this->phase_ != DeliveryPhase::IDLE || this->empty()) return;

    const CommandFrame *frame = this->current_frame_();
    if (frame == nullptr) {
      this->finish_current_(DeliveryFailure::WRITE_FAILED);
      return;
    }
    this->write_started_at_ = now;
    if (writer(*frame)) {
      this->phase_ = DeliveryPhase::WRITE_PENDING;
    } else {
      this->retry_or_fail_(DeliveryFailure::WRITE_FAILED);
    }
  }

  // Only the ESP_GATTC_WRITE_CHAR_EVT completion (or the equivalent host fake
  // event) transitions a submitted write to acknowledgement waiting. A write
  // completion is transport confirmation only; it never advances the FIFO.
  void on_write_complete(uint32_t now, bool success) {
    if (this->phase_ != DeliveryPhase::WRITE_PENDING) return;
    if (!success) {
      this->retry_or_fail_(DeliveryFailure::WRITE_FAILED);
      return;
    }
    this->awaiting_since_ = now;
    this->phase_ = DeliveryPhase::AWAITING_ACK;
  }

  // response_command is the parsed controller acknowledgement. A fresh state
  // can independently acknowledge only the current frame, and only if it was
  // received after that frame's GATT completion. This makes delayed state from
  // an earlier operation unable to advance a new operation.
  void on_response(uint32_t response_at, bool response_command_valid, uint8_t response_command,
                   bool fresh_state_matches) {
    if (this->phase_ != DeliveryPhase::AWAITING_ACK || elapsed_(response_at, this->awaiting_since_) > 0x7fffffffu)
      return;
    const CommandFrame *frame = this->current_frame_();
    if (frame == nullptr) return;
    const bool command_matches = response_command_valid && response_command == (*frame)[4];
    if (!command_matches && !fresh_state_matches) return;
    this->advance_after_ack_();
  }

  // Ends the connection generation. Both queued and in-flight work belongs to
  // that generation and must not be retried after reconnect.
  void disconnect() {
    if (!this->empty() || this->phase_ != DeliveryPhase::IDLE) this->last_failure_ = DeliveryFailure::DISCONNECTED;
    this->head_ = 0;
    this->size_ = 0;
    this->phase_ = DeliveryPhase::IDLE;
    this->frame_index_ = 0;
    this->retries_ = 0;
    this->awaiting_since_ = 0;
    this->write_started_at_ = 0;
  }

  bool empty() const { return this->size_ == 0; }
  bool has_in_flight_write() const { return this->phase_ == DeliveryPhase::WRITE_PENDING; }
  bool awaiting_acknowledgement() const { return this->phase_ == DeliveryPhase::AWAITING_ACK; }
  bool transport_blocked() const { return this->phase_ == DeliveryPhase::TRANSPORT_BLOCKED; }
  DeliveryPhase phase() const { return this->phase_; }
  std::size_t queued_operations() const { return this->size_; }
  uint8_t retries() const { return this->retries_; }
  DeliveryFailure last_failure() const { return this->last_failure_; }
  uint32_t awaiting_since() const { return this->awaiting_since_; }
  const CommandFrame *current_frame() const { return this->current_frame_(); }

 private:
  struct Operation {
    CommandBatch batch{};
    bool poll{false};
  };

  static uint32_t elapsed_(uint32_t now, uint32_t then) { return now - then; }

  DeliveryEnqueueResult enqueue_(const CommandBatch &batch, bool poll) {
    if (this->phase_ == DeliveryPhase::TRANSPORT_BLOCKED) return DeliveryEnqueueResult::TRANSPORT_BLOCKED;
    if (!batch.sendable()) return DeliveryEnqueueResult::INVALID_BATCH;
    if (this->size_ == kQueueCapacity) return DeliveryEnqueueResult::QUEUE_FULL;
    const std::size_t tail = (this->head_ + this->size_) % kQueueCapacity;
    this->queue_[tail] = {batch, poll};
    ++this->size_;
    return DeliveryEnqueueResult::QUEUED;
  }

  void remove_pending_polls_() {
    if (this->empty()) return;
    std::array<Operation, kQueueCapacity> retained{};
    std::size_t retained_size = 0;
    for (std::size_t offset = 0; offset < this->size_; ++offset) {
      const Operation &operation = this->queue_[(this->head_ + offset) % kQueueCapacity];
      if (!operation.poll) retained[retained_size++] = operation;
    }
    this->queue_ = retained;
    this->head_ = 0;
    this->size_ = retained_size;
    this->frame_index_ = 0;
  }

  const CommandFrame *current_frame_() const {
    if (this->empty()) return nullptr;
    return this->queue_[this->head_].batch.frame_at(this->frame_index_);
  }

  void retry_or_fail_(DeliveryFailure failure) {
    if (this->retries_ < kMaxRetries) {
      ++this->retries_;
      this->phase_ = DeliveryPhase::IDLE;
      return;
    }
    this->finish_current_(failure);
  }

  void advance_after_ack_() {
    const CommandBatch &batch = this->queue_[this->head_].batch;
    ++this->frame_index_;
    this->retries_ = 0;
    this->phase_ = DeliveryPhase::IDLE;
    if (this->frame_index_ >= batch.size()) this->pop_current_();
  }

  void finish_current_(DeliveryFailure failure) {
    this->last_failure_ = failure;
    this->phase_ = DeliveryPhase::IDLE;
    this->retries_ = 0;
    this->pop_current_();
  }

  void block_transport_(DeliveryFailure failure) {
    this->last_failure_ = failure;
    this->head_ = 0;
    this->size_ = 0;
    this->frame_index_ = 0;
    this->retries_ = 0;
    this->phase_ = DeliveryPhase::TRANSPORT_BLOCKED;
  }

  void pop_current_() {
    if (this->empty()) return;
    this->head_ = (this->head_ + 1) % kQueueCapacity;
    --this->size_;
    this->frame_index_ = 0;
  }

  std::array<Operation, kQueueCapacity> queue_{};
  std::size_t head_{0};
  std::size_t size_{0};
  uint8_t frame_index_{0};
  uint8_t retries_{0};
  DeliveryPhase phase_{DeliveryPhase::IDLE};
  DeliveryFailure last_failure_{DeliveryFailure::NONE};
  uint32_t write_started_at_{0};
  uint32_t awaiting_since_{0};
};

static_assert(CommandDelivery::kQueueCapacity > 0, "command delivery must be bounded");
static_assert(CommandDelivery::kMaxRetries > 0, "command delivery needs bounded retry");
static_assert(CommandDelivery::kWriteCompletionTimeoutMs > 0, "submitted writes need a bounded completion wait");

}  // namespace diesel_heater_ble
}  // namespace esphome
