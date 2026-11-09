(function(root){
  'use strict';
  const I=typeof module!=='undefined'&&module.exports?require('./island-assets.js'):root.IslandAssets;
  let config=typeof module!=='undefined'&&module.exports?require('../cellular-automata-config.json'):null;
  function configure(value){
    for(const [key,low,high]of [['time_reverse_frames',0,120],['update_ms',1,60000],['time_hold_seconds',0,59],['clear_every_minutes',1,60],['fade_after_seconds',0,86400],['fade_duration_seconds',1,86400]])if(!Number.isInteger(value[key])||value[key]<low||value[key]>high)throw new Error('Invalid cellular automata setting: '+key);
    if(value.time_reverse_frames*value.update_ms+value.time_hold_seconds*1000>60000)throw new Error('Time animation lead-in plus hold must fit within one minute');
    if(60%value.clear_every_minutes||!['none','horizontal','vertical','both'].includes(value.gradient)||!['color','gradient_end_color'].every(key=>/^#[0-9a-f]{6}$/i.test(value[key])))throw new Error('Invalid cellular automata clear interval or colors');
    config=value;
  }
  if(config)configure(config);
  const states=new WeakMap();
  function create(){return {cells:new Uint8Array(4096),born:new Float64Array(4096),minuteMs:-1,overlayAt:-1,pending:false,seconds:0};}
  function clockCells(date){
    const clock=I.raster({islandPart:0,w:49,h:13,color:'#ffffff',secondary:'#ffffff',format:'12',speed:1},0,date);
    const cells=new Uint8Array(4096);
    for(let y=0;y<13;y++)for(let x=0;x<49;x++)cells[(25+y)*64+7+x]=clock[(y*49+x)*4+3]?1:0;
    return cells;
  }
  function clockFrames(date){
    const frames=[clockCells(date)],state={cells:frames[0]};
    for(let i=1;i<=config.time_reverse_frames;i++){step(state);frames.push(state.cells);}
    return frames;
  }
  function merge(state,cells,seconds){
    for(let i=0;i<4096;i++)if(cells[i]){if(!state.cells[i])state.born[i]=seconds+1;state.cells[i]=1;}
  }
  function target(wall){
    const minute=Math.floor(wall/60000)*60000,phase=wall-minute;
    if(phase<config.time_hold_seconds*1000)return minute;
    if(config.time_reverse_frames&&60000-phase<=config.time_reverse_frames*config.update_ms)return minute+60000;
    return -1;
  }
  function overlay(state,date){
    if(!state.pending)return null;
    const index=Math.max(0,Math.min(config.time_reverse_frames,Math.ceil((state.overlayAt-date.getTime())/config.update_ms)));
    return state.frames[index];
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
    const wall=date.getTime(),minute=Math.floor(wall/60000)*60000,initial=state.minuteMs<0,changed=state.minuteMs!==minute;
    if(changed){
      if(date.getMinutes()%config.clear_every_minutes===0){state.cells.fill(0);state.born.fill(0);}
      state.minuteMs=minute;
    }
    if(seconds-state.seconds>=config.update_ms/1000){step(state);state.seconds=seconds;}
    if(state.pending&&(Math.floor(state.overlayAt/60000)<minute/60000||Math.floor(state.overlayAt/60000)>minute/60000+1)){state.pending=false;state.overlayAt=-1;}
    if(state.pending&&wall>=state.overlayAt+config.time_hold_seconds*1000){merge(state,state.frames[0],seconds);state.pending=false;}
    let next=target(wall);
    if(next<0&&initial)next=wall;
    else if(next<0&&changed&&state.overlayAt!==minute)next=minute;
    if(next>=0&&next!==state.overlayAt){
      if(state.pending)merge(state,state.frames[0],seconds);
      state.frames=clockFrames(new Date(next));state.overlayAt=next;state.pending=true;
    }
    if(state.pending&&wall>=state.overlayAt+config.time_hold_seconds*1000){merge(state,state.frames[0],seconds);state.pending=false;}
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
    const top=overlay(state,date);
    const out=new Uint8ClampedArray(w.w*w.h*4);
    for(let y=0;y<w.h;y++)for(let x=0;x<w.w;x++){
      const sx=Math.floor((x+.5)*64/w.w),sy=Math.floor((y+.5)*64/w.h),i=sy*64+sx,covered=top&&sx>=7&&sx<56&&sy>=25&&sy<38;
      if(top?.[i]||state.cells[i]&&!covered)out.set(cellColor(sx,sy,top?.[i]?0:seconds+1-state.born[i]),(y*w.w+x)*4);
      else if(covered)out.set([0,0,0,255],(y*w.w+x)*4);
    }
    return out;
  }
  const api={configure,create,clockCells,clockFrames,merge,target,overlay,step,age,advance,cellColor,reset,raster};if(typeof module!=='undefined'&&module.exports)module.exports=api;else root.CellularAutomata=api;
})(typeof globalThis!=='undefined'?globalThis:this);
