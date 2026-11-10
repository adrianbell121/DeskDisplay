(async function(){
'use strict';
const Store=window.FaceStudioState,S=window.FaceScene,W=window.SceneWeather;
const banner=document.createElement('div');banner.id='clock-connection';banner.setAttribute('role','status');banner.style='padding:12px 28px;background:#17392c;color:#c9f2d9';banner.textContent='Connecting to this clock…';document.querySelector('.masthead').after(banner);
const key='deskdisplay.board-workspace.v1';
let revision=0,state=null,pending=null,saveTimer=null,saving=false,blocked=false;
let liveFace=null,liveTimer=null,liveBusy=false,lastScene='',liveError='',workspaceError='',menuBusy=false,previewActive=false;
const status=text=>{const el=document.getElementById('save-status');if(el)el.textContent=text;};
async function get(path){const r=await fetch(path,{cache:'no-store'}),data=await r.json();if(!r.ok)throw new Error(data.error||'Clock unavailable');return data;}
async function post(path,body,binary=false){const r=await fetch(path,{method:'POST',headers:{'Content-Type':binary?'application/octet-stream':'application/json','X-Face-Lab':'1'},body:binary?body:JSON.stringify(body)});const data=await r.json();if(!r.ok){if(r.status===409&&path==='/api/workspace')blocked=true;throw new Error(data.error||'Clock rejected the change');}return data;}
function describe(text){banner.textContent=liveError?'Preview only: '+liveError:workspaceError||text;status(banner.textContent);banner.style.background=liveError||workspaceError?'#533820':'#17392c';}
async function flush(){
 if(saving||blocked||!pending)return;
 saving=true;const next=pending;pending=null;
 try{const result=await post('/api/workspace',{state:next,revision});revision=result.revision;state=next;workspaceError='';describe('Live on clock · saving library…');}
 catch(error){pending=pending||next;workspaceError=error.message+' Browser recovery copy kept.';describe('');}
 finally{saving=false;if(pending&&!blocked)saveTimer=setTimeout(flush,1000);}
}
async function sendLive(){
 liveTimer=null;if(liveBusy||!liveFace)return;
 const face=liveFace;liveFace=null;liveBusy=true;
 try{
  const bytes=S.compile(face),signature=Array.from(bytes).join(',');
  if(signature!==lastScene){await post('/api/scene',bytes,true);lastScene=signature;}
  liveError='';describe('Live on clock · changes save automatically');
 }catch(error){liveError=error.message+' The last valid face stays on the clock.';describe('');}
 finally{liveBusy=false;if(liveFace&&!liveTimer)liveTimer=setTimeout(sendLive,60);}
}
function live(value){if(blocked||menuBusy||(!previewActive&&JSON.stringify(value)===JSON.stringify(state)))return;const f=value.faces.find(f=>f.id===value.selected);if(!f)return;previewActive=true;liveFace=S.clone(f);if(!liveTimer&&!liveBusy)liveTimer=setTimeout(sendLive,60);}
function save(value){
 if(!pending&&!saving&&JSON.stringify(value)===JSON.stringify(state))return;
 live(value);
 pending=JSON.parse(JSON.stringify(value));try{localStorage.setItem(key,JSON.stringify(pending));}catch{}
 clearTimeout(saveTimer);saveTimer=setTimeout(flush,200);describe('Live editing · saving library…');
}
async function getWeather(location){
 const trimmed=location.trim(),known=/^boston,? (ma|massachusetts)$/i.test(trimmed)?{latitude:42.3601,longitude:-71.0589,name:'Boston, Massachusetts'}:/^(bedford,? ma(,? 01730)?|01730)$/i.test(trimmed)?{latitude:42.49065,longitude:-71.27617,name:'Bedford, Massachusetts'}:null;
 let place=known;
 if(!place){const result=await fetch('https://geocoding-api.open-meteo.com/v1/search?'+new URLSearchParams({name:trimmed,count:'1',format:'json'}));const data=await result.json();place=data.results?.[0];if(!place)throw new Error('Location not found.');}
 try{const data=await get('/api/weather/current');if(Math.abs(data.latitude-place.latitude)<.001&&Math.abs(data.longitude-place.longitude)<.001)return {...data,location:place.name};}catch{}
 const params=new URLSearchParams({latitude:place.latitude,longitude:place.longitude,current:'temperature_2m,weather_code,is_day',temperature_unit:'fahrenheit',daily:'sunrise,sunset',timeformat:'unixtime',timezone:'auto',past_days:'1',forecast_days:'3'});
 const result=await fetch('https://api.open-meteo.com/v1/forecast?'+params),data=await result.json();if(!result.ok)throw new Error(data.reason||'Weather unavailable.');const code=data.current.weather_code;
 const condition=code===0?'sunny':[1,2].includes(code)?'partly-cloudy':[3,45,48].includes(code)?'cloudy':[71,73,75,77,85,86].includes(code)?'snowy':code>=95?'storm':'rainy';
 return {location:place.name,latitude:place.latitude,longitude:place.longitude,condition,tempF:data.current.temperature_2m,observedAt:data.current.time*1000,timezone:data.timezone,solarDays:data.daily.sunrise.map((sunrise,i)=>({sunrise,sunset:data.daily.sunset[i]}))};
}
try{
 const shared=await get('/api/workspace');revision=shared.revision;state=shared.state;
 if(!state){state=Store.fresh();revision=(await post('/api/workspace',{revision,state})).revision;}
 state=Store.clean(state);
 window.BoardWorkspace={live,getWeather,get,post,resetPreview(){lastScene='';previewActive=false;},async beginMenu(){menuBusy=true;clearTimeout(liveTimer);liveTimer=null;liveFace=null;const deadline=Date.now()+5000;while(liveBusy){if(Date.now()>deadline)throw new Error('A live preview is still sending. Retry after it finishes.');await new Promise(resolve=>setTimeout(resolve,20));}},endMenu(){menuBusy=false;lastScene='';previewActive=false;}};
 window.SharedWorkspace={get state(){return state;},save};
 for(const boot of window.BoardEditorBoot)boot();
 const sampleClock=document.querySelector('#clock-mode option[value=live]');if(sampleClock)sampleClock.textContent='Current time';
 setInterval(async()=>{
  try{
   const info=await get('/api/status');
   await window.BoardFaceMenu?.sync(info);
   if(info.saveError)throw new Error('Clock storage could not save. Export a backup before closing.');
   if(!pending&&!blocked)workspaceError='';
   if(!pending&&!saving&&info.revision!==revision){
    if(window.Studio.sharedBusy||document.querySelector('dialog[open]')||document.activeElement?.matches('input,textarea,select'))return;
    const latest=await get('/api/workspace');revision=latest.revision;state=Store.clean(latest.state);window.Studio.loadShared(state);lastScene='';
   }
   describe(blocked?'Another browser changed the library. Reload before editing.':info.pending||pending||saving?'Live on clock · saving…':'Live on clock · library saved');
  }catch(error){workspaceError=error.message;describe('');}
 },2000);
}catch(error){banner.textContent='Clock dashboard could not load: '+error.message;}
})();
