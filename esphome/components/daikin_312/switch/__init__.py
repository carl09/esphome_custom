import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch
from esphome.const import CONF_TYPE, ICON_AIR_FILTER
from .. import CONF_DAIKIN_312_ID, Daikin312Climate, daikin_312_ns

DEPENDENCIES = ["daikin_312"]
CODEOWNERS = ["@carl09"]

ICON_EYE = "mdi:eye"
ICON_EYE_AUTO = "mdi:eye-refresh"

Daikin312Switch = daikin_312_ns.class_(
    "Daikin312Switch", switch.Switch, cg.Component
)

Daikin312SwitchType = daikin_312_ns.enum("Daikin312SwitchType")
SWITCH_TYPES = {
    "purify": Daikin312SwitchType.DAIKIN312_SWITCH_PURIFY,
    "eye": Daikin312SwitchType.DAIKIN312_SWITCH_EYE,
    "eye_auto": Daikin312SwitchType.DAIKIN312_SWITCH_EYE_AUTO,
}


def get_icon(type_):
    if type_ == "eye":
        return ICON_EYE
    if type_ == "eye_auto":
        return ICON_EYE_AUTO
    return ICON_AIR_FILTER


CONFIG_SCHEMA = (
    switch.switch_schema(Daikin312Switch)
    .extend(
        {
            cv.Required(CONF_DAIKIN_312_ID): cv.use_id(Daikin312Climate),
            cv.Required(CONF_TYPE): cv.enum(SWITCH_TYPES, lower=True),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = await switch.new_switch(config)
    await cg.register_component(var, config)
    parent = await cg.get_variable(config[CONF_DAIKIN_312_ID])
    cg.add(var.set_parent(parent))
    cg.add(var.set_switch_type(config[CONF_TYPE]))
