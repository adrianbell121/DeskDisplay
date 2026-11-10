#pragma once
#include "cellular_automata.h"

#define REVERSE_CLOCK_BLOCK 64
typedef struct {
    int64_t target_ms;
    unsigned generations, block_index;
    uint64_t block[REVERSE_CLOCK_BLOCK][64];
    uint64_t checkpoints[][64];
} reverse_clock_t;

// Count neighbors in parallel so a full minute can be prepared between display frames.
static inline void reverse_clock_step(uint64_t cells[64])
{
    uint64_t next[64];
    for(unsigned y=0;y<64;y++){
        uint64_t a=cells[(y+63)%64],b=cells[y],c=cells[(y+1)%64];
        uint64_t neighbors[8]={(a<<1)|(a>>63),a,(a>>1)|(a<<63),(b<<1)|(b>>63),(b>>1)|(b<<63),(c<<1)|(c>>63),c,(c>>1)|(c<<63)};
        uint64_t ones=0,twos=0,fours=0,eights=0;
        for(unsigned i=0;i<8;i++){
            uint64_t carry=ones&neighbors[i];ones^=neighbors[i];
            uint64_t more=twos&carry;twos^=carry;
            eights|=fours&more;fours^=more;
        }
        next[y]=(ones|b)&twos&~fours&~eights;
    }
    memcpy(cells,next,sizeof(next));
}

static inline unsigned reverse_clock_generations(unsigned update_ms,unsigned hold_ms)
{
    return (60000-hold_ms+update_ms-1)/update_ms;
}

static inline void reverse_clock_prepare(reverse_clock_t *s,const uint64_t clock[64])
{
    memmove(s->block[0],clock,sizeof(s->block[0]));
    memcpy(s->checkpoints[0],s->block[0],sizeof(s->checkpoints[0]));
    // Checkpoints keep even the 1 ms configuration below 512 KB of history.
    for(unsigned i=1;i<=s->generations;i++){
        reverse_clock_step(s->block[0]);
        if(i%REVERSE_CLOCK_BLOCK==0)memcpy(s->checkpoints[i/REVERSE_CLOCK_BLOCK],s->block[0],sizeof(s->block[0]));
    }
    s->block_index=UINT32_MAX;
}

static inline const uint64_t *reverse_clock_frame(reverse_clock_t *s,unsigned frame)
{
    if(frame>s->generations)frame=s->generations;
    unsigned block=frame/REVERSE_CLOCK_BLOCK;
    if(block!=s->block_index){
        memcpy(s->block[0],s->checkpoints[block],sizeof(s->block[0]));
        for(unsigned i=1;i<REVERSE_CLOCK_BLOCK&&block*REVERSE_CLOCK_BLOCK+i<=s->generations;i++){
            memcpy(s->block[i],s->block[i-1],sizeof(s->block[i]));reverse_clock_step(s->block[i]);
        }
        s->block_index=block;
    }
    return s->block[frame%REVERSE_CLOCK_BLOCK];
}

void reverse_clock_render(reverse_clock_t **state,uint16_t *pixels,bool valid,int64_t wall_ms);
