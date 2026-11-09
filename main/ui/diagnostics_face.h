#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "scene_font.h"

static inline void diagnostics_postal(const char *location,char out[6])
{
    memcpy(out,"-----",6);
    if(!location)return;
    for(const char *p=location;*p;){
        if(*p<'0'||*p>'9'){p++;continue;}
        const char *start=p;while(*p>='0'&&*p<='9')p++;
        if(p-start==5){memcpy(out,start,5);out[5]=0;return;}
    }
}

static inline void diagnostics_lines(char rows[8][32],const char *ip,const char *zip,
                                     uint64_t uptime,const struct tm *t,bool valid)
{
    snprintf(rows[0],32,"DIAGNOSTICS");snprintf(rows[1],32,"IP");
    snprintf(rows[2],32,"%s",ip);snprintf(rows[3],32,"UPTIME");
    snprintf(rows[4],32,"%lluD %02u:%02u:%02u",(unsigned long long)(uptime/86400),
             (unsigned)(uptime/3600%24),(unsigned)(uptime/60%60),(unsigned)(uptime%60));
    snprintf(rows[5],32,"ZIP %s",zip);snprintf(rows[6],32,"TIME");
    if(valid)snprintf(rows[7],32,"%02d:%02d:%02d %s",t->tm_hour%12?t->tm_hour%12:12,t->tm_min,t->tm_sec,t->tm_hour<12?"AM":"PM");
    else snprintf(rows[7],32,"--:--:-- --");
}

static inline void diagnostics_render(uint16_t *pixels,char rows[8][32])
{
    static const int ys[]={1,9,16,24,31,40,48,55};
    for(unsigned row=0;row<8;row++){
        int x=(64-(int)strlen(rows[row])*4+1)/2;
        for(const char *p=rows[row];*p;p++,x+=4){
            unsigned bits=0;
            for(unsigned i=0;i<sizeof(scene_glyphs)/sizeof(scene_glyphs[0]);i++)if(scene_glyphs[i].ch==*p){bits=scene_glyphs[i].bits;break;}
            for(int dy=0;dy<5;dy++)for(int dx=0;dx<3;dx++)
                if((bits&(1U<<(14-dy*3-dx)))&&(unsigned)(x+dx)<64)pixels[(ys[row]+dy)*64+x+dx]=(row==0||row==1||row==3||row==6)?0x46d9:0xdf3b;
        }
    }
}
