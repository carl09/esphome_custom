# mipi_rgb: framebuffer-compatible bounce buffers

This external component is based on ESPHome 2026.10.0's built-in `mipi_rgb`.
Upstream source: https://github.com/esphome/esphome/tree/2026.10.0/esphome/components/mipi_rgb
ESPHome license: https://github.com/esphome/esphome/blob/2026.10.0/LICENSE

The only driver change is in `MipiRgb::common_setup_()`: choose the largest
row count <= 10 that evenly divides the panel height, instead of always using
10 rows. All upstream Python schema, models and other driver behavior remain
unchanged.

## Why

ESP-IDF 5.5.6 requires the framebuffer size to be an exact multiple of the
bounce-buffer size. A 480x272 RGB565 panel has 261120 framebuffer bytes;
the upstream 10-row bounce buffer has 9600 bytes and fails this requirement.
The patched 8-row buffer has 7680 bytes and divides the framebuffer exactly.
Upstream initialization fails with `ESP_ERR_INVALID_ARG`, logging
`frame buffer size must be multiple of bounce buffer size` followed by
`lcd setup failed`.

## Usage

Requires ESPHome 2026.10.0, native ESP-IDF, ESP32-S3 and PSRAM as upstream does.

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/carl09/esphome_custom
      ref: fix/mipi-rgb-bounce-buffer
    components: [mipi_rgb]
    refresh: 0s
```

Keep your existing `display: platform: mipi_rgb` configuration and panel timings.
`refresh: 0s` fetches branch updates on builds; pin to a tested commit for stable
production deployments. This is a frozen upstream copy, not an automatically
updated version of the built-in component. Do not load unrelated components
from this repository unless required by your configuration.

Compile-tested on ESPHome 2026.10.0 / ESP-IDF 5.5.6 using a standalone Sunton
480x272 test. Not hardware-tested. This fixes framebuffer divisibility, not
panel sync polarity or incorrect wiring/timings.

To revert, remove this component from `external_components` (or remove the
block if it is the only external component), then rebuild with the built-in
driver. That restores upstream behavior, including this bug until fixed there.
