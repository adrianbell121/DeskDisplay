#include "weather_layer.h"
#include "weather.h"
#include "esp_timer.h"
#include <math.h>
#include <string.h>
static float value(const uint8_t *p,int index){float v;memcpy(&v,p+4+index*4,4);return v;}
bool weather_layer_valid(const uint8_t *p,unsigned size){
 if((size!=WEATHER_PAYLOAD_BYTES&&size!=WEATHER_SKY_PAYLOAD_BYTES)||p[0]>6||p[1]>1||p[2]>1||p[3]!=(size==WEATHER_SKY_PAYLOAD_BYTES))return false;
 if(p[3]){float horizon;memcpy(&horizon,p+76,4);if(!isfinite(horizon)||horizon < -63||horizon>63)return false;}
 static const float low[]={0,1,1,-8,0,1,1,0,.1,1,0,5,5,.03,1,0,-90,-180};
 static const float high[]={100,60,5,8,100,30,3,12,5,8,3,600,600,.5,2,4,90,180};
 for(int i=0;i<18;i++){float n=value(p,i);if(!isfinite(n)||n<low[i]||n>high[i])return false;}
 return value(p,12)>=value(p,11);
}
static uint16_t *canvas;static int ox,oy,width,height,clip_y;
static void dot(float x,float y,uint16_t color){int ix=(int)floorf(x+.5f),iy=(int)floorf(y+.5f);if(ix<0||iy<0||ix>=width||iy>=height)return;ix+=ox;iy+=oy;if((unsigned)ix<64&&(unsigned)iy<64&&iy<clip_y)canvas[iy*64+ix]=color;}
static void line(float x,float y,float tx,float ty,uint16_t color,int thick){int steps=(int)ceilf(fmaxf(fmaxf(fabsf(tx-x),fabsf(ty-y)),1));for(int i=0;i<=steps;i++)for(int d=0;d<thick;d++)dot(x+(tx-x)*i/steps+d,y+(ty-y)*i/steps,color);}
static double random_at(int n){double a=sin(n*127.1+311.7)*43758.5453;return a-floor(a);}
void weather_layer_draw(uint16_t *pixels,const uint8_t *p,unsigned part,int x,int y,int w,int h,uint16_t ink,uint16_t accent,double seconds,double epoch){
 if(!p[1])return;
 int condition=p[0]?p[0]:board_weather_condition();if(!condition)return;
 canvas=pixels;ox=x;oy=y;width=w;height=h;clip_y=64;double t=seconds*value(p,15);
 if(part==0&&(condition==1||condition==2)){
  solar_phase_t phase={.sun=true};
  if(p[3]){if(!board_weather_sky(epoch,&phase))return;float horizon;memcpy(&horizon,p+76,4);clip_y=(int)ceilf(horizon);double lowest=horizon-(h-1)/2.0;oy=(int)floor(lowest-(lowest-y)*sin(3.141592653589793*phase.progress)+.5);}
  float cx=(w-1)/2.f,cy=(h-1)/2.f;
  if(!phase.sun){float r=fmaxf(1,fminf(w,h)*.46f);uint16_t moon=p[80]|((uint16_t)p[81]<<8),detail=p[82]|((uint16_t)p[83]<<8);
   for(int j=0;j<h;j++)for(int i=0;i<w;i++){float a=i-cx,b=j-cy,c=a-r*.7f,d=b+r*.2f;if(a*a+b*b<=r*r&&c*c+d*d>r*r*.81f)dot(i,j,i<cx-r*.3f&&j>cy?detail:moon);}
  }else{float r=fmaxf(1,fminf(w,h)*.26f);for(int j=0;j<h;j++)for(int i=0;i<w;i++)if((i-cx)*(i-cx)+(j-cy)*(j-cy)<=r*r)dot(i,j,ink);if(p[2])for(int i=0;i<8;i++){float a=i*3.14159265f/4;dot(cx+cosf(a)*(r+2),cy+sinf(a)*(r+2),accent);}}
 }
 if(part==1&&condition!=1){int n=condition==2?(int)ceilf(value(p,9)/2):value(p,9);for(int i=0;i<n;i++){float cx=fmodf(i*(w/(float)n)+7+sin(t*value(p,10)+i)*2,w),cy=3+(i%2)*2;for(int j=0;j<h;j++)for(int k=(int)fmaxf(0,cx-8);k<w&&k<=cx+8;k++){float a=(k-cx)/7,b=(j-cy)/2.5f,cc=(k-cx-2)/4,d=(j-cy+2)/3;if(a*a+b*b<1||cc*cc+d*d<1)dot(k,j,condition==4||condition==5?accent:ink);}}}
 if(part==2&&(condition==4||condition==5)){for(int i=0;i<value(p,0);i++){float travel=fmod(t*value(p,1)+random_at(i+1)*(h+8),h+8),yy=travel-5,xx=fmod(random_at(i+201)*w+travel*value(p,3)*.15+w*4,w);line(xx,yy,xx+value(p,3)*.2f,yy+value(p,2),ink,1);}}
 if(part==3&&condition==6){for(int i=0;i<value(p,4);i++){float travel=fmod(t*value(p,5)+random_at(i+11)*(h+8),h+8),yy=travel-4,xx=fmod(random_at(i+401)*w+sin(t*value(p,8)+i)*value(p,7)+w*4,w);dot(xx,yy,ink);if(value(p,6)>=2){dot(xx-1,yy,accent);dot(xx+1,yy,accent);dot(xx,yy-1,accent);dot(xx,yy+1,accent);}if(value(p,6)>=3){dot(xx-1,yy-1,ink);dot(xx+1,yy+1,ink);}}}
 if(part==4&&condition==5){
  static double next;static double previous;static int index;static float oldmin,oldmax;
  float min=value(p,11),max=value(p,12);
  if(t<previous||min!=oldmin||max!=oldmax){next=0;index=0;oldmin=min;oldmax=max;}
  previous=t;if(next==0)next=min+(max-min)*random_at(31);
  bool forced=esp_timer_get_time()<board_weather_flash_until();
  bool strike=forced;
  while(t>=next+value(p,13)){index++;next+=min+(max-min)*random_at(index+31);}
  strike=strike||(t>=next&&t<next+value(p,13));
  static bool was_strike;if(strike&&!was_strike)board_weather_note_strike(forced);was_strike=strike;
  if(strike){float xx=5+random_at(index+900)*(w-10),yy=0;for(int i=0;i<5;i++){float nx=fmaxf(0,fminf(w-1,xx+(random_at(index*7+i+501)-.5)*12)),ny=(i+1)*h/5.f;line(xx,yy,nx,ny,ink,value(p,14));if(i==2)line(xx,yy,xx+7,yy+4,accent,1);xx=nx;yy=ny;}}
 }
}
