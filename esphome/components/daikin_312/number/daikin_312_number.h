#pragma once

#include "esphome/core/component.h"
#include "esphome/core/preferences.h"
#include "esphome/components/number/number.h"
#include "../daikin_312.h"

namespace esphome {
namespace daikin_312 {

enum Daikin312NumberType : uint8_t {
  DAIKIN312_NUMBER_SLEEP_TIMER = 0,
};

class Daikin312Number : public number::Number, public Component {
 public:
  void set_parent(Daikin312Climate *parent) { this->parent_ = parent; }
  void set_number_type(Daikin312NumberType type) { this->type_ = type; }

  void setup() override;
  void dump_config() override;

 protected:
  void control(float value) override;

  Daikin312Climate *parent_;
  Daikin312NumberType type_{DAIKIN312_NUMBER_SLEEP_TIMER};
  ESPPreferenceObject pref_;
};

}  // namespace daikin_312
}  // namespace esphome
