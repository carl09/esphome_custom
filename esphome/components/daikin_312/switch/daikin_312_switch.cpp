#include "daikin_312_switch.h"
#include "esphome/core/log.h"

namespace esphome {
namespace daikin_312 {

static const char *const TAG = "daikin_312.switch";

void Daikin312CleanSwitch::dump_config() {
  LOG_SWITCH("", "Daikin 312 Purify Mode", this);
}

void Daikin312CleanSwitch::write_state(bool state) {
  if (this->parent_ != nullptr) {
    this->parent_->set_purify_enabled(state);
  }
  this->publish_state(state);
}

}  // namespace daikin_312
}  // namespace esphome
