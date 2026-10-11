const validDirections = [[4,5],[0,1],[2,3]];
function newStrip(x=1,y=1,z=0,axis=0,span=9,count=96){return{count,x,y,z,axis,span,orientation:validDirections[axis][0],membership:0,segments:[]};}
function roomLayout(){return Array.from({length:4},(_,i)=>({id:i+1,name:`Dual ${String(i+1).padStart(2,'0')}`,outputs:[newStrip(i%2?7:1,i<2?1:7),newStrip(i%2?7:1,i<2?1:7,8,i%2?2:1,3)]}));}
function wallRoomLayout(){
  const nodes=[];
  const add=(outputs)=>{const id=nodes.length+1;nodes.push({id,name:`Dual ${String(id).padStart(2,'0')}`,outputs});};
  // Opposite floor/ceiling strips run from the low edge to the high edge.
  for(let y=1;y<=9;y++)add([newStrip(1,y,0,1,9,100),newStrip(1,y,8,1,9,100)]);
  for(let x=2;x<=8;x++)add([{...newStrip(x,1,0,2,9,100),orientation:3},{...newStrip(x,1,8,2,9,100),orientation:3}]);
  // One ground-to-ceiling strip at each of the 32 distinct perimeter cells.
  const perimeter=[];
  for(let x=1;x<=9;x++)perimeter.push([x,1]);
  for(let y=2;y<=9;y++)perimeter.push([9,y]);
  for(let x=8;x>=1;x--)perimeter.push([x,9]);
  for(let y=8;y>=2;y--)perimeter.push([1,y]);
  for(let i=0;i<perimeter.length;i+=2)add(perimeter.slice(i,i+2).map(([x,y])=>newStrip(x,y,0,0,9,100)));
  return nodes;
}
function allRoomLayout(){
  const nodes=wallRoomLayout(),interior=[];
  // Upright columns inside the room, with room to see between them.
  for(const x of [2,4,6,8])for(const y of [2,4,6,8])interior.push(newStrip(x,y,0,0,9,100));
  // Cross the interior in both horizontal axes at heights 3, 5 and 7.
  for(const z of [2,4,6])for(const line of [3,5,7]){
    interior.push(newStrip(1,line,z,1,9,100));
    interior.push({...newStrip(line,1,z,2,9,100),orientation:3});
  }
  for(let i=0;i<interior.length;i+=2){
    const id=nodes.length+1;nodes.push({id,name:`Dual ${String(id).padStart(2,'0')}`,outputs:interior.slice(i,i+2)});
  }
  return nodes;
}
function isWallMode(nodes){return matchesLayout(nodes,wallRoomLayout());}
function isAllMode(nodes){return matchesLayout(nodes,allRoomLayout());}
function matchesLayout(nodes,expected){
  const fields=['count','x','y','z','span','axis','orientation','membership'];
  return Array.isArray(nodes)&&nodes.length===expected.length&&nodes.every((n,i)=>
    n.id===expected[i].id&&n.name===expected[i].name&&Array.isArray(n.outputs)&&n.outputs.length===2&&
    n.outputs.every((s,o)=>fields.every(key=>s[key]===expected[i].outputs[o][key])&&Array.isArray(s.segments)&&s.segments.length===0));
}

export {newStrip, roomLayout, wallRoomLayout, allRoomLayout, isWallMode, isAllMode};
