/* Pure, deterministic 64x64 RGB565 concept renderer. No browser APIs required. */
(function (root) {
  'use strict';
  const glyphs = {
    '0':'111101101101111','1':'010110010010111','2':'110001111100111','3':'110001111001110',
    '4':'101101111001001','5':'111100110001110','6':'011100111101111','7':'111001010010010',
    '8':'111101111101111','9':'111101111001110',A:'010101111101101',B:'110101110101110',
    C:'011100100100011',D:'110101101101110',E:'111100110100111',F:'111100110100100',
    G:'011100101101011',H:'101101111101101',I:'111010010010111',J:'001001001101010',
    K:'101101110101101',L:'100100100100111',M:'101111111101101',N:'101111111111101',
    O:'111101101101111',P:'110101110100100',Q:'010101101111011',R:'110101110101101',
    S:'011100010001110',T:'111010010010010',U:'101101101101111',V:'101101101101010',
    W:'101101111111101',X:'101101010101101',Y:'101101010010010',Z:'111001010100111',
    ':':'000010000010000','-':'000000111000000','/':'001001010100100',
    a:'000011001111111',b:'100100110101110',c:'000011100100011',e:'000011111100111',
    g:'000011101111001',l:'100100100100011',n:'000110101101101',o:'000011101101111',
    p:'000110101110100',r:'000110101100100',t:'010111010010011',u:'000101101101111',
    v:'000101101101010',y:'000101101111001'
  };
  const bitmapDigits = [[14,17,19,21,25,17,14],[4,12,4,4,4,4,14],[14,17,1,2,4,8,31],[30,1,1,14,1,1,30],[2,6,10,18,31,2,2],[31,16,16,30,1,1,30],[14,16,16,30,17,17,14],[31,1,2,4,8,8,8],[14,17,17,14,17,17,14],[14,17,17,15,1,1,14]];
  const segments = [63,6,91,79,102,109,125,7,127,111];
  const defaults = {ink:'#dde6d9',accent:'#44c6cb',background:'#03070b',digitStyle:'segment',format:'12',timeY:15,sceneY:0,motion:1,showDate:true,showSeconds:false};
  const presets = [
    {id:'island',name:'Island',kind:'Firmware study',description:'A quiet shore, rocking sailboat, and birds that leave and return. Scrub past 24 seconds to see them fly.',config:{}},
    {id:'ribbon',name:'Ribbon',kind:'Firmware study',description:'The original cyan wave, with generous pixel digits and a seconds track.',config:{digitStyle:'block',ink:'#d8f0ff',accent:'#34d2da',background:'#000000',timeY:17,showSeconds:true}},
    {id:'orbit',name:'Orbit',kind:'New concept',description:'A small solar system circles below the time. Gentle trails and a warm central star.',config:{accent:'#f4b76a',ink:'#f1e3cb'}},
    {id:'rain',name:'After rain',kind:'New concept',description:'Slow rain over a still horizon, with tiny ripples that dissolve into the night.',config:{accent:'#8caaf7',ink:'#dee7ff'}},
    {id:'peaks',name:'Alpine',kind:'New concept',description:'Layered mountains under a drifting moon. A little landscape with room to breathe.',config:{accent:'#8ccca9',ink:'#e1e9d3'}},
    {id:'garden',name:'Firefly garden',kind:'New concept',description:'Small flowers, gently swaying stems, and a handful of fireflies after dark.',config:{accent:'#d3acdc',ink:'#f1e3e9'}}
  ];
  function normalize(input) {
    const c = {...defaults};
    if (!input || typeof input !== 'object') return c;
    for (const key of ['ink','accent','background']) if (/^#[0-9a-f]{6}$/i.test(input[key])) c[key] = input[key];
    if (['segment','block'].includes(input.digitStyle)) c.digitStyle=input.digitStyle;
    if (['12','24'].includes(input.format)) c.format=input.format;
    for (const [key,min,max] of [['timeY',11,25],['sceneY',-5,5],['motion',0,2]]) {
      if (typeof input[key] === 'number' && Number.isFinite(input[key])) c[key]=Math.max(min,Math.min(max,input[key]));
    }
    c.timeY=Math.round(c.timeY); c.sceneY=Math.round(c.sceneY);
    for (const key of ['showDate','showSeconds']) if (typeof input[key] === 'boolean') c[key]=input[key];
    return c;
  }
  function color(hex, strength=1) {
    const n=parseInt(hex.slice(1),16);
    return [Math.min(255,Math.max(0,Math.round((n>>16)*strength))),Math.min(255,Math.max(0,Math.round(((n>>8)&255)*strength))),Math.min(255,Math.max(0,Math.round((n&255)*strength)))];
  }
  function render(face, input, seconds, date, target) {
    const c=normalize(input), data=target || new Uint8ClampedArray(64*64*4);
    const ink=color(c.ink), accent=color(c.accent), bg=color(c.background), muted=color(c.accent,.68), t=seconds*c.motion;
    function pixel(x,y,col) {
      x=Math.round(x);y=Math.round(y);
      if(x<0||x>63||y<0||y>63)return;
      const i=(y*64+x)*4;
      // Quantize to the panel's RGB565, then expand for browser display.
      data[i]=Math.round((col[0]>>3)*255/31);data[i+1]=Math.round((col[1]>>2)*255/63);data[i+2]=Math.round((col[2]>>3)*255/31);data[i+3]=255;
    }
    function box(x,y,w,h,col){for(let j=0;j<h;j++)for(let i=0;i<w;i++)pixel(x+i,y+j,col);}
    function line(x,y,tx,ty,col){x=Math.round(x);y=Math.round(y);tx=Math.round(tx);ty=Math.round(ty);let dx=Math.abs(tx-x),sx=x<tx?1:-1,dy=-Math.abs(ty-y),sy=y<ty?1:-1,err=dx+dy;for(;;){pixel(x,y,col);if(x===tx&&y===ty)break;const e=err*2;if(e>=dy){err+=dy;x+=sx;}if(e<=dx){err+=dx;y+=sy;}}}
    function text(str,x,y,col){for(const ch of str){const g=glyphs[ch]||glyphs[ch.toUpperCase()]||'';for(let j=0;j<5;j++)for(let i=0;i<3;i++)if(g[j*3+i]==='1')pixel(x+i,y+j,col);x+=4;}}
    function centered(str,y,col){text(str,Math.floor((65-str.length*4)/2),y,col);}
    function digit(n,x,y,col){
      if(c.digitStyle==='block'){for(let j=0;j<7;j++)for(let i=0;i<5;i++)if(bitmapDigits[n][j]&(1<<(4-i)))box(x+i*2,y+j*2,2,2,col);return;}
      const s=segments[n];
      if(s&1)box(x+1,y,5,2,col);if(s&2)box(x+5,y+1,2,5,col);if(s&4)box(x+5,y+7,2,5,col);if(s&8)box(x+1,y+11,5,2,col);if(s&16)box(x,y+7,2,5,col);if(s&32)box(x,y+1,2,5,col);if(s&64)box(x+1,y+6,5,2,col);
    }
    box(0,0,64,64,bg);
    // Scenes remain in their own lower region; the layout sliders may intentionally overlap.
    const sy=c.sceneY;
    function wave(x){return 53+sy+Math.sin(x*.085+t*.65)*1.6+Math.sin(x*.14-t*.45);}
    if(face==='island'){
      for(let x=3;x<=60;x++)for(let d=0;d<5;d++)pixel(x,Math.round(wave(x))+d,color(c.accent,.65/(d+1)));
      box(8,51+sy,24,1,[173,153,102]);box(6,52+sy,28,1,[173,153,102]);box(8,53+sy,25,1,[130,118,78]);
      function palm(x,ground,h){const crown=ground-h;const leaf=[66,128,91];line(x,ground,x-2,crown,[147,119,72]);line(x-2,crown,x-7,crown-2,leaf);line(x-7,crown-2,x-9,crown,leaf);line(x-2,crown,x+3,crown-3,leaf);line(x+3,crown-3,x+5,crown-1,leaf);line(x-2,crown,x+3,crown+1,leaf);line(x+3,crown+1,x+4,crown+3,leaf);line(x-2,crown,x-6,crown+2,leaf);pixel(x-2,crown+2,[119,90,54]);pixel(x,crown+2,[119,90,54]);}
      palm(16,50+sy,14);palm(26,50+sy,10);
      const a=Math.sin(t*1.2)*.075,cs=Math.cos(a),sn=Math.sin(a),water=wave(47);
      for(let y=-10;y<=1;y++)for(let x=-5;x<=5;x++){
        let col=null;
        if(y>=-9&&y<=-2&&x===0)col=[174,161,127];
        if(y>=-8&&y<=-2&&x<0&&x>=-Math.trunc((y+10)/2))col=[229,233,218];
        if(y>=-6&&y<=-2&&x>0&&x<=Math.trunc((y+8)/2))col=[207,221,213];
        if(y===0)col=[157,111,69];if(y===1&&x>=-3&&x<=3)col=[113,84,59];
        if(col)pixel(47+x*cs-y*sn,water-1+x*sn+y*cs,col);
      }
      // Fixed 52-second schedule makes pause, restart, and scrubbing reproducible.
      const cycle=t%52;
      if(cycle<28||cycle>=48)for(let i=0;i<2;i++){
        let p=cycle<24?0:cycle<28?Math.max(0,Math.min(1,(cycle-24-i*.35)/3.5)):1-Math.max(0,Math.min(1,(cycle-48-i*.35)/3.5));
        const x=18+i*10+p*53,y=34+sy+i*5-Math.sin(p*Math.PI/2)*(3+i*2),wing=p>0&&p<1&&Math.sin(t*17+i)>0?1:-1;
        pixel(x,y,[155,185,177]);pixel(x-1,y+wing,[155,185,177]);pixel(x+1,y+wing,[155,185,177]);
      }
    } else if(face==='ribbon'){
      for(let x=0;x<64;x++){const y=51+sy+Math.trunc(Math.sin(x*.085+t)*3+Math.sin(x*.14-t*.7)*2);for(let d=0;d<5;d++)pixel(x,y+d,color(c.accent,.85/(d+1)));}
    } else if(face==='orbit'){
      for(let j=0;j<34;j++){const a=j/34*Math.PI*2;pixel(32+Math.cos(a)*24,47+sy+Math.sin(a)*10,color(c.accent,.17));}
      box(30,45+sy,5,5,accent);box(31,44+sy,3,7,accent);box(29,46+sy,7,3,accent);
      for(let i=0;i<3;i++){const a=t*(.35+i*.12)+i*2.1,r=13+i*5;const x=32+Math.cos(a)*r,y=47+sy+Math.sin(a)*(5+i*2);box(x,y,i===1?3:2,i===1?3:2,i===1?ink:muted);}
      for(let i=0;i<9;i++)pixel((i*19+3)%62,34+sy+(i*7)%25,color(c.ink,.2+.15*Math.sin(t+i)));
    } else if(face==='rain'){
      line(3,55+sy,60,55+sy,color(c.accent,.3));
      for(let i=0;i<14;i++){const x=(i*17+7)%60+2,p=(t*(.13+(i%3)*.035)+i*.137)%1,y=33+sy+p*22;line(x,y,x-1,y+2,color(c.accent,.35+(i%3)*.2));if(p>.85){const r=(p-.85)*25;line(x-r,57+sy+(i%3),x+r,57+sy+(i%3),color(c.accent,(1-p)*3));}}
    } else if(face==='peaks'){
      const layers=[{points:[[0,53],[14,34],[33,57],[46,38],[64,54]],strength:.25},{points:[[0,60],[23,41],[40,59],[55,45],[64,55]],strength:.5}];
      for(const layer of layers)for(let i=0;i<layer.points.length-1;i++){const [x1,y1]=layer.points[i],[x2,y2]=layer.points[i+1];for(let x=x1;x<x2;x++){const y=y1+(y2-y1)*(x-x1)/(x2-x1);line(x,y+sy,x,61+sy,color(c.accent,layer.strength));}}
      line(10,39+sy,14,34+sy,ink);line(14,34+sy,18,39+sy,ink);line(20,44+sy,23,41+sy,color(c.ink,.7));
      const mx=48+Math.sin(t*.08)*4;box(mx,33+sy,4,4,ink);box(mx+2,32+sy,3,4,bg);
      for(let i=0;i<9;i++)pixel((i*23+3)%64,32+sy+(i*3)%9,color(c.ink,.25+.12*Math.sin(t*.8+i)));
    } else if(face==='garden'){
      for(let i=0;i<5;i++){const x=8+i*12,y=43+sy+(i*7)%9,sway=Math.sin(t*.8+i)*1.2;line(x,60+sy,x+sway,y,[58,107,69]);line(x,54+sy,x-3,51+sy,[58,107,69]);const col=i%2?ink:accent;pixel(x+sway,y,col);pixel(x+sway-1,y-1,col);pixel(x+sway+1,y-1,col);pixel(x+sway,y-2,col);pixel(x+sway,y-1,[229,182,88]);}
      for(let i=0;i<7;i++){const x=5+(i*17+Math.sin(t*.4+i)*4+64)%54,y=35+sy+(i*7)%18+Math.sin(t*.6+i)*2;pixel(x,y,color('#d7ee93',.35+.55*(.5+.5*Math.sin(t*1.3+i*2))));}
      line(3,61+sy,60,61+sy,[32,62,45]);
    }
    if(c.showDate){const months=['Jan','Feb','Mar','Apr','May','Jun','Jul','Aug','Sep','Oct','Nov','Dec'];centered(months[date.getMonth()]+' '+date.getDate(),4,muted);}
    const hour=c.format==='24'?date.getHours():date.getHours()%12||12,minute=date.getMinutes(),y=c.timeY;
    if(c.digitStyle==='block'){
      digit(Math.floor(hour/10),5,y,ink);digit(hour%10,17,y,ink);digit(Math.floor(minute/10),37,y,ink);digit(minute%10,49,y,ink);
      if(date.getSeconds()%2===0){box(31,y+3,2,2,accent);box(31,y+9,2,2,accent);}
    }else{
      if(hour>=10||c.format==='24')digit(Math.floor(hour/10),5,y,ink);digit(hour%10,14,y,ink);digit(Math.floor(minute/10),27,y,ink);digit(minute%10,36,y,ink);pixel(23,y+4,muted);pixel(23,y+9,muted);if(c.format==='12')text(date.getHours()<12?'AM':'PM',47,y+6,muted);
    }
    if(c.showSeconds){box(3,62,58,1,color(c.accent,.15));box(3,62,Math.floor(date.getSeconds()*58/59),1,accent);}
    return data;
  }
  const api={presets,defaults,normalize,render};
  if(typeof module!=='undefined'&&module.exports)module.exports=api;else root.FaceLab=api;
})(typeof globalThis!=='undefined'?globalThis:this);
