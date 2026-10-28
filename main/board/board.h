#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include "esp_err.h"
#include "driver/i2c_master.h"

#ifdef __cplusplus
extern "C" {
#endif

// Waveshare ESP32-S3-RGB-Matrix. HUB75 wiring is in sdkconfig.defaults.
#define BOARD_BUTTON 0
#define BOARD_I2C_SDA 47
#define BOARD_I2C_SCL 48
#define BOARD_I2S_MCLK 12
#define BOARD_I2S_BCLK 43
#define BOARD_I2S_WS 38
#define BOARD_I2S_DOUT 21
#define BOARD_AMP_ENABLE 11

esp_err_t board_display_init(void);
void board_display_brightness(unsigned percent); // Call with the LVGL lock held.
uint32_t board_display_frames(void);
bool board_display_snapshot(uint16_t *rgb565); // Copy 64x64 pixels with LVGL locked.
esp_err_t board_clock_init(void);
esp_err_t board_clock_set(time_t utc);
esp_err_t board_clock_update_rtc(time_t utc);
bool board_clock_valid(void);
i2c_master_bus_handle_t board_i2c(void);
esp_err_t board_audio_start(void);
esp_err_t board_audio_test(unsigned volume); // 0..85, queued; never blocks LVGL.
esp_err_t board_audio_status(void);

#ifdef __cplusplus
}
#endif
