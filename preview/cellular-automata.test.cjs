const {test}=require('node:test'),assert=require('node:assert/strict');
const C=require('./cellular-automata.js'),S=require('./scene.js'),Store=require('./studio-state.js');
const I=require('./island-assets.js');
const standard={...require('../cellular-automata-config.json'),time_reverse_frames:8,update_ms:500,clear_every_minutes:15,time_hold_seconds:5,fade_after_seconds:30,fade_duration_seconds:120};
C.configure(standard);
function pattern(points){const s=C.create();for(const [x,y]of points)s.cells[y*64+x]=1;return s;}
const glider=()=>pattern([[2,1],[3,2],[1,3],[2,3],[3,3]]);

test('empty grids, still lifes, and oscillators never reset',()=>{
  for(const points of [[],[[10,10],[11,10],[10,11],[11,11]]]){
    const s=pattern(points),initial=s.cells.slice();for(let i=0;i<100;i++)C.step(s);assert.deepEqual(s.cells,initial);
  }
  const s=pattern([[10,9],[10,10],[10,11]]),initial=s.cells.slice();
  C.step(s);assert.notDeepEqual(s.cells,initial);
  C.step(s);assert.deepEqual(s.cells,initial);
  for(let i=0;i<100;i++)C.step(s);assert.deepEqual(s.cells,initial);
});

test('Conway evolution keeps moving and wraps at the edges',()=>{
  const s=glider();
  for(let i=0;i<300;i++){C.step(s);assert.equal(s.cells.reduce((a,b)=>a+b),5);}
  const edge=pattern([[63,63],[0,63],[1,63]]);C.step(edge);
  assert.equal(edge.cells[62*64],1);assert.equal(edge.cells[63*64],1);assert.equal(edge.cells[0],1);
});

test('clock mask reuses Island digits and AM/PM without touching the game',()=>{
  for(const [h,m]of [[0,0],[1,2],[11,59],[12,0],[13,7],[23,59]]){
    const date=new Date(2026,10,9,h,m),cells=C.clockCells(date);
    const clock=I.raster({islandPart:0,w:49,h:13,color:'#ffffff',secondary:'#ffffff',format:'12',speed:1},0,date);
    for(let y=0;y<64;y++)for(let x=0;x<64;x++)assert.equal(cells[y*64+x],x>=7&&x<56&&y>=25&&y<38?(clock[((y-25)*49+x-7)*4+3]?1:0):0);
  }
});

test('eight reverse frames at 500 ms finish exactly on the incoming minute',()=>{
  const start=new Date(2026,10,9,12,34,55).getTime(),flip=new Date(2026,10,9,12,35).getTime(),s=C.create();
  C.advance(s,0,new Date(start));
  const frames=C.clockFrames(new Date(flip));assert.equal(frames.length,9);
  assert.equal(C.target(flip-4001),-1);assert.equal(C.target(flip-4000),flip);
  for(let i=0;i<8;i++){
    const wall=flip-4000+i*500;C.advance(s,(wall-start)/1000,new Date(wall));
    assert.equal(s.overlayAt,flip);assert.deepEqual(C.overlay(s,new Date(wall)),frames[8-i]);
  }
  C.advance(s,5,new Date(flip));assert.deepEqual(C.overlay(s,new Date(flip)),frames[0]);
  C.advance(s,9.999,new Date(flip+4999));assert.deepEqual(C.overlay(s,new Date(flip+4999)),frames[0]);
  C.advance(s,10,new Date(flip+5000));assert.equal(C.overlay(s,new Date(flip+5000)),null);
  for(let i=0;i<4096;i++)if(frames[0][i])assert.equal(s.cells[i],1);
});

test('the underlying game advances during the hold and merge never erases live cells',()=>{
  const s=glider(),date=new Date(2026,10,9,12,34,10),before=s.cells.slice();
  C.advance(s,0,date);const top=C.overlay(s,date).slice();assert.deepEqual(s.cells,before);
  const expected={cells:before};C.step(expected);
  C.advance(s,.5,new Date(date.getTime()+500));assert.deepEqual(s.cells,expected.cells);
  assert.deepEqual(C.overlay(s,new Date(date.getTime()+500)),top);
  const all=C.create();all.cells.fill(1);all.born.fill(7);C.merge(all,top,10);
  assert.ok(all.cells.every(v=>v===1));assert.ok(all.born.every(v=>v===7));
  const empty=C.create();C.merge(empty,top,10);assert.deepEqual(empty.cells,top);
  for(let i=0;i<4096;i++)assert.equal(empty.born[i],top[i]?11:0);
});

test('the base also keeps advancing throughout the reverse animation',()=>{
  const s=glider(),start=new Date(2026,10,9,12,34,56),expected={cells:s.cells.slice()};
  C.advance(s,0,start);
  for(let i=1;i<=8;i++){C.step(expected);C.advance(s,i*.5,new Date(start.getTime()+i*500));assert.deepEqual(s.cells,expected.cells);}
});

test('incoming frames use the next hour, AM/PM and date at midnight',()=>{
  for(const [h,m]of [[11,59],[12,59],[23,59]]){
    const date=new Date(2026,10,9,h,m,56),s=C.create();C.advance(s,0,date);
    const next=new Date(date.getTime()+4000);assert.deepEqual(s.frames[0],C.clockCells(next));
  }
});

