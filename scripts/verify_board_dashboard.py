"""Opt-in hardware checks: temporarily recolor the date, restore it, and reboot."""
import gzip,json,re,subprocess,sys,time,zlib
from pathlib import Path
from urllib.request import Request,urlopen
from urllib.error import HTTPError
sys.path.insert(0,str(Path(__file__).resolve().parent));sys.path.insert(0,str(Path(__file__).resolve().parent.parent/'.tools/host-python/Lib/site-packages'))
from migrate_board_dashboard import request,ROOT
from device import connect,request as usb,save_png

def main():
 out=ROOT/'artifacts/board-dashboard';url=(out/'address.txt').read_text().strip();snapshot=request(url,'/api/workspace');state=snapshot['state'];face=next(f for f in state['faces'] if f['id']==state['selected'])
 report={'url':url,'faces':len(state['faces']),'widgets':len(state['library'])}
 with urlopen(url+'/',timeout=10) as response:
  html=gzip.decompress(response.read()).decode();assert 'id="ai-open"' not in html and 'window.BoardEditorBoot=' in html
 assert request(url,'/api/capabilities')['boardHosted']
 for headers,expected in [({'Origin':'http://unrelated.invalid','X-Face-Lab':'1'},403),({'Host':'unrelated.invalid'},403)]:
  try:urlopen(Request(url+'/api/workspace',data=b'{}',headers=headers),timeout=10)
  except HTTPError as error:assert error.code==expected
  else:raise AssertionError('Unrelated website could change the clock')
 for path,body,binary,expected in [('/api/scene',b'invalid',True,400),('/api/workspace',{'revision':snapshot['revision']+100,'state':state},False,409),('/api/workspace',{'revision':snapshot['revision'],'state':{'version':2}},False,400)]:
  try:request(url,path,body,binary)
  except HTTPError as error:assert error.code==expected
  else:raise AssertionError('Invalid edit accepted')
 assert request(url,'/api/workspace')==snapshot
 (out/'verify-face.json').write_text(json.dumps(face),encoding='utf-8')
 code="""const fs=require('fs'),S=require('./preview/scene.js'),f=JSON.parse(fs.readFileSync(process.argv[1],'utf8'));fs.writeFileSync(process.argv[2]+'/original.fls',S.compile(f));const d=f.widgets.find(w=>w.type==='island'&&w.islandPart===1);d.color=d.secondary='#ff0000';fs.writeFileSync(process.argv[2]+'/temporary.fls',S.compile(f));"""
 subprocess.run(['node','-e',code,str(out/'verify-face.json'),str(out)],cwd=ROOT,check=True)
 original=(out/'original.fls').read_bytes();changed=(out/'temporary.fls').read_bytes();date=next(w for w in face['widgets'] if w.get('islandPart')==1)
 menu=request(url,'/api/faces');assert menu['items'][0].get('scene')==original.hex(),'Open and publish the first menu face before running this check'
 with connect('COM4') as link:
  try:
   start=time.monotonic();request(url,'/api/scene',changed,True);elapsed=time.monotonic()-start;time.sleep(.1)
   assert request(url,'/api/status')['sceneCrc']==zlib.crc32(changed)
   rows=re.findall(r'(?m)^([0-9a-f]{256})\r?$',usb(link,'capture','FRAME END',timeout=12));assert len(rows)==64
   frame=[int(row[i:i+4],16) for row in rows for i in range(0,256,4)];assert any(frame[y*64+x]==0xf800 for y in range(date['y'],date['y']+date['h']) for x in range(64))
   save_png(out/'live-edit-test.png',rows,scale=6);report['liveApplySeconds']=round(elapsed,3)
  finally:request(url,'/api/faces/select',{'id':menu['items'][0]['id']})
  deadline=time.monotonic()+20
  while time.monotonic()<deadline:
   status=request(url,'/api/status')
   if not status['pending'] and status['sceneCrc']==status['savedSceneCrc']==zlib.crc32(original):break
   time.sleep(.5)
  else:raise AssertionError('Clock did not save restored face')
  usb(link,'restart','RESTARTING')
 deadline=time.monotonic()+45
 while time.monotonic()<deadline:
  try:
   status=request(url,'/api/status',timeout=2);restored=request(url,'/api/workspace',timeout=5)
   if restored==snapshot and status['sceneCrc']==zlib.crc32(original):break
  except Exception:pass
  time.sleep(1)
 else:raise AssertionError('Saved library or active face did not survive reboot')
 report.update(restartPersistence=True,originChecks=True,invalidEditChecks=True,status=status)
 assert request(url,'/api/faces')['items']==menu['items'],'Live preview changed the published menu'
 (out/'verification.json').write_text(json.dumps(report,indent=2),encoding='utf-8');print('PASS:',json.dumps(report))

if __name__=='__main__':main()
