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
char *scene_store_faces_json(void);
esp_err_t scene_store_faces_update(const char *json,size_t size);
esp_err_t scene_store_faces_select(const char *id);
void scene_store_faces_state(uint32_t *revision,int *index);
// Caller holds the LVGL lock; never take the storage mutex from this callback.
bool scene_store_faces_next(void);
