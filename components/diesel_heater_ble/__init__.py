import esphome.codegen as cg
from esphome.components import ble_client
import esphome.config_validation as cv
from esphome.const import CONF_ID

CODEOWNERS = ["@warehog"]
DEPENDENCIES = ["ble_client"]

CONF_DIESEL_HEATER_BLE = "diesel_heater_ble"
CONF_PROTOCOL = "protocol"
CONF_PASSKEY = "passkey"

diesel_heater_ble_ns = cg.esphome_ns.namespace("diesel_heater_ble")
DieselHeaterBLE = diesel_heater_ble_ns.class_(
    "DieselHeaterBLE", cg.Component, ble_client.BLEClientNode
)
ProtocolVariant = diesel_heater_ble_ns.enum("ProtocolVariant", is_class=True)

PROTOCOL_VARIANTS = {
    "auto": ProtocolVariant.UNKNOWN,
    "aa55_plain": ProtocolVariant.MD1_AA55_PLAIN,
    "aa55_encrypted": ProtocolVariant.MD2_AA55_ENCRYPTED,
    "aa66_plain": ProtocolVariant.MD3_AA66_PLAIN,
    "aa66_encrypted": ProtocolVariant.MD4_AA66_ENCRYPTED,
}

CONFIG_SCHEMA = (
    cv.Schema({
        cv.GenerateID(): cv.declare_id(DieselHeaterBLE),
        cv.Optional(CONF_PROTOCOL, default="auto"): cv.enum(PROTOCOL_VARIANTS, lower=True),
        cv.Optional(CONF_PASSKEY, default=1234): cv.int_range(min=1, max=9999),
    })
    .extend(cv.COMPONENT_SCHEMA)
    .extend(ble_client.BLE_CLIENT_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    cg.add(var.set_protocol(config[CONF_PROTOCOL]))
    cg.add(var.set_passkey(config[CONF_PASSKEY]))
    await cg.register_component(var, config)
    await ble_client.register_ble_node(var, config)
