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
  if (this->node_state == esp32_ble_tracker::ClientState::ESTABLISHED) {
    if (this->last_request_ + 1000 < millis()) {
      this->last_request_ = millis();
      if (this->selection_.driver() != nullptr) {
        this->send_batch(this->selection_.driver()->status(this->passkey_));
      } else if (this->selection_.should_probe()) {
        // App-derived shared discovery probe: configured passkey, command 1,
        // value 0. It does not select a driver on its own.
        CommandBatch probe = this->selection_.discovery_probe(this->passkey_);
        this->selection_.record_probe();
        this->send_batch(probe);
      } else if (!this->discovery_timeout_logged_) {
        if (this->selection_.invalid_response_seen()) {
          ESP_LOGW(TAG, "Protocol discovery failed after %u probes: invalid response received.",
                   this->selection_.probes_sent());
        } else {
          ESP_LOGW(TAG, "Protocol discovery timed out after %u probes: no response received.",
                   this->selection_.probes_sent());
        }
        this->discovery_timeout_logged_ = true;
      }
    }
    this->update_sensors(this->state_);
  }
}

void DieselHeaterBLE::gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                                          esp_ble_gattc_cb_param_t *param) {
  switch (event) {
    case ESP_GATTC_OPEN_EVT:
      ESP_LOGD(TAG, "GATT client opened.");
      this->selection_.reset();
      this->discovery_response_failure_logged_ = false;
      this->discovery_timeout_logged_ = false;
      break;

    case ESP_GATTC_DISCONNECT_EVT:
      ESP_LOGD(TAG, "GATT client disconnected.");
      this->node_state = esp32_ble_tracker::ClientState::DISCONNECTING;
      this->handle_ = 0;
      this->selection_.reset();
      this->discovery_response_failure_logged_ = false;
      this->discovery_timeout_logged_ = false;
      break;

    case ESP_GATTC_SEARCH_CMPL_EVT:
      if (param->search_cmpl.status != ESP_GATT_OK) {
        ESP_LOGD(TAG, "Service search failed, status: %d", param->search_cmpl.status);
        break;
      }
      this->ble_register_for_notify(this->parent()->get_gattc_if(), this->parent()->get_remote_bda());
      break;

    case ESP_GATTC_REG_FOR_NOTIFY_EVT:
      if (param->reg_for_notify.status != ESP_GATT_OK) {
        ESP_LOGD(TAG, "Register for notify failed, status: %d", param->reg_for_notify.status);
        break;
      }
      this->node_state = esp32_ble_tracker::ClientState::ESTABLISHED;
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
  esp_err_t ret = esp_ble_gattc_register_for_notify(gattc_if, remote_bda, this->handle_);
  if (ret != ESP_OK) {
    ESP_LOGD(TAG, "Register for notify failed, status: %d", ret);
    return false;
  }
  return true;
}

void DieselHeaterBLE::on_notification_received(const std::vector<uint8_t> &data) {
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
}

void DieselHeaterBLE::send_batch(const CommandBatch &batch) {
  if (!batch.sendable()) {
    ESP_LOGD(TAG, "Ignored command, result: %u", static_cast<unsigned>(batch.result()));
    return;
  }
  for (std::size_t index = 0; index < batch.size(); ++index) {
    const CommandFrame *frame = batch.frame_at(index);
    CommandFrame writable_frame = *frame;
    this->ble_write_chr(this->parent()->get_gattc_if(), this->parent()->get_remote_bda(), this->handle_,
                        writable_frame.data(), writable_frame.size());
  }
}

void DieselHeaterBLE::update_climate(const HeaterState &new_state) {
  if (this->climate_ == nullptr) {
    return;
  }

  this->climate_->sync_from_heater(new_state);

  this->climate_->publish_state();
}

void DieselHeaterBLE::update_sensors(const HeaterState &new_state) {
  if (!new_state.valid) {
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
}

void DieselHeaterBLE::on_power_level_number(float value) {
  const ProtocolDriver *driver = this->selection_.driver();
  if (driver == nullptr || value < 1.0f || value > 10.0f || std::floor(value) != value) {
    this->send_batch(CommandBatch::with_result(CommandResult::STATE_UNAVAILABLE));
    return;
  }
  this->send_batch(driver->manual_level(this->state_, this->passkey_, static_cast<uint8_t>(value)));
}

void DieselHeaterBLE::on_temp_number(float value) {
  const ProtocolDriver *driver = this->selection_.driver();
  if (driver == nullptr) {
    this->send_batch(CommandBatch::with_result(CommandResult::STATE_UNAVAILABLE));
    return;
  }
  this->send_batch(driver->temperature_c(this->state_, this->passkey_, value));
}

void DieselHeaterBLE::on_power_switch(bool state) {
  const ProtocolDriver *driver = this->selection_.driver();
  if (driver == nullptr) {
    this->send_batch(CommandBatch::with_result(CommandResult::STATE_UNAVAILABLE));
    return;
  }
  this->send_batch(driver->power(this->state_, this->passkey_, state));
}

void DieselHeaterBLE::on_level_up_button_press() {
  const ProtocolDriver *driver = this->selection_.driver();
  if (driver == nullptr || !this->state_.manual_level.valid || this->state_.manual_level.value >= 10) {
    this->send_batch(CommandBatch::with_result(CommandResult::STATE_UNAVAILABLE));
    return;
  }
  this->send_batch(driver->manual_level(this->state_, this->passkey_, this->state_.manual_level.value + 1));
}

void DieselHeaterBLE::on_level_down_button_press() {
  const ProtocolDriver *driver = this->selection_.driver();
  if (driver == nullptr || !this->state_.manual_level.valid || this->state_.manual_level.value <= 1) {
    this->send_batch(CommandBatch::with_result(CommandResult::STATE_UNAVAILABLE));
    return;
  }
  this->send_batch(driver->manual_level(this->state_, this->passkey_, this->state_.manual_level.value - 1));
}

void DieselHeaterBLE::on_temp_up_button_press() {
  const ProtocolDriver *driver = this->selection_.driver();
  if (driver == nullptr || !this->state_.target_temperature_c.valid) {
    this->send_batch(CommandBatch::with_result(CommandResult::STATE_UNAVAILABLE));
    return;
  }
  this->send_batch(driver->temperature_c(this->state_, this->passkey_, this->state_.target_temperature_c.value + 1.0f));
}

void DieselHeaterBLE::on_temp_down_button_press() {
  const ProtocolDriver *driver = this->selection_.driver();
  if (driver == nullptr || !this->state_.target_temperature_c.valid) {
    this->send_batch(CommandBatch::with_result(CommandResult::STATE_UNAVAILABLE));
    return;
  }
  this->send_batch(driver->temperature_c(this->state_, this->passkey_, this->state_.target_temperature_c.value - 1.0f));
}

}  // namespace diesel_heater_ble
}  // namespace esphome

#endif
