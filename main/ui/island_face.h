#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

// Called only by the LVGL task; uses the existing RGB565 canvas.
void island_face_render(uint16_t *pixels, const struct tm *time, bool valid, int64_t now_us);
const char *island_bird_state(void);
int island_bird_next_ms(int64_t now_us);
