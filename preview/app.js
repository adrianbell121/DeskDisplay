(function () {
  'use strict';
  const $=id=>document.getElementById(id), engine=window.FaceLab, workspaces=window.FaceLabWorkspace, storageKey='deskdisplay.face-lab.v1';
  let state={version:1,faces:workspaces.freshFaces(),selected:'island',sampleTime:'2026-10-23T10:08:00',clockMode:'fixed',advanceClock:false,view:'pixels',speed:1,brightness:100};
  let playing=true,elapsed=0,lastTick=null,lastPaint=-Infinity,filter='all',frames=[],toastTimer,storageOK=true;
  try{const saved=localStorage.getItem(storageKey);if(saved)state=workspaces.clean(JSON.parse(saved));}catch(error){storageOK=false;console.warn('Face Lab could not restore local storage:',error.message);}
  function toast(message){$('toast').textContent=message;$('toast').classList.add('visible');clearTimeout(toastTimer);toastTimer=setTimeout(()=>$('toast').classList.remove('visible'),3200);}
  function persist(){try{localStorage.setItem(storageKey,JSON.stringify(state));storageOK=true;}catch{storageOK=false;}$('save-status').textContent=storageOK?'Saved in this browser':'Browser storage unavailable · export to keep changes';}
  const selected=()=>state.faces.find(f=>f.id===state.selected);
  const makeId=()=>'v-'+Date.now().toString(36)+'-'+Math.random().toString(36).slice(2,7);
  const preset=face=>engine.presets.find(p=>p.id===face.base);
  function slug(name){return name.toLowerCase().replace(/[^a-z0-9]+/g,'-').replace(/^-|-$/g,'')||'face';}
  function download(blob,filename){const url=URL.createObjectURL(blob),a=document.createElement('a');a.href=url;a.download=filename;a.click();setTimeout(()=>URL.revokeObjectURL(url),2000);}
  function currentDate(){return state.clockMode==='live'?new Date():new Date(new Date(state.sampleTime).getTime()+(state.advanceClock?elapsed*1000:0));}
  function clockFields(){
    $('clock-mode').value=state.clockMode;$('sample-time').value=state.sampleTime;$('sample-time').disabled=state.clockMode==='live';$('advance-clock').checked=state.advanceClock;$('advance-clock').disabled=state.clockMode==='live';
  }
  function syncInspector(){
    const f=selected(),c=f.config;
    $('selected-title').textContent=f.name;$('selected-description').textContent=preset(f).description;$('design-name').value=f.name;
    for(const key of ['ink','accent','background'])$(key).value=c[key];
    $('digit-style').value=c.digitStyle;$('format').value=c.format;$('time-y').value=c.timeY;$('scene-y').value=c.sceneY;$('motion').value=c.motion;$('show-date').checked=c.showDate;$('show-seconds').checked=c.showSeconds;$('notes').value=f.notes;
    $('time-y-value').textContent=c.timeY+' px';$('scene-y-value').textContent=(c.sceneY>0?'+':'')+c.sceneY+' px';$('motion-value').textContent=c.motion.toFixed(1)+'×';$('delete').hidden=f.id===f.base;
  }
  function buildGallery(){
    const gallery=$('gallery');gallery.replaceChildren();frames=[];
    const visible=state.faces.filter(f=>filter!=='shortlist'||f.favorite);
    for(const f of visible){
      const card=document.createElement('article');card.className='face-card'+(state.selected===f.id?' selected':'');card.dataset.id=f.id;
      const open=document.createElement('button');open.className='face-open';open.setAttribute('aria-label','Edit '+f.name);open.setAttribute('aria-pressed',String(state.selected===f.id));
      const number=document.createElement('span');number.className='card-number';number.textContent=String(state.faces.indexOf(f)+1).padStart(2,'0');
      const screen=document.createElement('span');screen.className='screen';
      const canvas=document.createElement('canvas');canvas.width=64;canvas.height=64;canvas.setAttribute('aria-label',f.name+' animated clock preview');screen.append(canvas);open.append(number,screen);
      const meta=document.createElement('div');meta.className='face-meta';const labels=document.createElement('div'),name=document.createElement('p'),kind=document.createElement('p');
      name.className='face-name';name.textContent=f.name;kind.className='face-kind';kind.textContent=f.id===f.base?preset(f).kind:'Saved variation';labels.append(name,kind);
      const star=document.createElement('button');star.className='star';star.textContent=f.favorite?'★':'☆';star.setAttribute('aria-label',(f.favorite?'Remove ':'Shortlist ')+f.name+(f.favorite?' from shortlist':''));star.setAttribute('aria-pressed',String(f.favorite));
      star.addEventListener('click',()=>{f.favorite=!f.favorite;persist();buildGallery();paint();});
      open.addEventListener('click',()=>{state.selected=f.id;persist();syncInspector();for(const el of gallery.children){const active=el.dataset.id===f.id;el.classList.toggle('selected',active);el.querySelector('.face-open').setAttribute('aria-pressed',String(active));}});
      meta.append(labels,star);card.append(open,meta);gallery.append(card);
      frames.push({face:f,canvas,ctx:canvas.getContext('2d'),image:new ImageData(64,64)});
    }
    $('empty').hidden=visible.length!==0;$('all-count').textContent=state.faces.length;$('short-count').textContent=state.faces.filter(f=>f.favorite).length;
    $('all').setAttribute('aria-pressed',String(filter==='all'));$('shortlist').setAttribute('aria-pressed',String(filter==='shortlist'));
    gallery.classList.toggle('led',state.view==='led');applyBrightness();
  }
  function applyBrightness(){for(const f of frames)f.canvas.style.filter=`brightness(${state.brightness/100})`;}
  function paint(){
    const date=currentDate();
    for(const f of frames){engine.render(f.face.base,f.face.config,elapsed,date,f.image.data);f.ctx.putImageData(f.image,0,0);}
    const mins=Math.floor(elapsed/60),secs=(elapsed%60).toFixed(1).padStart(4,'0');$('elapsed').textContent=String(mins).padStart(2,'0')+':'+secs;
    $('timeline').value=Math.min(120,elapsed);
  }
  function setPlaying(value){playing=value;lastTick=null;$('play').textContent=playing?'Ⅱ Pause':'▶ Play';$('play').setAttribute('aria-label',playing?'Pause animation':'Play animation');}
  function updateConfig(key,value){const f=selected();f.config=engine.normalize({...f.config,[key]:value});persist();$('time-y-value').textContent=f.config.timeY+' px';$('scene-y-value').textContent=(f.config.sceneY>0?'+':'')+f.config.sceneY+' px';$('motion-value').textContent=f.config.motion.toFixed(1)+'×';paint();}
  for(const key of ['ink','accent','background'])$(key).addEventListener('input',e=>updateConfig(key,e.target.value));
  for(const [id,key] of [['digit-style','digitStyle'],['format','format']])$(id).addEventListener('change',e=>updateConfig(key,e.target.value));
  for(const [id,key] of [['time-y','timeY'],['scene-y','sceneY'],['motion','motion']])$(id).addEventListener('input',e=>updateConfig(key,Number(e.target.value)));
  for(const [id,key] of [['show-date','showDate'],['show-seconds','showSeconds']])$(id).addEventListener('change',e=>updateConfig(key,e.target.checked));
  $('design-name').addEventListener('input',e=>{const f=selected();f.name=e.target.value||preset(f).name;$('selected-title').textContent=f.name;const card=[...$('gallery').children].find(el=>el.dataset.id===f.id);if(card){card.querySelector('.face-name').textContent=f.name;card.querySelector('.face-open').setAttribute('aria-label','Edit '+f.name);card.querySelector('canvas').setAttribute('aria-label',f.name+' animated clock preview');const star=card.querySelector('.star');star.setAttribute('aria-label',f.favorite?'Remove '+f.name+' from shortlist':'Shortlist '+f.name);}persist();});
  $('notes').addEventListener('input',e=>{selected().notes=e.target.value;persist();});
  $('play').addEventListener('click',()=>setPlaying(!playing));
  $('step').addEventListener('click',()=>{setPlaying(false);elapsed+=1/30;paint();});
  $('restart').addEventListener('click',()=>{elapsed=0;lastTick=null;paint();});
  $('timeline').addEventListener('input',e=>{setPlaying(false);elapsed=Number(e.target.value);paint();});
  $('speed').value=state.speed;$('speed').addEventListener('change',e=>{state.speed=Number(e.target.value);persist();});
  $('clock-mode').addEventListener('change',e=>{state.clockMode=e.target.value;clockFields();persist();paint();});
  $('sample-time').addEventListener('change',e=>{if(e.target.value&&Number.isFinite(new Date(e.target.value).getTime())){state.sampleTime=e.target.value;persist();paint();}else clockFields();});
  $('advance-clock').addEventListener('change',e=>{state.advanceClock=e.target.checked;persist();paint();});
  $('view').value=state.view;$('view').addEventListener('change',e=>{state.view=e.target.value;$('gallery').classList.toggle('led',state.view==='led');persist();});
  $('brightness').value=state.brightness;$('brightness-value').textContent=state.brightness+'%';$('brightness').addEventListener('input',e=>{state.brightness=Number(e.target.value);$('brightness-value').textContent=state.brightness+'%';applyBrightness();persist();});
  for(const id of ['all','shortlist'])$(id).addEventListener('click',()=>{filter=id;buildGallery();paint();});
  $('show-all').addEventListener('click',()=>$('all').click());
  $('save-variation').addEventListener('click',()=>{
    if(state.faces.length>=60){toast('Workspace full. Export a copy before removing old variations.');return;}
    const f=selected(),id=makeId();
    const copy={...f,id,name:f.name.slice(0,48)+' variation',config:{...f.config},favorite:true};state.faces.push(copy);state.selected=id;persist();buildGallery();syncInspector();paint();toast('Variation saved and added to your shortlist.');
  });
  $('reset').addEventListener('click',()=>{const f=selected(),p=preset(f);f.config=engine.normalize({...engine.defaults,...p.config});persist();syncInspector();paint();toast('Layout, movement, and palette reset.');});
  $('delete').addEventListener('click',()=>{const f=selected();if(f.id===f.base)return;state.faces=state.faces.filter(item=>item.id!==f.id);state.selected=state.faces.some(item=>item.id===f.base)?f.base:state.faces[0].id;persist();buildGallery();syncInspector();paint();toast('Variation deleted.');});
  $('snapshot').addEventListener('click',()=>{
    const f=selected(),canvas=document.createElement('canvas');canvas.width=canvas.height=64;
    canvas.getContext('2d').putImageData(new ImageData(engine.render(f.base,f.config,elapsed,currentDate()),64,64),0,0);
    canvas.toBlob(blob=>{if(blob){download(blob,slug(f.name)+'-64x64.png');toast('Saved a native 64×64 PNG.');}else toast('Could not export this frame.');},'image/png');
  });
  function buildBrief(){const f=selected();return `DeskDisplay build brief\n\nDesign: ${f.name}\nBase concept: ${f.base}\nTarget: 64 x 64 HUB75, RGB565, LVGL 9.3\nPreview settings:\n${JSON.stringify(f.config,null,2)}\n\nReview notes: ${f.notes||'(none yet)'}\n\nSample date/time: ${currentDate().toString()}\nAnimation position: ${elapsed.toFixed(3)} seconds\n\nUse preview/engine.js for the selected scene and preview workspace JSON for settings. This is a concept preview, not an exact LVGL simulation. Check fonts, memory, frame rate, and appearance on the board during implementation.\n`;}
  $('copy-brief').addEventListener('click',async()=>{try{await navigator.clipboard.writeText(buildBrief());toast('Build brief copied. Paste it into your next design request.');}catch{download(new Blob([buildBrief()],{type:'text/plain'}),slug(selected().name)+'-brief.txt');toast('Clipboard unavailable; build brief downloaded instead.');}});
  $('export').addEventListener('click',()=>{download(new Blob([JSON.stringify({...state,exportedAt:new Date().toISOString()},null,2)],{type:'application/json'}),'deskdisplay-face-lab.json');toast('Workspace exported, including variations and notes.');});
  $('import').addEventListener('click',()=>$('import-file').click());
  $('import-file').addEventListener('change',async e=>{
    const file=e.target.files[0];if(!file)return;
    try{
      if(file.size>300000)throw new Error('This file is too large for a Face Lab workspace.');
      const result=workspaces.merge(state,JSON.parse(await file.text()),makeId);
      state=result.state;elapsed=0;lastTick=null;clockFields();$('speed').value=state.speed;$('view').value=state.view;$('brightness').value=state.brightness;$('brightness-value').textContent=state.brightness+'%';
      persist();buildGallery();syncInspector();paint();toast(result.additions?`Imported ${result.additions} face${result.additions===1?'':'s'}; your existing work is kept.`:'Workspace restored; these faces are already saved.');
    }catch(error){toast(error instanceof SyntaxError?'That file is not valid JSON.':error.message);}finally{e.target.value='';}
  });
  document.addEventListener('keydown',e=>{if(e.code==='Space'&&!['INPUT','TEXTAREA','SELECT','BUTTON','A'].includes(document.activeElement.tagName)){e.preventDefault();setPlaying(!playing);}});
  document.addEventListener('visibilitychange',()=>{lastTick=null;});
  function tick(now){
    if(lastTick!==null&&playing&&!document.hidden)elapsed+=Math.min((now-lastTick)/1000,.25)*state.speed;
    lastTick=now;
    if(now-lastPaint>=1000/30){if(playing||state.clockMode==='live')paint();lastPaint=now;}
    requestAnimationFrame(tick);
  }
  clockFields();buildGallery();syncInspector();paint();$('save-status').textContent=storageOK?'Saved in this browser':'Browser storage unavailable · export to keep changes';requestAnimationFrame(tick);
})();
