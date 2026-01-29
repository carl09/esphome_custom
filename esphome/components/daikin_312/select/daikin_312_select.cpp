#include "daikin_312_select.h"
#include "esphome/core/log.h"

namespace esphome {
namespace daikin_312 {

static const char *const TAG = "daikin_312.select";

void Daikin312Select::setup() {
  if (this->parent_ == nullptr)
    return;

  // Setup preferences for state restoration
  this->pref_ = global_preferences->make_preference<size_t>(this->get_preference_hash());
  
  size_t restored_index;
  bool has_restored = this->pref_.load(&restored_index) && this->has_index(restored_index);

  if (has_restored) {
    // Use restored state from preferences
    const char *restored_value = this->option_at(restored_index);
    ESP_LOGD(TAG, "Restored state: %s", restored_value);
    
    // Apply the restored state to the parent
    if (this->type_ == DAIKIN312_SELECT_BEEP) {
      uint8_t beep_value;
      if (strcmp(restored_value, "Loud") == 0) {
        beep_value = 2;  // kDaikinBeepLoud
      } else if (strcmp(restored_value, "Quiet") == 0) {
        beep_value = 1;  // kDaikinBeepQuiet
      } else {
        beep_value = 3;  // kDaikinBeepOff
      }
      this->parent_->set_beep(beep_value);
    } else {
      uint8_t light_value;
      if (strcmp(restored_value, "Bright") == 0) {
        light_value = 1;  // kDaikinLightBright
      } else if (strcmp(restored_value, "Dim") == 0) {
        light_value = 2;  // kDaikinLightDim
      } else {
        light_value = 3;  // kDaikinLightOff
      }
      this->parent_->set_light(light_value);
    }
    this->publish_state(restored_index);
  } else {
    // No saved state, get current state from parent
    uint8_t value;
    if (this->type_ == DAIKIN312_SELECT_BEEP) {
      value = this->parent_->get_beep();
      switch (value) {
        case 1:  // kDaikinBeepQuiet
          this->publish_state("Quiet");
          break;
        case 2:  // kDaikinBeepLoud
          this->publish_state("Loud");
          break;
        case 3:  // kDaikinBeepOff
        default:
          this->publish_state("Off");
          break;
      }
    } else {
      value = this->parent_->get_light();
      switch (value) {
        case 1:  // kDaikinLightBright
          this->publish_state("Bright");
          break;
        case 2:  // kDaikinLightDim
          this->publish_state("Dim");
          break;
        case 3:  // kDaikinLightOff
        default:
          this->publish_state("Off");
          break;
      }
    }
  }
}

void Daikin312Select::dump_config() {
  if (this->type_ == DAIKIN312_SELECT_BEEP) {
    LOG_SELECT("", "Daikin 312 Beep", this);
    ESP_LOGCONFIG(TAG, "  Current value: %s", this->state.c_str());
  } else {
    LOG_SELECT("", "Daikin 312 Light", this);
    ESP_LOGCONFIG(TAG, "  Current value: %s", this->state.c_str());
  }
}

void Daikin312Select::control(const std::string &value) {
  if (this->parent_ == nullptr)
    return;

  if (this->type_ == DAIKIN312_SELECT_BEEP) {
    uint8_t beep_value;
    if (value == "Loud") {
      beep_value = 2;  // kDaikinBeepLoud
    } else if (value == "Quiet") {
      beep_value = 1;  // kDaikinBeepQuiet
    } else {
      beep_value = 3;  // kDaikinBeepOff
    }
    this->parent_->set_beep(beep_value);
  } else {
    uint8_t light_value;
    if (value == "Bright") {
      light_value = 1;  // kDaikinLightBright
    } else if (value == "Dim") {
      light_value = 2;  // kDaikinLightDim
    } else {
      light_value = 3;  // kDaikinLightOff
    }
    this->parent_->set_light(light_value);
  }
  this->publish_state(value);
  
  // Save state to preferences
  auto index = this->index_of(value);
  if (index.has_value()) {
    this->pref_.save(&index.value());
  }
}

}  // namespace daikin_312
}  // namespace esphome
