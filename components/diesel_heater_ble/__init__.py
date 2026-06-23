import esphome.codegen as cg
from esphome.components import ble_client, sensor, time as time_
import esphome.config_validation as cv
from esphome.const import CONF_ID

CODEOWNERS = ["@warehog"]
DEPENDENCIES = ["ble_client"]
AUTO_LOAD = ["sensor", "button", "number", "switch", "climate"]

CONF_DIESEL_HEATER_BLE = "diesel_heater_ble"
CONF_TIME_ID = "time_id"

diesel_heater_ble_ns = cg.esphome_ns.namespace("diesel_heater_ble")
DieselHeaterBLE = diesel_heater_ble_ns.class_(
    "DieselHeaterBLE", sensor.Sensor, cg.Component, ble_client.BLEClientNode
)

CONFIG_SCHEMA = (
    cv.Schema({
        cv.GenerateID(): cv.declare_id(DieselHeaterBLE),
        cv.Required(CONF_TIME_ID): cv.use_id(time_.RealTimeClock),
    })
    .extend(cv.COMPONENT_SCHEMA)
    .extend(ble_client.BLE_CLIENT_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await ble_client.register_ble_node(var, config)
    time_var = await cg.get_variable(config[CONF_TIME_ID])
    cg.add(var.set_time(time_var))
