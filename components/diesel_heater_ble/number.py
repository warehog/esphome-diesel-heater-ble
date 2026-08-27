import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv
from esphome.const import DEVICE_CLASS_TEMPERATURE_DELTA, UNIT_CELSIUS

from . import CONF_DIESEL_HEATER_BLE, DieselHeaterBLE, diesel_heater_ble_ns

CODEOWNERS = ["@warehog"]
DEPENDENCIES = ["diesel_heater_ble"]
AUTO_LOAD = ["number"]

CONF_POWER_LEVEL = "power_level"
PowerLevelNumber = diesel_heater_ble_ns.class_("PowerLevelNumber", number.Number)

CONF_SET_TEMP = "set_temp"
SetTempNumber = diesel_heater_ble_ns.class_("SetTempNumber", number.Number)

CONF_TEMPERATURE_OFFSET = "temperature_offset"
TemperatureOffsetNumber = diesel_heater_ble_ns.class_("TemperatureOffsetNumber", number.Number)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_DIESEL_HEATER_BLE): cv.use_id(DieselHeaterBLE),
        cv.Optional(CONF_POWER_LEVEL): number.number_schema(PowerLevelNumber),
        cv.Optional(CONF_SET_TEMP): number.number_schema(SetTempNumber),
        cv.Optional(CONF_TEMPERATURE_OFFSET): number.number_schema(
            TemperatureOffsetNumber,
            device_class=DEVICE_CLASS_TEMPERATURE_DELTA,
            unit_of_measurement=UNIT_CELSIUS,
        ),
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
    if conf := config.get(CONF_TEMPERATURE_OFFSET):
        # The app's integer -9..9 raw slider is unit-dependent. Keep this
        # static entity in the safe normalized-Celsius overlap; runtime
        # admission additionally requires an encrypted extended-settings
        # response and confirmed state.
        b = await number.new_number(conf, min_value=-5, max_value=5, step=1)
        await cg.register_parented(b, parent)
        cg.add(getattr(parent, f"set_{CONF_TEMPERATURE_OFFSET}_number")(b))
