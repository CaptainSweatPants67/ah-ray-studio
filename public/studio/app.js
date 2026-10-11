import {newStrip, roomLayout, wallRoomLayout, allRoomLayout, isWallMode, isAllMode} from './layouts.mjs';
import {request} from './bridge.mjs';
'use strict';
const $ = id => document.getElementById(id);
const el = (tag, text, cls) => { const e = document.createElement(tag); if(text !== undefined) e.textContent = text; if(cls) e.className = cls; return e; };
let meta, session, frame, selected = 0, token, view = '3d', yaw = -.65, pitch = .48, zoom = 1;
let saveTimer, saveChain = Promise.resolve(), hitPoints = [], monitorRows = [], drag, lastPoll = 0;
const params = {25:['Persistence','Band Size','Aurora Count'],27:['Center Well','Density','Particle Size'],28:['Center Well','Density','Particle Size'],29:['Wave Decay','Duty Cycle','Wave Height'],30:['Star Size','Density','Twinkle'],31:['Fade Speed','Collapse Speed','Direction'],32:['Persistence','Edge Width','Expand Speed'],33:['Color Blend','Water Height','Tilt'],34:['Hold Time','Thickness','Bolt Speed'],35:['Trail','Density','Rain Speed'],36:['Trail','Ring Width','Ring Tilt'],37:['Trail','Ring Width','Ring Tilt'],38:['Orb Size','Particles','Particle Speed'],39:['Edge Gradient','Shell Width','Blast Speed'],40:['Edge Gradient','Shell Width','Implode Speed'],41:['Thickness','Turns','Direction / Speed'],42:['Thickness','Turns','Direction / Speed'],43:['Random Fade','Beam Count','Spin'],44:['Line Fade','Next Beam','Start Beam'],45:['Tail Fade','Density','Direction'],46:['Stack Fade','Unused','Grow Curve']};
const axisNames = ['Z · vertical','X · left / right','Y · back / front'];
const directionNames = ['Right →','Left ←','Up / reverse Y','Down / forward Y','Ground ↑','Ceiling ↓'];
const validDirections = [[4,5],[0,1],[2,3]];

