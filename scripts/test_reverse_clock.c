#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef _MSC_VER
#pragma warning(disable:4200)
#endif
#include "../main/ui/reverse_clock.h"

static uint32_t random_state=12345;
static uint32_t random32(void){random_state^=random_state<<13;random_state^=random_state>>17;random_state^=random_state<<5;return random_state;}

int main(void)
{
    uint64_t cells[64],reference[64],clock[64]={0};
    for(unsigned sample=0;sample<12;sample++){
        for(unsigned y=0;y<64;y++)cells[y]=((uint64_t)random32()<<32)|random32();
        memcpy(reference,cells,sizeof(cells));
        for(unsigned step=0;step<8;step++){
            cellular_automata_step_rows(reference);reverse_clock_step(cells);
            assert(!memcmp(cells,reference,sizeof(cells)));
        }
    }
    assert(reverse_clock_generations(125,5000)==440);
    assert(reverse_clock_generations(500,5000)==110);
    assert(reverse_clock_generations(333,5000)==166);
    assert(reverse_clock_generations(1,0)==60000);
    clock[25]=UINT64_C(7)<<12;clock[26]=UINT64_C(1)<<12;clock[27]=UINT64_C(1)<<13;
    const unsigned n=440;
    reverse_clock_t *s=calloc(1,sizeof(*s)+(n/REVERSE_CLOCK_BLOCK+1)*sizeof(uint64_t[64]));assert(s);s->generations=n;
    uint64_t (*expected)[64]=malloc((n+1)*sizeof(*expected));assert(expected);memcpy(expected[0],clock,sizeof(clock));
    for(unsigned i=1;i<=n;i++){memcpy(expected[i],expected[i-1],sizeof(clock));cellular_automata_step_rows(expected[i]);}
    reverse_clock_prepare(s,clock);
    for(unsigned i=n+1;i-->0;)assert(!memcmp(reverse_clock_frame(s,i),expected[i],sizeof(clock)));
    for(unsigned elapsed=0;elapsed<55000;elapsed+=125){
        unsigned frame=cellular_automata_frame(5000+elapsed,60000,n,125);
        assert(frame==n-elapsed/125);assert(!memcmp(reverse_clock_frame(s,frame),expected[frame],sizeof(clock)));
    }
    assert(cellular_automata_frame(60000,60000,n,125)==0);
    assert(!memcmp(reverse_clock_frame(s,0),clock,sizeof(clock)));
    // A new minute discards the previous clock's history completely.
    clock[10]=UINT64_C(3)<<4;reverse_clock_prepare(s,clock);
    assert(!memcmp(reverse_clock_frame(s,0),clock,sizeof(clock)));
    free(expected);free(s);
    assert(sizeof(reverse_clock_t)+(60000/REVERSE_CLOCK_BLOCK+1)*sizeof(uint64_t[64])<512*1024);
    puts("Reverse clock: Life parity, all 440 reverse frames, timing, and history replacement passed");return 0;
}
