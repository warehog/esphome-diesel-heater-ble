#include "heater.h"

#ifdef USE_ESP32

namespace esphome {
namespace diesel_heater_ble {

const char *TAG = "diesel_heater_ble";

void DieselHeaterBLE::loop() {
  if (this->node_state == esp32_ble_tracker::ClientState::ESTABLISHED) {
    if (this->last_request_ + 1000 < millis()) {
      this->last_request_ = millis();
      uint8_t data[8] = {0xaa, 0x55, 0x0c, 0x22, 0x01, 0x00, 0x00, 0x2f};
      this->ble_write_chr(this->parent()->get_gattc_if(), this->parent()->get_remote_bda(), this->handle_, data,
                          sizeof(data));
    }
    this->update_sensors(this->state_);
  }
}

void DieselHeaterBLE::gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                                          esp_ble_gattc_cb_param_t *param) {
  switch (event) {
    case ESP_GATTC_OPEN_EVT:
      ESP_LOGD(TAG, "GATT client opened.");
      break;

    case ESP_GATTC_DISCONNECT_EVT:
      ESP_LOGD(TAG, "GATT client disconnected.");
      this->node_state = esp32_ble_tracker::ClientState::DISCONNECTING;
      this->handle_ = 0;
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
  // ESP_LOGD(TAG, "Notification received: %s", format_hex_pretty(data).c_str());
  if (this->controller_ == nullptr) {
    this->controller_ = ControllerSelector::get_controller(ResponseParser::detect_heater_class(data));
    if (this->controller_ == nullptr) {
      ESP_LOGD(TAG, "Failed to get controller.");
      return;
    }
  }

  bool ret = ResponseParser::parse(data, this->state_);
  if (!ret) {
    ESP_LOGD(TAG, "Failed to parse response.");
    return;
  }
}

void DieselHeaterBLE::update_sensors(const HeaterState &new_state) {
  if (running_state_ != nullptr && running_state_->state != new_state.runningstate) {
    if (this->power_switch_ != nullptr) {
      this->power_switch_->publish_state(new_state.runningstate);
    }
    running_state_->publish_state(new_state.runningstate);
    return;
  }
  if (error_code_ != nullptr && error_code_->state != new_state.errcode) {
    error_code_->publish_state(new_state.errcode);
    return;
  }
  if (running_step_ != nullptr && running_step_->state != new_state.runningstep) {
    running_step_->publish_state(new_state.runningstep);
    return;
  }
  if (altitude_ != nullptr && altitude_->state != new_state.altitude) {
    altitude_->publish_state(new_state.altitude);
    return;
  }
  if (running_mode_ != nullptr && running_mode_->state != new_state.runningmode) {
    running_mode_->publish_state(new_state.runningmode);
    return;
  }
  if (set_level_ != nullptr && set_level_->state != new_state.setlevel) {
    if (this->power_level_number_ != nullptr) {
      this->power_level_number_->publish_state(new_state.setlevel);
    }
    set_level_->publish_state(new_state.setlevel);
    return;
  }
  if (set_temp_ != nullptr && set_temp_->state != new_state.settemp) {
    if (this->set_temp_number_ != nullptr) {
      this->set_temp_number_->publish_state(new_state.settemp);
    }
    set_temp_->publish_state(new_state.settemp);
    return;
  }
  if (set_temp_number_ != nullptr && set_temp_number_->state != new_state.settemp) {
    set_temp_number_->publish_state(new_state.settemp);
    return;
  }
  if (supply_voltage_ != nullptr && supply_voltage_->state != new_state.supplyvoltage) {
    supply_voltage_->publish_state(new_state.supplyvoltage);
    return;
  }
  if (case_temp_ != nullptr && case_temp_->state != new_state.casetemp) {
    case_temp_->publish_state(new_state.casetemp);
    return;
  }
  if (cab_temp_ != nullptr && cab_temp_->state != new_state.cabtemp) {
    cab_temp_->publish_state(new_state.cabtemp);
    return;
  }
  if (start_time_ != nullptr && start_time_->state != new_state.sttime) {
    start_time_->publish_state(new_state.sttime);
    return;
  }
  if (auto_time_ != nullptr && auto_time_->state != new_state.autotime) {
    auto_time_->publish_state(new_state.autotime);
    return;
  }
  if (run_time_ != nullptr && run_time_->state != new_state.runtime) {
    run_time_->publish_state(new_state.runtime);
    return;
  }
  if (is_auto_ != nullptr && is_auto_->state != new_state.isauto) {
    is_auto_->publish_state(new_state.isauto);
    return;
  }
  if (language_ != nullptr && language_->state != new_state.language) {
    language_->publish_state(new_state.language);
    return;
  }
  if (temp_offset_ != nullptr && temp_offset_->state != new_state.tempoffset) {
    temp_offset_->publish_state(new_state.tempoffset);
    return;
  }
  if (tank_volume_ != nullptr && tank_volume_->state != new_state.tankvolume) {
    tank_volume_->publish_state(new_state.tankvolume);
    return;
  }
  if (oil_pump_type_ != nullptr && oil_pump_type_->state != new_state.oilpumptype) {
    oil_pump_type_->publish_state(new_state.oilpumptype);
    return;
  }
  if (rf433_on_off_ != nullptr && rf433_on_off_->state != static_cast<float>(new_state.rf433onoff)) {
    rf433_on_off_->publish_state(new_state.rf433onoff);
    return;
  }
  if (temp_unit_ != nullptr && temp_unit_->state != new_state.tempunit) {
    temp_unit_->publish_state(new_state.tempunit);
    return;
  }
  if (altitude_unit_ != nullptr && altitude_unit_->state != new_state.altiunit) {
    altitude_unit_->publish_state(new_state.altiunit);
    return;
  }
  if (automatic_heating_ != nullptr && automatic_heating_->state != new_state.automaticheating) {
    automatic_heating_->publish_state(new_state.automaticheating);
    return;
  }
}

void DieselHeaterBLE::on_power_level_number(float value) {
  this->sent_requests(this->controller_->gen_level_command(this->state_, value));
}

void DieselHeaterBLE::on_temp_number(float value) {
  this->sent_requests(this->controller_->gen_temp_command(this->state_, value));
}

void DieselHeaterBLE::on_power_switch(bool state) {
  this->sent_requests(this->controller_->gen_power_command(this->state_, state));
}

void DieselHeaterBLE::on_level_up_button_press() {
  this->sent_requests(this->controller_->gen_level_up_command(this->state_));
}

void DieselHeaterBLE::on_level_down_button_press() {
  this->sent_requests(this->controller_->gen_level_down_command(this->state_));
}

void DieselHeaterBLE::on_temp_up_button_press() {
  this->sent_requests(this->controller_->gen_temp_up_command(this->state_));
}

void DieselHeaterBLE::on_temp_down_button_press() {
  this->sent_requests(this->controller_->gen_temp_down_command(this->state_));
}

}  // namespace diesel_heater_ble
}  // namespace esphome

#endif
