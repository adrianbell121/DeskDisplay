#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
typedef struct { int64_t sunrise,sunset; } solar_day_t;
typedef struct { bool sun; int64_t start,end; double progress; } solar_phase_t;
// Day wins at sunrise. Night is bounded by consecutive dated sunsets/sunrises.
static inline bool solar_phase(const solar_day_t *days,unsigned count,double epoch,solar_phase_t *out){
 if(!isfinite(epoch))return false;
 int64_t start=0,end=INT64_MAX;
 for(unsigned i=0;i<count;i++){
  if(days[i].sunrise<=0||days[i].sunset<=days[i].sunrise)continue;
  if(epoch>=days[i].sunrise&&epoch<days[i].sunset){*out=(solar_phase_t){true,days[i].sunrise,days[i].sunset,(epoch-days[i].sunrise)/(days[i].sunset-days[i].sunrise)};return true;}
  if(days[i].sunset<=epoch&&days[i].sunset>start)start=days[i].sunset;
  if(days[i].sunrise>epoch&&days[i].sunrise<end)end=days[i].sunrise;
 }
 if(!start||end==INT64_MAX||end-start>36*3600)return false;
 *out=(solar_phase_t){false,start,end,(epoch-start)/(end-start)};return true;
}
