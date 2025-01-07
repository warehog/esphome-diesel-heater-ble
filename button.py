import esphome.codegen as cg
from esphome.components import button
import esphome.config_validation as cv

from . import CONF_DIESEL_HEATER_BLE, DieselHeaterBLE, diesel_heater_ble_ns

CONF_LEVEL_UP = "level_up"
CONF_LEVEL_DOWN = "level_down"
CONF_TEMP_UP = "temp_up"
CONF_TEMP_DOWN = "temp_down"


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_DIESEL_HEATER_BLE): cv.use_id(DieselHeaterBLE),
        cv.Optional(CONF_LEVEL_UP): button.button_schema(
            diesel_heater_ble_ns.class_("LevelUpButton", button.Button),
            icon="mdi:arrow-up",
        ),
        cv.Optional(CONF_LEVEL_DOWN): button.button_schema(
            diesel_heater_ble_ns.class_("LevelDownButton", button.Button),
            icon="mdi:arrow-down",
        ),
        cv.Optional(CONF_TEMP_UP): button.button_schema(
            diesel_heater_ble_ns.class_("TempUpButton", button.Button),
            icon="mdi:arrow-up",
        ),
        cv.Optional(CONF_TEMP_DOWN): button.button_schema(
            diesel_heater_ble_ns.class_("TempDownButton", button.Button),
            icon="mdi:arrow-down",
        ),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_DIESEL_HEATER_BLE])

    for var in [
        CONF_LEVEL_UP,
        CONF_LEVEL_DOWN,
        CONF_TEMP_UP,
        CONF_TEMP_DOWN,
    ]:
        if conf := config.get(var):
            sw_var = await button.new_button(conf)
            await cg.register_parented(sw_var, parent)
            cg.add(getattr(parent, f"set_{var}_button")(sw_var))
