#include "board.h"
#include <math.h>
#include <stdatomic.h>
#include "driver/i2s_std.h"
#include "esp_codec_dev.h"
#include "esp_codec_dev_defaults.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

static QueueHandle_t requests;
static atomic_int audio_status = ESP_ERR_INVALID_STATE;
static esp_codec_dev_handle_t speaker;

static esp_err_t audio_init(void)
{
    if (!board_i2c()) return ESP_ERR_INVALID_STATE;
    i2s_chan_handle_t tx = NULL;
    i2s_chan_config_t channel = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    channel.auto_clear = true;
    esp_err_t err = i2s_new_channel(&channel, &tx, NULL);
    if (err != ESP_OK) return err;
    i2s_std_config_t cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(16000),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {.mclk = BOARD_I2S_MCLK, .bclk = BOARD_I2S_BCLK,
                     .ws = BOARD_I2S_WS, .dout = BOARD_I2S_DOUT, .din = I2S_GPIO_UNUSED},
    };
    err = i2s_channel_init_std_mode(tx, &cfg);
    if (err != ESP_OK) return err;
    err = i2s_channel_enable(tx);
    if (err != ESP_OK) return err;
    audio_codec_i2s_cfg_t data_cfg = {.port = I2S_NUM_0, .tx_handle = tx};
    const audio_codec_data_if_t *data = audio_codec_new_i2s_data(&data_cfg);
    const audio_codec_gpio_if_t *gpio = audio_codec_new_gpio();
    audio_codec_i2c_cfg_t i2c = {
        .port = I2C_NUM_0, .addr = ES8311_CODEC_DEFAULT_ADDR, .bus_handle = board_i2c(),
    };
    const audio_codec_ctrl_if_t *ctrl = audio_codec_new_i2c_ctrl(&i2c);
    if (!data || !gpio || !ctrl) return ESP_ERR_NO_MEM;
    es8311_codec_cfg_t codec_cfg = {
        .ctrl_if = ctrl, .gpio_if = gpio, .codec_mode = ESP_CODEC_DEV_WORK_MODE_DAC,
        .pa_pin = BOARD_AMP_ENABLE, .use_mclk = true,
        .hw_gain = {.pa_voltage = 5.0f, .codec_dac_voltage = 3.3f},
    };
    const audio_codec_if_t *codec = es8311_codec_new(&codec_cfg);
    if (!codec) return ESP_ERR_NO_MEM;
    esp_codec_dev_cfg_t dev_cfg = {
        .dev_type = ESP_CODEC_DEV_TYPE_OUT, .codec_if = codec, .data_if = data,
    };
    speaker = esp_codec_dev_new(&dev_cfg);
    if (!speaker) return ESP_ERR_NO_MEM;
    esp_codec_dev_sample_info_t fs = {.sample_rate = 16000, .channel = 2, .bits_per_sample = 16};
    if (esp_codec_dev_open(speaker, &fs) != ESP_CODEC_DEV_OK) return ESP_FAIL;
    if (esp_codec_dev_set_out_vol(speaker, 65) != ESP_CODEC_DEV_OK) return ESP_FAIL;
    if (esp_codec_dev_set_out_mute(speaker, true) != ESP_CODEC_DEV_OK) return ESP_FAIL;
    return ESP_OK;
}

static esp_err_t tone(float frequency)
{
    int16_t pcm[256 * 2];
    const unsigned samples = 16000 * 180 / 1000;
    for (unsigned offset = 0; offset < samples; offset += 256) {
        unsigned count = samples - offset;
        if (count > 256) count = 256;
        for (unsigned i = 0; i < count; ++i) {
            unsigned n = offset + i;
            float envelope = fminf(1.0f, fminf(n / 160.0f, (samples - 1 - n) / 160.0f));
            int16_t sample = (int16_t)(6000.0f * envelope * sinf(6.2831853f * frequency * n / 16000.0f));
            pcm[i * 2] = pcm[i * 2 + 1] = sample;
        }
        if (esp_codec_dev_write(speaker, pcm, count * 2 * sizeof(int16_t)) != ESP_CODEC_DEV_OK)
            return ESP_FAIL;
    }
    return ESP_OK;
}

static void audio_task(void *arg)
{
    (void)arg;
    esp_err_t err = audio_init();
    atomic_store(&audio_status, err);
    ESP_LOGI("audio", "ES8311 speaker init: %s", esp_err_to_name(err));
    uint8_t request;
    while (xQueueReceive(requests, &request, portMAX_DELAY)) {
        if (err != ESP_OK) { ESP_LOGW("audio", "Speaker unavailable"); continue; }
        err = esp_codec_dev_set_out_vol(speaker, request) == ESP_CODEC_DEV_OK ? ESP_OK : ESP_FAIL;
        if (err == ESP_OK) err = esp_codec_dev_set_out_mute(speaker, false) == ESP_CODEC_DEV_OK ? ESP_OK : ESP_FAIL;
        if (err == ESP_OK) err = tone(523.25f);
        if (err == ESP_OK) err = tone(783.99f);
        int16_t silence[512] = {0};
        if (esp_codec_dev_write(speaker, silence, sizeof(silence)) != ESP_CODEC_DEV_OK) err = ESP_FAIL;
        vTaskDelay(pdMS_TO_TICKS(50));
        if (esp_codec_dev_set_out_mute(speaker, true) != ESP_CODEC_DEV_OK) err = ESP_FAIL;
        atomic_store(&audio_status, err);
        ESP_LOGI("audio", "CHIME %s (%u%% volume)", esp_err_to_name(err), request);
    }
    vTaskDelete(NULL);
}
esp_err_t board_audio_start(void)
{
    requests = xQueueCreate(2, sizeof(uint8_t));
    if (!requests) return ESP_ERR_NO_MEM;
    if (xTaskCreate(audio_task, "speaker", 4096, NULL, 4, NULL) != pdPASS) {
        vQueueDelete(requests); requests = NULL; return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}
esp_err_t board_audio_status(void) { return atomic_load(&audio_status); }
esp_err_t board_audio_test(unsigned volume)
{
    if (!requests) return ESP_ERR_INVALID_STATE;
    if (volume > 85) return ESP_ERR_INVALID_ARG;
    uint8_t request = volume;
    return xQueueSend(requests, &request, 0) == pdTRUE ? ESP_OK : ESP_ERR_TIMEOUT;
}
