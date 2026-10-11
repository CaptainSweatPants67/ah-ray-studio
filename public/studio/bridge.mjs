const worker=new Worker(new URL('./fx-worker.mjs',import.meta.url),{type:'module'});
const pending=new Map();let nextId=1;
const key='ah-ray-studio.session.v1';
worker.onmessage=({data:{id,result,error}})=>{
  const task=pending.get(id);if(!task)return;pending.delete(id);clearTimeout(task.timer);
  if(error)task.reject(Error(error));else task.resolve(result);
};
worker.onerror=()=>{for(const task of pending.values()){clearTimeout(task.timer);task.reject(Error('The browser effects engine could not start. Reload the page.'));}pending.clear();};
export async function request(path,data) {
  if(path==='init'&&data===undefined){try{data=JSON.parse(localStorage.getItem(key));}catch{data=null;}}
  const id=nextId++;
  const result=await new Promise((resolve,reject)=>{
    const timer=setTimeout(()=>{pending.delete(id);reject(Error('Effects engine timed out. Reload the page.'));},30000);
    pending.set(id,{resolve,reject,timer});worker.postMessage({id,path,data});
  });
  if(path==='session'||path==='init'){
    try{localStorage.setItem(key,JSON.stringify(result.session));}catch{}
  }
  return result;
}
