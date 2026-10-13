#pragma once

#include "esphome/components/ble_client/ble_client.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/core/component.h"

#include <array>
#include <cstdint>
#include <vector>

namespace esphome::cobi_bike {

class CobiBikeComponent final : public PollingComponent, public ble_client::BLEClientNode {
 public:
  void setup() override;
  void dump_config() override;
  void update() override;
  void loop() override;

  void set_discovery_scan(bool enabled) { this->discovery_scan_enabled_ = enabled; }
  void set_pairing(bool enabled) { this->pairing_enabled_ = enabled; }
  void set_wake_hub(bool enabled) { this->wake_hub_enabled_ = enabled; }
  void set_traction_battery_level_sensor(sensor::Sensor *sensor) { this->traction_battery_level_sensor_ = sensor; }
  void set_traction_battery_state_sensor(text_sensor::TextSensor *sensor) { this->traction_battery_state_sensor_ = sensor; }
  void set_battery_health_soh_sensor(sensor::Sensor *sensor) { this->battery_health_soh_sensor_ = sensor; }
  void set_battery_voltage_raw_sensor(sensor::Sensor *sensor) { this->battery_voltage_raw_sensor_ = sensor; }
  void set_battery_current_raw_sensor(sensor::Sensor *sensor) { this->battery_current_raw_sensor_ = sensor; }
  void set_battery_capacity_raw_sensor(sensor::Sensor *sensor) { this->battery_capacity_raw_sensor_ = sensor; }
  void set_hub_battery_level_sensor(sensor::Sensor *sensor) { this->hub_battery_level_sensor_ = sensor; }
  void set_hub_battery_state_sensor(text_sensor::TextSensor *sensor) { this->hub_battery_state_sensor_ = sensor; }
  void set_motor_power_on_time_raw_sensor(sensor::Sensor *sensor) { this->motor_power_on_time_raw_sensor_ = sensor; }
  void set_odometer_sensor(sensor::Sensor *sensor) { this->odometer_sensor_ = sensor; }
  void set_motor_range_sensor(sensor::Sensor *sensor) { this->motor_range_sensor_ = sensor; }
  void set_battery_range_sensor(sensor::Sensor *sensor) { this->battery_range_sensor_ = sensor; }
  void set_speed_sensor(sensor::Sensor *sensor) { this->speed_sensor_ = sensor; }
  void set_cadence_sensor(sensor::Sensor *sensor) { this->cadence_sensor_ = sensor; }
  void set_rider_power_sensor(sensor::Sensor *sensor) { this->rider_power_sensor_ = sensor; }
  void set_assist_level_sensor(sensor::Sensor *sensor) { this->assist_level_sensor_ = sensor; }
  void set_next_service_distance_sensor(sensor::Sensor *sensor) { this->next_service_distance_sensor_ = sensor; }
  void set_wheel_circumference_sensor(sensor::Sensor *sensor) { this->wheel_circumference_sensor_ = sensor; }
  void set_drive_mode_sensor(sensor::Sensor *sensor) { this->drive_mode_sensor_ = sensor; }
  void set_supported_drive_modes_sensor(sensor::Sensor *sensor) { this->supported_drive_modes_sensor_ = sensor; }
  void set_frame_acceleration_forward_sensor(sensor::Sensor *sensor) { this->frame_acceleration_forward_sensor_ = sensor; }
  void set_frame_acceleration_vertical_sensor(sensor::Sensor *sensor) { this->frame_acceleration_vertical_sensor_ = sensor; }
  void set_motor_type_sensor(text_sensor::TextSensor *sensor) { this->motor_type_sensor_ = sensor; }
  void set_motor_firmware_sensor(text_sensor::TextSensor *sensor) { this->motor_firmware_sensor_ = sensor; }
  void set_motor_part_number_sensor(text_sensor::TextSensor *sensor) { this->motor_part_number_sensor_ = sensor; }
  void set_battery_type_sensor(text_sensor::TextSensor *sensor) { this->battery_type_sensor_ = sensor; }
  void set_battery_firmware_sensor(text_sensor::TextSensor *sensor) { this->battery_firmware_sensor_ = sensor; }
  void set_battery_part_number_sensor(text_sensor::TextSensor *sensor) { this->battery_part_number_sensor_ = sensor; }
  void set_hub_firmware_sensor(text_sensor::TextSensor *sensor) { this->hub_firmware_sensor_ = sensor; }
  void set_display_interface_sensor(text_sensor::TextSensor *sensor) { this->display_interface_sensor_ = sensor; }
  void set_link_status_sensor(text_sensor::TextSensor *sensor) { this->link_status_sensor_ = sensor; }

