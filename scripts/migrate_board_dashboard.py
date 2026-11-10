"""Copy the desktop library to an empty board dashboard, retaining its active face."""
import argparse,json,shutil,subprocess,time,zlib
from pathlib import Path
from urllib.request import Request,urlopen
from urllib.parse import urlsplit
from ipaddress import ip_address

ROOT=Path(__file__).resolve().parent.parent

def request(url,path,body=None,binary=False,timeout=30):
 headers={'Origin':url,'X-Face-Lab':'1'}
 data=None
 if body is not None:
  data=body if binary else json.dumps(body,ensure_ascii=False).encode()
  headers['Content-Type']='application/octet-stream' if binary else 'application/json'
 with urlopen(Request(url+path,data=data,headers=headers),timeout=timeout) as response:return json.load(response)

def main():
 parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('url');args=parser.parse_args();url=args.url.rstrip('/')
 parsed=urlsplit(url)
 if parsed.scheme!='http' or not ip_address(parsed.hostname).is_private or parsed.path:raise SystemExit('Use the clock’s private IPv4 URL reported by the USB web command.')
 remote=request(url,'/api/workspace')
 if remote['state'] is not None:raise SystemExit('The clock already has a library. Export it before replacing or importing designs in its dashboard.')
 out=ROOT/'artifacts/board-dashboard';out.mkdir(parents=True,exist_ok=True)
 backup=out/('migration-source-'+str(int(time.time()))+'.json');shutil.copy2(ROOT/'data/workspace.json',backup)
 state=json.loads(backup.read_text(encoding='utf-8'))['state'];packs=out/'migration-packs';packs.mkdir(exist_ok=True)
 code="""const fs=require('fs'),S=require('./preview/scene.js'),state=JSON.parse(fs.readFileSync(process.argv[1],'utf8')).state;for(let i=0;i<state.faces.length;i++){try{fs.writeFileSync(process.argv[2]+'/'+i+'.fls',S.compile(S.cleanScene(state.faces[i])));}catch{}}"""
 subprocess.run(['node','-e',code,str(backup),str(packs)],cwd=ROOT,check=True)
 active=request(url,'/api/status')['sceneCrc'];matches=[]
 for i,face in enumerate(state['faces']):
  package=packs/(str(i)+'.fls')
  if package.exists() and zlib.crc32(package.read_bytes())==active:matches.append(i)
 if not matches:raise SystemExit('Could not match the board’s active face to the desktop library. No designs were changed.')
 index=next((i for i in matches if state['faces'][i]['id']==state['selected']),matches[0]);state['selected']=state['faces'][index]['id']
 revision=request(url,'/api/workspace',{'revision':remote['revision'],'state':state})['revision']
 package=(packs/(str(index)+'.fls')).read_bytes();menu=request(url,'/api/faces')
 item={'id':state['faces'][index]['id'],'name':state['faces'][index]['name'],'mode':'custom','scene':package.hex()}
 menu['items'][0]=item
 request(url,'/api/faces',{'revision':menu['revision'],'items':menu['items']})
 request(url,'/api/faces/select',{'id':item['id']})
 deadline=time.monotonic()+30
 while time.monotonic()<deadline:
  status=request(url,'/api/status')
  if status['savedRevision']==revision and status['savedSceneCrc']==zlib.crc32(package) and not status['pending']:break
  time.sleep(.5)
 else:raise RuntimeError('Library transferred, but flash save has not completed: '+str(status))
 restored=request(url,'/api/workspace');assert restored['state']==state,'Saved library does not match source'
 (out/'migrated-workspace.json').write_text(json.dumps(restored,ensure_ascii=False),encoding='utf-8');(out/'address.txt').write_text(url)
 (ROOT/'data/board-dashboard.url').write_text(url+'/',encoding='utf-8')
 print('Migrated and verified',len(state['faces']),'faces and',len(state['library']),'widgets. Active face:',state['faces'][index]['name'])

if __name__=='__main__':main()
