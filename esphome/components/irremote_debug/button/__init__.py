"""Button platform for IRremote Debug component."""

import esphome.codegen as cg
from esphome.components import button
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    ENTITY_CATEGORY_DIAGNOSTIC,
    ICON_PULSE,
)

from .. import CONF_IRREMOTE_DEBUG_ID, IRremoteDebugComponent, irremote_debug_ns

DumpButton = irremote_debug_ns.class_("DumpButton", button.Button)

CONFIG_SCHEMA = button.button_schema(
    DumpButton,
    entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    icon=ICON_PULSE,
).extend(
    {
        cv.GenerateID(CONF_IRREMOTE_DEBUG_ID): cv.use_id(IRremoteDebugComponent),
    }
)


async def to_code(config):
    var = await button.new_button(config)
    await cg.register_parented(var, config[CONF_IRREMOTE_DEBUG_ID])
