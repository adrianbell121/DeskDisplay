#include "board.h"
#include <stdatomic.h>
#include <stdlib.h>
#include <sys/time.h>
#include "esp_log.h"

static i2c_master_bus_handle_t bus;
static i2c_master_dev_handle_t rtc;
static atomic_bool valid;
static int unbcd(uint8_t n) { return (n >> 4) * 10 + (n & 15); }
static uint8_t bcd(int n) { return ((n / 10) << 4) | (n % 10); }
static bool leap(int y) { return y % 4 == 0 && (y % 100 != 0 || y % 400 == 0); }
static time_t utc_epoch(const struct tm *t)
{
    static const int month_days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    int year = t->tm_year + 1900;
    int limit = month_days[t->tm_mon] + (t->tm_mon == 1 && leap(year));
    if (t->tm_mday > limit) return (time_t)-1;
    int64_t days = 0;
    for (int y = 1970; y < year; y++) days += 365 + leap(y);
    for (int m = 0; m < t->tm_mon; m++) days += month_days[m] + (m == 1 && leap(year));
    days += t->tm_mday - 1;
    return days * 86400 + t->tm_hour * 3600 + t->tm_min * 60 + t->tm_sec;
}
i2c_master_bus_handle_t board_i2c(void) { return bus; }
bool board_clock_valid(void) { return atomic_load(&valid); }

esp_err_t board_clock_init(void)
{
    setenv("TZ", "EST5EDT,M3.2.0,M11.1.0", 1);
    tzset();
    const i2c_master_bus_config_t cfg = {
        .i2c_port = I2C_NUM_0, .sda_io_num = BOARD_I2C_SDA,
        .scl_io_num = BOARD_I2C_SCL, .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7, .flags.enable_internal_pullup = true,
    };
    esp_err_t err = i2c_new_master_bus(&cfg, &bus);
    if (err != ESP_OK) return err;
    const i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7, .device_address = 0x51,
        .scl_speed_hz = 100000,
    };
    err = i2c_master_bus_add_device(bus, &dev_cfg, &rtc);
    if (err != ESP_OK) return err;
    uint8_t reg = 0x04, values[7];
    err = i2c_master_transmit_receive(rtc, &reg, 1, values, sizeof(values), 100);
    if (err != ESP_OK) return err;
    // Oscillator-stop means the RTC lost power; do not treat that as real time.
    if (values[0] & 0x80) return ESP_ERR_INVALID_STATE;
    for (unsigned i = 0; i < 7; ++i) {
        uint8_t n = values[i];
        if ((n & 15) > 9 || (n >> 4) > 9) return ESP_ERR_INVALID_RESPONSE;
    }
    struct tm t = {
        .tm_sec = unbcd(values[0] & 0x7f), .tm_min = unbcd(values[1] & 0x7f),
        .tm_hour = unbcd(values[2] & 0x3f), .tm_mday = unbcd(values[3] & 0x3f),
        .tm_mon = unbcd(values[5] & 0x1f) - 1, .tm_year = 100 + unbcd(values[6]),
    };
    if (t.tm_sec > 59 || t.tm_min > 59 || t.tm_hour > 23 || t.tm_mday < 1 ||
        t.tm_mday > 31 || t.tm_mon < 0 || t.tm_mon > 11 || t.tm_year < 124)
        return ESP_ERR_INVALID_RESPONSE;
    time_t utc = utc_epoch(&t);
    if (utc == (time_t)-1) return ESP_ERR_INVALID_RESPONSE;
    const struct timeval tv = {.tv_sec = utc};
    if (settimeofday(&tv, NULL) != 0) return ESP_FAIL;
    atomic_store(&valid, true);
    ESP_LOGI("clock", "Restored UTC from PCF85063 RTC");
    return ESP_OK;
}

esp_err_t board_clock_set(time_t utc)
{
    if (utc < 1704067200LL || utc > 4102444799LL) return ESP_ERR_INVALID_ARG;
    const struct timeval tv = {.tv_sec = utc};
    if (settimeofday(&tv, NULL) != 0) return ESP_FAIL;
    atomic_store(&valid, true);
    if (!rtc) return ESP_ERR_NOT_FOUND;
    struct tm t;
    gmtime_r(&utc, &t);
    uint8_t values[] = {0x04, bcd(t.tm_sec), bcd(t.tm_min), bcd(t.tm_hour),
                       bcd(t.tm_mday), bcd(t.tm_wday), bcd(t.tm_mon + 1), bcd(t.tm_year - 100)};
    esp_err_t err = i2c_master_transmit(rtc, values, sizeof(values), 100);
    if (err == ESP_OK) {
        uint8_t ctrl[] = {0x00, 0x00}; // 24-hour mode, oscillator running.
        err = i2c_master_transmit(rtc, ctrl, sizeof(ctrl), 100);
    }
    return err;
}
