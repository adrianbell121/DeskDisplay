const {test}=require('node:test'),assert=require('node:assert/strict');
const W=require('./scene-weather.js'),S=require('./scene.js'),Store=require('./studio-state.js');
const date=new Date(2026,9,28,18,0);
const widget=(type,condition)=>S.cleanWidget({...W.templates().find(w=>w.type===type),weather:{condition}});
const visible=image=>image.some((v,i)=>i%4===3&&v>0);
test('each condition activates only its weather layers; auto uses the configured location',()=>{
 for(const condition of W.conditions)for(const type of W.kinds.filter(k=>k!=='weatherLightning')){const should=type==='weatherSun'?['sunny','partly-cloudy'].includes(condition):type==='weatherClouds'?condition!=='sunny':type==='weatherRain'?['rainy','storm'].includes(condition):condition==='snowy';assert.equal(visible(S.raster(widget(type,condition),1,date)),should,condition+' '+type);}
 W.setData('Boston, MA',{condition:'sunny',tempF:60});assert.equal(W.active(W.clean()),'sunny');assert.equal(W.active(W.clean({location:'Other place'})),undefined);
});
test('rain and snow move, zero density hides them, and settings persist without altering the base',()=>{
 for(const type of ['weatherRain','weatherSnow']){const w=widget(type,type==='weatherRain'?'rainy':'snowy');assert.notDeepEqual(S.raster(w,1,date),S.raster(w,1.5,date));w.weather=W.clean({...w.weather,rainDensity:0,snowDensity:0});assert.equal(visible(S.raster(w,1,date)),false);}
 const base=S.boardIsland(),f=S.cleanScene({...base,weather:{condition:'snowy',location:'Boston, MA',snowSpeed:8,snowDrift:5},widgets:[...base.widgets,...W.templates().map(S.cleanWidget)]});assert.deepEqual(f.widgets.slice(0,12),base.widgets);assert.deepEqual(S.cleanScene(JSON.parse(JSON.stringify(f))),f);assert.ok(S.compile(f).length>200);
 const state=Store.fresh();state.faces.push(f);f.id='adrian';state.defaultFaceId='adrian';assert.equal(Store.clean(state).defaultFaceId,'adrian');
});
test('lightning is thin and brief, with randomized intervals inside the configured bounds',()=>{
 const c=W.clean(),starts=[];let previous=false;for(let t=0;t<1000;t+=.02){const now=!!W.strikeAt(t,c);if(now&&!previous)starts.push(t);previous=now;}assert.ok(starts.length>=8);for(let i=1;i<starts.length;i++)assert.ok(starts[i]-starts[i-1]>=59.98&&starts[i]-starts[i-1]<=120.02);
 const w=widget('weatherLightning','storm');assert.equal(visible(S.raster(w,1,date)),false);assert.equal(visible(S.raster(w,starts[0],date)),true);assert.equal(visible(S.raster(w,starts[0]+.5,date)),false);
});
test('weather parameters clamp invalid inputs and ensure a valid lightning range',()=>{const c=W.clean({rainDensity:999,snowSpeed:NaN,lightningMin:120,lightningMax:20});assert.equal(c.rainDensity,100);assert.equal(c.snowSpeed,W.defaults.snowSpeed);assert.equal(c.lightningMax,120);});
