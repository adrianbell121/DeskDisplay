const {test}=require('node:test'),assert=require('node:assert/strict');
const W=require('./scene-weather.js'),S=require('./scene.js');
// Boston's DST fallback occurs during this overnight interval.
const epoch=s=>Date.parse(s)/1000;
const days=[
 {sunrise:epoch('2026-10-31T07:17:00-04:00'),sunset:epoch('2026-10-31T17:38:00-04:00')},
 {sunrise:epoch('2026-11-01T06:18:00-05:00'),sunset:epoch('2026-11-01T16:37:00-05:00')},
 {sunrise:epoch('2026-11-02T06:19:00-05:00'),sunset:epoch('2026-11-02T16:36:00-05:00')}
];
const loc='Solar test Boston';
const config={location:loc,condition:'sunny',sunCycle:true};
const sun=S.cleanWidget(W.templates()[0]);
W.setData(loc,{condition:'sunny',tempF:60,latitude:42.3601,longitude:-71.0589,solarDays:days});
test('sun takes priority exactly at sunrise; the night arc spans sunset to next sunrise including DST',()=>{
 const d=days[1];assert.equal(W.skyPhase(days,d.sunrise-1).body,'moon');assert.deepEqual(W.skyPhase(days,d.sunrise),{body:'sun',start:d.sunrise,end:d.sunset,progress:0});assert.equal(W.skyPhase(days,d.sunset-.001).body,'sun');assert.equal(W.skyPhase(days,d.sunset).body,'moon');
 const start=days[0].sunset,end=d.sunrise;assert.equal(end-start,13*3600+40*60);assert.equal(W.skyPhase(days,(start+end)/2).progress,.5);assert.equal(W.skyPhase(days,(start+end)/2).body,'moon');
 for(const offset of ['2026-11-01T01:30:00-04:00','2026-11-01T01:30:00-05:00'])assert.equal(W.skyPhase(days,epoch(offset)).body,'moon');
});
test('day and night use the same fixed peak and horizon, with no stale or guessed interval',()=>{
 const c=W.clean(config),placement=t=>W.skyPlacement(sun,new Date(t*1000),c,55);
 assert.equal(placement((days[1].sunrise+days[1].sunset)/2).top,30);assert.equal(placement((days[0].sunset+days[1].sunrise)/2).top,30);
 for(const t of [days[1].sunrise,days[1].sunset-.01,days[1].sunset,days[1].sunrise-.01])assert.equal(placement(t).top,50);
 let last=50;for(let i=0;i<=100;i++){const p=placement(days[1].sunrise+(days[1].sunset-days[1].sunrise)*i/200);assert(p.top<=last&&p.top>=30);last=p.top;}
 assert.equal(W.skyPhase(days,days[0].sunrise-3600),null);assert.equal(W.skyPhase(days,days[2].sunset+24*3600),null);assert.equal(W.skyPhase([],days[1].sunrise),null);
});
test('sky pixels stop at permanent water and waves obscure both sun and moon',()=>{
 const sky={...sun,weather:W.clean(config)},scene=S.cleanScene({background:'#000000',weather:config,widgets:[sky]}),date=new Date((days[1].sunrise+2000)*1000);
 for(const t of [days[1].sunrise,(days[1].sunrise+days[1].sunset)/2,days[1].sunset]){const image=S.render(scene,0,new Date(t*1000));assert(image.slice(0,55*64*4).some((v,i)=>i%4!==3&&v));assert(!image.slice(55*64*4).some((v,i)=>i%4!==3&&v));}
 for(const t of [(days[1].sunrise+days[1].sunset)/2,(days[0].sunset+days[1].sunrise)/2]){
  const above=S.render(scene,0,new Date(t*1000));assert(above.some((v,i)=>i%4!==3&&v));
  // An opaque foreground layer covers the moving sky irrespective of its icon.
  const wave=S.cleanWidget({type:'box',x:0,y:30,w:64,h:25,color:'#123456'}),covered=S.render({...scene,widgets:[sky,wave]},0,new Date(t*1000)),only=S.render({...scene,widgets:[wave]},0,date);assert.deepEqual(covered,only);
 }
 const waves=S.boardIsland().widgets.filter(w=>[2,3,4,5].includes(w.islandPart));
 for(const t of [days[1].sunrise-1,days[1].sunrise,days[1].sunset-1,days[1].sunset,days[1].sunset+1])for(let seconds=0;seconds<=16;seconds+=.25){
  const at=new Date(t*1000),base=S.render({...scene,widgets:waves},seconds,at),withSky=S.render({...scene,widgets:[sky,...waves]},seconds,at);
  assert.notDeepEqual(withSky,base,'Half-covered sky remains visible across sunrise and sunset');
 }
 for(const t of [days[1].sunset-1800,days[0].sunset+1800]){
  const at=new Date(t*1000),base=S.render({...scene,widgets:waves},1,at),back=S.render({...scene,widgets:[sky,...waves]},1,at),front=S.render({...scene,widgets:[...waves,sky]},1,at);
  let occluded=0;for(let i=0;i<base.length;i+=4)if(base[i]||base[i+1]||base[i+2]){assert.deepEqual(back.slice(i,i+3),base.slice(i,i+3));if(front[i]!==base[i]||front[i+1]!==base[i+1]||front[i+2]!==base[i+2])occluded++;}
  assert(occluded>0,'Native wave pixels cover part of the setting/rising sky icon');
 }
 assert.equal(W.clean({}).sunCycle,false);assert.equal(W.clean(config).sunCycle,true);
});
test('solar package extends only sky layers and records horizon and moon colors',()=>{
 const water=S.cleanWidget({type:'island',islandPart:2,x:0,y:55,w:64,h:9}),scene=S.cleanScene({background:'#000000',weather:config,widgets:[sun,water,...W.templates().slice(1).map(S.cleanWidget)]}),bytes=S.compile(scene),v=new DataView(bytes.buffer);
 assert.equal(bytes[8],17);assert.equal(v.getUint16(22,true),84);assert.equal(bytes[27],1);assert.equal(v.getFloat32(100,true),55);assert.equal(bytes[108],7);assert.equal(bytes[124],18);assert.equal(v.getUint16(138,true),76);
 const path=require('node:path'),fs=require('node:fs');fs.mkdirSync(path.join(__dirname,'../artifacts/solar-tests'),{recursive:true});fs.writeFileSync(path.join(__dirname,'../artifacts/solar-tests/solar.fls'),bytes);
});
