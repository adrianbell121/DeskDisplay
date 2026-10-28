#include "network.h"
#include "board.h"
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "esp_sntp.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "nvs.h"
#include "nvs_flash.h"

#define APPLY_CONFIG 1U
#define CONNECT 2U
#define GOT_IP 4U
#define DISCONNECTED 8U
#define SYNC_INTERVAL_MS 3600000U
typedef struct { uint8_t ssid_len, password_len; uint8_t ssid[32], password[64]; } credentials_t;
enum { UNCONFIGURED, CONNECTING, ONLINE, FAILED };
static TaskHandle_t worker;
static QueueHandle_t synced;
static atomic_uint phase, last_sync, sync_count, disconnect_reason;
static credentials_t pending;
static bool staging;
static int64_t staging_at;

const char *board_network_status(void)
{ static const char *states[]={"unconfigured","connecting","connected","error"}; return states[atomic_load(&phase)]; }
uint32_t board_network_last_sync(void) { return atomic_load(&last_sync); }
uint32_t board_network_sync_count(void) { return atomic_load(&sync_count); }
unsigned board_network_disconnect_reason(void) { return atomic_load(&disconnect_reason); }
unsigned board_network_sync_interval(void) { return esp_sntp_get_sync_interval() / 1000; }
static bool valid_credentials(const credentials_t *c)
{
    if(!c->ssid_len||c->ssid_len>32||c->password_len>63||(c->password_len&&c->password_len<8))return false;
    for(unsigned i=0;i<c->ssid_len;i++)if(!c->ssid[i])return false;
    for(unsigned i=0;i<c->password_len;i++)if(c->password[i]<32||c->password[i]>126)return false;
    return true;
}
static esp_err_t load_credentials(credentials_t *c)
{
    nvs_handle_t h;esp_err_t err=nvs_open("network",NVS_READONLY,&h);if(err!=ESP_OK)return err;
    size_t len=sizeof(*c);err=nvs_get_blob(h,"wifi",c,&len);nvs_close(h);
    return err==ESP_OK&&(len!=sizeof(*c)||!valid_credentials(c))?ESP_ERR_INVALID_ARG:err;
}
static void on_sync(struct timeval *tv)
{
    // The TCP/IP task only posts an event. RTC I2C writes run in our worker.
    time_t utc=tv->tv_sec;
    if(utc>=1704067200LL&&utc<=4102444799LL)xQueueOverwrite(synced,&utc);
}
static void on_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;
    if(base==WIFI_EVENT&&id==WIFI_EVENT_STA_START)xTaskNotify(worker,CONNECT,eSetBits);
    if(base==WIFI_EVENT&&id==WIFI_EVENT_STA_DISCONNECTED){
        atomic_store(&phase,CONNECTING);
        atomic_store(&disconnect_reason,((wifi_event_sta_disconnected_t *)data)->reason);
        xTaskNotify(worker,DISCONNECTED,eSetBits);
    }
    if(base==IP_EVENT&&id==IP_EVENT_STA_GOT_IP){
        atomic_store(&phase,ONLINE);atomic_store(&disconnect_reason,0);
        xTaskNotify(worker,GOT_IP,eSetBits);
    }
}
static void network_worker(void *arg)
{
    (void)arg;bool started=false, retry=false;unsigned backoff=1;int64_t next_attempt=0;
    for(;;){
        uint32_t events=0;xTaskNotifyWait(0,UINT32_MAX,&events,pdMS_TO_TICKS(100));
        if(events&APPLY_CONFIG){
            credentials_t c={0};esp_err_t err=load_credentials(&c);
            if(err==ESP_OK){
                if(started){esp_wifi_stop();started=false;}
                wifi_config_t cfg={0};memcpy(cfg.sta.ssid,c.ssid,c.ssid_len);memcpy(cfg.sta.password,c.password,c.password_len);
                cfg.sta.threshold.authmode=c.password_len?WIFI_AUTH_WPA2_PSK:WIFI_AUTH_OPEN;
                cfg.sta.sae_pwe_h2e=WPA3_SAE_PWE_BOTH;
                err=esp_wifi_set_config(WIFI_IF_STA,&cfg);
                memset(&c,0,sizeof(c));memset(&cfg,0,sizeof(cfg));
                if(err==ESP_OK)err=esp_wifi_start();
                if(err==ESP_OK){started=true;atomic_store(&phase,CONNECTING);backoff=1;}
                else{atomic_store(&phase,FAILED);ESP_LOGW("network","Wi-Fi setup: %s",esp_err_to_name(err));}
            }else if(err==ESP_ERR_NVS_NOT_FOUND)atomic_store(&phase,UNCONFIGURED);
            else {atomic_store(&phase,FAILED);ESP_LOGW("network","Saved Wi-Fi configuration unavailable");}
        }
        if(events&CONNECT){retry=true;next_attempt=0;}
        if(events&DISCONNECTED){retry=true;next_attempt=esp_timer_get_time()+backoff*1000000LL;if(backoff<60)backoff=backoff*2>60?60:backoff*2;}
        if(events&GOT_IP){
            retry=false;backoff=1;
            esp_err_t err=esp_netif_sntp_start();
            ESP_LOGI("network","Wi-Fi connected; internet time sync %s (hourly)",esp_err_to_name(err));
        }
        if(started&&retry&&atomic_load(&phase)!=ONLINE&&esp_timer_get_time()>=next_attempt){
            esp_err_t err=esp_wifi_connect();next_attempt=esp_timer_get_time()+60000000;
            if(err!=ESP_OK)next_attempt=esp_timer_get_time()+5000000;
        }
        time_t utc;
        if(xQueueReceive(synced,&utc,0)==pdTRUE){
            // SNTP already set the precise system clock. Only copy current UTC to RTC.
            utc=time(NULL);esp_err_t err=board_clock_update_rtc(utc);
            atomic_store(&last_sync,(unsigned)utc);atomic_fetch_add(&sync_count,1);
            struct tm local;localtime_r(&utc,&local);char text[48];strftime(text,sizeof(text),"%Y-%m-%d %H:%M:%S %Z",&local);
            ESP_LOGI("network","NTP synced: %s; RTC %s",text,esp_err_to_name(err));
        }
    }
}
esp_err_t board_network_init(void)
{
    // Never erase NVS automatically: credentials and other saved settings share it.
    esp_err_t err=nvs_flash_init();if(err!=ESP_OK)return err;
    synced=xQueueCreate(1,sizeof(time_t));if(!synced)return ESP_ERR_NO_MEM;
    if((err=esp_netif_init())!=ESP_OK)return err;
    if((err=esp_event_loop_create_default())!=ESP_OK)return err;
    if(!esp_netif_create_default_wifi_sta())return ESP_FAIL;
    wifi_init_config_t cfg=WIFI_INIT_CONFIG_DEFAULT();
    if((err=esp_wifi_init(&cfg))!=ESP_OK)return err;
    // Our single NVS blob stores credentials; avoid a second driver-managed copy.
    if((err=esp_wifi_set_storage(WIFI_STORAGE_RAM))!=ESP_OK)return err;
    if((err=esp_wifi_set_mode(WIFI_MODE_STA))!=ESP_OK)return err;
    if((err=esp_event_handler_register(WIFI_EVENT,ESP_EVENT_ANY_ID,on_event,NULL))!=ESP_OK)return err;
    if((err=esp_event_handler_register(IP_EVENT,IP_EVENT_STA_GOT_IP,on_event,NULL))!=ESP_OK)return err;
    esp_sntp_set_sync_interval(SYNC_INTERVAL_MS);
    esp_sntp_config_t sntp=ESP_NETIF_SNTP_DEFAULT_CONFIG_MULTIPLE(3,ESP_SNTP_SERVER_LIST("0.pool.ntp.org","1.pool.ntp.org","2.pool.ntp.org"));
    sntp.start=false;sntp.wait_for_sync=false;sntp.sync_cb=on_sync;
    if((err=esp_netif_sntp_init(&sntp))!=ESP_OK)return err;
    if(xTaskCreate(network_worker,"network",6144,NULL,3,&worker)!=pdPASS)return ESP_ERR_NO_MEM;
    xTaskNotify(worker,APPLY_CONFIG,eSetBits);
    return ESP_OK;
}
static int nibble(char c)
{return c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:-1;}
static int decode(const char *s,uint8_t *out,size_t capacity)
{
    size_t n=strlen(s);if((n&1)||n/2>capacity)return -1;
    for(size_t i=0;i<n;i++)if(nibble(s[i])<0)return -1;
    for(size_t i=0;i<n;i+=2)out[i/2]=(nibble(s[i])<<4)|nibble(s[i+1]);
    return n/2;
}
void board_network_command(const char *arg)
{
    if(!worker){puts("WIFI ERROR network service unavailable");return;}
    if(!arg||!strcmp(arg,"status")){printf("WIFI STATUS state=%s last_sync=%u syncs=%u reason=%u interval_s=3600\n",board_network_status(),(unsigned)board_network_last_sync(),(unsigned)board_network_sync_count(),board_network_disconnect_reason());return;}
    if(!strcmp(arg,"begin")){memset(&pending,0,sizeof(pending));staging=true;staging_at=esp_timer_get_time();puts("WIFI READY");return;}
    if(!strcmp(arg,"abort")){memset(&pending,0,sizeof(pending));staging=false;puts("WIFI ABORTED");return;}
    if(!staging||esp_timer_get_time()-staging_at>60000000){memset(&pending,0,sizeof(pending));staging=false;puts("WIFI ERROR begin configuration first");return;}
    staging_at=esp_timer_get_time();
    if(!strncmp(arg,"ssid ",5)){int n=decode(arg+5,pending.ssid,32);if(n<=0){puts("WIFI ERROR invalid SSID bytes");return;}pending.ssid_len=n;puts("WIFI SSID OK");return;}
    if(!strncmp(arg,"pass ",5)){
        char *end;unsigned long offset=strtoul(arg+5,&end,10);
        if(end==arg+5||*end!=' '||offset!=pending.password_len){puts("WIFI ERROR invalid password offset");return;}
        int n=decode(end+1,pending.password+pending.password_len,63-pending.password_len);
        if(n<=0){puts("WIFI ERROR invalid password bytes");return;}pending.password_len+=n;puts("WIFI PASS OK");return;
    }
    if(!strcmp(arg,"commit")){
        if(!valid_credentials(&pending)){puts("WIFI ERROR SSID 1..32 bytes; password empty or 8..63 printable ASCII bytes");return;}
        nvs_handle_t h;esp_err_t err=nvs_open("network",NVS_READWRITE,&h);
        if(err==ESP_OK){err=nvs_set_blob(h,"wifi",&pending,sizeof(pending));if(err==ESP_OK)err=nvs_commit(h);nvs_close(h);}
        memset(&pending,0,sizeof(pending));staging=false;
        if(err==ESP_OK){xTaskNotify(worker,APPLY_CONFIG,eSetBits);puts("WIFI OK saved=1 connecting=1");}
        else printf("WIFI ERROR saving %s\n",esp_err_to_name(err));
        return;
    }
    puts("WIFI ERROR invalid operation");
}
