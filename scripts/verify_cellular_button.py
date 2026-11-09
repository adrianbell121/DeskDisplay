"""Opt-in board check: Island toggle, reverse-time overlay, minute hold, merge and reboot."""
import argparse,json,re,subprocess,sys,time,urllib.request
from datetime import datetime,timedelta
from pathlib import Path

ROOT=Path(__file__).resolve().parent.parent
sys.path.insert(0,str(ROOT/'.tools/host-python/Lib/site-packages'))
from device import connect,request,save_png
OUT=ROOT/'artifacts/automata-button'
CONFIG=json.loads((ROOT/'cellular-automata-config.json').read_text(encoding='utf-8'))

def get(name):
    url=(ROOT/'data/board-dashboard.url').read_text().strip().rstrip('/')
    with urllib.request.urlopen(url+'/api/'+name,timeout=8) as response:return json.load(response)

def capture(link):
    for _ in range(3):
        text=request(link,'capture rle','FRAME END',timeout=12);rows=[]
        for line in text.splitlines():
            line=line.strip()
            if not line or len(line)%6 or not re.fullmatch('[0-9a-f]+',line):continue
            row=''.join(line[i+2:i+6]*int(line[i:i+2],16) for i in range(0,len(line),6))
            if len(row)==256:rows.append(row)
        if len(rows)==64:return rows
    raise AssertionError('Incomplete framebuffer capture')

def clock_frames(local):
    code="const C=require('./preview/cellular-automata.js'),[h,m]=process.argv.slice(1).map(Number);console.log(JSON.stringify(C.clockFrames(new Date(2026,0,1,h,m)).map(f=>Array.from(f,(_,i)=>f[i]?i:-1).filter(i=>i>=0))));"
    return json.loads(subprocess.run(['node','-e',code,str(local.hour),str(local.minute)],cwd=ROOT,check=True,capture_output=True,text=True).stdout)

def raw_cells(rows):return [int(row[i:i+4],16) for row in rows for i in range(0,256,4)]

def full_color(i):
    x,y=i%64,i//64;direction=CONFIG['gradient']
    pos=x if direction=='horizontal' else y if direction=='vertical' else x+y if direction=='both' else 0
    extent=126 if direction=='both' else 63
    a=int(CONFIG['color'][1:],16);b=int(CONFIG['gradient_end_color'][1:],16)
    r,g,blue=[(((a>>n)&255)*(extent-pos)+((b>>n)&255)*pos)//extent for n in (16,8,0)]
    return ((r&248)<<8)|((g&252)<<3)|(blue>>3)

def has_overlay(values,mask):return bool(mask) and all(values[i]==full_color(i) for i in mask)

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port',default='COM4');parser.add_argument('--backup',action='store_true');args=parser.parse_args()
    OUT.mkdir(parents=True,exist_ok=True)
    if args.backup:
        for name in ('status','workspace'):(OUT/('before-'+name+'.json')).write_text(json.dumps(get(name),indent=2),encoding='utf-8')
        with connect(args.port) as link:
            status=request(link,'status','STATUS ')
            (OUT/'before-status.txt').write_text(status,encoding='utf-8');save_png(OUT/'before-display.png',capture(link))
        print('Backed up the saved Island workspace and captured the current display.');return
    before=json.loads((OUT/'before-status.json').read_text(encoding='utf-8'))
    workspace=json.loads((OUT/'before-workspace.json').read_text(encoding='utf-8'))
    after=get('status');assert after['sceneCrc']==after['savedSceneCrc']==before['savedSceneCrc'],(before,after)
    assert get('workspace')==workspace,'Saved Island workspace changed'
    with connect(args.port) as link:
        status=request(link,'status','STATUS ');assert 'mode=custom' in status and 'cellular_automata=1' in status,status
        try:
            request(link,'mode next','MODE ')
            status=request(link,'status','STATUS ');assert 'mode=automata' in status,status
            utc=int(re.search(r' utc=(\d+)',status)[1]);flip=(utc//60+1)*60
            local=datetime.fromisoformat(re.search(r' local=(\S+)',status)[1]);next_minute=local.replace(second=0)+timedelta(minutes=1)
            frames=clock_frames(next_minute);seen=set();held=False;after=False
            lead=CONFIG['time_reverse_frames']*CONFIG['update_ms']/1000;hold=CONFIG['time_hold_seconds']
            print(f'Checking {lead:g}s reverse lead-in, minute flip at {next_minute:%I:%M %p}, {hold}s hold and merge...',flush=True)
            deadline=time.monotonic()+70+hold
            while time.monotonic()<deadline:
                rows=capture(link);values=raw_cells(rows)
                status=request(link,'status','STATUS ');utc=int(re.search(r' utc=(\d+)',status)[1])
                if flip-lead<=utc<flip:
                    for i in range(1,len(frames)):
                        if i not in seen and has_overlay(values,frames[i]):
                            seen.add(i);save_png(OUT/f'reverse-{i}.png',rows)
                elif flip<=utc<flip+hold:
                    if utc==flip and not has_overlay(values,frames[0]):continue
                    assert has_overlay(values,frames[0]),'Completed clock missing during overlay hold'
                    lit=set(frames[0]);assert all(values[y*64+x]==0 for y in range(25,38) for x in range(7,56) if y*64+x not in lit),'Clock backing did not mask the underlying game'
                    if not held:save_png(OUT/'minute-overlay.png',rows);held=True
                elif utc>=flip+hold+2:
                    assert not has_overlay(values,frames[0]),'Time overlay did not release into the game'
                    save_png(OUT/'after-merge.png',rows);after=True;break
                time.sleep(.02)
            assert after,'Overlay cycle timed out'
            if hold:assert held,'Clock hold was not captured'
            unique={tuple(mask) for mask in frames[1:] if mask}
            assert len(seen)>=min(3,len(unique)),('Reverse frames were not observed',seen)
            print('PASS: reverse animation, clean clock at minute change, overlay hold and game after merge.',flush=True)
            request(link,'mode next','MODE ');assert 'mode=diagnostics' in request(link,'status','STATUS ')
            request(link,'mode next','MODE ');assert 'mode=custom' in request(link,'status','STATUS ')
            save_png(OUT/'restored-island.png',capture(link))
        finally:request(link,'mode custom','MODE ')
        request(link,'restart','RESTARTING')
    time.sleep(4)
    with connect(args.port) as link:
        status=request(link,'status','STATUS ');assert 'mode=custom' in status,status
        print(status.strip())
    after=get('status');assert after['sceneCrc']==after['savedSceneCrc']==before['savedSceneCrc']
    assert get('workspace')==workspace
    report={'generationMs':CONFIG['update_ms'],'reverseFrames':CONFIG['time_reverse_frames'],'observedReverseFrames':sorted(seen,reverse=True),'timeHoldSeconds':hold,'minute':next_minute.isoformat(),'mergeReleased':True,'workspaceUnchanged':True,'islandRestoredAfterReboot':True}
    (OUT/'verification.json').write_text(json.dumps(report,indent=2),encoding='utf-8');print('PASS:',json.dumps(report))

if __name__=='__main__':main()
