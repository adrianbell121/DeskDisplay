/* Editable widget model, deterministic rasterizer, and USB scene compiler. */
(function(root){
  'use strict';
  const E=typeof module!=='undefined'&&module.exports?require('./engine.js'):root.FaceLab;
  const D=typeof module!=='undefined'&&module.exports?require('./widget-data.js'):root.WidgetData;
  const A=typeof module!=='undefined'&&module.exports?require('./animation.js'):root.WidgetAnimation;
  const I=typeof module!=='undefined'&&module.exports?require('./island-assets.js'):root.IslandAssets;
  const clone=value=>JSON.parse(JSON.stringify(value));
  const types=['clock','date','seconds','text','box','ellipse','wave','boat','palm','bird','orbit','rain','mountains','moon','flower','fireflies','sprite','weather','forecast','calendar','island'];
  const effects=['none','bob','sway','drift','blink','orbit'];
  const palette=['#000000','#edf1da','#b7cfbd','#44c6cb','#126976','#407c59','#a79161','#79583b','#e6b05a','#d4a3d9','#89a7ef','#ef7167','#293c53','#ffffff','#94e780'];
  const num=(n,min,max,fallback)=>typeof n==='number'&&Number.isFinite(n)?Math.max(min,Math.min(max,n)):fallback;
  const hex=(s,fallback)=>typeof s==='string'&&/^#[\da-f]{6}$/i.test(s)?s:fallback;
  const uid=()=> 'w-'+Date.now().toString(36)+'-'+Math.random().toString(36).slice(2,9);
  function cleanWidget(raw){
    if(!raw||!types.includes(raw.type))throw new Error('Unknown widget type.');
    const w={id:typeof raw.id==='string'?raw.id.slice(0,80):uid(),name:String(raw.name||raw.type).slice(0,60),type:raw.type,x:Math.round(num(raw.x,-63,63,0)),y:Math.round(num(raw.y,-63,63,0)),w:Math.round(num(raw.w,1,64,16)),h:Math.round(num(raw.h,1,64,16)),color:hex(raw.color,'#dde6d9'),secondary:hex(raw.secondary,'#44c6cb'),visible:raw.visible!==false,locked:raw.locked===true,text:String(raw.text||'Hello').slice(0,80),font:raw.font==='block'?'block':'segment',format:raw.format==='24'?'24':'12',flipX:raw.flipX===true,flipY:raw.flipY===true,rotation:[0,90,180,270].includes(raw.rotation)?raw.rotation:0,effect:effects.includes(raw.effect)?raw.effect:'none',speed:num(raw.speed,0,4,1),amount:num(raw.amount,0,10,1),period:Math.round(num(raw.period,1,16,4))};
    if(w.type==='island')w.islandPart=Math.round(num(raw.islandPart,0,11,0));
    w.dateFormat=typeof raw.dateFormat==='string'?raw.dateFormat.slice(0,80):'MMM D';
    w.binding=D.bindings.includes(raw.binding)?raw.binding:({date:'clock.date',weather:'weather.current',forecast:'weather.forecast',calendar:'calendar.events'}[w.type]||'none');
    w.prefix=String(raw.prefix||'').slice(0,40);w.suffix=String(raw.suffix||'').slice(0,40);w.eventLimit=raw.eventLimit===2?2:3;w.animation=A.clean(raw.animation);
    if(w.type==='sprite'){
      const s=raw.sprite;
      if(!s||!Number.isInteger(s.width)||!Number.isInteger(s.height)||s.width<1||s.width>64||s.height<1||s.height>64||!Array.isArray(s.frames)||!s.frames.length||s.frames.length>32||!Array.isArray(s.palette)||!s.palette.length||s.palette.length>15)throw new Error('Sprite needs dimensions 1–64, 1–32 frames, and 1–15 colors.');
      const colors=s.palette.map(c=>{if(!/^#[\da-f]{6}$/i.test(c))throw new Error('Invalid sprite color.');return c;});
      const frames=s.frames.map(f=>{if(typeof f!=='string'||f.length!==s.width*s.height||!/^[\da-f]+$/i.test(f)||[...f].some(p=>parseInt(p,16)>colors.length))throw new Error('Sprite frame has invalid pixels or palette indices.');return f.toLowerCase();});
      w.sprite={width:s.width,height:s.height,palette:colors,frames,fps:Math.round(num(s.fps,1,30,6))};
    }
    return w;
  }
  function make(type,name,x,y,w,h,extra={}){return cleanWidget({type,name,x,y,w,h,...extra});}
  const templates=[
    make('clock','Live clock',5,15,54,14),make('date','Live date',17,4,31,5,{color:'#86a59e'}),make('seconds','Seconds track',3,62,58,1,{color:'#44c6cb'}),make('text','Text label',16,10,31,5,{text:'HELLO'}),
    make('wave','Water ribbon',0,49,64,12,{color:'#44c6cb'}),make('boat','Sailboat',41,39,15,17,{color:'#e5e9da',secondary:'#9d6f45'}),make('palm','Palm tree',5,32,22,21,{color:'#42805b',secondary:'#937748'}),make('bird','Flying bird',18,33,9,7,{color:'#9bb9b1'}),
    make('box','Rectangle',8,51,25,3,{color:'#ad9966'}),make('ellipse','Oval',14,45,28,10,{color:'#ad9966'}),make('orbit','Solar orbit',3,33,58,28,{color:'#f4b76a'}),make('rain','Rain & ripples',3,33,58,28,{color:'#8caaf7'}),make('mountains','Mountain range',0,33,64,29,{color:'#8ccca9'}),make('moon','Crescent moon',44,32,9,9),make('flower','Flower',7,42,9,20,{color:'#d3acdc',secondary:'#3a6b45'}),make('fireflies','Fireflies',3,32,58,27,{color:'#d7ee93'}),
    make('sprite','Pixel sprite',24,24,16,16,{sprite:{width:16,height:16,palette:palette.slice(),frames:['0'.repeat(256)],fps:6}}),
    make('weather','Weather now',0,28,64,32,{color:'#e8eee6',secondary:'#8ed9e6'}),make('forecast','Five-day forecast',0,22,64,42,{secondary:'#8ed9e6'}),make('calendar','Calendar alerts',0,22,64,42,{secondary:'#d1b5ed'})
  ];
  const islandWidgets=I.specs.map(([name,x,y,w,h,color,secondary],islandPart)=>make('island',name,x,y,w,h,{color,secondary,islandPart}));
  templates.push(...islandWidgets);
  templates.forEach((w,i)=>w.id='lib-'+w.type+(w.type==='island'?'-'+w.islandPart:''));
  function nativeSize(w){if(w.type==='clock')return [54,14];if(w.type==='date')return [31,5];if(w.type==='seconds')return [58,1];if(w.type==='text')return [Math.max(3,w.text.length*4-1),5];if(w.type==='sprite')return [w.sprite.width,w.sprite.height];return {wave:[64,12],boat:[15,17],palm:[22,21],bird:[9,7],orbit:[58,28],rain:[58,28],mountains:[64,29],moon:[9,9],flower:[9,20],fireflies:[58,27]}[w.type]||[w.w,w.h];}
  function rgba(hex,strength=1){const n=parseInt(hex.slice(1),16);return [(n>>16)*strength,((n>>8)&255)*strength,(n&255)*strength];}
  function rgb565(col){return ((Math.max(0,Math.min(255,col[0]))>>3)<<11)|((Math.max(0,Math.min(255,col[1]))>>2)<<5)|(Math.max(0,Math.min(255,col[2]))>>3);}
  function expanded(n){return [Math.round((n>>11)*255/31),Math.round(((n>>5)&63)*255/63),Math.round((n&31)*255/31)];}
  function raster(w,seconds,date){
    if(w.type==='island')return I.raster(w,seconds,date);
    let [nw,nh]=nativeSize(w);if(w.type==='date'){nw=Math.max(3,D.formatDate(date,w.dateFormat).length*4-1);nh=5;}if(w.type==='text')nw=Math.max(3,D.textValue(w,date).length*4-1);if(['weather','forecast','calendar'].includes(w.type)){nw=64;nh=w.type==='weather'?32:42;}
    const raw=new Uint8ClampedArray(nw*nh*4),ink=rgba(w.color),accent=rgba(w.secondary),phase=seconds*w.speed*Math.PI*2/w.period;
    function dot(x,y,c){x=Math.round(x);y=Math.round(y);if(x<0||y<0||x>=nw||y>=nh)return;const i=(y*nw+x)*4;const col=expanded(rgb565(c));raw[i]=col[0];raw[i+1]=col[1];raw[i+2]=col[2];raw[i+3]=255;}
    function box(x,y,width,height,c){for(let j=0;j<height;j++)for(let i=0;i<width;i++)dot(x+i,y+j,c);}
    function line(x,y,tx,ty,c){x=Math.round(x);y=Math.round(y);tx=Math.round(tx);ty=Math.round(ty);const dx=Math.abs(tx-x),sx=x<tx?1:-1,dy=-Math.abs(ty-y),sy=y<ty?1:-1;let e=dx+dy;for(;;){dot(x,y,c);if(x===tx&&y===ty)break;const a=e*2;if(a>=dy){e+=dy;x+=sx;}if(a<=dx){e+=dx;y+=sy;}}}
    function text(s,x,y,c){for(const ch of s){const g=E.glyphs[ch]||E.glyphs[ch.toUpperCase()]||'';for(let j=0;j<5;j++)for(let i=0;i<3;i++)if(g[j*3+i]==='1')dot(x+i,y+j,c);x+=4;}}
    function digit(n,x,y){if(w.font==='block'){for(let j=0;j<7;j++)for(let i=0;i<5;i++)if(E.bitmapDigits[n][j]&(1<<(4-i)))box(x+i*2,y+j*2,2,2,ink);return;}const s=E.segments[n];for(const [mask,b] of [[1,[1,0,5,2]],[2,[5,1,2,5]],[4,[5,7,2,5]],[8,[1,11,5,2]],[16,[0,7,2,5]],[32,[0,1,2,5]],[64,[1,6,5,2]]])if(s&mask)box(x+b[0],y+b[1],b[2],b[3],ink);}
    if(w.type==='clock'){
      const hour=w.format==='24'?date.getHours():date.getHours()%12||12,minute=date.getMinutes();
      if(w.font==='block'){digit(Math.floor(hour/10),0,0);digit(hour%10,12,0);digit(Math.floor(minute/10),32,0);digit(minute%10,44,0);if(date.getSeconds()%2===0){box(26,3,2,2,accent);box(26,9,2,2,accent);}}
      else{if(hour>=10||w.format==='24')digit(Math.floor(hour/10),0,0);digit(hour%10,9,0);digit(Math.floor(minute/10),22,0);digit(minute%10,31,0);dot(18,4,accent);dot(18,9,accent);if(w.format==='12')text(date.getHours()<12?'AM':'PM',42,6,accent);}
    }else if(w.type==='date'){
      const s=D.formatDate(date,w.dateFormat);text(s,0,0,ink);
    }else if(w.type==='seconds'){box(0,0,nw,1,rgba(w.color,.15));box(0,0,Math.floor(date.getSeconds()*58/59),1,ink);
    }else if(w.type==='text'){text(D.textValue(w,date),0,0,ink);
    }else if(['weather','forecast','calendar'].includes(w.type)){
      const data=D.getData(date);
      function icon(condition,x,y){const sun=[238,192,90];if(condition==='sunny'){box(x+3,y+2,3,3,sun);dot(x+4,y,sun);dot(x+4,y+6,sun);dot(x+1,y+3,sun);dot(x+7,y+3,sun);}else{box(x+1,y+2,7,3,accent);box(x+3,y,3,5,accent);if(condition==='rainy'){line(x+2,y+6,x+1,y+7,accent);line(x+6,y+6,x+5,y+7,accent);}if(condition==='snowy'){dot(x+2,y+7,ink);dot(x+5,y+6,ink);dot(x+7,y+7,ink);}}}
      if(w.type==='weather'){text('NOW',2,2,accent);icon(data.current.condition,4,12);text(Math.round(data.current.tempF)+'F',20,12,ink);text('PREC '+Math.round(data.current.precipProbability)+'%',2,25,accent);}
      if(w.type==='forecast'){data.days.slice(0,5).forEach((day,i)=>{const x=i*13;const dayDate=new Date(day.date+'T12:00:00');text(D.formatDate(dayDate,'ddd').toUpperCase(),x,0,accent);icon(day.condition,x+1,8);text(String(Math.round(day.tempF))+'F',x,18,ink);});text('PREC%',0,24,rgba(w.color,.6));text('0-100',44,24,rgba(w.color,.6));line(0,40,63,40,rgba(w.secondary,.25));for(let x=0;x<63;x++){const y=40-D.curve(data.hourly,x/63*(data.hourly.length-1))*.1,ny=40-D.curve(data.hourly,(x+1)/63*(data.hourly.length-1))*.1;line(x,y,x+1,ny,accent);}}
      if(w.type==='calendar'){text('AGENDA',2,0,accent);line(1,7,62,7,rgba(w.secondary,.4));const events=D.agenda(data.events,date,w.eventLimit);if(!events.length)text('NO EVENTS',2,18,ink);events.forEach((event,i)=>{const today=event.start.slice(0,10)===D.formatDate(date,'YYYY-MM-DD'),start=new Date(event.start);text((today?'TOD ':'TMR ')+(event.allDay?'ALL DAY':D.formatDate(start,'h:mm A')),2,10+i*11,accent);text(event.title.slice(0,15),2,15+i*11,ink);});}
    }else if(w.type==='sprite'){
      const s=w.sprite,f=s.frames[Math.floor(seconds*w.speed*s.fps)%s.frames.length];for(let i=0;i<f.length;i++){const n=parseInt(f[i],16);if(n)dot(i%nw,Math.floor(i/nw),rgba(s.palette[n-1]));}
    }else{
      // Procedural widgets draw in a normalized local coordinate space.
      const D=(x,y,c)=>dot(x*(nw-1)/63,y*(nh-1)/63,c),L=(x,y,tx,ty,c)=>line(x*(nw-1)/63,y*(nh-1)/63,tx*(nw-1)/63,ty*(nh-1)/63,c);
      if(w.type==='box')box(0,0,nw,nh,ink);
      if(w.type==='ellipse'){for(let y=0;y<nh;y++)for(let x=0;x<nw;x++)if(((x+.5-nw/2)/(nw/2))**2+((y+.5-nh/2)/(nh/2))**2<=1)dot(x,y,ink);}
      if(w.type==='wave')for(let x=0;x<nw;x++){const y=nh*.35+Math.sin(x*.12+phase)*nh*.17+Math.sin(x*.22-phase)*nh*.1;for(let d=0;d<Math.max(2,nh*.4);d++)dot(x,y+d,rgba(w.color,.85/(d+1)));}
      if(w.type==='boat'){
        const a=Math.sin(phase)*.09,cs=Math.cos(a),sn=Math.sin(a);
        for(let y=-10;y<=1;y++)for(let x=-5;x<=5;x++){let c=null;if(y>=-9&&y<=-2&&x===0)c=accent;if(y>=-8&&y<=-2&&x<0&&x>=-Math.trunc((y+10)/2))c=ink;if(y>=-6&&y<=-2&&x>0&&x<=Math.trunc((y+8)/2))c=rgba(w.color,.85);if(y===0)c=accent;if(y===1&&x>=-3&&x<=3)c=rgba(w.secondary,.7);if(c)dot(7+x*cs-y*sn,13+x*sn+y*cs+Math.sin(phase),c);}
      }
      if(w.type==='palm'){const sway=Math.sin(phase)*2;L(34,61,29+sway,17,accent);for(const [x,y,tx,ty]of [[29,17,7,8],[7,8,1,18],[29,17,51,3],[51,3,61,15],[29,17,52,21],[52,21,58,33],[29,17,12,27]])L(x+sway,y,tx+sway,ty,ink);D(28+sway,25,accent);D(34+sway,25,accent);}
      if(w.type==='bird'){const flap=Math.sin(phase*4)>0?12:-12;L(9,31+flap,31,31,ink);L(31,31,54,31+flap,ink);}
      if(w.type==='orbit'){for(let i=0;i<38;i++){const a=i*Math.PI*2/38;D(32+Math.cos(a)*30,32+Math.sin(a)*26,rgba(w.color,.22));}for(let i=0;i<3;i++){const a=phase*(i+1)+i*2;D(32+Math.cos(a)*(14+i*7),32+Math.sin(a)*(12+i*6),i===1?accent:ink);}box(nw/2-2,nh/2-2,4,4,ink);}
      if(w.type==='rain'){L(0,55,63,55,rgba(w.color,.3));for(let i=0;i<12;i++){const p=(seconds*w.speed/w.period+(i*.137))%1,x=(i*17+7)%60+2;L(x,p*50,x-1,p*50+5,rgba(w.color,.3+(i%3)*.25));if(p>.8)L(x-(p-.8)*30,58+(i%3),x+(p-.8)*30,58+(i%3),rgba(w.color,(1-p)*3));}}
      if(w.type==='mountains'){for(const [pts,str]of [[[[0,43],[14,1],[33,53],[46,13],[63,46]],.35],[[[0,61],[23,20],[40,57],[55,28],[63,51]],.7]])for(let i=0;i<pts.length-1;i++){const [x1,y1]=pts[i],[x2,y2]=pts[i+1];for(let x=x1;x<=x2;x++)L(x,y1+(y2-y1)*(x-x1)/(x2-x1),x,63,rgba(w.color,str));}L(9,17,14,1,ink);L(14,1,19,17,ink);}
      if(w.type==='moon'){for(let y=0;y<nh;y++)for(let x=0;x<nw;x++){const dx=(x+.5)/nw-.5,dy=(y+.5)/nh-.5;if(dx*dx+dy*dy<.24&&(dx-.23)**2+(dy+.15)**2>.18)dot(x,y,ink);}}
      if(w.type==='flower'){const sway=Math.sin(phase)*4;L(32,63,32+sway,11,accent);L(32,43,7,32,accent);D(32+sway,4,ink);D(20+sway,11,ink);D(44+sway,11,ink);D(32+sway,18,ink);D(32+sway,11,[229,182,88]);}
      if(w.type==='fireflies')for(let i=0;i<9;i++)D(6+(i*17)%51+Math.sin(phase+i)*4,6+(i*7)%49+Math.cos(phase+i)*4,rgba(w.color,.2+.8*(.5+.5*Math.sin(phase*2+i*2))));
    }
    const out=new Uint8ClampedArray(w.w*w.h*4),cx=(w.w-1)/2,cy=(w.h-1)/2;
    let angle=0,dx=0,dy=0,opacity=1;
    if(w.effect==='bob')dy=Math.sin(phase)*w.amount;if(w.effect==='drift')dx=Math.sin(phase)*w.amount;if(w.effect==='sway')angle+=Math.sin(phase)*w.amount*.05;if(w.effect==='orbit'){dx=Math.cos(phase)*w.amount;dy=Math.sin(phase)*w.amount;}if(w.effect==='blink')opacity=Math.sin(phase)>=0?1:0;
    const cs=Math.cos(angle),sn=Math.sin(angle);
    for(let y=0;y<w.h;y++)for(let x=0;x<w.w;x++){
      const px=x-cx-dx,py=y-cy-dy;let u=(px*cs+py*sn+cx+.5)/w.w,v=(-px*sn+py*cs+cy+.5)/w.h;
      const originalU=u;if(w.rotation===90){u=v;v=1-originalU;}if(w.rotation===180){u=1-u;v=1-v;}if(w.rotation===270){u=1-v;v=originalU;}if(w.flipX)u=1-u;if(w.flipY)v=1-v;const sx=Math.floor(u*nw),sy=Math.floor(v*nh);if(sx<0||sy<0||sx>=nw||sy>=nh||!opacity)continue;
      const src=(sy*nw+sx)*4,dst=(y*w.w+x)*4;out.set(raw.subarray(src,src+4),dst);
    }
    return out;
  }
  function render(scene,seconds,date,target){
    const out=target||new Uint8ClampedArray(16384),bg=expanded(rgb565(rgba(scene.background)));
    for(let i=0;i<out.length;i+=4){out[i]=bg[0];out[i+1]=bg[1];out[i+2]=bg[2];out[i+3]=255;}
    for(const w of scene.widgets){if(!w.visible)continue;const motion=A.at(w.animation,seconds),image=raster(w,seconds,date),width=Math.max(1,Math.min(256,Math.round(w.w*motion.scale))),height=Math.max(1,Math.min(256,Math.round(w.h*motion.scale)));for(let y=0;y<height;y++)for(let x=0;x<width;x++){const tx=Math.round(x+w.x+motion.x),ty=Math.round(y+w.y+motion.y);if(tx<0||ty<0||tx>63||ty>63)continue;const s=(Math.floor(y*w.h/height)*w.w+Math.floor(x*w.w/width))*4;if(!image[s+3]||!motion.opacity)continue;const dst=(ty*64+tx)*4;for(let c=0;c<3;c++)out[dst+c]=Math.round(out[dst+c]*(1-motion.opacity)+image[s+c]*motion.opacity);}}
    return out;
  }
  function fromLegacy(face){
    const c=E.normalize(face.config),widgets=[],add=(type,x,y,w,h,extra={})=>widgets.push(make(type,templates.find(t=>t.type===type).name,x,y,w,h,{color:c.ink,secondary:c.accent,...extra}));
    if(face.base==='island'){add('wave',3,49+c.sceneY,58,12,{color:c.accent,speed:c.motion});add('box',6,51+c.sceneY,28,3,{color:'#ad9966'});add('palm',6,32+c.sceneY,20,20,{color:'#42805b',secondary:'#937748',speed:c.motion});add('palm',18,36+c.sceneY,19,16,{color:'#42805b',secondary:'#937748',speed:c.motion});add('boat',40,39+c.sceneY,16,17,{color:'#e5e9da',secondary:'#9d6f45',speed:c.motion});add('bird',16,32+c.sceneY,5,4,{color:'#9bb9b1',speed:c.motion});add('bird',26,37+c.sceneY,5,4,{color:'#9bb9b1',speed:c.motion});}
    if(face.base==='ribbon')add('wave',0,45+c.sceneY,64,16,{color:c.accent,speed:c.motion});
    for(const [base,type]of [['orbit','orbit'],['rain','rain'],['peaks','mountains']])if(face.base===base)add(type,0,33+c.sceneY,64,29,{color:c.accent,speed:c.motion});
    if(face.base==='peaks')add('moon',46,32+c.sceneY,7,7);
    if(face.base==='garden'){for(let i=0;i<5;i++)add('flower',5+i*12,42+c.sceneY+(i*7)%9,8,20-(i*7)%9,{color:i%2?c.ink:c.accent,secondary:'#3a6b45',speed:c.motion});add('fireflies',3,32+c.sceneY,58,27,{color:'#d7ee93',speed:c.motion});}
    add('clock',5,c.timeY,54,14,{font:c.digitStyle,format:c.format});if(c.showDate)add('date',17,4,31,5,{color:c.accent});if(c.showSeconds)add('seconds',3,62,58,1,{color:c.accent});
    return {id:face.id,name:face.name,background:c.background,favorite:face.favorite===true,notes:face.notes||'',widgets};
  }
  function cleanScene(raw){if(!raw||!Array.isArray(raw.widgets)||raw.widgets.length>32)throw new Error('A face can have up to 32 widgets.');const widgets=raw.widgets.map(cleanWidget),ids=new Set();for(const w of widgets){if(ids.has(w.id))w.id=uid();ids.add(w.id);}return {id:String(raw.id||uid()).slice(0,80),name:String(raw.name||'Untitled face').slice(0,60),background:hex(raw.background,'#03070b'),favorite:raw.favorite===true,notes:String(raw.notes||'').slice(0,2000),widgets};}
  function compile(scene){
    // Header: FLS2, layer count u16, background u16. Each layer has 16-byte header.
    const chunks=[],header=new Uint8Array(8),hv=new DataView(header.buffer);header.set([70,76,83,50]);const widgets=scene.widgets.filter(w=>w.visible);hv.setUint16(4,widgets.length,true);hv.setUint16(6,rgb565(rgba(scene.background)),true);chunks.push(header);
    for(const w of widgets){
      if(['weather','forecast','calendar'].includes(w.type)||w.binding.startsWith('weather.')||w.binding==='calendar.events')throw new Error('Weather/calendar use sample or connected desktop data. Convert to pixels for an explicit snapshot; live provider sync to the board is not installed.');
      if(w.animation?.keys.length)throw new Error('Keyframe tracks are editable in Studio. Bake a face animation for a scene package; live clock tracks cannot be exported as live data.');
      if(w.type==='date'&&w.dateFormat!=='MMM D')throw new Error('Custom date patterns are supported in Studio. The current board scene player supports MMM D only.');
      const dynamic=['clock','date','seconds','island'].includes(w.type),kind=w.type==='island'?5+w.islandPart:w.type==='clock'?(w.font==='block'?2:1):w.type==='date'?3:w.type==='seconds'?4:0;
      if(w.type==='island'&&w.islandPart===0&&w.format!=='12')throw new Error('The original Island clock uses 12-hour format. Use the standard Live clock widget for 24-hour format.');
      if(w.type==='island'&&w.speed!==1)throw new Error('Island assets currently keep their original animation speed on the board. Set speed to 1 before upload.');
      if(dynamic&&(w.effect!=='none'||w.rotation||w.flipX||w.flipY))throw new Error('Live clock/date/seconds support move, resize, colors, and format on USB. Remove their extra effect or convert them to a sprite.');
      const frames=dynamic?1:Math.max(1,Math.min(32,w.type==='sprite'?w.sprite.frames.length:w.speed===0||['box','ellipse','text','mountains','moon'].includes(w.type)&&w.effect==='none'?1:32));
      const fps=dynamic?1:w.type==='sprite'?Math.max(1,Math.min(30,Math.round(w.sprite.fps*w.speed)||1)):Math.max(1,Math.round(frames/w.period));
      const colors=[],indices=[];
      if(!dynamic)for(let f=0;f<frames;f++){const image=raster(w,f/fps,new Date(2026,0,1));for(let i=0;i<image.length;i+=4){if(!image[i+3]){indices.push(0);continue;}const col=rgb565(image.subarray(i,i+3));let ix=colors.indexOf(col);if(ix<0){if(colors.length>=255)throw new Error('This widget uses too many colors.');ix=colors.length;colors.push(col);}indices.push(ix+1);}}
      const bytes=new Uint8Array(16+colors.length*2+indices.length),v=new DataView(bytes.buffer);bytes[0]=dynamic?kind:0;v.setInt8(1,w.x);v.setInt8(2,w.y);bytes[3]=w.w;bytes[4]=w.h;bytes[5]=frames;bytes[6]=fps;bytes[7]=w.format==='24'?24:12;v.setUint16(8,rgb565(rgba(w.color)),true);v.setUint16(10,rgb565(w.type==='seconds'?rgba(w.color,.15):rgba(w.secondary)),true);v.setUint16(12,colors.length,true);v.setUint16(14,0,true);colors.forEach((col,i)=>v.setUint16(16+i*2,col,true));bytes.set(indices,16+colors.length*2);chunks.push(bytes);
    }
    const length=chunks.reduce((n,c)=>n+c.length,0);if(length>262144)throw new Error('Scene exceeds the 256 KB board limit. Reduce widget sizes or animation frames.');const result=new Uint8Array(length);let offset=0;for(const c of chunks){result.set(c,offset);offset+=c.length;}return result;
  }
  function boardIsland(){return cleanScene({id:'island-board-v1',name:'Island — board layout',background:'#000000',notes:'Original board spacing. Twelve independent elements. Birds use a repeatable preview pause and random 20–30 second pauses on the board.',widgets:clone(islandWidgets)});}
  const api={boardIsland,clone,types,effects,palette,templates,uid,cleanWidget,cleanScene,fromLegacy,nativeSize,raster,render,compile,rgb565};if(typeof module!=='undefined'&&module.exports)module.exports=api;else root.FaceScene=api;
})(typeof globalThis!=='undefined'?globalThis:this);
