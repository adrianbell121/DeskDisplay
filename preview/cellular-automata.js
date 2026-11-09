(function(root){
  'use strict';
  const I=typeof module!=='undefined'&&module.exports?require('./island-assets.js'):root.IslandAssets;
  let config=typeof module!=='undefined'&&module.exports?require('../cellular-automata-config.json'):null;
  function configure(value){
    for(const [key,low,high]of [['update_ms',1,60000],['time_hold_seconds',0,59],['clear_every_minutes',1,60],['fade_after_seconds',0,86400],['fade_duration_seconds',1,86400]])if(!Number.isInteger(value[key])||value[key]<low||value[key]>high)throw new Error('Invalid cellular automata setting: '+key);
    if(60%value.clear_every_minutes||!['none','horizontal','vertical','both'].includes(value.gradient)||!['color','gradient_end_color'].every(key=>/^#[0-9a-f]{6}$/i.test(value[key])))throw new Error('Invalid cellular automata clear interval or colors');
    config=value;
  }
  if(config)configure(config);
  const states=new WeakMap();
  function create(){return {cells:new Uint8Array(4096),born:new Float64Array(4096),minute:-1,holdUntil:0};}
  function stamp(state,date){
    const clock=I.raster({islandPart:0,w:49,h:13,color:'#ffffff',secondary:'#ffffff',format:'12',speed:1},0,date);
    for(let y=0;y<13;y++)for(let x=0;x<49;x++){const i=(25+y)*64+7+x;state.cells[i]=clock[(y*49+x)*4+3]?1:0;state.born[i]=0;}
    state.minute=date.getHours()*60+date.getMinutes();
  }
  function step(state){
    const next=new Uint8Array(4096),cells=state.cells;
    for(let y=0;y<64;y++)for(let x=0;x<64;x++){
      let neighbors=0;
      for(let dy=-1;dy<=1;dy++)for(let dx=-1;dx<=1;dx++)if(dx||dy)neighbors+=cells[((y+dy+64)%64)*64+(x+dx+64)%64];
      const i=y*64+x;next[i]=neighbors===3||cells[i]&&neighbors===2?1:0;
    }
    state.cells=next;
  }
  function reset(w){states.delete(w);}
  function age(state,seconds){
    for(let i=0;i<4096;i++){
      if(!state.cells[i]){state.born[i]=0;continue;}
      if(!state.born[i])state.born[i]=seconds+1;
      if(seconds+1-state.born[i]>=config.fade_after_seconds+config.fade_duration_seconds){state.cells[i]=0;state.born[i]=0;}
    }
  }
  function advance(state,seconds,date){
    age(state,seconds);
    if(state.minute!==date.getHours()*60+date.getMinutes()){
      if(date.getMinutes()%config.clear_every_minutes===0){state.cells.fill(0);state.born.fill(0);}
      stamp(state,date);state.seconds=seconds;state.holdUntil=seconds+config.time_hold_seconds;
    }else if(seconds>=state.holdUntil&&seconds-state.seconds>=config.update_ms/1000){step(state);state.seconds=seconds;}
    age(state,seconds);
  }
  function cellColor(x,y,ageSeconds){
    const p=config.gradient==='horizontal'?x/63:config.gradient==='vertical'?y/63:config.gradient==='both'?(x+y)/126:0;
    const a=parseInt(config.color.slice(1),16),b=parseInt(config.gradient_end_color.slice(1),16);
    const light=Math.max(0,Math.min(1,1-(ageSeconds-config.fade_after_seconds)/config.fade_duration_seconds));
    const rgb=[16,8,0].map(shift=>Math.floor(Math.floor(((a>>shift)&255)*(1-p)+((b>>shift)&255)*p)*light));
    return [Math.round((rgb[0]>>3)*255/31),Math.round((rgb[1]>>2)*255/63),Math.round((rgb[2]>>3)*255/31),255];
  }
  function raster(w,seconds,date){
    let state=states.get(w);
    if(!state||seconds<state.seconds){state=create();state.seconds=seconds;states.set(w,state);}
    advance(state,seconds,date);
    const out=new Uint8ClampedArray(w.w*w.h*4);
    for(let y=0;y<w.h;y++)for(let x=0;x<w.w;x++){const sx=Math.floor((x+.5)*64/w.w),sy=Math.floor((y+.5)*64/w.h),i=sy*64+sx;if(state.cells[i])out.set(cellColor(sx,sy,seconds+1-state.born[i]),(y*w.w+x)*4);}
    return out;
  }
  const api={configure,create,stamp,step,age,advance,cellColor,reset,raster};if(typeof module!=='undefined'&&module.exports)module.exports=api;else root.CellularAutomata=api;
})(typeof globalThis!=='undefined'?globalThis:this);
