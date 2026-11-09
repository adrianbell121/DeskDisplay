/* Offline checks: compile as C with the host compiler, then run. */
#include <assert.h>
#include <stdio.h>
#include "../main/ui/cellular_automata.h"

static void glider(cellular_automata_t *s)
{
    memset(s,0,sizeof(*s));s->cells[1]=4;s->cells[2]=8;s->cells[3]=14;
}
static unsigned population(const cellular_automata_t *s)
{
    unsigned n=0;for(unsigned y=0;y<64;y++)for(unsigned x=0;x<64;x++)n+=(unsigned)((s->cells[y]>>x)&1);return n;
}
int main(void)
{
    cellular_automata_t s={0},initial;
    for(unsigned i=0;i<100;i++)cellular_automata_step(&s);
    assert(population(&s)==0);
    s.cells[10]=s.cells[11]=UINT64_C(3)<<10;initial=s;
    for(unsigned i=0;i<100;i++)cellular_automata_step(&s);
    assert(!memcmp(s.cells,initial.cells,sizeof(s.cells)));
    memset(&s,0,sizeof(s));s.cells[9]=s.cells[10]=s.cells[11]=UINT64_C(1)<<10;initial=s;
    for(unsigned i=0;i<100;i++)cellular_automata_step(&s);
    assert(!memcmp(s.cells,initial.cells,sizeof(s.cells)));
    glider(&s);
    for(unsigned i=0;i<300;i++){cellular_automata_step(&s);assert(population(&s)==5);}
    memset(&s,0,sizeof(s));s.cells[63]=(UINT64_C(1)<<63)|3;
    cellular_automata_step(&s);assert(s.cells[62]==1&&s.cells[63]==1&&s.cells[0]==1);
    memset(s.cells,255,sizeof(s.cells));uint64_t clock[13]={0};clock[0]=1;clock[12]=UINT64_C(1)<<48;
    cellular_automata_stamp(&s,clock);
    const uint64_t mask=((UINT64_C(1)<<49)-1)<<7;
    for(unsigned y=0;y<64;y++)assert(s.cells[y]==(y<25||y>=38?UINT64_MAX:(UINT64_MAX&~mask)|(clock[y-25]<<7)));
    memset(&s,0,sizeof(s));s.cells[0]=1;
    cellular_automata_age(&s,0,30000,120000);assert(s.born[0]==1);
    cellular_automata_age(&s,149999,30000,120000);assert(s.cells[0]==1);
    cellular_automata_age(&s,150000,30000,120000);assert(s.cells[0]==0&&s.born[0]==0);
    s.cells[0]=1;cellular_automata_age(&s,151000,30000,120000);assert(s.born[0]==151001);
    puts("Cellular automata C checks passed");return 0;
}
