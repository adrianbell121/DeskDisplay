"""Atomic shared workspace persistence with optimistic concurrency and recovery copies."""
import json,os,threading,time,hashlib
from pathlib import Path
ROOT=Path(__file__).resolve().parent.parent/'data'
LOCK=threading.Lock()
class Conflict(Exception):pass

def validate(state):
 if not isinstance(state,dict) or state.get('version')!=2:raise ValueError('Expected workspace version 2')
 faces=state.get('faces');library=state.get('library')
 if not isinstance(faces,list) or not 1<=len(faces)<=60 or not isinstance(library,list) or len(library)>128:raise ValueError('Invalid workspace collections')
 ids=set()
 for face in faces:
  if not isinstance(face,dict) or not isinstance(face.get('id'),str) or face['id'] in ids or not isinstance(face.get('widgets'),list) or len(face['widgets'])>32:raise ValueError('Invalid or duplicate face')
  ids.add(face['id'])
  for w in face['widgets']:
   if not isinstance(w,dict) or not isinstance(w.get('type'),str):raise ValueError('Invalid widget')
 return state

def read():
 p=ROOT/'workspace.json'
 if not p.exists():return {'revision':0,'state':None}
 return json.loads(p.read_text(encoding='utf-8'))

def recovery(state):
 validate(state);raw=json.dumps(state,sort_keys=True,ensure_ascii=False).encode('utf-8');folder=ROOT/'recovery';folder.mkdir(parents=True,exist_ok=True);p=folder/(hashlib.sha256(raw).hexdigest()+'.json')
 if not p.exists():p.write_bytes(raw)

def save(state,revision):
 validate(state)
 if not isinstance(revision,int):raise ValueError('Missing workspace revision')
 with LOCK:
  old=read()
  if old['revision']!=revision:raise Conflict('Workspace changed in another browser. Reload to receive it; your unsaved copy is kept locally.')
  ROOT.mkdir(parents=True,exist_ok=True)
  if old['state'] is not None:recovery(old['state'])
  result={'revision':revision+1,'savedAt':time.time(),'state':state};p=ROOT/'workspace.json';temp=ROOT/'workspace.tmp'
  with temp.open('w',encoding='utf-8') as f:json.dump(result,f,ensure_ascii=False);f.flush();os.fsync(f.fileno())
  os.replace(temp,p)
  return result
