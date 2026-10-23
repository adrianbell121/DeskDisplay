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
    int minute;
    const char *birds;
    int birds_next_ms;
} desk_ui_state_t;
void desk_ui_get_state(desk_ui_state_t *state);
