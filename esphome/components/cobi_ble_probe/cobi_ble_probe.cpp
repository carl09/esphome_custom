#include "cobi_ble_probe.h"

#include "esphome/components/esp32_ble/ble.h"
#include "esphome/core/log.h"

#include <esp_gatt_defs.h>

#include <cstring>

namespace esphome::cobi_ble_probe {

static const char *const TAG = "cobi_ble_probe";
static constexpr uint32_t DISCONNECTING_TIMEOUT_MS = 10000;

void CobiBleProbe::set_target_address(uint64_t address) {
  this->target_address_ = address;
  for (size_t i = 0; i < sizeof(this->remote_bda_); i++) {
    this->remote_bda_[i] = static_cast<uint8_t>((address >> ((5 - i) * 8)) & 0xFF);
  }
}

void CobiBleProbe::setup() {
  if (this->target_address_ == 0) {
    ESP_LOGE(TAG, "A non-zero target MAC address is required");
    this->mark_failed();
    return;
  }
  ESP_LOGI(TAG, "Raw Bluedroid probe ready: direct connection attempts (retried), service discovery only, no pairing or writes");
}

void CobiBleProbe::dump_config() {
  uint8_t address[6];
  for (size_t i = 0; i < sizeof(address); i++)
    address[i] = static_cast<uint8_t>((this->target_address_ >> ((5 - i) * 8)) & 0xFF);
  char address_str[MAC_ADDRESS_PRETTY_BUFFER_SIZE];
  format_mac_addr_upper(address, address_str);
  ESP_LOGCONFIG(TAG, "Raw Bluedroid BLE probe:");
  ESP_LOGCONFIG(TAG, "  Target: %s", address_str);
  ESP_LOGCONFIG(TAG, "  Retry interval: %u ms; max attempts: %u (0 = unlimited)", (unsigned) this->retry_interval_ms_,
                (unsigned) this->max_attempts_);
  ESP_LOGCONFIG(TAG, "  Pairing: disabled; GATT writes: disabled");
}

float CobiBleProbe::get_setup_priority() const { return setup_priority::AFTER_BLUETOOTH; }

void CobiBleProbe::loop() {
  if (this->state() == esp32_ble_tracker::ClientState::DISCONNECTING) {
    if (millis() - this->disconnecting_started_ms_ >= DISCONNECTING_TIMEOUT_MS) {
      ESP_LOGW(TAG, "Timed out waiting for GATT close/disconnect event; returning client state to IDLE");
      this->finish_connection_();
    }
    return;
  }

  if (this->state() != esp32_ble_tracker::ClientState::INIT || this->registration_requested_)
    return;
  if (esp32_ble::global_ble == nullptr || !esp32_ble::global_ble->is_active())
    return;

  this->registration_requested_ = true;
  const esp_err_t err = esp_ble_gattc_app_register(this->app_id);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_ble_gattc_app_register failed: %s (%d)", esp_err_to_name(err), err);
    this->mark_failed();
    return;
  }
  // Registration completes asynchronously; the event callback will move us to IDLE.
  this->disable_loop();
}

