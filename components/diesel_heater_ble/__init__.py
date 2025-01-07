import esphome.codegen as cg
from esphome.components import ble_client, sensor
import esphome.config_validation as cv
from esphome.const import CONF_ID

CODEOWNERS = ["@warehog"]
DEPENDENCIES = ["ble_client"]

CONF_DIESEL_HEATER_BLE = "diesel_heater_ble"

diesel_heater_ble_ns = cg.esphome_ns.namespace("diesel_heater_ble")
DieselHeaterBLE = diesel_heater_ble_ns.class_(
    "DieselHeaterBLE", sensor.Sensor, cg.Component, ble_client.BLEClientNode
)

CONFIG_SCHEMA = (
    cv.Schema({cv.GenerateID(): cv.declare_id(DieselHeaterBLE)})
    .extend(cv.COMPONENT_SCHEMA)
    .extend(ble_client.BLE_CLIENT_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await ble_client.register_ble_node(var, config)
