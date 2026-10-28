/* Pixel-for-pixel Island assets. Keep source rectangles aligned with island_face.c. */
(function(root){
'use strict';
const specs=[
 ['Island clock',5,15,49,13,'#dde6d9','#86a59e'],['Island date',0,4,64,5,'#86a59e','#86a59e'],
 ['Island permanent water',0,59,64,5,'#05232d','#05232d'],
 ['Island distant waves',0,53,64,11,'#084054','#082034'],['Island middle waves',0,53,64,11,'#085c70','#082e42'],['Island front waves',0,53,64,11,'#08788c','#083c50'],
 ['Island shore',6,59,28,3,'#ad9966','#82764e'],['Island tall palm',7,40,16,19,'#42805b','#937748'],['Island short palm',17,44,16,15,'#42805b','#937748'],
 ['Island rigid sailboat',42,48,11,16,'#e5e9da','#9d6f45'],['Island bird one',0,38,64,14,'#9bb9b1','#9bb9b1'],['Island bird two',0,38,64,14,'#9bb9b1','#9bb9b1']
];
const glyphs={"0": 31599, "1": 11415, "2": 29671, "3": 29647, "4": 23497, "5": 31183, "6": 31215, "7": 29257, "8": 31727, "9": 31695, "A": 11245, "D": 27502, "F": 31140, "J": 4714, "M": 24557, "N": 24573, "O": 31599, "P": 31716, "S": 31183, "a": 3199, "b": 18862, "c": 3879, "e": 4071, "g": 3961, "l": 18723, "n": 3437, "o": 3951, "p": 3964, "r": 3428, "t": 11923, "u": 2927, "v": 2922, "y": 2937, "-": 448, ":": 1040};
const colors=h=>[parseInt(h.slice(1,3),16),parseInt(h.slice(3,5),16),parseInt(h.slice(5,7),16)];
const quant=c=>[(c[0]>>3)*255/31,(c[1]>>2)*255/63,(c[2]>>3)*255/31].map(Math.round);
function raster(w,seconds,date){
 const spec=specs[w.islandPart],[,bx,by,bw,bh,primary,secondary]=spec,out=new Uint8ClampedArray(w.w*w.h*4),raw=new Uint8ClampedArray(64*64*4);
 const rgb=(r,g,b)=>[r,g,b],white=rgb(221,230,217),muted=rgb(134,165,158);let offset=0;
 const round=n=>n<0?-Math.round(-n):Math.round(n);
 function dot(x,y,c){y+=offset;if(x<0||y<0||x>=64||y>=64)return;raw.set([...quant(c),255],(y*64+x)*4);}
 function box(x,y,w,h,c){for(let j=0;j<h;j++)for(let i=0;i<w;i++)dot(x+i,y+j,c);}
 function line(x,y,tx,ty,c){const dx=Math.abs(tx-x),sx=x<tx?1:-1,dy=-Math.abs(ty-y),sy=y<ty?1:-1;let error=dx+dy;for(;;){dot(x,y,c);if(x===tx&&y===ty)break;const twice=2*error;if(twice>=dy){error+=dy;x+=sx;}if(twice<=dx){error+=dx;y+=sy;}}}
 function text(s,x,y,c){for(const ch of s){const bits=glyphs[ch]||0;for(let row=0;row<5;row++)for(let col=0;col<3;col++)if(bits&(1<<(14-row*3-col)))dot(x+col,y+row,c);x+=4;}}
 function digit(n,x,y,c){const s=[63,6,91,79,102,109,125,7,127,111][n];for(const [mask,b]of [[1,[1,0,5,2]],[2,[5,1,2,5]],[4,[5,7,2,5]],[8,[1,11,5,2]],[16,[0,7,2,5]],[32,[0,1,2,5]],[64,[1,6,5,2]]])if(s&mask)box(x+b[0],y+b[1],b[2],b[3],c);}
 const part=w.islandPart,phase=seconds*w.speed%3600;
 const surface=(x,t)=>53+Math.sin(x*.085+t*.65)*1.25+Math.sin(x*.14-t*.45)*.75;
 if(part===0){const hour=w.format==='24'?date.getHours():date.getHours()%12||12;if(hour>=10||w.format==='24')digit(Math.floor(hour/10),5,15,white);digit(hour%10,14,15,white);digit(Math.floor(date.getMinutes()/10),27,15,white);digit(date.getMinutes()%10,36,15,white);dot(23,19,muted);dot(23,24,muted);if(w.format!=='24')text(date.getHours()<12?'AM':'PM',47,21,muted);}
 if(part===1){const s=['Jan','Feb','Mar','Apr','May','Jun','Jul','Aug','Sep','Oct','Nov','Dec'][date.getMonth()]+' '+date.getDate();text(s,Math.trunc((64-(s.length*4-1))/2),4,muted);}
 offset=8;
 if(part===2)box(0,51,64,13,rgb(5,35,45));
 if(part>=3&&part<=5){const band=part-3,timing=phase*(band===0?.8:band===1?1.1:1)+(2-band)*1.7;for(let x=0;x<64;x++){const y=round(surface(x,timing))-(2-band)*2;for(let d=0;d<2;d++){const strength=Math.trunc((64+band*28)/(d+1));dot(x,y+d,rgb(8,strength,strength+20));}}}
 if(part===6){box(8,51,24,1,rgb(173,153,102));box(6,52,28,1,rgb(173,153,102));box(8,53,25,1,rgb(130,118,78));}
 if(part===7||part===8){const x=part===7?16:26,ground=50,crown=ground-(part===7?14:10),leaf=rgb(66,128,91);line(x,ground,x-2,crown,rgb(147,119,72));line(x+1,ground,x-1,crown,rgb(115,93,57));const frond=(x,y,tx,ty,c)=>{line(x,y,tx,ty,c);line(x,y+1,tx,ty+1,c);};for(const [a,b,c,d]of [[-2,0,-7,-2],[-7,-2,-9,0],[-2,0,3,-3],[3,-3,6,-1],[-2,0,3,1],[3,1,5,4],[-2,0,-6,2],[-6,2,-8,4]])frond(x+a,crown+b,x+c,crown+d,leaf);frond(x-2,crown,x-3,crown-4,rgb(83,151,100));dot(x-2,crown+2,rgb(119,90,54));dot(x,crown+2,rgb(119,90,54));}
 if(part===9){const water=round(surface(47,phase));for(let y=-10;y<=1;y++)for(let x=-5;x<=5;x++){let c;if(y>=-9&&y<=-2&&x===0)c=rgb(174,161,127);if(y>=-8&&y<=-2&&x<0&&x>=-Math.trunc((y+10)/2))c=rgb(229,233,218);if(y>=-6&&y<=-2&&x>0&&x<=Math.trunc((y+8)/2))c=rgb(207,221,213);if(y===0)c=rgb(157,111,69);if(y===1&&x>=-3&&x<=3)c=rgb(113,84,59);if(c)dot(47+x,water-1+y,c);}}
 // Preview uses a repeatable 25-second pause; the board retains random 20–30s pauses.
 if(part>=10){const i=part-10,t=phase%58,elapsed=t<29?t-25:t-54;if(t<25||t>=29&&t<54){if(t<25){const x=18+i*10,y=34+i*5;dot(x,y,rgb(155,185,177));dot(x-1,y-1,rgb(155,185,177));dot(x+1,y-1,rgb(155,185,177));}}else{let p=Math.max(0,Math.min(1,(elapsed-i*.35)/3.5));if(t>=54)p=1-p;const x=round(18+i*10+p*53),y=round(34+i*5-Math.sin(p*1.5708)*(3+i*2)),wing=p>0&&p<1&&Math.sin(elapsed*17+i)>0?1:-1;dot(x,y,rgb(155,185,177));dot(x-1,y+wing,rgb(155,185,177));dot(x+1,y+wing,rgb(155,185,177));}}
 const defaults=[quant(colors(primary)),quant(colors(secondary))],replacements=[quant(colors(w.color)),quant(colors(w.secondary))];
 for(let y=0;y<w.h;y++)for(let x=0;x<w.w;x++){const sx=bx+Math.floor((2*x+1)*bw/(2*w.w)),sy=by+Math.floor((2*y+1)*bh/(2*w.h)),src=(sy*64+sx)*4,dst=(y*w.w+x)*4;out.set(raw.subarray(src,src+4),dst);for(let c=0;c<2;c++)if(raw[src+3]&&defaults[c].every((v,k)=>raw[src+k]===v)){out.set(replacements[c],dst);break;}}
 return out;
}
const api={specs,raster};if(typeof module!=='undefined'&&module.exports)module.exports=api;else root.IslandAssets=api;
})(typeof globalThis!=='undefined'?globalThis:this);
