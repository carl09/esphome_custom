"""Experimental raw Bluedroid connection-only probe for a COBI BLE hub."""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import esp32_ble_tracker
from esphome.const import CONF_ID, CONF_MAC_ADDRESS

CONF_RETRY_INTERVAL = "retry_interval"
CONF_MAX_ATTEMPTS = "max_attempts"

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
            cv.Optional(CONF_RETRY_INTERVAL, default="30s"): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_MAX_ATTEMPTS, default=0): cv.int_range(min=0),
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
    cg.add(var.set_retry_interval(config[CONF_RETRY_INTERVAL].total_milliseconds))
    cg.add(var.set_max_attempts(config[CONF_MAX_ATTEMPTS]))
    await esp32_ble_tracker.register_client(var, config)
