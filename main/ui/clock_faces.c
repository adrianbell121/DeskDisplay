#include "clock_faces.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define PI 3.14159265359f
static uint16_t *target;
static uint16_t sprite[64 * 64];
static const uint8_t numbers[10][7] = {
    {14,17,19,21,25,17,14}, {4,12,4,4,4,4,14}, {14,17,1,2,4,8,31},
    {30,1,1,14,1,1,30}, {2,6,10,18,31,2,2}, {31,16,16,30,1,1,30},
    {14,16,16,30,17,17,14}, {31,1,2,4,8,8,8}, {14,17,17,14,17,17,14},
    {14,17,17,15,1,1,14},
};

static float clamp(float n) { return fminf(1, fmaxf(0, n)); }
static float smooth(float n) { n = clamp(n); return n * n * (3 - 2 * n); }
static uint16_t color(int r, int g, int b)
{
    r = r < 0 ? 0 : r > 255 ? 255 : r;
    g = g < 0 ? 0 : g > 255 ? 255 : g;
    b = b < 0 ? 0 : b > 255 ? 255 : b;
    return ((r & 248) << 8) | ((g & 252) << 3) | (b >> 3);
}
static void dot(int x, int y, uint16_t c)
{ if ((unsigned)x < 64 && (unsigned)y < 64) target[y * 64 + x] = c; }
static void box(int x, int y, int w, int h, uint16_t c)
{ for (int dy = 0; dy < h; dy++) for (int dx = 0; dx < w; dx++) dot(x + dx, y + dy, c); }
static void line(int x0, int y0, int x1, int y1, uint16_t c)
{
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1, err = dx + dy;
    for (;;) {
        dot(x0, y0, c);
        if (x0 == x1 && y0 == y1) break;
        int twice = 2 * err;
        if (twice >= dy) { err += dy; x0 += sx; }
        if (twice <= dx) { err += dx; y0 += sy; }
    }
}
static void ellipse(float cx, float cy, float rx, float ry, uint16_t c)
{
    for (int i = 0; i < 100; i++) {
        float a = i * (2 * PI / 100);
        dot(lroundf(cx + cosf(a) * rx), lroundf(cy + sinf(a) * ry), c);
    }
}

// The two clock layers carry the old and new hour/minute through a transition.
// Incoming pixels are revealed only by the corresponding water movement.
static void time_layer(const struct tm *t, bool valid, water_face_t face,
                       float u, bool incoming, int pass)
{
    int hour = t->tm_hour % 12;
    if (!hour) hour = 12;
    const int values[] = {hour / 10, hour % 10, t->tm_min / 10, t->tm_min % 10};
    const int left[] = {5,17,37,49};
    float p = u < 0 ? 0 : clamp(incoming ? (u - .40f) / .60f : u / .55f);
    float eased = smooth(p);
    float scale = incoming ? eased : 1 - eased;
    float angle = incoming ? -(1 - p) * 7 : p * 7;
    float ca = cosf(angle), sa = sinf(angle);
    float flip = u < 0 ? 1 : fabsf(cosf(PI * u));
    for (int part = 0; part < 5; part++) {
        int width = part == 4 ? 2 : 10;
        for (int iy = 0; iy < 14; iy++) for (int ix = 0; ix < width; ix++) {
            bool ink;
            int x = part == 4 ? 31 + ix : left[part] + ix;
            int y = 14 + iy;
            if (part == 4) ink = (iy >= 3 && iy < 5) || (iy >= 9 && iy < 11);
            else if (!valid) ink = iy >= 6 && iy < 8 && ix >= 1 && ix < 9;
            else ink = numbers[values[part]][iy / 2] & (1 << (4 - ix / 2));
            if (!ink) continue;
            float px = x, py = y, light = 1;
            if (u >= 0) {
                if (face == WATER_FACE_CUP) {
                    if (incoming) { py -= (1 - eased) * (24 + (x * 7) % 17); light = p; }
                    else { py += p * p * 44; px += p * 7; light = 1 - p; }
                } else if (face == WATER_FACE_DRAIN) {
                    float dx = x - 32, dy = y - 46;
                    px = 32 + scale * (dx * ca - dy * sa);
                    py = 46 + scale * (dx * sa + dy * ca);
                    light = incoming ? p : 1 - p;
                } else if (face == WATER_FACE_HOURGLASS) {
                    if (incoming != (u >= .5f)) continue;
                    py = 21 + (y - 21) * flip;
                } else if (face == WATER_FACE_TIDE) {
                    if (incoming != (x <= u * 90 - 14)) continue;
                } else if (face == WATER_FACE_RIPPLE) {
                    float dx = x - 32, dy = y - 48, radius = u * 79;
                    if (incoming != (dx * dx + dy * dy <= radius * radius)) continue;
                }
            }
            if (light < .02f) continue;
            uint16_t c = pass == 0 ? color(0, 5, 15) : color(216 * light, 244 * light, 255 * light);
            int out_x = lroundf(px) + (pass == 0), out_y = lroundf(py) + (pass == 0);
            if (out_y >= 12) dot(out_x, out_y, c);
        }
    }
}
static void clock_time(const clock_face_frame_t *f, water_face_t face)
{
    float u = f->rollover < 0 ? -1 : clamp(f->rollover / 3);
    for (int pass = 0; pass < 2; pass++) {
        if (u >= 0) time_layer(&f->before, f->valid, face, u, false, pass);
        time_layer(&f->now, f->valid, face, u, true, pass);
    }
}

