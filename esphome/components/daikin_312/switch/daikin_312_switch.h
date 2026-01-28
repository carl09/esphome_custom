#pragma once

#include "esphome/core/component.h"
#include "esphome/components/switch/switch.h"
#include "../daikin_312.h"

namespace esphome {
namespace daikin_312 {

class Daikin312CleanSwitch : public switch_::Switch, public Component {
 public:
  void set_parent(Daikin312Climate *parent) { this->parent_ = parent; }

  void dump_config() override;

 protected:
  void write_state(bool state) override;

  Daikin312Climate *parent_;
};

}  // namespace daikin_312
}  // namespace esphome
