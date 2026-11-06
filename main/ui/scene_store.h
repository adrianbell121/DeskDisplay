#pragma once
#include "esp_err.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
esp_err_t scene_store_init(void);
void scene_store_command(const char *arg);
esp_err_t scene_store_live(const uint8_t *data,size_t size);
esp_err_t scene_store_save_live(void);
void scene_store_crc(uint32_t *live,uint32_t *saved);
