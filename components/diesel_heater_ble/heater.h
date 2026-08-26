#pragma once

#ifdef USE_ESP32

#include <esp_gattc_api.h>
#include <algorithm>
#include <iterator>
#include "esphome/components/ble_client/ble_client.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "esphome/core/component.h"
#include "esphome/core/log.h"

#include "protocol.h"
#include "protocol_aa.h"
#include "availability.h"
#include "command_delivery.h"
#include "entity_control.h"
#include "lifecycle.h"
#include "state.h"

namespace esphome {
namespace button {
class Button;
}
namespace number {
class Number;
}
namespace sensor {
class Sensor;
}
namespace switch_ {
class Switch;
}
namespace diesel_heater_ble {

class DieselHeaterClimate;

class DieselHeaterBLE : public Component, public ble_client::BLEClientNode {
 public:
  DieselHeaterBLE() = default;

  void loop() override;
  void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                           esp_ble_gattc_cb_param_t *param) override;

  bool ble_write_chr(esp_gatt_if_t gattc_if, esp_bd_addr_t remote_bda, uint16_t handle, uint8_t *data, uint16_t len);
  bool ble_read_chr(esp_gatt_if_t gattc_if, esp_bd_addr_t remote_bda, uint16_t handle);
  bool ble_register_for_notify(esp_gatt_if_t gattc_if, esp_bd_addr_t remote_bda);

  void on_notification_received(const std::vector<uint8_t> &data);
#if defined(USE_ESP32) && defined(USE_CLIMATE)
  void update_climate(const HeaterState &new_state);
#endif
  void update_sensors(const HeaterState &new_state);

  // Sensor setters
#if defined(USE_ESP32) && defined(USE_SENSOR)
  void set_running_state(sensor::Sensor *sensor) { running_state_ = sensor; availability_.register_entity(AvailabilityEntity::RUNNING_STATE); }
  void set_error_code(sensor::Sensor *sensor) { error_code_ = sensor; availability_.register_entity(AvailabilityEntity::ERROR_CODE); }
  void set_running_step(sensor::Sensor *sensor) { running_step_ = sensor; availability_.register_entity(AvailabilityEntity::RUNNING_STEP); }
  void set_altitude(sensor::Sensor *sensor) { altitude_ = sensor; availability_.register_entity(AvailabilityEntity::ALTITUDE); }
  void set_running_mode(sensor::Sensor *sensor) { running_mode_ = sensor; availability_.register_entity(AvailabilityEntity::RUNNING_MODE); }
  void set_set_level(sensor::Sensor *sensor) { set_level_ = sensor; availability_.register_entity(AvailabilityEntity::SET_LEVEL); }
  void set_set_temp(sensor::Sensor *sensor) { set_temp_ = sensor; availability_.register_entity(AvailabilityEntity::SET_TEMP); }
  void set_supply_voltage(sensor::Sensor *sensor) { supply_voltage_ = sensor; availability_.register_entity(AvailabilityEntity::SUPPLY_VOLTAGE); }
  void set_case_temp(sensor::Sensor *sensor) { case_temp_ = sensor; availability_.register_entity(AvailabilityEntity::CASE_TEMP); }
  void set_cab_temp(sensor::Sensor *sensor) { cab_temp_ = sensor; availability_.register_entity(AvailabilityEntity::CAB_TEMP); }
  void set_start_time(sensor::Sensor *sensor) { start_time_ = sensor; availability_.register_entity(AvailabilityEntity::START_TIME); }
  void set_auto_time(sensor::Sensor *sensor) { auto_time_ = sensor; availability_.register_entity(AvailabilityEntity::AUTO_TIME); }
  void set_run_time(sensor::Sensor *sensor) { run_time_ = sensor; availability_.register_entity(AvailabilityEntity::RUN_TIME); }
  void set_is_auto(sensor::Sensor *sensor) { is_auto_ = sensor; availability_.register_entity(AvailabilityEntity::IS_AUTO); }
  void set_language(sensor::Sensor *sensor) { language_ = sensor; availability_.register_entity(AvailabilityEntity::LANGUAGE); }
  void set_temp_offset(sensor::Sensor *sensor) { temp_offset_ = sensor; availability_.register_entity(AvailabilityEntity::TEMP_OFFSET); }
  void set_tank_volume(sensor::Sensor *sensor) { tank_volume_ = sensor; availability_.register_entity(AvailabilityEntity::TANK_VOLUME); }
  void set_oil_pump_type(sensor::Sensor *sensor) { oil_pump_type_ = sensor; availability_.register_entity(AvailabilityEntity::OIL_PUMP_TYPE); }
  void set_rf433_on_off(sensor::Sensor *sensor) { rf433_on_off_ = sensor; availability_.register_entity(AvailabilityEntity::RF433_ON_OFF); }
  void set_temp_unit(sensor::Sensor *sensor) { temp_unit_ = sensor; availability_.register_entity(AvailabilityEntity::TEMP_UNIT); }
  void set_altitude_unit(sensor::Sensor *sensor) { altitude_unit_ = sensor; availability_.register_entity(AvailabilityEntity::ALTITUDE_UNIT); }
  void set_automatic_heating(sensor::Sensor *sensor) { automatic_heating_ = sensor; availability_.register_entity(AvailabilityEntity::AUTOMATIC_HEATING); }
#endif

