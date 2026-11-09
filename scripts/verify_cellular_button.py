"""Opt-in board check: Island toggle, clock-seeded Life, minute stamping and reboot."""
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

def clock(local):
    code="""const I=require('./preview/island-assets.js'),[h,m]=process.argv.slice(1).map(Number);
const a=I.raster({islandPart:0,w:49,h:13,color:'#ffffff',secondary:'#ffffff',format:'12',speed:1},0,new Date(2026,0,1,h,m));
console.log(JSON.stringify(Array.from({length:13},(_,y)=>Array.from({length:49},(_,x)=>a[(y*49+x)*4+3]?'1':'0').join(''))));"""
    return json.loads(subprocess.run(['node','-e',code,str(local.hour),str(local.minute)],cwd=ROOT,check=True,capture_output=True,text=True).stdout)

def cells(rows):return [int(row[i:i+4],16)!=0 for row in rows for i in range(0,256,4)]

def has_clock(rows,expected):
    grid=cells(rows)
    return all(grid[(25+y)*64+7+x]==(expected[y][x]=='1') for y in range(13) for x in range(49))

def step(grid):
    out=[False]*4096
    for y in range(64):
        for x in range(64):
            n=sum(grid[((y+dy)%64)*64+(x+dx)%64] for dy in (-1,0,1) for dx in (-1,0,1) if dx or dy)
            out[y*64+x]=n==3 or grid[y*64+x] and n==2
    return out

def outside(grid):return [v for i,v in enumerate(grid) if not (25<=i//64<38 and 7<=i%64<56)]

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
            local=datetime.fromisoformat(re.search(r' local=(\S+)',status)[1]);expected=clock(local)
            # mode next calls desk_ui_next(), exactly as the debounced BOOT handler does.
            assert 'MODE OK' in request(link,'mode next','MODE ')
            time.sleep(.04);first=capture(link);assert has_clock(first,expected),'Initial centered Island clock missing'
            assert not any(outside(cells(first))),'Initial seed contains cells outside the clock'
            save_png(OUT/'time-seed.png',first)
            time.sleep(3);assert capture(link)==first,'Clock changed during its five-second hold'
            time.sleep(2.3);evolved=capture(link);assert evolved!=first,'Clock pixels are not evolving after the hold'
            save_png(OUT/'time-evolved.png',evolved)
            status=request(link,'status','STATUS ');assert 'mode=automata' in status,status
            local=datetime.fromisoformat(re.search(r' local=(\S+)',status)[1]);next_minute=local.replace(second=0)+timedelta(minutes=1)
            expected=clock(next_minute);deadline=time.monotonic()+65;previous=evolved
            print('Checking the next natural minute stamp at '+next_minute.strftime('%I:%M %p')+'...',flush=True)
            while time.monotonic()<deadline:
                frame=capture(link)
                if has_clock(frame,expected):break
                previous=frame;time.sleep(.03)
            else:raise AssertionError('No centered clock stamp at the next minute')
            save_png(OUT/'minute-stamp.png',frame)
            candidate=cells(previous);target=outside(cells(frame))
            if next_minute.minute%CONFIG['clear_every_minutes']==0:
                assert not any(target),'Quarter-hour clear did not wipe outside cells'
                candidate=cells(frame)
            for _ in range(5):
                if outside(candidate)==target:break
                candidate=step(candidate)
            else:raise AssertionError('Minute stamp reset cells outside the clock')
            time.sleep(3);assert has_clock(capture(link),expected),'Minute stamp did not hold for five seconds'
            time.sleep(2.3);assert capture(link)!=frame,'Minute stamp did not resume evolution'
            print('PASS: centered time on entry, natural minute stamp, surrounding cells retained, and evolution resumed.',flush=True)
            assert 'MODE OK' in request(link,'mode next','MODE ')
            assert 'mode=diagnostics' in request(link,'status','STATUS ')
            assert 'MODE OK' in request(link,'mode next','MODE ')
            assert 'mode=custom' in request(link,'status','STATUS ')
            time.sleep(.1);save_png(OUT/'restored-island.png',capture(link))
        finally:
            request(link,'mode custom','MODE ')
        request(link,'restart','RESTARTING')
    time.sleep(4)
    with connect(args.port) as link:
        status=request(link,'status','STATUS ');assert 'mode=custom' in status,status
        print(status.strip())
    after=get('status');assert after['sceneCrc']==after['savedSceneCrc']==before['savedSceneCrc']
    assert get('workspace')==workspace
    report={'generationMs':CONFIG['update_ms'],'timeHoldSeconds':CONFIG['time_hold_seconds'],'cycle':'saved Island -> automata -> diagnostics -> saved Island','centeredTimeOnEntry':True,'naturalMinuteStamp':next_minute.isoformat(),'outsideCellsPreserved':True,'evolutionResumed':True,'savedIslandCrc':after['savedSceneCrc'],'workspaceUnchanged':True,'islandRestoredAfterReboot':True}
    (OUT/'verification.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    print('PASS:',json.dumps(report))

if __name__=='__main__':main()
