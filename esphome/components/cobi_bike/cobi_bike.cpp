#include "cobi_bike.h"

#include "esphome/components/esp32_ble/ble_uuid.h"
#include "esphome/components/esp32_ble_client/ble_characteristic.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "esphome/core/log.h"

#include <algorithm>
#include <array>
#include <cstring>

#ifdef USE_ESP32
#include <esp_gattc_api.h>
#endif

namespace esphome::cobi_bike {

static const char *const TAG = "cobi_bike";
static constexpr char AIR_SERVICE_UUID[] = "c0b11802-fee1-c001-fee1-fa57fee15afe";
static constexpr char AIR_C2P_UUID[] = "c0b12a06-fee1-c001-fee1-fa57fee15afe";
static constexpr char AIR_P2C_UUID[] = "c0b12a07-fee1-c001-fee1-fa57fee15afe";
static constexpr char AIR_COUNTER_UUID[] = "c0b12a09-fee1-c001-fee1-fa57fee15afe";
static constexpr uint8_t HUB_CHANNEL = 3;
static constexpr uint8_t HUB_APPLICATION_MODE_PROPERTY = 21;
static constexpr uint8_t HUB_RIDE_LIFECYCLE_STATE_PROPERTY = 34;
static constexpr uint8_t APPLICATION_MODE_APP = 0;
static constexpr uint8_t RIDE_LIFECYCLE_AWAY = 0;
static constexpr uint8_t RIDE_LIFECYCLE_NOT_RIDABLE = 1;
static constexpr uint32_t LIFECYCLE_READ_RETRY_INTERVAL_MS = 2000;

static uint16_t read_u16_be(const uint8_t *data) {
  return (static_cast<uint16_t>(data[0]) << 8) | data[1];
}

static uint32_t read_u32_be(const uint8_t *data) {
  return (static_cast<uint32_t>(data[0]) << 24) | (static_cast<uint32_t>(data[1]) << 16) |
         (static_cast<uint32_t>(data[2]) << 8) | data[3];
}

void CobiBikeComponent::setup() { this->set_link_status_("Waiting for COBI BLE connection"); }

void CobiBikeComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "COBI AIR telemetry:");
  ESP_LOGCONFIG(TAG, "  Pair on connection: %s", this->pairing_enabled_ ? "YES" : "NO");
  ESP_LOGCONFIG(TAG, "  Lifecycle wake WRITE: %s", this->wake_hub_enabled_ ? "YES (opt-in)" : "NO");
  ESP_LOGCONFIG(TAG, "  Telemetry uses READ messages; bike-control commands are disabled");
}

void CobiBikeComponent::set_link_status_(const char *status) {
  if (this->link_status_sensor_ != nullptr)
    this->link_status_sensor_->publish_state(status);
}

void CobiBikeComponent::reset_session_() {
  this->c2p_handle_ = 0;
  this->p2c_handle_ = 0;
  this->counter_handle_ = 0;
  this->send_counter_ = 0;
  this->expected_receive_counter_ = 0;
  this->fragment_buffer_.clear();
  this->services_discovered_ = false;
  this->transport_initialization_started_ = false;
  this->p2c_registration_pending_ = false;
  this->p2c_notifications_ready_ = false;
  this->counter_registration_pending_ = false;
  this->counter_notifications_ready_ = true;
  this->counter_sync_pending_ = false;
  this->counter_sync_ready_ = true;
  this->session_ready_ = false;
  this->unknown_message_logged_ = false;
  this->application_mode_known_ = false;
  this->application_mode_is_app_ = false;
  this->lifecycle_state_received_ = false;
  this->hub_awake_ = false;
  this->wake_write_sent_ = false;
  this->last_lifecycle_state_read_ms_ = 0;
}

void CobiBikeComponent::update() {
  if (!this->session_ready_ || (this->wake_hub_enabled_ && !this->hub_awake_))
    return;
  this->send_read_batch_();
}

void CobiBikeComponent::loop() {
  if (!this->session_ready_ || !this->wake_hub_enabled_ || this->hub_awake_ ||
      (this->application_mode_known_ && !this->application_mode_is_app_))
    return;

  if (millis() - this->last_lifecycle_state_read_ms_ >= LIFECYCLE_READ_RETRY_INTERVAL_MS)
    this->request_hub_wake_state_();
}

