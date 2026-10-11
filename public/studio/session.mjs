import {allRoomLayout} from './layouts.mjs';

export const tick = 1000/60;
const fields = ['x','y','z','span','axis','orientation','membership'];
const bounds = [[1,9],[1,9],[0,8],[1,9],[0,2],[0,5],[0,255]];
const integer = (v,lo,hi,label)=>{
  if(!Number.isInteger(v)||v<lo||v>hi) throw Error(`${label} must be an integer from ${lo} to ${hi}`);
  return v;
};
const vector = (v,length,lo,hi,label)=>{
  if(!Array.isArray(v)||v.length!==length) throw Error(`Expected ${length} ${label} values`);
  return v.map(n=>integer(n,lo,hi,label));
};
function placement(v) {
  if(!v||typeof v!=='object') throw Error('Invalid strip placement');
  const p=Object.fromEntries(fields.map((k,i)=>[k,integer(v[k],...bounds[i],k)]));
  if(![[4,5],[0,1],[2,3]][p.axis].includes(p.orientation)) throw Error('Direction does not match the strip axis');
  return p;
}
export function defaultSession() {
  return {format:'ahray-effects-lab-v1',dmx:[220,61,180,85,120,108,0,25,160,128,100,0],
    rgb:[255,96,0],stage:[5,5,1],autoBeat:true,nodes:allRoomLayout()};
}
export function validate(v,meta) {
  if(!v||v.format!=='ahray-effects-lab-v1') throw Error('Choose an AH-RAY session JSON file');
  const result={format:v.format,dmx:vector(v.dmx,12,0,255,'DMX'),rgb:vector(v.rgb,3,0,255,'RGB'),
    stage:vector(v.stage,3,1,9,'Focal point'),autoBeat:Boolean(v.autoBeat??true),nodes:[]};
  if(result.dmx[7]>=meta.effects.length) throw Error('Unknown light effect ID');
  if(result.dmx[6]>=meta.spatial.length) throw Error('Unknown spatial effect ID');
  if(!Array.isArray(v.nodes)||v.nodes.length<1||v.nodes.length>meta.maxNodes) throw Error(`Use 1 to ${meta.maxNodes} Dual nodes`);
  const ids=new Set();
  result.nodes=v.nodes.map(n=>{
    if(!n||typeof n!=='object') throw Error('Invalid node definition');
    const id=integer(n.id,0,254,'Node ID');
    if(ids.has(id)) throw Error('Node IDs must be unique'); ids.add(id);
    if(typeof n.name!=='string'||n.name.length<1||n.name.length>64) throw Error('Node name must be 1 to 64 characters');
    if(!Array.isArray(n.outputs)||n.outputs.length!==2) throw Error('Every Dual node needs outputs A and B');
    return {id,name:n.name,outputs:n.outputs.map(s=>{
      const out={...placement(s),count:integer(s.count,0,1000,'LED count'),segments:[]};
      if(!Array.isArray(s.segments??[])||(s.segments??[]).length>8) throw Error('Use up to 8 virtual segments per output');
      let end=0;
      out.segments=(s.segments??[]).map(segment=>{
        const p={...placement(segment),start:integer(segment.start,0,out.count-1,'Segment start'),count:integer(segment.count,1,out.count,'Segment length')};
        if(p.start<end||p.start+p.count>out.count) throw Error('Virtual segments must be ordered, must not overlap, and must fit the output');
        end=p.start+p.count;return p;
      });
      return out;
    })};
  });
  return result;
}

// Equivalent to the desktop Lab clock and reset behavior. Transport always runs at 60 Hz.
export class Studio {
  constructor(engine,session=defaultSession(),clock=()=>performance.now()) {
    this.engine=engine;this.session=validate(session,engine.meta);this.clock=clock;
    engine.configure(this.session);engine.reset();
    this.now=this.start=this.beat=this.nextBeat=this.accumulator=0;
    this.playing=true;this.speed=1;this.blackout=false;this.lastWall=clock();this.revision=0;
    this.render();
  }
  render() {
    if(this.session.autoBeat&&this.now>=this.nextBeat) {
      this.beat=this.now;
      if(this.session.dmx[5]===0){this.start=this.now;this.engine.reset();}
      this.nextBeat=this.now+(this.session.dmx[4]?60000/this.session.dmx[4]:1000);
    }
    this.frames=this.engine.render(this.session,this.now,this.start,this.beat,this.blackout);this.revision++;
  }
  advance() {
    const wall=this.clock(),delta=Math.max(0,Math.min(250,wall-this.lastWall));this.lastWall=wall;
    if(this.playing){this.accumulator+=delta*this.speed;while(this.accumulator>=tick){this.now+=tick;this.accumulator-=tick;this.render();}}
  }
  update(payload) {
    const candidate=validate(payload,this.engine.meta),previous=this.session;
    const changedPhase=[6,7].some(i=>candidate.dmx[i]!==previous.dmx[i]);
    const changedBuffer=changedPhase||['nodes','stage','rgb'].some(k=>JSON.stringify(candidate[k])!==JSON.stringify(previous[k]))||[0,1,2,3,11].some(i=>candidate.dmx[i]!==previous.dmx[i]);
    this.session=candidate;this.engine.configure(candidate);
    if(changedPhase)this.start=this.beat=this.now;
    if(changedBuffer)this.engine.reset();
    if(candidate.dmx[4]!==previous.dmx[4]||candidate.autoBeat!==previous.autoBeat)this.nextBeat=this.now;
    if(changedBuffer||(!this.playing&&JSON.stringify(candidate)!==JSON.stringify(previous)))this.render();
  }
  action({action,value}) {
    if(action==='play'){this.playing=Boolean(value);this.accumulator=0;}
    else if(action==='step'){this.playing=false;this.now+=tick;this.render();}
    else if(action==='restart'){this.engine.reset();this.now=this.start=this.beat=this.nextBeat=this.accumulator=0;this.render();}
    else if(action==='beat'){this.start=this.beat=this.now;this.engine.reset();this.render();}
    else if(action==='blackout'){this.blackout=Boolean(value);this.render();}
    else if(action==='speed'){if(![.1,.25,.5,1,2].includes(value))throw Error('Unsupported playback speed');this.speed=value;}
    else if(action==='rebuild'){this.engine.reset();this.render();}
    else throw Error('Unknown transport action');
    this.lastWall=this.clock();
  }
  frame(){return {now:Math.round(this.now),playing:this.playing,speed:this.speed,blackout:this.blackout,
    stale:false,fingerprint:this.engine.meta.fingerprint,frames:this.frames,labels:this.engine.labels(this.session),revision:this.revision};}
}
