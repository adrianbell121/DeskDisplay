/* Workspace validation and lossless import merging, shared by the UI and tests. */
(function(root){
  'use strict';
  const engine=typeof module!=='undefined'&&module.exports?require('./engine.js'):root.FaceLab;
  const freshFaces=()=>engine.presets.map(p=>({id:p.id,base:p.id,name:p.name,config:engine.normalize({...engine.defaults,...p.config}),favorite:false,notes:''}));
  function clean(raw){
    if(!raw||raw.version!==1||!Array.isArray(raw.faces)||!raw.faces.length||raw.faces.length>60)throw new Error('Use a Face Lab workspace export (version 1, up to 60 faces).');
    const ids=new Set();
    const faces=raw.faces.map(f=>{
      if(!f||typeof f.id!=='string'||!/^[a-z0-9-]{1,80}$/.test(f.id)||ids.has(f.id)||!engine.presets.some(p=>p.id===f.base)||engine.presets.some(p=>p.id===f.id&&p.id!==f.base))throw new Error('This workspace contains an invalid or duplicate face.');
      ids.add(f.id);
      return {id:f.id,base:f.base,name:typeof f.name==='string'?f.name.slice(0,60)||'Untitled':f.base,config:engine.normalize(f.config),favorite:f.favorite===true,notes:typeof f.notes==='string'?f.notes.slice(0,2000):''};
    });
    for(const f of freshFaces())if(!ids.has(f.id)){
      if(faces.length>=60)throw new Error('Leave room for the six original faces (60 faces maximum).');
      faces.unshift(f);ids.add(f.id);
    }
    const sampleTime=typeof raw.sampleTime==='string'&&/^\d{4}-\d\d-\d\dT\d\d:\d\d(:\d\d)?$/.test(raw.sampleTime)&&Number.isFinite(new Date(raw.sampleTime).getTime())?raw.sampleTime:'2026-10-23T10:08:00';
    return {version:1,faces,selected:ids.has(raw.selected)?raw.selected:faces[0].id,sampleTime,clockMode:raw.clockMode==='live'?'live':'fixed',advanceClock:raw.advanceClock===true,view:raw.view==='led'?'led':'pixels',speed:[.25,.5,1,2,4].includes(raw.speed)?raw.speed:1,brightness:typeof raw.brightness==='number'&&Number.isFinite(raw.brightness)?Math.max(15,Math.min(100,raw.brightness)):100};
  }
  function merge(current,raw,makeId){
    const incoming=clean(raw),faces=current.faces.slice(),mapping=new Map();let additions=0;
    const content=f=>JSON.stringify([f.base,f.name,f.config,f.favorite,f.notes]);
    for(const f of incoming.faces){
      const identical=faces.find(item=>content(item)===content(f));
      if(identical){mapping.set(f.id,identical.id);continue;}
      if(faces.length>=60)throw new Error('Import would exceed 60 faces. Export and remove some variations first.');
      const id=faces.some(item=>item.id===f.id)?makeId():f.id;
      if(faces.some(item=>item.id===id))throw new Error('Could not create a unique variation. Try importing again.');
      mapping.set(f.id,id);faces.push({...f,id});additions++;
    }
    return {state:{...incoming,faces,selected:mapping.get(incoming.selected)||current.selected},additions};
  }
  const api={freshFaces,clean,merge};
  if(typeof module!=='undefined'&&module.exports)module.exports=api;else root.FaceLabWorkspace=api;
})(typeof globalThis!=='undefined'?globalThis:this);
