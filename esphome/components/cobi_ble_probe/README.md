# COBI raw Bluedroid connection probe

Experimental, connection-only diagnostic for separating ESPHome's `BLEClientBase` from the raw ESP-IDF GATT-client path.

The probe does **not** use the `ble_client` or `cobi_bike` components. It calls `esp_ble_gattc_open()` directly and handles the resulting GATT-client events itself. ESPHome's `esp32_ble_tracker` is intentionally retained for scanning and connection arbitration, and ESPHome still initializes the Bluedroid host/controller. This is therefore a staged test, not a complete replacement of ESPHome BLE.

For a matching advertisement, it makes **one direct connection attempt per boot**, logs physical-connect and GATT-open events, performs read-only GATT service discovery, and closes the connection. It does not request bonding, enable notifications, read characteristic values, or write to the bike. Reboot the node to make another attempt.

Use it by itself in a temporary test configuration. Do not include `ble_client:` or `cobi_bike:` in that configuration, so the test has only one GATT client. Example:

```yaml
esp32_ble_tracker:
  id: cobi_scanner

cobi_ble_probe:
  id: raw_probe
  esp32_ble_id: cobi_scanner
  mac_address: 7C:57:4E:01:94:A1
```

A successful result should include `ESP_GATTC_CONNECT_EVT (event=40)`, `ESP_GATTC_OPEN_EVT (event=2)` with status 0, and service-discovery completion. If it reports the same `0x3e` before event 40, that points away from the `BLEClientBase` state machine; an alternate host (such as a standalone NimBLE test) would then be the next A/B test.
