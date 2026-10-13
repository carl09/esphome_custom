"""Experimental raw Bluedroid connection-only probe for a COBI BLE hub."""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import esp32_ble_tracker
from esphome.const import CONF_ID, CONF_MAC_ADDRESS

DEPENDENCIES = ["esp32_ble_tracker"]
CODEOWNERS = ["@carl09"]

cobi_ble_probe_ns = cg.esphome_ns.namespace("cobi_ble_probe")
CobiBleProbe = cobi_ble_probe_ns.class_(
    "CobiBleProbe", esp32_ble_tracker.ESPBTClient, cg.Component
)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(CobiBleProbe),
            cv.Required(CONF_MAC_ADDRESS): cv.mac_address,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(esp32_ble_tracker.ESP_BLE_DEVICE_SCHEMA),
    cv.only_on_esp32,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(var.set_target_address(config[CONF_MAC_ADDRESS].as_hex))
    await esp32_ble_tracker.register_client(var, config)
