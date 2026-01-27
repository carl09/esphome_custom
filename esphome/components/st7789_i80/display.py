from esphome import pins
import esphome.codegen as cg
from esphome.components import display
import esphome.config_validation as cv
from esphome.const import (
    CONF_BACKLIGHT_PIN,
    CONF_CS_PIN,
    CONF_DATA_PINS,
    CONF_DC_PIN,
    CONF_HEIGHT,
    CONF_ID,
    CONF_INVERT_COLORS,
    CONF_LAMBDA,
    CONF_MIRROR_X,
    CONF_MIRROR_Y,
    CONF_MODEL,
    CONF_OFFSET_HEIGHT,
    CONF_OFFSET_WIDTH,
    CONF_RESET_PIN,
    CONF_SWAP_XY,
    CONF_TRANSFORM,
    CONF_WIDTH,
)

from . import st7789_i80_ns

CONF_WR_PIN = "wr_pin"
CONF_RD_PIN = "rd_pin"
CONF_PCLK_FREQUENCY = "pclk_frequency"

CODEOWNERS = ["@carl09"]

DEPENDENCIES = ["esp32"]

ST7789I80 = st7789_i80_ns.class_(
    "ST7789I80", display.Display, cg.Component
)

# Model presets for common boards - use GPIO strings for pins
# Note: backlight_pin intentionally omitted to allow users to configure via
# separate output/light component for PWM dimming (e.g., GPIO0 on ESP32_2432S022C)
MODELS = {
    "ESP32_2432S022C": {
        CONF_WIDTH: 240,
        CONF_HEIGHT: 320,
        CONF_OFFSET_WIDTH: 0,
        CONF_OFFSET_HEIGHT: 0,
        CONF_DC_PIN: "GPIO16",
        CONF_WR_PIN: "GPIO4",
        CONF_CS_PIN: "GPIO17",
        CONF_RD_PIN: "GPIO2",
        CONF_DATA_PINS: ["GPIO15", "GPIO13", "GPIO12", "GPIO14", "GPIO27", "GPIO25", "GPIO33", "GPIO32"],
        CONF_INVERT_COLORS: True,
    },
    "CUSTOM": {},
}


def validate_data_pins(value):
    """Validate that exactly 8 data pins are provided for 8-bit parallel mode."""
    if not isinstance(value, list):
        raise cv.Invalid("data_pins must be a list")
    if len(value) != 8:
        raise cv.Invalid(f"Exactly 8 data pins required for 8-bit parallel mode, got {len(value)}")
    return value


def validate_st7789_i80(config):
    """Apply model presets and validate the configuration."""
    model = config.get(CONF_MODEL, "CUSTOM")
    if model in MODELS:
        presets = MODELS[model]
        for key, value in presets.items():
            if key not in config:
                # Apply pin schema to pin values from presets
                if key == CONF_DATA_PINS:
                    value = [pins.internal_gpio_output_pin_schema(p) for p in value]
                elif key in (CONF_DC_PIN, CONF_WR_PIN, CONF_CS_PIN, CONF_RD_PIN):
                    value = pins.internal_gpio_output_pin_schema(value)
                elif key in (CONF_RESET_PIN, CONF_BACKLIGHT_PIN):
                    value = pins.gpio_output_pin_schema(value)
                config[key] = value

    # Validate required fields
    required = [CONF_WIDTH, CONF_HEIGHT, CONF_DATA_PINS, CONF_DC_PIN, CONF_WR_PIN]
    for field in required:
        if field not in config:
            raise cv.Invalid(f"{field} is required")

    # Validate data pin count
    if CONF_DATA_PINS in config:
        validate_data_pins(config[CONF_DATA_PINS])

    return config


