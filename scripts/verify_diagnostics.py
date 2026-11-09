"""Opt-in hardware check for Diagnostics and the BOOT face cycle."""
import json,re,time
from datetime import datetime,timedelta
from pathlib import Path
from verify_cellular_button import connect,request,capture,get,OUT,ROOT,save_png

def expected(text):
    font={ch:int(bits,16) for ch,bits in re.findall(r"\{'(.)', 0x([0-9a-f]+)\}",(ROOT/'main/ui/scene_font.h').read_text())}
    mask=[False]*320;x=(64-len(text)*4+1)//2
    for ch in text:
        for y in range(5):
            for dx in range(3):
                if font.get(ch,0)&(1<<(14-y*3-dx)):mask[y*64+x+dx]=True
        x+=4
    return mask

def band(rows,y):return [int(row[x:x+4],16)!=0 for row in rows[y:y+5] for x in range(0,256,4)]

def main():
    before=get('status');workspace=get('workspace')
    with connect('COM4') as link:
        try:
            request(link,'mode custom','MODE ')
            for name in ('automata','diagnostics'):
                request(link,'mode next','MODE ');assert 'mode='+name in request(link,'status','STATUS ')
            time.sleep(.1);status=request(link,'status','STATUS ');rows=capture(link)
            ip=re.search(r'url=http://([^/]+)/',request(link,'web','WEB '))[1]
            assert band(rows,1)==expected('DIAGNOSTICS')
            assert band(rows,16)==expected(ip)
            state=workspace['state'];face=next(f for f in state['faces'] if f['id']==state['selected'])
            location=face.get('weather',{}).get('location','')
            if not re.search(r'(?<!\d)\d{5}(?!\d)',location):
                face=next((f for f in state['faces'] if f['id']==state.get('defaultFaceId')),{});location=face.get('weather',{}).get('location','')
            found=re.search(r'(?<!\d)(\d{5})(?!\d)',location);postal=found[1] if found else '-----'
            assert band(rows,40)==expected('ZIP '+postal)
            local=datetime.fromisoformat(re.search(r' local=(\S+)',status)[1])
            assert any(band(rows,55)==expected((local+timedelta(seconds=i)).strftime('%I:%M:%S %p')) for i in range(3))
            uptime=int(re.search(r' uptime=(\d+)',status)[1])
            assert any(band(rows,31)==expected(f'{v//86400}D {v//3600%24:02}:{v//60%60:02}:{v%60:02}') for v in range(uptime,uptime+3))
            save_png(OUT/'diagnostics.png',rows)
            time.sleep(1.2);later=capture(link);assert band(later,55)!=band(rows,55);assert band(later,31)!=band(rows,31)
            request(link,'mode next','MODE ');assert 'mode=custom' in request(link,'status','STATUS ')
        finally:request(link,'mode custom','MODE ')
    assert get('workspace')==workspace
    after=get('status');assert after['sceneCrc']==before['sceneCrc']==after['savedSceneCrc']
    (OUT/'diagnostics-verification.json').write_text(json.dumps({'cycle':['Island','automata','diagnostics','Island'],'ip':ip,'postal':postal,'clockAndUptimeAdvance':True,'workspaceUnchanged':True},indent=2))
    print('PASS: complete button cycle, IP, ZIP '+postal+', live seconds, advancing uptime, saved Island preserved.')

if __name__=='__main__':main()
