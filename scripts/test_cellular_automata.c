/* Offline checks: compile as C with the host compiler, then run. */
#include <assert.h>
#include <stdio.h>
#ifdef _MSC_VER
// MSVC warns on standard C flexible-array members.
#pragma warning(disable:4200)
#endif
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
    memset(&s,0,sizeof(s));s.cells[0]=1;s.cells[25]=UINT64_C(1)<<6;s.born[0]=123;
    uint64_t clock[13]={0},overlay[64];clock[0]=1;clock[12]=UINT64_C(1)<<48;
    cellular_automata_clock(overlay,clock);cellular_automata_merge(&s,overlay,1000);
    assert(s.cells[0]==1&&s.born[0]==123);
    assert(s.cells[25]==((UINT64_C(1)<<6)|(UINT64_C(1)<<7)));
    assert(s.cells[37]==UINT64_C(1)<<55&&s.born[25*64+7]==1001);
    assert(cellular_automata_target(55999,8,500,5000)==-1);
    assert(cellular_automata_target(56000,8,500,5000)==60000);
    assert(cellular_automata_target(60000,8,500,5000)==60000);
    assert(cellular_automata_target(65000,8,500,5000)==-1);
    for(unsigned i=0;i<8;i++)assert(cellular_automata_frame(56000+i*500,60000,8,500)==8-i);
    assert(cellular_automata_frame(60000,60000,8,500)==0);
    assert(cellular_automata_target(59999,0,500,5000)==-1);
    memset(&s,0,sizeof(s));s.cells[0]=1;
    cellular_automata_age(&s,0,30000,120000);assert(s.born[0]==1);
    cellular_automata_age(&s,149999,30000,120000);assert(s.cells[0]==1);
    cellular_automata_age(&s,150000,30000,120000);assert(s.cells[0]==0&&s.born[0]==0);
    s.cells[0]=1;cellular_automata_age(&s,151000,30000,120000);assert(s.born[0]==151001);
    puts("Cellular automata C checks passed");return 0;
}
