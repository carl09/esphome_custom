#pragma once

#include "esphome/components/button/button.h"
#include "esphome/core/helpers.h"
#include "../irremote_debug.h"

namespace esphome {
namespace irremote_debug {

class DumpButton : public button::Button, public Parented<IRremoteDebugComponent> {
 public:
  DumpButton() = default;

 protected:
  void press_action() override { this->parent_->dump_last_signal(); }
};

}  // namespace irremote_debug
}  // namespace esphome