static void water(float surface, float phase, int bottom, int left, int right)
{
    for (int x = left; x <= right; x++) {
        int top = lroundf(surface + sinf(x * .20f + phase * 2) * 1.4f
                        + sinf(x * .37f - phase) * .65f);
        for (int y = top; y <= bottom; y++) {
            int depth = y - top;
            dot(x, y, color(2, 70 - depth / 3, 127 - depth));
        }
        dot(x, top, color(65, 208, 231));
        if ((x + (int)(phase * 6)) % 19 == 0) dot(x, top + 1, color(154, 235, 242));
    }
}

static void sprite_pixel(int x, int y, uint16_t c)
{
    if ((unsigned)x < 64 && (unsigned)y < 64) sprite[y * 64 + x] = c;
}
static void rotate_sprite(float cx, float cy, float ca, float sa, float scale)
{
    // Inverse sampling fills every destination pixel; forward rotation leaves
    // checkerboard holes when the glass tilts between cardinal angles.
    for (int y = 28; y < 64; y++) for (int x = 0; x < 64; x++) {
        float dx = (x - cx) / scale, dy = (y - cy) / scale;
        int sx = lroundf(cx + dx * ca + dy * sa);
        int sy = lroundf(cy - dx * sa + dy * ca);
        if ((unsigned)sx < 64 && (unsigned)sy < 64 && sprite[sy * 64 + sx])
            dot(x, y, sprite[sy * 64 + sx]);
    }
}
static void cup(const clock_face_frame_t *f)
{
    memset(sprite, 0, sizeof(sprite));
    float progress = f->valid ? f->seconds / 60 : 0;
    float u = f->rollover < 0 ? -1 : clamp(f->rollover / 3);
    float tilt = u < 0 ? 0 : sinf(PI * u);
    float angle = tilt * 1.05f, scale = 1 - tilt * .25f;
    float ca = cosf(angle), sa = sinf(angle);
    float fill = u < 0 ? progress : (1 - smooth(u)) + progress * smooth(u);
    for (int y = 37; y <= 58; y++) {
        int half = 10 + (58 - y) / 6;
        for (int x = 31 - half; x <= 31 + half; x++) {
            float surface = 58 - fill * 21 + sinf(x * .4f + f->phase * 3) * .7f;
            uint16_t c = y >= surface ? color(4, 105 - (y - 37), 166) : color(2, 10, 18);
            if (y >= surface && y < surface + 1) c = color(90, 220, 240);
            sprite_pixel(x, y, c);
        }
        sprite_pixel(30 - half, y, color(101, 158, 174));
        sprite_pixel(32 + half, y, color(101, 158, 174));
    }
    for (int x = 17; x <= 45; x++) sprite_pixel(x, 36, color(190, 227, 235));
    for (int x = 20; x <= 42; x++) sprite_pixel(x, 59, color(122, 178, 192));
    for (int y = 41; y <= 52; y++) sprite_pixel(51, y, color(101, 158, 174));
    for (int x = 46; x <= 51; x++) {
        sprite_pixel(x, 40, color(101, 158, 174));
        sprite_pixel(x, 53, color(101, 158, 174));
    }
    rotate_sprite(31, 48, ca, sa, scale);
    if (u < 0 && f->valid) {
        line(25, 32, 31, 32, color(75, 131, 151));
        for (int y = 33; y < 58 - fill * 21; y++)
            if (((y + (int)(f->phase * 20)) % 7) < 4) dot(31, y, color(100, 220, 255));
    }
    if (u >= 0 && u < .88f) {
        float lip_x = 31 + scale * (14 * ca + 12 * sa);
        float lip_y = 48 + scale * (14 * sa - 12 * ca);
        for (int i = 0; i < 24; i++) {
            float p = clamp((u - i * .008f) * 1.5f);
            int x = lroundf(lip_x + p * (9 + i % 8));
            int y = lroundf(lip_y + p * (i % 5 - 8) + p * p * (18 + i % 11));
            box(x, y, i % 3 == 0 ? 2 : 1, 2, color(40, 166 + i % 70, 232));
        }
        water(62 - sinf(u * PI) * 3, f->phase * 2, 63, 0, 63);
    }
    clock_time(f, WATER_FACE_CUP);
}

