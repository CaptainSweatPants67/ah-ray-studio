// The RGB and geometry pipeline lives in the compiled firmware, never in JavaScript.
const fields = ['x', 'y', 'z', 'span', 'axis', 'orientation', 'membership'];
const hex = Array.from({length:256}, (_, i)=>i.toString(16).padStart(2,'0'));
const decoder = new TextDecoder();

export async function createEngine(bytes, manifest) {
  const {instance} = await WebAssembly.instantiate(bytes, {});
  const dll = instance.exports;
  dll._initialize();
  const scratch = dll.malloc(24 + 144 + 16 + 3000 + 6000);
  if(!scratch) throw Error('Could not allocate effects memory');
  const config = scratch, segments = scratch + 24, dmx = segments + 144;
  const rgb = dmx + 12, output = dmx + 16, xyz = output + 3000;
  const string = pointer => {
    const memory = new Uint8Array(dll.memory.buffer);
    let end = pointer; while(memory[end]) end++;
    return decoder.decode(memory.subarray(pointer, end));
  };
  const valueName = (channel, value)=>string(dll.lab_value_name(channel, value));
  const items = (channel, values)=>values.map(value=>({value,name:valueName(channel,value)}));
  const meta = {
    version:dll.lab_version(), maxNodes:dll.lab_node_capacity(),
    fingerprint:manifest.sourceFingerprint,
    source:'Dual S3 effects and spatial pipeline · FastLED · WebAssembly',
    effects:Array.from({length:dll.lab_effect_count()},(_,i)=>string(dll.lab_effect_name(i))),
    spatial:Array.from({length:dll.lab_spatial_count()},(_,i)=>string(dll.lab_spatial_name(i))),
    colors:items(2,[0,...Array.from({length:64},(_,i)=>1+i*4).filter(v=>v<254)]),
    beats:items(6,[0,...Array.from({length:13},(_,i)=>Math.floor(i*256/13)+1)]),
    zones:items(12,Array.from({length:12},(_,i)=>i?Math.floor(i*256/12)+1:0))
  };
  let configs = [];
  return {
    meta,
    reset:()=>dll.lab_reset(),
    labels:session=>session.dmx.map((v,i)=>valueName(i+1,v)),
    configure(session) {
      configs = session.nodes.flatMap((node,ni)=>node.outputs.map((s,oi)=>({
        slot:ni*2+oi,count:s.count,
        config:[node.id,s.count,...fields.map(k=>s[k]),...session.stage],
        segments:s.segments.flatMap(seg=>[seg.start,seg.count,...fields.map(k=>seg[k])]),
        segmentCount:s.segments.length
      })));
    },
    render(session, now, start, beat, blackout) {
      new Uint8Array(dll.memory.buffer,dmx,12).set(session.dmx);
      new Uint8Array(dll.memory.buffer,rgb,3).set(session.rgb);
      return configs.map(s=>{
        new Uint16Array(dll.memory.buffer,config,12).set(s.config);
        new Uint16Array(dll.memory.buffer,segments,72).set(s.segments);
        dll.lab_configure(s.slot,config,segments,s.segmentCount);
        dll.lab_render(dmx,rgb,Math.floor(now),Math.floor(start),Math.floor(beat),Number(blackout),output,xyz);
        const colors = new Uint8Array(dll.memory.buffer,output,s.count*3);
        let encoded = ''; for(const v of colors) encoded += hex[v];
        return {slot:s.slot,rgb:encoded,xyz:Array.from(new Int16Array(dll.memory.buffer,xyz,s.count*3))};
      });
    }
  };
}
