#include "daikin_312.h"
#include "esphome/core/log.h"
#include "esphome/components/climate/climate_mode.h"

#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <ir_Daikin.h>

namespace esphome {
namespace daikin_312 {

static const char *const TAG = "daikin_312.climate";

const uint8_t DEFAULT_TEMP_MIN = 18;         // Celsius
const uint8_t DEFAULT_TEMP_MAX = 31;         // Celsius
const uint8_t DEFAULT_TARGET_TEMP_MAX = 22;  // Celsius
const uint32_t POWERFUL_MODE_DURATION_MS = 20 * 60 * 1000;  // 20 minutes in milliseconds

void Daikin312Climate::setup() {
  if (this->sensor_) {
    this->sensor_->add_on_state_callback([this](float state) {
      this->current_temperature = state;
      this->publish_state();
    });
    this->current_temperature = this->sensor_->state;
  } else {
    this->current_temperature = NAN;
  }

  // restore set points
  auto restore = this->restore_state_();

  if (restore.has_value()) {
    restore->apply(this);
  } else {
    // restore from defaults
    this->mode = climate::CLIMATE_MODE_OFF;
    this->target_temperature = DEFAULT_TARGET_TEMP_MAX;
    this->fan_mode = climate::CLIMATE_FAN_AUTO;
    this->swing_mode = climate::CLIMATE_SWING_OFF;
  }

  if (isnan(this->target_temperature))
    this->target_temperature = DEFAULT_TARGET_TEMP_MAX;

  this->ac_ = new IRDaikin312(this->pin_->get_pin());
  this->ac_->begin();

  // Syncing Remote settings
  this->set_mode_(false);
  this->set_fan_mode_(false);
  this->set_swing_mode_(false);
  this->set_target_temperature_(false);

  this->ac_->setPurify(true);
  this->ac_->setPurify(this->purify_enabled_);
}

void Daikin312Climate::loop() {
  // Check if powerful mode should auto-expire (AC unit turns it off after 20 mins)
  if (this->powerful_mode_active_) {
    uint32_t now = millis();
    if ((now - this->powerful_mode_start_time_) >= POWERFUL_MODE_DURATION_MS) {
      this->clear_powerful_mode_();
    }
  }
}

void Daikin312Climate::clear_powerful_mode_() {
  ESP_LOGD(TAG, "Powerful mode auto-expired after 20 minutes");
  this->powerful_mode_active_ = false;
  this->ac_->setPowerful(false);
  
  // Reset to previous fan mode or auto
  if (this->fan_mode.has_value()) {
    this->set_fan_mode_(false);
  } else {
    this->ac_->setFan(kDaikinFanAuto);
    this->fan_mode = climate::CLIMATE_FAN_AUTO;
  }
  
  // Clear the custom fan mode (Turbo) from UI
  this->clear_custom_fan_mode_();
  this->publish_state();
}

climate::ClimateTraits Daikin312Climate::traits() {
  auto traits = climate::ClimateTraits();
  if (this->sensor_ != nullptr) {
    traits.add_feature_flags(climate::CLIMATE_SUPPORTS_CURRENT_TEMPERATURE);
  }

  traits.set_supported_modes({climate::CLIMATE_MODE_OFF, climate::CLIMATE_MODE_HEAT_COOL, climate::CLIMATE_MODE_COOL,
                              climate::CLIMATE_MODE_HEAT, climate::CLIMATE_MODE_DRY, climate::CLIMATE_MODE_FAN_ONLY});

  traits.set_visual_min_temperature(DEFAULT_TEMP_MIN);
  traits.set_visual_max_temperature(DEFAULT_TEMP_MAX);
  traits.set_visual_temperature_step(1);

  traits.set_supported_fan_modes(
      {climate::CLIMATE_FAN_AUTO, climate::CLIMATE_FAN_QUIET, climate::CLIMATE_FAN_LOW,
       climate::CLIMATE_FAN_MEDIUM, climate::CLIMATE_FAN_HIGH});

  traits.set_supported_custom_fan_modes({"Max", "Turbo"});

  traits.set_supported_swing_modes({climate::CLIMATE_SWING_OFF, climate::CLIMATE_SWING_BOTH,
                                    climate::CLIMATE_SWING_VERTICAL, climate::CLIMATE_SWING_HORIZONTAL});

  traits.set_supported_presets({climate::CLIMATE_PRESET_NONE, climate::CLIMATE_PRESET_BOOST,
                                climate::CLIMATE_PRESET_ECO, climate::CLIMATE_PRESET_SLEEP});

  return traits;
}

void Daikin312Climate::control(const climate::ClimateCall &call) {
  if (call.get_mode().has_value()) {
    this->mode = *call.get_mode();
    this->set_mode_(true);
  }

  if (call.get_target_temperature().has_value()) {
    this->target_temperature = *call.get_target_temperature();
    this->set_target_temperature_(true);
  }

  if (call.has_custom_fan_mode() &&
      (this->get_custom_fan_mode().empty() || this->get_custom_fan_mode() != call.get_custom_fan_mode())) {
    this->fan_mode.reset();
    this->set_custom_fan_mode_(call.get_custom_fan_mode());
    this->transmit_state_();
  }

  if (call.get_fan_mode().has_value() &&
      (!this->fan_mode.has_value() || this->fan_mode.value() != call.get_fan_mode().value())) {
    this->clear_custom_fan_mode_();
    this->fan_mode = *call.get_fan_mode();
    this->set_fan_mode_(true);
  }

  if (call.get_swing_mode().has_value()) {
    this->swing_mode = *call.get_swing_mode();
    this->set_swing_mode_(true);
  }

  if (call.has_custom_preset()) {
    this->preset.reset();
    this->set_custom_preset_(call.get_custom_preset());
    this->transmit_state_();
  }

  if (call.get_preset().has_value()) {
    this->preset = *call.get_preset();
    this->set_preset_(true);
  }

  this->publish_state();
}

void Daikin312Climate::dump_config() {
  ESP_LOGCONFIG(TAG, "Daikin 312:");
  LOG_CLIMATE("", "IR Climate", this);
  LOG_PIN("  Step Pin: ", this->pin_);

  ESP_LOGCONFIG(TAG, "  mode %s", LOG_STR_ARG(climate_mode_to_string(this->mode)));
  ESP_LOGCONFIG(TAG, "  target_temperature %f", this->target_temperature);
  if (this->fan_mode.has_value()) {
    ESP_LOGCONFIG(TAG, "  fan_mode %s", LOG_STR_ARG(climate_fan_mode_to_string(this->fan_mode.value())));
  }
  if (!this->get_custom_fan_mode().empty()) {
    ESP_LOGCONFIG(TAG, "  custom fan_mode %s", this->get_custom_fan_mode().c_str());
  }
  ESP_LOGCONFIG(TAG, "  swing_mode %s", LOG_STR_ARG(climate_swing_mode_to_string(this->swing_mode)));
  if (this->preset.has_value()) {
    ESP_LOGCONFIG(TAG, "  preset %s", LOG_STR_ARG(climate_preset_to_string(this->preset.value())));
  }
  if (!this->get_custom_preset().empty()) {
    ESP_LOGCONFIG(TAG, "  custom preset %s", this->get_custom_preset().c_str());
  }
}

void Daikin312Climate::set_mode_(bool send) {
  if (this->mode == climate::CLIMATE_MODE_OFF) {
    this->ac_->setPower(false);
  } else {
    switch (this->mode) {
      case climate::CLIMATE_MODE_COOL:
        this->ac_->setMode(kDaikinCool);
        break;
      case climate::CLIMATE_MODE_HEAT:
        this->ac_->setMode(kDaikinHeat);
        break;
      case climate::CLIMATE_MODE_HEAT_COOL:
        this->ac_->setMode(kDaikinAuto);
        break;
      case climate::CLIMATE_MODE_FAN_ONLY:
        this->ac_->setMode(kDaikinFan);
        break;
      case climate::CLIMATE_MODE_DRY:
        this->ac_->setMode(kDaikinDry);
        break;
      case climate::CLIMATE_MODE_OFF:
      default:
        ESP_LOGW(TAG, "No Mode: %u", this->mode);
    }
    this->ac_->setPower(true);
  }
  if (send) {
    this->ac_->send();
  }
}

void Daikin312Climate::set_target_temperature_(bool send) {
  this->ac_->setTemp(this->target_temperature);
  if (send) {
    this->ac_->send();
  }
}

void Daikin312Climate::set_custom_fan_mode_(const std::string &mode) {
  if (str_equals_case_insensitive(mode, "Max")) {
    this->ac_->setFan(kDaikinFanMax);
    this->powerful_mode_active_ = false;
  } else if (str_equals_case_insensitive(mode, "Turbo")) {
    this->ac_->setPowerful(true);
    this->powerful_mode_active_ = true;
    this->powerful_mode_start_time_ = millis();
    ESP_LOGD(TAG, "Powerful mode activated - will auto-expire in 20 minutes");
  } else {
    ESP_LOGW(TAG, "Unknown Custom Fan Mode: %s", mode.c_str());
  }
}

void Daikin312Climate::set_fan_mode_(bool send) {
  if (!this->fan_mode.has_value()) {
    ESP_LOGD(TAG, "No Fan Mode set");
    return;
  }

  switch (this->fan_mode.value()) {
    case climate::CLIMATE_FAN_AUTO:
      this->ac_->setFan(kDaikinFanAuto);
      break;
    case climate::CLIMATE_FAN_QUIET:
      this->ac_->setQuiet(true);
      break;
    case climate::CLIMATE_FAN_LOW:
      this->ac_->setFan(kDaikinFanMin);
      break;
    case climate::CLIMATE_FAN_MEDIUM:
      this->ac_->setFan(kDaikinFanMed);
      break;
    case climate::CLIMATE_FAN_HIGH:
      this->ac_->setFan(kDaikinFanMax);
      break;
    default:
      ESP_LOGW(TAG, "Unsupported Fan Mode: %u", this->fan_mode.value());
      break;
  }

  // Ensure quiet mode is disabled when switching to non-quiet fan modes
  if (this->fan_mode.value() != climate::CLIMATE_FAN_QUIET) {
    this->ac_->setQuiet(false);
  }

  // Disable powerful mode when switching to a regular fan mode
  if (this->powerful_mode_active_) {
    this->ac_->setPowerful(false);
    this->powerful_mode_active_ = false;
  }

  if (send) {
    this->ac_->send();
  }
}

void Daikin312Climate::set_swing_mode_(bool send) {
  switch (this->swing_mode) {
    case climate::CLIMATE_SWING_OFF:
      this->ac_->setSwingVertical(kDaikin312SwingVOff);
      this->ac_->setSwingHorizontal(kDaikin312SwingHOff);
      break;
    case climate::CLIMATE_SWING_BOTH:
      this->ac_->setSwingVertical(kDaikin312SwingVAuto);
      this->ac_->setSwingHorizontal(kDaikin312SwingHAuto);
      break;
    case climate::CLIMATE_SWING_VERTICAL:
      this->ac_->setSwingVertical(kDaikin312SwingVAuto);
      this->ac_->setSwingHorizontal(kDaikin312SwingHOff);
      break;
    case climate::CLIMATE_SWING_HORIZONTAL:
      this->ac_->setSwingVertical(kDaikin312SwingVOff);
      this->ac_->setSwingHorizontal(kDaikin312SwingHAuto);
      break;
  }

  if (send) {
    this->ac_->send();
  }
}

void Daikin312Climate::set_preset_(bool send) {
  if (!this->preset.has_value()) {
    return;
  }

  // Reset all preset-related settings first
  this->ac_->setPowerful(false);
  this->ac_->setEcono(false);

  switch (this->preset.value()) {
    case climate::CLIMATE_PRESET_BOOST:
      this->ac_->setPowerful(true);
      ESP_LOGD(TAG, "Preset: Boost (Powerful mode)");
      break;
    case climate::CLIMATE_PRESET_ECO:
      this->ac_->setEcono(true);
      ESP_LOGD(TAG, "Preset: Eco");
      break;
    case climate::CLIMATE_PRESET_SLEEP:
      // Sleep mode typically uses quiet fan and comfort settings
      this->ac_->setQuiet(true);
      ESP_LOGD(TAG, "Preset: Sleep");
      break;
    case climate::CLIMATE_PRESET_NONE:
    default:
      ESP_LOGD(TAG, "Preset: None");
      break;
  }

  if (send) {
    this->ac_->send();
  }
}

void Daikin312Climate::set_custom_preset_(const std::string &preset) {
  // Custom presets not implemented for this model
}

void Daikin312Climate::transmit_state_() { this->ac_->send(); }

void Daikin312Climate::set_purify_enabled(bool enabled) {
  this->purify_enabled_ = enabled;
  if (this->ac_ != nullptr) {
    this->ac_->setPurify(enabled);
    this->ac_->send();
  }
}

bool Daikin312Climate::get_purify() {
  if (this->ac_ != nullptr) {
    return this->ac_->getPurify();
  }
  return false;
}

void Daikin312Climate::set_eye(bool enabled) {
  if (this->ac_ != nullptr) {
    this->ac_->setEye(enabled);
    this->ac_->send();
  }
}

bool Daikin312Climate::get_eye() {
  if (this->ac_ != nullptr) {
    return this->ac_->getEye();
  }
  return false;
}

void Daikin312Climate::set_eye_auto(bool enabled) {
  if (this->ac_ != nullptr) {
    this->ac_->setEyeAuto(enabled);
    this->ac_->send();
  }
}

bool Daikin312Climate::get_eye_auto() {
  if (this->ac_ != nullptr) {
    return this->ac_->getEyeAuto();
  }
  return false;
}

void Daikin312Climate::set_light(uint8_t light) {
  if (this->ac_ != nullptr) {
    this->ac_->setLight(light);
    this->ac_->send();
  }
}

uint8_t Daikin312Climate::get_light() {
  if (this->ac_ != nullptr) {
    return this->ac_->getLight();
  }
  return 3;  // Default to Off (kDaikinLightOff)
}

void Daikin312Climate::set_beep(uint8_t beep) {
  if (this->ac_ != nullptr) {
    this->ac_->setBeep(beep);
    this->ac_->send();
  }
}

uint8_t Daikin312Climate::get_beep() {
  if (this->ac_ != nullptr) {
    return this->ac_->getBeep();
  }
  return 3;  // Default to Off (kDaikinBeepOff)
}

}  // namespace daikin_312
}  // namespace esphome