void CobiBikeComponent::gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t,
                                             esp_ble_gattc_cb_param_t *param) {
  if (param == nullptr)
    return;

  switch (event) {
    case ESP_GATTC_CONNECT_EVT: {
      this->reset_session_();
      this->secure_link_ready_ = !this->pairing_enabled_;
      this->set_link_status_(this->pairing_enabled_ ? "Connected; requesting BLE pairing" : "Connected; discovering COBI AIR");
      if (this->pairing_enabled_ && this->parent() != nullptr) {
        const esp_err_t err = this->parent()->pair();
        if (err != ESP_OK)
          ESP_LOGW(TAG, "BLE pairing request returned error %d", err);
      }
      break;
    }
    case ESP_GATTC_SEARCH_CMPL_EVT:
      this->services_discovered_ = true;
      // Keep ESPHome's discovered GATT cache alive until pairing completes.
      if (!this->secure_link_ready_)
        this->node_state = esp32_ble_tracker::ClientState::CONNECTED;
      this->discover_transport_();
      this->maybe_initialize_transport_();
      break;
    case ESP_GATTC_REG_FOR_NOTIFY_EVT: {
      const uint16_t handle = param->reg_for_notify.handle;
      const bool succeeded = param->reg_for_notify.status == ESP_GATT_OK;
      if (handle == this->p2c_handle_) {
        this->p2c_registration_pending_ = false;
        this->p2c_notifications_ready_ = succeeded;
        if (!succeeded)
          ESP_LOGW(TAG, "Could not subscribe to COBI AIR P2C notifications (status=%d)", param->reg_for_notify.status);
      } else if (handle == this->counter_handle_) {
        this->counter_registration_pending_ = false;
        // Counter-mismatch indications are recovery support, not required for the first read.
        this->counter_notifications_ready_ = true;
        if (!succeeded)
          ESP_LOGW(TAG, "Counter-mismatch indication subscription failed (status=%d)", param->reg_for_notify.status);
      }
      this->maybe_initialize_transport_();
      break;
    }
    case ESP_GATTC_WRITE_CHAR_EVT:
      if (param->write.handle == this->counter_handle_ && this->counter_sync_pending_) {
        this->counter_sync_pending_ = false;
        this->counter_sync_ready_ = param->write.status == ESP_GATT_OK;
        if (!this->counter_sync_ready_)
          ESP_LOGW(TAG, "Could not initialize COBI AIR transport counter (status=%d)", param->write.status);
        this->maybe_initialize_transport_();
      }
      break;
    case ESP_GATTC_NOTIFY_EVT:
      if (param->notify.handle == this->p2c_handle_) {
        this->handle_transport_value_(param->notify.value, param->notify.value_len);
      } else if (param->notify.handle == this->counter_handle_ && param->notify.value_len >= 4) {
        // The hub reports the last valid client counter as a big-endian uint32.
        ESP_LOGW(TAG, "COBI AIR counter mismatch reported (%u); resetting transport sequence",
                 static_cast<unsigned>(read_u32_be(param->notify.value)));
        this->session_ready_ = false;
        this->send_counter_ = 0;
        this->counter_sync_ready_ = false;
        this->application_mode_known_ = false;
        this->application_mode_is_app_ = false;
        this->lifecycle_state_received_ = false;
        this->hub_awake_ = false;
        this->wake_write_sent_ = false;
        this->last_lifecycle_state_read_ms_ = 0;
        this->write_transport_counter_(this->send_counter_);
      }
      break;
    case ESP_GATTC_DISCONNECT_EVT:
    case ESP_GATTC_CLOSE_EVT:
      this->reset_session_();
      this->set_link_status_("Disconnected");
      break;
    default:
      break;
  }
}

void CobiBikeComponent::gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
  if (event != ESP_GAP_BLE_AUTH_CMPL_EVT || param == nullptr)
    return;
  if (param->ble_security.auth_cmpl.success) {
    this->secure_link_ready_ = true;
    this->node_state = esp32_ble_tracker::ClientState::CONNECTED;
    this->set_link_status_("BLE security ready; discovering COBI AIR");
    if (this->services_discovered_)
      this->discover_transport_();
  } else {
    this->secure_link_ready_ = false;
    this->node_state = esp32_ble_tracker::ClientState::ESTABLISHED;
    this->set_link_status_("BLE pairing failed");
    ESP_LOGW(TAG, "BLE authentication failed (reason=%d)", param->ble_security.auth_cmpl.fail_reason);
  }
}

