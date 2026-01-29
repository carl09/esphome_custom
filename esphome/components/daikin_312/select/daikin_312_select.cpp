#include "daikin_312_select.h"
#include "esphome/core/log.h"

namespace esphome {
namespace daikin_312 {

static const char *const TAG = "daikin_312.select";

void Daikin312LightSelect::setup() {
  // Set initial state based on current AC light setting
  if (this->parent_ != nullptr) {
    uint8_t light = this->parent_->get_light();
    switch (light) {
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

void Daikin312LightSelect::dump_config() {
  LOG_SELECT("", "Daikin 312 Light", this);
}

void Daikin312LightSelect::control(const std::string &value) {
  if (this->parent_ != nullptr) {
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
}

}  // namespace daikin_312
}  // namespace esphome
