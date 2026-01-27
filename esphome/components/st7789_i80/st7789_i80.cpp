#ifdef USE_ESP32

#include "st7789_i80.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/components/display/display_color_utils.h"
#include <driver/gpio.h>
#include <esp_heap_caps.h>

namespace esphome {
namespace st7789_i80 {

static const char *const TAG = "display.st7789_i80";

// ST7789 Commands
static const uint8_t ST7789_NOP = 0x00;
static const uint8_t ST7789_SWRESET = 0x01;
static const uint8_t ST7789_SLPIN = 0x10;
static const uint8_t ST7789_SLPOUT = 0x11;
static const uint8_t ST7789_NORON = 0x13;
static const uint8_t ST7789_INVOFF = 0x20;
static const uint8_t ST7789_INVON = 0x21;
static const uint8_t ST7789_DISPOFF = 0x28;
static const uint8_t ST7789_DISPON = 0x29;
static const uint8_t ST7789_CASET = 0x2A;
static const uint8_t ST7789_RASET = 0x2B;
static const uint8_t ST7789_RAMWR = 0x2C;
static const uint8_t ST7789_MADCTL = 0x36;
static const uint8_t ST7789_COLMOD = 0x3A;
static const uint8_t ST7789_PORCTRL = 0xB2;
static const uint8_t ST7789_GCTRL = 0xB7;
static const uint8_t ST7789_VCOMS = 0xBB;
static const uint8_t ST7789_LCMCTRL = 0xC0;
static const uint8_t ST7789_VDVVRHEN = 0xC2;
static const uint8_t ST7789_VRHS = 0xC3;
static const uint8_t ST7789_VDVS = 0xC4;
static const uint8_t ST7789_FRCTRL2 = 0xC6;
static const uint8_t ST7789_PWCTRL1 = 0xD0;
static const uint8_t ST7789_PVGAMCTRL = 0xE0;
static const uint8_t ST7789_NVGAMCTRL = 0xE1;

// MADCTL flags
static const uint8_t ST7789_MADCTL_MY = 0x80;   // Row address order
static const uint8_t ST7789_MADCTL_MX = 0x40;   // Column address order
static const uint8_t ST7789_MADCTL_MV = 0x20;   // Row/Column exchange
static const uint8_t ST7789_MADCTL_ML = 0x10;   // Vertical refresh order
static const uint8_t ST7789_MADCTL_RGB = 0x00;  // RGB color order
static const uint8_t ST7789_MADCTL_BGR = 0x08;  // BGR color order

void ST7789I80::setup() {
  ESP_LOGCONFIG(TAG, "Setting up ST7789 I80 display...");
  App.feed_wdt();

  // Configure RD pin as output HIGH (not used for writing, but needs to be inactive)
  if (this->rd_pin_ != nullptr) {
    this->rd_pin_->setup();
    this->rd_pin_->digital_write(true);
  }

  // Reset the display
  this->hard_reset_();
  App.feed_wdt();

  // Configure I80 bus
  esp_lcd_i80_bus_config_t bus_config = {};
  bus_config.clk_src = LCD_CLK_SRC_DEFAULT;
  bus_config.dc_gpio_num = this->dc_pin_->get_pin();
  bus_config.wr_gpio_num = this->wr_pin_->get_pin();
  for (size_t i = 0; i < 8; i++) {
    bus_config.data_gpio_nums[i] = this->data_pins_[i]->get_pin();
  }
  bus_config.bus_width = 8;
  bus_config.max_transfer_bytes = this->width_ * this->height_ * 2 / 10;  // Transfer 1/10 of screen at a time

  esp_err_t err = esp_lcd_new_i80_bus(&bus_config, &this->i80_bus_);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to create I80 bus: %s", esp_err_to_name(err));
    this->mark_failed();
    return;
  }

  // Configure panel IO
  esp_lcd_panel_io_i80_config_t io_config = {};
  io_config.cs_gpio_num = this->cs_pin_ != nullptr ? this->cs_pin_->get_pin() : -1;
  io_config.pclk_hz = this->pclk_frequency_;
  io_config.trans_queue_depth = 10;
  io_config.lcd_cmd_bits = 8;
  io_config.lcd_param_bits = 8;
  io_config.dc_levels.dc_idle_level = 0;
  io_config.dc_levels.dc_cmd_level = 0;
  io_config.dc_levels.dc_dummy_level = 0;
  io_config.dc_levels.dc_data_level = 1;
  io_config.flags.cs_active_high = false;
  io_config.flags.reverse_color_bits = false;
  io_config.flags.swap_color_bytes = false;
  io_config.flags.pclk_active_neg = false;
  io_config.flags.pclk_idle_low = false;