static void drain(const clock_face_frame_t *f)
{
    float progress = f->valid ? f->seconds / 60 : 0;
    float u = f->rollover < 0 ? -1 : clamp(f->rollover / 3);
    if (u < 0) water(65 - progress * 65, f->phase, 63, 0, 63);
    else {
        float surface = -3 + smooth(u) * 68;
        for (int x = 0; x < 64; x++) {
            float dip = fmaxf(0, 1 - abs(x - 32) / 18.0f) * sinf(u * PI) * 14;
            water(surface + dip, f->phase * 2, 63, x, x);
        }
        for (int i = 0; i < 150; i++) {
            float p = i / 150.0f, a = p * 5 * PI + u * 16;
            float r = (1 - p) * (1 - u) * 29 + 1;
            dot(lroundf(32 + cosf(a) * r), lroundf(48 + sinf(a) * r * .5f),
                color(15, 115 * (1 - u), 180 * (1 - u)));
        }
        ellipse(32, 48, 2 + (1 - u) * 3, 1 + (1 - u), color(0, 10, 26));
    }
    clock_time(f, WATER_FACE_DRAIN);
}

static void hourglass(const clock_face_frame_t *f)
{
    memset(sprite, 0, sizeof(sprite));
    float progress = f->valid ? f->seconds / 60 : 0;
    float u = f->rollover < 0 ? -1 : clamp(f->rollover / 3);
    float angle = u < 0 ? 0 : smooth(u) * PI;
    float ca = cosf(angle), sa = sinf(angle);
    float fill = u < 0 ? progress : 1;
    float remaining = sqrtf(1 - clamp(fill));
    float top_surface = 46 - 11 * remaining;
    float bottom_surface = 59 - 11 * (1 - remaining);
    for (int y = 35; y <= 59; y++) {
        int half = abs(y - 47) + 1;
        for (int x = 32 - half; x <= 32 + half; x++) {
            bool wet = y <= 46 ? y >= top_surface : y >= bottom_surface;
            uint16_t c = wet ? color(4, 110, 165) : color(2, 12, 22);
            if (wet && (x + y) % 11 == 0) c = color(25, 151, 192);
            sprite_pixel(x, y, c);
        }
        sprite_pixel(31 - half, y, color(91, 163, 179));
        sprite_pixel(33 + half, y, color(91, 163, 179));
    }
    for (int x = 17; x <= 47; x++) {
        sprite_pixel(x, 34, color(157, 205, 213));
        sprite_pixel(x, 60, color(157, 205, 213));
    }
    rotate_sprite(32, 47, ca, sa, 1 - .22f * fabsf(sinf(2 * angle)));
    if (u < 0 && f->valid) {
        int drop = 43 + ((int)(f->phase * 15) % 10);
        box(32, drop, 1, 2, color(151, 231, 250));
    }
    clock_time(f, WATER_FACE_HOURGLASS);
}