bool CobiBleProbe::parse_device(const esp32_ble_tracker::ESPBTDevice &device) {
  if (device.address_uint64() != this->target_address_)
    return false;

  // Dump the raw advertisement (+ scan response) so connectability/flags/UUIDs/manufacturer data can be inspected.
  // Logged when the payload changes or every 10 s.
  this->adv_count_++;
  const auto &sr = device.get_scan_result();
  const size_t len = static_cast<size_t>(sr.adv_data_len) + sr.scan_rsp_len;
  uint8_t hash = 0;
  for (size_t i = 0; i < len; i++)
    hash = static_cast<uint8_t>(hash * 31 + sr.ble_adv[i]);
  if (hash != this->last_adv_hash_ || millis() - this->last_adv_log_ms_ >= 10000) {
    this->last_adv_hash_ = hash;
    this->last_adv_log_ms_ = millis();
    char hex[(ESP_BLE_ADV_DATA_LEN_MAX + ESP_BLE_SCAN_RSP_DATA_LEN_MAX) * 2 + 1];
    format_hex_to(hex, sizeof(hex), sr.ble_adv, len);
    ESP_LOGI(TAG, "ADV #%u rssi=%d addr_type=%u adv_len=%u scan_rsp_len=%u flags=%s name='%s' raw=%s",
             (unsigned) this->adv_count_, device.get_rssi(), (unsigned) device.get_address_type(),
             (unsigned) sr.adv_data_len, (unsigned) sr.scan_rsp_len,
             device.get_ad_flag().has_value() ? "present" : "absent", device.get_name().c_str(), hex);
  }

  // COBI AIR advertisement (Bosch company id 0x02A6), layout from the app's AirAdvertisementInformationFactory:
  // [0..1] component id, [2] version, [3] component type, [4] data format, [5] device mode (1=bootloader, 2=app),
  // [6] flags (bit0 = pairing mode, bit1 = bonding issue), [8..20] serial number.
  static uint8_t last_flags = 0xFF;
  for (const auto &md : device.get_manufacturer_datas()) {
    if (md.uuid.get_uuid().len != ESP_UUID_LEN_16 || md.uuid.get_uuid().uuid.uuid16 != 0x02A6 || md.data.size() < 7)
      continue;
    if (md.data[6] != last_flags) {
      last_flags = md.data[6];
      ESP_LOGI(TAG, "COBI adv: mode=%u flags=0x%02X pairing_mode=%s bonding_issue=%s data_format=%u", md.data[5],
               md.data[6], (md.data[6] & 1) ? "YES" : "no", (md.data[6] & 2) ? "YES" : "no", md.data[4]);
    }
  }

  if (this->state() != esp32_ble_tracker::ClientState::IDLE || this->gattc_if_ == ESP_GATT_IF_NONE)
    return false;
  if (this->max_attempts_ != 0 && this->attempts_ >= this->max_attempts_)
    return false;
  if (this->attempts_ != 0 && millis() - this->attempt_finished_ms_ < this->retry_interval_ms_)
    return false;

  std::memcpy(this->remote_bda_, device.address(), sizeof(this->remote_bda_));
  this->remote_addr_type_ = device.get_address_type();
  this->set_state(esp32_ble_tracker::ClientState::DISCOVERED);

  char address_str[MAC_ADDRESS_PRETTY_BUFFER_SIZE];
  format_mac_addr_upper(this->remote_bda_, address_str);
  ESP_LOGI(TAG, "Found target %s (RSSI=%d, addr_type=%u); scheduling raw direct GATT open (attempt %u)", address_str,
           device.get_rssi(), static_cast<unsigned>(this->remote_addr_type_), (unsigned) (this->attempts_ + 1));
  return true;
}

void CobiBleProbe::connect() {
  if (this->state() != esp32_ble_tracker::ClientState::DISCOVERED || this->gattc_if_ == ESP_GATT_IF_NONE)
    return;

  this->attempts_++;
  this->attempt_started_ms_ = millis();
  this->conn_id_ = NO_CONN_ID;
  this->discovered_service_count_ = 0;
  this->gatt_open_ = false;
  this->service_search_started_ = false;
  this->close_requested_ = false;
  this->set_state(esp32_ble_tracker::ClientState::CONNECTING);

  char address_str[MAC_ADDRESS_PRETTY_BUFFER_SIZE];
  format_mac_addr_upper(this->remote_bda_, address_str);
  ESP_LOGI(TAG, "Calling esp_ble_gattc_open directly for %s (addr_type=%u, direct=true)", address_str,
           static_cast<unsigned>(this->remote_addr_type_));

  const esp_err_t err = esp_ble_gattc_open(this->gattc_if_, this->remote_bda_, this->remote_addr_type_, true);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "esp_ble_gattc_open returned %s (%d)", esp_err_to_name(err), err);
    this->finish_connection_();
  }
}

void CobiBleProbe::disconnect() { this->close_connection_("tracker requested disconnect"); }

void CobiBleProbe::ble_before_disabled_event_handler() {
  this->gattc_if_ = ESP_GATT_IF_NONE;
  this->conn_id_ = NO_CONN_ID;
  this->registration_requested_ = false;
  this->gatt_open_ = false;
  this->service_search_started_ = false;
  this->close_requested_ = false;
  this->set_state(esp32_ble_tracker::ClientState::INIT);
  this->enable_loop();
}

bool CobiBleProbe::is_target_address_(const esp_bd_addr_t address) const {
  return std::memcmp(address, this->remote_bda_, sizeof(this->remote_bda_)) == 0;
}

