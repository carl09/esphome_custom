"""Text sensor platform for IRremote Debug component."""

import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import (
    ENTITY_CATEGORY_DIAGNOSTIC,
    ICON_REMOTE,
)

from .. import CONF_IRREMOTE_DEBUG_ID, IRremoteDebugComponent, irremote_debug_ns

DEPENDENCIES = ["irremote_debug"]

CONFIG_SCHEMA = text_sensor.text_sensor_schema(
    icon=ICON_REMOTE,
    entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
).extend(
    {
        cv.GenerateID(CONF_IRREMOTE_DEBUG_ID): cv.use_id(IRremoteDebugComponent),
    }
)


async def to_code(config):
    var = await text_sensor.new_text_sensor(config)
    parent = await cg.get_variable(config[CONF_IRREMOTE_DEBUG_ID])
    cg.add(parent.set_protocol_text_sensor(var))
