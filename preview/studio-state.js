(function(root){
  'use strict';
  const node=typeof module!=='undefined'&&module.exports,S=node?require('./scene.js'):root.FaceScene,E=node?require('./engine.js'):root.FaceLab;
  const D=node?require('./widget-data.js'):root.WidgetData;
  function fresh(){return {version:2,faces:E.presets.map(p=>S.fromLegacy({id:p.id,base:p.id,name:p.name,config:{...E.defaults,...p.config}})),library:S.clone(S.templates),selected:'island',sampleTime:'2026-10-23T10:08:00',clockMode:'fixed',advanceClock:false,view:'pixels',speed:1,brightness:100};}
  function clean(raw){
    if(!raw||![1,2].includes(raw.version)||!Array.isArray(raw.faces)||!raw.faces.length||raw.faces.length>60)throw new Error('Expected a Face Lab workspace with 1–60 faces.');
    const faces=raw.version===1?raw.faces.map(S.fromLegacy):raw.faces.map(S.cleanScene),ids=new Set();
    for(const f of faces){if(ids.has(f.id))throw new Error('Duplicate face IDs.');ids.add(f.id);}
    const library=raw.version===1?S.clone(S.templates):Array.isArray(raw.library)&&raw.library.length<=128?raw.library.map(S.cleanWidget):null;
    if(!library)throw new Error('A widget library can contain up to 128 widgets.');
    const libIds=new Set();for(const w of library){if(libIds.has(w.id))w.id=S.uid();libIds.add(w.id);}
    if(raw.catalogVersion!==3)for(const w of S.templates.filter(w=>['weather','forecast','calendar'].includes(w.type)))if(library.length<128&&!library.some(item=>item.type===w.type))library.push(S.clone(w));
    if(raw.catalogVersion!==4&&!faces.some(f=>f.id==='island-board-v1')&&faces.length<60)faces.push(S.boardIsland());
    if(raw.catalogVersion!==4)for(const w of S.templates.filter(w=>w.type==='island'))if(library.length<128&&!library.some(item=>item.type==='island'&&item.islandPart===w.islandPart))library.push(S.clone(w));
    const fallback=fresh();return {version:2,faces,library,selected:ids.has(raw.selected)?raw.selected:faces[0].id,sampleTime:typeof raw.sampleTime==='string'&&Number.isFinite(new Date(raw.sampleTime).getTime())?raw.sampleTime:fallback.sampleTime,clockMode:raw.clockMode==='live'?'live':'fixed',advanceClock:raw.advanceClock===true,view:raw.view==='led'?'led':'pixels',speed:[.25,.5,1,2,4].includes(raw.speed)?raw.speed:1,brightness:typeof raw.brightness==='number'&&Number.isFinite(raw.brightness)?Math.min(100,Math.max(15,raw.brightness)):100};
  }
  function merge(current,raw){
    const incoming=cleanWithData(raw),result=S.clone(current);let addedFaces=0,addedWidgets=0;
    const content=o=>JSON.stringify(o,(key,value)=>key==='id'?'':value);
    for(const w of incoming.library){if(result.library.some(p=>content(p)===content(w)))continue;if(result.library.length>=128)throw new Error('Library limit reached (128 widgets).');if(result.library.some(p=>p.id===w.id))w.id=S.uid();result.library.push(w);addedWidgets++;}
    for(const f of incoming.faces){const old=result.faces.find(p=>content(p)===content(f));if(old){if(f.id===incoming.selected)result.selected=old.id;continue;}if(result.faces.length>=60)throw new Error('Face limit reached (60 faces).');const original=f.id;if(result.faces.some(p=>p.id===f.id))f.id=S.uid();result.faces.push(f);if(original===incoming.selected)result.selected=f.id;addedFaces++;}
    for(const key of ['sampleTime','clockMode','advanceClock','view','speed','brightness','catalogVersion'])result[key]=incoming[key];if(incoming.previewData)result.previewData=incoming.previewData;return {state:result,addedFaces,addedWidgets};
  }
  function cleanWithData(raw){const result=clean(raw);result.catalogVersion=4;if(raw.previewData)result.previewData=D.clean(raw.previewData);return result;}
  const api={fresh:()=>({...fresh(),faces:[...fresh().faces,S.boardIsland()],selected:'island-board-v1',catalogVersion:4}),clean:cleanWithData,merge};if(node)module.exports=api;else root.FaceStudioState=api;
})(typeof globalThis!=='undefined'?globalThis:this);
