const {test}=require('node:test'),assert=require('node:assert/strict');
const C=require('./cellular-automata.js'),S=require('./scene.js'),Store=require('./studio-state.js');
const I=require('./island-assets.js');
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

test('time stamping reuses Island digits and AM/PM and replaces only the centered rectangle',()=>{
  for(const [h,m]of [[0,0],[1,2],[11,59],[12,0],[13,7],[23,59]]){
    const s=C.create();s.cells.fill(1);const date=new Date(2026,10,9,h,m);
    const clock=I.raster({islandPart:0,w:49,h:13,color:'#ffffff',secondary:'#ffffff',format:'12',speed:1},0,date);
    C.stamp(s,date);assert.equal(s.minute,h*60+m);
    for(let y=0;y<64;y++)for(let x=0;x<64;x++)assert.equal(s.cells[y*64+x],x>=7&&x<56&&y>=25&&y<38?(clock[((y-25)*49+x-7)*4+3]?1:0):1);
  }
});

test('preview holds time five seconds, then evolves at configured speed',()=>{
  const w=S.cellularAutomata().widgets[0],date=new Date(2026,10,9,12,34),first=S.raster(w,0,date);
  assert.deepEqual(S.raster(w,.249,date),first);
  assert.deepEqual(S.raster(w,4.999,date),first);
  const evolved=S.raster(w,5,date);assert.notDeepEqual(evolved,first);
  assert.deepEqual(S.raster(w,5.249,date),evolved);
  assert.notDeepEqual(S.raster(w,5.25,date),evolved);
  const next=new Date(2026,10,9,12,35),stamped=S.raster(w,6,next);
  assert.notDeepEqual(stamped,evolved);assert.deepEqual(S.raster(w,10.999,next),stamped);
  assert.notDeepEqual(S.raster(w,11,next),stamped);
  C.reset(w);assert.deepEqual(S.raster(w,5,date),first,'Re-entry starts with the time itself');
  S.raster(w,5.25,date);assert.deepEqual(S.raster(w,0,date),first,'Rewind starts with the time itself');
});

test('quarter-hour clear runs before stamping; ordinary minutes preserve outside cells',()=>{
  for(const minute of [0,15,30,45]){const q=C.create();q.cells[0]=1;C.advance(q,0,new Date(2026,10,9,13,minute));assert.equal(q.cells[0],0);assert.ok(q.cells.some(Boolean));}
  const s=C.create();s.cells[0]=1;C.advance(s,0,new Date(2026,10,9,12,14));assert.equal(s.cells[0],1);
  C.advance(s,1,new Date(2026,10,9,12,15));assert.equal(s.cells[0],0);assert.ok(s.cells.some(Boolean));
  s.cells[0]=1;C.advance(s,2,new Date(2026,10,9,12,15));assert.equal(s.cells[0],1,'Clear only once per boundary');
  C.advance(s,3,new Date(2026,10,9,12,16));assert.equal(s.cells[0],1);
});

test('continuous life fades after 30 seconds, expires at 150, and death resets age',()=>{
  const s=C.create();s.cells[0]=1;C.age(s,0);assert.equal(s.born[0],1);
  const full=C.cellColor(0,0,30);assert.ok(C.cellColor(0,0,90)[1]<full[1]);
  C.age(s,149.999);assert.equal(s.cells[0],1);C.age(s,150);assert.equal(s.cells[0],0);
  s.cells[0]=1;C.age(s,151);assert.equal(s.born[0],152);
  s.cells[0]=0;C.age(s,152);assert.equal(s.born[0],0);
});

test('config supports solid, horizontal, vertical and two-axis gradients',()=>{
  const original=require('./cellular-automata-config.json');
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
  const original=require('./cellular-automata-config.json');
  for(const change of [{update_ms:0},{fade_duration_seconds:0},{clear_every_minutes:7},{color:'green'},{gradient:'diagonal'}])assert.throws(()=>C.configure({...original,...change}),/Invalid/);
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
