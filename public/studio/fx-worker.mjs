import {createEngine} from './engine.mjs';
import {Studio,defaultSession,validate} from './session.mjs';

const ready=(async()=>{
  const [binary,manifest]=await Promise.all([
    fetch(new URL('./fx.wasm',import.meta.url)),fetch(new URL('./fx-manifest.json',import.meta.url))]);
  if(!binary.ok||!manifest.ok)throw Error('Effects package could not load. Reload the page.');
  return createEngine(await binary.arrayBuffer(),await manifest.json());
})();
let studio;
let queue=Promise.resolve();
self.onmessage=({data:{id,path,data}})=>{
  queue=queue.catch(()=>{}).then(async()=>{
    try{
      const engine=await ready;
      let result;
      if(path==='init'){
        let restored=false;
        if(!studio){let session=defaultSession();if(data){try{session=validate(data,engine.meta);}catch{restored=true;}}studio=new Studio(engine,session);}
        result={...engine.meta,session:studio.session,restored};
      }else{
        if(!studio)throw Error('Studio is still loading');
        studio.advance();
        if(path==='session')studio.update(data);
        else if(path==='action')studio.action(data);
        else if(path!=='frame')throw Error('Unknown Studio request');
        result=studio.frame();
        if(path!=='frame')result.session=studio.session;
      }
      self.postMessage({id,result});
    }catch(error){self.postMessage({id,error:error.message});}
  });
};
