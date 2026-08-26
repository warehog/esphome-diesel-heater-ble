#include "heater.h"
#include "climate.h"

#include <cmath>

#ifdef USE_ESP32

namespace esphome {
namespace diesel_heater_ble {

const char *TAG = "diesel_heater_ble";

namespace {

template<typename T> float sensor_value(const T &value) { return static_cast<float>(value); }

template<> float sensor_value(const TemperatureUnit &value) {
  return static_cast<float>(static_cast<uint8_t>(value));
}

template<> float sensor_value(const AltitudeUnit &value) {
  return static_cast<float>(static_cast<uint8_t>(value));
}

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

}  // namespace

void DieselHeaterBLE::loop() {
  if (this->lifecycle_.connected()) {
    if (this->last_request_ + 1000 < millis()) {
      this->last_request_ = millis();
      const ProtocolDriver *driver = this->selection_.driver();
      if (driver != nullptr) {
        const CommandAdmission admission = this->admit_command_(capability_bits(Capability::STATUS), true);
        if (admission == CommandAdmission::READY) {
          const CommandBatch batch = driver->status(this->passkey_);
          this->dispatch_command_(admission, &batch);
        } else {
          this->dispatch_command_(admission, nullptr);
        }
      } else if (this->selection_.should_probe()) {
        // App-derived shared discovery probe: configured passkey, command 1,
        // value 0. It does not select a driver on its own.
        CommandBatch probe = this->selection_.discovery_probe(this->passkey_);
        this->selection_.record_probe();
        this->send_discovery_probe_(probe);
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

    case ESP_GATTC_READ_CHAR_EVT:
    case ESP_GATTC_WRITE_CHAR_EVT:
      break;

    default:
      ESP_LOGD(TAG, "Unhandled GATT event: %d", event);
      break;
  }
}

bool DieselHeaterBLE::ble_write_chr(esp_gatt_if_t gattc_if, esp_bd_addr_t remote_bda, uint16_t handle, uint8_t *data,
                                    uint16_t len) {
  esp_err_t ret =
      esp_ble_gattc_write_char(gattc_if, 0, handle, len, data, ESP_GATT_WRITE_TYPE_RSP, ESP_GATT_AUTH_REQ_NONE);
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
  this->last_update_ = millis();
}

CommandAdmission DieselHeaterBLE::admit_command_(Capabilities required_capabilities, bool value_valid) {
  const CommandAdmission admission = this->lifecycle_.admit(required_capabilities, value_valid);
  if (admission != CommandAdmission::READY) this->log_admission_rejection_(admission);
  return admission;
}

void DieselHeaterBLE::dispatch_command_(CommandAdmission admission, const CommandBatch *batch) {
  const DispatchResult result = CommandDispatchGate::dispatch(admission, batch, [this](const CommandFrame &frame) {
    CommandFrame writable_frame = frame;
    this->ble_write_chr(this->parent()->get_gattc_if(), this->parent()->get_remote_bda(), this->handle_,
                        writable_frame.data(), writable_frame.size());
  });
  if (result == DispatchResult::BATCH_REJECTED && batch != nullptr) this->log_batch_rejection_(batch->result());
}

void DieselHeaterBLE::send_discovery_probe_(const CommandBatch &batch) {
  if (!this->lifecycle_.transport_ready() || !batch.sendable()) return;
  for (std::size_t index = 0; index < batch.size(); ++index) {
    const CommandFrame *frame = batch.frame_at(index);
    CommandFrame writable_frame = *frame;
    this->ble_write_chr(this->parent()->get_gattc_if(), this->parent()->get_remote_bda(), this->handle_,
                        writable_frame.data(), writable_frame.size());
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

void DieselHeaterBLE::log_lifecycle_(const char *message) {
  const uint32_t now = millis();
  if (now - this->last_lifecycle_log_ < 1000) return;
  this->last_lifecycle_log_ = now;
  ESP_LOGD(TAG, "%s", message);
}

void DieselHeaterBLE::clear_connection_state_() {
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
  if (this->climate_ != nullptr) this->climate_->on_heater_disconnect();
  if (this->power_switch_ != nullptr) this->power_switch_->set_has_state(false);
  if (unavailable_entities != 0) this->publish_unavailable_entities_(unavailable_entities);
}

void DieselHeaterBLE::publish_unavailable_entities_(AvailabilityMask entities) {
  const auto unavailable_sensor = [](sensor::Sensor *entity) {
    if (entity != nullptr) entity->publish_state(NAN);
  };
  const auto unavailable_number = [](number::Number *entity) {
    if (entity != nullptr) entity->publish_state(NAN);
  };

  const auto unavailable = [entities](AvailabilityEntity identity, auto entity, const auto &publish) {
    if ((entities & availability_bit(identity)) != 0) publish(entity);
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
  unavailable(AvailabilityEntity::POWER_LEVEL_NUMBER, this->power_level_number_, unavailable_number);
  unavailable(AvailabilityEntity::SET_TEMP_NUMBER, this->set_temp_number_, unavailable_number);

}

void DieselHeaterBLE::update_climate(const HeaterState &new_state) {
  if (this->climate_ == nullptr) {
    return;
  }

  this->climate_->sync_from_heater(new_state);

  this->climate_->publish_state();
}

void DieselHeaterBLE::update_sensors(const HeaterState &new_state) {
  if (!new_state.valid || !this->lifecycle_.can_publish_state()) {
    return;
  }

  this->update_climate(new_state);

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
  publish_number_field(this->power_level_number_, new_state.manual_level);
  publish_number_field(this->set_temp_number_, new_state.target_temperature_c);

  if (this->power_switch_ != nullptr && new_state.power.valid &&
      this->power_switch_->state != new_state.power.value) {
    this->power_switch_->publish_state(new_state.power.value);
  }
  this->lifecycle_.mark_entities_published();
  this->availability_.mark_published();
}

void DieselHeaterBLE::on_power_level_number(float value) {
  const ProtocolDriver *driver = this->selection_.driver();
  const bool valid_value = std::isfinite(value) && value >= 1.0f && value <= 10.0f && std::floor(value) == value;
  const CommandAdmission admission =
      this->admit_command_(capability_bits(Capability::MODE) | Capability::MANUAL_LEVEL, valid_value);
  if (admission != CommandAdmission::READY) {
    this->dispatch_command_(admission, nullptr);
    return;
  }
  const CommandBatch batch = driver->manual_level(this->state_, this->passkey_, static_cast<uint8_t>(value));
  this->dispatch_command_(admission, &batch);
}

void DieselHeaterBLE::on_temp_number(float value) {
  const ProtocolDriver *driver = this->selection_.driver();
  const bool valid_value = std::isfinite(value) && value >= 8.0f && value <= 36.0f;
  const CommandAdmission admission =
      this->admit_command_(capability_bits(Capability::MODE) | Capability::SET_TEMPERATURE, valid_value);
  if (admission != CommandAdmission::READY) {
    this->dispatch_command_(admission, nullptr);
    return;
  }
  const CommandBatch batch = driver->temperature_c(this->state_, this->passkey_, value);
  this->dispatch_command_(admission, &batch);
}

void DieselHeaterBLE::on_power_switch(bool state) {
  const ProtocolDriver *driver = this->selection_.driver();
  const CommandAdmission admission = this->admit_command_(capability_bits(Capability::POWER), true);
  if (admission != CommandAdmission::READY) {
    this->dispatch_command_(admission, nullptr);
    return;
  }
  const CommandBatch batch = driver->power(this->state_, this->passkey_, state);
  this->dispatch_command_(admission, &batch);
}

void DieselHeaterBLE::on_level_up_button_press() {
  const ProtocolDriver *driver = this->selection_.driver();
  const bool valid_value = this->state_.manual_level.valid && this->state_.manual_level.value < 10;
  const CommandAdmission admission =
      this->admit_command_(capability_bits(Capability::MODE) | Capability::MANUAL_LEVEL, valid_value);
  if (admission != CommandAdmission::READY) {
    this->dispatch_command_(admission, nullptr);
    return;
  }
  const CommandBatch batch = driver->manual_level(this->state_, this->passkey_, this->state_.manual_level.value + 1);
  this->dispatch_command_(admission, &batch);
}

void DieselHeaterBLE::on_level_down_button_press() {
  const ProtocolDriver *driver = this->selection_.driver();
  const bool valid_value = this->state_.manual_level.valid && this->state_.manual_level.value > 1;
  const CommandAdmission admission =
      this->admit_command_(capability_bits(Capability::MODE) | Capability::MANUAL_LEVEL, valid_value);
  if (admission != CommandAdmission::READY) {
    this->dispatch_command_(admission, nullptr);
    return;
  }
  const CommandBatch batch = driver->manual_level(this->state_, this->passkey_, this->state_.manual_level.value - 1);
  this->dispatch_command_(admission, &batch);
}

void DieselHeaterBLE::on_temp_up_button_press() {
  const ProtocolDriver *driver = this->selection_.driver();
  const bool valid_value = this->state_.target_temperature_c.valid &&
                           this->state_.target_temperature_c.value < 36.0f;
  const CommandAdmission admission =
      this->admit_command_(capability_bits(Capability::MODE) | Capability::SET_TEMPERATURE, valid_value);
  if (admission != CommandAdmission::READY) {
    this->dispatch_command_(admission, nullptr);
    return;
  }
  const CommandBatch batch =
      driver->temperature_c(this->state_, this->passkey_, this->state_.target_temperature_c.value + 1.0f);
  this->dispatch_command_(admission, &batch);
}

void DieselHeaterBLE::on_temp_down_button_press() {
  const ProtocolDriver *driver = this->selection_.driver();
  const bool valid_value = this->state_.target_temperature_c.valid &&
                           this->state_.target_temperature_c.value > 8.0f;
  const CommandAdmission admission =
      this->admit_command_(capability_bits(Capability::MODE) | Capability::SET_TEMPERATURE, valid_value);
  if (admission != CommandAdmission::READY) {
    this->dispatch_command_(admission, nullptr);
    return;
  }
  const CommandBatch batch =
      driver->temperature_c(this->state_, this->passkey_, this->state_.target_temperature_c.value - 1.0f);
  this->dispatch_command_(admission, &batch);
}

}  // namespace diesel_heater_ble
}  // namespace esphome

#endif
