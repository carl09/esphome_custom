#include "irremote_debug.h"

namespace esphome {
namespace irremote_debug {

static const char *const TAG = "irremote_debug";

void IRremoteDebugComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up IRremote Debug...");

  uint8_t pin_num = this->pin_->get_pin();
  this->ir_receiver_ = new IRrecv(pin_num, this->buffer_size_, this->timeout_ms_, true);
  this->ir_receiver_->setUnknownThreshold(12);
  this->ir_receiver_->setTolerance(kTolerance);
  this->ir_receiver_->enableIRIn();

  ESP_LOGCONFIG(TAG, "  Pin: GPIO%u", pin_num);
  ESP_LOGCONFIG(TAG, "  Buffer Size: %u", this->buffer_size_);
  ESP_LOGCONFIG(TAG, "  Timeout: %u ms", this->timeout_ms_);
}

void IRremoteDebugComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "IRremote Debug:");
  ESP_LOGCONFIG(TAG, "  Pin: GPIO%u", this->pin_->get_pin());
  ESP_LOGCONFIG(TAG, "  Buffer Size: %u", this->buffer_size_);
  ESP_LOGCONFIG(TAG, "  Timeout: %u ms", this->timeout_ms_);

  const char *verbosity_str;
  switch (this->verbosity_) {
    case VERBOSITY_MINIMAL:
      verbosity_str = "minimal";
      break;
    case VERBOSITY_NORMAL:
      verbosity_str = "normal";
      break;
    case VERBOSITY_DETAILED:
      verbosity_str = "detailed";
      break;
    case VERBOSITY_RAW:
      verbosity_str = "raw";
      break;
    default:
      verbosity_str = "unknown";
  }
  ESP_LOGCONFIG(TAG, "  Verbosity: %s", verbosity_str);
}

void IRremoteDebugComponent::update() {
  decode_results results;

  if (this->ir_receiver_->decode(&results)) {
    // Store for later re-dump
    this->last_results_ = results;
    this->last_signal_valid_ = true;

    // Process and log
    this->process_signal_(&results);

    // Resume receiving
    this->ir_receiver_->resume();
  }
}

void IRremoteDebugComponent::process_signal_(decode_results *results) {
  ESP_LOGI(TAG, "========== IR Signal Received ==========");

  // Update text sensor if configured
#ifdef USE_TEXT_SENSOR
  if (this->protocol_sensor_ != nullptr) {
    std::string protocol_str = this->get_protocol_string_(results);
    this->protocol_sensor_->publish_state(protocol_str);
  }
#endif

  // Log based on verbosity level
  switch (this->verbosity_) {
    case VERBOSITY_MINIMAL:
      this->log_minimal_(results);
      break;
    case VERBOSITY_NORMAL:
      this->log_normal_(results);
      break;
    case VERBOSITY_DETAILED:
      this->log_detailed_(results);
      break;
    case VERBOSITY_RAW:
      this->log_raw_(results);
      break;
  }

  ESP_LOGI(TAG, "=========================================");
}

void IRremoteDebugComponent::log_minimal_(decode_results *results) {
  String protocol = typeToString(results->decode_type, results->repeat);
  ESP_LOGI(TAG, "Protocol: %s", protocol.c_str());

  if (results->decode_type != decode_type_t::UNKNOWN) {
    if (hasACState(results->decode_type)) {
      // State-based protocol (AC)
      String hex_str = resultToHexidecimal(results);
      ESP_LOGI(TAG, "Code: %s", hex_str.c_str());
    } else {
      // Simple protocol
      ESP_LOGI(TAG, "Code: 0x%llX (%u bits)", results->value, results->bits);
    }
  }
}

void IRremoteDebugComponent::log_normal_(decode_results *results) {
  // First log minimal info
  this->log_minimal_(results);

  // Add more details
  ESP_LOGI(TAG, "Bits: %u", results->bits);
  ESP_LOGI(TAG, "Address: 0x%X", results->address);
  ESP_LOGI(TAG, "Command: 0x%X", results->command);

  if (results->repeat) {
    ESP_LOGI(TAG, "Repeat: true");
  }

  if (results->overflow) {
    ESP_LOGW(TAG, "WARNING: Buffer overflow detected!");
  }
}

