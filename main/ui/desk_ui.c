#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "board.h"
#include "desk_ui.h"
#include "esp_timer.h"
#include "lvgl.h"

enum { CLOCK, ORBIT, RAIN, TEST };
static const char *names[] = {"clock", "orbit", "rain", "test"};
static int mode;
static int64_t test_until;
static uint16_t pixels[64 * 64] __attribute__((aligned(4)));
static lv_obj_t *canvas, *heading, *detail, *footer;
static time_t last_second = -1;
static uint16_t rgb(unsigned r, unsigned g, unsigned b)
{ return ((r & 248) << 8) | ((g & 252) << 3) | (b >> 3); }
static void pixel(int x, int y, uint16_t c)
{ if ((unsigned)x < 64 && (unsigned)y < 64) pixels[y * 64 + x] = c; }
static void rect(int x, int y, int w, int h, uint16_t c)
{ for (int dy = 0; dy < h; dy++) for (int dx = 0; dx < w; dx++) pixel(x + dx, y + dy, c); }
static const uint8_t digits[10][7] = {
    {14,17,19,21,25,17,14}, {4,12,4,4,4,4,14}, {14,17,1,2,4,8,31},
    {30,1,1,14,1,1,30}, {2,6,10,18,31,2,2}, {31,16,16,30,1,1,30},
    {14,16,16,30,17,17,14}, {31,1,2,4,8,8,8}, {14,17,17,14,17,17,14},
    {14,17,17,15,1,1,14},
};
static void digit(int value, int x, int y, uint16_t c)
{
    for (int row = 0; row < 7; row++) for (int col = 0; col < 5; col++)
        if (digits[value][row] & (1 << (4 - col))) rect(x + col * 2, y + row * 2, 2, 2, c);
}
static void clock_face(const struct tm *t, bool valid, float phase)
{
    const uint16_t white = rgb(216, 240, 255), cyan = rgb(52, 210, 218);
    if (valid) {
        int hour = t->tm_hour % 12;
        if (!hour) hour = 12;
        digit(hour / 10, 5, 17, white); digit(hour % 10, 17, 17, white);
        digit(t->tm_min / 10, 37, 17, white); digit(t->tm_min % 10, 49, 17, white);
        if (t->tm_sec % 2 == 0) { rect(31, 20, 2, 2, cyan); rect(31, 26, 2, 2, cyan); }
    } else {
        for (int x = 7; x < 60; x += 14) rect(x, 23, 8, 2, white);
    }
    for (int x = 0; x < 64; x++) {
        float wave = sinf(x * .085f + phase) * 3 + sinf(x * .14f - phase * .7f) * 2;
        int y = 51 + (int)wave;
        for (int d = 0; d < 5; d++) {
            unsigned strength = 160 / (d + 1);
            pixel(x, y + d, rgb(12, strength, strength + 48));
        }
    }
    rect(3, 62, 58, 1, rgb(8, 20, 28));
    if (valid) rect(3, 62, (t->tm_sec * 58) / 59, 1, cyan);
}
static void orbit_face(float phase)
{
    for (int i = 0; i < 28; i++) {
        int x = (i * 37 + 11) % 64, y = 11 + (i * 23 + 7) % 42;
        unsigned light = 35 + (unsigned)((sinf(phase + i) + 1) * 35);
        pixel(x, y, rgb(light / 2, light / 2, light));
    }
    for (int i = 0; i < 180; i++) {
        float a = i * 6.2831853f / 180;
        float x = 22 * cosf(a), y = 8 * sinf(a);
        pixel(32 + (int)(x * .94f - y * .34f), 32 + (int)(x * .34f + y * .94f), rgb(18, 80, 100));
    }
    for (int y = -8; y <= 8; y++) for (int x = -8; x <= 8; x++) {
        if (x * x + y * y <= 64) {
            float light = (8 - x) / 16.0f;
            pixel(32 + x, 32 + y, rgb(30 + light * 80, 80 + light * 140, 150 + light * 100));
        }
    }
    float x = 22 * cosf(phase), y = 8 * sinf(phase);
    rect(31 + (int)(x * .94f - y * .34f), 31 + (int)(x * .34f + y * .94f), 3, 3, rgb(255, 190, 92));
}
static void rain_face(float phase)
{
    for (int i = 0; i < 20; i++) {
        int x = (i * 17 + 3) % 64;
        int head = ((int)(phase * (8 + i % 7)) + i * 31) % 84 - 10;
        for (int tail = 0; tail < 10; tail++) {
            unsigned light = 240 / (tail + 1);
            pixel(x, head - tail, tail == 0 ? rgb(165, 255, 220) : rgb(0, light, light / 2));
        }
    }
}
static lv_obj_t *label(int y, uint32_t color)
{
    lv_obj_t *obj = lv_label_create(lv_screen_active());
    lv_obj_set_style_text_font(obj, &lv_font_unscii_8, 0);
    lv_obj_set_style_text_color(obj, lv_color_hex(color), 0);
    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(obj, 64);
    lv_obj_set_pos(obj, 0, y);
    lv_label_set_text(obj, "");
    return obj;
}
static void render(lv_timer_t *timer)
{
    (void)timer;
    int64_t now_us = esp_timer_get_time();
    if (mode == TEST && now_us > test_until) { mode = CLOCK; last_second = -1; }
    float phase = (now_us % 3600000000LL) / 1000000.0f;
    time_t now = time(NULL);
    struct tm t;
    localtime_r(&now, &t);
    bool valid = board_clock_valid();
    memset(pixels, 0, sizeof(pixels));
    if (mode == CLOCK) clock_face(&t, valid, phase);
    if (mode == ORBIT) orbit_face(phase);
    if (mode == RAIN) rain_face(phase);
    if (mode == TEST) {
        static const uint16_t colors[] = {0xf800, 0x07e0, 0x001f, 0x8410};
        unsigned stage = (now_us / 1000000) % 4;
        rect(0, 0, 64, 64, colors[stage]);
        rect(0, 0, 3, 3, 0xffff); rect(61, 0, 3, 6, 0xffff);
        rect(0, 58, 3, 6, 0xffff); rect(58, 61, 6, 3, 0xffff);
    }
    lv_obj_invalidate(canvas);
    if (now == last_second) return;
    last_second = now;
    char top[16] = "SET TIME", date[16] = "USB SYNC", small[16] = "--:--";
    if (valid) {
        static const char *days[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
        static const char *months[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
        snprintf(top, sizeof(top), "%s  %s", days[t.tm_wday], t.tm_hour < 12 ? "AM" : "PM");
        snprintf(date, sizeof(date), "%s %02d", months[t.tm_mon], t.tm_mday);
        int hour = t.tm_hour % 12;
        snprintf(small, sizeof(small), "%02d:%02d", hour ? hour : 12, t.tm_min);
    }
    lv_label_set_text(heading, mode == CLOCK ? top : mode == ORBIT ? "ORBIT" : mode == RAIN ? "RAIN" : "RGB TEST");
    lv_label_set_text(detail, mode == CLOCK ? date : "");
    lv_label_set_text(footer, mode == ORBIT || mode == RAIN ? small : "");
}
esp_err_t desk_ui_init(void)
{
    lv_obj_t *screen = lv_screen_active();
    lv_obj_remove_style_all(screen);
    lv_obj_set_style_bg_color(screen, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    canvas = lv_canvas_create(screen);
    lv_canvas_set_buffer(canvas, pixels, 64, 64, LV_COLOR_FORMAT_RGB565);
    lv_obj_set_pos(canvas, 0, 0);
    heading = label(3, 0x4db0b8);
    detail = label(36, 0x9faec0);
    footer = label(55, 0xb8dae7);
    if (!lv_timer_create(render, 33, NULL)) return ESP_ERR_NO_MEM;
    render(NULL);
    return ESP_OK;
}
bool desk_ui_mode(const char *name)
{
    for (int i = CLOCK; i <= RAIN; i++) if (strcmp(name, names[i]) == 0) {
        mode = i; last_second = -1; return true;
    }
    return false;
}
void desk_ui_next(void) { mode = (mode + 1) % 3; last_second = -1; }
const char *desk_ui_mode_name(void) { return names[mode]; }
void desk_ui_test(void) { mode = TEST; test_until = esp_timer_get_time() + 8000000; last_second = -1; }
