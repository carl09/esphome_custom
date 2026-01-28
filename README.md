# Custom ESPHome Components

This repository hosts custom components for [ESPHome](https://esphome.io/), designed to be easily integrated using the `external_components` feature.

## Usage

To use components from this repository, add the following to your ESPHome YAML configuration:

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/your-username/esphome_custom
    components: [ st7789_i80, daikin_312 ]
```

## Available Components

### `daikin_312`

A climate component for controlling Daikin air conditioners that usage the 312-bit protocol (e.g., using the ARC466A58 remote).
The implementation uses a custom logic for the IR protocol.

**Features:**
- Implements `climate` platform for controlling mode, temperature, fan speed, etc.
- Supports an external sensor for current ambient temperature.
- Provides a `switch` platform to control specific functions (e.g., Clean/Purify).

**Configuration:**

**Climate:**

```yaml
climate:
  - platform: daikin_312
    id: my_ac
    name: "Living Room AC"
    pin: GPIO4  # IR Transmitter pin
    sensor: temp_sensor_id # Optional: ID of a sensor component for current temperature
```

**Switch (Clean/Purify):**

```yaml
switch:
  - platform: daikin_312
    name: "Living Room AC Clean"
    daikin_312_id: my_ac
```

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
