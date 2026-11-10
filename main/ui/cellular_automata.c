#include "cellular_automata.h"
#include "reverse_clock.h"
#include "esp_heap_caps.h"
#include "esp_random.h"
#include "island_face.h"
#include "cellular_automata_settings.h"

static uint16_t *clock_pixels;
static uint16_t cell_color(unsigned x,unsigned y,int64_t age_ms);

static bool clock_mask(uint64_t rows[64],int64_t target_ms)
{
    if(!clock_pixels)clock_pixels=heap_caps_calloc(4096,sizeof(*clock_pixels),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    if(!clock_pixels)return false;
    time_t epoch=target_ms/1000;struct tm t;localtime_r(&epoch,&t);
    island_element_render(clock_pixels,0,&t,true,0);
    uint64_t clock[13]={0};
    for(unsigned y=0;y<13;y++)for(unsigned x=0;x<49;x++)if(clock_pixels[(15+y)*64+5+x])clock[y]|=UINT64_C(1)<<x;
    cellular_automata_clock(rows,clock);return true;
}

static bool prepare_time(cellular_automata_t *s,int64_t target_ms)
{
    if(!clock_mask(s->frames[0],target_ms))return false;
    // Play this forward history backwards; Life itself has no unique inverse.
    for(unsigned i=1;i<=CA_TIME_REVERSE_FRAMES;i++){
        memcpy(s->frames[i],s->frames[i-1],sizeof(s->frames[i]));cellular_automata_step_rows(s->frames[i]);
    }
    s->overlay_at_ms=target_ms;s->overlay_pending=true;return true;
}

void reverse_clock_render(reverse_clock_t **state,uint16_t *pixels,bool valid,int64_t wall_ms)
{
    if(!valid)return;
    if(!*state){
        unsigned n=reverse_clock_generations(CA_UPDATE_MS,CA_TIME_HOLD_SECONDS*1000);
        *state=heap_caps_calloc(1,sizeof(**state)+(n/REVERSE_CLOCK_BLOCK+1)*sizeof(uint64_t[64]),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
        if(!*state)return;
        (*state)->generations=n;(*state)->target_ms=-1;
    }
    reverse_clock_t *s=*state;
    int64_t minute=wall_ms/60000*60000,target=wall_ms-minute<CA_TIME_HOLD_SECONDS*1000LL?minute:minute+60000;
    if(s->target_ms!=target){
        if(!clock_mask(s->block[0],target))return;
        reverse_clock_prepare(s,s->block[0]);s->target_ms=target;
    }
    unsigned frame=cellular_automata_frame(wall_ms,target,s->generations,CA_UPDATE_MS);
    const uint64_t *rows=reverse_clock_frame(s,frame);
    for(unsigned y=0;y<64;y++)for(unsigned x=0;x<64;x++)if((rows[y]>>x)&1)pixels[y*64+x]=cell_color(x,y,0);
}

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
                             const struct tm *time, bool valid, int64_t now_ms,int64_t wall_ms)
{
    (void)color;
    if(!*state){
        *state=heap_caps_calloc(1,sizeof(**state)+(CA_TIME_REVERSE_FRAMES+1)*sizeof(uint64_t[64]),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
        if(!*state)return;
        (*state)->minute_ms=-1;(*state)->overlay_at_ms=-1;(*state)->updated_ms=now_ms;(*state)->spawned_ms=now_ms;
    }
    cellular_automata_t *s=*state;
    cellular_automata_age(s,now_ms,CA_FADE_AFTER_SECONDS*1000LL,CA_FADE_DURATION_SECONDS*1000LL);
    int64_t minute_ms=wall_ms/60000*60000;
    bool initial=s->minute_ms<0,changed=valid&&s->minute_ms!=minute_ms;
    if(changed){
        if(time->tm_min%CA_CLEAR_EVERY_MINUTES==0){memset(s->cells,0,sizeof(s->cells));memset(s->born,0,sizeof(s->born));}
        s->minute_ms=minute_ms;
    }
    if(now_ms-s->updated_ms>=CA_UPDATE_MS){cellular_automata_step(s);s->updated_ms=now_ms;}
    if(!valid){s->overlay_pending=false;s->overlay_at_ms=-1;}
    else{
        if(s->overlay_pending&&(s->overlay_at_ms/60000<minute_ms/60000||s->overlay_at_ms/60000>minute_ms/60000+1)){s->overlay_pending=false;s->overlay_at_ms=-1;}
        if(s->overlay_pending&&wall_ms>=s->overlay_at_ms+CA_TIME_HOLD_SECONDS*1000LL){
            cellular_automata_merge(s,s->frames[0],now_ms);s->overlay_pending=false;
        }
        int64_t target=cellular_automata_target(wall_ms,CA_TIME_REVERSE_FRAMES,CA_UPDATE_MS,CA_TIME_HOLD_SECONDS*1000);
        if(target<0&&initial)target=wall_ms;
        else if(target<0&&changed&&s->overlay_at_ms!=minute_ms)target=minute_ms;
        if(target>=0&&target!=s->overlay_at_ms){
            if(s->overlay_pending){cellular_automata_merge(s,s->frames[0],now_ms);s->overlay_pending=false;}
            prepare_time(s,target);
        }
        if(s->overlay_pending&&wall_ms>=s->overlay_at_ms+CA_TIME_HOLD_SECONDS*1000LL){
            cellular_automata_merge(s,s->frames[0],now_ms);s->overlay_pending=false;
        }
    }
    if(cellular_automata_spawn_due(s,now_ms,CA_SWITCH_ENGINE_SPAWN_SECONDS)){
        uint32_t random=esp_random();cellular_automata_switch_engine(s,random&63,(random>>6)&63,(random>>12)&7,now_ms);
    }
    cellular_automata_age(s,now_ms,CA_FADE_AFTER_SECONDS*1000LL,CA_FADE_DURATION_SECONDS*1000LL);
    for(unsigned dy=0;dy<h;dy++)for(unsigned dx=0;dx<w;dx++){
        int tx=x+(int)dx,ty=y+(int)dy;unsigned sx=(2*dx+1)*64/(2*w),sy=(2*dy+1)*64/(2*h);
        if((unsigned)tx>=64||(unsigned)ty>=64)continue;
        if((s->cells[sy]>>sx)&1)pixels[ty*64+tx]=cell_color(sx,sy,now_ms+1-s->born[sy*64+sx]);
        if(s->overlay_pending){unsigned frame=cellular_automata_frame(wall_ms,s->overlay_at_ms,CA_TIME_REVERSE_FRAMES,CA_UPDATE_MS);
            if(sx>=7&&sx<56&&sy>=25&&sy<38)pixels[ty*64+tx]=0;
            if((s->frames[frame][sy]>>sx)&1)pixels[ty*64+tx]=cell_color(sx,sy,0);
        }
    }
}
