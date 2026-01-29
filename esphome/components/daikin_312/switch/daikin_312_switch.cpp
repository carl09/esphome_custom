#include "daikin_312_switch.h"
#include "esphome/core/log.h"

namespace esphome {
namespace daikin_312 {

static const char *const TAG = "daikin_312.switch";

void Daikin312Switch::setup() {
  if (this->parent_ == nullptr)
    return;

  // Try to restore state from preferences first
  auto restored = this->get_initial_state_with_restore_mode();
  bool state;
  
  if (restored.has_value()) {
    // Use restored state from preferences
    state = restored.value();
    ESP_LOGD(TAG, "Restored state: %s", ONOFF(state));
    
    // Apply the restored state to the parent (without sending IR yet, parent handles that)
    switch (this->type_) {
      case DAIKIN312_SWITCH_EYE:
        this->parent_->set_eye(state);
        break;
      case DAIKIN312_SWITCH_EYE_AUTO:
        this->parent_->set_eye_auto(state);
        break;
      case DAIKIN312_SWITCH_PURIFY:
      default:
        this->parent_->set_purify_enabled(state);
        break;
    }
  } else {
    // No restore mode or no saved state, get current state from parent
    switch (this->type_) {
      case DAIKIN312_SWITCH_EYE:
        state = this->parent_->get_eye();
        break;
      case DAIKIN312_SWITCH_EYE_AUTO:
        state = this->parent_->get_eye_auto();
        break;
      case DAIKIN312_SWITCH_PURIFY:
      default:
        state = this->parent_->get_purify();
        break;
    }
  }
  this->publish_state(state);
}

void Daikin312Switch::dump_config() {
  const char *type_name;
  switch (this->type_) {
    case DAIKIN312_SWITCH_EYE:
      type_name = "Eye";
      break;
    case DAIKIN312_SWITCH_EYE_AUTO:
      type_name = "Eye Auto";
      break;
    case DAIKIN312_SWITCH_PURIFY:
    default:
      type_name = "Purify";
      break;
  }
  LOG_SWITCH("", "Daikin 312", this);
  ESP_LOGCONFIG(TAG, "  Type: %s", type_name);
  ESP_LOGCONFIG(TAG, "  Current state: %s", ONOFF(this->state));
}

void Daikin312Switch::write_state(bool state) {
  if (this->parent_ == nullptr)
    return;

  switch (this->type_) {
    case DAIKIN312_SWITCH_EYE:
      this->parent_->set_eye(state);
      break;
    case DAIKIN312_SWITCH_EYE_AUTO:
      this->parent_->set_eye_auto(state);
      break;
    case DAIKIN312_SWITCH_PURIFY:
    default:
      this->parent_->set_purify_enabled(state);
      break;
  }
  this->publish_state(state);
}

}  // namespace daikin_312
}  // namespace esphome
