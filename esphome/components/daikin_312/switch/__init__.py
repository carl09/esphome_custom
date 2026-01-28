import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch
from esphome.const import ICON_AIR_FILTER
from .. import CONF_DAIKIN_312_ID, Daikin312Climate, daikin_312_ns

DEPENDENCIES = ["daikin_312"]
CODEOWNERS = ["@carl09"]

Daikin312CleanSwitch = daikin_312_ns.class_(
    "Daikin312CleanSwitch", switch.Switch, cg.Component
)

CONFIG_SCHEMA = switch.switch_schema(
    Daikin312CleanSwitch,
    icon=ICON_AIR_FILTER,
).extend(
    {
        cv.Required(CONF_DAIKIN_312_ID): cv.use_id(Daikin312Climate),
    }
)


async def to_code(config):
    var = await switch.new_switch(config)
    parent = await cg.get_variable(config[CONF_DAIKIN_312_ID])
    cg.add(var.set_parent(parent))
