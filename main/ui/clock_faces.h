#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

typedef enum {
    WATER_FACE_CUP,
    WATER_FACE_DRAIN,
    WATER_FACE_HOURGLASS,
    WATER_FACE_TIDE,
    WATER_FACE_RIPPLE,
} water_face_t;

typedef struct {
    struct tm now;
    struct tm before;
    bool valid;
    float seconds;  // Actual fractional second within the minute, 0..60.
    float phase;    // Continuous animation time.
    float rollover; // Seconds since an observed minute boundary, or -1.
} clock_face_frame_t;

// Render into the existing RGB565 LVGL canvas. Call only from the LVGL task.
void clock_faces_render(uint16_t *pixels, water_face_t face, const clock_face_frame_t *frame);
