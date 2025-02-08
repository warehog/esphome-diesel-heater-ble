import esphome.codegen as cg
from esphome.components import ble_client, sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ALTITUDE,
    DEVICE_CLASS_EMPTY,
    DEVICE_CLASS_TEMPERATURE,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_EMPTY,
    UNIT_METER,
    UNIT_SECOND,
    UNIT_VOLT,
)

from . import CONF_DIESEL_HEATER_BLE, DieselHeaterBLE

CODEOWNERS = ["@warehog"]
DEPENDENCIES = ["diesel_heater_ble"]
AUTO_LOAD = ["sensor"]

# Sensor configurations
CONF_RUNNING_STATE = "running_state"
CONF_ERROR_CODE = "error_code"
CONF_RUNNING_STEP = "running_step"
CONF_RUNNING_MODE = "running_mode"
CONF_SET_LEVEL = "set_level"
CONF_SET_TEMP = "set_temp"
CONF_CASE_TEMP = "case_temp"
CONF_CAB_TEMP = "cab_temp"
CONF_START_TIME = "start_time"
CONF_AUTO_TIME = "auto_time"
CONF_RUN_TIME = "run_time"
CONF_IS_AUTO = "is_auto"
CONF_LANGUAGE = "language"
CONF_TEMP_OFFSET = "temp_offset"
CONF_TANK_VOLUME = "tank_volume"
CONF_OIL_PUMP_TYPE = "oil_pump_type"
CONF_RF433_ON_OFF = "rf433_on_off"
CONF_TEMP_UNIT = "temp_unit"
CONF_ALTITUDE_UNIT = "altitude_unit"
CONF_AUTOMATIC_HEATING = "automatic_heating"
CONF_SUPPLY_VOLTAGE = "supply_voltage"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_DIESEL_HEATER_BLE): cv.use_id(DieselHeaterBLE),
        cv.Optional(CONF_RUNNING_STATE): sensor.sensor_schema(
            unit_of_measurement=UNIT_EMPTY,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_EMPTY,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_ERROR_CODE): sensor.sensor_schema(
            unit_of_measurement=UNIT_EMPTY,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_EMPTY,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_RUNNING_STEP): sensor.sensor_schema(
            unit_of_measurement=UNIT_EMPTY,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_EMPTY,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_ALTITUDE): sensor.sensor_schema(
            unit_of_measurement=UNIT_METER,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_EMPTY,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_RUNNING_MODE): sensor.sensor_schema(
            unit_of_measurement=UNIT_EMPTY,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_EMPTY,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_SET_LEVEL): sensor.sensor_schema(
            unit_of_measurement=UNIT_EMPTY,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_EMPTY,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_SET_TEMP): sensor.sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_SUPPLY_VOLTAGE): sensor.sensor_schema(
            unit_of_measurement=UNIT_VOLT,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_EMPTY,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_CASE_TEMP): sensor.sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_CAB_TEMP): sensor.sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_START_TIME): sensor.sensor_schema(
            unit_of_measurement=UNIT_SECOND,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_EMPTY,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_AUTO_TIME): sensor.sensor_schema(
            unit_of_measurement=UNIT_SECOND,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_EMPTY,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_RUN_TIME): sensor.sensor_schema(
            unit_of_measurement=UNIT_SECOND,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_EMPTY,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_IS_AUTO): sensor.sensor_schema(
            unit_of_measurement=UNIT_EMPTY,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_EMPTY,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_LANGUAGE): sensor.sensor_schema(
            unit_of_measurement=UNIT_EMPTY,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_EMPTY,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_TEMP_OFFSET): sensor.sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_TANK_VOLUME): sensor.sensor_schema(
            unit_of_measurement=UNIT_EMPTY,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_EMPTY,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_OIL_PUMP_TYPE): sensor.sensor_schema(
            unit_of_measurement=UNIT_EMPTY,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_EMPTY,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_RF433_ON_OFF): sensor.sensor_schema(
            unit_of_measurement=UNIT_EMPTY,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_EMPTY,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_TEMP_UNIT): sensor.sensor_schema(
            unit_of_measurement=UNIT_EMPTY,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_EMPTY,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_ALTITUDE_UNIT): sensor.sensor_schema(
            unit_of_measurement=UNIT_EMPTY,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_EMPTY,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_AUTOMATIC_HEATING): sensor.sensor_schema(
            unit_of_measurement=UNIT_EMPTY,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_EMPTY,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
    }
).extend(ble_client.BLE_CLIENT_SCHEMA)


async def to_code(config):
    var = await cg.get_variable(config[CONF_DIESEL_HEATER_BLE])

    for sensor_name in [
        CONF_RUNNING_STATE,
        CONF_ERROR_CODE,
        CONF_RUNNING_STEP,
        CONF_ALTITUDE,
        CONF_RUNNING_MODE,
        CONF_SET_LEVEL,
        CONF_SET_TEMP,
        CONF_SUPPLY_VOLTAGE,
        CONF_CASE_TEMP,
        CONF_CAB_TEMP,
        CONF_START_TIME,
        CONF_AUTO_TIME,
        CONF_RUN_TIME,
        CONF_IS_AUTO,
        CONF_LANGUAGE,
        CONF_TEMP_OFFSET,
        CONF_TANK_VOLUME,
        CONF_OIL_PUMP_TYPE,
        CONF_RF433_ON_OFF,
        CONF_TEMP_UNIT,
        CONF_ALTITUDE_UNIT,
        CONF_AUTOMATIC_HEATING,
    ]:
        if sensor_config := config.get(sensor_name):
            sens = await sensor.new_sensor(sensor_config)
            cg.add(getattr(var, f"set_{sensor_name}")(sens))