bool CobiBleProbe::gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                                       esp_ble_gattc_cb_param_t *param) {
  if (param == nullptr)
    return false;

  if (event == ESP_GATTC_REG_EVT) {
    if (param->reg.app_id != this->app_id)
      return false;
    if (param->reg.status != ESP_GATT_OK) {
      ESP_LOGE(TAG, "GATT app registration failed (status=%d)", param->reg.status);
      this->mark_failed();
      return true;
    }
    this->gattc_if_ = gattc_if;
    ESP_LOGI(TAG, "Raw GATT client registered (app_id=%u, gattc_if=%d)", static_cast<unsigned>(this->app_id),
             this->gattc_if_);
    this->set_state(esp32_ble_tracker::ClientState::IDLE);
    return true;
  }

  if (this->gattc_if_ == ESP_GATT_IF_NONE ||
      (gattc_if != ESP_GATT_IF_NONE && gattc_if != this->gattc_if_))
    return false;

  switch (event) {
    case ESP_GATTC_CONNECT_EVT:
      if (!this->is_target_address_(param->connect.remote_bda))
        return false;
      this->conn_id_ = param->connect.conn_id;
      this->set_state(esp32_ble_tracker::ClientState::CONNECTED);
      ESP_LOGI(TAG, "ESP_GATTC_CONNECT_EVT (event=40): physical BLE link established, conn_id=%u",
               static_cast<unsigned>(this->conn_id_));
      break;

    case ESP_GATTC_OPEN_EVT:
      if (!this->is_target_address_(param->open.remote_bda))
        return false;
      ESP_LOGI(TAG, "ESP_GATTC_OPEN_EVT (event=2): status=%d conn_id=%u mtu=%u", param->open.status,
               static_cast<unsigned>(param->open.conn_id), static_cast<unsigned>(param->open.mtu));
      if (param->open.status != ESP_GATT_OK && param->open.status != ESP_GATT_ALREADY_OPEN) {
        ESP_LOGW(TAG, "Raw GATT open failed before service discovery (status=%d)", param->open.status);
        this->finish_connection_();
        break;
      }

      this->conn_id_ = param->open.conn_id;
      this->gatt_open_ = true;
      this->set_state(esp32_ble_tracker::ClientState::CONNECTED);
      this->service_search_started_ = true;
      ESP_LOGI(TAG, "GATT open succeeded; starting read-only service discovery");
      if (const esp_err_t err = esp_ble_gattc_search_service(this->gattc_if_, this->conn_id_, nullptr);
          err != ESP_OK) {
        this->service_search_started_ = false;
        ESP_LOGW(TAG, "esp_ble_gattc_search_service returned %s (%d)", esp_err_to_name(err), err);
        this->close_connection_("service discovery could not be started");
      }
      break;

    case ESP_GATTC_SEARCH_RES_EVT:
      if (param->search_res.conn_id == this->conn_id_ && this->service_search_started_)
        this->discovered_service_count_++;
      break;

    case ESP_GATTC_SEARCH_CMPL_EVT:
      if (param->search_cmpl.conn_id != this->conn_id_ || !this->service_search_started_)
        break;
      this->service_search_started_ = false;
      ESP_LOGI(TAG, "Read-only service discovery completed: status=%d, services=%u", param->search_cmpl.status,
               static_cast<unsigned>(this->discovered_service_count_));
      this->close_connection_(param->search_cmpl.status == ESP_GATT_OK ? "service discovery completed"
                                                                       : "service discovery failed");
      break;

    case ESP_GATTC_DISCONNECT_EVT:
      if (!this->is_target_address_(param->disconnect.remote_bda))
        return false;
      ESP_LOGI(TAG, "ESP_GATTC_DISCONNECT_EVT: reason=0x%02X conn_id=%u", param->disconnect.reason,
               static_cast<unsigned>(param->disconnect.conn_id));
      this->finish_connection_();
      break;

    case ESP_GATTC_CLOSE_EVT:
      if (!this->is_target_address_(param->close.remote_bda))
        return false;
      ESP_LOGI(TAG, "ESP_GATTC_CLOSE_EVT: status=%d reason=0x%02X conn_id=%u", param->close.status,
               param->close.reason, static_cast<unsigned>(param->close.conn_id));
      this->finish_connection_();
      break;

    default:
      break;
  }
  return true;
}

void CobiBleProbe::gap_event_handler(esp_gap_ble_cb_event_t, esp_ble_gap_cb_param_t *) {}

void CobiBleProbe::close_connection_(const char *reason) {
  if (this->close_requested_ || this->conn_id_ == NO_CONN_ID || this->gattc_if_ == ESP_GATT_IF_NONE) {
    if (this->conn_id_ == NO_CONN_ID)
      this->finish_connection_();
    return;
  }

  this->close_requested_ = true;
  this->disconnecting_started_ms_ = millis();
  this->set_state(esp32_ble_tracker::ClientState::DISCONNECTING);
  this->enable_loop();
  ESP_LOGI(TAG, "Closing raw GATT connection (%s)", reason);

  const esp_err_t err = esp_ble_gattc_close(this->gattc_if_, this->conn_id_);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "esp_ble_gattc_close returned %s (%d)", esp_err_to_name(err), err);
    this->finish_connection_();
  }
}

void CobiBleProbe::finish_connection_() {
  this->attempt_finished_ms_ = millis();
  ESP_LOGI(TAG, "Attempt %u ended %u ms after open was called", (unsigned) this->attempts_,
           (unsigned) (this->attempt_finished_ms_ - this->attempt_started_ms_));
  this->conn_id_ = NO_CONN_ID;
  this->gatt_open_ = false;
  this->service_search_started_ = false;
  this->close_requested_ = false;
  this->set_state(esp32_ble_tracker::ClientState::IDLE);
  this->disable_loop();
}

}  // namespace esphome::cobi_ble_probe
