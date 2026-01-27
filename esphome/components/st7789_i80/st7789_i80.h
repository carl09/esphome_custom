#pragma once

#ifdef USE_ESP32

#include "esphome/core/component.h"
#include "esphome/core/gpio.h"
#include "esphome/components/display/display.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"

namespace esphome {
namespace st7789_i80 {

class ST7789I80 : public display::Display {
 public:
  void setup() override;
  void dump_config() override;
  void update() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }
  
  display::DisplayType get_display_type() override { return display::DisplayType::DISPLAY_TYPE_COLOR; }
  
  int get_width_internal() override { return this->swap_xy_ ? this->height_ : this->width_; }
  int get_height_internal() override { return this->swap_xy_ ? this->width_ : this->height_; }
  
  void draw_pixels_at(int x_start, int y_start, int w, int h, const uint8_t *ptr,
                      display::ColorOrder order, display::ColorBitness bitness, 
                      bool big_endian, int x_offset, int y_offset, int x_pad) override;
  
  void draw_pixel_at(int x, int y, Color color) override;
  
  void fill(Color color) override;
  
  // Configuration setters
  void set_dimensions(uint16_t width, uint16_t height) {
    this->width_ = width;
    this->height_ = height;
  }
  void set_offsets(int16_t offset_x, int16_t offset_y) {
    this->offset_x_ = offset_x;
    this->offset_y_ = offset_y;
  }
  void add_data_pin(InternalGPIOPin *pin, size_t index) {
    if (index < 8)
      this->data_pins_[index] = pin;
  }
  void set_dc_pin(InternalGPIOPin *pin) { this->dc_pin_ = pin; }
  void set_wr_pin(InternalGPIOPin *pin) { this->wr_pin_ = pin; }
  void set_cs_pin(InternalGPIOPin *pin) { this->cs_pin_ = pin; }
  void set_rd_pin(InternalGPIOPin *pin) { this->rd_pin_ = pin; }
  void set_reset_pin(GPIOPin *pin) { this->reset_pin_ = pin; }
  void set_backlight_pin(GPIOPin *pin) { this->backlight_pin_ = pin; }
  void set_invert_colors(bool invert) { this->invert_colors_ = invert; }
  void set_pclk_frequency(uint32_t freq) { this->pclk_frequency_ = freq; }
  void set_swap_xy(bool swap) { this->swap_xy_ = swap; }
  void set_mirror_x(bool mirror) { this->mirror_x_ = mirror; }
  void set_mirror_y(bool mirror) { this->mirror_y_ = mirror; }

 protected:
  void hard_reset_();
  void set_backlight_(bool on);
  void wait_for_pending_transfers_();  // Wait for DMA to complete
  
  // Display dimensions
  uint16_t width_{240};
  uint16_t height_{320};
  int16_t offset_x_{0};
  int16_t offset_y_{0};
  
  // Pin configuration
  InternalGPIOPin *data_pins_[8]{nullptr};
  InternalGPIOPin *dc_pin_{nullptr};
  InternalGPIOPin *wr_pin_{nullptr};
  InternalGPIOPin *cs_pin_{nullptr};
  InternalGPIOPin *rd_pin_{nullptr};
  GPIOPin *reset_pin_{nullptr};
  GPIOPin *backlight_pin_{nullptr};
  
  // Display settings
  bool invert_colors_{false};
  uint32_t pclk_frequency_{12000000};  // 12MHz default
  bool swap_xy_{false};
  bool mirror_x_{false};
  bool mirror_y_{false};
  
  // ESP-IDF handles
  esp_lcd_i80_bus_handle_t i80_bus_{nullptr};
  esp_lcd_panel_io_handle_t io_handle_{nullptr};
  esp_lcd_panel_handle_t panel_handle_{nullptr};
  
  // Persistent DMA buffer for fill() - allocated once, never freed during operation
  uint16_t *fill_buffer_{nullptr};
  size_t fill_buffer_pixels_{0};
  static const int FILL_ROWS_PER_CHUNK = 20;  // Rows per DMA transfer
  
  // DMA transfer buffer for draw_pixels_at() - LVGL buffers may not be DMA-capable
  uint8_t *dma_transfer_buffer_{nullptr};
  size_t dma_transfer_buffer_size_{0};
};

}  // namespace st7789_i80
}  // namespace esphome

#endif  // USE_ESP32
