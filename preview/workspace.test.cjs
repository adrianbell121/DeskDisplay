const {test}=require('node:test');
const assert=require('node:assert/strict');
const {freshFaces,clean,merge}=require('./workspace.js');
const fresh=()=>clean({version:1,faces:freshFaces(),selected:'island'});
let sequence=0;
const makeId=()=>'v-test-'+(++sequence);

test('JSON round trip preserves variations, notes, selection, and preview settings',()=>{
  const initial=fresh();initial.faces.push({...initial.faces[0],id:'v-saved',name:'Quiet shore',notes:'Less motion',config:{...initial.faces[0].config,motion:.4},favorite:true});
  Object.assign(initial,{selected:'v-saved',sampleTime:'2026-12-31T23:59:58',clockMode:'live',view:'led',speed:.5,brightness:35,advanceClock:true});
  assert.deepEqual(clean(JSON.parse(JSON.stringify(initial))),initial);
});
test('import retains local edits, restores controls, and deduplicates repeated imports',()=>{
  const current=fresh(),incoming=fresh();incoming.faces[0].config.motion=.4;incoming.faces[0].notes='Keep this';incoming.selected='island';incoming.view='led';incoming.speed=2;
  const original=JSON.stringify(current),first=merge(current,incoming,makeId);
  assert.equal(JSON.stringify(current),original);assert.equal(first.additions,1);assert.equal(first.state.faces.length,7);assert.equal(first.state.view,'led');assert.equal(first.state.speed,2);
  const variation=first.state.faces.find(f=>f.id===first.state.selected);assert.equal(variation.config.motion,.4);assert.notEqual(variation.id,'island');
  const second=merge(first.state,incoming,makeId);assert.equal(second.additions,0);assert.equal(second.state.faces.length,7);assert.equal(second.state.selected,variation.id);
});
test('invalid imports and base identities are rejected',()=>{
  assert.throws(()=>clean({version:2,faces:freshFaces()}));
  const duplicate=fresh();duplicate.faces.push({...duplicate.faces[0]});assert.throws(()=>clean(duplicate),/duplicate/);
  const mismatch=fresh();mismatch.faces[0].base='orbit';assert.throws(()=>clean(mismatch),/invalid/);
  const unknown=fresh();unknown.faces[0].base='unknown';assert.throws(()=>clean(unknown),/invalid/);
});
test('capacity failures never partly modify the current workspace',()=>{
  const current=fresh();for(let i=0;i<53;i++)current.faces.push({...current.faces[0],id:'v-existing-'+i,name:'Variant '+i});
  const incoming=fresh();incoming.faces[0].name='Import A';incoming.faces[1].name='Import B';
  const before=JSON.stringify(current);assert.throws(()=>merge(current,incoming,makeId),/60 faces/);assert.equal(JSON.stringify(current),before);
});
