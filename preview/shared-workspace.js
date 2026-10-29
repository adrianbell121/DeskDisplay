(async function(){
'use strict';
const Store=window.FaceStudioState,key='deskdisplay.widget-studio.v2';
let revision=0,state=null,pending=null,timer=null,running=false,blocked=false;
const status=text=>{const el=document.getElementById('save-status');if(el)el.textContent=text;};
async function get(){const r=await fetch('/api/workspace');if(!r.ok)throw new Error('Shared library is unavailable. Start the dashboard launcher and reload.');return r.json();}
async function post(path,body){return fetch(path,{method:'POST',headers:{'Content-Type':'application/json','X-Face-Lab':'1'},body:JSON.stringify(body)});}
async function flush(){if(running||blocked||!pending)return;running=true;const next=pending;pending=null;try{const r=await post('/api/workspace',{state:next,revision});const result=await r.json();if(!r.ok){if(r.status===409)blocked=true;throw new Error(result.error||'Shared save failed');}revision=result.revision;state=next;status('Saved to shared library');}catch(e){if(!pending)pending=next;status(e.message+' Browser recovery copy kept.');}finally{running=false;if(pending&&!blocked)timer=setTimeout(flush,2000);}}
try{
 const shared=await get();revision=shared.revision;state=shared.state;
 let local=null;try{const raw=localStorage.getItem(key);if(raw)local=Store.clean(JSON.parse(raw));}catch{}
 if(local&&state&&JSON.stringify(local)!==JSON.stringify(state)){const r=await post('/api/workspace/recovery',{state:local});if(!r.ok)throw new Error('Could not back up this browser’s existing designs. Reload to retry.');}
 if(!state){state=local||Store.fresh();const r=await post('/api/workspace',{revision,state});if(r.status===409){const latest=await get();state=latest.state;revision=latest.revision;}else{if(!r.ok)throw new Error('Could not initialize the shared library');revision=(await r.json()).revision;}}
 state=Store.clean(state);
 window.SharedWorkspace={get state(){return state;},save(value){if(!pending&&!running&&!blocked&&JSON.stringify(value)===JSON.stringify(state)){status('Saved to shared library');return;}pending=JSON.parse(JSON.stringify(value));status(blocked?'Another browser saved changes. Reload; local recovery copy kept.':'Saving shared library…');clearTimeout(timer);timer=setTimeout(flush,200);}};
 for(const source of ['app.js','designer.js','weather-panel.js'])await new Promise((resolve,reject)=>{const el=document.createElement('script');el.src=source;el.onload=resolve;el.onerror=reject;document.head.append(el);});
 if(!pending)status('Saved to shared library');
 setInterval(async()=>{if(pending||running||blocked||window.Studio?.sharedBusy)return;const active=document.activeElement;if(active&&(active.matches('input,textarea,select,[contenteditable=true]'))||document.querySelector('dialog[open]'))return;try{const latest=await get();if(latest.revision!==revision){window.Studio.loadShared(latest.state);revision=latest.revision;state=latest.state;status('Shared library updated from another browser');}}catch{status('Shared server unavailable; local recovery copy kept.');}},3000);
 window.addEventListener('beforeunload',()=>{if(pending)fetch('/api/workspace',{method:'POST',headers:{'Content-Type':'application/json','X-Face-Lab':'1'},body:JSON.stringify({revision,state:pending}),keepalive:true}).catch(()=>{});});
}catch(error){const box=document.createElement('div');box.style='padding:24px;background:#3d2424;color:white';box.textContent=error.message;document.body.prepend(box);}
})();
