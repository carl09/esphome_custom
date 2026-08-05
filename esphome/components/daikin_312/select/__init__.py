import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import select
from esphome.const import CONF_ID, CONF_TYPE, ENTITY_CATEGORY_CONFIG, ICON_LIGHTBULB
from .. import CONF_DAIKIN_312_ID, Daikin312Climate, daikin_312_ns

DEPENDENCIES = ["daikin_312"]
CODEOWNERS = ["@carl09"]

ICON_VOLUME = "mdi:volume-high"
CONF_INITIAL_OPTION = "initial_option"

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


def validate_initial_option(config):
    if CONF_INITIAL_OPTION in config and config[CONF_TYPE] != "light":
        raise cv.Invalid("'initial_option' is only supported when type is 'light'")
    return config


CONFIG_SCHEMA = cv.All(
    select.select_schema(Daikin312Select, entity_category=ENTITY_CATEGORY_CONFIG)
    .extend(
        {
            cv.Required(CONF_DAIKIN_312_ID): cv.use_id(Daikin312Climate),
            cv.Required(CONF_TYPE): cv.enum(SELECT_TYPES, lower=True),
            cv.Optional(CONF_INITIAL_OPTION): cv.one_of(*LIGHT_OPTIONS),
        }
    )
    .extend(cv.COMPONENT_SCHEMA),
    validate_initial_option,
)


async def to_code(config):
    type_ = config[CONF_TYPE]
    options = get_options(type_)
    var = await select.new_select(config, options=options)
    await cg.register_component(var, config)
    parent = await cg.get_variable(config[CONF_DAIKIN_312_ID])
    cg.add(var.set_parent(parent))
    cg.add(var.set_select_type(config[CONF_TYPE]))
    if CONF_INITIAL_OPTION in config:
        cg.add(var.set_initial_option(config[CONF_INITIAL_OPTION]))
