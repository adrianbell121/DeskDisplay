#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

typedef struct {
    uint64_t cells[64];
    int64_t born[4096];
    int64_t minute_ms, updated_ms, overlay_at_ms, spawned_ms;
    bool overlay_pending;
    uint64_t frames[][64];
} cellular_automata_t;

void cellular_automata_render(cellular_automata_t **state, uint16_t *pixels,
                             int x, int y, unsigned w, unsigned h, uint16_t color,
                             const struct tm *time, bool valid, int64_t now_ms, int64_t wall_ms);

static inline void cellular_automata_clock(uint64_t rows[64],const uint64_t clock[13])
{
    memset(rows,0,64*sizeof(*rows));
    for(unsigned y=0;y<13;y++)rows[25+y]=clock[y]<<7;
}

static inline void cellular_automata_merge(cellular_automata_t *s,const uint64_t overlay[64],int64_t now_ms)
{
    for(unsigned y=0;y<64;y++){
        uint64_t added=overlay[y]&~s->cells[y];
        for(unsigned x=0;x<64;x++)if((added>>x)&1)s->born[y*64+x]=now_ms+1;
        s->cells[y]|=overlay[y];
    }
}

static inline bool cellular_automata_spawn_due(cellular_automata_t *s,int64_t now_ms,unsigned interval_seconds)
{
    if(!interval_seconds||now_ms-s->spawned_ms<(int64_t)interval_seconds*1000)return false;
    s->spawned_ms=now_ms;return true;
}

static inline void cellular_automata_switch_engine(cellular_automata_t *s,unsigned x,unsigned y,unsigned orientation,int64_t now_ms)
{
    // Corderman's seed: https://conwaylife.com/patterns/switchengine.cells
    static const int points[8][2]={{1,0},{3,0},{0,1},{1,2},{4,2},{3,3},{4,3},{5,3}};
    for(unsigned i=0;i<8;i++){
        int dx=points[i][0],dy=points[i][1];
        if(orientation&4)dx=-dx;
        for(unsigned turn=0;turn<(orientation&3);turn++){int t=dx;dx=-dy;dy=t;}
        unsigned tx=(unsigned)((int)x+dx+64)%64,ty=(unsigned)((int)y+dy+64)%64;
        uint64_t bit=UINT64_C(1)<<tx;
        if(!(s->cells[ty]&bit))s->born[ty*64+tx]=now_ms+1;
        s->cells[ty]|=bit;
    }
}

static inline int64_t cellular_automata_target(int64_t wall_ms,unsigned frames,unsigned update_ms,unsigned hold_ms)
{
    int64_t minute=wall_ms/60000*60000,phase=wall_ms-minute;
    if(phase<hold_ms)return minute;
    if(frames&&60000-phase<=(int64_t)frames*update_ms)return minute+60000;
    return -1;
}

static inline unsigned cellular_automata_frame(int64_t wall_ms,int64_t target,unsigned frames,unsigned update_ms)
{
    if(wall_ms>=target)return 0;
    uint64_t remaining=(uint64_t)(target-wall_ms+update_ms-1)/update_ms;
    return remaining>frames?frames:(unsigned)remaining;
}

static inline void cellular_automata_age(cellular_automata_t *s,int64_t now_ms,int64_t delay_ms,int64_t fade_ms)
{
    for(unsigned y=0;y<64;y++)for(unsigned x=0;x<64;x++){
        unsigned i=y*64+x;
        if(!((s->cells[y]>>x)&1)){s->born[i]=0;continue;}
        if(!s->born[i])s->born[i]=now_ms+1;
        if(now_ms+1-s->born[i]>=delay_ms+fade_ms){s->cells[y]&=~(UINT64_C(1)<<x);s->born[i]=0;}
    }
}

static inline void cellular_automata_step_rows(uint64_t cells[64])
{
    uint64_t next[64]={0};
    for (unsigned y=0;y<64;y++) for (unsigned x=0;x<64;x++) {
        unsigned neighbors=0;
        for (int dy=-1;dy<=1;dy++) for (int dx=-1;dx<=1;dx++)
            if(dx||dy)neighbors+=(cells[(y+dy+64)%64]>>((x+dx+64)%64))&1;
        if(neighbors==3 || (neighbors==2 && ((cells[y]>>x)&1)))next[y]|=UINT64_C(1)<<x;
    }
    memcpy(cells,next,sizeof(next));
}

static inline void cellular_automata_step(cellular_automata_t *s){cellular_automata_step_rows(s->cells);}
