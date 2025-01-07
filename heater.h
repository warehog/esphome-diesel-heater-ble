#pragma once

#ifdef USE_ESP32

#include <esp_gattc_api.h>
#include <algorithm>
#include <iterator>
#include "esphome/components/ble_client/ble_client.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/number/number.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/button/button.h"
#include "esphome/core/component.h"
#include "esphome/core/log.h"

#include "messages.h"
#include "state.h"
#include "controllers.h"

namespace esphome {
namespace diesel_heater_ble {

class DieselHeaterBLE : public Component, public ble_client::BLEClientNode {
 public:
  DieselHeaterBLE() = default;

  void loop() override;
  void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                           esp_ble_gattc_cb_param_t *param) override;

  void sent_request(const std::vector<uint8_t> &data) {
    std::vector<uint8_t> data_(data);
    this->ble_write_chr(this->parent()->get_gattc_if(), this->parent()->get_remote_bda(), this->handle_, data_.data(),
                        data.size());
  }

  void sent_requests(const std::vector<Request> &requests) {
    for (const auto &request : requests) {
      this->sent_request(request.toBytes());
    }
  }

  bool ble_write_chr(esp_gatt_if_t gattc_if, esp_bd_addr_t remote_bda, uint16_t handle, uint8_t *data, uint16_t len);
  bool ble_read_chr(esp_gatt_if_t gattc_if, esp_bd_addr_t remote_bda, uint16_t handle);
  bool ble_register_for_notify(esp_gatt_if_t gattc_if, esp_bd_addr_t remote_bda);

  void on_notification_received(const std::vector<uint8_t> &data);
  void update_sensors(const HeaterState &new_state);

  // Sensor setters
  void set_running_state(sensor::Sensor *sensor) { running_state_ = sensor; }
  void set_error_code(sensor::Sensor *sensor) { error_code_ = sensor; }
  void set_running_step(sensor::Sensor *sensor) { running_step_ = sensor; }
  void set_altitude(sensor::Sensor *sensor) { altitude_ = sensor; }
  void set_running_mode(sensor::Sensor *sensor) { running_mode_ = sensor; }
  void set_set_level(sensor::Sensor *sensor) { set_level_ = sensor; }
  void set_set_temp(sensor::Sensor *sensor) { set_temp_ = sensor; }
  void set_supply_voltage(sensor::Sensor *sensor) { supply_voltage_ = sensor; }
  void set_case_temp(sensor::Sensor *sensor) { case_temp_ = sensor; }
  void set_cab_temp(sensor::Sensor *sensor) { cab_temp_ = sensor; }
  void set_start_time(sensor::Sensor *sensor) { start_time_ = sensor; }
  void set_auto_time(sensor::Sensor *sensor) { auto_time_ = sensor; }
  void set_run_time(sensor::Sensor *sensor) { run_time_ = sensor; }
  void set_is_auto(sensor::Sensor *sensor) { is_auto_ = sensor; }
  void set_language(sensor::Sensor *sensor) { language_ = sensor; }
  void set_temp_offset(sensor::Sensor *sensor) { temp_offset_ = sensor; }
  void set_tank_volume(sensor::Sensor *sensor) { tank_volume_ = sensor; }
  void set_oil_pump_type(sensor::Sensor *sensor) { oil_pump_type_ = sensor; }
  void set_rf433_on_off(sensor::Sensor *sensor) { rf433_on_off_ = sensor; }
  void set_temp_unit(sensor::Sensor *sensor) { temp_unit_ = sensor; }
  void set_altitude_unit(sensor::Sensor *sensor) { altitude_unit_ = sensor; }
  void set_automatic_heating(sensor::Sensor *sensor) { automatic_heating_ = sensor; }

  // Button setters
  void set_level_up_button(button::Button *button) { level_up_button_ = button; }
  void set_level_down_button(button::Button *button) { level_down_button_ = button; }
  void set_temp_up_button(button::Button *button) { temp_up_button_ = button; }
  void set_temp_down_button(button::Button *button) { temp_down_button_ = button; }

  void on_level_up_button_press();
  void on_level_down_button_press();
  void on_temp_up_button_press();
  void on_temp_down_button_press();

  // Number setters
  void set_power_level_number(number::Number *number) { power_level_number_ = number; }
  void on_power_level_number(float value);

  void set_set_temp_number(number::Number *number) { set_temp_number_ = number; }
  void on_temp_number(float value);

  // Switch setter
  void set_power_switch(switch_::Switch *sw) { power_switch_ = sw; }
  void on_power_switch(bool state);

  HeaterState get_state() { return this->state_; }

 protected:
  uint16_t handle_{0};
  esp32_ble_tracker::ESPBTUUID service_uuid_ =
      esp32_ble_tracker::ESPBTUUID::from_raw("0000ffe0-0000-1000-8000-00805f9b34fb");
  esp32_ble_tracker::ESPBTUUID characteristic_uuid_ =
      esp32_ble_tracker::ESPBTUUID::from_raw("0000ffe1-0000-1000-8000-00805f9b34fb");

  HeaterState state_;
  HeaterController *controller_{};

  bool response_received_{false};
  uint32_t last_request_{0};
  uint32_t last_update_{0};

  // Sensor fields
  sensor::Sensor *running_state_{};
  sensor::Sensor *error_code_{};
  sensor::Sensor *running_step_{};
  sensor::Sensor *altitude_{};
  sensor::Sensor *running_mode_{};
  sensor::Sensor *set_level_{};
  sensor::Sensor *set_temp_{};
  sensor::Sensor *supply_voltage_{};
  sensor::Sensor *case_temp_{};
  sensor::Sensor *cab_temp_{};
  sensor::Sensor *start_time_{};
  sensor::Sensor *auto_time_{};
  sensor::Sensor *run_time_{};
  sensor::Sensor *is_auto_{};
  sensor::Sensor *language_{};
  sensor::Sensor *temp_offset_{};
  sensor::Sensor *tank_volume_{};
  sensor::Sensor *oil_pump_type_{};
  sensor::Sensor *rf433_on_off_{};
  sensor::Sensor *temp_unit_{};
  sensor::Sensor *altitude_unit_{};
  sensor::Sensor *automatic_heating_{};

  button::Button *level_up_button_{};
  button::Button *level_down_button_{};
  button::Button *temp_up_button_{};
  button::Button *temp_down_button_{};

  number::Number *power_level_number_{};
  number::Number *set_temp_number_{};

  switch_::Switch *power_switch_{};
};

}  // namespace diesel_heater_ble
}  // namespace esphome

#endif  // USE_ESP32