  err = esp_lcd_new_panel_io_i80(this->i80_bus_, &io_config, &this->io_handle_);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to create panel IO: %s", esp_err_to_name(err));
    this->mark_failed();
    return;
  }

  // Configure panel
  esp_lcd_panel_dev_config_t panel_config = {};
  panel_config.reset_gpio_num = -1;  // We handle reset ourselves
  panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
  panel_config.bits_per_pixel = 16;
  panel_config.flags.reset_active_high = false;

  err = esp_lcd_new_panel_st7789(this->io_handle_, &panel_config, &this->panel_handle_);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to create ST7789 panel: %s", esp_err_to_name(err));
    this->mark_failed();
    return;
  }

  // Reset and initialize the panel
  err = esp_lcd_panel_reset(this->panel_handle_);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to reset panel: %s", esp_err_to_name(err));
  }

  err = esp_lcd_panel_init(this->panel_handle_);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to initialize panel: %s", esp_err_to_name(err));
    this->mark_failed();
    return;
  }
  App.feed_wdt();

  // Configure display inversion (IPS panels typically need inversion)
  if (this->invert_colors_) {
    err = esp_lcd_panel_invert_color(this->panel_handle_, true);
    if (err != ESP_OK) {
      ESP_LOGW(TAG, "Failed to set color inversion: %s", esp_err_to_name(err));
    }
  }

  // Configure mirroring
  if (this->mirror_x_ || this->mirror_y_) {
    err = esp_lcd_panel_mirror(this->panel_handle_, this->mirror_x_, this->mirror_y_);
    if (err != ESP_OK) {
      ESP_LOGW(TAG, "Failed to set mirroring: %s", esp_err_to_name(err));
    }
  }

  // Configure XY swap
  if (this->swap_xy_) {
    err = esp_lcd_panel_swap_xy(this->panel_handle_, true);
    if (err != ESP_OK) {
      ESP_LOGW(TAG, "Failed to set XY swap: %s", esp_err_to_name(err));
    }
  }

  // Set gap (offset)
  if (this->offset_x_ != 0 || this->offset_y_ != 0) {
    err = esp_lcd_panel_set_gap(this->panel_handle_, this->offset_x_, this->offset_y_);
    if (err != ESP_OK) {
      ESP_LOGW(TAG, "Failed to set gap: %s", esp_err_to_name(err));
    }
  }

  // Turn on display
  err = esp_lcd_panel_disp_on_off(this->panel_handle_, true);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "Failed to turn on display: %s", esp_err_to_name(err));
  }
  App.feed_wdt();

  // Allocate persistent DMA buffer for fill() - this must live for the lifetime of the display
  this->fill_buffer_pixels_ = this->width_ * FILL_ROWS_PER_CHUNK;
  this->fill_buffer_ = (uint16_t *)heap_caps_malloc(
      this->fill_buffer_pixels_ * sizeof(uint16_t),
      MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
  if (this->fill_buffer_ == nullptr) {
    ESP_LOGE(TAG, "Failed to allocate DMA buffer for fill()");
    this->mark_failed();
    return;
  }
  
  // Allocate DMA transfer buffer for draw_pixels_at() - same size as fill buffer
  // LVGL and other callers may pass non-DMA-capable memory, so we copy to this buffer
  this->dma_transfer_buffer_size_ = this->fill_buffer_pixels_ * sizeof(uint16_t);
  this->dma_transfer_buffer_ = (uint8_t *)heap_caps_malloc(
      this->dma_transfer_buffer_size_,
      MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
  if (this->dma_transfer_buffer_ == nullptr) {
    ESP_LOGE(TAG, "Failed to allocate DMA transfer buffer");
    this->mark_failed();
    return;
  }

  // Small delay for display to stabilize
  delay(50);

  // Clear the display to black before turning on backlight
  // This prevents garbage pixels from showing
  this->fill(Color::BLACK);

  // Turn on backlight
  this->set_backlight_(true);

  ESP_LOGCONFIG(TAG, "ST7789 I80 display initialized successfully");
}

void ST7789I80::dump_config() {
  LOG_DISPLAY("", "ST7789 I80 Display", this);
  ESP_LOGCONFIG(TAG, "  Dimensions: %dx%d", this->width_, this->height_);
  ESP_LOGCONFIG(TAG, "  Offsets: x=%d, y=%d", this->offset_x_, this->offset_y_);
  LOG_PIN("  DC Pin: ", this->dc_pin_);
  LOG_PIN("  WR Pin: ", this->wr_pin_);
  if (this->cs_pin_ != nullptr) {
    LOG_PIN("  CS Pin: ", this->cs_pin_);
  }
  if (this->rd_pin_ != nullptr) {
    LOG_PIN("  RD Pin: ", this->rd_pin_);
  }
  if (this->reset_pin_ != nullptr) {
    LOG_PIN("  Reset Pin: ", this->reset_pin_);
  }
  if (this->backlight_pin_ != nullptr) {
    LOG_PIN("  Backlight Pin: ", this->backlight_pin_);
  }
  ESP_LOGCONFIG(TAG, "  Data Pins: D0=GPIO%d, D1=GPIO%d, D2=GPIO%d, D3=GPIO%d, D4=GPIO%d, D5=GPIO%d, D6=GPIO%d, D7=GPIO%d",
                this->data_pins_[0]->get_pin(), this->data_pins_[1]->get_pin(), 
                this->data_pins_[2]->get_pin(), this->data_pins_[3]->get_pin(),
                this->data_pins_[4]->get_pin(), this->data_pins_[5]->get_pin(), 
                this->data_pins_[6]->get_pin(), this->data_pins_[7]->get_pin());
  ESP_LOGCONFIG(TAG, "  Pixel Clock: %d Hz", this->pclk_frequency_);
  ESP_LOGCONFIG(TAG, "  Invert Colors: %s", YESNO(this->invert_colors_));
  ESP_LOGCONFIG(TAG, "  Swap XY: %s", YESNO(this->swap_xy_));
  ESP_LOGCONFIG(TAG, "  Mirror X: %s", YESNO(this->mirror_x_));
  ESP_LOGCONFIG(TAG, "  Mirror Y: %s", YESNO(this->mirror_y_));
  LOG_UPDATE_INTERVAL(this);
}

void ST7789I80::update() {
  this->do_update_();
}

void ST7789I80::draw_pixels_at(int x_start, int y_start, int w, int h, const uint8_t *ptr,
                                display::ColorOrder order, display::ColorBitness bitness,
                                bool big_endian, int x_offset, int y_offset, int x_pad) {
  if (w <= 0 || h <= 0)
    return;

  // If color mapping is required, pass to parent implementation
  if (bitness != display::COLOR_BITNESS_565) {
    return display::Display::draw_pixels_at(x_start, y_start, w, h, ptr, order, bitness, 
                                            big_endian, x_offset, y_offset, x_pad);
  }

  // DMA requires data in DMA-capable memory. LVGL and other callers may pass
  // non-DMA-capable buffers, causing LoadStoreAlignment crashes. Copy to our
  // DMA-safe buffer in chunks.
  
  esp_err_t err = ESP_OK;
  const size_t bytes_per_pixel = 2;  // RGB565
  const size_t max_pixels_per_transfer = this->dma_transfer_buffer_size_ / bytes_per_pixel;
  
  if (x_offset == 0 && x_pad == 0 && y_offset == 0) {
    // Simple case - contiguous data
    const size_t total_pixels = w * h;
    const uint8_t *src = ptr;
    int y_pos = y_start;
    size_t pixels_remaining = total_pixels;
    
    while (pixels_remaining > 0) {
      // Calculate how many rows we can send in this chunk
      size_t pixels_this_chunk = std::min(pixels_remaining, max_pixels_per_transfer);
      // Round down to full rows for simplicity
      size_t rows_this_chunk = pixels_this_chunk / w;
      if (rows_this_chunk == 0) rows_this_chunk = 1;  // At least 1 row
      pixels_this_chunk = rows_this_chunk * w;
      if (pixels_this_chunk > pixels_remaining) pixels_this_chunk = pixels_remaining;
      
      size_t bytes_this_chunk = pixels_this_chunk * bytes_per_pixel;
      
      // Copy to DMA-safe buffer
      memcpy(this->dma_transfer_buffer_, src, bytes_this_chunk);
      
      // Send the chunk
      err = esp_lcd_panel_draw_bitmap(this->panel_handle_, x_start, y_pos,
                                       x_start + w, y_pos + rows_this_chunk,
                                       this->dma_transfer_buffer_);
      
      // Wait for DMA to complete before reusing buffer
      this->wait_for_pending_transfers_();
      
      if (err != ESP_OK) break;
      
      src += bytes_this_chunk;
      y_pos += rows_this_chunk;
      pixels_remaining -= pixels_this_chunk;
    }
  } else {
    // Need to handle offsets - draw line by line
    auto stride = (x_offset + w + x_pad) * bytes_per_pixel;
    size_t row_bytes = w * bytes_per_pixel;
    
    for (int y = 0; y < h; y++) {
      // Copy row to DMA-safe buffer
      memcpy(this->dma_transfer_buffer_, 
             ptr + ((y + y_offset) * stride + x_offset * bytes_per_pixel),
             row_bytes);
      
      err = esp_lcd_panel_draw_bitmap(this->panel_handle_, x_start, y + y_start,
                                       x_start + w, y + y_start + 1,
                                       this->dma_transfer_buffer_);
      // Wait for DMA to complete before reusing buffer
      this->wait_for_pending_transfers_();
      if (err != ESP_OK)
        break;
    }
  }

  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to draw bitmap: %s", esp_err_to_name(err));
  }
}

