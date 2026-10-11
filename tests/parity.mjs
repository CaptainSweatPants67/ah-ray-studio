import {readFile} from 'node:fs/promises';
import assert from 'node:assert/strict';
import {createEngine} from '../public/studio/engine.mjs';
const e=await createEngine(await readFile(new URL('../public/studio/fx.wasm',import.meta.url)),JSON.parse(await readFile(new URL('../public/studio/fx-manifest.json',import.meta.url))));
const cases=JSON.parse(await readFile(new URL('../.build/native-fixtures.json',import.meta.url)));
let count=0;
for(const c of cases){
  e.configure(c.session);e.reset();
  for(const sample of c.samples){
    const actual=e.render(c.session,sample.now,0,0,false);
    assert.deepEqual(actual,sample.frames,`FX ${c.session.dmx[7]}, spatial ${c.session.dmx[6]}, speed ${c.session.dmx[5]}, ${sample.now}ms`);count++;
  }
}
console.log(`Exact native/WASM RGB and XYZ parity: ${count} frames across ${cases.length} scenarios.`);
