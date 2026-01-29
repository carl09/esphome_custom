#include "daikin_312_number.h"
#include "esphome/core/log.h"

namespace esphome {
namespace daikin_312 {

static const char *const TAG = "daikin_312.number";

void Daikin312Number::setup() {
  if (this->parent_ == nullptr)
    return;

  // Setup preferences for state restoration
  this->pref_ = global_preferences->make_preference<float>(this->get_object_id_hash());
  
  float restored_value;
  bool has_restored = this->pref_.load(&restored_value);

  if (has_restored && restored_value >= 0) {
    // Use restored state from preferences
    ESP_LOGD(TAG, "Restored sleep timer state: %.0f minutes", restored_value);
    
    // Apply the restored state to the parent
    if (this->type_ == DAIKIN312_NUMBER_SLEEP_TIMER) {
      this->parent_->set_sleep_timer(static_cast<uint16_t>(restored_value));
    }
    this->publish_state(restored_value);
  } else {
    // No saved state, get current state from parent
    if (this->type_ == DAIKIN312_NUMBER_SLEEP_TIMER) {
      float value = 0;
      if (this->parent_->get_sleep_timer_enabled()) {
        value = static_cast<float>(this->parent_->get_sleep_timer());
      }
      this->publish_state(value);
    }
  }
}

void Daikin312Number::dump_config() {
  if (this->type_ == DAIKIN312_NUMBER_SLEEP_TIMER) {
    LOG_NUMBER("", "Daikin 312 Sleep Timer", this);
    ESP_LOGCONFIG(TAG, "  Current value: %.0f minutes", this->state);
  }
}

void Daikin312Number::control(float value) {
  if (this->parent_ == nullptr)
    return;

  if (this->type_ == DAIKIN312_NUMBER_SLEEP_TIMER) {
    uint16_t minutes = static_cast<uint16_t>(value);
    ESP_LOGD(TAG, "Setting sleep timer to %d minutes", minutes);
    this->parent_->set_sleep_timer(minutes);
  }
  
  this->publish_state(value);
  
  // Save state to preferences
  this->pref_.save(&value);
}

}  // namespace daikin_312
}  // namespace esphome