function error(message) { $('error').textContent = message; $('error').hidden = !message; }
const api = request;
function save() {
  clearTimeout(saveTimer);
  saveTimer = setTimeout(() => {
    const snapshot = structuredClone(session);
    saveChain = saveChain.catch(()=>{}).then(async()=>{
      try { await api('session', snapshot); error(''); }
      catch(e) { error(e.message); }
    });
  }, 80);
}
async function flush() { clearTimeout(saveTimer); await saveChain.catch(()=>{}); await api('session', session); }
async function action(name, value) {
  try { await flush(); frame = await api('action', {action:name,value}); updateTransport(); }
  catch(e) { error(e.message); }
}
function options(select, items, value) {
  select.replaceChildren(); items.forEach((item,i)=>{
    const option = el('option', typeof item === 'string' ? `${String(i).padStart(2,'0')} · ${item}` : item.name);
    option.value = typeof item === 'string' ? i : item.value; select.append(option);
  }); select.value = value;
}
function selectDmx(id, index, items) {
  const input = $(id); options(input,items,session.dmx[index]);
  // Arbitrary imported DMX values retain their exact bytes, even within a named range.
  if(input.value === '') { const match=items.filter(item=>typeof item==='object'&&item.value<=session.dmx[index]).at(-1); const option = el('option',`${match?.name||'Value'} · DMX ${session.dmx[index]}`); option.value=session.dmx[index]; input.append(option); input.value=session.dmx[index]; }
  input.onchange = () => { session.dmx[index] = +input.value; updateEffectLabels(); save(); };
}
function slider(id,index,title,max=255) {
  const label = el('label'); const heading=el('div',undefined,'slider-label');
  const name=el('span',title); name.id=id+'Label'; const output=el('output',String(session.dmx[index])); output.id=id+'Value';
  heading.append(name,output); const pair=el('div',undefined,'range-pair');
  const range=el('input'); range.type='range'; range.min=0; range.max=max; range.value=session.dmx[index]; range.id=id+'Range'; range.setAttribute('aria-label',title);
  const num=el('input'); num.type='number'; num.min=0; num.max=max; num.value=range.value; num.setAttribute('aria-label',title+' DMX value');
  const change=value=>{ if(!Number.isFinite(+value)) return; const v=Math.max(0,Math.min(max,Math.round(+value))); session.dmx[index]=v; range.value=v; num.value=v; output.textContent=v; save(); };
  range.oninput=()=>change(range.value); num.onchange=()=>change(num.value);
  pair.append(range,num); label.append(heading,pair); $(id).replaceChildren(label);
}
function numberField(parent, title, value, min, max, onChange, cls='') {
  const label=el('label',title,cls); const input=el('input'); input.type='number'; input.min=min; input.max=max; input.value=value;
  input.setAttribute('aria-label',title);
  input.onchange=()=>{ const v=+input.value; if(!Number.isInteger(v)||v<min||v>max){input.value=value;return;} onChange(v); save(); };
  label.append(input); parent.append(label); return input;
}
function placementFields(parent,s,onRefresh) {
  numberField(parent,'X',s.x,1,9,v=>s.x=v); numberField(parent,'Y',s.y,1,9,v=>s.y=v);
  numberField(parent,'HEIGHT · 1–9',s.z+1,1,9,v=>s.z=v-1); numberField(parent,'SPAN · CELLS',s.span,1,9,v=>s.span=v);
  const axisLabel=el('label','AXIS'); const axis=el('select'); options(axis,axisNames,s.axis);
  axis.onchange=()=>{s.axis=+axis.value;s.orientation=validDirections[s.axis][0];onRefresh();save();}; axisLabel.append(axis); parent.append(axisLabel);
  const directionLabel=el('label','DIRECTION'); const direction=el('select');
  options(direction,validDirections[s.axis].map(v=>({value:v,name:directionNames[v]})),s.orientation);
  direction.onchange=()=>{s.orientation=+direction.value;save();}; directionLabel.append(direction); parent.append(directionLabel);
  numberField(parent,'MEMBERSHIP',s.membership,0,255,v=>s.membership=v);
}
function bindControls() {
  $('search').value=''; selectDmx('effect',7,meta.effects); selectDmx('spatial',6,meta.spatial);
  selectDmx('mainColor',1,meta.colors); selectDmx('accentColor',3,[{value:0,name:'Follow main color / RGB'},...meta.colors.slice(1)]);
  selectDmx('beatDivision',5,meta.beats); selectDmx('zone',11,meta.zones);
  slider('mainDimmer',0,'MAIN DIMMER'); slider('accentDimmer',2,'ACCENT DIMMER');
  slider('fade',8,'FX FADE'); slider('expand',9,'FX EXPAND'); slider('warp',10,'FX WARP'); slider('bpm',4,'BPM');
  $('autoBeat').checked=session.autoBeat; $('autoBeat').onchange=()=>{session.autoBeat=$('autoBeat').checked;save();};
  $('rgb').value='#'+session.rgb.map(v=>v.toString(16).padStart(2,'0')).join('');
  $('rgb').oninput=()=>{session.rgb=$('rgb').value.slice(1).match(/../g).map(v=>parseInt(v,16));save();};
  $('stage').replaceChildren(); ['X','Y','HEIGHT'].forEach((name,i)=>numberField($('stage'),name,session.stage[i],1,9,v=>session.stage[i]=v));
  $('search').oninput=()=>{ const query=$('search').value.toLowerCase(); options($('effect'),meta.effects.map((name,i)=>({name:`${String(i).padStart(2,'0')} · ${name}`,value:i})).filter(v=>v.name.toLowerCase().includes(query)),session.dmx[7]); };
  updateEffectLabels(); buildNodes(); buildInspector();
}
function updateEffectLabels() {
  const names=params[session.dmx[7]] || ['FX Fade','FX Expand','FX Warp'];
  ['fade','expand','warp'].forEach((id,i)=>{ $(id+'Label').textContent=names[i].toUpperCase(); $(id+'Range').setAttribute('aria-label',names[i]); $(id).querySelector('input[type=number]').setAttribute('aria-label',names[i]+' DMX value'); });
  $('trigger').disabled=session.dmx[5]!==0;
  $('trigger').title=session.dmx[5]===0?'Trigger a beat pulse':'Choose BEAT RENDERED to trigger a pulse manually';
  $('customRgb').hidden=session.dmx[1]!==0;
  $('effectTitle').textContent=meta.effects[session.dmx[7]];
  $('subtitle').textContent=`${meta.spatial[session.dmx[6]]} · ${session.nodes.length} Dual nodes · each dot is one LED`;
}
function buildNodes() {
  $('nodes').replaceChildren();
  session.nodes.forEach((n,ni)=>{
    const card=el('div',undefined,'node');const title=el('div',undefined,'node-heading'); title.append(el('span',n.name),el('small',`ID ${n.id}`));card.append(title);
    const outputs=el('div',undefined,'node-outputs'); n.outputs.forEach((s,oi)=>{
      const button=el('button',`${oi?'B':'A'} · ${s.count} LEDs`,selected===ni*2+oi?'selected':''); button.onclick=()=>selectOutput(ni*2+oi); outputs.append(button);
    });card.append(outputs);$('nodes').append(card);
  });
  $('ledTotal').textContent=session.nodes.reduce((sum,n)=>sum+n.outputs.reduce((sum,s)=>sum+s.count,0),0)+' LEDs';
  $('addNode').disabled=session.nodes.length>=meta.maxNodes; updateEffectLabels();
  const all=isAllMode(session.nodes),wall=isWallMode(session.nodes);
  for(const [id,active] of [['allMode',all],['wallMode',wall]]){
    $(id).classList.toggle('selected',active);$(id).setAttribute('aria-pressed',String(active));
  }
  $('layoutSummary').textContent=all||wall?`${session.nodes.length} Dual nodes · ${session.nodes.length*2} strips · span 9 · 100 LEDs / strip`:'Custom layout';
}
function selectOutput(slot) { selected=slot;buildNodes();buildInspector(); }
function buildInspector() {
  selected=Math.min(selected,session.nodes.length*2-1);
  const node=session.nodes[Math.floor(selected/2)], s=node.outputs[selected%2];
  $('selectedTitle').textContent=`${node.name} / ${selected%2?'B':'A'}`;
  $('nodeFields').replaceChildren(); const grid=el('div',undefined,'field-grid');
  const nameLabel=el('label','NODE NAME','full'),name=el('input');name.type='text';name.maxLength=64;name.value=node.name;
  name.onchange=()=>{if(!name.value.trim())return;node.name=name.value;buildNodes();buildInspector();save();};nameLabel.append(name);grid.append(nameLabel);
  numberField(grid,'NODE ID',node.id,0,254,v=>{node.id=v;buildNodes();});
  numberField(grid,'LED COUNT',s.count,0,1000,v=>{s.count=v;s.segments=s.segments.filter(seg=>seg.start+seg.count<=v);buildNodes();buildInspector();});
  placementFields(grid,s,buildInspector);$('nodeFields').append(grid);
  $('removeNode').disabled=session.nodes.length===1;
  $('segments').replaceChildren();$('segmentCount').textContent=`${s.segments.length} / 8`;
  s.segments.forEach((seg,i)=>{
    const card=el('div',undefined,'segment'),head=el('div',undefined,'segment-head');head.append(el('span',`Segment ${i+1}`));
    const remove=el('button','×');remove.setAttribute('aria-label',`Remove segment ${i+1}`);remove.onclick=()=>{s.segments.splice(i,1);buildInspector();save();};head.append(remove);card.append(head);
    const fields=el('div',undefined,'field-grid'); numberField(fields,'START · 0 BASED',seg.start,0,s.count-1,v=>seg.start=v);
    numberField(fields,'LED LENGTH',seg.count,1,s.count,v=>seg.count=v);placementFields(fields,seg,buildInspector);card.append(fields);$('segments').append(card);
  });
  const end=s.segments.reduce((end,seg)=>Math.max(end,seg.start+seg.count),0);
  $('addSegment').disabled=s.segments.length>=8||end>=s.count;
  $('addSegment').onclick=()=>{s.segments.push({...structuredClone(s),segments:undefined,start:end,count:Math.min(24,s.count-end)});buildInspector();save();};
}
function updateTransport() {
  if(!frame)return;
  $('play').textContent=frame.playing?'Pause':'Play';$('speed').value=frame.speed;
  $('time').textContent=(frame.now/1000).toFixed(2)+' s';$('blackout').classList.toggle('selected',frame.blackout);
  $('package').textContent=`FX v${meta.version} · ${meta.effects.length} effects`;
  $('rebuild').classList.toggle('stale',frame.stale);$('rebuild').textContent='Reload engine';
  $('sceneStatus').textContent=frame.stale?'SOURCE CHANGED · PREVIEW USES LAST BUILD':'BROWSER ENGINE · 60 HZ';
  $('sceneStatus').classList.toggle('stale',frame.stale);
  ['mainDimmer','accentDimmer','fade','expand','warp','bpm'].forEach((id,i)=>{
    const index=[0,2,8,9,10,4][i]; if($(id+'Value'))$(id+'Value').textContent=frame.labels?.[index] || session.dmx[index];
  });
}

