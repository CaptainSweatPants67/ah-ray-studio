import test from 'node:test';
import assert from 'node:assert/strict';
import {readFile,readdir,stat} from 'node:fs/promises';
import {createHash} from 'node:crypto';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
const root=fileURLToPath(new URL('../',import.meta.url));
const manifest=JSON.parse(await readFile(path.join(root,'public/studio/fx-manifest.json')));
test('published WASM matches the manifest',async()=>{
  const binary=await readFile(path.join(root,'public/studio/fx.wasm'));
  assert.equal(createHash('sha256').update(binary).digest('hex'),manifest.wasmSha256);
});
test('source snapshot matches the shipped browser package',async()=>{
  const files=[];
  async function walk(dir){for(const name of await readdir(dir)){const p=path.join(dir,name);if((await stat(p)).isDirectory())await walk(p);else files.push(p);}}
  await walk(path.join(root,'fx'));
  const relative=p=>path.relative(root,p).replaceAll('\\','/');
  files.sort((a,b)=>relative(a)<relative(b)?-1:relative(a)>relative(b)?1:0);
  const hash=createHash('sha256');
  for(const p of files){hash.update(relative(p));hash.update(Buffer.from((await readFile(p)).toString('latin1').replaceAll('\r\n','\n'),'latin1'));}
  assert.equal(hash.digest('hex'),manifest.sourceFingerprint);
});
test('Studio assets use relative paths and no local backend',async()=>{
  const html=await readFile(path.join(root,'public/studio/index.html'),'utf8');
  for(const match of html.matchAll(/(?:src|href)="([^"#]+)"/g)){
    assert.ok(!match[1].startsWith('/'),`Absolute Studio asset: ${match[1]}`);
    await stat(path.resolve(root,'public/studio',match[1]));
  }
  for(const name of ['app.js','bridge.mjs','engine.mjs','fx-worker.mjs','session.mjs','layouts.mjs']){
    const source=await readFile(path.join(root,'public/studio',name),'utf8');
    assert.doesNotMatch(source,/\/api\/|127\.0\.0\.1|localhost|X-Lab-Token/);
  }
});