void CobiBikeComponent::discover_transport_() {
  if (!this->services_discovered_ || !this->secure_link_ready_ || this->parent() == nullptr ||
      this->transport_initialization_started_)
    return;

  const auto service_uuid = esp32_ble_tracker::ESPBTUUID::from_raw(AIR_SERVICE_UUID);
  auto *c2p = this->parent()->get_characteristic(service_uuid, esp32_ble_tracker::ESPBTUUID::from_raw(AIR_C2P_UUID));
  auto *p2c = this->parent()->get_characteristic(service_uuid, esp32_ble_tracker::ESPBTUUID::from_raw(AIR_P2C_UUID));
  auto *counter = this->parent()->get_characteristic(service_uuid, esp32_ble_tracker::ESPBTUUID::from_raw(AIR_COUNTER_UUID));

  if (c2p == nullptr || p2c == nullptr) {
    this->set_link_status_("COBI AIR transport characteristics not found");
    ESP_LOGW(TAG, "Expected COBI AIR C2P/P2C characteristics were not found; this may be a COBI Pro hub or an incomplete UUID map");
    return;
  }

  this->c2p_handle_ = c2p->handle;
  this->p2c_handle_ = p2c->handle;
  this->counter_handle_ = counter == nullptr ? 0 : counter->handle;
  this->transport_initialization_started_ = true;
  this->set_link_status_("COBI AIR transport found; enabling telemetry notifications");

  this->p2c_notifications_ready_ = false;
  this->p2c_registration_pending_ = true;
  esp_err_t err = this->parent()->register_for_notify(this->p2c_handle_);
  if (err != ESP_OK) {
    this->p2c_registration_pending_ = false;
    this->set_link_status_("Could not subscribe to COBI notifications");
    ESP_LOGW(TAG, "P2C notification registration failed (error=%d)", err);
    return;
  }

  this->counter_notifications_ready_ = this->counter_handle_ == 0;
  if (this->counter_handle_ != 0) {
    this->counter_registration_pending_ = true;
    err = this->parent()->register_for_notify(this->counter_handle_);
    if (err != ESP_OK) {
      this->counter_registration_pending_ = false;
      this->counter_notifications_ready_ = true;
      ESP_LOGW(TAG, "Counter-mismatch indication registration failed (error=%d)", err);
    }
    this->counter_sync_ready_ = false;
    this->write_transport_counter_(this->send_counter_);
  } else {
    this->counter_sync_ready_ = true;
    ESP_LOGW(TAG, "COBI AIR counter characteristic is absent; counter recovery will not be available");
  }

  this->maybe_initialize_transport_();
}

void CobiBikeComponent::write_transport_counter_(uint8_t value) {
  if (this->counter_handle_ == 0 || this->parent() == nullptr)
    return;
  this->counter_sync_pending_ = true;
  const esp_err_t err = esp_ble_gattc_write_char(
      this->parent()->get_gattc_if(), this->parent()->get_conn_id(), this->counter_handle_, sizeof(value), &value,
      ESP_GATT_WRITE_TYPE_RSP, ESP_GATT_AUTH_REQ_NONE);
  if (err != ESP_OK) {
    this->counter_sync_pending_ = false;
    this->counter_sync_ready_ = false;
    this->set_link_status_("COBI AIR counter synchronization failed");
    ESP_LOGW(TAG, "COBI AIR counter write failed (error=%d)", err);
  }
}

bool CobiBikeComponent::send_bus_payload_(const uint8_t *payload, size_t length, const char *description) {
  if (!this->session_ready_ || this->c2p_handle_ == 0 || this->parent() == nullptr || payload == nullptr || length == 0)
    return false;
  if (length > 8) {
    ESP_LOGW(TAG, "Refusing oversized COBI bus payload (%u bytes)", static_cast<unsigned>(length));
    return false;
  }

  // COBI AIR COMPLETE packets carry up to eight bus bytes in a compact header.
  std::array<uint8_t, 10> frame{};
  frame[0] = this->send_counter_;
  frame[1] = static_cast<uint8_t>((length << 3) | 0x03);  // PacketType.COMPLETE
  std::copy_n(payload, length, frame.begin() + 2);

  const esp_err_t err = esp_ble_gattc_write_char(
      this->parent()->get_gattc_if(), this->parent()->get_conn_id(), this->c2p_handle_, length + 2, frame.data(),
      ESP_GATT_WRITE_TYPE_NO_RSP, ESP_GATT_AUTH_REQ_NONE);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "COBI %s write failed (error=%d)", description, err);
    return false;
  }

  this->send_counter_++;
  ESP_LOGV(TAG, "Sent COBI %s (%u bus bytes)", description, static_cast<unsigned>(length));
  return true;
}

