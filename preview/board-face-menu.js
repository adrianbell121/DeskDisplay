(function(root){
  'use strict';
  const modes=['custom','clock','island','automata','diagnostics'],limit=32,maxBytes=0x80000-16;
  const bytes=text=>new TextEncoder().encode(text).length;
  function clean(items){
    if(!Array.isArray(items)||!items.length||items.length>limit)throw new Error('Keep between 1 and 32 faces in the button menu.');
    const ids=new Set();let size=8;
    const result=items.map(raw=>{
      if(!raw||typeof raw.id!=='string'||!raw.id||bytes(raw.id)>80||raw.id.includes('\0')||ids.has(raw.id)||typeof raw.name!=='string'||!raw.name||bytes(raw.name)>240||raw.name.includes('\0')||!modes.includes(raw.mode))throw new Error('Invalid or duplicate board face.');
      ids.add(raw.id);const item={id:raw.id,name:raw.name,mode:raw.mode};
      if(item.mode==='custom'){
        if(typeof raw.scene!=='string'||raw.scene.length<16||raw.scene.length>524288||raw.scene.length%2||!/^464c5332[0-9a-f]*$/i.test(raw.scene))throw new Error('This face needs a valid live scene package.');
        item.scene=raw.scene.toLowerCase();
      }else if(raw.scene)throw new Error('Built-in faces do not contain a scene package.');
      size+=12+bytes(item.id)+bytes(item.name)+(item.scene?.length||0)/2;return item;
    });
    if(size>maxBytes)throw new Error('The board menu is full. Remove a face or reduce its animation size.');
    return result;
  }
  function reorder(items,id,target){
    const result=items.slice(),from=result.findIndex(f=>f.id===id),to=target===null?result.length-1:result.findIndex(f=>f.id===target);
    if(from<0||to<0)throw new Error('Face is no longer in the menu.');
    result.splice(to,0,result.splice(from,1)[0]);return result;
  }
  function upsert(items,item,replace=item.id){
    const result=items.slice(),index=result.findIndex(f=>f.id===replace);
    if(index<0)result.push(item);else result[index]=item;
    return clean(result);
  }
  function remove(items,id){return clean(items.filter(f=>f.id!==id));}
  const hex=data=>Array.from(data,n=>n.toString(16).padStart(2,'0')).join('');
  if(typeof module!=='undefined'&&module.exports){module.exports={clean,reorder,upsert,remove,hex,limit,maxBytes};return;}
  if(!root.BoardWorkspace)return;
  const B=root.BoardWorkspace,S=root.FaceScene,api=root.Studio;
  const panel=document.createElement('section');panel.id='board-face-menu';panel.setAttribute('aria-labelledby','board-face-menu-title');
  panel.innerHTML='<div class="board-menu-heading"><div><p class="eyebrow">SAVED ON THIS BOARD</p><h2 id="board-face-menu-title">BOOT button menu</h2><p>Drag faces into the order you want. The first face opens after a restart.</p></div><button id="board-menu-reload">Reload menu</button></div><p class="panel-hint">This is your playable menu. Removing a face here keeps its editable design in the library. Editor changes are live previews; add or update them here to keep them in the button cycle.</p><ol id="board-face-order" aria-label="Faces in button order"></ol><div class="board-menu-actions"><button id="board-face-add" class="primary">Add current face to button menu</button><label>Built-in face <select id="board-builtin"><option value="automata">Cellular automata</option><option value="diagnostics">Diagnostics</option><option value="island">Island</option><option value="clock">Ribbon clock</option></select></label><button id="board-builtin-add">Add built-in face</button></div><p id="board-menu-status" role="status">Loading board menu…</p>';
  document.querySelector('.studio-workspace').before(panel);
  const style=document.createElement('style');style.textContent='#board-face-menu{border:1px solid #385047;border-radius:12px;background:#18231e;padding:20px;margin:20px 0}#board-face-menu h2{margin:0 0 8px;font-size:22px}#board-face-menu p{line-height:1.5}.board-menu-heading,.board-menu-actions{display:flex;align-items:center;justify-content:space-between;gap:12px;flex-wrap:wrap}.board-menu-actions{justify-content:flex-start;margin-top:16px}.board-menu-actions label{display:flex;align-items:center;gap:8px}#board-face-order{display:grid;grid-template-columns:repeat(auto-fit,minmax(235px,1fr));gap:12px;padding:0;list-style:none;counter-reset:board-menu}.board-face-card{border:1px solid #425b4e;border-radius:9px;padding:14px;background:#101b16;cursor:grab}.board-face-card[aria-current=true]{border-color:#b5edca}.board-face-card.drag-over{outline:2px solid #b5edca}.board-face-card strong{display:block;overflow-wrap:anywhere;font-size:15px}.board-face-meta{font-size:12px;color:#b5cbbf;margin:8px 0}.board-face-controls{display:flex;gap:5px;flex-wrap:wrap}.board-face-controls button{padding:6px 9px;font-size:12px}#board-menu-status{margin-bottom:0;font-size:13px}';document.head.append(style);
  const list=panel.querySelector('#board-face-order'),note=panel.querySelector('#board-menu-status'),add=panel.querySelector('#board-face-add'),builtins=panel.querySelector('#board-builtin');
  let menu=null,busy=false,dragging=null;const links=new Map();
  const names={automata:'Cellular automata',diagnostics:'Diagnostics',island:'Island',clock:'Ribbon clock'};
  function button(text,action,label){const b=document.createElement('button');b.type='button';b.textContent=text;if(label)b.setAttribute('aria-label',label);b.disabled=busy;b.onclick=action;return b;}
  function linkLegacy(){
    for(const item of menu.items)if(item.id==='saved-scene'&&!links.has(item.id)){
      for(const face of api.state.faces)try{if(hex(S.compile(face))===item.scene){links.set(item.id,face.id);break;}}catch{}
    }
  }
  function render(){
    list.replaceChildren();
    if(menu)for(let i=0;i<menu.items.length;i++){
      const item=menu.items[i],row=document.createElement('li');row.className='board-face-card';row.style.touchAction='none';row.style.userSelect='none';row.dataset.faceId=item.id;row.setAttribute('aria-current',String(menu.activeIndex===i));
      const title=document.createElement('strong'),source=api.state.faces.find(f=>f.id===(links.get(item.id)||item.id));title.textContent=(i+1)+'. '+(item.id==='saved-scene'&&source?source.name:item.name);
      const meta=document.createElement('p');meta.className='board-face-meta';meta.textContent=[i===0?'Opens first':null,menu.activeIndex===i?'Showing now':null,item.mode==='custom'?'Saved design':'Built-in face'].filter(Boolean).join(' · ');
      const actions=document.createElement('div');actions.className='board-face-controls';
      actions.append(button('Show',()=>show(item.id),'Show '+item.name));
      if(source)actions.append(button('Edit',()=>api.openFace(source.id),'Edit '+source.name));
      const up=button('↑',()=>commit(reorder(menu.items,item.id,menu.items[i-1].id)),'Move '+item.name+' up');up.disabled=busy||i===0;
      const down=button('↓',()=>commit(reorder(menu.items,item.id,menu.items[i+1].id)),'Move '+item.name+' down');down.disabled=busy||i===menu.items.length-1;
      const del=button('Remove',()=>commit(remove(menu.items,item.id)),'Remove '+item.name+' from button menu');del.disabled=busy||menu.items.length===1;
      actions.append(up,down,del);row.append(title,meta,actions);list.append(row);
      row.onpointerdown=e=>{if(busy||e.button||e.target.closest('button'))return;e.preventDefault();dragging=item.id;row.setPointerCapture(e.pointerId);};
      row.onpointermove=e=>{if(!dragging)return;const over=document.elementFromPoint(e.clientX,e.clientY)?.closest('.board-face-card');for(const r of list.children)r.classList.toggle('drag-over',r===over&&r.dataset.faceId!==dragging);};
      row.onpointerup=e=>{if(!dragging)return;const id=dragging,over=document.elementFromPoint(e.clientX,e.clientY)?.closest('.board-face-card');dragging=null;for(const r of list.children)r.classList.remove('drag-over');if(!busy&&over?.parentElement===list&&over.dataset.faceId!==id)commit(reorder(menu.items,id,over.dataset.faceId));};
      row.onpointercancel=()=>{dragging=null;for(const r of list.children)r.classList.remove('drag-over');};
    }
    const f=api.face,existing=menu?.items.some(item=>item.id===f?.id||links.get(item.id)===f?.id);
    add.textContent=(existing?'Update ':'Add ')+(f?.name||'current face')+(existing?' on board':' to button menu');add.disabled=busy||!menu||!f;
    panel.querySelector('#board-menu-reload').disabled=busy;panel.querySelector('#board-builtin-add').disabled=busy||!menu;builtins.disabled=busy;
  }
  async function load(){const next=await B.get('/api/faces');next.items=clean(next.items);menu=next;linkLegacy();render();return next;}
  async function commit(items){
    if(busy||!menu)return;
    busy=true;render();note.textContent='Saving button menu…';
    try{
      items=clean(items);await B.beginMenu();const result=await B.post('/api/faces',{revision:menu.revision,items});
      menu={...menu,...result,items};B.resetPreview();note.textContent='Saved on board · '+items.length+' faces in button order';
    }catch(error){note.textContent=error.message+' The saved menu was kept. Reload the menu before retrying.';}
    finally{B.endMenu();busy=false;render();}
  }
  async function show(id){
    if(busy)return;busy=true;render();
    try{await B.beginMenu();const result=await B.post('/api/faces/select',{id});menu.activeIndex=result.activeIndex;B.resetPreview();note.textContent='Showing the saved face on the board';}
    catch(error){note.textContent=error.message;}
    finally{B.endMenu();busy=false;render();}
  }
  add.onclick=()=>{
    try{const f=api.face,item={id:f.id,name:f.name,mode:'custom',scene:hex(S.compile(f))},old=menu.items.find(e=>e.id===f.id||links.get(e.id)===f.id);commit(upsert(menu.items,item,old?.id||f.id));}
    catch(error){note.textContent=error.message;}
  };
  panel.querySelector('#board-builtin-add').onclick=()=>{try{const mode=builtins.value;commit(upsert(menu.items,{id:'builtin:'+mode,name:names[mode],mode}));}catch(error){note.textContent=error.message;}};
  panel.querySelector('#board-menu-reload').onclick=()=>load().then(()=>note.textContent='Board menu reloaded').catch(error=>note.textContent=error.message);
  root.addEventListener('studio:refresh',()=>{if(!busy&&!dragging)render();});
  root.BoardFaceMenu={get busy(){return busy;},async sync(info){
    if(busy||dragging||!menu)return;
    if(info.faceMenuRevision!==menu.revision){await load();note.textContent='Board menu updated';}
    else if(info.faceMenuIndex!==menu.activeIndex){menu.activeIndex=info.faceMenuIndex;render();}
  }};
  load().then(()=>note.textContent='Loaded from board · '+menu.items.length+' faces. Use Show to display a saved face.').catch(error=>{note.textContent=error.message;render();});
})(typeof globalThis!=='undefined'?globalThis:this);
