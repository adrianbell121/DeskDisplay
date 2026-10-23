const {test}=require('node:test');
const assert=require('node:assert/strict');
const {presets,defaults,normalize,render}=require('./engine.js');
const sample=new Date(2026,9,23,10,8,42);
const config=p=>normalize({...defaults,...p.config});

test('all faces produce opaque RGB565 frames at time and layout boundaries',()=>{
  const redBlue=new Set(Array.from({length:32},(_,i)=>Math.round(i*255/31)));
  const green=new Set(Array.from({length:64},(_,i)=>Math.round(i*255/63)));
  const samples=[new Date(2026,0,1,0,0,0),new Date(2026,8,30,23,59,59),sample];
  for(const p of presets)for(const digitStyle of ['segment','block'])for(const date of samples)for(const timeY of [11,25]){
    const buffer=new Uint8ClampedArray(16384);
    assert.equal(render(p.id,{...config(p),digitStyle,timeY,sceneY:timeY===11?-5:5,format:'24',showSeconds:true},51.5,date,buffer),buffer);
    assert.ok(buffer.some((v,i)=>i%4!==3&&v>0),`${p.id} is blank`);
    for(let i=0;i<buffer.length;i+=4){assert.ok(redBlue.has(buffer[i]));assert.ok(green.has(buffer[i+1]));assert.ok(redBlue.has(buffer[i+2]));assert.equal(buffer[i+3],255);}
  }
});
test('seeking does not depend on playback history, including bird departure and return',()=>{
  for(const p of presets)for(const seconds of [0,24,26,28,48,50,52,119.99]){
    const first=render(p.id,config(p),seconds,sample);
    render(p.id,config(p),seconds+1000,sample);
    assert.deepEqual(render(p.id,config(p),seconds,sample),first);
  }
});
test('each scene animates and zero motion freezes it',()=>{
  for(const p of presets){
    assert.notDeepEqual(render(p.id,config(p),0,sample),render(p.id,config(p),7,sample),p.id);
    const frozen={...config(p),motion:0};
    assert.deepEqual(render(p.id,frozen,0,sample),render(p.id,frozen,97,sample),p.id);
  }
});
test('configuration rejects malformed colors, enums, non-finite and out-of-bounds settings',()=>{
  const c=normalize({ink:'red',accent:'#ff00AA',background:'url(bad)',timeY:Infinity,sceneY:900,motion:-7,format:'42',digitStyle:'bad',showDate:'false'});
  assert.equal(c.ink,defaults.ink);assert.equal(c.accent,'#ff00AA');assert.equal(c.background,defaults.background);assert.equal(c.timeY,defaults.timeY);assert.equal(c.sceneY,5);assert.equal(c.motion,0);assert.equal(c.format,'12');assert.equal(c.digitStyle,'segment');assert.equal(c.showDate,true);
});
