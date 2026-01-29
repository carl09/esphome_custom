from esphome import automation, pins
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import climate, sensor, text_sensor
from esphome.const import CONF_PIN, CONF_SENSOR, CONF_TRIGGER_ID
from esphome.core import CORE

from . import CONF_DAIKIN_312_ID, Daikin312Climate, daikin_312_ns

AUTO_LOAD = ["daikin_312"]
CODEOWNERS = ["@carl09"]

# External state sync configuration keys
CONF_EXTERNAL_MODE = "external_mode"
CONF_EXTERNAL_TEMPERATURE = "external_temperature"
CONF_EXTERNAL_FAN_MODE = "external_fan_mode"
CONF_EXTERNAL_SWING_MODE = "external_swing_mode"

# Automation trigger key
CONF_ON_TURN_OFF = "on_turn_off"

Daikin312Climate = daikin_312_ns.class_(
    "Daikin312Climate", climate.Climate, cg.Component
)

# Trigger class for on_turn_off automation
Daikin312TurnOffTrigger = daikin_312_ns.class_(
    "Daikin312TurnOffTrigger", automation.Trigger.template()
)

CONFIG_SCHEMA = (
    climate.climate_schema(Daikin312Climate)
    .extend(
        {
            cv.Required(CONF_PIN): pins.internal_gpio_output_pin_schema,
            cv.Optional(CONF_SENSOR): cv.use_id(sensor.Sensor),
            # External state sync from Home Assistant Daikin integration
            cv.Optional(CONF_EXTERNAL_MODE): cv.use_id(text_sensor.TextSensor),
            cv.Optional(CONF_EXTERNAL_TEMPERATURE): cv.use_id(sensor.Sensor),
            cv.Optional(CONF_EXTERNAL_FAN_MODE): cv.use_id(text_sensor.TextSensor),
            cv.Optional(CONF_EXTERNAL_SWING_MODE): cv.use_id(text_sensor.TextSensor),
            # Automation triggers
            cv.Optional(CONF_ON_TURN_OFF): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(Daikin312TurnOffTrigger),
                }
            ),
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

    # External state sync sensors
    if external_mode := config.get(CONF_EXTERNAL_MODE):
        ext_mode_sensor = await cg.get_variable(external_mode)
        cg.add(var.set_external_mode_sensor(ext_mode_sensor))

    if external_temp := config.get(CONF_EXTERNAL_TEMPERATURE):
        ext_temp_sensor = await cg.get_variable(external_temp)
        cg.add(var.set_external_temperature_sensor(ext_temp_sensor))

    if external_fan := config.get(CONF_EXTERNAL_FAN_MODE):
        ext_fan_sensor = await cg.get_variable(external_fan)
        cg.add(var.set_external_fan_mode_sensor(ext_fan_sensor))

    if external_swing := config.get(CONF_EXTERNAL_SWING_MODE):
        ext_swing_sensor = await cg.get_variable(external_swing)
        cg.add(var.set_external_swing_mode_sensor(ext_swing_sensor))

    # Setup automation triggers
    for conf in config.get(CONF_ON_TURN_OFF, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID])
        cg.add(var.set_turn_off_trigger(trigger))
        await automation.build_automation(trigger, [], conf)

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

