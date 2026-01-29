# Custom ESPHome Components

This repository hosts custom components for [ESPHome](https://esphome.io/), designed to be easily integrated using the `external_components` feature.

## Usage

To use components from this repository, add the following to your ESPHome YAML configuration:

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/your-username/esphome_custom
    components: [ st7789_i80, daikin_312, irremote_debug ]
```

## Available Components

### `daikin_312`

A climate component for controlling Daikin air conditioners that usage the 312-bit protocol (e.g., using the ARC466A58 remote).
The implementation uses a custom logic for the IR protocol.

**Features:**
- Implements `climate` platform for controlling mode, temperature, fan speed, etc.
- Supports an external sensor for current ambient temperature.
- Supports external state sync from Home Assistant Daikin cloud integration.
- Provides a `switch` platform to control specific functions (e.g., Purify, Eye).
- Provides a `select` platform to control light and beep settings.
- Provides a `number` platform to set the sleep timer duration.

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

```yaml
switch:
  - platform: daikin_312
    daikin_312_id: my_ac
    type: purify  # Options: purify, eye, eye_auto
    name: "Living Room AC Clean"
    restore_mode: RESTORE_DEFAULT_OFF  # Optional: restore state on boot
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
  - platform: daikin_312
    daikin_312_id: my_ac
    type: beep
    name: "Daikin Beep"
```

**Number (Sleep Timer):**

Sets the sleep timer duration in minutes. When enabled, the AC will automatically turn off after the specified duration. Set to 0 to disable the timer. State is automatically restored on boot.

```yaml
number:
  - platform: daikin_312
    daikin_312_id: my_ac
    type: sleep_timer
    name: "Daikin Sleep Timer"
```

- **Range:** 0-720 minutes (0 = off, max 12 hours)
- **Unit:** minutes
- **Note:** The sleep timer shares the timer location with the On Timer in the IR protocol. Enabling the sleep timer will disable any On Timer setting.

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