void CobiBikeComponent::request_hub_wake_state_() {
  if (!this->wake_hub_enabled_ || !this->session_ready_)
    return;

  const uint8_t read_header = static_cast<uint8_t>(HUB_CHANNEL << 2);  // V2 Action.READ
  const std::array<uint8_t, 4> bus = {read_header, HUB_APPLICATION_MODE_PROPERTY, read_header,
                                     HUB_RIDE_LIFECYCLE_STATE_PROPERTY};
  this->last_lifecycle_state_read_ms_ = millis();
  this->send_bus_payload_(bus.data(), bus.size(), "hub state READs");
}

void CobiBikeComponent::maybe_wake_hub_() {
  if (!this->wake_hub_enabled_ || !this->session_ready_ || !this->lifecycle_state_received_ || this->hub_awake_ ||
      this->wake_write_sent_ || !this->application_mode_known_)
    return;

  if (!this->application_mode_is_app_) {
    this->set_link_status_("Hub update mode; lifecycle wake skipped");
    return;
  }

  const std::array<uint8_t, 3> bus = {
      static_cast<uint8_t>((HUB_CHANNEL << 2) | 1),  // V2 Action.WRITE
      HUB_RIDE_LIFECYCLE_STATE_PROPERTY,
      RIDE_LIFECYCLE_NOT_RIDABLE,
  };
  if (this->send_bus_payload_(bus.data(), bus.size(), "optional lifecycle wake WRITE")) {
    this->wake_write_sent_ = true;
    this->set_link_status_("Hub asleep; sent lifecycle wake request");
    ESP_LOGI(TAG, "Hub reported AWAY; sent optional rideLifecycleState=NOT_RIDABLE wake WRITE");
  }
}

void CobiBikeComponent::handle_lifecycle_state_(uint8_t state) {
  const bool was_awake = this->hub_awake_;
  this->lifecycle_state_received_ = true;
  this->hub_awake_ = state != RIDE_LIFECYCLE_AWAY;

  if (this->hub_awake_) {
    this->wake_write_sent_ = false;
    if (!was_awake && this->session_ready_) {
      this->set_link_status_("COBI AIR hub awake; read-only telemetry active");
      ESP_LOGI(TAG, "Hub lifecycle state is %u; starting telemetry READs", static_cast<unsigned>(state));
      this->update();
    }
    return;
  }

  if (was_awake)
    this->wake_write_sent_ = false;
  if (!this->wake_hub_enabled_)
    return;
  if (!this->application_mode_known_) {
    this->set_link_status_("Hub asleep; checking application mode before wake");
    return;
  }
  this->maybe_wake_hub_();
}

void CobiBikeComponent::maybe_initialize_transport_() {
  if (!this->services_discovered_ || !this->secure_link_ready_ || !this->transport_initialization_started_ ||
      !this->p2c_notifications_ready_ || !this->counter_notifications_ready_ || !this->counter_sync_ready_ ||
      this->session_ready_)
    return;
  this->session_ready_ = true;
  if (this->wake_hub_enabled_) {
    this->set_link_status_("Connected to COBI AIR; checking hub lifecycle");
    ESP_LOGI(TAG, "COBI AIR session ready; checking lifecycle before optional wake");
    this->request_hub_wake_state_();
  } else {
    this->set_link_status_("Connected to COBI AIR; read-only telemetry active");
    ESP_LOGI(TAG, "COBI AIR telemetry session ready; bike-control messages are disabled");
    this->update();
  }
}

