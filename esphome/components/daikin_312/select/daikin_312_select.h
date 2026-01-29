#pragma once

#include "esphome/core/component.h"
#include "esphome/components/select/select.h"
#include "../daikin_312.h"

namespace esphome {
namespace daikin_312 {

class Daikin312LightSelect : public select::Select, public Component {
 public:
  void set_parent(Daikin312Climate *parent) { this->parent_ = parent; }

  void setup() override;
  void dump_config() override;

 protected:
  void control(const std::string &value) override;

  Daikin312Climate *parent_;
};

}  // namespace daikin_312
}  // namespace esphome
