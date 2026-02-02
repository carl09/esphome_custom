#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif

#include <IRrecv.h>
#include <IRutils.h>
#include <IRac.h>

namespace esphome {
namespace irremote_debug {

enum VerbosityLevel : uint8_t {
  VERBOSITY_MINIMAL = 0,
  VERBOSITY_NORMAL = 1,
  VERBOSITY_DETAILED = 2,
  VERBOSITY_RAW = 3,
};

class IRremoteDebugComponent : public PollingComponent {
 public:
  void setup() override;
  void update() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  void set_pin(InternalGPIOPin *pin) { this->pin_ = pin; }
  void set_buffer_size(uint16_t buffer_size) { this->buffer_size_ = buffer_size; }
  void set_timeout(uint32_t timeout) { this->timeout_ms_ = timeout; }
  void set_verbosity(VerbosityLevel verbosity) { this->verbosity_ = verbosity; }

#ifdef USE_TEXT_SENSOR
  void set_protocol_text_sensor(text_sensor::TextSensor *sensor) { this->protocol_sensor_ = sensor; }
#endif

  /// Manually dump the last received signal (called by button)
  void dump_last_signal();

  /// Check if we have a signal stored
  bool has_last_signal() const { return this->last_signal_valid_; }

 protected:
  void process_signal_(decode_results *results);
  void log_minimal_(decode_results *results);
  void log_normal_(decode_results *results);
  void log_detailed_(decode_results *results);
  void log_raw_(decode_results *results);
  std::string get_protocol_string_(decode_results *results);

  InternalGPIOPin *pin_{nullptr};
  uint16_t buffer_size_{1024};
  uint32_t timeout_ms_{15};
  VerbosityLevel verbosity_{VERBOSITY_NORMAL};

  IRrecv *ir_receiver_{nullptr};
  decode_results last_results_;
  bool last_signal_valid_{false};

#ifdef USE_TEXT_SENSOR
  text_sensor::TextSensor *protocol_sensor_{nullptr};
#endif
};

}  // namespace irremote_debug
}  // namespace esphome
