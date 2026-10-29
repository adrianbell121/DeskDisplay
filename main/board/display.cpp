#include "board.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "hub75.h"
#include "lvgl.h"
#include <atomic>
#include <new>
#include <cstring>

static Hub75Driver *panel;
static std::atomic<uint32_t> frames{0};
static const uint8_t *last_frame;

static void flush(lv_display_t *display, const lv_area_t *area, uint8_t *pixels)
{
    panel->draw_pixels(area->x1, area->y1, area->x2 - area->x1 + 1,
                       area->y2 - area->y1 + 1, pixels,
                       Hub75PixelFormat::RGB565, Hub75ColorOrder::RGB, false);
    // FULL rendering gives each DMA back buffer a complete frame.
    if (lv_display_flush_is_last(display)) {
        last_frame = pixels;
        panel->flip_buffer();
        frames.fetch_add(1, std::memory_order_relaxed);
    }
    lv_display_flush_ready(display);
}

extern "C" esp_err_t board_display_init(void)
{
    Hub75Config cfg{};
    cfg.panel_width = CONFIG_HUB75_PANEL_WIDTH;
    cfg.panel_height = CONFIG_HUB75_PANEL_HEIGHT;
    cfg.layout_rows = 1;
    cfg.layout_cols = 1;
    cfg.layout = Hub75PanelLayout::HORIZONTAL;
    cfg.scan_wiring = Hub75ScanWiring::STANDARD_TWO_SCAN;
    cfg.shift_driver = Hub75ShiftDriver::GENERIC;
    cfg.pins.r1 = CONFIG_HUB75_PIN_R1;
    cfg.pins.g1 = CONFIG_HUB75_PIN_G1;
    cfg.pins.b1 = CONFIG_HUB75_PIN_B1;
    cfg.pins.r2 = CONFIG_HUB75_PIN_R2;
    cfg.pins.g2 = CONFIG_HUB75_PIN_G2;
    cfg.pins.b2 = CONFIG_HUB75_PIN_B2;
    cfg.pins.a = CONFIG_HUB75_PIN_A;
    cfg.pins.b = CONFIG_HUB75_PIN_B;
    cfg.pins.c = CONFIG_HUB75_PIN_C;
    cfg.pins.d = CONFIG_HUB75_PIN_D;
    cfg.pins.e = CONFIG_HUB75_PIN_E;
    cfg.pins.lat = CONFIG_HUB75_PIN_LAT;
    cfg.pins.oe = CONFIG_HUB75_PIN_OE;
    cfg.pins.clk = CONFIG_HUB75_PIN_CLK;
    cfg.output_clock_speed = Hub75ClockSpeed::HZ_10M;
    cfg.double_buffer = true;
    cfg.brightness = 64;
    panel = new (std::nothrow) Hub75Driver(cfg);
    if (!panel) return ESP_ERR_NO_MEM;
    if (!panel->begin()) { delete panel; panel = nullptr; return ESP_FAIL; }

    constexpr size_t bytes = 64 * 64 * sizeof(uint16_t);
    void *buffer = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!buffer) { delete panel; panel = nullptr; return ESP_ERR_NO_MEM; }
    lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    port_cfg.task_stack = 6144;
    port_cfg.task_max_sleep_ms = 33;
    esp_err_t err = lvgl_port_init(&port_cfg);
    if (err != ESP_OK) { heap_caps_free(buffer); delete panel; panel = nullptr; return err; }
    if (!lvgl_port_lock(2000)) return ESP_ERR_TIMEOUT;
    lv_display_t *display = lv_display_create(64, 64);
    if (!display) { lvgl_port_unlock(); return ESP_ERR_NO_MEM; }
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_flush_cb(display, flush);
    lv_display_set_buffers(display, buffer, nullptr, bytes, LV_DISPLAY_RENDER_MODE_FULL);
    lvgl_port_unlock();
    ESP_LOGI("display", "HUB75 64x64 + LVGL 9.3 ready; DMA double buffering, 25%% brightness");
    return ESP_OK;
}

extern "C" void board_display_brightness(unsigned percent)
{
    if (percent > 100) percent = 100;
    panel->set_brightness((percent * 255 + 50) / 100);
}
extern "C" uint32_t board_display_frames(void) { return frames.load(std::memory_order_relaxed); }
extern "C" bool board_display_snapshot(uint16_t *out)
{
    if (!last_frame) return false;
    std::memcpy(out, last_frame, 64 * 64 * sizeof(uint16_t));
    return true;
}
