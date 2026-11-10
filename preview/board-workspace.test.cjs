const {test}=require('node:test'),assert=require('node:assert/strict'),vm=require('node:vm'),fs=require('node:fs');
async function dashboard(){
  const state={faces:[{id:'a',background:'#000000'}],selected:'a'},calls=[],timers=new Map();let next=0;
  const element=()=>({get style(){return {};},set style(value){},setAttribute(){},after(){}}),window={FaceStudioState:{clean:x=>structuredClone(x)},FaceScene:{clone:structuredClone,compile:f=>new Uint8Array([f.background==='#000000'?0:1])},BoardEditorBoot:[]};
  const context={window,document:{createElement:element,querySelector:element,getElementById:element},localStorage:{setItem(){}},setInterval(){},setTimeout(fn,ms){timers.set(++next,{fn,ms});return next;},clearTimeout:id=>timers.delete(id),URLSearchParams,Date,fetch:async(path,options)=>{
    calls.push({path,options});return {ok:true,status:200,json:async()=>path==='/api/workspace'&&!options?.method?{revision:1,state}:{revision:2}};
  }};
  await vm.runInNewContext(fs.readFileSync(__dirname+'/board-workspace.js','utf8'),context);
  return {state,window,calls,timers,async run(ms){for(const [id,timer]of [...timers])if(timer.ms===ms){timers.delete(id);await timer.fn();}}};
}
test('loading and automatic weather refresh do not replace the displayed menu face',async()=>{
  const d=await dashboard();d.window.BoardWorkspace.live(structuredClone(d.state));await d.run(60);
  assert.deepEqual(d.calls.map(c=>c.path),['/api/workspace']);
  const edited=structuredClone(d.state);edited.faces[0].background='#ff0000';d.window.SharedWorkspace.save(edited);await d.run(60);await d.run(200);
  assert.equal(d.calls.filter(c=>c.path==='/api/scene').length,1);
  d.window.BoardWorkspace.resetPreview();d.window.BoardWorkspace.live(edited);await d.run(60);
  assert.equal(d.calls.filter(c=>c.path==='/api/scene').length,1);
});
test('showing a menu face cancels queued editor previews',async()=>{
  const d=await dashboard(),edited=structuredClone(d.state);edited.faces[0].background='#ff0000';
  d.window.BoardWorkspace.live(edited);await d.window.BoardWorkspace.beginMenu();await d.run(60);
  assert.equal(d.calls.filter(c=>c.path==='/api/scene').length,0);
  d.window.BoardWorkspace.live(edited);await d.run(60);
  assert.equal(d.calls.filter(c=>c.path==='/api/scene').length,0);
  d.window.BoardWorkspace.endMenu();
});
