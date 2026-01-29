#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/core/hal.h"
#include "esphome/components/climate/climate.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"

#include <ir_Daikin.h>

// Remote ARC466A58 DAIKIN312

namespace esphome {
namespace daikin_312 {

class Daikin312Climate : public climate::Climate, public Component {
 public:
  void setup() override;
  void loop() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  void set_sensor(sensor::Sensor *sensor) { this->sensor_ = sensor; }
  void set_pin(InternalGPIOPin *pin) { this->pin_ = pin; }

  // Automation triggers
  void set_turn_off_trigger(Trigger<> *trigger) { this->turn_off_trigger_ = trigger; }

  // External state sync from Home Assistant (no IR transmission)
  void set_external_mode_sensor(text_sensor::TextSensor *sensor);
  void set_external_temperature_sensor(sensor::Sensor *sensor);
  void set_external_fan_mode_sensor(text_sensor::TextSensor *sensor);
  void set_external_swing_mode_sensor(text_sensor::TextSensor *sensor);

//   void set_mold_enabled(bool enabled);
  void set_purify_enabled(bool enabled);
  bool get_purify();
  void set_eye(bool enabled);
  bool get_eye();
  void set_eye_auto(bool enabled);
  bool get_eye_auto();
  void set_light(uint8_t light);
  uint8_t get_light();
  void set_beep(uint8_t beep);
  uint8_t get_beep();

  void dump_config() override;

 protected:
  void control(const climate::ClimateCall &call) override;
  climate::ClimateTraits traits() override;

  sensor::Sensor *sensor_{nullptr};
  IRDaikin312 *ac_{nullptr};
  InternalGPIOPin *pin_{nullptr};
//   bool mold_enabled_{true};
  bool purify_enabled_{true};

  // External state sensors (for syncing from HA Daikin integration)
  text_sensor::TextSensor *external_mode_sensor_{nullptr};
  sensor::Sensor *external_temperature_sensor_{nullptr};
  text_sensor::TextSensor *external_fan_mode_sensor_{nullptr};
  text_sensor::TextSensor *external_swing_mode_sensor_{nullptr};

  // Helper methods for external state updates
  void update_mode_from_external_(const std::string &mode);
  void update_temperature_from_external_(float temp);
  void update_fan_mode_from_external_(const std::string &fan_mode);
  void update_swing_mode_from_external_(const std::string &swing_mode);
  void set_mode_(bool send);
  void set_target_temperature_(bool send);
  void set_fan_mode_(bool send);
  void set_swing_mode_(bool send);
  void set_custom_preset_(const std::string &preset);
  void set_preset_(bool send);
  void set_custom_fan_mode_(const std::string &mode);
  void transmit_state_();
  void clear_powerful_mode_();
  uint32_t powerful_mode_start_time_{0};
  bool powerful_mode_active_{false};

  // Automation triggers
  Trigger<> *turn_off_trigger_{nullptr};
};

}  // namespace daikin_312
}  // namespace esphome