static void tide(const clock_face_frame_t *f)
{
    float progress = f->valid ? f->seconds / 60 : 0;
    float u = f->rollover < 0 ? -1 : clamp(f->rollover / 3);
    float level = u < 0 ? progress : 1 - smooth(u);
    water(63 - level * 25, f->phase, 63, 0, 63);
    line(0, 62, 63, 62, color(6, 46, 65));
    // Lighthouse on a rock; its beam makes one full sweep each minute.
    box(46, 57, 16, 5, color(25, 42, 54));
    box(50, 39, 6, 18, color(161, 178, 176));
    box(50, 45, 6, 3, color(145, 63, 60));
    box(50, 52, 6, 3, color(145, 63, 60));
    box(49, 36, 8, 4, color(241, 199, 96));
    line(48, 35, 58, 35, color(80, 116, 135));
    line(49, 34, 53, 31, color(103, 147, 163));
    line(53, 31, 57, 34, color(103, 147, 163));
    float angle = progress * 2 * PI;
    for (int i = -1; i <= 1; i++)
        line(53, 37, 53 + (int)(cosf(angle + i * .05f) * 21),
             37 + (int)(sinf(angle + i * .05f) * 12), color(64, 62, 28));
    clock_time(f, WATER_FACE_TIDE);
    if (u >= 0) {
        float front = u * 90 - 14;
        for (int y = 10; y < 64; y++) {
            int crest = lroundf(front + sinf(y * .095f + u * 4) * 5);
            for (int dx = -5; dx <= 2; dx++)
                dot(crest + dx, y, dx >= 0 ? color(128, 226, 240) : color(8, 113 + dx * 8, 159));
        }
        ellipse(front - 3, 19, 6, 6, color(111, 219, 235));
    }
}

static void ripple(const clock_face_frame_t *f)
{
    float progress = f->valid ? f->seconds / 60 : 0;
    float u = f->rollover < 0 ? -1 : clamp(f->rollover / 3);
    for (int y = 33; y < 62; y++) for (int x = 2; x < 62; x++) {
        float dx = (x - 32) / 30.0f, dy = (y - 48) / 14.0f;
        if (dx * dx + dy * dy <= 1) dot(x, y, color(0, 15, 30));
    }
    for (int i = 0; i < 60; i++) {
        float a = -PI / 2 + i * (2 * PI / 60);
        bool lit = i < progress * 60;
        uint16_t c = lit ? color(32, 175, 197) : color(10, 36, 51);
        if (u >= 0 && u < .35f) c = color(90 * (1 - u), 210 * (1 - u), 235 * (1 - u));
        dot(lroundf(32 + cosf(a) * 29), lroundf(48 + sinf(a) * 13), c);
    }
    float fraction = f->seconds - floorf(f->seconds);
    for (int i = 0; i < 3; i++) {
        float radius = (fraction + i) * 9;
        ellipse(32, 48, radius, radius * .38f,
                color(8, 130 / (i + 1), 185 / (i + 1)));
    }
    if (f->valid && fraction < .4f) box(32, 33 + fraction * 34, 1, 3, color(166, 239, 252));
    clock_time(f, WATER_FACE_RIPPLE);
    if (u >= 0) {
        float radius = u * 79;
        ellipse(32, 48, radius, radius, color(110 * (1 - u), 230 * (1 - u), 250 * (1 - u)));
        ellipse(32, 48, radius + 2, radius + 2, color(0, 81 * (1 - u), 132 * (1 - u)));
    }
}

void clock_faces_render(uint16_t *pixels, water_face_t face, const clock_face_frame_t *frame)
{
    target = pixels;
    switch (face) {
        case WATER_FACE_CUP: cup(frame); break;
        case WATER_FACE_DRAIN: drain(frame); break;
        case WATER_FACE_HOURGLASS: hourglass(frame); break;
        case WATER_FACE_TIDE: tide(frame); break;
        case WATER_FACE_RIPPLE: ripple(frame); break;
    }
}
