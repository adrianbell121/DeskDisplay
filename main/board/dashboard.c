#include "dashboard.h"
#include "dashboard_assets.h"
#include "scene_store.h"
#include "weather.h"
#include "diagnostics_face.h"
#include "esp_http_server.h"
#include "esp_partition.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "lwip/sockets.h"
#include "lwip/tcp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/idf_additions.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <inttypes.h>

#define WORKSPACE_LIMIT (1024*1024)
#define SLOT_SIZE (2*1024*1024)
#define MAGIC 0x44535732U
typedef struct {uint32_t magic,revision,size,crc;} record_t;
static const esp_partition_t *partition;
static SemaphoreHandle_t mutex;
static char *workspace;
static size_t workspace_size;
static uint32_t revision,saved_revision;
static int slot=-1;
static int64_t dirty_at;
static bool scene_dirty;
static esp_err_t save_error;
static httpd_handle_t server;
static char postal[6]="-----";
static portMUX_TYPE postal_mux=portMUX_INITIALIZER_UNLOCKED;
void dashboard_postal(char out[6]){portENTER_CRITICAL(&postal_mux);memcpy(out,postal,6);portEXIT_CRITICAL(&postal_mux);}
static void update_postal(cJSON *root){
 cJSON *state=cJSON_GetObjectItemCaseSensitive(root,"state"),*faces=cJSON_GetObjectItemCaseSensitive(state,"faces");
 cJSON *selected=cJSON_GetObjectItemCaseSensitive(state,"selected"),*fallback=cJSON_GetObjectItemCaseSensitive(state,"defaultFaceId");char out[6]="-----";
 for(int pass=0;pass<2;pass++){cJSON *id=pass?fallback:selected,*face;if(!cJSON_IsString(id))continue;
  cJSON_ArrayForEach(face,faces){cJSON *fid=cJSON_GetObjectItemCaseSensitive(face,"id");if(!cJSON_IsString(fid)||strcmp(fid->valuestring,id->valuestring))continue;
   cJSON *location=cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(face,"weather"),"location");
   if(cJSON_IsString(location))diagnostics_postal(location->valuestring,out);
  }if(strcmp(out,"-----"))break;
 }
 portENTER_CRITICAL(&postal_mux);memcpy(postal,out,6);portEXIT_CRITICAL(&postal_mux);
}
static esp_err_t opened(httpd_handle_t handle,int fd){int enabled=1;return setsockopt(fd,IPPROTO_TCP,TCP_NODELAY,&enabled,sizeof(enabled))==0?ESP_OK:ESP_FAIL;}
static void *json_alloc(size_t size){return heap_caps_malloc(size,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);}
static uint32_t crc32(const void *ptr,size_t size){const uint8_t *p=ptr;uint32_t crc=0xffffffff;for(size_t i=0;i<size;i++){crc^=p[i];for(int j=0;j<8;j++)crc=(crc>>1)^(0xedb88320U&-(crc&1U));}return ~crc;}
static void address(char *out,size_t size){esp_netif_ip_info_t ip={0};esp_netif_t *net=esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");if(net)esp_netif_get_ip_info(net,&ip);snprintf(out,size,IPSTR,IP2STR(&ip.ip));}
void dashboard_address(char *out,size_t size){address(out,size);}
static bool host_ok(httpd_req_t *req){char host[80],ip[32],with_port[40];address(ip,sizeof(ip));snprintf(with_port,sizeof(with_port),"%s:80",ip);return httpd_req_get_hdr_value_str(req,"Host",host,sizeof(host))==ESP_OK&&(!strcmp(host,ip)||!strcmp(host,with_port));}
static bool write_ok(httpd_req_t *req){char origin[100],wanted[100],host[80],tag[8];if(!host_ok(req)||httpd_req_get_hdr_value_str(req,"Host",host,sizeof(host))!=ESP_OK)return false;snprintf(wanted,sizeof(wanted),"http://%s",host);return httpd_req_get_hdr_value_str(req,"Origin",origin,sizeof(origin))==ESP_OK&&!strcmp(origin,wanted)&&httpd_req_get_hdr_value_str(req,"X-Face-Lab",tag,sizeof(tag))==ESP_OK&&!strcmp(tag,"1");}
static esp_err_t json_reply(httpd_req_t *req,const char *json){httpd_resp_set_type(req,"application/json");httpd_resp_set_hdr(req,"Cache-Control","no-store");httpd_resp_set_hdr(req,"X-Content-Type-Options","nosniff");size_t length=strlen(json);for(size_t at=0;at<length;at+=1024){size_t n=length-at;if(n>1024)n=1024;if(httpd_resp_send_chunk(req,json+at,n)!=ESP_OK)return ESP_FAIL;}return httpd_resp_send_chunk(req,NULL,0);}
static esp_err_t error(httpd_req_t *req,const char *status,const char *message){httpd_resp_set_status(req,status);return json_reply(req,message);}
static char *body(httpd_req_t *req,size_t max){if(req->content_len<1||req->content_len>max)return NULL;char *p=heap_caps_malloc(req->content_len+1,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);if(!p)return NULL;size_t at=0;while(at<req->content_len){int n=httpd_req_recv(req,p+at,req->content_len-at);if(n<=0){free(p);return NULL;}at+=n;}p[at]=0;return p;}
static bool valid_state(cJSON *state){
 cJSON *version=cJSON_GetObjectItemCaseSensitive(state,"version"),*faces=cJSON_GetObjectItemCaseSensitive(state,"faces"),*library=cJSON_GetObjectItemCaseSensitive(state,"library");
 if(!cJSON_IsObject(state)||!cJSON_IsNumber(version)||version->valuedouble!=2||!cJSON_IsArray(faces)||cJSON_GetArraySize(faces)<1||cJSON_GetArraySize(faces)>60||!cJSON_IsArray(library)||cJSON_GetArraySize(library)>128)return false;
 cJSON *f,*w;cJSON_ArrayForEach(f,faces){cJSON *id=cJSON_GetObjectItemCaseSensitive(f,"id"),*widgets=cJSON_GetObjectItemCaseSensitive(f,"widgets");if(!cJSON_IsString(id)||!cJSON_IsArray(widgets)||cJSON_GetArraySize(widgets)>32)return false;cJSON *other=faces->child;for(;other!=f;other=other->next){cJSON *other_id=cJSON_GetObjectItemCaseSensitive(other,"id");if(cJSON_IsString(other_id)&&!strcmp(id->valuestring,other_id->valuestring))return false;}cJSON_ArrayForEach(w,widgets)if(!cJSON_IsObject(w)||!cJSON_IsString(cJSON_GetObjectItemCaseSensitive(w,"type")))return false;}
 cJSON_ArrayForEach(w,library)if(!cJSON_IsObject(w)||!cJSON_IsString(cJSON_GetObjectItemCaseSensitive(w,"type")))return false;
 return true;
}
static esp_err_t save_workspace(void){
 if(!workspace||revision==saved_revision)return ESP_OK;
 int next=slot==0?1:0;record_t h={MAGIC,revision,workspace_size,crc32(workspace,workspace_size)};
 size_t erased=(sizeof(h)+workspace_size+4095)&~(size_t)4095;
 esp_err_t err=esp_partition_erase_range(partition,next*SLOT_SIZE,erased);
 if(err==ESP_OK)err=esp_partition_write(partition,next*SLOT_SIZE+sizeof(h),workspace,workspace_size);
 if(err==ESP_OK)err=esp_partition_write(partition,next*SLOT_SIZE,&h,sizeof(h));
 if(err==ESP_OK){slot=next;saved_revision=revision;}return err;
}
static void saver(void *arg){
 for(;;){vTaskDelay(pdMS_TO_TICKS(250));xSemaphoreTake(mutex,portMAX_DELAY);
  if(dirty_at&&esp_timer_get_time()-dirty_at>=2000000){
   save_error=save_workspace();if(save_error==ESP_OK&&scene_dirty)save_error=scene_store_save_live();
   if(save_error==ESP_OK){dirty_at=0;scene_dirty=false;}else dirty_at=esp_timer_get_time();
  }
  xSemaphoreGive(mutex);
 }
}
static esp_err_t get(httpd_req_t *req){
 if(!host_ok(req))return error(req,"403 Forbidden","{\"error\":\"Use the clock's own network address\"}");
 if(!strcmp(req->uri,"/api/capabilities"))return json_reply(req,"{\"ai\":false,\"sceneVersion\":2,\"boardHosted\":true,\"workspaceLimit\":1048576}");
 if(!strcmp(req->uri,"/api/workspace")){xSemaphoreTake(mutex,portMAX_DELAY);esp_err_t err=json_reply(req,workspace?workspace:"{\"revision\":0,\"state\":null}");xSemaphoreGive(mutex);return err;}
 if(!strcmp(req->uri,"/api/status")){
  xSemaphoreTake(mutex,portMAX_DELAY);uint32_t live,saved;scene_store_crc(&live,&saved);char json[240];snprintf(json,sizeof(json),"{\"revision\":%u,\"savedRevision\":%u,\"sceneCrc\":%u,\"savedSceneCrc\":%u,\"pending\":%s,\"saveError\":%d}",(unsigned)revision,(unsigned)saved_revision,(unsigned)live,(unsigned)saved,dirty_at?"true":"false",save_error);xSemaphoreGive(mutex);return json_reply(req,json);
 }
 if(!strncmp(req->uri,"/api/weather/current",20)){char *json=board_weather_json();if(!json)return error(req,"503 Service Unavailable","{\"error\":\"Waiting for current weather\"}");esp_err_t err=json_reply(req,json);free(json);return err;}
 if(!strncmp(req->uri,"/api/",5))return error(req,"404 Not Found","{\"error\":\"Unknown route\"}");
 const char *path=!strcmp(req->uri,"/")?"/index.html":req->uri;
 for(unsigned i=0;i<sizeof(dashboard_assets)/sizeof(dashboard_assets[0]);i++)if(!strcmp(path,dashboard_assets[i].path)){
  const dashboard_asset_t *a=&dashboard_assets[i];httpd_resp_set_type(req,a->type);httpd_resp_set_hdr(req,"Content-Encoding","gzip");httpd_resp_set_hdr(req,"Cache-Control","no-store");httpd_resp_set_hdr(req,"X-Content-Type-Options","nosniff");
  for(size_t at=0;at<a->size;at+=2048){size_t n=a->size-at;if(n>2048)n=2048;if(httpd_resp_send_chunk(req,(const char *)a->data+at,n)!=ESP_OK)return ESP_FAIL;}return httpd_resp_send_chunk(req,NULL,0);
 }
 return httpd_resp_send_err(req,HTTPD_404_NOT_FOUND,"Not found");
}
static esp_err_t post(httpd_req_t *req){
 if(!write_ok(req))return error(req,"403 Forbidden","{\"error\":\"Open this clock's dashboard to make changes\"}");
 bool is_scene=!strcmp(req->uri,"/api/scene"),is_workspace=!strcmp(req->uri,"/api/workspace");
 if(!is_scene&&!is_workspace)return error(req,"404 Not Found","{\"error\":\"Unknown route\"}");
 char *p=body(req,is_scene?262144:WORKSPACE_LIMIT);if(!p)return error(req,"400 Bad Request","{\"error\":\"Invalid body or size limit exceeded\"}");
 if(is_scene){esp_err_t result=scene_store_live((const uint8_t *)p,req->content_len);free(p);if(result!=ESP_OK)return error(req,"400 Bad Request","{\"error\":\"Invalid scene or display busy\"}");xSemaphoreTake(mutex,portMAX_DELAY);scene_dirty=true;dirty_at=esp_timer_get_time();xSemaphoreGive(mutex);return json_reply(req,"{\"live\":true}");}
 cJSON *root=cJSON_ParseWithLength(p,req->content_len+1);free(p);cJSON *state=cJSON_GetObjectItemCaseSensitive(root,"state"),*rev=cJSON_GetObjectItemCaseSensitive(root,"revision");
 if(!valid_state(state)||!cJSON_IsNumber(rev)){cJSON_Delete(root);return error(req,"400 Bad Request","{\"error\":\"Invalid workspace\"}");}
 xSemaphoreTake(mutex,portMAX_DELAY);
 if(rev->valuedouble!=revision){xSemaphoreGive(mutex);cJSON_Delete(root);return error(req,"409 Conflict","{\"error\":\"Another browser saved changes. Reload before editing.\"}");}
 cJSON *envelope=cJSON_CreateObject();cJSON_AddNumberToObject(envelope,"revision",revision+1);cJSON_AddNumberToObject(envelope,"savedAt",time(NULL));cJSON_AddItemToObject(envelope,"state",cJSON_DetachItemFromObjectCaseSensitive(root,"state"));char *raw=cJSON_PrintUnformatted(envelope);
 if(!raw||strlen(raw)>WORKSPACE_LIMIT){cJSON_Delete(envelope);cJSON_Delete(root);free(raw);xSemaphoreGive(mutex);return error(req,"413 Content Too Large","{\"error\":\"Clock library limit is 1 MB. Export a backup and remove unused variations.\"}");}
 update_postal(envelope);cJSON_Delete(envelope);cJSON_Delete(root);
 free(workspace);workspace=raw;workspace_size=strlen(raw);revision++;dirty_at=esp_timer_get_time();save_error=ESP_OK;
 char response[64];snprintf(response,sizeof(response),"{\"revision\":%u}",(unsigned)revision);xSemaphoreGive(mutex);return json_reply(req,response);
}
esp_err_t dashboard_init(void){
 cJSON_Hooks hooks={.malloc_fn=json_alloc,.free_fn=free};cJSON_InitHooks(&hooks);
 mutex=xSemaphoreCreateMutex();if(!mutex)return ESP_ERR_NO_MEM;
 partition=esp_partition_find_first(ESP_PARTITION_TYPE_DATA,ESP_PARTITION_SUBTYPE_ANY,"workspace");if(!partition||partition->size<2*SLOT_SIZE)return ESP_ERR_NOT_FOUND;
 for(int i=0;i<2;i++){record_t h;if(esp_partition_read(partition,i*SLOT_SIZE,&h,sizeof(h))!=ESP_OK||h.magic!=MAGIC||h.size<1||h.size>WORKSPACE_LIMIT||h.revision<=revision)continue;
  char *candidate=heap_caps_malloc(h.size+1,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);if(!candidate)continue;
  if(esp_partition_read(partition,i*SLOT_SIZE+sizeof(h),candidate,h.size)==ESP_OK&&crc32(candidate,h.size)==h.crc){candidate[h.size]=0;cJSON *root=cJSON_ParseWithLength(candidate,h.size+1);bool valid=valid_state(cJSON_GetObjectItemCaseSensitive(root,"state"));cJSON *r=cJSON_GetObjectItemCaseSensitive(root,"revision");valid=valid&&cJSON_IsNumber(r)&&r->valuedouble==h.revision;if(valid)update_postal(root);cJSON_Delete(root);if(valid){free(workspace);workspace=candidate;workspace_size=h.size;revision=saved_revision=h.revision;slot=i;continue;}}
  free(candidate);
 }
 // Flash operations disable the PSRAM cache; their task stack must be internal.
 if(xTaskCreatePinnedToCore(saver,"dashboard-save",4096,NULL,1,NULL,1)!=pdPASS)return ESP_ERR_NO_MEM;
 httpd_config_t cfg=HTTPD_DEFAULT_CONFIG();cfg.stack_size=8192;cfg.task_caps=MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT;cfg.core_id=1;cfg.max_uri_handlers=4;cfg.max_open_sockets=3;cfg.lru_purge_enable=true;cfg.uri_match_fn=httpd_uri_match_wildcard;cfg.recv_wait_timeout=10;cfg.send_wait_timeout=10;cfg.open_fn=opened;
 esp_err_t err=httpd_start(&server,&cfg);if(err!=ESP_OK)return err;
 httpd_uri_t read={.uri="/*",.method=HTTP_GET,.handler=get},write={.uri="/*",.method=HTTP_POST,.handler=post};
 ESP_ERROR_CHECK(httpd_register_uri_handler(server,&read));ESP_ERROR_CHECK(httpd_register_uri_handler(server,&write));return ESP_OK;
}
void dashboard_status(void){char ip[32];address(ip,sizeof(ip));printf("WEB url=http://%s/ ready=%d revision=%u saved=%u\n",ip,server!=NULL,(unsigned)revision,(unsigned)saved_revision);}
