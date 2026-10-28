const {test}=require('node:test'),assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path');
const S=require('./scene.js'),Store=require('./studio-state.js'),E=require('./engine.js');
const date=new Date(2026,9,23,10,8,42);
test('all migrated faces contain independent editable assets and compile below board limits',()=>{
  const state=Store.fresh();
  for(const f of state.faces){assert.ok(f.widgets.length>1);const image=S.render(f,1,date);assert.equal(image.length,16384);for(let i=3;i<image.length;i+=4)assert.equal(image[i],255);const binary=S.compile(f);assert.equal(Buffer.from(binary.subarray(0,4)).toString(),'FLS2');assert.ok(binary.length<262144,f.name);}
  const island=state.faces.find(f=>f.id==='island');assert.equal(island.widgets.filter(w=>w.type==='palm').length,2);assert.equal(island.widgets.filter(w=>w.type==='boat').length,1);
});
test('moving, resizing, hiding, removing, layering, and effects change only the intended instance',()=>{
  const f=Store.fresh().faces[0],original=S.clone(f),boat=f.widgets.find(w=>w.type==='boat');
  const start=S.render(f,1,date);Object.assign(boat,{x:12,y:30,w:28,h:24,effect:'bob'});assert.notDeepEqual(S.render(f,1,date),start);
  assert.deepEqual(f.widgets.filter(w=>w.id!==boat.id),original.widgets.filter(w=>w.id!==boat.id));
  boat.visible=false;const hidden=S.render(f,1,date);f.widgets=f.widgets.filter(w=>w.id!==boat.id);assert.deepEqual(S.render(f,1,date),hidden);
  const top=S.cleanWidget({type:'box',x:10,y:10,w:10,h:10,color:'#ff0000'}),bottom=S.cleanWidget({...top,id:S.uid(),color:'#0000ff'});const scene={background:'#000000',widgets:[bottom,top]};assert.equal(S.render(scene,0,date)[(10*64+10)*4],255);scene.widgets.reverse();assert.equal(S.render(scene,0,date)[(10*64+10)*4+2],255);
});
test('custom pixel frames preserve transparency, loop deterministically, and export',()=>{
  const w=S.cleanWidget({type:'sprite',x:3,y:3,w:4,h:4,sprite:{width:2,height:2,palette:['#ff0000'],frames:['1000','0001'],fps:2}});
  const a=S.raster(w,0,date),b=S.raster(w,.5,date);assert.notDeepEqual(a,b);assert.deepEqual(S.raster(w,1,date),a);assert.equal(a[3],255);assert.equal(a[15],0);assert.ok(S.compile({background:'#000000',widgets:[w]}).length>8);
  assert.throws(()=>S.cleanWidget({...w,sprite:{...w.sprite,frames:['2000']}}),/invalid/);
  assert.throws(()=>S.cleanWidget({...w,sprite:{...w.sprite,frames:['1']}}),/invalid/);
});
test('workspace migration, copy, and import preserve widgets without aliasing or duplicate imports',()=>{
  const legacy={version:1,selected:'island',faces:[{id:'island',base:'island',name:'My shore',config:{...E.defaults,motion:.5},favorite:true,notes:'Keep boat'}]};
  const migrated=Store.clean(legacy);assert.equal(migrated.faces[0].notes,'Keep boat');assert.equal(migrated.faces[0].widgets.find(w=>w.type==='boat').speed,.5);
  const first=Store.merge(Store.fresh(),legacy),second=Store.merge(first.state,legacy);assert.equal(second.addedFaces,0);assert.equal(second.addedWidgets,0);
  const roundtrip=Store.clean(JSON.parse(JSON.stringify(migrated)));assert.deepEqual(roundtrip,migrated);const copy=S.clone(migrated.faces[0].widgets[0]);copy.color='#ff0000';assert.notEqual(copy.color,migrated.faces[0].widgets[0].color);
});
test('invalid scenes, transforms on live data, and oversized packages fail before upload',()=>{
  assert.throws(()=>S.cleanWidget({type:'eval'}),/Unknown/);assert.throws(()=>S.cleanScene({widgets:Array(33).fill({type:'box'})}),/32/);
  const w=S.cleanWidget({type:'clock',w:54,h:14,effect:'sway'});assert.throws(()=>S.compile({background:'#000000',widgets:[w]}),/Live/);
  const big=S.cleanWidget({type:'rain',w:64,h:64});assert.throws(()=>S.compile({background:'#000000',widgets:Array.from({length:3},()=>S.clone(big))}),/256 KB/);
});
test('write cross-language package fixtures for offline USB tests',()=>{
  const dir=path.join(__dirname,'../artifacts/studio-tests');fs.mkdirSync(dir,{recursive:true});
  for(const f of Store.fresh().faces)fs.writeFileSync(path.join(dir,f.id+'.fls'),S.compile(f));
});
