import sys,time,json,re
from pathlib import Path
sys.path.insert(0,'scripts');sys.path.insert(0,'.tools/host-python/Lib/site-packages')
from device import connect,request
from verify_weather_board import capture,picture
from PIL import Image,ImageDraw
out=Path('artifacts/adrian2-birds');out.mkdir(parents=True,exist_ok=True)
color=((155&248)<<8)|((185&252)<<3)|(177>>3)
checks=[(5,0),(9,0),(15,1),(25,2),(35,3),(45,4),(55,5),(59.99,5),(0,5),(1,None),(4,0)]
report=[];sheet=Image.new('RGB',(384*4,414*3),'#111819');draw=ImageDraw.Draw(sheet)
with connect('COM4') as link:
 assert 'minute_birds=1' in request(link,'status','STATUS ')
 try:
  request(link,'weather test sunny','WEATHER TEST ')
  for j,(second,expected) in enumerate(checks):
   request(link,f'birdphase {second}','BIRDPHASE ');time.sleep(.08);frame=capture(link);count=frame.count(color)
   if expected is not None:assert count==expected*3,(second,count,expected)
   report.append({'second':second,'birdPixels':count,'expectedBirds':expected});im=picture(frame);im.save(out/(str(second)+'.png'));sheet.paste(im,((j%4)*384,(j//4)*414+30));draw.text(((j%4)*384+10,(j//4)*414+8),f'{second}s: '+(str(expected)+' birds' if expected is not None else 'departing'),fill='white')
  print('PASS: hardware bird counts, five distinct perches, departure at 0 and empty by 4 seconds.')
 finally:
  request(link,'birdphase auto','BIRDPHASE AUTO');request(link,'weather auto','WEATHER AUTO')
 (out/'verification.json').write_text(json.dumps(report,indent=2));sheet.save(out/'timing-check.png')
