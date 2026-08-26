#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace esphome {
namespace diesel_heater_ble {

// Stable identifiers for the four AA-family response formats. These names do
// not select or enable a runtime parser; T04 performs that migration.
enum class ProtocolVariant : uint8_t {
  UNKNOWN = 0,
  MD1_AA55_PLAIN,
  MD2_AA55_ENCRYPTED,
  MD3_AA66_PLAIN,
  MD4_AA66_ENCRYPTED,
};

enum class ParseResult : uint8_t {
  OK,
  INCOMPLETE,
  UNKNOWN_PROTOCOL,
  INVALID_FRAME,
  UNSUPPORTED_MESSAGE,
};

// Command construction always returns an explicit outcome.
enum class CommandResult : uint8_t {
  READY,
  NO_CHANGE,
  UNSUPPORTED,
  STATE_UNAVAILABLE,
  INVALID_VALUE,
};

enum class Capability : uint16_t {
  NONE = 0,
  STATUS = 1U << 0,
  POWER = 1U << 1,
  MODE = 1U << 2,
  MANUAL_LEVEL = 1U << 3,
  SET_TEMPERATURE = 1U << 4,
  CLOCK = 1U << 5,
  EXTENDED_SETTINGS = 1U << 6,
};

using Capabilities = uint16_t;

constexpr Capabilities capability_bits(Capability capability) {
  return static_cast<Capabilities>(capability);
}

constexpr Capabilities operator|(Capability lhs, Capability rhs) {
  return capability_bits(lhs) | capability_bits(rhs);
}

constexpr Capabilities operator|(Capabilities lhs, Capability rhs) {
  return lhs | capability_bits(rhs);
}

constexpr bool has_capability(Capabilities capabilities, Capability capability) {
  return (capabilities & capability_bits(capability)) != 0;
}

constexpr Capabilities kCoreCapabilities =
    Capability::STATUS | Capability::POWER | Capability::MODE | Capability::MANUAL_LEVEL |
    Capability::SET_TEMPERATURE | Capability::CLOCK;

// App-derived matrix from AirHeaterBLE 1.20. "No" means unavailable through
// that app UI, not proof that a heater firmware rejects the command.
constexpr Capabilities capabilities_for(ProtocolVariant variant) {
  switch (variant) {
    case ProtocolVariant::MD1_AA55_PLAIN:
    case ProtocolVariant::MD3_AA66_PLAIN:
      return kCoreCapabilities;
    case ProtocolVariant::MD2_AA55_ENCRYPTED:
    case ProtocolVariant::MD4_AA66_ENCRYPTED:
      return kCoreCapabilities | Capability::EXTENDED_SETTINGS;
    case ProtocolVariant::UNKNOWN:
      return capability_bits(Capability::NONE);
  }
  return capability_bits(Capability::NONE);
}

// The app-derived state rule maps raw running states 0 and 3 to off and every
// other raw state to on. T03 retains the raw byte as a diagnostic even though
// this normalized Boolean is sufficient for power control.
constexpr bool power_from_raw_running_state(uint8_t raw_running_state) {
  return raw_running_state != 0 && raw_running_state != 3;
}

// The response format uses raw mode 1 for manual level and raw mode 2 for the
// heater thermostat. Other raw values, including 0, are diagnostic-only until
// stronger evidence defines a universal control meaning.
enum class OperatingMode : uint8_t {
  UNKNOWN = 0,
  MANUAL_LEVEL,
  THERMOSTAT,
};

constexpr OperatingMode operating_mode_from_raw(uint8_t raw_operating_mode) {
  switch (raw_operating_mode) {
    case 1:
      return OperatingMode::MANUAL_LEVEL;
    case 2:
      return OperatingMode::THERMOSTAT;
    default:
      return OperatingMode::UNKNOWN;
  }
}

constexpr bool is_normalized_operating_mode(OperatingMode mode) {
  return mode == OperatingMode::MANUAL_LEVEL || mode == OperatingMode::THERMOSTAT;
}

// AA-family requests are always eight bytes. A current command operation can
// require a mode frame followed by a value frame, so the fixed batch owns two
// frames. It has no vector or other dynamically allocated storage and is safe
// to construct on the stack or as component-owned storage.
constexpr std::size_t kCommandFrameSize = 8;
constexpr std::size_t kCommandBatchCapacity = 2;
using CommandFrame = std::array<uint8_t, kCommandFrameSize>;

class CommandBatch {
 public:
  constexpr CommandBatch() = default;

  static constexpr CommandBatch with_result(CommandResult result) {
    CommandBatch batch;
    batch.result_ = result;
    return batch;
  }

  constexpr CommandResult result() const { return this->result_; }
  constexpr std::size_t size() const { return this->size_; }
  constexpr bool empty() const { return this->size_ == 0; }
  constexpr bool ready() const { return this->result_ == CommandResult::READY; }

