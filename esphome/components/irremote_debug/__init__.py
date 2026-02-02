"""IRremote Debug component for ESPHome."""

import esphome.codegen as cg
from esphome import pins
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_PIN,
    CONF_BUFFER_SIZE,
    CONF_TIMEOUT,
)
from esphome.core import CORE

CODEOWNERS = ["@carl09"]
DEPENDENCIES = ["logger"]

CONF_IRREMOTE_DEBUG_ID = "irremote_debug_id"
CONF_VERBOSITY = "verbosity"

irremote_debug_ns = cg.esphome_ns.namespace("irremote_debug")
IRremoteDebugComponent = irremote_debug_ns.class_(
    "IRremoteDebugComponent", cg.PollingComponent
)

VerbosityLevel = irremote_debug_ns.enum("VerbosityLevel")
VERBOSITY_LEVELS = {
    "minimal": VerbosityLevel.VERBOSITY_MINIMAL,
    "normal": VerbosityLevel.VERBOSITY_NORMAL,
    "detailed": VerbosityLevel.VERBOSITY_DETAILED,
    "raw": VerbosityLevel.VERBOSITY_RAW,
}

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(IRremoteDebugComponent),
        cv.Required(CONF_PIN): pins.internal_gpio_input_pin_schema,
        cv.Optional(CONF_BUFFER_SIZE, default=1024): cv.int_range(min=100, max=2048),
        cv.Optional(CONF_TIMEOUT, default="15ms"): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_VERBOSITY, default="normal"): cv.enum(
            VERBOSITY_LEVELS, lower=True
        ),
    }
).extend(cv.polling_component_schema("100ms"))


async def to_code(config):
    # Enable serial on ESP8266 as IRremoteESP8266 library uses Serial
    if CORE.is_esp8266:
        from esphome.components.esp8266.const import enable_serial
        enable_serial()

    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    pin = await cg.gpio_pin_expression(config[CONF_PIN])
    cg.add(var.set_pin(pin))

    cg.add(var.set_buffer_size(config[CONF_BUFFER_SIZE]))
    cg.add(var.set_timeout(config[CONF_TIMEOUT]))
    cg.add(var.set_verbosity(config[CONF_VERBOSITY]))

    # This debug component needs ALL protocols enabled to decode unknown/any IR signals.
    # Explicitly set _IR_ENABLE_DEFAULT_=true to override any other component's
    # restrictive settings (e.g., daikin_312 sets it to false for size optimization)
    cg.add_build_flag("-D_IR_ENABLE_DEFAULT_=true")

    # Add the IRremoteESP8266 library from the fork
    cg.add_library(
        "IRremoteESP8266",
        None,
        "https://github.com/carl09/IRremoteESP8266.git#daikin_312",
    )
