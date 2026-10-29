#pragma once
#include <stdint.h>
#include <stdbool.h>
void board_weather_init(void);
void board_weather_configure(float lat,float lon,bool active);
int board_weather_condition(void);
int64_t board_weather_flash_until(void);
void board_weather_command(const char *arg);

void board_weather_note_strike(bool forced);
