import test from 'node:test';
import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';
import {createEngine} from '../public/studio/engine.mjs';
import {Studio,defaultSession,validate,tick} from '../public/studio/session.mjs';
import {allRoomLayout,wallRoomLayout,newStrip} from '../public/studio/layouts.mjs';

async function engine(){return createEngine(await readFile(new URL('../public/studio/fx.wasm',import.meta.url)),JSON.parse(await readFile(new URL('../public/studio/fx-manifest.json',import.meta.url))));}
const nodes=[{id:1,name:'Test Dual',outputs:[newStrip(1,1,0,1,9,20),newStrip(1,1,0,2,9,20)]}];

test('WASM is a self-contained renderer with the actual v8 catalogue',async()=>{
  const e=await engine();assert.equal(e.meta.version,8);assert.equal(e.meta.effects.length,47);assert.equal(e.meta.spatial.length,25);assert.equal(e.meta.maxNodes,128);
  assert.equal(e.meta.effects[45],'Particle Field');assert.equal(e.meta.beats[6].name,'1 Beat');
  const m=new WebAssembly.Module(await readFile(new URL('../public/studio/fx.wasm',import.meta.url)));
  assert.deepEqual(WebAssembly.Module.imports(m),[]);
});
test('wall and full room retain edge starts, all axes, 9-span and 100 LEDs',()=>{
  const wall=wallRoomLayout(),all=allRoomLayout();assert.equal(wall.length,32);assert.equal(all.length,49);
  assert.deepEqual(all.slice(0,32),wall);
  const interior=all.slice(32).flatMap(n=>n.outputs);
  assert.deepEqual(new Set(interior.map(s=>s.axis)),new Set([0,1,2]));
  for(const s of all.flatMap(n=>n.outputs)){assert.equal(s.span,9);assert.equal(s.count,100);assert.ok(s.axis===0?s.z===0:s.axis===1?s.x===1:s.y===1);}
});
test('all 47 effects and 25 spatial modes render legal RGB and world coordinates',async()=>{
  const e=await engine(),s=defaultSession();s.nodes=nodes;e.configure(s);
  for(let effect=0;effect<47;effect++)for(let spatial=0;spatial<25;spatial++){
    s.dmx[7]=effect;s.dmx[6]=spatial;e.reset();
    for(const now of [0,17,333,1217]){
      for(const f of e.render(s,now,0,0,false)){
        assert.equal(f.rgb.length,120);assert.match(f.rgb,/^[a-f0-9]+$/);assert.equal(f.xyz.length,60);
        assert.ok(f.xyz.every(v=>Number.isInteger(v)&&v>=2&&v<=18));
      }
    }
  }
});
test('All mode renders every LED and particle intersections share global color',async()=>{
  const e=await engine(),s=defaultSession();s.dmx[7]=45;s.dmx[8]=128;s.dmx[9]=255;s.dmx[5]=108;e.configure(s);
  for(const direction of [0,32,64,128,192,255]){
    s.dmx[10]=direction;e.reset();
    for(const now of [250,750,1500]){
      const frames=e.render(s,now,0,0,false),seen=new Map();assert.equal(frames.length,98);
      assert.equal(frames.reduce((n,f)=>n+f.rgb.length/6,0),9800);
      for(const f of frames)for(let i=0;i<f.xyz.length;i+=3){const key=f.xyz.slice(i,i+3).join(','),color=f.rgb.slice(i*2,i*2+6);if(seen.has(key))assert.equal(color,seen.get(key),`${direction}/${now}/${key}`);else seen.set(key,color);}
    }
  }
});
test('transport steps, pauses, restarts and blacks out real output',async()=>{
  const e=await engine(),s=defaultSession();s.nodes=nodes;let time=0;const lab=new Studio(e,s,()=>time);
  lab.action({action:'play',value:false});time=1000;lab.advance();assert.equal(lab.now,0);
  lab.action({action:'step'});assert.equal(lab.now,tick);assert.equal(lab.playing,false);
  lab.action({action:'blackout',value:true});assert.ok(lab.frames.every(f=>/^0+$/.test(f.rgb)));
  lab.action({action:'blackout',value:false});lab.action({action:'restart'});assert.equal(lab.now,0);
  assert.throws(()=>lab.action({action:'speed',value:9}),/Unsupported/);
});
test('live shape controls retain animation phase and session validation protects WASM memory',async()=>{
  const e=await engine(),s=defaultSession();s.nodes=nodes;const lab=new Studio(e,s,()=>0);
  lab.action({action:'step'});const start=lab.start;
  const changed=structuredClone(s);changed.dmx[10]=180;lab.update(changed);assert.equal(lab.start,start);
  const broken=structuredClone(s);broken.nodes[0].outputs[0].count=1001;assert.throws(()=>validate(broken,e.meta),/LED count/);
  const duplicate=structuredClone(s);duplicate.nodes.push(structuredClone(duplicate.nodes[0]));assert.throws(()=>validate(duplicate,e.meta),/unique/);
  const badEffect=structuredClone(s);badEffect.dmx[7]=47;assert.throws(()=>validate(badEffect,e.meta),/Unknown/);
  const overlap=structuredClone(s);const strip=overlap.nodes[0].outputs[0];strip.segments=[{...newStrip(1,1,0,1,9,8),start:0},{...newStrip(1,1,0,1,9,8),start:4}];assert.throws(()=>validate(overlap,e.meta),/overlap/);
  const segments=structuredClone(s);segments.nodes[0].outputs[0].segments=[{...newStrip(3,3,0,0,9,8),start:3}];
  const valid=validate(segments,e.meta);e.configure(valid);const frame=e.render(valid,0,0,0,false)[0];assert.equal(frame.xyz[3*3],6);assert.equal(frame.xyz[3*3+1],6);
  const empty=structuredClone(s);empty.nodes[0].outputs[0].count=0;e.configure(validate(empty,e.meta));assert.equal(e.render(empty,0,0,0,false)[0].rgb,'');
});
