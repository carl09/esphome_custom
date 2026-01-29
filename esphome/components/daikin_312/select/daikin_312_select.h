#pragma once

#include "esphome/core/component.h"
#include "esphome/components/select/select.h"
#include "../daikin_312.h"

namespace esphome {
namespace daikin_312 {

enum Daikin312SelectType : uint8_t {
  DAIKIN312_SELECT_LIGHT = 0,
  DAIKIN312_SELECT_BEEP = 1,
};

class Daikin312Select : public select::Select, public Component {
 public:
  void set_parent(Daikin312Climate *parent) { this->parent_ = parent; }
  void set_select_type(Daikin312SelectType type) { this->type_ = type; }

  void setup() override;
  void dump_config() override;

 protected:
  void control(const std::string &value) override;

  Daikin312Climate *parent_;
  Daikin312SelectType type_{DAIKIN312_SELECT_LIGHT};
};

}  // namespace daikin_312
}  // namespace esphome
