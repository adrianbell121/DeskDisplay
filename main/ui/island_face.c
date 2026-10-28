#include "island_face.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_random.h"
#include "esp_heap_caps.h"

static uint16_t *canvas;
static int scene_y;
static int birds; // 0 perched, 1 departing, 2 away, 3 returning.
static int64_t flight_start, next_flight;
static uint32_t random_state;

static uint16_t rgb(unsigned r, unsigned g, unsigned b)
{ return ((r & 248) << 8) | ((g & 252) << 3) | (b >> 3); }
static void dot(int x, int y, uint16_t color)
{ y += scene_y; if ((unsigned)x < 64 && (unsigned)y < 64) canvas[y * 64 + x] = color; }
static void box(int x, int y, int w, int h, uint16_t color)
{ for (int j = 0; j < h; j++) for (int i = 0; i < w; i++) dot(x + i, y + j, color); }
static void line(int x, int y, int tx, int ty, uint16_t color)
{
    int dx = abs(tx - x), sx = x < tx ? 1 : -1;
    int dy = -abs(ty - y), sy = y < ty ? 1 : -1, error = dx + dy;
    for (;;) {
        dot(x, y, color);
        if (x == tx && y == ty) break;
        int twice = 2 * error;
        if (twice >= dy) { error += dy; x += sx; }
        if (twice <= dx) { error += dx; y += sy; }
    }
}

