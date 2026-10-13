#pragma once

#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "esphome/core/component.h"

#include <esp_gap_ble_api.h>
#include <esp_gattc_api.h>

#include <cstdint>

namespace esphome::cobi_ble_probe {

class CobiBleProbe final : public esp32_ble_tracker::ESPBTClient, public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override;

  bool parse_device(const esp32_ble_tracker::ESPBTDevice &device) override;
  bool gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                           esp_ble_gattc_cb_param_t *param) override;
  void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) override;
  void connect() override;
  void disconnect() override;
  void ble_before_disabled_event_handler() override;

  void set_target_address(uint64_t address);

 protected:
  static constexpr uint16_t NO_CONN_ID = 0xFFFF;

  bool is_target_address_(const esp_bd_addr_t address) const;
  void close_connection_(const char *reason);
  void finish_connection_();

  uint64_t target_address_{0};
  esp_bd_addr_t remote_bda_{};
  esp_ble_addr_type_t remote_addr_type_{BLE_ADDR_TYPE_PUBLIC};
  esp_gatt_if_t gattc_if_{ESP_GATT_IF_NONE};
  uint16_t conn_id_{NO_CONN_ID};
  uint16_t discovered_service_count_{0};
  uint32_t disconnecting_started_ms_{0};

  bool registration_requested_{false};
  bool attempt_started_{false};
  bool gatt_open_{false};
  bool service_search_started_{false};
  bool close_requested_{false};
};

}  // namespace esphome::cobi_ble_probe
