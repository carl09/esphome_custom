import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import select
from esphome.const import CONF_ID, ENTITY_CATEGORY_CONFIG, ICON_LIGHTBULB
from .. import CONF_DAIKIN_312_ID, Daikin312Climate, daikin_312_ns

DEPENDENCIES = ["daikin_312"]
CODEOWNERS = ["@carl09"]

Daikin312LightSelect = daikin_312_ns.class_(
    "Daikin312LightSelect", select.Select, cg.Component
)

CONFIG_SCHEMA = select.select_schema(
    Daikin312LightSelect,
    entity_category=ENTITY_CATEGORY_CONFIG,
    icon=ICON_LIGHTBULB,
).extend(
    {
        cv.Required(CONF_DAIKIN_312_ID): cv.use_id(Daikin312Climate),
    }
)


async def to_code(config):
    var = await select.new_select(config, options=["Off", "Dim", "Bright"])
    await cg.register_component(var, config)
    parent = await cg.get_variable(config[CONF_DAIKIN_312_ID])
    cg.add(var.set_parent(parent))
