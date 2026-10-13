# COBI raw Bluedroid connection probe

Experimental, connection-only diagnostic for separating ESPHome's `BLEClientBase` from the raw ESP-IDF GATT-client path.

The probe does **not** use the `ble_client` or `cobi_bike` components. It calls `esp_ble_gattc_open()` directly and handles the resulting GATT-client events itself. ESPHome's `esp32_ble_tracker` is intentionally retained for scanning and connection arbitration, and ESPHome still initializes the Bluedroid host/controller. This is therefore a staged test, not a complete replacement of ESPHome BLE.

For a matching advertisement, it makes a direct connection attempt, logs physical-connect and GATT-open events, performs read-only GATT service discovery, and closes the connection. It retries every `retry_interval` (default `30s`, `max_attempts: 0` = unlimited). It does not request bonding, enable notifications, read characteristic values, or write to the bike.

It also logs the target's raw advertisement (`ADV #n ... raw=<hex>`, on change or every 10 s) and decodes the COBI AIR manufacturer data (company `0x02A6`): `COBI adv: mode=... flags=0x.. pairing_mode=YES/no`. Use this to see whether the hub is in pairing mode; an unbonded ESP32 only connects while it is (see the main README).

Use it by itself in a temporary test configuration. Do not include `ble_client:` or `cobi_bike:` in that configuration, so the test has only one GATT client. Example:

```yaml
esp32_ble_tracker:
  id: cobi_scanner

cobi_ble_probe:
  id: raw_probe
  esp32_ble_id: cobi_scanner
  mac_address: 7C:57:4E:01:94:A1
  retry_interval: 30s   # optional
  max_attempts: 0       # optional, 0 = unlimited
```

Set `logger: logs: cobi_ble_probe: DEBUG` if the INFO lines don't appear.

A successful result should include `ESP_GATTC_CONNECT_EVT (event=40)`, `ESP_GATTC_OPEN_EVT (event=2)` with status 0, and service-discovery completion. Without pairing mode, expect `ESP_GATTC_DISCONNECT_EVT reason=0x100` then `OPEN_EVT status=133` after about 19 s and no event 40; with the hub in pairing mode the same probe connected in about 180 ms and discovered 6 services.
