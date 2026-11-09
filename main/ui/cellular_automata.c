#include "cellular_automata.h"
#include "esp_heap_caps.h"
#include "island_face.h"
#include "cellular_automata_settings.h"

static uint16_t *clock_pixels;

static uint16_t cell_color(unsigned x,unsigned y,int64_t age_ms)
{
    unsigned position=CA_GRADIENT==1?x:CA_GRADIENT==2?y:CA_GRADIENT==3?x+y:0;
    unsigned extent=CA_GRADIENT==3?126:63;
    float light=age_ms<=CA_FADE_AFTER_SECONDS*1000LL?1.f:1.f-(float)(age_ms-CA_FADE_AFTER_SECONDS*1000LL)/(CA_FADE_DURATION_SECONDS*1000LL);
    unsigned rgb[3];
    for(unsigned i=0;i<3;i++){
        unsigned a=(CA_COLOR>>(16-8*i))&255,b=(CA_GRADIENT_END_COLOR>>(16-8*i))&255;
        rgb[i]=(unsigned)(((a*(extent-position)+b*position)/extent)*light);
    }
    return ((rgb[0]&248)<<8)|((rgb[1]&252)<<3)|(rgb[2]>>3);
}

void cellular_automata_render(cellular_automata_t **state, uint16_t *pixels,
                             int x, int y, unsigned w, unsigned h, uint16_t color,
                             const struct tm *time, bool valid, int64_t now_ms)
{
    (void)color;
    if(!*state){
        *state=heap_caps_calloc(1,sizeof(**state),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
        if(!*state)return;
        (*state)->minute=-1;(*state)->updated_ms=now_ms;
    }
    cellular_automata_t *s=*state;
    cellular_automata_age(s,now_ms,CA_FADE_AFTER_SECONDS*1000LL,CA_FADE_DURATION_SECONDS*1000LL);
    int minute=time->tm_hour*60+time->tm_min;
    if(valid&&s->minute!=minute){
        if(!clock_pixels)clock_pixels=heap_caps_calloc(4096,sizeof(*clock_pixels),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
        if(!clock_pixels)return;
        island_element_render(clock_pixels,0,time,true,now_ms*1000);
        uint64_t clock[13]={0};
        for(unsigned dy=0;dy<13;dy++)for(unsigned dx=0;dx<49;dx++)
            if(clock_pixels[(15+dy)*64+5+dx])clock[dy]|=UINT64_C(1)<<dx;
        if(minute%CA_CLEAR_EVERY_MINUTES==0){memset(s->cells,0,sizeof(s->cells));memset(s->born,0,sizeof(s->born));}
        cellular_automata_stamp(s,clock);s->minute=minute;s->updated_ms=now_ms;
        s->hold_until_ms=now_ms+CA_TIME_HOLD_SECONDS*1000LL;
    }else if(now_ms>=s->hold_until_ms&&now_ms-s->updated_ms>=CA_UPDATE_MS){cellular_automata_step(s);s->updated_ms=now_ms;}
    cellular_automata_age(s,now_ms,CA_FADE_AFTER_SECONDS*1000LL,CA_FADE_DURATION_SECONDS*1000LL);
    for(unsigned dy=0;dy<h;dy++)for(unsigned dx=0;dx<w;dx++){
        int tx=x+(int)dx,ty=y+(int)dy;unsigned sx=(2*dx+1)*64/(2*w),sy=(2*dy+1)*64/(2*h);
        if((unsigned)tx<64&&(unsigned)ty<64&&((s->cells[sy]>>sx)&1))pixels[ty*64+tx]=cell_color(sx,sy,now_ms+1-s->born[sy*64+sx]);
    }
}
