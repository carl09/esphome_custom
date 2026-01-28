from esphome import pins
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import climate, sensor
from esphome.const import CONF_PIN, CONF_SENSOR

CODEOWNERS = ["@carl09"]

CONF_DAIKIN_312_ID = "daikin_312_id"

daikin_312_ns = cg.esphome_ns.namespace("daikin_312")
Daikin312Climate = daikin_312_ns.class_(
    "Daikin312Climate", climate.Climate, cg.Component
)

CONFIG_SCHEMA = (
    climate.climate_schema(Daikin312Climate)
    .extend(
        {
            cv.Required(CONF_PIN): pins.internal_gpio_output_pin_schema,
            cv.Optional(CONF_SENSOR): cv.use_id(sensor.Sensor),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = await climate.new_climate(config)
    await cg.register_component(var, config)

    pin = await cg.gpio_pin_expression(config[CONF_PIN])
    cg.add(var.set_pin(pin))

    if sensor_config := config.get(CONF_SENSOR):
        sens = await cg.get_variable(sensor_config)
        cg.add(var.set_sensor(sens))

    cg.add_library(
        None,
        None,
        "https://github.com/carl09/IRremoteESP8266.git#daikin_312",
    )

