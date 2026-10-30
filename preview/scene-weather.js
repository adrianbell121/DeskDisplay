(function(root){
'use strict';
const conditions=['sunny','partly-cloudy','cloudy','rainy','storm','snowy'],kinds=['weatherSun','weatherClouds','weatherRain','weatherSnow','weatherLightning'];
const defaults={enabled:true,location:'Boston, MA',condition:'auto',rainDensity:22,rainSpeed:19,rainLength:2,rainSlant:1,snowDensity:18,snowSpeed:6,snowSize:2,snowDrift:3,snowDriftSpeed:1,cloudCount:5,cloudSpeed:.15,lightningMin:60,lightningMax:120,lightningDuration:.12,lightningWidth:1,sunRays:true};
const limits={rainDensity:[0,100],rainSpeed:[1,60],rainLength:[1,5],rainSlant:[-8,8],snowDensity:[0,100],snowSpeed:[1,30],snowSize:[1,3],snowDrift:[0,12],snowDriftSpeed:[.1,5],cloudCount:[1,8],cloudSpeed:[0,3],lightningMin:[5,600],lightningMax:[5,600],lightningDuration:[.03,.5],lightningWidth:[1,2]};
const cache=new Map();
function clean(raw={}){const c={...defaults};c.enabled=raw.enabled!==false;c.location=typeof raw.location==='string'?raw.location.slice(0,100):defaults.location;c.condition=['auto',...conditions].includes(raw.condition)?raw.condition:'auto';c.sunRays=raw.sunRays!==false;c.sunCycle=raw.sunCycle===true;c.moonColor=/^#[0-9a-f]{6}$/i.test(raw.moonColor)?raw.moonColor:'#d6e6f4';c.moonAccent=/^#[0-9a-f]{6}$/i.test(raw.moonAccent)?raw.moonAccent:'#9cb8d3';for(const [k,[min,max]]of Object.entries(limits))c[k]=typeof raw[k]==='number'&&Number.isFinite(raw[k])?Math.min(max,Math.max(min,raw[k])):defaults[k];c.lightningMax=Math.max(c.lightningMin,c.lightningMax);return c;}
function setData(location,data){if(!data||!conditions.includes(data.condition)||!Number.isFinite(data.tempF))throw new Error('Invalid current weather.');cache.set(location.trim().toLowerCase(),data);}
function getData(location){return cache.get(location.trim().toLowerCase());}
function active(c){return c.condition==='auto'?getData(c.location)?.condition:c.condition;}
// Epoch intervals retain their real duration across midnight and DST changes.
function skyPhase(days,epoch){
 if(!Array.isArray(days)||!Number.isFinite(epoch))return null;
 const valid=days.filter(d=>Number.isFinite(d.sunrise)&&Number.isFinite(d.sunset)&&d.sunrise<d.sunset);
 for(const d of valid)if(epoch>=d.sunrise&&epoch<d.sunset)return {body:'sun',start:d.sunrise,end:d.sunset,progress:(epoch-d.sunrise)/(d.sunset-d.sunrise)};
 let start=-Infinity,end=Infinity;for(const d of valid){if(d.sunset<=epoch)start=Math.max(start,d.sunset);if(d.sunrise>epoch)end=Math.min(end,d.sunrise);}
 return Number.isFinite(start)&&Number.isFinite(end)&&end-start<=36*3600?{body:'moon',start,end,progress:(epoch-start)/(end-start)}:null;
}
function horizon(scene){return scene.widgets.find(w=>w.visible&&w.type==='island'&&w.islandPart===2)?.y??55;}
function skyPlacement(w,date,c,horizonY=55){
 if(!c.sunCycle||w.type!=='weatherSun')return null;
 const phase=skyPhase(getData(c.location)?.solarDays,date.getTime()/1000);
 if(!phase)return {missing:true,horizon:horizonY,offset:0};
 // At either end of an arc, the waterline crosses the icon's center.
 const lowest=horizonY-(w.h-1)/2;
 const top=Math.round(lowest-(lowest-w.y)*Math.sin(Math.PI*phase.progress));
 return {...phase,horizon:horizonY,offset:top-w.y,top};
}
const schedules=new Map();
function strikeAt(t,c){if(!Number.isFinite(t)||t<0)return null;const key=c.lightningMin+':'+c.lightningMax;let starts=schedules.get(key);if(!starts){starts=[];schedules.set(key,starts);}while(!starts.length||starts.at(-1)<=t){const i=starts.length;starts.push((starts.at(-1)||0)+c.lightningMin+(c.lightningMax-c.lightningMin)*random(i+31));}let lo=0,hi=starts.length-1,index=-1;while(lo<=hi){const mid=(lo+hi)>>1;if(starts[mid]<=t){index=mid;lo=mid+1;}else hi=mid-1;}return index>=0&&t-starts[index]<c.lightningDuration?{index,age:t-starts[index]}:null;}
function random(n){const a=Math.sin(n*127.1+311.7)*43758.5453;return a-Math.floor(a);}
function raster(w,seconds,date=new Date()){const c=clean(w.weather),condition=active(c),out=new Uint8ClampedArray(w.w*w.h*4);if(!c.enabled||!condition)return out;const sky=w.sky||skyPlacement(w,date,c);if(sky?.missing)return out;const color=h=>[parseInt(h.slice(1,3),16),parseInt(h.slice(3,5),16),parseInt(h.slice(5,7),16)];
 function dot(x,y,ink){x=Math.round(x);y=Math.round(y);if(x<0||y<0||x>=w.w||y>=w.h)return;out.set([...ink,255],(y*w.w+x)*4);}
 function line(x,y,tx,ty,ink,width=1){const steps=Math.max(Math.abs(tx-x),Math.abs(ty-y),1);for(let i=0;i<=steps;i++)for(let d=0;d<width;d++)dot(x+(tx-x)*i/steps+d,y+(ty-y)*i/steps,ink);}
 const ink=color(w.color),accent=color(w.secondary),t=seconds*w.speed;
 if(w.type==='weatherSun'&&['sunny','partly-cloudy'].includes(condition)){const cx=(w.w-1)/2,cy=(w.h-1)/2;
  if(sky?.body==='moon'){const r=Math.max(1,Math.min(w.w,w.h)*.46),moon=color(c.moonColor),detail=color(c.moonAccent);for(let y=0;y<w.h;y++)for(let x=0;x<w.w;x++)if((x-cx)**2+(y-cy)**2<=r*r&&(x-cx-r*.7)**2+(y-cy+r*.2)**2>r*r*.81)dot(x,y,(x<cx-r*.3&&y>cy)?detail:moon);}
  else{const r=Math.max(1,Math.min(w.w,w.h)*.26);for(let y=0;y<w.h;y++)for(let x=0;x<w.w;x++)if((x-cx)**2+(y-cy)**2<=r*r)dot(x,y,ink);if(c.sunRays)for(let i=0;i<8;i++){const a=i*Math.PI/4;dot(cx+Math.cos(a)*(r+2),cy+Math.sin(a)*(r+2),accent);}}
 }
 if(w.type==='weatherClouds'&&condition!=='sunny'){const storm=['rainy','storm'].includes(condition),n=condition==='partly-cloudy'?Math.max(1,Math.ceil(c.cloudCount/2)):c.cloudCount;for(let i=0;i<n;i++){const cx=(i*(w.w/n)+7+Math.sin(t*c.cloudSpeed+i)*2)%w.w,cy=3+(i%2)*2;for(let y=0;y<w.h;y++)for(let x=0;x<w.w;x++)if(((x-cx)/7)**2+((y-cy)/2.5)**2<1||((x-cx-2)/4)**2+((y-cy+2)/3)**2<1)dot(x,y,storm?accent:ink);}}
 if(w.type==='weatherRain'&&['rainy','storm'].includes(condition)){for(let i=0;i<c.rainDensity;i++){const travel=(t*c.rainSpeed+random(i+1)*(w.h+8))%(w.h+8),y=travel-5,x=(random(i+201)*w.w+travel*c.rainSlant*.15+w.w*4)%w.w;line(x,y,x+c.rainSlant*.2,y+c.rainLength,ink);}}
 if(w.type==='weatherSnow'&&condition==='snowy'){for(let i=0;i<c.snowDensity;i++){const travel=(t*c.snowSpeed+random(i+11)*(w.h+8))%(w.h+8),y=travel-4,x=(random(i+401)*w.w+Math.sin(t*c.snowDriftSpeed+i)*c.snowDrift+w.w*4)%w.w;dot(x,y,ink);if(c.snowSize>=2){dot(x-1,y,accent);dot(x+1,y,accent);dot(x,y-1,accent);dot(x,y+1,accent);}if(c.snowSize>=3){dot(x-1,y-1,ink);dot(x+1,y+1,ink);}}}
 if(w.type==='weatherLightning'&&condition==='storm'){const strike=strikeAt(t,c);if(strike){let x=5+random(strike.index+900)*(w.w-10),y=0;for(let i=0;i<5;i++){const nx=Math.max(0,Math.min(w.w-1,x+(random(strike.index*7+i+501)-.5)*12)),ny=(i+1)*w.h/5;line(x,y,nx,ny,ink,c.lightningWidth);if(i===2)line(x,y,x+7,y+4,accent);x=nx;y=ny;}}}
 return out;
}
function templates(){return [
 {type:'weatherSun',name:'Weather sun',x:52,y:30,w:11,h:11,color:'#ffd36a',secondary:'#ffecb0'},
 {type:'weatherClouds',name:'Weather clouds',x:0,y:32,w:64,h:10,color:'#a8bdc4',secondary:'#374956'},
 {type:'weatherRain',name:'Weather rain',x:0,y:39,w:64,h:25,color:'#76a9c5',secondary:'#76a9c5'},
 {type:'weatherSnow',name:'Weather snow',x:0,y:39,w:64,h:25,color:'#ffffff',secondary:'#c8dfe8'},
 {type:'weatherLightning',name:'Weather lightning',x:0,y:38,w:64,h:26,color:'#fff6ca',secondary:'#d7e7ff'}
 ];}
const api={defaults,limits,kinds,conditions,clean,active,setData,getData,skyPhase,skyPlacement,horizon,strikeAt,raster,templates};if(typeof module!=='undefined'&&module.exports)module.exports=api;else root.SceneWeather=api;
})(typeof globalThis!=='undefined'?globalThis:this);
