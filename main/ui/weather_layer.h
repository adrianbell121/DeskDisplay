#pragma once
#include <stdint.h>
#include <stdbool.h>
#define WEATHER_PAYLOAD_BYTES 76
bool weather_layer_valid(const uint8_t *p);
void weather_layer_draw(uint16_t *pixels,const uint8_t *p,unsigned part,int x,int y,int w,int h,uint16_t color,uint16_t accent,double seconds);
