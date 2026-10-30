#include "weather.h"
#include "network.h"
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <inttypes.h>
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "mbedtls/platform.h"
#include "esp_timer.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/idf_additions.h"
#include "esp_heap_caps.h"
static portMUX_TYPE mux=portMUX_INITIALIZER_UNLOCKED;
static float latitude,longitude;
static bool enabled;
static unsigned revision;
static atomic_int current,override;
static atomic_uint syncs,strikes,natural_strikes;
static TaskHandle_t weather_task;
static atomic_int last_result;
static atomic_uint attempts;
static int64_t last_sync,flash_until;
static solar_day_t solar_days[4];
static unsigned solar_count;
static int64_t sky_override;
bool board_weather_sky(double epoch,solar_phase_t *phase){
 solar_day_t days[4];portENTER_CRITICAL(&mux);unsigned count=solar_count;int64_t forced=sky_override;memcpy(days,solar_days,sizeof(days));portEXIT_CRITICAL(&mux);
 return solar_phase(days,count,forced?forced:epoch,phase);
}
static const char *names[]={"unknown","sunny","partly-cloudy","cloudy","rainy","storm","snowy"};
int board_weather_condition(void){int forced=atomic_load(&override);return forced?forced:atomic_load(&current);}
int64_t board_weather_flash_until(void){portENTER_CRITICAL(&mux);int64_t t=flash_until;portEXIT_CRITICAL(&mux);return t;}
void board_weather_configure(float lat,float lon,bool active){portENTER_CRITICAL(&mux);if(latitude!=lat||longitude!=lon||enabled!=active){latitude=lat;longitude=lon;enabled=active;revision++;solar_count=0;atomic_store(&current,0);}portEXIT_CRITICAL(&mux);}
static int classify(int code){if(code==0)return 1;if(code==1||code==2)return 2;if(code==3||code==45||code==48)return 3;if(code==71||code==73||code==75||code==77||code==85||code==86)return 6;if(code>=95)return 5;return 4;}
static void worker(void *arg){
 unsigned seen=~0U;int64_t next=0;
 for(;;){
  portENTER_CRITICAL(&mux);float lat=latitude,lon=longitude;bool active=enabled;unsigned rev=revision;portEXIT_CRITICAL(&mux);
  if(rev!=seen){next=0;seen=rev;}
  int64_t now=esp_timer_get_time();
  if(active&&!strcmp(board_network_status(),"connected")&&now>=next){
   atomic_fetch_add(&attempts,1);atomic_store(&last_result,-1);
   next=now+30000000;char url[320];snprintf(url,sizeof(url),"https://api.open-meteo.com/v1/forecast?latitude=%.5f&longitude=%.5f&current=weather_code&daily=sunrise,sunset&timezone=auto&timeformat=unixtime&past_days=1&forecast_days=3",lat,lon);
   esp_http_client_config_t cfg={.url=url,.crt_bundle_attach=esp_crt_bundle_attach,.timeout_ms=30000,.buffer_size=1024,.addr_type=HTTP_ADDR_TYPE_INET};
   esp_http_client_handle_t client=esp_http_client_init(&cfg);
   if(client){atomic_store(&last_result,-2);char *body=heap_caps_calloc(1,8193,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);int total=0;bool ok=false;
    esp_err_t opened=body?esp_http_client_open(client,0):ESP_ERR_NO_MEM;atomic_store(&last_result,opened);
    if(body&&opened==ESP_OK){esp_http_client_fetch_headers(client);atomic_store(&last_result,esp_http_client_get_status_code(client));if(esp_http_client_get_status_code(client)==200){
     int n;while(total<8192&&(n=esp_http_client_read(client,body+total,8192-total))>0)total+=n;body[total]=0;
     cJSON *json=cJSON_Parse(body),*item=cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(json,"current"),"weather_code");
     cJSON *daily=cJSON_GetObjectItemCaseSensitive(json,"daily"),*rises=cJSON_GetObjectItemCaseSensitive(daily,"sunrise"),*sets=cJSON_GetObjectItemCaseSensitive(daily,"sunset");
     solar_day_t days[4]={0};unsigned count=0;
     for(int i=0;i<4;i++){cJSON *rise=cJSON_GetArrayItem(rises,i),*setting=cJSON_GetArrayItem(sets,i);if(cJSON_IsNumber(rise)&&cJSON_IsNumber(setting)&&isfinite(rise->valuedouble)&&isfinite(setting->valuedouble)&&rise->valuedouble>0&&setting->valuedouble>rise->valuedouble&&setting->valuedouble<4102444800.0)days[count++]=(solar_day_t){(int64_t)rise->valuedouble,(int64_t)setting->valuedouble};}
     if(cJSON_IsNumber(item)){portENTER_CRITICAL(&mux);if(revision==rev){atomic_store(&current,classify(item->valueint));if(count){memcpy(solar_days,days,sizeof(days));solar_count=count;}last_sync=esp_timer_get_time();atomic_fetch_add(&syncs,1);ok=true;}portEXIT_CRITICAL(&mux);}cJSON_Delete(json);
    }}
    esp_http_client_cleanup(client);free(body);if(ok)next=esp_timer_get_time()+600000000;
   }
  }
  vTaskDelay(pdMS_TO_TICKS(1000));
 }
}
void board_weather_note_strike(bool forced){atomic_fetch_add(&strikes,1);if(!forced)atomic_fetch_add(&natural_strikes,1);}
static void *tls_calloc(size_t n,size_t size){return heap_caps_calloc(n,size,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);}
void board_weather_init(void){
 // TLS record buffers exceed available internal RAM after the HUB75 DMA driver.
 mbedtls_platform_set_calloc_free(tls_calloc,free);
if(xTaskCreatePinnedToCoreWithCaps(worker,"weather",8192,NULL,2,&weather_task,1,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT)!=pdPASS)puts("WEATHER ERROR task memory");}
void board_weather_command(const char *arg){
 if(arg&&!strncmp(arg,"skytime ",8)){int64_t epoch=0;char *end=NULL;if(strcmp(arg+8,"auto")){epoch=strtoll(arg+8,&end,10);if(!end||*end||epoch<1577836800LL||epoch>4102444800LL){puts("SOLAR ERROR time");return;}}portENTER_CRITICAL(&mux);sky_override=epoch;portEXIT_CRITICAL(&mux);printf("SOLAR TIME %" PRId64 "\n",epoch);return;}
 if(arg&&!strcmp(arg,"sky")){
  solar_day_t days[4];portENTER_CRITICAL(&mux);unsigned count=solar_count;int64_t forced=sky_override;memcpy(days,solar_days,sizeof(days));portEXIT_CRITICAL(&mux);
  double epoch=forced?forced:time(NULL);solar_phase_t phase={0};bool known=solar_phase(days,count,epoch,&phase);
  printf("SOLAR {\"time\":%.0f,\"override\":%d,\"body\":\"%s\",\"progress\":%.8f,\"start\":%" PRId64 ",\"end\":%" PRId64 ",\"days\":[",epoch,forced!=0,known?(phase.sun?"sun":"moon"):"unknown",phase.progress,phase.start,phase.end);
  for(unsigned i=0;i<count;i++)printf("%s{\"sunrise\":%" PRId64 ",\"sunset\":%" PRId64 "}",i?",":"",days[i].sunrise,days[i].sunset);
  puts("]}");return;
 }
 if(arg&&!strcmp(arg,"auto")){atomic_store(&override,0);puts("WEATHER AUTO");return;}
 if(arg&&!strcmp(arg,"strike")){portENTER_CRITICAL(&mux);flash_until=esp_timer_get_time()+120000;portEXIT_CRITICAL(&mux);puts("WEATHER STRIKE");return;}
 if(arg&&!strncmp(arg,"test ",5)){for(int i=1;i<=6;i++)if(!strcmp(arg+5,names[i])){atomic_store(&override,i);printf("WEATHER TEST %s\n",names[i]);return;}puts("WEATHER ERROR condition");return;}
 portENTER_CRITICAL(&mux);int64_t sync=last_sync;bool active=enabled;portEXIT_CRITICAL(&mux);
 printf("WEATHER STATUS enabled=%d condition=%s override=%d syncs=%u age_s=%lld interval_s=600 strikes=%u natural_strikes=%u task=%d http=%d attempts=%u state=%d internal_free=%u internal_largest=%u\n",active,names[board_weather_condition()],atomic_load(&override),atomic_load(&syncs),sync?(esp_timer_get_time()-sync)/1000000:-1LL,atomic_load(&strikes),atomic_load(&natural_strikes),weather_task!=NULL,atomic_load(&last_result),atomic_load(&attempts),weather_task?(int)eTaskGetState(weather_task):-1,(unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT),(unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT));
}