  // A transport may send a batch only when this predicate holds. READY alone
  // is insufficient because it can describe a builder which has not appended
  // a frame yet.
  constexpr bool sendable() const {
    return this->ready() && this->size_ > 0 && this->size_ <= kCommandBatchCapacity;
  }

  constexpr bool append(const CommandFrame &frame) {
    if (!this->ready() || this->size_ >= kCommandBatchCapacity) {
      return false;
    }
    this->frames_[this->size_++] = frame;
    return true;
  }

  constexpr const CommandFrame *frame_at(std::size_t index) const {
    return index < this->size_ ? &this->frames_[index] : nullptr;
  }

 private:
  CommandResult result_{CommandResult::STATE_UNAVAILABLE};
  uint8_t size_{0};
  std::array<CommandFrame, kCommandBatchCapacity> frames_{};
};

template<typename T> struct StateField {
  T value{};
  bool valid{false};

  constexpr void invalidate() {
    this->value = T{};
    this->valid = false;
  }
};

enum class TemperatureUnit : uint8_t {
  UNKNOWN = 0,
  CELSIUS,
  FAHRENHEIT,
};

enum class AltitudeUnit : uint8_t {
  UNKNOWN = 0,
  METRES,
  IMPERIAL,
};

// The app exposes three 16-bit timer observations but does not establish their
// physical unit. Preserve their value with this explicit protocol-tick unit
// rather than inventing seconds or minutes. A later evidence-backed parser may
// select a concrete unit without changing the field's ownership or validity.
enum class TimeValueUnit : uint8_t {
  PROTOCOL_TICKS = 0,
  MINUTES_SINCE_MIDNIGHT,
  MINUTES_DURATION,
};

struct TimedField {
  uint16_t value{0};
  TimeValueUnit unit{TimeValueUnit::PROTOCOL_TICKS};
  bool valid{false};

  constexpr void invalidate() {
    this->value = 0;
    this->unit = TimeValueUnit::PROTOCOL_TICKS;
    this->valid = false;
  }
};

// Values in this state are normalized before entity consumers observe them:
// temperatures are Celsius, voltage is volts, altitude is metres, and level
// is the displayed manual level (1-10). A Fahrenheit temperature offset is
// converted as a delta (F * 5 / 9) and retained as a float without rounding.
// A field with valid == false is unavailable and its value must not be read as
// a substitute value.
struct NormalizedHeaterState {
  bool valid{false};
  ProtocolVariant variant{ProtocolVariant::UNKNOWN};

  // Universal, entity-facing controls.
  StateField<bool> power{};
  StateField<OperatingMode> operating_mode{};
  StateField<uint8_t> manual_level{};
  StateField<float> target_temperature_c{};

  // Common observations.
  StateField<uint8_t> error_code{};
  StateField<float> case_temperature_c{};
  StateField<float> cabin_temperature_c{};
  StateField<float> supply_voltage_v{};
  StateField<uint16_t> altitude_m{};
  StateField<float> temperature_offset_c{};

  // Extended encrypted-format observations. Each remains unavailable on a
  // plain response rather than retaining a previous encrypted value.
  TimedField start_time{};
  TimedField auto_time{};
  TimedField run_time{};
  StateField<bool> automatic_start_stop_enabled{};
  StateField<uint8_t> language{};
  StateField<uint8_t> tank_volume{};
  StateField<uint8_t> oil_pump_type{};
  StateField<bool> rf433_enabled{};
  StateField<TemperatureUnit> reported_temperature_unit{};
  StateField<AltitudeUnit> reported_altitude_unit{};
  StateField<bool> automatic_heating_enabled{};

  // Raw protocol diagnostics are not universal controls. In particular, the
  // protocol has no evidence-backed universal running-step meaning.
  StateField<uint8_t> acknowledged_command{};
  StateField<uint8_t> raw_running_state{};
  StateField<uint8_t> raw_operating_mode{};
  StateField<uint8_t> raw_running_step{};

  constexpr void invalidate() { *this = NormalizedHeaterState{}; }
};

static_assert(sizeof(CommandFrame) == kCommandFrameSize, "AA command frames must remain eight bytes");
static_assert(kCommandBatchCapacity >= 2, "mode and value commands need a two-frame batch");
static_assert(sizeof(CommandBatch) <= 32, "command batches must remain small stack-owned values");
static_assert(std::is_trivially_copyable<CommandFrame>::value, "command frames must have value ownership");
static_assert(std::is_trivially_copyable<CommandBatch>::value, "command batches must not own heap storage");
static_assert(std::is_same<decltype(NormalizedHeaterState::temperature_offset_c.value), float>::value,
              "temperature offset must preserve fractional Celsius values");
static_assert(std::is_trivially_copyable<NormalizedHeaterState>::value,
              "normalized state must support whole-value replacement");

}  // namespace diesel_heater_ble
}  // namespace esphome
