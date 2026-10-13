"""COBI AIR telemetry with opt-in BLE bonding and lifecycle wake."""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import ble_client, sensor, text_sensor
from esphome.const import CONF_ID

CODEOWNERS = ["@carl09"]
DEPENDENCIES = ["esp32_ble_tracker"]
AUTO_LOAD = ["ble_client", "sensor", "text_sensor"]

cobi_bike_ns = cg.esphome_ns.namespace("cobi_bike")
CobiBikeComponent = cobi_bike_ns.class_(
    "CobiBikeComponent", cg.PollingComponent, ble_client.BLEClientNode
)

CONF_PAIR = "pair"
CONF_WAKE_HUB = "wake_hub"
CONF_TRACTION_BATTERY_LEVEL = "traction_battery_level"
CONF_TRACTION_BATTERY_STATE = "traction_battery_state"
CONF_BATTERY_HEALTH_SOH = "battery_health_soh"
CONF_BATTERY_VOLTAGE_RAW = "battery_voltage_raw"
CONF_BATTERY_CURRENT_RAW = "battery_current_raw"
CONF_BATTERY_CAPACITY_RAW = "battery_capacity_raw"
CONF_HUB_BATTERY_LEVEL = "hub_battery_level"
CONF_HUB_BATTERY_STATE = "hub_battery_state"
CONF_MOTOR_POWER_ON_TIME_RAW = "motor_power_on_time_raw"
CONF_ODOMETER = "odometer"
CONF_MOTOR_RANGE = "motor_range"
CONF_BATTERY_RANGE = "battery_range"
CONF_SPEED = "speed"
CONF_CADENCE = "cadence"
CONF_RIDER_POWER = "rider_power"
CONF_ASSIST_LEVEL = "assist_level"
CONF_LINK_STATUS = "link_status"

SENSOR_KEYS = {
    CONF_TRACTION_BATTERY_LEVEL: "set_traction_battery_level_sensor",
    CONF_BATTERY_HEALTH_SOH: "set_battery_health_soh_sensor",
    CONF_BATTERY_VOLTAGE_RAW: "set_battery_voltage_raw_sensor",
    CONF_BATTERY_CURRENT_RAW: "set_battery_current_raw_sensor",
    CONF_BATTERY_CAPACITY_RAW: "set_battery_capacity_raw_sensor",
    CONF_HUB_BATTERY_LEVEL: "set_hub_battery_level_sensor",
    CONF_MOTOR_POWER_ON_TIME_RAW: "set_motor_power_on_time_raw_sensor",
    CONF_ODOMETER: "set_odometer_sensor",
    CONF_MOTOR_RANGE: "set_motor_range_sensor",
    CONF_BATTERY_RANGE: "set_battery_range_sensor",
    CONF_SPEED: "set_speed_sensor",
    CONF_CADENCE: "set_cadence_sensor",
    CONF_RIDER_POWER: "set_rider_power_sensor",
    CONF_ASSIST_LEVEL: "set_assist_level_sensor",
}

TEXT_SENSOR_KEYS = {
    CONF_TRACTION_BATTERY_STATE: "set_traction_battery_state_sensor",
    CONF_HUB_BATTERY_STATE: "set_hub_battery_state_sensor",
    CONF_LINK_STATUS: "set_link_status_sensor",
}

RAW_SENSOR_SCHEMA = sensor.sensor_schema(
    accuracy_decimals=0,
    entity_category="diagnostic",
)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(CobiBikeComponent),
            cv.Optional(CONF_PAIR, default=False): cv.boolean,
            cv.Optional(CONF_WAKE_HUB, default=False): cv.boolean,
            cv.Optional(CONF_TRACTION_BATTERY_LEVEL): sensor.sensor_schema(
                unit_of_measurement="%",
                accuracy_decimals=0,
                device_class="battery",
                state_class="measurement",
            ),
            cv.Optional(CONF_TRACTION_BATTERY_STATE): text_sensor.text_sensor_schema(),
            cv.Optional(CONF_BATTERY_HEALTH_SOH): sensor.sensor_schema(
                unit_of_measurement="%",
                accuracy_decimals=0,
                icon="mdi:battery-heart-variant",
                entity_category="diagnostic",
            ),
            cv.Optional(CONF_BATTERY_VOLTAGE_RAW): RAW_SENSOR_SCHEMA,
            cv.Optional(CONF_BATTERY_CURRENT_RAW): RAW_SENSOR_SCHEMA,
            cv.Optional(CONF_BATTERY_CAPACITY_RAW): RAW_SENSOR_SCHEMA,
            cv.Optional(CONF_HUB_BATTERY_LEVEL): sensor.sensor_schema(
                unit_of_measurement="%",
                accuracy_decimals=0,
                device_class="battery",
                state_class="measurement",
                entity_category="diagnostic",
            ),
            cv.Optional(CONF_HUB_BATTERY_STATE): text_sensor.text_sensor_schema(),
            cv.Optional(CONF_MOTOR_POWER_ON_TIME_RAW): RAW_SENSOR_SCHEMA,
            cv.Optional(CONF_ODOMETER): sensor.sensor_schema(
                unit_of_measurement="km",
                accuracy_decimals=2,
                icon="mdi:counter",
                device_class="distance",
                state_class="total_increasing",
            ),
            cv.Optional(CONF_MOTOR_RANGE): sensor.sensor_schema(
                unit_of_measurement="km", accuracy_decimals=0, icon="mdi:map-marker-distance"
            ),
            cv.Optional(CONF_BATTERY_RANGE): sensor.sensor_schema(
                unit_of_measurement="km", accuracy_decimals=1, icon="mdi:map-marker-distance"
            ),
            cv.Optional(CONF_SPEED): sensor.sensor_schema(
                unit_of_measurement="km/h", accuracy_decimals=1, device_class="speed", state_class="measurement"
            ),
            cv.Optional(CONF_CADENCE): sensor.sensor_schema(
                unit_of_measurement="rpm", accuracy_decimals=0, icon="mdi:rotate-right", state_class="measurement"
            ),
            cv.Optional(CONF_RIDER_POWER): sensor.sensor_schema(
                unit_of_measurement="W", accuracy_decimals=0, device_class="power", state_class="measurement"
            ),
            cv.Optional(CONF_ASSIST_LEVEL): sensor.sensor_schema(accuracy_decimals=0, icon="mdi:speedometer"),
            cv.Optional(CONF_LINK_STATUS): text_sensor.text_sensor_schema(),
        }
    )
    .extend(ble_client.BLE_CLIENT_SCHEMA)
    .extend(cv.polling_component_schema("60s")),
    cv.only_on_esp32,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(var.set_pairing(config[CONF_PAIR]))
    cg.add(var.set_wake_hub(config[CONF_WAKE_HUB]))
    await ble_client.register_ble_node(var, config)

    for key, setter in SENSOR_KEYS.items():
        if key in config:
            child = await sensor.new_sensor(config[key])
            cg.add(getattr(var, setter)(child))

    for key, setter in TEXT_SENSOR_KEYS.items():
        if key in config:
            child = await text_sensor.new_text_sensor(config[key])
            cg.add(getattr(var, setter)(child))
