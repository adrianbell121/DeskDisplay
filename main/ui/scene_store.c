#include "scene_store.h"
#include "scene_player.h"
#include "desk_ui.h"
#include "face_menu.h"
#include "cJSON.h"
#include <stdatomic.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_heap_caps.h"
#include "esp_lvgl_port.h"
#include "esp_partition.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

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
static SemaphoreHandle_t mutex;
static size_t active_size;
static atomic_uint live_crc;
static uint32_t saved_crc;
static uint8_t *preview;
static face_menu_entry_t *faces;
static unsigned face_count;
static atomic_int face_index;
static uint32_t menu_crc;
static const char *face_modes[]={"custom","clock","island","automata","diagnostics"};
typedef struct { uint32_t magic, sequence, size, crc; } record_t;
static uint32_t crc32(const uint8_t *data, size_t size)
{
    uint32_t crc=0xffffffff;
    for(size_t i=0;i<size;i++){crc^=data[i];for(unsigned j=0;j<8;j++)crc=(crc>>1)^(0xedb88320U&-(crc&1U));}
    return ~crc;
}
static void abort_upload(void) { free(upload); upload=NULL; expected=received=0; }
static void *allocate(size_t size)
{
    uint8_t *p=heap_caps_malloc(size,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    return p?p:malloc(size);
}
static esp_err_t persist_scene(const uint8_t *data,size_t size,uint32_t crc);
static bool activate_face(unsigned index)
{
    if(index>=face_count)return false;
    face_menu_entry_t *f=&faces[index];
    scene_player_clear();free(preview);preview=NULL;
    if(!f->kind&&!scene_player_set(f->scene,f->size,esp_timer_get_time()/1000))return false;
    face_index=(int)index;live_crc=f->kind?0:crc32(f->scene,f->size);
    return desk_ui_mode(face_modes[f->kind]);
}
static int current_face(void)
{
    const char *mode=desk_ui_mode_name();int index=face_index;
    if(!strcmp(mode,"custom")&&preview)return -1;
    if(index>=0&&(unsigned)index<face_count&&!strcmp(mode,face_modes[faces[index].kind]))return index;
    for(unsigned i=0;i<face_count;i++)if(faces[i].kind&&!strcmp(mode,face_modes[faces[i].kind]))return (int)i;
    return -1;
}
bool scene_store_faces_next(void)
{
    if(!face_count)return false;
    return activate_face((unsigned)(current_face()+1)%face_count);
}
static void install_menu(uint8_t *data,size_t size,face_menu_entry_t *entries,unsigned count,const char *selected)
{
    unsigned index=0;
    if(selected)for(unsigned i=0;i<count;i++)if(!strcmp(entries[i].id,selected)){index=i;break;}
    scene_player_clear();free(preview);preview=NULL;
    free(active);free(faces);active=data;active_size=size;faces=entries;face_count=count;
    menu_crc=crc32(data,size);activate_face(index);
}
static size_t write_face(uint8_t *p,unsigned kind,const char *id,const char *name,const uint8_t *data,size_t size)
{
    size_t id_size=strlen(id),name_size=strlen(name);memset(p,0,12);p[0]=kind;
    face_menu_put16(p+2,id_size);face_menu_put16(p+4,name_size);face_menu_put32(p+8,size);
    memcpy(p+12,id,id_size);memcpy(p+12+id_size,name,name_size);
    if(size)memcpy(p+12+id_size+name_size,data,size);
    return 12+id_size+name_size+size;
}
static esp_err_t migrate_faces(void)
{
    uint8_t *data=allocate(active_size+256);face_menu_entry_t *entries=allocate(FACE_MENU_LIMIT*sizeof(*entries));
    if(!data||!entries){free(data);free(entries);return ESP_ERR_NO_MEM;}
    memcpy(data,"FMN1\3\0\0\0",8);size_t at=8;
    at+=write_face(data+at,active?0:2,active?"saved-scene":"builtin:island",active?"Saved face":"Island",active,active_size);
    at+=write_face(data+at,3,"builtin:automata","Cellular automata",NULL,0);
    at+=write_face(data+at,4,"builtin:diagnostics","Diagnostics",NULL,0);
    unsigned count=face_menu_parse(data,at,entries,scene_player_validate);
    if(!count){free(data);free(entries);return ESP_ERR_INVALID_ARG;}
    install_menu(data,at,entries,count,NULL);
    return persist_scene(active,active_size,menu_crc);
}
esp_err_t scene_store_init(void)
{
    mutex=xSemaphoreCreateMutex();if(!mutex)return ESP_ERR_NO_MEM;
    partition=esp_partition_find_first(ESP_PARTITION_TYPE_DATA,ESP_PARTITION_SUBTYPE_ANY,"scenes");
    if(!partition||partition->size<2*SLOT_BYTES)return ESP_ERR_NOT_FOUND;
    for(int slot=0;slot<2;slot++){
        record_t h;
        if(esp_partition_read(partition,slot*SLOT_BYTES,&h,sizeof(h))!=ESP_OK||h.magic!=STORE_MAGIC||h.size<8||h.size>FACE_MENU_MAX_BYTES||h.sequence<=sequence)continue;
        uint8_t *candidate=allocate(h.size);if(!candidate)continue;
        bool installed=false;
        if(esp_partition_read(partition,slot*SLOT_BYTES+sizeof(h),candidate,h.size)==ESP_OK&&crc32(candidate,h.size)==h.crc){
            if(!memcmp(candidate,"FMN1",4)){
                face_menu_entry_t *entries=allocate(FACE_MENU_LIMIT*sizeof(*entries));
                unsigned count=entries?face_menu_parse(candidate,h.size,entries,scene_player_validate):0;
                if(count){install_menu(candidate,h.size,entries,count,NULL);installed=true;}else free(entries);
            }else if(scene_player_set(candidate,h.size,esp_timer_get_time()/1000)){
                free(active);free(faces);faces=NULL;face_count=0;active=candidate;active_size=h.size;live_crc=h.crc;installed=true;
            }
        }
        if(installed){saved_crc=h.crc;sequence=h.sequence;active_slot=slot;}else free(candidate);
    }
    return face_count?ESP_OK:migrate_faces();
}
static int nibble(char c)
{ return c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:-1; }
static esp_err_t persist_scene(const uint8_t *data,size_t size,uint32_t crc)
{
    int slot=active_slot==0?1:0;
    record_t h={STORE_MAGIC,sequence+1,(uint32_t)size,crc};
    size_t erase=(sizeof(h)+size+4095)&~(size_t)4095;
    esp_err_t err=esp_partition_erase_range(partition,slot*SLOT_BYTES,erase);
    if(err==ESP_OK)err=esp_partition_write(partition,slot*SLOT_BYTES+sizeof(h),data,size);
    if(err==ESP_OK)err=esp_partition_write(partition,slot*SLOT_BYTES,&h,sizeof(h));
    if(err==ESP_OK){active_slot=slot;sequence=h.sequence;saved_crc=crc;}
    return err;
}
static void command_locked(const char *arg)
{
    if(!arg){puts("SCENE ERROR missing operation");return;}
    if(!strcmp(arg,"abort")){abort_upload();puts("SCENE ABORTED");return;}
    if(!strncmp(arg,"begin ",6)){
        if(face_count){puts("SCENE ERROR use the board face menu to publish; USB uploads would replace that menu");return;}
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
        esp_err_t err=persist_scene(upload,expected,expected_crc);
        if(err==ESP_OK){scene_player_set(upload,expected,esp_timer_get_time()/1000);free(active);active=upload;active_size=expected;live_crc=expected_crc;upload=NULL;expected=received=0;desk_ui_custom_changed();}
        lvgl_port_unlock();
        if(err==ESP_OK)puts("SCENE OK saved=1 mode=custom");else printf("SCENE ERROR flash %s\n",esp_err_to_name(err));
        return;
    }
    puts("SCENE ERROR invalid operation");
}
void scene_store_command(const char *arg){xSemaphoreTake(mutex,portMAX_DELAY);command_locked(arg);xSemaphoreGive(mutex);}
esp_err_t scene_store_live(const uint8_t *data,size_t size){
    if(!scene_player_validate(data,size))return ESP_ERR_INVALID_ARG;
    uint8_t *copy=allocate(size);if(!copy)return ESP_ERR_NO_MEM;memcpy(copy,data,size);
    xSemaphoreTake(mutex,portMAX_DELAY);
    if(upload){xSemaphoreGive(mutex);free(copy);return ESP_ERR_INVALID_STATE;}
    if(!lvgl_port_lock(1000)){xSemaphoreGive(mutex);free(copy);return ESP_ERR_TIMEOUT;}
    scene_player_set_live(copy,size,esp_timer_get_time()/1000);
    if(face_count){free(preview);preview=copy;}else{free(active);active=copy;active_size=size;}
    live_crc=crc32(data,size);desk_ui_custom_changed();
    lvgl_port_unlock();xSemaphoreGive(mutex);return ESP_OK;
}
esp_err_t scene_store_save_live(void){
    xSemaphoreTake(mutex,portMAX_DELAY);
    esp_err_t err=face_count||live_crc==saved_crc?ESP_OK:partition&&active?persist_scene(active,active_size,live_crc):ESP_ERR_INVALID_STATE;
    xSemaphoreGive(mutex);return err;
}
void scene_store_crc(uint32_t *live,uint32_t *saved){xSemaphoreTake(mutex,portMAX_DELAY);*live=live_crc;int i=face_index;*saved=face_count?(i>=0&&(unsigned)i<face_count&&!faces[i].kind?crc32(faces[i].scene,faces[i].size):0):saved_crc;xSemaphoreGive(mutex);}

void scene_store_faces_state(uint32_t *revision,int *index)
{
    xSemaphoreTake(mutex,portMAX_DELAY);*revision=menu_crc;*index=-1;
    if(lvgl_port_lock(100)){*index=current_face();lvgl_port_unlock();}
    xSemaphoreGive(mutex);
}
char *scene_store_faces_json(void)
{
    xSemaphoreTake(mutex,portMAX_DELAY);
    cJSON *root=cJSON_CreateObject(),*items=cJSON_CreateArray();
    if(!root||!items){cJSON_Delete(root);cJSON_Delete(items);xSemaphoreGive(mutex);return NULL;}
    cJSON_AddNumberToObject(root,"revision",menu_crc);cJSON_AddNumberToObject(root,"limit",FACE_MENU_LIMIT);
    cJSON_AddNumberToObject(root,"maxBytes",FACE_MENU_MAX_BYTES);cJSON_AddNumberToObject(root,"bytes",active_size);
    int index=-1;if(lvgl_port_lock(100)){index=current_face();lvgl_port_unlock();}
    cJSON_AddNumberToObject(root,"activeIndex",index);cJSON_AddItemToObject(root,"items",items);
    bool ok=true;
    for(unsigned i=0;i<face_count&&ok;i++){
        face_menu_entry_t *f=&faces[i];cJSON *item=cJSON_CreateObject();
        if(!item){ok=false;break;}
        cJSON_AddItemToArray(items,item);cJSON_AddStringToObject(item,"id",f->id);cJSON_AddStringToObject(item,"name",f->name);cJSON_AddStringToObject(item,"mode",face_modes[f->kind]);
        if(!f->kind){
            char *hex=allocate(f->size*2+1);if(!hex){ok=false;break;}
            static const char digits[]="0123456789abcdef";
            for(size_t j=0;j<f->size;j++){hex[j*2]=digits[f->scene[j]>>4];hex[j*2+1]=digits[f->scene[j]&15];}hex[f->size*2]=0;
            if(!cJSON_AddStringToObject(item,"scene",hex))ok=false;
            free(hex);
        }
    }
    char *json=ok?cJSON_PrintUnformatted(root):NULL;cJSON_Delete(root);xSemaphoreGive(mutex);return json;
}
esp_err_t scene_store_faces_select(const char *id)
{
    if(!id)return ESP_ERR_INVALID_ARG;
    xSemaphoreTake(mutex,portMAX_DELAY);esp_err_t err=ESP_ERR_NOT_FOUND;
    if(lvgl_port_lock(1000)){
        for(unsigned i=0;i<face_count;i++)if(!strcmp(faces[i].id,id)){err=activate_face(i)?ESP_OK:ESP_FAIL;break;}
        lvgl_port_unlock();
    }else err=ESP_ERR_TIMEOUT;
    xSemaphoreGive(mutex);return err;
}
esp_err_t scene_store_faces_update(const char *json,size_t size)
{
    cJSON *root=cJSON_ParseWithLength(json,size),*items=cJSON_GetObjectItemCaseSensitive(root,"items"),*rev=cJSON_GetObjectItemCaseSensitive(root,"revision");
    unsigned count=cJSON_IsArray(items)?cJSON_GetArraySize(items):0;size_t length=8;
    esp_err_t err=ESP_ERR_INVALID_ARG;uint8_t *data=NULL;face_menu_entry_t *entries=NULL;
    if(!cJSON_IsNumber(rev)||rev->valuedouble<0||rev->valuedouble>UINT32_MAX||rev->valuedouble!=(double)(uint32_t)rev->valuedouble||!count||count>FACE_MENU_LIMIT)goto done;
    cJSON *item;
    cJSON_ArrayForEach(item,items){
        cJSON *id=cJSON_GetObjectItemCaseSensitive(item,"id"),*name=cJSON_GetObjectItemCaseSensitive(item,"name"),*mode=cJSON_GetObjectItemCaseSensitive(item,"mode"),*scene=cJSON_GetObjectItemCaseSensitive(item,"scene");
        if(!cJSON_IsString(id)||!strlen(id->valuestring)||strlen(id->valuestring)>80||!cJSON_IsString(name)||!strlen(name->valuestring)||strlen(name->valuestring)>240||!cJSON_IsString(mode))goto done;
        unsigned kind=0;while(kind<5&&strcmp(mode->valuestring,face_modes[kind]))kind++;
        if(kind==5||(kind?(scene&&(!cJSON_IsString(scene)||scene->valuestring[0])):(!cJSON_IsString(scene)||(strlen(scene->valuestring)&1)||strlen(scene->valuestring)>SCENE_MAX_BYTES*2)))goto done;
        length+=12+strlen(id->valuestring)+strlen(name->valuestring)+(kind?0:strlen(scene->valuestring)/2);
        if(length>FACE_MENU_MAX_BYTES){err=ESP_ERR_INVALID_SIZE;goto done;}
    }
    data=allocate(length);entries=allocate(FACE_MENU_LIMIT*sizeof(*entries));
    if(!data||!entries){err=ESP_ERR_NO_MEM;goto done;}
    memcpy(data,"FMN1",4);face_menu_put16(data+4,count);face_menu_put16(data+6,0);size_t at=8;
    cJSON_ArrayForEach(item,items){
        const char *id=cJSON_GetObjectItemCaseSensitive(item,"id")->valuestring,*name=cJSON_GetObjectItemCaseSensitive(item,"name")->valuestring,*mode=cJSON_GetObjectItemCaseSensitive(item,"mode")->valuestring;
        unsigned kind=0;while(strcmp(mode,face_modes[kind]))kind++;
        const char *hex=kind?"":cJSON_GetObjectItemCaseSensitive(item,"scene")->valuestring;size_t bytes=strlen(hex)/2;
        size_t header=write_face(data+at,kind,id,name,NULL,0);face_menu_put32(data+at+8,bytes);at+=header;
        for(size_t j=0;j<bytes;j++){int hi=nibble(hex[j*2]),lo=nibble(hex[j*2+1]);if(hi<0||lo<0)goto done;data[at++]=(hi<<4)|lo;}
    }
    if(!face_menu_parse(data,length,entries,scene_player_validate))goto done;
    xSemaphoreTake(mutex,portMAX_DELAY);
    if((uint32_t)rev->valuedouble!=menu_crc||upload)err=ESP_ERR_INVALID_STATE;
    else if(!partition)err=ESP_ERR_NOT_FOUND;
    else if(!lvgl_port_lock(2000))err=ESP_ERR_TIMEOUT;
    else{
        char selected[81]="";int index=current_face();if(index>=0)snprintf(selected,sizeof(selected),"%s",faces[index].id);
        uint32_t crc=crc32(data,length);err=persist_scene(data,length,crc);
        if(err==ESP_OK){install_menu(data,length,entries,count,selected);data=NULL;entries=NULL;}
        lvgl_port_unlock();
    }
    xSemaphoreGive(mutex);
done:
    free(data);free(entries);cJSON_Delete(root);return err;
}
