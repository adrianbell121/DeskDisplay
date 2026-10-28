/* Portable scene decoder. The browser compiler emits this bounded FLS2 format. */
#include "scene_player.h"
#include <stdio.h>
#include <string.h>
#include "scene_font.h"
#include "island_face.h"
#include "esp_heap_caps.h"
#include <stdlib.h>

typedef struct {
    uint8_t type, w, h, frames, fps, format;
    int8_t x, y;
    uint16_t color, secondary, colors;
    const uint8_t *palette, *pixels;
} layer_t;
static layer_t layers[32];
static unsigned count;
static uint16_t background;
static bool ready;
static int64_t started;
static uint16_t *island_pixels;
static uint16_t native[54 * 14];
static uint8_t opaque[54 * 14];
static unsigned native_w, native_h;

static uint16_t u16(const uint8_t *p) { return p[0] | ((uint16_t)p[1] << 8); }
static bool decode(const uint8_t *data, size_t size, bool install)
{
    if (!data || size < 8 || size > SCENE_MAX_BYTES || memcmp(data, "FLS2", 4)) return false;
    unsigned n = u16(data + 4);
    if (n > 32) return false;
    size_t offset = 8;
    for (unsigned i = 0; i < n; i++) {
        if (size - offset < 16) return false;
        const uint8_t *p = data + offset;
        layer_t layer = {.type=p[0], .x=(int8_t)p[1], .y=(int8_t)p[2], .w=p[3], .h=p[4],
            .frames=p[5], .fps=p[6], .format=p[7], .color=u16(p+8), .secondary=u16(p+10), .colors=u16(p+12)};
        if (layer.type > 16 || layer.x < -63 || layer.y < -63 || !layer.w || layer.w > 64 ||
            !layer.h || layer.h > 64 || !layer.frames || layer.frames > 32 || !layer.fps ||
            layer.fps > 30 || layer.colors > 255 || (layer.format != 12 && layer.format != 24) || u16(p+14)) return false;
        size_t length = layer.type ? 0 : (size_t)layer.w * layer.h * layer.frames;
        if (layer.type && (layer.frames != 1 || layer.colors)) return false;
        size_t bytes = layer.colors * 2 + length;
        offset += 16;
        if (bytes > size - offset) return false;
        layer.palette = data + offset;
        layer.pixels = layer.palette + layer.colors * 2;
        for (size_t k = 0; k < length; k++) if (layer.pixels[k] > layer.colors) return false;
        offset += bytes;
        if (install) layers[i] = layer;
    }
    if (offset != size) return false;
    if (install) { count=n; background=u16(data+6); }
    return true;
}
bool scene_player_validate(const uint8_t *data, size_t size) { return decode(data, size, false); }
bool scene_player_set(const uint8_t *data, size_t size, int64_t now_ms)
{
    if (!scene_player_validate(data,size)) return false;
    decode(data,size,true); started=now_ms; ready=true; return true;
}
bool scene_player_ready(void) { return ready; }
static void dot(int x, int y, uint16_t color)
{
    if (x < 0 || y < 0 || x >= (int)native_w || y >= (int)native_h) return;
    unsigned i = y * native_w + x; native[i]=color; opaque[i]=1;
}
static void box(int x, int y, int w, int h, uint16_t color)
{ for (int j=0;j<h;j++) for (int i=0;i<w;i++) dot(x+i,y+j,color); }
static void text(const char *s, int x, int y, uint16_t color)
{
    for (; *s; s++,x+=4) {
        uint16_t bits=0;
        for (unsigned i=0;i<sizeof(scene_glyphs)/sizeof(scene_glyphs[0]);i++)
            if (scene_glyphs[i].ch==*s) { bits=scene_glyphs[i].bits; break; }
        for (int j=0;j<5;j++) for(int i=0;i<3;i++) if(bits & (1U<<(14-j*3-i))) dot(x+i,y+j,color);
    }
}
static void digit(int n, int x, int y, uint16_t color, bool blocks)
{
    if (blocks) {
        for(int j=0;j<7;j++)for(int i=0;i<5;i++)if(scene_digits[n][j]&(1U<<(4-i)))box(x+i*2,y+j*2,2,2,color);
        return;
    }
    unsigned s=scene_segments[n];
    if(s&1)box(x+1,y,5,2,color);
    if(s&2)box(x+5,y+1,2,5,color);
    if(s&4)box(x+5,y+7,2,5,color);
    if(s&8)box(x+1,y+11,5,2,color);
    if(s&16)box(x,y+7,2,5,color);
    if(s&32)box(x,y+1,2,5,color);
    if(s&64)box(x+1,y+6,5,2,color);
}
static void live(const layer_t *l, const struct tm *t, bool valid)
{
    native_w=l->type<=2?54:l->type==3?31:58;native_h=l->type<=2?14:l->type==3?5:1;
    memset(opaque,0,sizeof(opaque));
    if(!valid){if(l->type<=2)text("--:--",17,4,l->color);else if(l->type==3)text("-- --",6,0,l->color);return;}
    if(l->type<=2){
        int h=l->format==24?t->tm_hour:t->tm_hour%12?t->tm_hour%12:12;
        bool blocks=l->type==2;
        if(blocks){digit(h/10,0,0,l->color,true);digit(h%10,12,0,l->color,true);digit(t->tm_min/10,32,0,l->color,true);digit(t->tm_min%10,44,0,l->color,true);if(t->tm_sec%2==0){box(26,3,2,2,l->secondary);box(26,9,2,2,l->secondary);}}
        else{if(h>=10||l->format==24)digit(h/10,0,0,l->color,false);digit(h%10,9,0,l->color,false);digit(t->tm_min/10,22,0,l->color,false);digit(t->tm_min%10,31,0,l->color,false);dot(18,4,l->secondary);dot(18,9,l->secondary);if(l->format==12)text(t->tm_hour<12?"AM":"PM",42,6,l->secondary);}
    }else if(l->type==3){
        static const char *months[]={"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};char s[12];
        snprintf(s,sizeof(s),"%s %d",months[t->tm_mon],t->tm_mday);text(s,((int)native_w-((int)strlen(s)*4-1))/2,0,l->color);
    }else{box(0,0,58,1,l->secondary);box(0,0,t->tm_sec*58/59,1,l->color);}
}
void scene_player_render(uint16_t *pixels, const struct tm *time, bool valid, int64_t now_ms)
{
    for(unsigned i=0;i<4096;i++)pixels[i]=background;
    if(!ready)return;
    uint64_t elapsed=now_ms>started?(uint64_t)(now_ms-started):0;
    for(unsigned i=0;i<count;i++){
        const layer_t *l=&layers[i];unsigned frame=l->type?0:(elapsed*l->fps/1000)%l->frames;
        if(l->type>=5) {
            if(!island_pixels)island_pixels=heap_caps_calloc(4096,sizeof(*island_pixels),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
            if(!island_pixels)continue;
            unsigned part=l->type-5;
            const int *b=island_element_bounds[part];
            island_element_render(island_pixels,part,time,valid,now_ms*1000);
            for(unsigned y=0;y<l->h;y++)for(unsigned x=0;x<l->w;x++) {
                int tx=l->x+x,ty=l->y+y;
                if(tx<0||ty<0||tx>63||ty>63)continue;
                unsigned sx=b[0]+(2*x+1)*b[2]/(2*l->w),sy=b[1]+(2*y+1)*b[3]/(2*l->h);
                uint16_t color=island_pixels[sy*64+sx];
                static const uint16_t defaults[12][2]={{0xdf3b,0x8533},{0x8533,0x8533},{0x0105,0x0105},{0x0a0a,0x0906},{0x0aee,0x0968},{0x0bd1,0x09ea},{0xaccc,0x83a9},{0x440b,0x93a9},{0x440b,0x93a9},{0xe75b,0x9b68},{0x9dd6,0x9dd6},{0x9dd6,0x9dd6}};
                if(color==defaults[part][0])color=l->color;
                else if(color==defaults[part][1])color=l->secondary;
                if(island_pixels[sy*64+sx])pixels[ty*64+tx]=color;
            }
            continue;
        }
        if(l->type)live(l,time,valid);
        for(unsigned y=0;y<l->h;y++)for(unsigned x=0;x<l->w;x++){
            int tx=l->x+(int)x,ty=l->y+(int)y;if(tx<0||ty<0||tx>63||ty>63)continue;
            if(l->type){unsigned sx=(2*x+1)*native_w/(2*l->w),sy=(2*y+1)*native_h/(2*l->h),src=sy*native_w+sx;if(opaque[src])pixels[ty*64+tx]=native[src];}
            else{unsigned ix=l->pixels[(frame*l->h+y)*l->w+x];if(ix)pixels[ty*64+tx]=u16(l->palette+(ix-1)*2);}
        }
    }
}

unsigned scene_player_compare_island(const struct tm *time, bool valid, int64_t now_ms)
{
    uint16_t *expected=heap_caps_malloc(8192,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    uint16_t *actual=heap_caps_malloc(8192,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    if(!expected||!actual){free(expected);free(actual);return UINT32_MAX;}
    island_face_render(expected,time,valid,now_ms*1000);
    scene_player_render(actual,time,valid,now_ms);
    unsigned differences=0;
    for(unsigned i=0;i<4096;i++)if(expected[i]!=actual[i])differences++;
    free(expected);free(actual);return differences;
}
