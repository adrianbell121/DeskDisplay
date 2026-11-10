"""Opt-in board check: four-face cycle, reverse frames, minute hold, clear, and reboot."""
import json, math, re, subprocess, time
from datetime import datetime, timezone
from pathlib import Path
from migrate_board_dashboard import request as http
from verify_cellular_button import connect, request, capture, raw_cells, full_color, save_png, ROOT, CONFIG

OUT=ROOT/'artifacts/reverse-clock'

def frames(local, count):
    code="const C=require('./preview/cellular-automata.js'),[h,m,n]=process.argv.slice(1).map(Number),s={cells:C.clockCells(new Date(2026,0,1,h,m))},out=[];for(let i=0;i<=n;i++){out.push(Array.from(s.cells,(_,i)=>s.cells[i]?i:-1).filter(i=>i>=0));C.step(s);}console.log(JSON.stringify(out));"
    result=subprocess.run(['node','-e',code,str(local.hour),str(local.minute),str(count)],cwd=ROOT,check=True,capture_output=True,text=True)
    return [frozenset(mask) for mask in json.loads(result.stdout)]

def main():
    OUT.mkdir(parents=True,exist_ok=True)
    url=(ROOT/'data/board-dashboard.url').read_text().strip().rstrip('/')
    get=lambda name:http(url,'/api/'+name)
    menu=get('faces');before=json.loads((OUT/'before-faces.json').read_text())
    assert len(menu['items'])==4 and menu['items'][:3]==before['items']
    assert menu['items'][3]['mode']=='reverse'
    hold=CONFIG['time_hold_seconds'];update=CONFIG['update_ms'];n=math.ceil((60000-hold*1000)/update)
    expected={};observed=set();held=set();cleared=False
    colors=[full_color(i) for i in range(4096)]
    with connect('COM4') as link:
        http(url,'/api/faces/select',{'id':menu['items'][0]['id']})
        for i in range(1,5):
            request(link,'mode next','MODE ')
            status=request(link,'status','STATUS ')
            assert 'mode='+menu['items'][i%4]['mode'] in status,status
        http(url,'/api/faces/select',{'id':'builtin:reverse'})
        status=request(link,'status','STATUS ');utc=int(re.search(r' utc=(\d+)',status)[1])
        local=datetime.fromisoformat(re.search(r' local=(\S+)',status)[1])
        offset=int(local.replace(tzinfo=timezone.utc).timestamp())-utc;flip=(utc//60+1)*60
        for target in (flip-60,flip,flip+60):expected[target]=frames(datetime.fromtimestamp(target+offset,timezone.utc),n)
        print(f'Checking {n} reverse frames at {update} ms, {hold}s hold, then clear/rebuild at the next minute...',flush=True)
        deadline=time.monotonic()+75
        while time.monotonic()<deadline:
            first=int(re.search(r' utc=(\d+)',request(link,'status','STATUS '))[1])
            rows=capture(link);values=raw_cells(rows)
            last=int(re.search(r' utc=(\d+)',request(link,'status','STATUS '))[1])
            if first!=last:continue
            assert all(value in (0,colors[i]) for i,value in enumerate(values)), 'Wrong gradient or unexpected cell fading'
            mask=frozenset(i for i,v in enumerate(values) if v)
            minute=first//60*60;phase=first-minute;target=minute if phase<hold else minute+60
            if target not in expected:continue
            if phase<hold:
                if phase==0 and mask!=expected[target][0]:continue
                assert mask==expected[target][0], 'Completed time did not hold unchanged'
                if minute==flip:
                    if not held:save_png(OUT/'minute-hold.png',rows)
                    held.add(phase)
            else:
                low=max(1,math.ceil((target-first-1)*1000/update)-1)
                high=min(n,math.ceil((target-first)*1000/update)+1)
                matches=[i for i in range(low,high+1) if expected[target][i]==mask]
                assert matches, ('Frame did not match reversed forward Life history',phase,low,high)
                if target==flip:
                    observed.update(matches)
                    for i in matches:
                        if i<=8 and not (OUT/f'reverse-{i}.png').exists():save_png(OUT/f'reverse-{i}.png',rows)
                if minute==flip and phase>=hold+1:
                    assert mask!=expected[flip][0], 'Previous time was not cleared'
                    save_png(OUT/'next-minute-sequence.png',rows);cleared=True;break
        assert cleared and (not hold or held), 'Minute hold/clear cycle not observed'
        assert len(observed)>=3, 'Too few reverse frames observed'
        request(link,'restart','RESTARTING')
    time.sleep(3)
    for attempt in range(20):
        try:
            restored=http(url,'/api/faces',timeout=3)
            assert restored['items']==menu['items'] and restored['activeIndex']==0
            break
        except OSError:
            if attempt==19:raise
            time.sleep(1)
    assert get('workspace')==json.loads((OUT/'before-workspace.json').read_text())
    http(url,'/api/faces/select',{'id':'builtin:reverse'})
    report={'faces':4,'updateMs':update,'reverseGenerations':n,'holdSeconds':hold,'observedHoldSeconds':sorted(held),'reverseFramesMatched':True,'clearedAndStartedNextMinute':cleared,'fourFaceCycle':True,'restartPersistence':True,'libraryUnchanged':True}
    (OUT/'verification.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    print('PASS:',json.dumps(report),flush=True)

if __name__=='__main__':main()
