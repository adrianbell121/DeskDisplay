(function(root){
'use strict';
const perches=[[9,43],[17,42],[20,47],[25,46],[30,49]];
function pose(index,second){const s=((second%60)+60)%60,[px,py]=perches[index];let p,x,y,flight=false;
 if(s<4){const duration=3+index*.15;if(s>=duration)return null;p=s/duration;const q=p*p*(3-2*p);x=px+((index%2?-2:65)-px)*q;y=py+(32+index*.6-py)*q;flight=true;}
 else{const arrival=(index+1)*10;if(s<arrival)return null;p=Math.min(1,(s-arrival)/2);const q=p*p*(3-2*p),start=index%2?65:-2;x=start+(px-start)*q;y=py-5*(1-q)-Math.sin(p*Math.PI)*2;flight=p<1;}
 return {x:Math.round(x),y:Math.round(y),wing:flight&&Math.sin(p*18+index)>0?1:-1,flight};
}
function raster(w,date){const out=new Uint8ClampedArray(w.w*w.h*4),p=pose(w.birdIndex,date.getSeconds()+date.getMilliseconds()/1000);if(!p)return out;const n=parseInt(w.color.slice(1),16),color=[Math.round((n>>19)*255/31),Math.round(((n>>10)&63)*255/63),Math.round(((n>>3)&31)*255/31)];for(const [x,y]of [[p.x,p.y],[p.x-1,p.y+p.wing],[p.x+1,p.y+p.wing]]){const sx=x,sy=y-32;for(let j=0;j<w.h;j++)for(let i=0;i<w.w;i++)if(Math.floor((i+.5)*64/w.w)===sx&&Math.floor((j+.5)*32/w.h)===sy)out.set([...color,255],(j*w.w+i)*4);}return out;}
const api={pose,raster,perches};if(typeof module!=='undefined'&&module.exports)module.exports=api;else root.MinuteBirds=api;
})(typeof globalThis!=='undefined'?globalThis:this);
