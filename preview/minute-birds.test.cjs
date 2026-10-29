const {test}=require('node:test'),assert=require('node:assert/strict'),B=require('./minute-birds.js'),S=require('./scene.js');
const count=s=>B.perches.filter((_,i)=>B.pose(i,s)).length;
test('five birds accumulate at each ten second interval and stay through second 59',()=>{
 for(const [s,n]of [[4,0],[9.999,0],[10,1],[19.999,1],[20,2],[29.999,2],[30,3],[39.999,3],[40,4],[49.999,4],[50,5],[59.999,5]])assert.equal(count(s),n,String(s));
 for(let i=0;i<5;i++){const p=B.pose(i,59.999);assert.deepEqual([p.x,p.y],B.perches[i]);assert.equal(p.flight,false);}
});
test('all birds leave only at rollover and clear the scene before second four',()=>{
 assert.equal(count(0),5);for(let i=0;i<5;i++){assert.equal(B.pose(i,.1).flight,true);assert.equal(B.pose(i,4),null);assert.equal(B.pose(i,9),null);}assert.equal(count(60),5);
});
test('arrivals start off screen from alternating sides, land, and preserve the three pixel design',()=>{
 for(let i=0;i<5;i++){const start=B.pose(i,(i+1)*10);assert.equal(start.x,i%2?65:-2);const landed=B.pose(i,(i+1)*10+2);assert.deepEqual([landed.x,landed.y],B.perches[i]);const w=S.cleanWidget({type:'minuteBird',birdIndex:i,x:0,y:32,w:64,h:32,color:'#9bb9b1'}),d=new Date(2026,9,29,12,0,59);const image=S.raster(w,1000,d);assert.equal([...image].filter((v,j)=>j%4===3&&v).length,3);assert.equal(S.compile({background:'#000000',widgets:[w]})[8],22+i);}
 assert.equal(new Set(B.perches.map(p=>p.join(','))).size,5);
});
test('clock seconds control birds independently of animation playhead and edits round trip',()=>{
 const w=S.cleanWidget({type:'minuteBird',birdIndex:4,x:0,y:32,w:64,h:32,color:'#9bb9b1'}),d=new Date(2026,9,29,12,0,55);assert.deepEqual(S.raster(w,0,d),S.raster(w,999,d));assert.equal(S.cleanWidget(JSON.parse(JSON.stringify(w))).birdIndex,4);w.speed=2;assert.throws(()=>S.compile({background:'#000000',widgets:[w]}),/clock seconds/);
});
test('arrival and departure paths stay continuous across every integer-second boundary',()=>{
 for(let i=0;i<5;i++)for(const [start,end]of [[0,4],[(i+1)*10,(i+1)*10+3]]){let previous=null;for(let ms=Math.round(start*1000);ms<=end*1000;ms++){const p=B.pose(i,ms/1000);if(p&&previous){assert.ok(Math.abs(p.x-previous.x)<=1,`bird ${i} jumped horizontally at ${ms}`);assert.ok(Math.abs(p.y-previous.y)<=1,`bird ${i} jumped vertically at ${ms}`);}previous=p;}}
 // A split clock read at 11.999 -> 12.001 fabricated phase 11.001:
 // reproduce its five-pixel backward jump, which coherent frames must avoid.
 const before=B.pose(0,11.999),mixed=B.pose(0,11.001),after=B.pose(0,12.001);
 assert.ok(Math.abs(before.x-mixed.x)>1);assert.deepEqual([before.x,before.y],[after.x,after.y]);
});
