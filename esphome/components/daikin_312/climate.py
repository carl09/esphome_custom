from esphome import pins
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import climate, sensor
from esphome.const import CONF_PIN, CONF_SENSOR
from esphome.core import CORE

from . import CONF_DAIKIN_312_ID, Daikin312Climate, daikin_312_ns

AUTO_LOAD = ["daikin_312"]
CODEOWNERS = ["@carl09"]

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
    # Enable serial on ESP8266 as IRremoteESP8266 library uses Serial
    if CORE.is_esp8266:
        from esphome.components.esp8266.const import enable_serial
        enable_serial()

    var = await climate.new_climate(config)
    await cg.register_component(var, config)

    pin = await cg.gpio_pin_expression(config[CONF_PIN])
    cg.add(var.set_pin(pin))

    if sensor_config := config.get(CONF_SENSOR):
        sens = await cg.get_variable(sensor_config)
        cg.add(var.set_sensor(sens))

    # Optimize IRremoteESP8266 build - disable all protocols except DAIKIN312
    # This significantly reduces binary size
    # BUT: if irremote_debug component is also used, we need all protocols
    # enabled for debugging, so skip the restrictive flags
    if "irremote_debug" not in CORE.config:
        cg.add_build_flag("-D_IR_ENABLE_DEFAULT_=false")
        cg.add_build_flag("-DSEND_DAIKIN312=true")
        cg.add_build_flag("-DDECODE_DAIKIN312=true")

    cg.add_library(
        "IRremoteESP8266",
        None,
        "https://github.com/carl09/IRremoteESP8266.git#daikin_312",
    )

