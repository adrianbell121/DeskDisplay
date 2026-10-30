"""Accelerated sun/moon checks using fetched epochs; never changes the system clock."""
import sys,time,json,math,re
from pathlib import Path
sys.path.insert(0,'scripts');sys.path.insert(0,'.tools/host-python/Lib/site-packages')
from device import connect,request
from verify_weather_board import capture,picture
from PIL import Image,ImageDraw

out=Path('artifacts/solar-cycle');out.mkdir(parents=True,exist_ok=True)
def sky(link):
 result=request(link,'weather sky','SOLAR {')
 return json.loads(next(line[6:] for line in result.splitlines() if line.startswith('SOLAR {')))
def rgb565(value):
 r,g,b=bytes.fromhex(value[1:]);return ((r&248)<<8)|((g&252)<<3)|(b>>3)
state=json.loads(Path('data/workspace.json').read_text(encoding='utf-8'))['state'];face=next(f for f in state['faces'] if f['name']=='Island — Adrian2');sun=next(w for w in face['widgets'] if w['type']=='weatherSun')
colors={rgb565(sun['color']),rgb565(sun['secondary']),rgb565(face['weather']['moonColor']),rgb565(face['weather']['moonAccent'])}
report={'checks':[]}
with connect('COM4') as link:
 status=request(link,'status','STATUS ');assert 'solar_cycle=1' in status,status
 deadline=time.monotonic()+90
 while True:
  data=sky(link)
  if len(data['days'])>=3:break
  if time.monotonic()>deadline:raise RuntimeError('Board did not fetch dated sunrise/sunset times over Wi-Fi.')
  time.sleep(2)
 report['fetchedSolarDays']=data['days'];report['weatherStatus']=request(link,'weather','WEATHER STATUS ')
 days=data['days'];d=days[1];rise,setting=d['sunrise'],d['sunset'];night_end=days[2]['sunrise']
 checks=[('Sunrise',rise,'sun'),('Morning',rise+(setting-rise)//4,'sun'),('Day peak',(rise+setting)//2,'sun'),('Just before sunset',setting-1,'sun'),('Sunset',setting,'moon'),('Night peak',(setting+night_end)//2,'moon'),('Before dawn',night_end-1,'moon'),('Next sunrise',night_end,'sun')]
 sheet=Image.new('RGB',(384*4,420*2),'#111819');draw=ImageDraw.Draw(sheet)
 try:
  request(link,'weather test sunny','WEATHER TEST ');request(link,'birdphase 55','BIRDPHASE ')
  for i,(name,epoch,body) in enumerate(checks):
   request(link,f'weather skytime {epoch}','SOLAR TIME ');time.sleep(.12);phase=sky(link);assert phase['body']==body,(name,phase)
   frame=capture(link);points=[(p%64,p//64) for p,c in enumerate(frame) if c in colors];assert points and all(y<55 for x,y in points),(name,points)
   if name in ('Sunrise','Just before sunset','Sunset','Before dawn','Next sunrise'):assert min(y for x,y in points)==50,(name,points)
   if name in ('Day peak','Night peak'):assert points and min(y for x,y in points)==30,(name,points)
   if body=='sun':assert not any(c in {rgb565(face['weather']['moonColor']),rgb565(face['weather']['moonAccent'])} for c in frame)
   else:assert not any(c in {rgb565(sun['color']),rgb565(sun['secondary'])} for c in frame)
   report['checks'].append({'name':name,'epoch':epoch,'phase':phase,'visibleSkyPixels':len(points),'bounds':[min(points),max(points)] if points else None})
   image=picture(frame);image.save(out/(str(i)+'-'+body+'.png'));sheet.paste(image,((i%4)*384,(i//4)*420+36));draw.text(((i%4)*384+8,(i//4)*420+10),name+' - '+body,fill='white')
  report['status']=request(link,'status','STATUS ')
 finally:
  request(link,'weather skytime auto','SOLAR TIME ');request(link,'weather auto','WEATHER AUTO');request(link,'birdphase auto','BIRDPHASE AUTO')
 report['restored']=sky(link)
 assert not report['restored']['override']
 sheet.save(out/'board-cycle.png');(out/'verification.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
 print('PASS: live solar fetch; dawn/day/dusk/night transitions; fixed peak at row 30; sky hidden below row 55; automatic time and weather restored.')
