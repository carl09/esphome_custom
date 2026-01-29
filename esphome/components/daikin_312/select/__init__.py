import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import select
from esphome.const import CONF_ID, CONF_TYPE, ENTITY_CATEGORY_CONFIG, ICON_LIGHTBULB
from .. import CONF_DAIKIN_312_ID, Daikin312Climate, daikin_312_ns

DEPENDENCIES = ["daikin_312"]
CODEOWNERS = ["@carl09"]

ICON_VOLUME = "mdi:volume-high"

Daikin312Select = daikin_312_ns.class_(
    "Daikin312Select", select.Select, cg.Component
)

Daikin312SelectType = daikin_312_ns.enum("Daikin312SelectType")
SELECT_TYPES = {
    "light": Daikin312SelectType.DAIKIN312_SELECT_LIGHT,
    "beep": Daikin312SelectType.DAIKIN312_SELECT_BEEP,
}

LIGHT_OPTIONS = ["Off", "Dim", "Bright"]
BEEP_OPTIONS = ["Off", "Quiet", "Loud"]


def get_options(type_):
    if type_ == "beep":
        return BEEP_OPTIONS
    return LIGHT_OPTIONS


def get_icon(type_):
    if type_ == "beep":
        return ICON_VOLUME
    return ICON_LIGHTBULB


CONFIG_SCHEMA = (
    select.select_schema(Daikin312Select, entity_category=ENTITY_CATEGORY_CONFIG)
    .extend(
        {
            cv.Required(CONF_DAIKIN_312_ID): cv.use_id(Daikin312Climate),
            cv.Required(CONF_TYPE): cv.enum(SELECT_TYPES, lower=True),
        }
    )
)


async def to_code(config):
    type_ = config[CONF_TYPE]
    options = get_options(type_)
    var = await select.new_select(config, options=options)
    await cg.register_component(var, config)
    parent = await cg.get_variable(config[CONF_DAIKIN_312_ID])
    cg.add(var.set_parent(parent))
    cg.add(var.set_select_type(config[CONF_TYPE]))