void CobiBikeComponent::send_read_batch_() {
  if (!this->session_ready_ || (this->wake_hub_enabled_ && !this->hub_awake_) || this->c2p_handle_ == 0 ||
      this->parent() == nullptr)
    return;

  std::array<uint8_t, 8> bus{};
  size_t length = 0;
  const auto append_read = [&bus, &length](uint8_t channel, uint8_t property) {
    bus[length++] = static_cast<uint8_t>(channel << 2);  // V2 Action.READ
    bus[length++] = property;
  };

  if (this->traction_battery_level_sensor_ != nullptr || this->traction_battery_state_sensor_ != nullptr)
    append_read(5, 6);  // Battery.state
  if (this->battery_health_soh_sensor_ != nullptr || this->battery_voltage_raw_sensor_ != nullptr ||
      this->battery_current_raw_sensor_ != nullptr || this->battery_capacity_raw_sensor_ != nullptr)
    append_read(5, 8);  // Battery.health
  if (this->hub_battery_level_sensor_ != nullptr || this->hub_battery_state_sensor_ != nullptr)
    append_read(3, 30);  // Hub.internalBatteryState (distinct from traction battery)
  if (this->motor_power_on_time_raw_sensor_ != nullptr)
    append_read(7, 29);  // Motor.powerOnTime

  if (length == 0)
    return;

  this->send_bus_payload_(bus.data(), length, "telemetry READ batch");
}

void CobiBikeComponent::handle_transport_value_(const uint8_t *data, size_t length) {
  if (data == nullptr || length < 2)
    return;

  const uint8_t counter = data[0];
  if (counter != this->expected_receive_counter_) {
    ESP_LOGW(TAG, "COBI P2C transport counter was %u; expected %u", counter, this->expected_receive_counter_);
    this->fragment_buffer_.clear();
  }
  this->expected_receive_counter_ = static_cast<uint8_t>(counter + 1);

  size_t offset = 1;
  while (offset < length) {
    const uint8_t header = data[offset++];
    const uint8_t type = header & 0x03;
    size_t packet_length = header >> 3;
    if ((header & 0x04) != 0) {
      if (offset >= length) {
        ESP_LOGW(TAG, "Truncated COBI extended transport header");
        this->fragment_buffer_.clear();
        return;
      }
      packet_length = (packet_length << 8) | data[offset++];
    }
    if (packet_length > length - offset) {
      ESP_LOGW(TAG, "Truncated COBI transport packet");
      this->fragment_buffer_.clear();
      return;
    }
    this->handle_packet_(type, data + offset, packet_length);
    offset += packet_length;
  }
}

void CobiBikeComponent::handle_packet_(uint8_t type, const uint8_t *data, size_t length) {
  switch (type) {
    case 3:  // COMPLETE
      this->fragment_buffer_.clear();
      this->handle_bus_messages_(data, length);
      break;
    case 1:  // START
      this->fragment_buffer_.assign(data, data + length);
      break;
    case 0:  // CONTINUE
      if (!this->fragment_buffer_.empty())
        this->fragment_buffer_.insert(this->fragment_buffer_.end(), data, data + length);
      else
        ESP_LOGW(TAG, "Unexpected COBI CONTINUE packet");
      break;
    case 2:  // END
      if (this->fragment_buffer_.empty()) {
        ESP_LOGW(TAG, "Unexpected COBI END packet");
      } else {
        this->fragment_buffer_.insert(this->fragment_buffer_.end(), data, data + length);
        this->handle_bus_messages_(this->fragment_buffer_.data(), this->fragment_buffer_.size());
        this->fragment_buffer_.clear();
      }
      break;
    default:
      break;
  }
}

size_t CobiBikeComponent::known_payload_length_(uint8_t action, uint8_t channel, uint8_t property) const {
  if (action == 0)  // READ messages have no value payload.
    return 0;
  if (action != 2)  // Ignore unrequested writes.
    return SIZE_MAX;

  if (channel == HUB_CHANNEL && property == HUB_APPLICATION_MODE_PROPERTY)
    return 1;  // ApplicationMode is a one-byte enum.
  if (channel == HUB_CHANNEL && property == HUB_RIDE_LIFECYCLE_STATE_PROPERTY)
    return 1;  // RideLifecycleState is a one-byte enum.
  if (channel == 5 && property == 6)
    return 2;  // BatteryCondition: uint8 percentage + enum byte
  if (channel == 5 && property == 8)
    return 7;  // BatteryHealth: uint16, int16, uint16, uint8
  if (channel == 3 && property == 30)
    return 2;  // Hub internal BatteryCondition
  if (channel == 7 && property == 29)
    return 4;  // uint32 Motor.powerOnTime
  return SIZE_MAX;
}

