#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

typedef struct {
    uint64_t cells[64];
    int64_t born[4096];
    int minute;
    int64_t updated_ms, hold_until_ms;
} cellular_automata_t;

void cellular_automata_render(cellular_automata_t **state, uint16_t *pixels,
                             int x, int y, unsigned w, unsigned h, uint16_t color,
                             const struct tm *time, bool valid, int64_t now_ms);

static inline void cellular_automata_stamp(cellular_automata_t *s, const uint64_t clock[13])
{
    const uint64_t mask=((UINT64_C(1)<<49)-1)<<7;
    for(unsigned y=0;y<13;y++){
        s->cells[25+y]=(s->cells[25+y]&~mask)|(clock[y]<<7);
        memset(&s->born[(25+y)*64+7],0,49*sizeof(s->born[0]));
    }
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

static inline void cellular_automata_step(cellular_automata_t *s)
{
    uint64_t next[64]={0};
    for (unsigned y=0;y<64;y++) for (unsigned x=0;x<64;x++) {
        unsigned neighbors=0;
        for (int dy=-1;dy<=1;dy++) for (int dx=-1;dx<=1;dx++)
            if(dx||dy)neighbors+=(s->cells[(y+dy+64)%64]>>((x+dx+64)%64))&1;
        if(neighbors==3 || (neighbors==2 && ((s->cells[y]>>x)&1)))next[y]|=UINT64_C(1)<<x;
    }
    memcpy(s->cells,next,sizeof(next));
}
