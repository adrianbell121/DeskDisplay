#pragma once
#include <stdbool.h>
#include "esp_err.h"
// Hold lvgl_port_lock() for these calls. Future providers should enqueue events
// for the application task instead of calling LVGL from network callbacks.
esp_err_t desk_ui_init(void);
bool desk_ui_mode(const char *name);
void desk_ui_next(void);
const char *desk_ui_mode_name(void);
void desk_ui_test(void);

typedef struct {
    bool preview;
    int minute;
    int rollover_ms;
} desk_ui_state_t;

// -1 plays a 16-second demonstration; 0..64999 freezes a visual test sample.
// Preview advances only the UI, never the system clock or RTC. "off" ends it.
bool desk_ui_preview(const char *name, int sample_ms);
void desk_ui_get_state(desk_ui_state_t *state);
