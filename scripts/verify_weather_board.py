"""Cycle the uploaded weather scene on real hardware and save captured animations."""
import sys,time,re,json
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parent))
sys.path.insert(0,str(Path(__file__).resolve().parent.parent/'.tools/host-python/Lib/site-packages'))
from device import connect,request
from PIL import Image,ImageDraw
ROOT=Path(__file__).resolve().parent.parent
OUT=ROOT/'artifacts/weather-board';OUT.mkdir(parents=True,exist_ok=True)
def rgb(r,g,b):return ((r&248)<<8)|((g&252)<<3)|(b>>3)
def capture_once(link):
 text=request(link,'capture rle','FRAME END',timeout=8)
 rows=[]
 for line in text.splitlines():
  line=line.strip()
  if not line or len(line)%6 or not re.fullmatch('[0-9a-f]+',line):continue
  row=[]
  for i in range(0,len(line),6):row.extend([int(line[i+2:i+6],16)]*int(line[i:i+2],16))
  if len(row)==64:rows.append(row)
 if len(rows)!=64:(OUT/'incomplete-frame.txt').write_text(text)
 assert len(rows)==64,f'Incomplete frame: {len(rows)} rows'
 return [v for row in rows for v in row]
def capture(link):
 for attempt in range(3):
  try:return capture_once(link)
  except AssertionError:
   if attempt==2:raise
 raise AssertionError('No complete frame')
def picture(values):
 im=Image.new('RGB',(64,64));im.putdata([((v>>11)*255//31,((v>>5)&63)*255//63,(v&31)*255//31) for v in values]);return im.resize((384,384),Image.Resampling.NEAREST)
def main():
 report={};contact=Image.new('RGB',(3*384,2*414),'#111819');draw=ImageDraw.Draw(contact)
 names=['sunny','partly-cloudy','cloudy','rainy','storm','snowy']
 with connect(None) as link:
  initial_status=request(link,'status','STATUS ')
  assert 'mode=custom' in initial_status
  start_frames=int(re.search(r'frames=(\d+)',initial_status)[1]);start_uptime=int(re.search(r'uptime=(\d+)',initial_status)[1])
  try:
   for index,name in enumerate(names):
    assert 'WEATHER TEST '+name in request(link,'weather test '+name,'WEATHER TEST ')
    time.sleep(.25)
    frames=[];raw=[];timestamps=[]
    for n in range(24):
     values=capture(link);raw.append(values);timestamps.append(time.monotonic());frames.append(picture(values));time.sleep(.06)
    if name in ['sunny','partly-cloudy']:assert any(rgb(255,211,106) in frame for frame in raw),'Sun absent'
    if name in ['cloudy','partly-cloudy','snowy']:assert any(rgb(168,189,196) in frame for frame in raw),'Clouds absent'
    if name in ['rainy','storm']:
     counts=[f.count(rgb(118,169,197)) for f in raw];assert min(counts)>0,'Rain absent';assert len({tuple(i for i,v in enumerate(f) if v==rgb(118,169,197)) for f in raw})>5,'Rain frozen'
    if name=='snowy':assert min(f.count(65535) for f in raw)>0,'Snow absent';assert len({tuple(i for i,v in enumerate(f) if v==65535) for f in raw})>5,'Snow frozen'
    frames[12].save(OUT/(name+'.png'));frames[0].save(OUT/(name+'.gif'),save_all=True,append_images=frames[1:],duration=[max(30,int((timestamps[min(i+1,len(timestamps)-1)]-timestamps[max(0,i-1)])/ (2 if 0<i<len(timestamps)-1 else 1)*1000)) for i in range(len(timestamps))],loop=0)
    contact.paste(frames[12],((index%3)*384,(index//3)*414+30));draw.text(((index%3)*384+12,(index//3)*414+8),name,fill='white')
    report[name]={'frames':len(frames),'distinctFrames':len({tuple(f) for f in raw})};print('PASS',name,report[name],flush=True)
    if name=='storm':
     request(link,'weather strike','WEATHER STRIKE');time.sleep(.045);lit=[]
     for i in range(4):lit.append(capture(link))
     assert any(rgb(255,246,202) in f for f in lit),'Lightning not captured'
     picture(next(f for f in lit if rgb(255,246,202) in f)).save(OUT/'lightning.png')
   # Let one real scheduled strike occur with the original 60–120 second settings.
   request(link,'weather test storm','WEATHER TEST ')
   before=request(link,'weather status','WEATHER STATUS ');initial=int(re.search(r'natural_strikes=(\d+)',before)[1]);deadline=time.monotonic()+125
   while True:
    status=request(link,'weather status','WEATHER STATUS ')
    if int(re.search(r'natural_strikes=(\d+)',status)[1])>initial:break
    assert time.monotonic()<deadline,'No scheduled lightning within two minutes'
    time.sleep(1)
   report['scheduledLightning']=status.strip();print('PASS scheduled lightning',status.strip(),flush=True)
  finally:
   print(request(link,'weather auto','WEATHER AUTO'),flush=True)
  deadline=time.monotonic()+90
  while True:
   status=request(link,'weather status','WEATHER STATUS ')
   weather_line=next((line.strip() for line in status.splitlines() if line.startswith('WEATHER STATUS ')),'')
   if re.search(r'(?<![\w])syncs=[1-9]',weather_line):status=weather_line;break
   assert time.monotonic()<deadline,'Board did not fetch live conditions'
   time.sleep(2)
  report['liveWeather']=status.strip();report['board']=request(link,'status','STATUS ').strip();end_frames=int(re.search(r'frames=(\d+)',report['board'])[1]);end_uptime=int(re.search(r'uptime=(\d+)',report['board'])[1]);assert end_uptime>start_uptime,'Board restarted during verification';report['renderFPS']=round((end_frames-start_frames)/(end_uptime-start_uptime),1);assert report['renderFPS']>=20,'Animation rendering below 20 frames/sec';report['liveWeatherVerified']=True;print('PASS live weather',status.strip(),flush=True)
 contact.save(OUT/'all-conditions.png');(OUT/'verification.json').write_text(json.dumps(report,indent=2));print('Saved captures and report to',OUT,flush=True)
if __name__=='__main__':main()
