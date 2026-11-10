const {test}=require('node:test'),assert=require('node:assert/strict');
const M=require('./board-face-menu.js'),S=require('./scene.js');
const sample=()=>[{id:'island',name:'Island',mode:'island'},{id:'life',name:'Game',mode:'automata'},{id:'diag',name:'Diagnostics',mode:'diagnostics'}];
test('drag ordering works in both directions without modifying the saved snapshot',()=>{
  const initial=sample(),before=JSON.stringify(initial);
  assert.deepEqual(M.reorder(initial,'island','life').map(f=>f.id),['life','island','diag']);
  assert.deepEqual(M.reorder(initial,'diag','island').map(f=>f.id),['diag','island','life']);
  assert.deepEqual(M.reorder(initial,'island',null).map(f=>f.id),['life','diag','island']);
  assert.equal(JSON.stringify(initial),before);assert.throws(()=>M.reorder(initial,'missing','life'));
});
test('adding and updating an editable face keeps other menu entries intact',()=>{
  const initial=sample(),face=S.boardIsland(),item={id:face.id,name:face.name,mode:'custom',scene:M.hex(S.compile(face))};
  const added=M.upsert(initial,item);assert.equal(added.length,4);assert.deepEqual(added.slice(0,3),initial);
  const changed=M.upsert(added,{...item,name:'My edited island'});assert.equal(changed.length,4);assert.equal(changed[3].name,'My edited island');
  const migrated=M.upsert([{...item,id:'saved-scene'}],item,'saved-scene');assert.equal(migrated[0].id,face.id);
  const removed=M.remove(added,item.id);assert.deepEqual(removed,initial);assert.equal(face.widgets.length,12);
});
test('invalid, duplicate, empty, and oversized menus are rejected before sending',()=>{
  const initial=sample();assert.throws(()=>M.clean([]),/1 and 32/);assert.throws(()=>M.remove([initial[0]],'island'));
  assert.throws(()=>M.clean([...initial,initial[0]]),/duplicate/);
  for(const item of [{...initial[0],id:'x'.repeat(81)},{...initial[0],name:'🌴'.repeat(61)},{...initial[0],mode:'eval'},{...initial[0],mode:'custom',scene:'bad'}])assert.throws(()=>M.clean([item]));
  const scene='464c533200000000'+'00'.repeat(200000),a={id:'a',name:'A',mode:'custom',scene};
  assert.throws(()=>M.clean([a,{...a,id:'b'},{...a,id:'c'}]),/full/);
});