  // Button setters
#if defined(USE_ESP32) && defined(USE_BUTTON)
  void set_level_up_button(button::Button *button) { level_up_button_ = button; }
  void set_level_down_button(button::Button *button) { level_down_button_ = button; }
  void set_temp_up_button(button::Button *button) { temp_up_button_ = button; }
  void set_temp_down_button(button::Button *button) { temp_down_button_ = button; }

  EntityCommandResult on_level_up_button_press();
  EntityCommandResult on_level_down_button_press();
  EntityCommandResult on_temp_up_button_press();
  EntityCommandResult on_temp_down_button_press();
#endif

  // Number setters
#if defined(USE_ESP32) && defined(USE_NUMBER)
  void set_power_level_number(number::Number *number) { power_level_number_ = number; availability_.register_entity(AvailabilityEntity::POWER_LEVEL_NUMBER); }
  void set_set_temp_number(number::Number *number) { set_temp_number_ = number; availability_.register_entity(AvailabilityEntity::SET_TEMP_NUMBER); }
#endif
  EntityCommandResult on_power_level_number(float value, uint32_t transaction_id = 0);
  EntityCommandResult on_temp_number(float value, uint32_t transaction_id = 0);

  // Switch setter
#if defined(USE_ESP32) && defined(USE_SWITCH)
  void set_power_switch(switch_::Switch *sw) { power_switch_ = sw; }
#endif
  EntityCommandResult on_power_switch(bool state, uint32_t transaction_id = 0);

  // Climate checks a complete strategy before it constructs a multi-command
  // sequence, so unsupported capabilities cannot produce partial controls.
#if defined(USE_ESP32) && defined(USE_CLIMATE)
  EntityCommandResult admit_climate_strategy(ClimateStrategy strategy) const;
  void cancel_pending_climate_transaction(uint32_t transaction_id) {
    this->delivery_.cancel_pending_tag(transaction_id);
  }

  // Climate setter
  void set_climate(DieselHeaterClimate *climate) { climate_ = climate; }
#endif

  HeaterState get_state() { return this->state_; }
  void set_protocol(ProtocolVariant protocol) { this->selection_.set_configured(protocol); }
  void set_passkey(uint16_t passkey) { this->passkey_ = passkey; }

 protected:
  EntityCommandResult admit_command_(EntityOperation operation, float value = 0.0f);
  EntityCommandResult dispatch_command_(EntityCommandResult admission, const CommandBatch *batch, uint32_t transaction_id = 0);
  void dispatch_delivery_events_();
  bool state_matches_current_command_(const HeaterState &state) const;
  void clear_connection_state_();
  void publish_unavailable_entities_(AvailabilityMask entities);
  void log_admission_rejection_(CommandAdmission result);
  void log_batch_rejection_(CommandResult result);
  void log_delivery_enqueue_(DeliveryEnqueueResult result);
  void log_delivery_failure_();
  void log_lifecycle_(const char *message);

  uint16_t handle_{0};
  esp32_ble_tracker::ESPBTUUID service_uuid_ =
      esp32_ble_tracker::ESPBTUUID::from_raw("0000ffe0-0000-1000-8000-00805f9b34fb");
  esp32_ble_tracker::ESPBTUUID characteristic_uuid_ =
      esp32_ble_tracker::ESPBTUUID::from_raw("0000ffe1-0000-1000-8000-00805f9b34fb");

  HeaterState state_;
  ProtocolSelection selection_{};
  LifecycleSession lifecycle_{};
  AvailabilityGeneration availability_{};
  CommandDelivery delivery_{};
  uint16_t passkey_{kDefaultAAPasskey};
  bool discovery_response_failure_logged_{false};
  bool discovery_timeout_logged_{false};

  uint32_t last_request_{0};
  uint32_t last_update_{0};
  uint32_t last_rejection_log_{0};
  uint32_t last_lifecycle_log_{0};
  uint32_t last_delivery_log_{0};
  CommandAdmission last_admission_rejection_{CommandAdmission::READY};
  CommandResult last_batch_rejection_{CommandResult::READY};
  DeliveryEnqueueResult last_delivery_enqueue_{DeliveryEnqueueResult::QUEUED};
  DeliveryFailure last_delivery_failure_{DeliveryFailure::NONE};

  // Optional platform fields are compiled only with their owning platform.
#if defined(USE_ESP32) && defined(USE_SENSOR)
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
#endif

#if defined(USE_ESP32) && defined(USE_BUTTON)
  button::Button *level_up_button_{};
  button::Button *level_down_button_{};
  button::Button *temp_up_button_{};
  button::Button *temp_down_button_{};
#endif

#if defined(USE_ESP32) && defined(USE_NUMBER)
  number::Number *power_level_number_{};
  number::Number *set_temp_number_{};
#endif

#if defined(USE_ESP32) && defined(USE_SWITCH)
  switch_::Switch *power_switch_{};
#endif

#if defined(USE_ESP32) && defined(USE_CLIMATE)
  DieselHeaterClimate *climate_{};
#endif
};

}  // namespace diesel_heater_ble
}  // namespace esphome

#endif  // USE_ESP32
