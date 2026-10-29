#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "board.h"
#include "network.h"
#include "weather.h"
#include "desk_ui.h"
#include "scene_store.h"
#include "scene_player.h"
#include "driver/gpio.h"
#include "driver/usb_serial_jtag.h"
#include "driver/usb_serial_jtag_vfs.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static unsigned brightness = 25;
static uint16_t *capture;
static void status(void)
{
    time_t utc_now=time(NULL);struct tm local;localtime_r(&utc_now,&local);
    char local_text[40],zone[12],offset[12];
    strftime(local_text,sizeof(local_text),"%Y-%m-%dT%H:%M:%S",&local);
    strftime(zone,sizeof(zone),"%Z",&local);strftime(offset,sizeof(offset),"%z",&local);
    const char *mode = "busy";
    desk_ui_state_t ui = {.minute = -1, .birds = "inactive", .birds_next_ms = -1};
    if (lvgl_port_lock(100)) { mode = desk_ui_mode_name(); desk_ui_get_state(&ui); lvgl_port_unlock(); }
    printf("STATUS version=0.1.0 panel=64x64 mode=%s brightness=%u clock=%s audio=%s frames=%" PRIu32
           " heap=%u min_heap=%u uptime=%" PRIi64 " utc=%" PRIi64
           " ui_minute=%d birds=%s birds_next_ms=%d scene_version=2 weather_version=1"
           " wifi=%s ntp_last=%" PRIu32 " ntp_syncs=%" PRIu32 " ntp_interval_s=%u timezone=America/New_York local=%s zone=%s utc_offset=%s\n",
           mode, brightness, board_clock_valid() ? "set" : "unset", esp_err_to_name(board_audio_status()),
           board_display_frames(), (unsigned)esp_get_free_heap_size(), (unsigned)esp_get_minimum_free_heap_size(),
           esp_timer_get_time() / 1000000, (int64_t)time(NULL), ui.minute, ui.birds, ui.birds_next_ms,
           board_network_status(), board_network_last_sync(), board_network_sync_count(), board_network_sync_interval(), local_text, zone, offset);
    fflush(stdout);
}
static bool number(const char *text, int64_t *out)
{
    errno = 0;
    char *end;
    long long n = strtoll(text, &end, 10);
    if (errno || !text[0] || *end) return false;
    *out = n;
    return true;
}
static void timezone_test(void)
{
    static const struct {time_t utc;const char *expected;} cases[]={
        {1772953199,"2026-03-08T01:59:59 EST -0500"},
        {1772953200,"2026-03-08T03:00:00 EDT -0400"},
        {1793512799,"2026-11-01T01:59:59 EDT -0400"},
        {1793512800,"2026-11-01T01:00:00 EST -0500"},
    };
    bool ok=true;
    for(unsigned i=0;i<sizeof(cases)/sizeof(cases[0]);i++){
        struct tm t;char out[64];localtime_r(&cases[i].utc,&t);strftime(out,sizeof(out),"%Y-%m-%dT%H:%M:%S %Z %z",&t);
        bool match=!strcmp(out,cases[i].expected);printf("TZCASE %u %s %s\n",i,out,match?"OK":"FAIL");ok=ok&&match;
    }
    puts(ok?"TZTEST PASS spring_and_fall":"TZTEST FAIL");
}
static void command(char *line)
{
    char *arg = strchr(line, ' ');
    if (arg) *arg++ = '\0';
    int64_t value;
    if (!strcmp(line, "help")) {
        puts("COMMANDS status | time <UTC epoch> | mode clock|island|custom | wifi status|begin|ssid|pass|commit|abort | scene begin|data|commit|abort | brightness 0..100 | sound [0..85] | test | capture");
    } else if (!strcmp(line, "status")) status();
    else if (!strcmp(line, "islandcompare")) {
        time_t now=time(NULL);struct tm t;localtime_r(&now,&t);
        if(lvgl_port_lock(1000)) {
            unsigned differences=scene_player_compare_island(&t,board_clock_valid(),esp_timer_get_time()/1000);
            lvgl_port_unlock();printf("ISLANDCOMPARE pixels_different=%u\n",differences);
        } else puts("ERROR display busy");
    }
    else if (!strcmp(line, "tztest")) timezone_test();
    else if (!strcmp(line, "restart")) {puts("RESTARTING");fflush(stdout);vTaskDelay(pdMS_TO_TICKS(100));esp_restart();}
    else if (!strcmp(line, "scene")) scene_store_command(arg);
    else if (!strcmp(line, "weather")) board_weather_command(arg);
    else if (!strcmp(line, "wifi")) board_network_command(arg);
    else if (!strcmp(line, "time") && arg && number(arg, &value)) {
        esp_err_t err = board_clock_set((time_t)value);
        if (err == ESP_ERR_INVALID_ARG) puts("ERROR time must be a UTC epoch in 2024..2099");
        else printf("TIME SET utc=%" PRIi64 " rtc=%s\n", value, esp_err_to_name(err));
    } else if (!strcmp(line, "mode") && arg) {
        if (lvgl_port_lock(1000)) {
            bool ok = desk_ui_mode(arg);
            lvgl_port_unlock();
            puts(ok ? "MODE OK" : "ERROR use clock, island, or an uploaded custom face");
        } else puts("ERROR display busy");
    } else if (!strcmp(line, "brightness") && arg && number(arg, &value) && value >= 0 && value <= 100) {
        if (lvgl_port_lock(1000)) {
            brightness = value;
            board_display_brightness(brightness);
            lvgl_port_unlock();
            puts("BRIGHTNESS OK");
        } else puts("ERROR display busy");
    } else if (!strcmp(line, "sound")) {
        value = 65;
        if (arg && (!number(arg, &value) || value < 0 || value > 85)) puts("ERROR sound volume must be 0..85");
        else printf("SOUND QUEUED %s\n", esp_err_to_name(board_audio_test((unsigned)value)));
    }
    else if (!strcmp(line, "test")) {
        if (lvgl_port_lock(1000)) { desk_ui_test(); lvgl_port_unlock(); puts("TEST 8 seconds"); }
        else puts("ERROR display busy");
    } else if (!strcmp(line, "capture")) {
        if(!capture)capture=heap_caps_malloc(64*64*sizeof(*capture),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
        if(!capture){puts("ERROR capture memory");return;}
        bool ok = false;
        if (lvgl_port_lock(1000)) { ok = board_display_snapshot(capture); lvgl_port_unlock(); }
        if (ok) {
            bool compressed=arg&&!strcmp(arg,"rle");
            puts(compressed?"FRAME RLE 64 64 RGB565":"FRAME BEGIN 64 64 RGB565");
            for (int y = 0; y < 64; y++) {
                char row[64 * 6 + 1];
                if(compressed){int at=0;for(int x=0;x<64;){int count=1;while(x+count<64&&capture[y*64+x+count]==capture[y*64+x])count++;snprintf(row+at,7,"%02x%04x",(unsigned)(count&255),(unsigned)capture[y*64+x]);at+=6;x+=count;}}
                else for (int x = 0; x < 64; x++) snprintf(row + x * 4, 5, "%04x", capture[y * 64 + x]);
                puts(row);
            }
            puts("FRAME END");
        } else puts("ERROR no rendered frame");
    } else puts("ERROR invalid command; send help");
    fflush(stdout);
}
void app_main(void)
{
    ESP_LOGI("deskdisplay", "DeskDisplay 0.1.0; reset reason=%d", esp_reset_reason());
    ESP_ERROR_CHECK(board_display_init());
    esp_err_t clock_err = board_clock_init();
    ESP_LOGI("deskdisplay", "RTC: %s", esp_err_to_name(clock_err));
    ESP_ERROR_CHECK(lvgl_port_lock(2000) ? ESP_OK : ESP_ERR_TIMEOUT);
    esp_err_t scene_err = scene_store_init();
    if (scene_err != ESP_OK) ESP_LOGW("deskdisplay", "Scene storage: %s", esp_err_to_name(scene_err));
    ESP_ERROR_CHECK(desk_ui_init());
    lvgl_port_unlock();
    esp_err_t audio_err = board_audio_start();
    if (audio_err != ESP_OK) ESP_LOGW("deskdisplay", "Speaker task: %s", esp_err_to_name(audio_err));
    gpio_config_t button = {.pin_bit_mask = 1ULL << BOARD_BUTTON, .mode = GPIO_MODE_INPUT,
                            .pull_up_en = GPIO_PULLUP_ENABLE};
    ESP_ERROR_CHECK(gpio_config(&button));
    usb_serial_jtag_driver_config_t usb = {.tx_buffer_size = 2048, .rx_buffer_size = 512};
    ESP_ERROR_CHECK(usb_serial_jtag_driver_install(&usb));
    usb_serial_jtag_vfs_use_driver();
    esp_err_t network_err = board_network_init();
    if (network_err != ESP_OK) ESP_LOGW("deskdisplay", "Network service: %s", esp_err_to_name(network_err));
    board_weather_init();
    puts("DESKDISPLAY READY; short BOOT: next mode; hold BOOT 1 second: speaker test; send help");
    fflush(stdout);
    char line[128];
    size_t used = 0;
    bool overflow = false, pressed = false, long_press = false;
    int last_raw = 1, stable = 1;
    int64_t changed_at = 0, pressed_at = 0, heartbeat = 0;
    for (;;) {
        uint8_t data[64];
        int n = usb_serial_jtag_read_bytes(data, sizeof(data), pdMS_TO_TICKS(20));
        for (int i = 0; i < n; i++) {
            char ch = (char)data[i];
            if (ch == '\n' || ch == '\r') {
                if (overflow) puts("ERROR command too long");
                else if (used) { line[used] = '\0'; command(line); }
                used = 0; overflow = false;
            } else if (ch >= ' ' && ch <= '~' && !overflow) {
                if (used + 1 < sizeof(line)) line[used++] = ch;
                else overflow = true;
            }
        }
        int64_t now = esp_timer_get_time();
        int raw = gpio_get_level(BOARD_BUTTON);
        if (raw != last_raw) { last_raw = raw; changed_at = now; }
        if (raw != stable && now - changed_at >= 30000) {
            stable = raw;
            if (!stable) { pressed = true; long_press = false; pressed_at = now; }
            else if (pressed) {
                if (!long_press && lvgl_port_lock(100)) { desk_ui_next(); lvgl_port_unlock(); }
                pressed = false;
            }
        }
        if (pressed && !long_press && now - pressed_at >= 1000000) { long_press = true; board_audio_test(65); }
        if (now - heartbeat >= 30000000) { heartbeat = now; status(); }
    }
}