void CobiBikeComponent::handle_bus_messages_(const uint8_t *data, size_t length) {
  size_t offset = 0;
  while (offset + 2 <= length) {
    const uint8_t header = data[offset++];
    const uint8_t action = header & 0x03;
    const uint8_t channel = header >> 2;
    const uint8_t property = data[offset++];
    const size_t payload_length = this->known_payload_length_(action, channel, property);
    if (payload_length == SIZE_MAX || payload_length > length - offset) {
      if (!this->unknown_message_logged_) {
        ESP_LOGW(TAG, "Received an unsupported COBI bus message; ignoring the rest of this batch");
        this->unknown_message_logged_ = true;
      }
      return;
    }
    this->handle_bus_message_(action, channel, property, data + offset, payload_length);
    offset += payload_length;
  }
  if (offset != length)
    ESP_LOGW(TAG, "Incomplete COBI V2 message at end of batch");
}

void CobiBikeComponent::handle_bus_message_(uint8_t action, uint8_t channel, uint8_t property, const uint8_t *payload,
                                              size_t payload_length) {
  if (action != 2 || payload == nullptr)
    return;

  if (channel == HUB_CHANNEL && property == HUB_APPLICATION_MODE_PROPERTY && payload_length == 1) {
    this->application_mode_known_ = true;
    this->application_mode_is_app_ = payload[0] == APPLICATION_MODE_APP;
    if (this->lifecycle_state_received_ && !this->hub_awake_)
      this->maybe_wake_hub_();
    return;
  }

  if (channel == HUB_CHANNEL && property == HUB_RIDE_LIFECYCLE_STATE_PROPERTY && payload_length == 1) {
    this->handle_lifecycle_state_(payload[0]);
    return;
  }

  if (channel == 5 && property == 6 && payload_length == 2) {
    if (payload[0] <= 100 && this->traction_battery_level_sensor_ != nullptr)
      this->traction_battery_level_sensor_->publish_state(payload[0]);
    if (this->traction_battery_state_sensor_ != nullptr)
      this->traction_battery_state_sensor_->publish_state(this->battery_state_name_(payload[1]));
    return;
  }

  if (channel == 5 && property == 8 && payload_length == 7) {
    const uint16_t voltage_raw = read_u16_be(payload);
    const int16_t current_raw = static_cast<int16_t>(read_u16_be(payload + 2));
    const uint16_t capacity_raw = read_u16_be(payload + 4);
    const uint8_t state_of_health = payload[6];
    if (this->battery_voltage_raw_sensor_ != nullptr)
      this->battery_voltage_raw_sensor_->publish_state(voltage_raw);
    if (this->battery_current_raw_sensor_ != nullptr)
      this->battery_current_raw_sensor_->publish_state(current_raw);
    if (this->battery_capacity_raw_sensor_ != nullptr)
      this->battery_capacity_raw_sensor_->publish_state(capacity_raw);
    if (state_of_health <= 100 && this->battery_health_soh_sensor_ != nullptr)
      this->battery_health_soh_sensor_->publish_state(state_of_health);
    return;
  }

  if (channel == 3 && property == 30 && payload_length == 2) {
    if (payload[0] <= 100 && this->hub_battery_level_sensor_ != nullptr)
      this->hub_battery_level_sensor_->publish_state(payload[0]);
    if (this->hub_battery_state_sensor_ != nullptr)
      this->hub_battery_state_sensor_->publish_state(this->battery_state_name_(payload[1]));
    return;
  }

  if (channel == 7 && property == 29 && payload_length == 4 && this->motor_power_on_time_raw_sensor_ != nullptr)
    this->motor_power_on_time_raw_sensor_->publish_state(read_u32_be(payload));
}

const char *CobiBikeComponent::battery_state_name_(uint8_t state) {
  switch (state) {
    case 0:
      return "UNKNOWN";
    case 1:
      return "CHARGING";
    case 2:
      return "LOW_CHARGE";
    case 3:
      return "INTERMEDIATE_CHARGE";
    case 4:
      return "FULL_CHARGE";
    default:
      return "UNRECOGNIZED";
  }
}

}  // namespace esphome::cobi_bike
