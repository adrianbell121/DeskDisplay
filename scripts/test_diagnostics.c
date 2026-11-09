#include <assert.h>
#include "../main/ui/diagnostics_face.h"
int main(void)
{
    char zip[6],rows[8][32];
    diagnostics_postal("Bedford, MA, 01730",zip);assert(!strcmp(zip,"01730"));
    diagnostics_postal("Boston, MA",zip);assert(!strcmp(zip,"-----"));
    diagnostics_postal("123456",zip);assert(!strcmp(zip,"-----"));
    diagnostics_postal(NULL,zip);assert(!strcmp(zip,"-----"));
    struct tm t={.tm_hour=0,.tm_min=1,.tm_sec=2};
    diagnostics_lines(rows,"255.255.255.255","01730",90061,&t,true);
    assert(!strcmp(rows[4],"1D 01:01:01"));assert(!strcmp(rows[5],"ZIP 01730"));assert(!strcmp(rows[7],"12:01:02 AM"));
    t.tm_hour=12;diagnostics_lines(rows,"OFFLINE",zip,0,&t,true);assert(!strcmp(rows[7],"12:01:02 PM"));
    t.tm_hour=23;diagnostics_lines(rows,"OFFLINE",zip,0,&t,true);assert(!strcmp(rows[7],"11:01:02 PM"));
    diagnostics_lines(rows,"OFFLINE",zip,0,&t,false);assert(!strcmp(rows[7],"--:--:-- --"));
    uint16_t buffer[4098]={0};buffer[0]=buffer[4097]=0xffff;
    diagnostics_render(buffer+1,rows);assert(buffer[0]==0xffff&&buffer[4097]==0xffff);
    for(unsigned i=0;i<8;i++)assert(strlen(rows[i])*4-1<=64);
    puts("Diagnostics formatting, postal and rendering checks passed");return 0;
}