void IRremoteDebugComponent::log_detailed_(decode_results *results) {
  // First log normal info
  this->log_normal_(results);

  // For AC protocols, try to decode the state
  if (hasACState(results->decode_type)) {
    ESP_LOGI(TAG, "--- AC State Details ---");

    // Log raw state bytes
    ESP_LOGI(TAG, "State Length: %u bytes", results->bits / 8);
    String state_str = "State: ";
    for (uint16_t i = 0; i < (results->bits / 8); i++) {
      char hex[4];
      snprintf(hex, sizeof(hex), "%02X ", results->state[i]);
      state_str += hex;
    }
    ESP_LOGI(TAG, "%s", state_str.c_str());

    // Use IRac to get human-readable state if possible
    stdAc::state_t ac_state;
    stdAc::state_t prev_state;
    prev_state.protocol = decode_type_t::UNKNOWN;

    if (IRAcUtils::decodeToState(results, &ac_state, &prev_state)) {
      ESP_LOGI(TAG, "--- Decoded AC Settings ---");
      ESP_LOGI(TAG, "  Power: %s", ac_state.power ? "ON" : "OFF");
      ESP_LOGI(TAG, "  Temperature: %.1f°C", ac_state.degrees);

      const char *mode_str;
      switch (ac_state.mode) {
        case stdAc::opmode_t::kOff:
          mode_str = "OFF";
          break;
        case stdAc::opmode_t::kAuto:
          mode_str = "AUTO";
          break;
        case stdAc::opmode_t::kCool:
          mode_str = "COOL";
          break;
        case stdAc::opmode_t::kHeat:
          mode_str = "HEAT";
          break;
        case stdAc::opmode_t::kDry:
          mode_str = "DRY";
          break;
        case stdAc::opmode_t::kFan:
          mode_str = "FAN";
          break;
        default:
          mode_str = "UNKNOWN";
      }
      ESP_LOGI(TAG, "  Mode: %s", mode_str);

      const char *fan_str;
      switch (ac_state.fanspeed) {
        case stdAc::fanspeed_t::kAuto:
          fan_str = "AUTO";
          break;
        case stdAc::fanspeed_t::kMin:
          fan_str = "MIN";
          break;
        case stdAc::fanspeed_t::kLow:
          fan_str = "LOW";
          break;
        case stdAc::fanspeed_t::kMedium:
          fan_str = "MEDIUM";
          break;
        case stdAc::fanspeed_t::kHigh:
          fan_str = "HIGH";
          break;
        case stdAc::fanspeed_t::kMax:
          fan_str = "MAX";
          break;
        default:
          fan_str = "UNKNOWN";
      }
      ESP_LOGI(TAG, "  Fan Speed: %s", fan_str);

      if (ac_state.swingv != stdAc::swingv_t::kOff) {
        ESP_LOGI(TAG, "  Swing V: ON");
      }
      if (ac_state.swingh != stdAc::swingh_t::kOff) {
        ESP_LOGI(TAG, "  Swing H: ON");
      }
      if (ac_state.quiet) {
        ESP_LOGI(TAG, "  Quiet: ON");
      }
      if (ac_state.turbo) {
        ESP_LOGI(TAG, "  Turbo: ON");
      }
      if (ac_state.econo) {
        ESP_LOGI(TAG, "  Econo: ON");
      }
      if (ac_state.light) {
        ESP_LOGI(TAG, "  Light: ON");
      }
      if (ac_state.filter) {
        ESP_LOGI(TAG, "  Filter: ON");
      }
      if (ac_state.clean) {
        ESP_LOGI(TAG, "  Clean: ON");
      }
      if (ac_state.beep) {
        ESP_LOGI(TAG, "  Beep: ON");
      }
    }

    // Also output the full human-readable string from the library
    String human_str = IRAcUtils::resultAcToString(results);
    if (human_str.length() > 0) {
      ESP_LOGI(TAG, "Library decode: %s", human_str.c_str());
    }
  }
}

void IRremoteDebugComponent::log_raw_(decode_results *results) {
  // First log detailed info
  this->log_detailed_(results);

  // Now add raw timing data
  ESP_LOGI(TAG, "--- Raw Timing Data ---");
  ESP_LOGI(TAG, "Raw Length: %u", results->rawlen);

  // Output in format suitable for IRremoteESP8266 analysis tools
  String raw_output = "uint16_t rawData[";
  raw_output += String(results->rawlen - 1);
  raw_output += "] = {";

  for (uint16_t i = 1; i < results->rawlen; i++) {
    if (i > 1) {
      raw_output += ", ";
    }
    raw_output += String(results->rawbuf[i] * kRawTick);
  }
  raw_output += "};";

  // Log raw data in chunks to avoid log buffer overflow
  uint16_t chunk_size = 200;
  for (uint16_t i = 0; i < raw_output.length(); i += chunk_size) {
    String chunk = raw_output.substring(i, min((uint16_t)(i + chunk_size), (uint16_t)raw_output.length()));
    ESP_LOGI(TAG, "%s", chunk.c_str());
  }

  // Output state array if it's an AC protocol
  if (hasACState(results->decode_type)) {
    String state_output = "uint8_t state[";
    state_output += String(results->bits / 8);
    state_output += "] = {";

    for (uint16_t i = 0; i < (results->bits / 8); i++) {
      if (i > 0) {
        state_output += ", ";
      }
      char hex[5];
      snprintf(hex, sizeof(hex), "0x%02X", results->state[i]);
      state_output += hex;
    }
    state_output += "};";
    ESP_LOGI(TAG, "%s", state_output.c_str());
  } else if (results->decode_type != decode_type_t::UNKNOWN) {
    ESP_LOGI(TAG, "uint64_t data = 0x%llX;", results->value);
  }
}

std::string IRremoteDebugComponent::get_protocol_string_(decode_results *results) {
  String protocol = typeToString(results->decode_type, results->repeat);
  std::string result = protocol.c_str();

  // Add hex code for text sensor
  if (results->decode_type != decode_type_t::UNKNOWN) {
    if (hasACState(results->decode_type)) {
      String hex_str = resultToHexidecimal(results);
      result += ": ";
      // Truncate if too long
      if (hex_str.length() > 40) {
        result += hex_str.substring(0, 37).c_str();
        result += "...";
      } else {
        result += hex_str.c_str();
      }
    } else {
      char hex[24];
      snprintf(hex, sizeof(hex), ": 0x%llX", results->value);
      result += hex;
    }
  }

  return result;
}

void IRremoteDebugComponent::dump_last_signal() {
  if (!this->last_signal_valid_) {
    ESP_LOGW(TAG, "No IR signal has been received yet.");
    return;
  }

  ESP_LOGI(TAG, "========== Dumping Last Signal ==========");
  // Always dump at RAW level when manually triggered
  this->log_raw_(&this->last_results_);
  ESP_LOGI(TAG, "==========================================");
}

}  // namespace irremote_debug
}  // namespace esphome
