"""Verify the uploaded original Island face against the native renderer on hardware."""
import re
import time
from device import connect,request,save_png


def main():
    colors={((r&248)<<8)|((g&252)<<3)|(b>>3) for r,g,b in [(174,161,127),(229,233,218),(207,221,213),(157,111,69),(113,84,59)]}
    shapes=set();heights=set()
    with connect(None) as link:
        status=request(link,'status','STATUS ')
        assert 'mode=custom' in status,'Upload Island — board layout first'
        for _ in range(40):
            result=request(link,'islandcompare','ISLANDCOMPARE ')
            assert 'pixels_different=0' in result,result
            frame=request(link,'capture','FRAME END',timeout=10)
            rows=re.findall(r'(?m)^([0-9a-f]{256})\r?$',frame)
            assert len(rows)==64
            pixels=[[int(row[x:x+4],16) for x in range(0,256,4)] for row in rows]
            assert all(pixels[y][x]==0 for y in range(29,38) for x in range(64))
            assert all(pixels[y][x]!=0 for y in range(59,64) for x in [0,63])
            points={(x,y,pixels[y][x]) for y in range(47,64) for x in range(42,53) if pixels[y][x] in colors}
            assert points
            top=min(y for x,y,c in points);heights.add(top)
            shapes.add(frozenset((x,y-top,c) for x,y,c in points))
            time.sleep(.25)
        assert len(shapes)==1,'Boat changed shape'
        assert len(heights)>1 and max(heights)-min(heights)<=4,'Boat motion incorrect'
        save_png('artifacts/island-dashboard-upload.png',rows)
        print(f'PASS: 40 pixel-identical native/uploaded frames; rigid boat at {len(heights)} heights; full-width sea.')
        print(request(link,'restart','RESTARTING'))
    time.sleep(5)
    with connect(None) as link:
        deadline=time.monotonic()+60
        while True:
            status=request(link,'status','STATUS ')
            if 'wifi=connected' in status and 'ntp_syncs=0' not in status:break
            assert time.monotonic()<deadline,'Timed out waiting for internet time after restart'
            time.sleep(2)
        assert 'mode=custom' in status,'Uploaded face did not survive restart'
        assert 'wifi=connected' in status,'Wi-Fi did not reconnect'
        assert 'ntp_syncs=0' not in status,'Internet time did not synchronize'
        assert 'pixels_different=0' in request(link,'islandcompare','ISLANDCOMPARE ')
        print('PASS: uploaded layout persisted after restart; Wi-Fi connected; internet time synchronized.')

if __name__=='__main__':main()
