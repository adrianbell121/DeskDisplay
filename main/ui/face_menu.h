#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#define FACE_MENU_LIMIT 32
#define FACE_MENU_MAX_BYTES (0x80000-16)
typedef struct {
    char id[81],name[241];
    unsigned kind;
    const uint8_t *scene;
    size_t size;
} face_menu_entry_t;

static inline unsigned face_menu_u16(const uint8_t *p){return p[0]|((unsigned)p[1]<<8);}
static inline uint32_t face_menu_u32(const uint8_t *p){return p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static inline void face_menu_put16(uint8_t *p,unsigned n){p[0]=(uint8_t)n;p[1]=(uint8_t)(n>>8);}
static inline void face_menu_put32(uint8_t *p,uint32_t n){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(n>>(8*i));}

static inline bool face_menu_text(const uint8_t *p,size_t size)
{
    for(size_t i=0;i<size;){
        unsigned ch=p[i++],need,min;
        if(ch<128){if(!ch)return false;continue;}
        if(ch>=0xc2&&ch<=0xdf){need=1;min=0x80;ch&=31;}
        else if(ch>=0xe0&&ch<=0xef){need=2;min=0x800;ch&=15;}
        else if(ch>=0xf0&&ch<=0xf4){need=3;min=0x10000;ch&=7;}
        else return false;
        if(need>size-i)return false;
        while(need--){if((p[i]&0xc0)!=0x80)return false;ch=(ch<<6)|(p[i++]&63);}
        if(ch<min||ch>0x10ffff||(ch>=0xd800&&ch<=0xdfff))return false;
    }
    return true;
}

static inline unsigned face_menu_parse(const uint8_t *data,size_t size,face_menu_entry_t *entries,
                                        bool (*valid_scene)(const uint8_t *,size_t))
{
    if(!data||size<8||size>FACE_MENU_MAX_BYTES||memcmp(data,"FMN1",4)||face_menu_u16(data+6))return 0;
    unsigned count=face_menu_u16(data+4);if(!count||count>FACE_MENU_LIMIT)return 0;
    size_t offset=8;
    for(unsigned i=0;i<count;i++){
        if(size-offset<12)return 0;
        const uint8_t *p=data+offset;unsigned id=face_menu_u16(p+2),name=face_menu_u16(p+4);
        size_t length=face_menu_u32(p+8);offset+=12;
        if(p[0]>5||p[1]||face_menu_u16(p+6)||!id||id>80||!name||name>240||id+name>size-offset)return 0;
        if(!face_menu_text(data+offset,id)||!face_menu_text(data+offset+id,name))return 0;
        face_menu_entry_t *e=&entries[i];memcpy(e->id,data+offset,id);e->id[id]=0;offset+=id;
        memcpy(e->name,data+offset,name);e->name[name]=0;offset+=name;
        e->kind=p[0];e->scene=data+offset;e->size=length;
        if(length>size-offset||(e->kind?length!=0:!valid_scene(e->scene,length)))return 0;
        for(unsigned j=0;j<i;j++)if(!strcmp(e->id,entries[j].id))return 0;
        offset+=length;
    }
    return offset==size?count:0;
}
