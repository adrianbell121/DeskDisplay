#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <sys/time.h>
#include "board.h"
#include "clock_faces.h"
#include "desk_ui.h"
#include "esp_timer.h"
#include "lvgl.h"

enum { CLOCK, CUP, DRAIN, HOURGLASS, TIDE, RIPPLE, FACE_COUNT, TEST = FACE_COUNT };
static const char *names[] = {"clock", "cup", "drain", "hourglass", "tide", "ripple", "test"};
static const char *titles[] = {"", "CUP", "DRAIN", "WTRGLASS", "TIDE", "RIPPLE", "RGB TEST"};
static int mode;
static int64_t test_until;
static uint16_t pixels[64 * 64] __attribute__((aligned(4)));
static lv_obj_t *canvas, *heading, *detail, *footer;
static time_t last_second = -1;
static int64_t previous_wall_us, previous_mono_us, transition_minute = -1;
static bool previous_valid;
static int64_t preview_started_us;
static time_t preview_base;
static int preview_sample_ms = -1;
static desk_ui_state_t ui_state;
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
static void reset_timeline(void)
{
    previous_valid = false;
    transition_minute = -1;
    last_second = -1;
}

static float live_rollover(int64_t wall_us, int64_t mono_us, bool valid)
{
    int64_t minute = wall_us / 60000000;
    bool continuous = valid && previous_valid &&
        llabs((wall_us - previous_wall_us) - (mono_us - previous_mono_us)) < 250000;
    if (!continuous) transition_minute = -1;
    else if (minute == previous_wall_us / 60000000 + 1) transition_minute = minute;
    else if (minute != previous_wall_us / 60000000) transition_minute = -1;
    previous_valid = valid;
    previous_wall_us = wall_us;
    previous_mono_us = mono_us;
    float second = (wall_us % 60000000) / 1000000.0f;
    return transition_minute == minute && second < 3 ? second : -1;
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
    struct timeval tv;
    gettimeofday(&tv, NULL);
    int64_t wall_us = (int64_t)tv.tv_sec * 1000000 + tv.tv_usec;
    bool valid = board_clock_valid();
    float rollover = live_rollover(wall_us, now_us, valid);
    bool preview = preview_started_us != 0;
    if (preview) {
        float elapsed = (now_us - preview_started_us) / 1000000.0f;
        if (elapsed >= (preview_sample_ms < 0 ? 16 : 30)) {
            preview_started_us = 0;
            preview = false;
            reset_timeline();
            rollover = -1;
        } else {
            float sample = preview_sample_ms >= 0 ? preview_sample_ms / 1000.0f :
                elapsed < 10 ? elapsed * 6 : 60 + elapsed - 10;
            wall_us = (int64_t)preview_base * 1000000 + (int64_t)(sample * 1000000);
            rollover = sample >= 60 && sample < 63 ? sample - 60 : -1;
        }
    }
    time_t now = wall_us / 1000000;
    struct tm t;
    localtime_r(&now, &t);
    ui_state.preview = preview;
    ui_state.minute = t.tm_min;
    ui_state.rollover_ms = rollover < 0 ? -1 : (int)(rollover * 1000);
    if (mode != CLOCK && rollover >= 0 && rollover < 1.5f) {
        time_t before = now - 60;
        struct tm old;
        localtime_r(&before, &old);
        ui_state.minute = old.tm_min;
    }
    memset(pixels, 0, sizeof(pixels));
    if (mode == CLOCK) clock_face(&t, valid, phase);
    if (mode >= CUP && mode < FACE_COUNT) {
        clock_face_frame_t frame = {.now = t, .valid = valid, .phase = phase,
            .seconds = valid ? (wall_us % 60000000) / 1000000.0f : 0,
            .rollover = rollover};
        time_t before = now - 60;
        localtime_r(&before, &frame.before);
        clock_faces_render(pixels, (water_face_t)(mode - CUP), &frame);
    }
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
    char top[16] = "SET TIME", date[16] = "USB SYNC";
    if (valid) {
        static const char *days[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
        static const char *months[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
        snprintf(top, sizeof(top), "%s  %s", days[t.tm_wday], t.tm_hour < 12 ? "AM" : "PM");
        snprintf(date, sizeof(date), "%s %02d", months[t.tm_mon], t.tm_mday);
    }
    lv_obj_set_style_text_color(heading, lv_color_hex(mode == CLOCK ? 0x4db0b8 : 0x83d7e6), 0);
    lv_label_set_text(heading, mode == CLOCK ? top : !valid ? "SET TIME" : preview ? "PREVIEW" : titles[mode]);
    lv_label_set_text(detail, mode == CLOCK ? date : "");
    lv_label_set_text(footer, "");
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
    for (int i = CLOCK; i < FACE_COUNT; i++) if (strcmp(name, names[i]) == 0) {
        mode = i; preview_started_us = 0; reset_timeline(); return true;
    }
    return false;
}
void desk_ui_next(void) { mode = (mode + 1) % FACE_COUNT; preview_started_us = 0; reset_timeline(); }
const char *desk_ui_mode_name(void) { return names[mode]; }
void desk_ui_test(void) { mode = TEST; preview_started_us = 0; test_until = esp_timer_get_time() + 8000000; reset_timeline(); }
void desk_ui_get_state(desk_ui_state_t *state) { *state = ui_state; }

bool desk_ui_preview(const char *name, int sample_ms)
{
    if (strcmp(name, "off") == 0) {
        preview_started_us = 0;
        reset_timeline();
        return true;
    }
    if (sample_ms < -1 || sample_ms > 64999 || !board_clock_valid()) return false;
    for (int i = CUP; i < FACE_COUNT; i++) if (strcmp(name, names[i]) == 0) {
        bool continuing = preview_started_us != 0 && mode == i;
        mode = i;
        if (!continuing) {
            time_t now = time(NULL);
            preview_base = now - now % 60;
        }
        preview_sample_ms = sample_ms;
        preview_started_us = esp_timer_get_time();
        reset_timeline();
        return true;
    }
    return false;
}
