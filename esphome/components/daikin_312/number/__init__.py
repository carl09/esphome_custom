import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import number
from esphome.const import (
    CONF_ID,
    CONF_TYPE,
    UNIT_MINUTE,
    ENTITY_CATEGORY_CONFIG,
    DEVICE_CLASS_DURATION,
)
from .. import CONF_DAIKIN_312_ID, Daikin312Climate, daikin_312_ns

DEPENDENCIES = ["daikin_312"]
CODEOWNERS = ["@carl09"]

ICON_SLEEP = "mdi:power-sleep"

Daikin312Number = daikin_312_ns.class_(
    "Daikin312Number", number.Number, cg.Component
)

Daikin312NumberType = daikin_312_ns.enum("Daikin312NumberType")
NUMBER_TYPES = {
    "sleep_timer": Daikin312NumberType.DAIKIN312_NUMBER_SLEEP_TIMER,
}

# Sleep timer max is 1440 minutes (24 hours), but practical max is likely 720 (12 hours)
SLEEP_TIMER_MIN = 0
SLEEP_TIMER_MAX = 720
SLEEP_TIMER_STEP = 1


CONFIG_SCHEMA = (
    number.number_schema(
        Daikin312Number,
        unit_of_measurement=UNIT_MINUTE,
        entity_category=ENTITY_CATEGORY_CONFIG,
        device_class=DEVICE_CLASS_DURATION,
        icon=ICON_SLEEP,
    )
    .extend(
        {
            cv.Required(CONF_DAIKIN_312_ID): cv.use_id(Daikin312Climate),
            cv.Required(CONF_TYPE): cv.enum(NUMBER_TYPES, lower=True),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = await number.new_number(
        config,
        min_value=SLEEP_TIMER_MIN,
        max_value=SLEEP_TIMER_MAX,
        step=SLEEP_TIMER_STEP,
    )
    await cg.register_component(var, config)
    parent = await cg.get_variable(config[CONF_DAIKIN_312_ID])
    cg.add(var.set_parent(parent))
    cg.add(var.set_number_type(config[CONF_TYPE]))