void ST7789I80::draw_pixel_at(int x, int y, Color color) {
  if (x < 0 || x >= this->get_width_internal() || y < 0 || y >= this->get_height_internal())
    return;
  
  // Convert color to RGB565
  uint16_t color565 = display::ColorUtil::color_to_565(color);
  // Swap bytes for correct endianness
  uint16_t pixel = __builtin_bswap16(color565);
  
  esp_lcd_panel_draw_bitmap(this->panel_handle_, x, y, x + 1, y + 1, &pixel);
  // CRITICAL: Must wait for DMA to complete since 'pixel' is on the stack!
  this->wait_for_pending_transfers_();
}

void ST7789I80::fill(Color color) {
  if (this->panel_handle_ == nullptr || this->fill_buffer_ == nullptr)
    return;

  App.feed_wdt();

  const int width = this->get_width_internal();
  const int height = this->get_height_internal();

  // Convert to RGB565 and swap bytes for ST7789
  uint16_t color565 = display::ColorUtil::color_to_565(color);
  uint16_t pixel = __builtin_bswap16(color565);

  // Pre-fill the persistent DMA buffer with the color
  for (size_t i = 0; i < this->fill_buffer_pixels_; i++) {
    this->fill_buffer_[i] = pixel;
  }

  const int rows_per_chunk = this->fill_buffer_pixels_ / width;

  for (int y = 0; y < height; y += rows_per_chunk) {
    int rows = rows_per_chunk;
    if (y + rows > height) {
      rows = height - y;
    }

    esp_err_t err = esp_lcd_panel_draw_bitmap(
        this->panel_handle_,
        0, y,
        width, y + rows,
        this->fill_buffer_);

    if (err != ESP_OK) {
      ESP_LOGE(TAG, "fill(): draw failed: %s", esp_err_to_name(err));
      break;
    }

    // Wait for DMA to complete before reusing buffer
    this->wait_for_pending_transfers_();

    // Feed watchdog periodically
    App.feed_wdt();
  }
}

void ST7789I80::wait_for_pending_transfers_() {
  // esp_lcd_panel_io_tx_param blocks until all pending color transfers are complete
  // Sending a NOP command (0x00) is a safe way to synchronize
  esp_lcd_panel_io_tx_param(this->io_handle_, 0x00, nullptr, 0);
}

void ST7789I80::hard_reset_() {
  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->setup();
    this->reset_pin_->digital_write(true);
    delay(10);
    this->reset_pin_->digital_write(false);
    delay(10);
    this->reset_pin_->digital_write(true);
    delay(120);
  }
}

void ST7789I80::set_backlight_(bool on) {
  if (this->backlight_pin_ != nullptr) {
    this->backlight_pin_->setup();
    this->backlight_pin_->digital_write(on);
  }
}

}  // namespace st7789_i80
}  // namespace esphome

#endif  // USE_ESP32
