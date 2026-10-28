const {test}=require('node:test'),assert=require('node:assert/strict');
const D=require('./widget-data.js'),A=require('./animation.js'),S=require('./scene.js'),Store=require('./studio-state.js');
const date=new Date(2026,9,28,14,5,9);
test('date tokens, literals, punctuation, and month boundaries',()=>{
  assert.equal(D.formatDate(date,'MM-DD-YYYY'),'10-28-2026');
  assert.equal(D.formatDate(date,'ddd, MMM D [at] hh:mm A'),'Wed, Oct 28 at 02:05 PM');
  assert.equal(D.formatDate(date,'[DATE:] YYYY/MM/DD | HH:mm:ss'),'DATE: 2026/10/28 | 14:05:09');
  assert.equal(D.formatDate(new Date(2028,1,29),'DD.MM.YY'),'29.02.28');
});
test('keyframes interpolate, hold, loop, normalize duplicate times, and survive saving',()=>{
  const a={duration:4,loop:false,easing:'linear',keys:[{t:0,x:0,y:0,scale:1,opacity:1},{t:4,x:20,y:-8,scale:2,opacity:0}]};
  assert.deepEqual(A.at(a,2),{x:10,y:-4,scale:1.5,opacity:.5});assert.equal(A.at(a,9).x,20);assert.equal(A.at({...a,loop:true},4).x,0);
  assert.equal(A.at({...a,easing:'step'},2).x,0);assert.equal(A.clean({...a,keys:[...a.keys,{t:4,x:12}]}).keys.length,2);
  const state=Store.fresh(),w=state.faces[0].widgets[0];w.animation=A.clean(a);w.dateFormat='MM-DD-YYYY';const saved=Store.clean(JSON.parse(JSON.stringify(state)));assert.deepEqual(saved.faces[0].widgets[0].animation,w.animation);
});
test('bound weather changes renderings, calendar filters today/tomorrow, curve stays within probability bounds',()=>{
  const sample=D.sample(date);D.setData(sample);
  const w=S.templates.find(w=>w.type==='weather'),before=S.raster(w,0,date);sample.current.tempF=32;sample.current.condition='snowy';D.setData(sample);assert.notDeepEqual(S.raster(w,0,date),before);
  sample.events.push({id:'outside',title:'Later',start:'2026-11-02T12:00:00'});assert.equal(D.agenda(sample.events,date,3).length,3);assert.equal(D.agenda(sample.events,date,2).length,2);assert.ok(D.agenda(sample.events,date).every(e=>e.id!=='outside'));
  for(let x=0;x<=119;x+=.13){const p=D.curve(sample.hourly,x);assert.ok(p>=0&&p<=100);}
  assert.equal(D.curve([{hour:0,precipProbability:0},{hour:1,precipProbability:100},{hour:10,precipProbability:100}],.2),100);
  D.setData(null);
});
test('provider data is validated, persisted, and merged; mock data cannot silently deploy as live',()=>{
  const state=Store.fresh();state.previewData=D.sample(date);const roundtrip=Store.clean(JSON.parse(JSON.stringify(state)));assert.equal(roundtrip.previewData.current.tempF,68);
  assert.equal(Store.merge(Store.fresh(),state).state.previewData.events.length,3);
  assert.throws(()=>D.clean({...state.previewData,current:{...state.previewData.current,precipProbability:NaN}}));
  for(const type of ['weather','forecast','calendar']){const w=S.templates.find(w=>w.type===type);assert.ok(S.raster(w,0,date).some(v=>v));assert.throws(()=>S.compile({background:'#000000',widgets:[w]}),/sample/);}
  assert.throws(()=>S.compile({background:'#000000',widgets:[S.cleanWidget({type:'date',dateFormat:'MM-DD-YYYY'})]}),/Custom date/);
});
