import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv

from . import CONF_HEATER_ID, DieselHeaterBLE, diesel_heater_ble_ns

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_HEATER_ID): cv.use_id(DieselHeaterBLE),
        cv.Optional("power"): switch.switch_schema(
            diesel_heater_ble_ns.class_("PowerSwitch", switch.Switch),
            icon="mdi:power",
        ),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_HEATER_ID])

    for switch_type in ["power"]:
        if conf := config.get(switch_type):
            sw_var = await switch.new_switch(conf)
            await cg.register_parented(sw_var, parent)
            cg.add(getattr(parent, f"set_{switch_type}_switch")(sw_var))
