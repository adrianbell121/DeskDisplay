const {test}=require('node:test'),assert=require('node:assert/strict');
const S=require('./scene.js'),Store=require('./studio-state.js');
const date=new Date(2026,9,28,18,2,42);
test('board Island contains twelve independently editable native layers with exact geometry',()=>{
 const f=S.boardIsland();assert.equal(f.background,'#000000');assert.equal(f.widgets.length,12);assert.equal(new Set(f.widgets.map(w=>w.id)).size,12);
 const b=S.compile(f);assert.equal(b.length,200);
 f.widgets.forEach((w,i)=>{assert.equal(w.islandPart,i);assert.equal(b[8+i*16],5+i);assert.equal(b[11+i*16],w.w);assert.equal(b[12+i*16],w.h);});
 const boat=f.widgets[9];assert.deepEqual([boat.x,boat.y,boat.w,boat.h],[42,48,11,16]);
 const original=S.clone(f);boat.x=38;boat.w=14;assert.notDeepEqual(S.compile(f),S.compile(original));assert.deepEqual(f.widgets.slice(0,9),original.widgets.slice(0,9));
});
test('Island boat stays rigid through its complete four-pixel bob and water stays full width',()=>{
 const f=S.boardIsland(),boat=f.widgets[9],shapes=new Set(),heights=new Set();
 for(let seconds=0;seconds<60;seconds+=.25){const image=S.raster(boat,seconds,date),points=[];for(let i=0;i<image.length;i+=4)if(image[i+3])points.push([i/4%boat.w,Math.floor(i/4/boat.w),...image.slice(i,i+3)]);const top=Math.min(...points.map(p=>p[1]));heights.add(top);shapes.add(JSON.stringify(points.map(([x,y,...c])=>[x,y-top,...c])));}
 assert.equal(shapes.size,1);assert.equal(heights.size,5);assert.equal(Math.max(...heights)-Math.min(...heights),4);
 const water=S.raster(f.widgets[2],0,date);for(let i=3;i<water.length;i+=4)assert.equal(water[i],255);
});
test('catalog migration preserves user faces, adds assets once, and respects later deletions',()=>{
 const old=Store.fresh();old.catalogVersion=3;old.faces=old.faces.filter(f=>f.id!=='island-board-v1');old.library=old.library.filter(w=>w.type!=='island');old.faces[0].name='My original';
 const next=Store.clean(old);assert.equal(next.faces[0].name,'My original');assert.equal(next.library.filter(w=>w.type==='island').length,12);assert.equal(next.faces.filter(f=>f.id==='island-board-v1').length,1);assert.deepEqual(Store.clean(next),next);
 next.faces=next.faces.filter(f=>f.id!=='island-board-v1');next.library=next.library.filter(w=>w.type!=='island');const reloaded=Store.clean(next);assert.equal(reloaded.library.filter(w=>w.type==='island').length,0);assert.equal(reloaded.faces.some(f=>f.id==='island-board-v1'),false);
});
test('Island color edits affect their own layer and unsupported transforms never upload silently',()=>{
 const f=S.boardIsland(),water=f.widgets[2],before=S.raster(water,0,date);water.color='#ff0000';const after=S.raster(water,0,date);assert.notDeepEqual(before,after);assert.equal(after[0],255);assert.equal(after[1],0);
 f.widgets[9].rotation=90;assert.ok(S.compile(f).length>8);f.widgets[9].rotation=0;f.widgets[9].speed=2;assert.throws(()=>S.compile(f),/speed/);
});