test('scheduled clears affect the game only, once per aligned boundary',()=>{
  for(const minute of [0,15,30,45]){const q=C.create();q.cells[0]=1;const d=new Date(2026,10,9,13,minute);C.advance(q,0,d);assert.equal(q.cells[0],0);assert.ok(C.overlay(q,d).some(Boolean));}
  const s=C.create();s.cells[0]=1;C.advance(s,0,new Date(2026,10,9,12,14));assert.equal(s.cells[0],1);
  C.advance(s,.01,new Date(2026,10,9,12,15));assert.equal(s.cells[0],0);
  s.cells[0]=1;C.advance(s,.02,new Date(2026,10,9,12,15));assert.equal(s.cells[0],1);
  C.advance(s,.03,new Date(2026,10,9,12,16));assert.equal(s.cells[0],1);
});

test('zero reverse frames disables the lead-in and zero hold merges immediately',()=>{
  try{
    C.configure({...standard,time_reverse_frames:0,time_hold_seconds:0});
    const s=C.create(),date=new Date(2026,10,9,12,34);C.advance(s,0,date);
    assert.equal(C.overlay(s,date),null);assert.deepEqual(s.cells,C.clockCells(date));
    assert.equal(C.target(date.getTime()+59999),-1);
  }finally{C.configure(standard);}
});

test('clock overlay has an opaque backing without permanently hiding the game',()=>{
  const w=S.cellularAutomata().widgets[0],date=new Date(2026,10,9,12,34),mask=C.clockCells(date);
  const image=C.raster(w,0,date);
  for(let y=25;y<38;y++)for(let x=7;x<56;x++){
    const i=y*64+x;assert.equal(image[i*4+3],255);
    if(!mask[i])assert.deepEqual([...image.slice(i*4,i*4+3)],[0,0,0]);
  }
  assert.equal(image[3],0,'Backing covers only the clock rectangle');
  C.raster(w,5,new Date(date.getTime()+5000));
  const released=C.raster(w,5,new Date(date.getTime()+5000));
  const gap=mask.findIndex((v,i)=>!v&&i%64>=7&&i%64<56&&i/64>=25&&i/64<38);
  assert.equal(released[gap*4+3],0,'Backing disappears after the overlay merges');
});

test('continuous life fades after 30 seconds, expires at 150, and death resets age',()=>{
  const s=C.create();s.cells[0]=1;C.age(s,0);assert.equal(s.born[0],1);
  const full=C.cellColor(0,0,30);assert.ok(C.cellColor(0,0,90)[1]<full[1]);
  C.age(s,149.999);assert.equal(s.cells[0],1);C.age(s,150);assert.equal(s.cells[0],0);
  s.cells[0]=1;C.age(s,151);assert.equal(s.born[0],152);
  s.cells[0]=0;C.age(s,152);assert.equal(s.born[0],0);
});

test('config supports solid, horizontal, vertical and two-axis gradients',()=>{
  const original=require('../cellular-automata-config.json');
  try{for(const direction of ['none','horizontal','vertical','both']){
    C.configure({...original,color:'#ff0000',gradient_end_color:'#0000ff',gradient:direction});
    assert.deepEqual(C.cellColor(0,0,0),[255,0,0,255]);
    assert.deepEqual(C.cellColor(63,63,0),direction==='none'?[255,0,0,255]:[0,0,255,255]);
    if(direction==='horizontal')assert.deepEqual(C.cellColor(63,0,0),[0,0,255,255]);
    if(direction==='vertical')assert.deepEqual(C.cellColor(0,63,0),[0,0,255,255]);
    if(direction==='both')assert.ok(C.cellColor(63,0,0)[0]>0&&C.cellColor(63,0,0)[2]>0);
  }}finally{C.configure(original);}
});

test('config rejects invalid timing, clear intervals and color values',()=>{
  const original=require('../cellular-automata-config.json');
  for(const change of [{time_reverse_frames:-1},{time_reverse_frames:121},{time_reverse_frames:120,update_ms:60000},{update_ms:0},{fade_duration_seconds:0},{clear_every_minutes:7},{color:'green'},{gradient:'diagonal'}])assert.throws(()=>C.configure({...original,...change}),/Invalid|fit within/);
});

test('scene stays procedural on upload and rejects unsupported movement',()=>{
  const face=S.cellularAutomata(),bytes=S.compile(face),v=new DataView(bytes.buffer);
  assert.equal(bytes.length,24);assert.equal(bytes[8],28);assert.equal(bytes[11],64);assert.equal(bytes[12],64);
  assert.equal(v.getUint16(20,true),0);assert.equal(v.getUint16(22,true),0);
  for(const update of [{speed:2},{effect:'bob'},{flipX:true},{rotation:90},{animation:{keys:[{t:0,x:1}]}}]){
    const w=S.cleanWidget({...face.widgets[0],...update});assert.throws(()=>S.compile({...face,widgets:[w]}),/runs live/);
  }
});

test('existing workspaces gain the face once, preserve edits, and respect later deletion',()=>{
  const old=Store.fresh();old.catalogVersion=4;old.faces=old.faces.filter(f=>f.id!=='cellular-automata-v1');old.library=old.library.filter(w=>w.type!=='cellularAutomata');
  old.faces[0].notes='Keep my changes';old.defaultFaceId=old.selected;
  const migrated=Store.clean(old);assert.equal(migrated.faces.length,old.faces.length+1);assert.equal(migrated.defaultFaceId,old.defaultFaceId);
  assert.deepEqual(migrated.faces.slice(0,-1),old.faces);assert.equal(migrated.faces.at(-1).name,'Cellular automata');
  assert.deepEqual(Store.clean(migrated),migrated);
  migrated.faces=migrated.faces.filter(f=>f.id!=='cellular-automata-v1');migrated.library=migrated.library.filter(w=>w.type!=='cellularAutomata');
  assert.deepEqual(Store.clean(migrated),migrated);
});
