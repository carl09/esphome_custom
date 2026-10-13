# Custom ESPHome Components

This repository hosts custom components for [ESPHome](https://esphome.io/), designed to be easily integrated using the `external_components` feature.

## Usage

To use components from this repository, add the following to your ESPHome YAML configuration:

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/your-username/esphome_custom
    components: [ st7789_i80, daikin_312, irremote_debug, cobi_bike, cobi_ble_probe ]
```

## Available Components

### `cobi_bike` (COBI AIR telemetry; optional pairing and hub wake)

Connects an ESP32 ESPHome node to the **COBI AIR** BLE transport (hub advertising as `Bosch Sm…`) and reads values from the hub. It exposes the traction-battery level/state, the hub's own battery (separately named, not the e-bike battery), and optional motor values: odometer, range, battery range, speed, cadence, rider power and assist level. Battery-health fields and motor power-on time are also available but still unverified (see below).

Telemetry requests use COBI protocol `READ` messages. The component does not send motor, light, alarm, reset, shutdown, or firmware-update commands. BLE notification subscription and AIR transport-counter synchronization are setup/recovery writes required by the transport. Hub wake is separately opt-in (`wake_hub: true`) and only sends the lifecycle `WRITE` `Hub.rideLifecycleState = NOT_RIDABLE` after the hub reports `AWAY` in app mode, matching the app's wake path; this is a state-changing request, not a bike-control command.

#### Pairing (required the first time)

The hub only accepts a connection from a device it hasn't bonded with **while it is in pairing mode** (its advertisement then carries the pairing flag; `cobi_ble_probe` logs `pairing_mode=YES`). Outside pairing mode, connection attempts from an unbonded ESP32 time out (`ESP_GATTC_DISCONNECT_EVT reason 0x100`, then `OPEN_EVT status=133`) and no link is ever formed. The official app only offers hubs that advertise pairing mode.

1. Close the COBI app and put the hub into pairing mode (see COBI/Bosch hub documentation; the window is short, roughly 10-15 s).
2. Flash with `pair: true` and `ble_client` `auto_connect: true`, so the ESP32 connects as soon as the pairing advertisement is seen. It bonds with Just Works (the ESP32 reports IO capability `none`); the bond is stored in the ESP32's NVS flash.
3. After a successful bond the ESP32 reconnects on its own without pairing mode (verified by rebooting it). Erasing the ESP32's flash, or using a different ESP32, means pairing again. `pair` can be set back to `false` afterwards.
4. Re-pairing may disturb the phone's bond; if the COBI app stops connecting, re-pair the phone.

#### Requirements and limitations

ESP32 only; ESPHome's `esp32_ble_tracker` and `ble_client`; COBI AIR hub path (COBI Pro is not implemented). The hub may allow only one connection at a time, so the ESP32 and the phone app may compete for it; this has not been tested.

- **Verified on a live hub:** pairing, bonded reconnect, and traction `Battery.state` / hub battery values (the traction level matched the app during the HCI capture). When the hub/bike turns off it reports level 0 / `UNKNOWN`; the component publishes `unknown` (not 0 %) for the level in that case.
- **Not verified:** `odometer` (`Motor.distance`, uint32 / 100; unit assumed km), `motor_range`, `battery_range`, `speed`, `cadence`, `rider_power`, `assist_level`, battery health and raw voltage/current/capacity, and `motor_power_on_time_raw`. These come from static APK protocol definitions. The hub has not answered the health and power-on-time READs so far, and speed/cadence/power are only meaningful while riding.
- Multi-byte values on the COBI bus are little-endian (the app decodes them from a little-endian buffer); the component reads them that way.
- Unknown bus messages are logged once per distinct (action, channel, property) at INFO (`Unsupported COBI bus message ...`); this helps identify unmapped hub traffic.

```yaml
esp32_ble_tracker:

ble_client:
  - mac_address: "AA:BB:CC:DD:EE:FF"  # COBI hub address
    id: cobi_hub
    auto_connect: true

cobi_bike:
  id: cobi
  ble_client_id: cobi_hub
  update_interval: 5min
  # Needed the first time, with the hub in pairing mode (see above). The bond
  # is persisted, so this can be false afterwards.
  pair: false
  # Optional diagnostic: once the session is ready, send ~90 read-only READs one at a time and log every raw
  # response as 'SCAN tx' / 'SCAN rx' (INFO) so unmapped values can be decoded. Turn off afterwards.
  discovery_scan: false
  # Optional: send the app-style lifecycle wake WRITE if the hub reports AWAY.
  # Runs only after the BLE/GATT and COBI AIR transports are established.
  wake_hub: false
  traction_battery_level:
    name: "E-bike traction battery"
  traction_battery_state:
    name: "E-bike traction battery state"
  battery_health_soh:
    name: "E-bike battery state of health"
  battery_voltage_raw:
    name: "E-bike battery voltage (raw)"
  battery_current_raw:
    name: "E-bike battery current (raw)"
  battery_capacity_raw:
    name: "E-bike battery capacity (raw)"
  hub_battery_level:
    name: "COBI hub battery"  # Not the e-bike traction battery
  hub_battery_state:
    name: "COBI hub battery state"
  motor_power_on_time_raw:
    name: "Motor power-on time (raw)"
  odometer:
    name: "E-bike odometer"
  motor_range:
    name: "E-bike range"
  battery_range:
    name: "E-bike battery range"
  speed:
    name: "E-bike speed"
  cadence:
    name: "E-bike cadence"
  rider_power:
    name: "E-bike rider power"
  assist_level:
    name: "E-bike assist level"
  link_status:
    name: "COBI BLE link status"
```

ESPHome BLE uses significant RAM. The ESP32 shows `esp32_ble_client` at VERBOSE very noisily; for readable logs use `logger: logs: esp32_ble_client: INFO`, `esp32_ble_tracker: INFO`, `cobi_bike: DEBUG`.

### `cobi_ble_probe` (diagnostic)

Connection-only diagnostic that bypasses `ble_client`/`cobi_bike`: it logs the hub's raw advertisement (including the decoded pairing flag), retries direct connections, and does read-only service discovery. See [its README](esphome/components/cobi_ble_probe/README.md). Do not run it together with `ble_client:`/`cobi_bike:`.

### `daikin_312`

A climate component for controlling Daikin air conditioners that usage the 312-bit protocol (e.g., using the ARC466A58 remote).
The implementation uses a custom logic for the IR protocol.

**Features:**
- Implements `climate` platform for controlling mode, temperature, fan speed, etc.
- Supports an external sensor for current ambient temperature.
- Supports external state sync from Home Assistant Daikin cloud integration.
- Provides a `switch` platform to control specific functions (e.g., Purify, Eye).
- Provides a `select` platform to control light and beep settings.

**Configuration:**

**Climate (Basic):**

```yaml
climate:
  - platform: daikin_312
    id: my_ac
    name: "Living Room AC"
    pin: GPIO4  # IR Transmitter pin
    sensor: temp_sensor_id # Optional: ID of a sensor component for current temperature
```

**Climate (with External State Sync):**

Sync state from the Home Assistant Daikin cloud integration to keep the ESPHome component in sync with the actual AC state:

```yaml
# Define Home Assistant sensors to receive state from the cloud integration
text_sensor:
  - platform: homeassistant
    id: ext_ha_mode
    entity_id: climate.daikin1
    attribute: hvac_action

  - platform: homeassistant
    id: ext_ha_fan
    entity_id: climate.daikin1
    attribute: fan_mode

  - platform: homeassistant
    id: ext_ha_swing
    entity_id: climate.daikin1
    attribute: swing_mode

sensor:
  - platform: homeassistant
    id: ext_ha_temp
    entity_id: climate.daikin1
    attribute: temperature

# Configure the climate component with external state sync
climate:
  - platform: daikin_312
    id: my_ac
    name: "Daikin 312 AC"
    pin: GPIO4
    external_mode: ext_ha_mode
    external_temperature: ext_ha_temp
    external_fan_mode: ext_ha_fan
    external_swing_mode: ext_ha_swing
    on_turn_off:
      - homeassistant.action:
          action: climate.turn_off
          data:
            entity_id: climate.daikin1
      - logger.log: "AC turned off via IR - backup service called"
```

**Switch (Purify, Eye, Eye Auto):**

Controls various AC unit features with state restoration on boot:
- `purify`: Enables the air purification/clean function
- `eye`: Enables the motion sensor (Intelligent Eye)
- `eye_auto`: Enables automatic eye mode

Set `restore_mode` to `RESTORE_DEFAULT_ON` or `RESTORE_DEFAULT_OFF` to
choose the default state when no saved state exists. This works for all three
switch types; a saved state takes precedence on subsequent boots.

```yaml
switch:
  - platform: daikin_312
    daikin_312_id: my_ac
    type: purify  # Options: purify, eye, eye_auto
    name: "Living Room AC Clean"
    restore_mode: RESTORE_DEFAULT_ON  # Optional: default to on on first boot
  - platform: daikin_312
    daikin_312_id: my_ac
    type: eye
    name: "Living Room AC Eye"
  - platform: daikin_312
    daikin_312_id: my_ac
    type: eye_auto
    name: "Living Room AC Eye Auto"
```

**Select (Light & Beep):**

Controls the LED status lights on the AC unit and the beep sound when receiving IR commands. State is automatically restored on boot.

```yaml
select:
  - platform: daikin_312
    daikin_312_id: my_ac
    type: light  # Options: light, beep
    name: "Daikin Light"
    initial_option: Dim  # Optional: Off, Dim, or Bright
  - platform: daikin_312
    daikin_312_id: my_ac
    type: beep
    name: "Daikin Beep"
```

`initial_option` sets the light selection and sends the corresponding IR command
when no previously saved light state exists. Once a light state has been saved,
that restored state takes precedence on boot.

### `st7789_i80`

A display driver for ST7789 screens connected via the Intel 8080 (8-bit parallel) interface.

**Primary Use Case:**
This component was created to support the **esp32-2432s022c** board. This board is a variant of the popular "Cheap Yellow Display" (CYD) family but uses an uncommon display configuration with an 8-bit parallel bus instead of the more typical SPI interface.

**Features:**
- Support for Intel 8080 8-bit parallel interface.
- Includes a preset configuration for the `ESP32_2432S022C`.

**Configuration:**

```yaml
display:
  - platform: st7789_i80
    model: ESP32_2432S022C
    # ... standard display options
```

### `irremote_debug`

A comprehensive IR receiver debugging component that decodes and logs received IR signals with multiple verbosity levels. Uses the [IRremoteESP8266](https://github.com/carl09/IRremoteESP8266.git#daikin_312) library.

**Features:**
- 4 verbosity levels: `minimal`, `normal`, `detailed`, `raw`
- Logs protocol name, hex code, bits, address, command
- For AC protocols: decodes temperature, mode, fan speed, swing, etc.
- Raw mode: outputs `uint16_t rawData[]` and `uint8_t state[]` ready for analysis tools
- Button platform: manually re-dump the last signal at RAW verbosity
- Text sensor platform: shows "PROTOCOL: 0xHEX" in Home Assistant

**Configuration:**

**Main Component:**

```yaml
irremote_debug:
  id: ir_debug
  pin: GPIO14          # Your IR receiver data pin
  buffer_size: 1024    # Optional, adjust if needed
  timeout: 15ms        # Optional, idle timeout
  verbosity: detailed  # Options: minimal, normal, detailed, raw
```

**Button (Dump Last Signal):**

```yaml
button:
  - platform: irremote_debug
    irremote_debug_id: ir_debug
    name: "Dump Last IR Signal"
```

**Text Sensor (Last Protocol):**

```yaml
text_sensor:
  - platform: irremote_debug
    irremote_debug_id: ir_debug
    name: "Last IR Protocol"
```
