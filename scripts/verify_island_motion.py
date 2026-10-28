"""Check real framebuffer samples for rigid boat motion and permanent water."""
import re
import time
from device import connect, request

def rgb(r,g,b): return ((r&248)<<8)|((g&252)<<3)|(b>>3)

def main():
    boat_colors={rgb(*c) for c in [(174,161,127),(229,233,218),(207,221,213),(157,111,69),(113,84,59)]}
    shapes=set();heights=set()
    with connect(None) as link:
        assert 'MODE OK' in request(link,'mode island','MODE ')
        for _ in range(40):
            result=request(link,'capture','FRAME END',timeout=10)
            rows=re.findall(r'(?m)^([0-9a-f]{256})\r?$',result)
            assert len(rows)==64,'Incomplete framebuffer'
            pixels=[[int(row[i:i+4],16) for i in range(0,256,4)] for row in rows]
            assert all(pixels[y][x]==0 for y in range(29,38) for x in range(64)),'Background pixels are not off'
            assert all(pixels[y][x]!=0 for y in range(59,64) for x in [0,63]),'Water does not span the panel'
            points={(x,y,pixels[y][x]) for y in range(47,64) for x in range(42,53) if pixels[y][x] in boat_colors}
            assert points,'Boat missing'
            top=min(p[1] for p in points);heights.add(top)
            shapes.add(frozenset((x,y-top,c) for x,y,c in points))
            time.sleep(.25)
        assert len(shapes)==1,'Boat deformed between frames'
        assert len(heights)>1,'Boat did not bob'
        assert max(heights)-min(heights)<=4,'Boat travel exceeded four pixels'
        print(f'PASS: 40 hardware frames; identical boat shape at {len(heights)} vertical positions; permanent water base')

if __name__=='__main__': main()
