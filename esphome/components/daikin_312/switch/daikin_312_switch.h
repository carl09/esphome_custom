#pragma once

#include "esphome/core/component.h"
#include "esphome/components/switch/switch.h"
#include "../daikin_312.h"

namespace esphome {
namespace daikin_312 {

enum Daikin312SwitchType : uint8_t {
  DAIKIN312_SWITCH_PURIFY = 0,
  DAIKIN312_SWITCH_EYE = 1,
  DAIKIN312_SWITCH_EYE_AUTO = 2,
};

class Daikin312Switch : public switch_::Switch, public Component {
 public:
  void set_parent(Daikin312Climate *parent) { this->parent_ = parent; }
  void set_switch_type(Daikin312SwitchType type) { this->type_ = type; }

  void setup() override;
  void dump_config() override;

 protected:
  void write_state(bool state) override;

  Daikin312Climate *parent_;
  Daikin312SwitchType type_{DAIKIN312_SWITCH_PURIFY};
};

}  // namespace daikin_312
}  // namespace esphome
