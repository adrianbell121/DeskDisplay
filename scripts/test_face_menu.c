#include <assert.h>
#include <stdio.h>
#include "../main/ui/face_menu.h"
static bool valid_scene(const uint8_t *data,size_t size){return size==8&&!memcmp(data,"FLS2",4);}
static size_t entry(uint8_t *out,unsigned kind,const char *id,const char *name,const uint8_t *data,unsigned size)
{
    unsigned a=(unsigned)strlen(id),b=(unsigned)strlen(name);memset(out,0,12);out[0]=(uint8_t)kind;
    face_menu_put16(out+2,a);face_menu_put16(out+4,b);face_menu_put32(out+8,size);
    memcpy(out+12,id,a);memcpy(out+12+a,name,b);if(size)memcpy(out+12+a+b,data,size);
    return 12+a+b+size;
}
int main(void)
{
    uint8_t data[512]={0},bad[512],scene[8]={'F','L','S','2',0};face_menu_entry_t faces[FACE_MENU_LIMIT];
    memcpy(data,"FMN1",4);face_menu_put16(data+4,2);
    size_t second=8+entry(data+8,0,"a","Saved island",scene,8),size=second+entry(data+second,3,"b","Cellular automata",NULL,0);
    assert(face_menu_parse(data,size,faces,valid_scene)==2);assert(!strcmp(faces[0].name,"Saved island"));assert(faces[0].scene==data+second-8);assert(faces[1].kind==3);
    for(size_t n=0;n<size;n++)assert(!face_menu_parse(data,n,faces,valid_scene));
    assert(!face_menu_parse(NULL,size,faces,valid_scene));assert(!face_menu_parse(data,size+1,faces,valid_scene));
    const uint8_t unicode[]={0x49,0xe2,0x80,0x94,0xf0,0x9f,0x8c,0xb4};
    const uint8_t bad_utf8[]={0xed,0xa0,0x80},overlong[]={0xc0,0x80};
    assert(face_menu_text(unicode,sizeof(unicode)));assert(!face_menu_text(bad_utf8,sizeof(bad_utf8)));assert(!face_menu_text(overlong,sizeof(overlong)));
    for(unsigned test=0;test<8;test++){
        memcpy(bad,data,size);
        switch(test){case 0:bad[0]='X';break;case 1:bad[4]=33;break;case 2:bad[6]=1;break;case 3:bad[8]=5;break;case 4:bad[10]=81;break;case 5:bad[second+12]='a';break;case 6:face_menu_put32(bad+second+8,UINT32_MAX);break;case 7:bad[second-8]='X';break;}
        assert(!face_menu_parse(bad,size,faces,valid_scene));
    }
    puts("Face menu decoder checks passed");return 0;
}
