/* Explicit data bindings, date tokens, and repeatable preview providers. */
(function(root){
  'use strict';
  const months=['January','February','March','April','May','June','July','August','September','October','November','December'];
  const days=['Sunday','Monday','Tuesday','Wednesday','Thursday','Friday','Saturday'];
  function formatDate(date,pattern='MMM D'){
    const pad=n=>String(n).padStart(2,'0'),h=date.getHours();
    const tokens={YYYY:String(date.getFullYear()),YY:pad(date.getFullYear()%100),MMMM:months[date.getMonth()],MMM:months[date.getMonth()].slice(0,3),MM:pad(date.getMonth()+1),M:String(date.getMonth()+1),DD:pad(date.getDate()),D:String(date.getDate()),dddd:days[date.getDay()],ddd:days[date.getDay()].slice(0,3),HH:pad(h),H:String(h),hh:pad(h%12||12),h:String(h%12||12),mm:pad(date.getMinutes()),m:String(date.getMinutes()),ss:pad(date.getSeconds()),s:String(date.getSeconds()),A:h<12?'AM':'PM',a:h<12?'am':'pm'};
    return String(pattern).replace(/\[[^\]]*\]|YYYY|MMMM|dddd|MMM|ddd|YY|MM|DD|HH|hh|mm|ss|M|D|H|h|m|s|A|a/g,t=>t[0]==='['?t.slice(1,-1):tokens[t]);
  }
  function sample(date=new Date()){
    const day=i=>{const d=new Date(date);d.setDate(d.getDate()+i);return formatDate(d,'YYYY-MM-DD');};
    const conditions=['sunny','cloudy','rainy','snowy','sunny'];
    return {mode:'sample',label:'Sample town',referenceDate:day(0),current:{condition:'cloudy',tempF:68,precipProbability:25},days:conditions.map((condition,i)=>({date:day(i),condition,tempF:[68,65,59,34,61][i],precipProbability:[25,45,85,65,15][i]})),hourly:Array.from({length:120},(_,i)=>({hour:i,precipProbability:Math.round(Math.max(0,Math.min(100,45+34*Math.sin(i*.07-1.5)+14*Math.sin(i*.23))))})),events:[{id:'sample-1',title:'Design review',start:day(0)+'T14:00:00',allDay:false},{id:'sample-2',title:'Pick up parcel',start:day(0)+'T17:30:00',allDay:false},{id:'sample-3',title:'Team standup',start:day(1)+'T09:30:00',allDay:false}]};
  }
  function agenda(events,date,limit=3){const today=formatDate(date,'YYYY-MM-DD'),tomorrow=new Date(date);tomorrow.setDate(tomorrow.getDate()+1);const end=formatDate(tomorrow,'YYYY-MM-DD');return events.filter(e=>typeof e.start==='string'&&[today,end].includes(e.start.slice(0,10))).sort((a,b)=>a.start.localeCompare(b.start)).slice(0,limit);}
  function curve(samples,x){if(!samples.length)return 0;if(samples.length===1)return samples[0].precipProbability;const p=Math.max(0,Math.min(samples.length-1,x)),hour=samples[0].hour+p/(samples.length-1)*(samples.at(-1).hour-samples[0].hour);let i=0;while(i<samples.length-2&&samples[i+1].hour<hour)i++;const t=(hour-samples[i].hour)/(samples[i+1].hour-samples[i].hour||1),s=t*t*(3-2*t);return samples[i].precipProbability*(1-s)+samples[i+1].precipProbability*s;}
  const bindings=['none','clock.date','weather.current','weather.forecast','calendar.events','weather.tempF','weather.precipProbability'];
  function clean(raw){
    if(!raw||typeof raw!=='object'||!/^\d{4}-\d\d-\d\d$/.test(raw.referenceDate)||!Number.isFinite(new Date(raw.referenceDate).getTime()))throw new Error('Invalid data reference date.');
    const weather=r=>{if(!r||!['sunny','cloudy','rainy','snowy'].includes(r.condition)||typeof r.tempF!=='number'||!Number.isFinite(r.tempF)||r.tempF< -150||r.tempF>150||typeof r.precipProbability!=='number'||!Number.isFinite(r.precipProbability)||r.precipProbability<0||r.precipProbability>100)throw new Error('Invalid weather values.');return {condition:r.condition,tempF:r.tempF,precipProbability:r.precipProbability};};
    if(!Array.isArray(raw.days)||raw.days.length!==5||!Array.isArray(raw.hourly)||raw.hourly.length<2||raw.hourly.length>240||!Array.isArray(raw.events)||raw.events.length>100)throw new Error('Data needs five days, 2–240 hourly samples, and at most 100 events.');
    return {mode:'preview',label:String(raw.label||'Preview data').slice(0,80),referenceDate:raw.referenceDate,current:weather(raw.current),days:raw.days.map((r,i)=>{const d=new Date(raw.referenceDate+'T12:00:00');d.setDate(d.getDate()+i);if(r.date!==formatDate(d,'YYYY-MM-DD'))throw new Error('Forecast dates must be consecutive.');return {...weather(r),date:r.date};}),hourly:raw.hourly.map((p,i)=>{if(typeof p.hour!=='number'||!Number.isFinite(p.hour)||p.hour<0||p.hour>240||i&&p.hour<=raw.hourly[i-1].hour||typeof p.precipProbability!=='number'||!Number.isFinite(p.precipProbability)||p.precipProbability<0||p.precipProbability>100)throw new Error('Invalid precipitation samples.');return {hour:p.hour,precipProbability:p.precipProbability};}),events:raw.events.map(e=>{if(!e||typeof e.start!=='string'||!Number.isFinite(new Date(e.start).getTime()))throw new Error('Invalid calendar event start.');return {id:String(e.id||'').slice(0,80),title:String(e.title||'Untitled').slice(0,120),start:e.start,allDay:e.allDay===true};})};
  }
  let data=null;
  function setData(value){data=value?clean(value):null;}
  function getData(date){return data||sample(date);}
  function textValue(w,date){const d=getData(date);if(w.binding==='clock.date')return formatDate(date,w.dateFormat);if(w.binding==='weather.tempF')return w.prefix+d.current.tempF+w.suffix;if(w.binding==='weather.precipProbability')return w.prefix+d.current.precipProbability+w.suffix;return w.text;}
  const api={formatDate,sample,agenda,curve,bindings,setData,getData,textValue,clean};if(typeof module!=='undefined'&&module.exports)module.exports=api;else root.WidgetData=api;
})(typeof globalThis!=='undefined'?globalThis:this);