  void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                          esp_ble_gattc_cb_param_t *param) override;
  void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) override;

 protected:
  void set_link_status_(const char *status);
  void reset_session_();
  bool is_bonded_(const uint8_t *remote_bda) const;
  void fail_session_(const char *reason);
  void discover_transport_();
  void maybe_initialize_transport_();
  void write_transport_counter_(uint8_t value);
  void request_hub_wake_state_();
  void maybe_wake_hub_();
  void handle_lifecycle_state_(uint8_t state);
  bool send_bus_payload_(const uint8_t *payload, size_t length, const char *description);
  void send_read_batch_();
  void queue_read_(uint8_t channel, uint8_t property);
  void drain_read_queue_();
  void handle_string_message_(uint8_t channel, uint8_t property, const uint8_t *payload, size_t payload_length);
  void handle_transport_value_(const uint8_t *data, size_t length);
  void handle_packet_(uint8_t type, const uint8_t *data, size_t length);
  void handle_bus_messages_(const uint8_t *data, size_t length);
  void handle_bus_message_(uint8_t action, uint8_t channel, uint8_t property, const uint8_t *payload,
                           size_t payload_length);
  size_t known_payload_length_(uint8_t action, uint8_t channel, uint8_t property) const;
  static const char *battery_state_name_(uint8_t state);

  sensor::Sensor *traction_battery_level_sensor_{nullptr};
  text_sensor::TextSensor *traction_battery_state_sensor_{nullptr};
  sensor::Sensor *battery_health_soh_sensor_{nullptr};
  sensor::Sensor *battery_voltage_raw_sensor_{nullptr};
  sensor::Sensor *battery_current_raw_sensor_{nullptr};
  sensor::Sensor *battery_capacity_raw_sensor_{nullptr};
  sensor::Sensor *hub_battery_level_sensor_{nullptr};
  text_sensor::TextSensor *hub_battery_state_sensor_{nullptr};
  sensor::Sensor *motor_power_on_time_raw_sensor_{nullptr};
  sensor::Sensor *odometer_sensor_{nullptr};
  sensor::Sensor *motor_range_sensor_{nullptr};
  sensor::Sensor *battery_range_sensor_{nullptr};
  sensor::Sensor *speed_sensor_{nullptr};
  sensor::Sensor *cadence_sensor_{nullptr};
  sensor::Sensor *rider_power_sensor_{nullptr};
  sensor::Sensor *assist_level_sensor_{nullptr};
  sensor::Sensor *next_service_distance_sensor_{nullptr};
  sensor::Sensor *wheel_circumference_sensor_{nullptr};
  sensor::Sensor *drive_mode_sensor_{nullptr};
  sensor::Sensor *supported_drive_modes_sensor_{nullptr};
  sensor::Sensor *frame_acceleration_forward_sensor_{nullptr};
  sensor::Sensor *frame_acceleration_vertical_sensor_{nullptr};
  text_sensor::TextSensor *motor_type_sensor_{nullptr};
  text_sensor::TextSensor *motor_firmware_sensor_{nullptr};
  text_sensor::TextSensor *motor_part_number_sensor_{nullptr};
  text_sensor::TextSensor *battery_type_sensor_{nullptr};
  text_sensor::TextSensor *battery_firmware_sensor_{nullptr};
  text_sensor::TextSensor *battery_part_number_sensor_{nullptr};
  text_sensor::TextSensor *hub_firmware_sensor_{nullptr};
  text_sensor::TextSensor *display_interface_sensor_{nullptr};
  text_sensor::TextSensor *link_status_sensor_{nullptr};
  std::array<uint16_t, 16> unsupported_seen_{};
  size_t unsupported_seen_count_{0};

  uint16_t c2p_handle_{0};
  uint16_t p2c_handle_{0};
  uint16_t counter_handle_{0};
  uint8_t send_counter_{0};
  uint8_t expected_receive_counter_{0};
  std::vector<uint8_t> fragment_buffer_;

  bool pairing_enabled_{false};
  bool wake_hub_enabled_{false};
  bool application_mode_known_{false};
  bool application_mode_is_app_{false};
  bool lifecycle_state_received_{false};
  bool hub_awake_{false};
  bool wake_write_sent_{false};
  uint32_t last_lifecycle_state_read_ms_{0};
  bool secure_link_ready_{true};
  bool services_discovered_{false};
  bool transport_initialization_started_{false};
  bool p2c_registration_pending_{false};
  bool p2c_notifications_ready_{false};
  bool counter_registration_pending_{false};
  bool counter_notifications_ready_{true};
  bool counter_sync_pending_{false};
  bool counter_sync_ready_{true};
  bool session_ready_{false};
  bool unknown_message_logged_{false};
  std::vector<std::array<uint8_t, 2>> pending_reads_;
  uint32_t last_read_write_ms_{0};
  bool static_info_requested_{false};
  // Reconnect robustness: encryption is requested on every connection to a bonded hub (not only with `pair: true`),
  // and a session that does not become ready in time is dropped so ESPHome's auto-connect can retry.
  bool connected_{false};
  bool encryption_requested_{false};
  bool force_encryption_{false};
  bool session_failed_{false};
  uint32_t connected_at_ms_{0};
  bool discovery_scan_enabled_{false};
  bool discovery_scan_started_{false};
  size_t discovery_scan_index_{0};
  uint32_t discovery_scan_last_ms_{0};
  uint32_t discovery_scan_logged_{0};
};

}  // namespace esphome::cobi_bike
