import esphome.codegen as cg
from esphome.components import climate
import esphome.config_validation as cv

from . import CONF_DIESEL_HEATER_BLE, DieselHeaterBLE, diesel_heater_ble_ns

CODEOWNERS = ["@warehog"]
DEPENDENCIES = ["diesel_heater_ble"]
AUTO_LOAD = ["climate"]

DieselHeaterClimate = diesel_heater_ble_ns.class_(
    "DieselHeaterClimate", climate.Climate, cg.Component
)

CONF_EXTERNAL_TEMPERATURE_TIMEOUT = "external_temperature_timeout"
CONF_EXTERNAL_TEMPERATURE_TIMEOUT_HEATING = "external_temperature_timeout_heating"
CONF_EXTERNAL_USE_FAHRENHEIT = "external_temperature_fahrenheit"
CONF_EXTERNAL_STAGE_GAIN = "external_stage_gain"
CONF_EXTERNAL_OFF_ERROR = "external_off_error"
CONF_EXTERNAL_AUTO_ON_ERROR = "external_auto_on_error"
CONF_EXTERNAL_MIN_ON_TIME = "external_min_on_time"
CONF_EXTERNAL_MIN_OFF_TIME = "external_min_off_time"
CONF_EXTERNAL_OVERSHOOT_HOLD = "external_overshoot_hold"
CONF_EXTERNAL_LEVEL_UPDATE_INTERVAL = "external_level_update_interval"

CONFIG_SCHEMA = (
    climate.climate_schema(DieselHeaterClimate)
    .extend(
        {
            cv.GenerateID(CONF_DIESEL_HEATER_BLE): cv.use_id(DieselHeaterBLE),
            cv.Optional(
                CONF_EXTERNAL_TEMPERATURE_TIMEOUT, default="2h"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(
                CONF_EXTERNAL_TEMPERATURE_TIMEOUT_HEATING, default="15min"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_EXTERNAL_USE_FAHRENHEIT, default=False): cv.boolean,
            cv.Optional(CONF_EXTERNAL_STAGE_GAIN, default=2.0): cv.float_range(min=0.01),
            cv.Optional(CONF_EXTERNAL_OFF_ERROR, default=-0.5): cv.float_,
            cv.Optional(CONF_EXTERNAL_AUTO_ON_ERROR, default=1.0): cv.float_,
            cv.Optional(CONF_EXTERNAL_MIN_ON_TIME, default="10min"): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_EXTERNAL_MIN_OFF_TIME, default="20min"): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_EXTERNAL_OVERSHOOT_HOLD, default="3min"): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_EXTERNAL_LEVEL_UPDATE_INTERVAL, default="10s"): cv.positive_time_period_milliseconds,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = await climate.new_climate(config)
    await cg.register_component(var, config)

    parent = await cg.get_variable(config[CONF_DIESEL_HEATER_BLE])
    cg.add(var.set_heater(parent))
    cg.add(parent.set_climate(var))
    cg.add(var.set_external_temperature_timeout(config[CONF_EXTERNAL_TEMPERATURE_TIMEOUT]))
    cg.add(var.set_external_temperature_timeout_heating(config[CONF_EXTERNAL_TEMPERATURE_TIMEOUT_HEATING]))
    cg.add(var.set_external_temperature_fahrenheit(config[CONF_EXTERNAL_USE_FAHRENHEIT]))
    cg.add(var.set_external_stage_gain(config[CONF_EXTERNAL_STAGE_GAIN]))
    cg.add(var.set_external_off_error(config[CONF_EXTERNAL_OFF_ERROR]))
    cg.add(var.set_external_auto_on_error(config[CONF_EXTERNAL_AUTO_ON_ERROR]))
    cg.add(var.set_external_min_on_time(config[CONF_EXTERNAL_MIN_ON_TIME]))
    cg.add(var.set_external_min_off_time(config[CONF_EXTERNAL_MIN_OFF_TIME]))
    cg.add(var.set_external_overshoot_hold(config[CONF_EXTERNAL_OVERSHOOT_HOLD]))
    cg.add(var.set_external_level_update_interval(config[CONF_EXTERNAL_LEVEL_UPDATE_INTERVAL]))
