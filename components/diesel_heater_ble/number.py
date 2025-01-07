import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv

from . import CONF_DIESEL_HEATER_BLE, DieselHeaterBLE, diesel_heater_ble_ns

CODEOWNERS = ["@warehog"]
DEPENDENCIES = ["diesel_heater_ble"]
AUTO_LOAD = ["number"]

CONF_POWER_LEVEL = "power_level"
PowerLevelNumber = diesel_heater_ble_ns.class_("PowerLevelNumber", number.Number)

CONF_SET_TEMP = "set_temp"
SetTempNumber = diesel_heater_ble_ns.class_("SetTempNumber", number.Number)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_DIESEL_HEATER_BLE): cv.use_id(DieselHeaterBLE),
        cv.Optional(CONF_POWER_LEVEL): number.number_schema(PowerLevelNumber),
        cv.Optional(CONF_SET_TEMP): number.number_schema(SetTempNumber),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_DIESEL_HEATER_BLE])

    # CONF_POWER_LEVEL
    if conf := config.get(CONF_POWER_LEVEL):
        b = await number.new_number(conf, min_value=1, max_value=10, step=1)
        await cg.register_parented(b, parent)
        cg.add(getattr(parent, f"set_{CONF_POWER_LEVEL}_number")(b))
    if conf := config.get(CONF_SET_TEMP):
        b = await number.new_number(conf, min_value=8, max_value=36, step=1)
        await cg.register_parented(b, parent)
        cg.add(getattr(parent, f"set_{CONF_SET_TEMP}_number")(b))
