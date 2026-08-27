#include "heater.h"

#if defined(USE_ESP32) && defined(USE_CLIMATE)
#include "climate.h"
#endif

#if defined(USE_ESP32) && defined(USE_SENSOR)
#include "esphome/components/sensor/sensor.h"
#endif

#if defined(USE_ESP32) && defined(USE_NUMBER)
#include "esphome/components/number/number.h"
#endif

#if defined(USE_ESP32) && defined(USE_SWITCH)
#include "esphome/components/switch/switch.h"
#endif

#if defined(USE_ESP32) && defined(USE_TIME)
#include "esphome/components/time/real_time_clock.h"
#endif

#include <cmath>

#ifdef USE_ESP32

namespace esphome {
namespace diesel_heater_ble {

const char *TAG = "diesel_heater_ble";

namespace {

#if defined(USE_ESP32) && (defined(USE_SENSOR) || defined(USE_NUMBER))
template<typename T> float sensor_value(const T &value) { return static_cast<float>(value); }

template<> float sensor_value(const TemperatureUnit &value) {
  return static_cast<float>(static_cast<uint8_t>(value));
}

template<> float sensor_value(const AltitudeUnit &value) {
  return static_cast<float>(static_cast<uint8_t>(value));
}
#endif

#if defined(USE_ESP32) && defined(USE_SENSOR)
template<typename T> void publish_sensor_field(sensor::Sensor *sensor, const StateField<T> &field) {
  if (sensor == nullptr) {
    return;
  }
  if (!field.valid) {
    if (!std::isnan(sensor->state)) {
      sensor->publish_state(NAN);
    }
    return;
  }
  const float value = sensor_value(field.value);
  if (std::isnan(sensor->state) || sensor->state != value) {
    sensor->publish_state(value);
  }
}

void publish_timed_sensor_field(sensor::Sensor *sensor, const TimedField &field) {
  if (sensor == nullptr) {
    return;
  }
  if (!field.valid) {
    if (!std::isnan(sensor->state)) {
      sensor->publish_state(NAN);
    }
    return;
  }
  const float value = static_cast<float>(field.value);
  if (std::isnan(sensor->state) || sensor->state != value) {
    sensor->publish_state(value);
  }
}
#endif

#if defined(USE_ESP32) && defined(USE_NUMBER)
template<typename T> void publish_number_field(number::Number *number, const StateField<T> &field) {
  if (number == nullptr) {
    return;
  }
  if (!field.valid) {
    if (!std::isnan(number->state)) {
      number->publish_state(NAN);
    }
    return;
  }
  const float value = sensor_value(field.value);
  if (std::isnan(number->state) || number->state != value) {
    number->publish_state(value);
  }
}
#endif

}  // namespace

void DieselHeaterBLE::loop() {
  const uint32_t now = millis();
  this->dispatch_delivery_events_();
  this->delivery_.tick(now, [this](const CommandFrame &frame) {
    CommandFrame writable_frame = frame;
    return this->ble_write_chr(this->parent()->get_gattc_if(), this->parent()->get_remote_bda(), this->handle_,
                               writable_frame.data(), writable_frame.size());
  });
  this->dispatch_delivery_events_();
  this->log_delivery_failure_();
#if defined(USE_ESP32) && defined(USE_TIME)
  this->try_clock_sync_(now);
#endif
  if (this->lifecycle_.connected()) {
    if (now - this->last_request_ >= 1000) {
      const ProtocolDriver *driver = this->selection_.driver();
      if (driver != nullptr) {
        const CommandAdmission admission = this->lifecycle_.admit(capability_bits(Capability::STATUS), true);
        if (admission == CommandAdmission::READY) {
          const CommandBatch batch = driver->status(this->passkey_);
          const DeliveryEnqueueResult result = this->delivery_.enqueue_poll(batch);
          if (result == DeliveryEnqueueResult::QUEUED || result == DeliveryEnqueueResult::POLL_SUPPRESSED)
            this->last_request_ = now;
          this->log_delivery_enqueue_(result);
        } else {
          this->log_admission_rejection_(admission);
        }
      } else if (this->selection_.should_probe()) {
        // App-derived shared discovery probe: configured passkey, command 1,
        // value 0. It does not select a driver on its own.
        CommandBatch probe = this->selection_.discovery_probe(this->passkey_);
        const DeliveryEnqueueResult queued = this->delivery_.enqueue_control(probe);
        if (queued == DeliveryEnqueueResult::QUEUED) {
          this->selection_.record_probe();
          this->last_request_ = now;
        }
        this->log_delivery_enqueue_(queued);
      } else if (!this->discovery_timeout_logged_) {
        if (this->selection_.invalid_response_seen()) {
          this->log_lifecycle_("Protocol discovery failed: invalid response.");
        } else {
          this->log_lifecycle_("Protocol discovery timed out: no response.");
        }
        this->discovery_timeout_logged_ = true;
      }
    }
    if (this->lifecycle_.can_publish_state()) this->update_sensors(this->state_);
  }
}

void DieselHeaterBLE::gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                                          esp_ble_gattc_cb_param_t *param) {
  switch (event) {
    case ESP_GATTC_OPEN_EVT:
      this->log_lifecycle_("GATT client opened.");
      this->clear_connection_state_();
      this->lifecycle_.begin_connection();
      break;

    case ESP_GATTC_DISCONNECT_EVT:
      this->log_lifecycle_("GATT client disconnected.");
      this->node_state = esp32_ble_tracker::ClientState::DISCONNECTING;
      this->clear_connection_state_();
      break;

    case ESP_GATTC_SEARCH_CMPL_EVT:
      if (param->search_cmpl.status != ESP_GATT_OK) {
        ESP_LOGD(TAG, "Service search failed, status: %d", param->search_cmpl.status);
        this->clear_connection_state_();
        break;
      }
      if (!this->ble_register_for_notify(this->parent()->get_gattc_if(), this->parent()->get_remote_bda()))
        this->clear_connection_state_();
      break;

    case ESP_GATTC_REG_FOR_NOTIFY_EVT:
      if (param->reg_for_notify.status != ESP_GATT_OK) {
        ESP_LOGD(TAG, "Register for notify failed, status: %d", param->reg_for_notify.status);
        this->clear_connection_state_();
        break;
      }
      this->node_state = esp32_ble_tracker::ClientState::ESTABLISHED;
      this->lifecycle_.set_established();
      break;

    case ESP_GATTC_NOTIFY_EVT:
      if (param->notify.conn_id != this->parent()->get_conn_id())
        break;
      if (param->notify.handle == this->handle_) {
        std::vector<uint8_t> data(param->notify.value, param->notify.value + param->notify.value_len);
        this->on_notification_received(data);
      }
      break;

    case ESP_GATTC_WRITE_CHAR_EVT:
      if (param->write.conn_id == this->parent()->get_conn_id() && param->write.handle == this->handle_)
        this->delivery_.on_write_complete(millis(), param->write.status == ESP_GATT_OK);
      break;

    case ESP_GATTC_READ_CHAR_EVT:
      break;

    default:
      ESP_LOGD(TAG, "Unhandled GATT event: %d", event);
      break;
  }
}

bool DieselHeaterBLE::ble_write_chr(esp_gatt_if_t gattc_if, esp_bd_addr_t remote_bda, uint16_t handle, uint8_t *data,
                                    uint16_t len) {
  esp_err_t ret =
      esp_ble_gattc_write_char(gattc_if, this->parent()->get_conn_id(), handle, len, data, ESP_GATT_WRITE_TYPE_RSP,
                               ESP_GATT_AUTH_REQ_NONE);
  if (ret != ESP_OK) {
    ESP_LOGD(TAG, "Write characteristic failed, status: %d", ret);
    return false;
  } else {
  }
  return true;
}

bool DieselHeaterBLE::ble_read_chr(esp_gatt_if_t gattc_if, esp_bd_addr_t remote_bda, uint16_t handle) {
  esp_err_t ret = esp_ble_gattc_read_char(gattc_if, 0, handle, ESP_GATT_AUTH_REQ_NONE);
  if (ret != ESP_OK) {
    ESP_LOGD(TAG, "Read characteristic failed, status: %d", ret);
    return false;
  }
  return true;
}

bool DieselHeaterBLE::ble_register_for_notify(esp_gatt_if_t gattc_if, esp_bd_addr_t remote_bda) {
  auto *chr = this->parent()->get_characteristic(this->service_uuid_, this->characteristic_uuid_);
  if (!chr) {
    ESP_LOGD(TAG, "Characteristic not found.");
    return false;
  }
  this->handle_ = chr->handle;
  this->lifecycle_.set_characteristic_handle(this->handle_);
  esp_err_t ret = esp_ble_gattc_register_for_notify(gattc_if, remote_bda, this->handle_);
  if (ret != ESP_OK) {
    ESP_LOGD(TAG, "Register for notify failed, status: %d", ret);
    return false;
  }
  return true;
}

void DieselHeaterBLE::on_notification_received(const std::vector<uint8_t> &data) {
  if (!this->lifecycle_.transport_ready()) return;
  HeaterState parsed{};
  ParseResult result{};
  if (!this->selection_.parse(data, parsed, result)) {
    if (!this->discovery_response_failure_logged_) {
      ESP_LOGD(TAG, "Ignored response, parse result: %u", static_cast<unsigned>(result));
      this->discovery_response_failure_logged_ = true;
    }
    return;
  }
  this->state_ = parsed;
  const ProtocolDriver *driver = this->selection_.driver();
  if (driver == nullptr) {
    return;
  }
  this->lifecycle_.confirm_response(driver->capabilities());
  const uint32_t now = millis();
  this->delivery_.on_response(now, parsed.acknowledged_command.valid, parsed.acknowledged_command.value,
                              this->state_matches_current_command_(parsed));
  this->last_update_ = now;
}

EntityCommandResult DieselHeaterBLE::admit_command_(EntityOperation operation, float value) {
  const CommandAdmission admission =
      this->lifecycle_.admit(EntityControlPolicy::required_capabilities(operation), EntityControlPolicy::valid(operation, value));
  if (admission != CommandAdmission::READY) this->log_admission_rejection_(admission);
  return EntityControlPolicy::from_admission(admission);
}

EntityCommandResult DieselHeaterBLE::dispatch_command_(EntityCommandResult admission, const CommandBatch *batch,
                                                        uint32_t transaction_id) {
  if (admission != EntityCommandResult::QUEUED) return admission;
  if (batch == nullptr || !batch->sendable()) {
    if (batch != nullptr) this->log_batch_rejection_(batch->result());
    return batch == nullptr ? EntityCommandResult::DELIVERY_REJECTED : EntityControlPolicy::from_batch(batch->result());
  }
  const DeliveryEnqueueResult result = this->delivery_.enqueue_control(*batch, transaction_id);
  this->log_delivery_enqueue_(result);
  return EntityControlPolicy::from_delivery(result);
}

void DieselHeaterBLE::dispatch_delivery_events_() {
  DeliveryEvent event;
  while (this->delivery_.consume_event(event)) {
#if defined(USE_ESP32) && defined(USE_TIME)
    if (event.tag == kClockSyncDeliveryTag) {
      this->clock_sync_.delivery_completed(millis(), event.success);
      continue;
    }
#endif
#if defined(USE_ESP32) && defined(USE_CLIMATE)
    if (this->climate_ != nullptr) this->climate_->on_control_delivery(event.tag, event.success);
#endif
  }
}

#if defined(USE_ESP32) && defined(USE_TIME)
void DieselHeaterBLE::try_clock_sync_(uint32_t now) {
  if (this->time_ == nullptr) return;
  const auto current_time = this->time_->now();
  ClockTime clock_time{current_time.is_valid(), current_time.hour, current_time.minute};
  uint16_t minutes_since_midnight = 0;
  if (!ClockSyncRuntime::minutes_since_midnight(clock_time, minutes_since_midnight)) return;

  const ProtocolDriver *driver = this->selection_.driver();
  const bool ready = this->lifecycle_.can_publish_state() && this->state_.valid && driver != nullptr;
  const bool delivery_idle = this->delivery_.empty() && this->delivery_.phase() == DeliveryPhase::IDLE;
  if (!this->clock_sync_.should_enqueue(now, ready, delivery_idle, clock_time)) return;

  const CommandBatch batch = driver->clock(this->passkey_, minutes_since_midnight);
  const DeliveryEnqueueResult result = this->delivery_.enqueue_control(batch, kClockSyncDeliveryTag);
  if (result == DeliveryEnqueueResult::QUEUED) {
    this->clock_sync_.queued(now);
  } else {
    this->log_delivery_enqueue_(result);
  }
}
#endif

bool DieselHeaterBLE::state_matches_current_command_(const HeaterState &state) const {
  const CommandFrame *frame = this->delivery_.current_frame();
  if (frame == nullptr || !state.valid) return false;
  const AACommand command = static_cast<AACommand>((*frame)[4]);
  const uint16_t wire_value = static_cast<uint16_t>((*frame)[5]) | (static_cast<uint16_t>((*frame)[6]) << 8U);
  switch (command) {
    case AACommand::STATUS:
      return true;
    case AACommand::POWER:
      return state.power.valid && state.power.value == (wire_value != 0);
    case AACommand::MODE:
      return state.operating_mode.valid &&
             ((wire_value == 1 && state.operating_mode.value == OperatingMode::MANUAL_LEVEL) ||
              (wire_value == 2 && state.operating_mode.value == OperatingMode::THERMOSTAT));
    case AACommand::VALUE:
      if (state.operating_mode.valid && state.operating_mode.value == OperatingMode::MANUAL_LEVEL)
        return state.manual_level.valid && state.manual_level.value == wire_value;
      if (state.operating_mode.valid && state.operating_mode.value == OperatingMode::THERMOSTAT &&
          state.target_temperature_c.valid) {
        float expected_c = static_cast<float>(wire_value);
        if (state.reported_temperature_unit.valid && state.reported_temperature_unit.value == TemperatureUnit::FAHRENHEIT)
          expected_c = (expected_c - 32.0f) * (5.0f / 9.0f);
        return std::fabs(state.target_temperature_c.value - expected_c) < 0.01f;
      }
      return false;
    default:
      return false;
  }
}

void DieselHeaterBLE::log_admission_rejection_(CommandAdmission result) {
  const uint32_t now = millis();
  if (result == this->last_admission_rejection_ && now - this->last_rejection_log_ < 5000) return;
  this->last_admission_rejection_ = result;
  this->last_rejection_log_ = now;
  ESP_LOGD(TAG, "Command rejected, admission: %u", static_cast<unsigned>(result));
}

void DieselHeaterBLE::log_batch_rejection_(CommandResult result) {
  const uint32_t now = millis();
  if (result == this->last_batch_rejection_ && now - this->last_rejection_log_ < 5000) return;
  this->last_batch_rejection_ = result;
  this->last_rejection_log_ = now;
  ESP_LOGD(TAG, "Command rejected, result: %u", static_cast<unsigned>(result));
}

void DieselHeaterBLE::log_delivery_enqueue_(DeliveryEnqueueResult result) {
  if (result == DeliveryEnqueueResult::QUEUED) return;
  const uint32_t now = millis();
  if (result == this->last_delivery_enqueue_ && now - this->last_delivery_log_ < 5000) return;
  this->last_delivery_enqueue_ = result;
  this->last_delivery_log_ = now;
  ESP_LOGD(TAG, "Command delivery not queued, result: %u", static_cast<unsigned>(result));
}

void DieselHeaterBLE::log_delivery_failure_() {
  const DeliveryFailure failure = this->delivery_.last_failure();
  if (failure == DeliveryFailure::NONE || failure == this->last_delivery_failure_) return;
  const uint32_t now = millis();
  if (now - this->last_delivery_log_ < 1000) return;
  this->last_delivery_failure_ = failure;
  this->last_delivery_log_ = now;
  ESP_LOGD(TAG, "Command delivery failed, reason: %u", static_cast<unsigned>(failure));
}

void DieselHeaterBLE::log_lifecycle_(const char *message) {
  const uint32_t now = millis();
  if (now - this->last_lifecycle_log_ < 1000) return;
  this->last_lifecycle_log_ = now;
  ESP_LOGD(TAG, "%s", message);
}

void DieselHeaterBLE::clear_connection_state_() {
  this->delivery_.disconnect();
#if defined(USE_ESP32) && defined(USE_TIME)
  this->clock_sync_.reset_connection();
#endif
  this->lifecycle_.disconnect();
  const AvailabilityMask unavailable_entities = this->availability_.disconnect();
  this->handle_ = 0;
  this->selection_.reset();
  this->state_.invalidate();
  this->last_request_ = 0;
  this->last_update_ = 0;
  this->discovery_response_failure_logged_ = false;
  this->discovery_timeout_logged_ = false;
  this->last_admission_rejection_ = CommandAdmission::READY;
  this->last_batch_rejection_ = CommandResult::READY;
  this->last_delivery_enqueue_ = DeliveryEnqueueResult::QUEUED;
  this->last_delivery_failure_ = DeliveryFailure::NONE;
#if defined(USE_ESP32) && defined(USE_CLIMATE)
  if (this->climate_ != nullptr) this->climate_->on_heater_disconnect();
#endif
#if defined(USE_ESP32) && defined(USE_SWITCH)
  if (this->power_switch_ != nullptr) this->power_switch_->set_has_state(false);
#endif
  if (unavailable_entities != 0) this->publish_unavailable_entities_(unavailable_entities);
}

void DieselHeaterBLE::publish_unavailable_entities_(AvailabilityMask entities) {
  const auto unavailable = [entities](AvailabilityEntity identity, auto entity, const auto &publish) {
    if ((entities & availability_bit(identity)) != 0) publish(entity);
  };
#if defined(USE_ESP32) && defined(USE_SENSOR)
  const auto unavailable_sensor = [](sensor::Sensor *entity) {
    if (entity != nullptr) entity->publish_state(NAN);
  };
  unavailable(AvailabilityEntity::RUNNING_STATE, this->running_state_, unavailable_sensor);
  unavailable(AvailabilityEntity::ERROR_CODE, this->error_code_, unavailable_sensor);
  unavailable(AvailabilityEntity::RUNNING_STEP, this->running_step_, unavailable_sensor);
  unavailable(AvailabilityEntity::ALTITUDE, this->altitude_, unavailable_sensor);
  unavailable(AvailabilityEntity::RUNNING_MODE, this->running_mode_, unavailable_sensor);
  unavailable(AvailabilityEntity::SET_LEVEL, this->set_level_, unavailable_sensor);
  unavailable(AvailabilityEntity::SET_TEMP, this->set_temp_, unavailable_sensor);
  unavailable(AvailabilityEntity::SUPPLY_VOLTAGE, this->supply_voltage_, unavailable_sensor);
  unavailable(AvailabilityEntity::CASE_TEMP, this->case_temp_, unavailable_sensor);
  unavailable(AvailabilityEntity::CAB_TEMP, this->cab_temp_, unavailable_sensor);
  unavailable(AvailabilityEntity::START_TIME, this->start_time_, unavailable_sensor);
  unavailable(AvailabilityEntity::AUTO_TIME, this->auto_time_, unavailable_sensor);
  unavailable(AvailabilityEntity::RUN_TIME, this->run_time_, unavailable_sensor);
  unavailable(AvailabilityEntity::IS_AUTO, this->is_auto_, unavailable_sensor);
  unavailable(AvailabilityEntity::LANGUAGE, this->language_, unavailable_sensor);
  unavailable(AvailabilityEntity::TEMP_OFFSET, this->temp_offset_, unavailable_sensor);
  unavailable(AvailabilityEntity::TANK_VOLUME, this->tank_volume_, unavailable_sensor);
  unavailable(AvailabilityEntity::OIL_PUMP_TYPE, this->oil_pump_type_, unavailable_sensor);
  unavailable(AvailabilityEntity::RF433_ON_OFF, this->rf433_on_off_, unavailable_sensor);
  unavailable(AvailabilityEntity::TEMP_UNIT, this->temp_unit_, unavailable_sensor);
  unavailable(AvailabilityEntity::ALTITUDE_UNIT, this->altitude_unit_, unavailable_sensor);
  unavailable(AvailabilityEntity::AUTOMATIC_HEATING, this->automatic_heating_, unavailable_sensor);
#endif
#if defined(USE_ESP32) && defined(USE_NUMBER)
  const auto unavailable_number = [](number::Number *entity) {
    if (entity != nullptr) entity->publish_state(NAN);
  };
  unavailable(AvailabilityEntity::POWER_LEVEL_NUMBER, this->power_level_number_, unavailable_number);
  unavailable(AvailabilityEntity::SET_TEMP_NUMBER, this->set_temp_number_, unavailable_number);
#endif
}

#if defined(USE_ESP32) && defined(USE_CLIMATE)
void DieselHeaterBLE::update_climate(const HeaterState &new_state) {
  if (this->climate_ == nullptr) {
    return;
  }

  this->climate_->sync_from_heater(new_state);

  this->climate_->publish_state();
}
#endif

void DieselHeaterBLE::update_sensors(const HeaterState &new_state) {
  if (!new_state.valid || !this->lifecycle_.can_publish_state()) {
    return;
  }

#if defined(USE_ESP32) && defined(USE_CLIMATE)
  this->update_climate(new_state);
#endif

#if defined(USE_ESP32) && defined(USE_SENSOR)
  publish_sensor_field(this->running_state_, new_state.raw_running_state);
  publish_sensor_field(this->error_code_, new_state.error_code);
  publish_sensor_field(this->running_step_, new_state.raw_running_step);
  publish_sensor_field(this->altitude_, new_state.altitude_m);
  publish_sensor_field(this->running_mode_, new_state.raw_operating_mode);
  publish_sensor_field(this->set_level_, new_state.manual_level);
  publish_sensor_field(this->set_temp_, new_state.target_temperature_c);
  publish_sensor_field(this->supply_voltage_, new_state.supply_voltage_v);
  publish_sensor_field(this->case_temp_, new_state.case_temperature_c);
  publish_sensor_field(this->cab_temp_, new_state.cabin_temperature_c);
  publish_timed_sensor_field(this->start_time_, new_state.start_time);
  publish_timed_sensor_field(this->auto_time_, new_state.auto_time);
  publish_timed_sensor_field(this->run_time_, new_state.run_time);
  publish_sensor_field(this->is_auto_, new_state.automatic_start_stop_enabled);
  publish_sensor_field(this->language_, new_state.language);
  publish_sensor_field(this->temp_offset_, new_state.temperature_offset_c);
  publish_sensor_field(this->tank_volume_, new_state.tank_volume);
  publish_sensor_field(this->oil_pump_type_, new_state.oil_pump_type);
  publish_sensor_field(this->rf433_on_off_, new_state.rf433_enabled);
  publish_sensor_field(this->temp_unit_, new_state.reported_temperature_unit);
  publish_sensor_field(this->altitude_unit_, new_state.reported_altitude_unit);
  publish_sensor_field(this->automatic_heating_, new_state.automatic_heating_enabled);
#endif
#if defined(USE_ESP32) && defined(USE_NUMBER)
  publish_number_field(this->power_level_number_, new_state.manual_level);
  publish_number_field(this->set_temp_number_, new_state.target_temperature_c);
#endif

#if defined(USE_ESP32) && defined(USE_SWITCH)
  if (this->power_switch_ != nullptr && new_state.power.valid &&
      this->power_switch_->state != new_state.power.value) {
    this->power_switch_->publish_state(new_state.power.value);
  }
#endif
  this->lifecycle_.mark_entities_published();
  this->availability_.mark_published();
}

EntityCommandResult DieselHeaterBLE::on_power_level_number(float value, uint32_t transaction_id) {
  const ProtocolDriver *driver = this->selection_.driver();
  const EntityCommandResult admission = this->admit_command_(EntityOperation::MANUAL_LEVEL, value);
  if (admission != EntityCommandResult::QUEUED) {
    return this->dispatch_command_(admission, nullptr);
  }
  const CommandBatch batch = driver->manual_level(this->state_, this->passkey_, static_cast<uint8_t>(value));
  return this->dispatch_command_(admission, &batch, transaction_id);
}

EntityCommandResult DieselHeaterBLE::on_temp_number(float value, uint32_t transaction_id) {
  const ProtocolDriver *driver = this->selection_.driver();
  const EntityCommandResult admission = this->admit_command_(EntityOperation::TEMPERATURE_C, value);
  if (admission != EntityCommandResult::QUEUED) {
    return this->dispatch_command_(admission, nullptr);
  }
  const CommandBatch batch = driver->temperature_c(this->state_, this->passkey_, value);
  return this->dispatch_command_(admission, &batch, transaction_id);
}

EntityCommandResult DieselHeaterBLE::on_power_switch(bool state, uint32_t transaction_id) {
  const ProtocolDriver *driver = this->selection_.driver();
  const EntityCommandResult admission = this->admit_command_(EntityOperation::POWER);
  if (admission != EntityCommandResult::QUEUED) {
    return this->dispatch_command_(admission, nullptr);
  }
  const CommandBatch batch = driver->power(this->state_, this->passkey_, state);
  return this->dispatch_command_(admission, &batch, transaction_id);
}

#if defined(USE_ESP32) && defined(USE_BUTTON)
EntityCommandResult DieselHeaterBLE::on_level_up_button_press() {
  if (!this->state_.manual_level.valid) {
    const EntityCommandResult admission = this->admit_command_(EntityOperation::MANUAL_LEVEL);
    return admission == EntityCommandResult::QUEUED ? EntityCommandResult::STATE_UNAVAILABLE : admission;
  }
  return this->on_power_level_number(static_cast<float>(this->state_.manual_level.value) + 1.0f);
}

EntityCommandResult DieselHeaterBLE::on_level_down_button_press() {
  if (!this->state_.manual_level.valid) {
    const EntityCommandResult admission = this->admit_command_(EntityOperation::MANUAL_LEVEL);
    return admission == EntityCommandResult::QUEUED ? EntityCommandResult::STATE_UNAVAILABLE : admission;
  }
  return this->on_power_level_number(static_cast<float>(this->state_.manual_level.value) - 1.0f);
}

EntityCommandResult DieselHeaterBLE::on_temp_up_button_press() {
  if (!this->state_.target_temperature_c.valid) {
    const EntityCommandResult admission = this->admit_command_(EntityOperation::TEMPERATURE_C);
    return admission == EntityCommandResult::QUEUED ? EntityCommandResult::STATE_UNAVAILABLE : admission;
  }
  return this->on_temp_number(this->state_.target_temperature_c.value + 1.0f);
}

EntityCommandResult DieselHeaterBLE::on_temp_down_button_press() {
  if (!this->state_.target_temperature_c.valid) {
    const EntityCommandResult admission = this->admit_command_(EntityOperation::TEMPERATURE_C);
    return admission == EntityCommandResult::QUEUED ? EntityCommandResult::STATE_UNAVAILABLE : admission;
  }
  return this->on_temp_number(this->state_.target_temperature_c.value - 1.0f);
}
#endif

#if defined(USE_ESP32) && defined(USE_CLIMATE)
EntityCommandResult DieselHeaterBLE::admit_climate_strategy(ClimateStrategy strategy) const {
  return EntityControlPolicy::admit(this->lifecycle_, strategy);
}
#endif

}  // namespace diesel_heater_ble
}  // namespace esphome

#endif
