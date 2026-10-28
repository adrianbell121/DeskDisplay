#include "scene_store.h"
#include "scene_player.h"
#include "desk_ui.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_heap_caps.h"
#include "esp_lvgl_port.h"
#include "esp_partition.h"
#include "esp_timer.h"

/* Two raw flash slots. A new header is written last; interrupted writes retain
 * the other slot. No filesystem, credentials, or existing NVS data is involved. */
#define SLOT_BYTES 0x80000
#define STORE_MAGIC 0x54534c46U
static const esp_partition_t *partition;
static uint8_t *active, *upload;
static size_t expected, received;
static uint32_t expected_crc, sequence;
static int active_slot=-1;
static int64_t last_chunk;
typedef struct { uint32_t magic, sequence, size, crc; } record_t;
static uint32_t crc32(const uint8_t *data, size_t size)
{
    uint32_t crc=0xffffffff;
    for(size_t i=0;i<size;i++){crc^=data[i];for(unsigned j=0;j<8;j++)crc=(crc>>1)^(0xedb88320U&-(crc&1U));}
    return ~crc;
}
static void abort_upload(void) { free(upload); upload=NULL; expected=received=0; }
static uint8_t *allocate(size_t size)
{
    uint8_t *p=heap_caps_malloc(size,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    return p?p:malloc(size);
}
esp_err_t scene_store_init(void)
{
    partition=esp_partition_find_first(ESP_PARTITION_TYPE_DATA,ESP_PARTITION_SUBTYPE_ANY,"scenes");
    if(!partition||partition->size<2*SLOT_BYTES)return ESP_ERR_NOT_FOUND;
    for(int slot=0;slot<2;slot++){
        record_t h;
        if(esp_partition_read(partition,slot*SLOT_BYTES,&h,sizeof(h))!=ESP_OK||h.magic!=STORE_MAGIC||h.size<8||h.size>SCENE_MAX_BYTES||h.sequence<=sequence)continue;
        uint8_t *candidate=allocate(h.size);if(!candidate)continue;
        if(esp_partition_read(partition,slot*SLOT_BYTES+sizeof(h),candidate,h.size)==ESP_OK&&crc32(candidate,h.size)==h.crc&&scene_player_set(candidate,h.size,esp_timer_get_time()/1000)){
            free(active);active=candidate;sequence=h.sequence;active_slot=slot;
        }else free(candidate);
    }
    return ESP_OK;
}
static int nibble(char c)
{ return c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:-1; }
static esp_err_t persist_scene(void)
{
    int slot=active_slot==0?1:0;
    record_t h={STORE_MAGIC,sequence+1,(uint32_t)expected,expected_crc};
    size_t erase=(sizeof(h)+expected+4095)&~(size_t)4095;
    esp_err_t err=esp_partition_erase_range(partition,slot*SLOT_BYTES,erase);
    if(err==ESP_OK)err=esp_partition_write(partition,slot*SLOT_BYTES+sizeof(h),upload,expected);
    if(err==ESP_OK)err=esp_partition_write(partition,slot*SLOT_BYTES,&h,sizeof(h));
    if(err==ESP_OK){active_slot=slot;sequence=h.sequence;}
    return err;
}
void scene_store_command(const char *arg)
{
    if(!arg){puts("SCENE ERROR missing operation");return;}
    if(!strcmp(arg,"abort")){abort_upload();puts("SCENE ABORTED");return;}
    if(!strncmp(arg,"begin ",6)){
        size_t size;unsigned crc;char extra;
        if(sscanf(arg+6,"%zu %x %c",&size,&crc,&extra)!=2||size<8||size>SCENE_MAX_BYTES){puts("SCENE ERROR size must be 8..262144");return;}
        if(!partition){puts("SCENE ERROR install scene partition firmware first");return;}
        abort_upload();upload=allocate(size);
        if(!upload){puts("SCENE ERROR no memory");return;}
        expected=size;expected_crc=crc;last_chunk=esp_timer_get_time();puts("SCENE READY");return;
    }
    if(!upload){puts("SCENE ERROR begin an upload first");return;}
    if(esp_timer_get_time()-last_chunk>60000000){abort_upload();puts("SCENE ERROR upload expired");return;}
    if(!strncmp(arg,"data ",5)){
        char *end;unsigned long offset=strtoul(arg+5,&end,10);
        if(end==arg+5||*end!=' '||offset!=received){puts("SCENE ERROR wrong offset");return;}
        const char *hex=end+1;size_t length=strlen(hex);
        if(!length||length>64||(length&1)||length/2>expected-received){puts("SCENE ERROR invalid chunk length");return;}
        for(size_t i=0;i<length;i++)if(nibble(hex[i])<0){puts("SCENE ERROR invalid hex");return;}
        for(size_t i=0;i<length;i+=2)upload[received++]=(nibble(hex[i])<<4)|nibble(hex[i+1]);
        last_chunk=esp_timer_get_time();printf("SCENE DATA %u\n",(unsigned)received);return;
    }
    if(!strcmp(arg,"commit")){
        if(received!=expected||crc32(upload,expected)!=expected_crc||!scene_player_validate(upload,expected)){abort_upload();puts("SCENE ERROR incomplete, corrupt, or invalid scene");return;}
        // Reserve the renderer before changing either flash or its live data.
        if(!lvgl_port_lock(2000)){puts("SCENE ERROR display busy; retry commit");return;}
        esp_err_t err=persist_scene();
        if(err==ESP_OK){scene_player_set(upload,expected,esp_timer_get_time()/1000);free(active);active=upload;upload=NULL;expected=received=0;desk_ui_custom_changed();}
        lvgl_port_unlock();
        if(err==ESP_OK)puts("SCENE OK saved=1 mode=custom");else printf("SCENE ERROR flash %s\n",esp_err_to_name(err));
        return;
    }
    puts("SCENE ERROR invalid operation");
}