CONFIG_SCHEMA = cv.All(
    display.FULL_DISPLAY_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(ST7789I80),
            cv.Optional(CONF_MODEL, default="CUSTOM"): cv.one_of(*MODELS.keys(), upper=True, space="_"),
            cv.Optional(CONF_WIDTH): cv.int_range(min=1, max=480),
            cv.Optional(CONF_HEIGHT): cv.int_range(min=1, max=480),
            cv.Optional(CONF_OFFSET_WIDTH, default=0): cv.int_,
            cv.Optional(CONF_OFFSET_HEIGHT, default=0): cv.int_,
            cv.Optional(CONF_DATA_PINS): cv.All(
                [pins.internal_gpio_output_pin_schema],
                validate_data_pins,
            ),
            cv.Optional(CONF_DC_PIN): pins.internal_gpio_output_pin_schema,
            cv.Optional(CONF_WR_PIN): pins.internal_gpio_output_pin_schema,
            cv.Optional(CONF_CS_PIN): pins.internal_gpio_output_pin_schema,
            cv.Optional(CONF_RD_PIN): pins.internal_gpio_output_pin_schema,
            cv.Optional(CONF_RESET_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_BACKLIGHT_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_INVERT_COLORS, default=False): cv.boolean,
            cv.Optional(CONF_PCLK_FREQUENCY, default="12MHz"): cv.frequency,
            cv.Optional(CONF_TRANSFORM): cv.Schema(
                {
                    cv.Optional(CONF_SWAP_XY, default=False): cv.boolean,
                    cv.Optional(CONF_MIRROR_X, default=False): cv.boolean,
                    cv.Optional(CONF_MIRROR_Y, default=False): cv.boolean,
                }
            ),
        }
    ).extend(cv.polling_component_schema("1s")),
    cv.only_on_esp32,
    validate_st7789_i80,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await display.register_display(var, config)

    cg.add(var.set_dimensions(config[CONF_WIDTH], config[CONF_HEIGHT]))
    cg.add(var.set_offsets(config[CONF_OFFSET_WIDTH], config[CONF_OFFSET_HEIGHT]))

    # Set data pins
    for i, pin_conf in enumerate(config[CONF_DATA_PINS]):
        pin = await cg.gpio_pin_expression(pin_conf)
        cg.add(var.add_data_pin(pin, i))

    dc_pin = await cg.gpio_pin_expression(config[CONF_DC_PIN])
    cg.add(var.set_dc_pin(dc_pin))

    wr_pin = await cg.gpio_pin_expression(config[CONF_WR_PIN])
    cg.add(var.set_wr_pin(wr_pin))

    if CONF_CS_PIN in config:
        cs_pin = await cg.gpio_pin_expression(config[CONF_CS_PIN])
        cg.add(var.set_cs_pin(cs_pin))

    if CONF_RD_PIN in config:
        rd_pin = await cg.gpio_pin_expression(config[CONF_RD_PIN])
        cg.add(var.set_rd_pin(rd_pin))

    if CONF_RESET_PIN in config:
        reset_pin = await cg.gpio_pin_expression(config[CONF_RESET_PIN])
        cg.add(var.set_reset_pin(reset_pin))

    if CONF_BACKLIGHT_PIN in config:
        backlight_pin = await cg.gpio_pin_expression(config[CONF_BACKLIGHT_PIN])
        cg.add(var.set_backlight_pin(backlight_pin))

    cg.add(var.set_invert_colors(config[CONF_INVERT_COLORS]))
    cg.add(var.set_pclk_frequency(int(config[CONF_PCLK_FREQUENCY])))

    if CONF_TRANSFORM in config:
        transform = config[CONF_TRANSFORM]
        cg.add(var.set_swap_xy(transform[CONF_SWAP_XY]))
        cg.add(var.set_mirror_x(transform[CONF_MIRROR_X]))
        cg.add(var.set_mirror_y(transform[CONF_MIRROR_Y]))

    if CONF_LAMBDA in config:
        lambda_ = await cg.process_lambda(
            config[CONF_LAMBDA], [(display.DisplayRef, "it")], return_type=cg.void
        )
        cg.add(var.set_writer(lambda_))
