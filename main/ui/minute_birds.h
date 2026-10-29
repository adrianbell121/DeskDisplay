#pragma once
#include <stdint.h>
#include <stdbool.h>
bool minute_bird_pose(unsigned index,double second,int *x,int *y,int *wing);
void minute_bird_render(uint16_t *pixels,unsigned index,double second,uint16_t color);

void minute_bird_test_phase(double second);
double minute_bird_phase(double actual);