function canvasSize(canvas,height) {
  const w=canvas.parentElement.clientWidth, h=height||canvas.parentElement.clientHeight, dpr=window.devicePixelRatio||1;
  if(canvas.width!==Math.round(w*dpr)||canvas.height!==Math.round(h*dpr)){canvas.width=Math.round(w*dpr);canvas.height=Math.round(h*dpr);canvas.style.height=h+'px';}
  const ctx=canvas.getContext('2d');ctx.setTransform(dpr,0,0,dpr,0,0);return{ctx,w,h};
}
function projector(w,h) {
  const scale=Math.min(w/30,h/29)*zoom;
  return (x,y,z)=>{
    x-=10;y-=10;z-=10;
    if(view==='top')return{x:w/2+x*scale,y:h/2+y*scale,depth:z};
    if(view==='front')return{x:w/2+x*scale,y:h/2-z*scale,depth:y};
    const a=x*Math.cos(yaw)-y*Math.sin(yaw), b=x*Math.sin(yaw)+y*Math.cos(yaw);
    const v=b*Math.sin(pitch)-z*Math.cos(pitch),d=b*Math.cos(pitch)+z*Math.sin(pitch);
    return{x:w/2+a*scale,y:h/2+v*scale,depth:d};
  };
}
function line(ctx,project,a,b,color,width=1){const p=project(...a),q=project(...b);ctx.beginPath();ctx.moveTo(p.x,p.y);ctx.lineTo(q.x,q.y);ctx.strokeStyle=color;ctx.lineWidth=width;ctx.stroke();}
function paintRoomSurfaces(ctx,project){
  // Half-cell boundaries surround the firmware's nine cell centers (2..18).
  // Draw the shell before LEDs so it remains a navigation aid from either side.
  const surfaces=[
    {floor:true,corners:[[1,1,1],[19,1,1],[19,19,1],[1,19,1]],fill:'#142b32',grid:'#2e5260',label:'FLOOR · HEIGHT 1',anchor:[10,20,1]},
    {floor:false,corners:[[1,1,1],[19,1,1],[19,1,19],[1,1,19]],fill:'#1b2239',grid:'#414d73',label:'BACK WALL · Y 1',anchor:[10,1,20]}
  ];
  const visible=surfaces.map(s=>({...s,points:s.corners.map(p=>project(...p))})).filter(s=>{
    const area=s.points.reduce((sum,p,i)=>{const q=s.points[(i+1)%4];return sum+p.x*q.y-q.x*p.y;},0);
    return Math.abs(area)>1;
  }).sort((a,b)=>a.points.reduce((sum,p)=>sum+p.depth,0)-b.points.reduce((sum,p)=>sum+p.depth,0));
  for(const s of visible){
    ctx.beginPath();s.points.forEach((p,i)=>i?ctx.lineTo(p.x,p.y):ctx.moveTo(p.x,p.y));ctx.closePath();
    ctx.fillStyle=s.fill;ctx.fill();ctx.strokeStyle=s.grid;ctx.lineWidth=1.5;ctx.stroke();
    for(let v=3;v<19;v+=2){
      if(s.floor){line(ctx,project,[v,1,1],[v,19,1],s.grid);line(ctx,project,[1,v,1],[19,v,1],s.grid);}
      else{line(ctx,project,[v,1,1],[v,1,19],s.grid);line(ctx,project,[1,1,v],[19,1,v],s.grid);}
    }
  }
  return visible;
}
function paintSurfaceLabels(ctx,project,surfaces){
  ctx.font='600 10px Segoe UI';ctx.textAlign='center';ctx.strokeStyle='#0b1420';ctx.lineWidth=3;
  for(const s of surfaces){
    const p=project(...s.anchor);ctx.fillStyle=s.floor?'#83b8c0':'#99a9d4';
    ctx.strokeText(s.label,p.x,p.y);ctx.fillText(s.label,p.x,p.y);
  }
  ctx.textAlign='start';ctx.lineWidth=1;
}
function decode(hex){const array=new Uint8Array(hex.length/2);for(let i=0;i<array.length;i++)array[i]=parseInt(hex.slice(i*2,i*2+2),16);return array;}
function colors(f){return f.colors||(f.colors=decode(f.rgb));}
function paintRoom() {
  const{ctx,w,h}=canvasSize($('room'));ctx.clearRect(0,0,w,h);const project=projector(w,h);
  const bg=ctx.createRadialGradient(w*.5,h*.6,10,w*.5,h*.6,w*.7);bg.addColorStop(0,'#101e31');bg.addColorStop(1,'#090d16');ctx.fillStyle=bg;ctx.fillRect(0,0,w,h);
  const surfaces=paintRoomSurfaces(ctx,project);
  const corners=[[1,1],[19,1],[19,19],[1,19]];corners.forEach((c,i)=>{const n=corners[(i+1)%4];line(ctx,project,[...c,1],[...c,19],'#35435d');line(ctx,project,[...c,19],[...n,19],'#2a3850');});
  for(const[pos,label,axis]of [[[20,1,1],'X →','x'],[[1,20,1],'Y →','y'],[[1,1,20],'Z ↑','z']]){
    if((view==='top'&&axis==='z')||(view==='front'&&axis==='y'))continue;
    const p=project(...pos);ctx.fillStyle='#8aa1bc';ctx.font='10px Segoe UI';ctx.fillText(label,p.x,p.y);
  }
  const focal=project(...session.stage.map(v=>v*2));ctx.strokeStyle='#d6b9ff';ctx.lineWidth=1.5;ctx.beginPath();ctx.arc(focal.x,focal.y,6,0,Math.PI*2);ctx.moveTo(focal.x-10,focal.y);ctx.lineTo(focal.x+10,focal.y);ctx.moveTo(focal.x,focal.y-10);ctx.lineTo(focal.x,focal.y+10);ctx.stroke();ctx.fillStyle='#b39ddd';ctx.fillText('FOCAL',focal.x+13,focal.y+3);
  if(!frame){paintSurfaceLabels(ctx,project,surfaces);return;}const points=[];
  frame.frames.forEach(f=>{
    const rgb=colors(f),ni=Math.floor(f.slot/2),node=session.nodes[ni];if(!node)return;
    const count=rgb.length/3;
    if(!count)return;
    for(let i=0;i<count;i++){
      const p=project(f.xyz[i*3],f.xyz[i*3+1],f.xyz[i*3+2]);const r=rgb[i*3],g=rgb[i*3+1],b=rgb[i*3+2];
      points.push({...p,slot:f.slot,index:i,r,g,b,xyz:f.xyz.slice(i*3,i*3+3),name:node.name});
    }
    const first=project(...f.xyz.slice(0,3)),last=project(...f.xyz.slice(-3));
    ctx.strokeStyle=f.slot===selected?'#6fa49866':'#53677b44';ctx.lineWidth=f.slot===selected?3:1;ctx.beginPath();ctx.moveTo(first.x,first.y);ctx.lineTo(last.x,last.y);ctx.stroke();
    if(frame.frames.length<=16||f.slot===selected){ctx.fillStyle=f.slot===selected?'#91d8ca':'#8297b0';ctx.font='10px Segoe UI';ctx.fillText(`${node.name} ${f.slot%2?'B':'A'}`,first.x+10,first.y+16);}
  });
  points.sort((a,b)=>a.depth-b.depth);
  const glow=$('glow').checked;
  for(const p of points){const active=Math.max(p.r,p.g,p.b)>3;const color=`rgb(${p.r},${p.g},${p.b})`;ctx.fillStyle=active?color:'#243346';ctx.shadowBlur=glow&&active?9:0;ctx.shadowColor=color;ctx.beginPath();ctx.arc(p.x,p.y,active?2.5:1.7,0,Math.PI*2);ctx.fill();}
  ctx.shadowBlur=0;hitPoints=points;paintSurfaceLabels(ctx,project,surfaces);
}
function paintStrips() {
  if(!frame)return;
  const rowH=39,header=12;const{ctx,w,h}=canvasSize($('strips'),Math.max(190,frame.frames.length*rowH+header));
  ctx.clearRect(0,0,w,h);monitorRows=[];
  frame.frames.forEach((f,ri)=>{
    const node=session.nodes[Math.floor(f.slot/2)];if(!node)return;const y=header+ri*rowH;
    const rgb=colors(f),count=rgb.length/3,left=128,right=w-20,width=right-left;
    ctx.fillStyle=f.slot===selected?'#172e32':'#0c1320';ctx.fillRect(0,y-3,w,rowH-2);
    ctx.fillStyle=f.slot===selected?'#8edecf':'#9aaac0';ctx.font='11px Segoe UI';ctx.fillText(`${node.name} / ${f.slot%2?'B':'A'}`,8,y+12);
    ctx.fillStyle='#526781';ctx.font='9px Segoe UI';ctx.fillText(`${count} LEDs`,8,y+24);
    ctx.fillStyle='#18273d';ctx.fillRect(left,y+3,width,16);
    for(let i=0;i<count;i++){ctx.fillStyle=`rgb(${rgb[i*3]},${rgb[i*3+1]},${rgb[i*3+2]})`;ctx.fillRect(left+i*width/count,y+3,Math.max(.5,width/count-(count<200?1:0)),16);}
    const s=node.outputs[f.slot%2];ctx.strokeStyle='#b59bff';s.segments.forEach(seg=>{const x=left+seg.start/count*width;ctx.beginPath();ctx.moveTo(x,y);ctx.lineTo(x,y+22);ctx.stroke();});
    monitorRows.push({slot:f.slot,y,left,width,count,rgb,xyz:f.xyz,name:node.name});
  });
}
function showPixel(p){if(!p){$('pixelInfo').hidden=true;return;}const xyz=p.xyz.map(v=>v/2);$('pixelInfo').hidden=false;$('pixelInfo').textContent=`${p.name} / ${p.slot%2?'B':'A'} · LED ${p.index} · RGB ${p.r}, ${p.g}, ${p.b} · XYZ ${xyz.join(', ')}`;}
function canvasPointer(event,canvas){const r=canvas.getBoundingClientRect();return{x:event.clientX-r.left,y:event.clientY-r.top};}
function bindEvents(){
  $('play').onclick=()=>action('play',!frame?.playing);$('step').onclick=()=>action('step');$('restart').onclick=()=>action('restart');$('trigger').onclick=()=>action('beat');
  $('blackout').onclick=()=>action('blackout',!frame?.blackout);$('speed').onchange=()=>action('speed',+$('speed').value);
  document.querySelectorAll('[data-view]').forEach(button=>button.onclick=()=>{view=button.dataset.view;document.querySelectorAll('[data-view]').forEach(b=>b.classList.toggle('selected',b===button));});
  $('resetCamera').onclick=()=>{yaw=-.65;pitch=.48;zoom=1;};
  $('room').onpointerdown=e=>{drag={...canvasPointer(e,$('room')),yaw,pitch,moved:false};$('room').setPointerCapture(e.pointerId);};
  $('room').onpointermove=e=>{
    const p=canvasPointer(e,$('room'));
    if(drag){const dx=p.x-drag.x,dy=p.y-drag.y;if(Math.abs(dx)+Math.abs(dy)>4)drag.moved=true;if(view==='3d'){yaw=drag.yaw-dx*.008;pitch=Math.max(-1.2,Math.min(1.3,drag.pitch+dy*.007));}}
    else{let nearest,best=10;for(const q of hitPoints){const d=Math.hypot(p.x-q.x,p.y-q.y);if(d<best){best=d;nearest=q;}}showPixel(nearest);}
  };
  $('room').onpointerup=e=>{if(drag&&!drag.moved){const p=canvasPointer(e,$('room'));let nearest,best=22;for(const q of hitPoints){const d=Math.hypot(p.x-q.x,p.y-q.y);if(d<best){best=d;nearest=q;}}if(nearest)selectOutput(nearest.slot);}drag=null;};
  $('room').onpointercancel=()=>drag=null;
  $('room').onwheel=e=>{e.preventDefault();zoom=Math.max(.45,Math.min(3,zoom*Math.exp(-e.deltaY*.001)));};
  $('strips').onpointermove=e=>{const p=canvasPointer(e,$('strips'));const row=monitorRows.find(r=>p.y>=r.y-3&&p.y<r.y+36);if(!row||p.x<row.left||p.x>row.left+row.width)return showPixel();const index=Math.min(row.count-1,Math.floor((p.x-row.left)/row.width*row.count));showPixel({name:row.name,slot:row.slot,index,r:row.rgb[index*3],g:row.rgb[index*3+1],b:row.rgb[index*3+2],xyz:row.xyz.slice(index*3,index*3+3)});};
  $('strips').onclick=e=>{const p=canvasPointer(e,$('strips'));const row=monitorRows.find(r=>p.y>=r.y-3&&p.y<r.y+36);if(row)selectOutput(row.slot);};
  $('addNode').onclick=()=>{const ids=new Set(session.nodes.map(n=>n.id));let id=1;while(ids.has(id))id++;session.nodes.push({id,name:`Dual ${String(id).padStart(2,'0')}`,outputs:[newStrip(5,5),newStrip(5,5,8,1,5)]});selected=session.nodes.length*2-2;buildNodes();buildInspector();save();};
  $('removeNode').onclick=()=>{if(session.nodes.length<=1)return;session.nodes.splice(Math.floor(selected/2),1);selected=0;buildNodes();buildInspector();save();};
  $('demoRoom').onclick=()=>{session.nodes=roomLayout();selected=0;buildNodes();buildInspector();save();};
  $('allMode').onclick=()=>{session.nodes=allRoomLayout();selected=0;buildNodes();buildInspector();save();};
  $('wallMode').onclick=()=>{session.nodes=wallRoomLayout();selected=0;buildNodes();buildInspector();save();};
  $('demoStrip').onclick=()=>{session.nodes=[{id:1,name:'Dual 01',outputs:[newStrip(1,3,0,1,9,150),newStrip(1,7,0,1,9,150)]}];selected=0;buildNodes();buildInspector();save();};
  $('export').onclick=()=>download(new Blob([JSON.stringify(session,null,2)],{type:'application/json'}),'ahray-effect-session.json');
  $('import').onclick=()=>$('file').click();$('file').onchange=async()=>{try{const file=$('file').files[0];if(!file)return;if(file.size>250000)throw Error('Session file is too large');const value=JSON.parse(await file.text());await saveChain.catch(()=>{});clearTimeout(saveTimer);const result=await api('session',value);session=result.session;frame=result;selected=0;bindControls();error('');}catch(e){error(e.message);}finally{$('file').value='';}};
  $('snapshot').onclick=()=>$('room').toBlob(blob=>download(blob,`ahray-${session.dmx[7]}-${frame?.now||0}ms.png`));
  $('rebuild').onclick=async()=>{const b=$('rebuild');b.disabled=true;b.textContent='Reloading…';try{await flush();await api('action',{action:'rebuild'});meta=await api('init');token=meta.token;session=meta.session;bindControls();$('source').textContent=`${meta.source}\nFX v${meta.version} · ${meta.fingerprint.slice(0,16)}`;error('');}catch(e){error(e.message);}finally{b.disabled=false;}};
  window.addEventListener('pagehide',()=>{try{localStorage.setItem('ah-ray-studio.session.v1',JSON.stringify(session));}catch{}});
  window.addEventListener('keydown',e=>{if(['INPUT','SELECT','TEXTAREA'].includes(document.activeElement.tagName))return;if(e.code==='Space'){e.preventDefault();action('play',!frame?.playing);}if(e.code==='ArrowRight'){e.preventDefault();action('step');}if(e.code==='KeyB')action('beat');});
}
function download(blob,name){const url=URL.createObjectURL(blob),a=el('a');a.href=url;a.download=name;a.click();setTimeout(()=>URL.revokeObjectURL(url),1000);}
async function poll(){try{frame=await api('frame');lastPoll=performance.now();updateTransport();}catch(e){error('The effects engine stopped. Reload the page to restart. '+e.message);}setTimeout(poll,33);}
let paintStamp = '';
function paint(){const stamp=[frame?.revision,selected,view,yaw,pitch,zoom,window.innerWidth,window.innerHeight,$('glow').checked].join(':');if(session&&stamp!==paintStamp){paintStamp=stamp;paintRoom();paintStrips();if(lastPoll&&performance.now()-lastPoll>3000)$('sceneStatus').textContent='WAITING FOR RENDERER…';}requestAnimationFrame(paint);}
async function start(){try{meta=await api('init');token=meta.token;session=meta.session;bindControls();bindEvents();$('source').textContent=`${meta.source}\nFX v${meta.version} · ${meta.fingerprint.slice(0,16)}`;poll();paint();}catch(e){error(e.message);}}
if(typeof document!=='undefined')start();
