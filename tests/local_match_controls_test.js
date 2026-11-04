// Run the room bridge's actual ActionScript handlers with a native command stub.
const fs=require('fs'),path=require('path'),assert=require('assert');
const source=fs.readFileSync(path.join(__dirname,'../zones/voice_ui_mp/rawfile/scaleform/default/codo.gfx.gfxpatch/as/codo/k33/LocalMatchControls.as'),'utf8');
function body(name) {
  const start=source.indexOf('function '+name+'(');assert(start>=0,name);
  let open=source.indexOf('{',start),depth=1,end=open+1;
  while(depth && end<source.length){if(source[end]==='{')depth++;if(source[end]==='}')depth--;end++;}
  return source.slice(open+1,end-1).replace(/var (\w+):[A-Za-z0-9_]+/g,'var $1');
}
let commands=[],dvars={ui_p2p_create:'0',ui_p2p_enabled:'0'};
global.CFunctionContainerCommon={Dvar_GetString:n=>dvars[n]||'',setdvar:(n,v)=>{dvars[n]=v},execnow:c=>commands.push(c)};
function control(map,gt,host=true){
 const c={room:null,lastContext:'',localSelection:false,visible:false};
 for(const n of ['mapID','category','gameType','owner','setRoom'])c[n]=new Function(n==='setRoom'?'value':'',body(n));
 c.enter=()=>c.setRoom({currentMap:{id:map},currentMatchMode:{id:gt},forces:{isHost:host}});
 return c;
}
dvars.ui_p2p_create='1';
let c=control('mp_killhouse_sh','war');c.enter();
assert.equal(c.visible,false,'room has no Local Play overlay');
assert.deepEqual(commands,['p2p_room mp_killhouse_sh war mp','p2p_localhost 1']);
assert.equal(dvars.ui_p2p_create,'0','creation choice consumed once');
assert.equal(c.localSelection,true);
commands=[];dvars.ui_p2p_enabled='1';c.setRoom(c.room);assert.deepEqual(commands,[],'same room has no duplicate start');
c.room.currentMap.id='mp_fav_tropical';c.setRoom(c.room);
assert.deepEqual(commands,['p2p_room mp_fav_tropical war mp'],'selected local route follows map change');
commands=[];c.setRoom(null);assert.deepEqual(commands,['p2p_localhost 0']);assert.equal(c.localSelection,false);
commands=[];dvars.ui_p2p_enabled='0';c=control('zm_factory_sh','zm');c.enter();
assert.deepEqual(commands,['p2p_room zm_factory_sh zm zm'],'default uses dedicated route');
commands=[];dvars.ui_p2p_enabled='1';c.setRoom(null);
c=control('coop_survival_mp_cross_fire','coop');dvars.ui_p2p_create='1';c.enter();
assert.deepEqual(commands,['p2p_room coop_survival_mp_cross_fire survival survival','p2p_localhost 1']);
commands=[];c=control('mp_killhouse_sh;quit','war');c.enter();assert.deepEqual(commands,[]);
commands=[];c=control('mp_killhouse_sh','war',false);c.enter();assert.deepEqual(commands,[]);
commands=[];c=control('coop_spec_mp_fav_tropical','coop');c.enter();assert.deepEqual(commands,[],'Campaign stays on its own route');
commands=[];c=control('mp_killhouse_sh','war');c.setRoom({currentMap:{id:'mp_killhouse_sh'},currentMatchMode:{id:'war'},forces:{isHost:true},isPlayList:true});assert.deepEqual(commands,[]);
assert(!source.includes('MouseEvent') && !source.includes('drawRect'),'room overlay removed');
console.log('Create Room choice transfers once to MP/Zombies/Survival native room context; default, map change, leave and exclusions pass');
