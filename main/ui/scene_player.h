#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

#define SCENE_MAX_BYTES 262144
bool scene_player_validate(const uint8_t *data, size_t size);
bool scene_player_set(const uint8_t *data, size_t size, int64_t now_ms);
bool scene_player_ready(void);
void scene_player_render(uint16_t *pixels, const struct tm *time, bool valid, int64_t now_ms, int32_t frame_usec);

unsigned scene_player_compare_island(const struct tm *time, bool valid, int64_t now_ms);
