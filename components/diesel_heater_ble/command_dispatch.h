#pragma once

#include "contracts.h"
#include "lifecycle.h"

namespace esphome {
namespace diesel_heater_ble {

enum class DispatchResult : uint8_t {
  WRITTEN,
  ADMISSION_REJECTED,
  BATCH_REJECTED,
};

// The only command-frame write gate. It is deliberately dependency-free so
// host tests can prove that every admission and batch rejection invokes no
// writer callback. The ESPHome component supplies the real BLE writer.
class CommandDispatchGate {
 public:
  template<typename Writer>
  static DispatchResult dispatch(CommandAdmission admission, const CommandBatch *batch, Writer &&writer) {
    if (admission != CommandAdmission::READY) return DispatchResult::ADMISSION_REJECTED;
    if (batch == nullptr || !batch->sendable()) return DispatchResult::BATCH_REJECTED;
    for (std::size_t index = 0; index < batch->size(); ++index) writer(*batch->frame_at(index));
    return DispatchResult::WRITTEN;
  }
};

}  // namespace diesel_heater_ble
}  // namespace esphome
