#include "minute_birds.h"
#include <math.h>
#include <string.h>
static double test_phase=-1;
void minute_bird_test_phase(double second){test_phase=second;}
double minute_bird_phase(double actual){return test_phase>=0?test_phase:actual;}
bool minute_bird_pose(unsigned index,double second,int *x,int *y,int *wing){
 static const int perches[5][2]={{9,43},{17,42},{20,47},{25,46},{30,49}};
 if(index>=5)return false;
 double s=fmod(second+60,60),p,xx,yy;bool flight=false;int px=perches[index][0],py=perches[index][1];
 if(s<4){double duration=3+index*.15;if(s>=duration)return false;p=s/duration;double q=p*p*(3-2*p);xx=px+((index%2?-2:65)-px)*q;yy=py+(32+index*.6-py)*q;flight=true;}
 else{int arrival=(index+1)*10;if(s<arrival)return false;p=fmin(1,(s-arrival)/2);double q=p*p*(3-2*p),start=index%2?65:-2;xx=start+(px-start)*q;yy=py-5*(1-q)-sin(p*3.141592653589793)*2;flight=p<1;}
 *x=(int)floor(xx+.5);*y=(int)floor(yy+.5);*wing=flight&&sin(p*18+index)>0?1:-1;return true;
}
void minute_bird_render(uint16_t *pixels,unsigned index,double second,uint16_t color){
 memset(pixels,0,8192);int x,y,wing;if(!minute_bird_pose(index,second,&x,&y,&wing))return;
 for(int dx=-1;dx<=1;dx++){int xx=x+dx,yy=y+(dx?wing:0);if((unsigned)xx<64&&(unsigned)yy<64)pixels[yy*64+xx]=color;}
}