// Compact mixed-case 3x5 font. Each character occupies four pixels.
static unsigned glyph(char c)
{
    switch (c) {
    case '0': return 0x7b6f;
    case '1': return 0x2c97;
    case '2': return 0x73e7;
    case '3': return 0x73cf;
    case '4': return 0x5bc9;
    case '5': return 0x79cf;
    case '6': return 0x79ef;
    case '7': return 0x7249;
    case '8': return 0x7bef;
    case '9': return 0x7bcf;
    case 'A': return 0x2bed;
    case 'D': return 0x6b6e;
    case 'F': return 0x79a4;
    case 'J': return 0x126a;
    case 'M': return 0x5fed;
    case 'N': return 0x5ffd;
    case 'O': return 0x7b6f;
    case 'P': return 0x7be4;
    case 'S': return 0x79cf;
    case 'a': return 0x0c7f;
    case 'b': return 0x49ae;
    case 'c': return 0x0f27;
    case 'e': return 0x0fe7;
    case 'g': return 0x0f79;
    case 'l': return 0x4923;
    case 'n': return 0x0d6d;
    case 'o': return 0x0f6f;
    case 'p': return 0x0f7c;
    case 'r': return 0x0d64;
    case 't': return 0x2e93;
    case 'u': return 0x0b6f;
    case 'v': return 0x0b6a;
    case 'y': return 0x0b79;
    case '-': return 0x01c0;
    case ':': return 0x0410;
    default: return 0;
    }
}
static void text(const char *s, int x, int y, uint16_t color)
{
    for (; *s; s++, x += 4) {
        unsigned bits = glyph(*s);
        for (int row = 0; row < 5; row++) for (int col = 0; col < 3; col++)
            if (bits & (1U << (14 - row * 3 - col))) dot(x + col, y + row, color);
    }
}
static void digit(int n, int x, int y, uint16_t color)
{
    static const uint8_t segments[] = {0x3f, 0x06, 0x5b, 0x4f, 0x66, 0x6d, 0x7d, 0x07, 0x7f, 0x6f};
    unsigned s = segments[n];
    if (s & 1) box(x + 1, y, 5, 2, color);
    if (s & 2) box(x + 5, y + 1, 2, 5, color);
    if (s & 4) box(x + 5, y + 7, 2, 5, color);
    if (s & 8) box(x + 1, y + 11, 5, 2, color);
    if (s & 16) box(x, y + 7, 2, 5, color);
    if (s & 32) box(x, y + 1, 2, 5, color);
    if (s & 64) box(x + 1, y + 6, 5, 2, color);
}
static float surface(float x, float phase)
{ return 53 + sinf(x * .085f + phase * .65f) * 1.25f + sinf(x * .14f - phase * .45f) * .75f; }
static void frond(int x, int y, int tx, int ty, uint16_t color)
{
    line(x, y, tx, ty, color);
    line(x, y + 1, tx, ty + 1, color);
}
static void palm(int x, int ground, int height)
{
    int crown = ground - height;
    uint16_t trunk = rgb(147, 119, 72), leaf = rgb(66, 128, 91);
    line(x, ground, x - 2, crown, trunk);
    line(x + 1, ground, x - 1, crown, rgb(115, 93, 57));
    frond(x - 2, crown, x - 7, crown - 2, leaf);
    frond(x - 7, crown - 2, x - 9, crown, leaf);
    frond(x - 2, crown, x + 3, crown - 3, leaf);
    frond(x + 3, crown - 3, x + 6, crown - 1, leaf);
    frond(x - 2, crown, x + 3, crown + 1, leaf);
    frond(x + 3, crown + 1, x + 5, crown + 4, leaf);
    frond(x - 2, crown, x - 6, crown + 2, leaf);
    frond(x - 6, crown + 2, x - 8, crown + 4, leaf);
    frond(x - 2, crown, x - 3, crown - 4, rgb(83, 151, 100));
    dot(x - 2, crown + 2, rgb(119, 90, 54));
    dot(x, crown + 2, rgb(119, 90, 54));
}
static void boat(float phase)
{
    // Translate the complete sprite by a single integer offset. Never rotate or
    // round individual transformed pixels: the boat's shape stays identical.
    int water = (int)lroundf(surface(47, phase));
    for (int y = -10; y <= 1; y++) for (int x = -5; x <= 5; x++) {
        uint16_t color = 0;
        if (y >= -9 && y <= -2 && x == 0) color = rgb(174, 161, 127);
        if (y >= -8 && y <= -2 && x < 0 && x >= -(y + 10) / 2) color = rgb(229, 233, 218);
        if (y >= -6 && y <= -2 && x > 0 && x <= (y + 8) / 2) color = rgb(207, 221, 213);
        if (y == 0 && x >= -5 && x <= 5) color = rgb(157, 111, 69);
        if (y == 1 && x >= -3 && x <= 3) color = rgb(113, 84, 59);
        if (color) dot(47 + x, water - 1 + y, color);
    }
}
static int64_t interval(void)
{
    random_state ^= random_state << 13;
    random_state ^= random_state >> 17;
    random_state ^= random_state << 5;
    return (20000 + random_state % 10001) * 1000LL;
}
const char *island_bird_state(void)
{
    static const char *names[] = {"perched", "departing", "away", "returning"};
    return names[birds];
}
int island_bird_next_ms(int64_t now_us)
{ return next_flight > now_us ? (int)((next_flight - now_us) / 1000) : 0; }
static void draw_birds(int64_t now_us, int index)
{
    if (!next_flight) {
        random_state = esp_random();
        if (!random_state) random_state = 1;
        next_flight = now_us + interval();
    }
    if ((birds == 1 || birds == 3) && now_us - flight_start >= 4000000)
        birds = birds == 1 ? 2 : 0;
    if (now_us >= next_flight) {
        birds = birds == 0 || birds == 3 ? 1 : 3;
        flight_start = now_us;
        next_flight = now_us + interval();
    }
    if (birds == 2) return;
    float elapsed = (now_us - flight_start) / 1000000.0f;
    for (int i = index; i <= index; i++) {
        float p = (elapsed - i * .35f) / 3.5f;
        p = fmaxf(0, fminf(1, p));
        if (birds == 3) p = 1 - p;
        if (birds == 0) p = 0;
        int x = lroundf(18 + i * 10 + p * 53);
        int y = lroundf(34 + i * 5 - sinf(p * 1.5708f) * (3 + i * 2));
        int wing = p > 0 && p < 1 && sinf(elapsed * 17 + i) > 0 ? 1 : -1;
        uint16_t color = rgb(155, 185, 177);
        dot(x, y, color); dot(x - 1, y + wing, color); dot(x + 1, y + wing, color);
    }
}
// Canonical source rectangles also used by the dashboard. They include the
// animation envelope, so resizing never clips the boat or the flying birds.
const int island_element_bounds[12][4] = {
 {5,15,49,13},{0,4,64,5},{0,59,64,5},
 {0,53,64,11},{0,53,64,11},{0,53,64,11},
 {6,59,28,3},{7,40,16,19},{17,44,16,15},
 {42,48,11,16},{0,38,64,14},{0,38,64,14}
};
void island_element_render(uint16_t *pixels, unsigned part, const struct tm *time, bool valid, int64_t now_us)
{
    canvas=pixels; scene_y=0;
    memset(pixels,0,64*64*sizeof(*pixels));
    uint16_t white = rgb(221, 230, 217), muted = rgb(134, 165, 158);
    if (valid) {
        static const char *months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
        char date[12];
        snprintf(date, sizeof(date), "%s %d", months[time->tm_mon], time->tm_mday);
        if (part == 1) text(date, (64 - ((int)strlen(date) * 4 - 1)) / 2, 4, muted);
        if (part == 0) {
        int hour = time->tm_hour % 12;
        if (!hour) hour = 12;
        if (hour >= 10) digit(hour / 10, 5, 15, white);
        digit(hour % 10, 14, 15, white);
        digit(time->tm_min / 10, 27, 15, white);
        digit(time->tm_min % 10, 36, 15, white);
        dot(23, 19, muted); dot(23, 24, muted);
        text(time->tm_hour < 12 ? "AM" : "PM", 47, 21, muted);
        }
    } else {
        if (part == 1) text("-- --", 22, 4, muted);
        if (part == 0) text("--:--", 22, 19, white);
    }
    float phase=(now_us % 3600000000LL)/1000000.0f;
    scene_y=8;
    if(part==2) box(0,51,64,13,rgb(5,35,45));
    if(part>=3 && part<=5) {
        int band=part-3;
        float timing=phase*(band==0?.8f:band==1?1.1f:1.f)+(2-band)*1.7f;
        for(int x=0;x<64;x++) {
            int y=(int)lroundf(surface(x,timing))-(2-band)*2;
            for(int d=0;d<2;d++) {
                unsigned strength=(64+band*28)/(d+1);
                dot(x,y+d,rgb(8,strength,strength+20));
            }
        }
    }
    if(part==6) {
        uint16_t sand=rgb(173,153,102);
        box(8,51,24,1,sand);box(6,52,28,1,sand);box(8,53,25,1,rgb(130,118,78));
    }
    if(part==7)palm(16,50,14);
    if(part==8)palm(26,50,10);
    if(part==9)boat(phase);
    if(part==10 || part==11)draw_birds(now_us,part-10);
}
void island_face_render(uint16_t *pixels, const struct tm *time, bool valid, int64_t now_us)
{
    static uint16_t *layer;
    if(!layer)layer=heap_caps_calloc(4096,sizeof(*layer),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    memset(pixels,0,4096*sizeof(*pixels));
    if(!layer)return;
    for(unsigned part=0;part<12;part++) {
        island_element_render(layer,part,time,valid,now_us);
        for(unsigned i=0;i<4096;i++)if(layer[i])pixels[i]=layer[i];
    }
}
