// Run the room bridge's actual ActionScript handlers with a native command stub.
const fs=require('fs'),path=require('path'),assert=require('assert');
const asRoot=process.env.P2P_AS_ROOT || path.join(__dirname,'../zones/voice_ui_mp/rawfile/scaleform/default/codo.gfx.gfxpatch/as');
const source=fs.readFileSync(path.join(asRoot,'codo/k33/LocalMatchControls.as'),'utf8');
function body(name) {
  const start=source.indexOf('function '+name+'(');assert(start>=0,name);
  let open=source.indexOf('{',start),depth=1,end=open+1;
  while(depth && end<source.length){if(source[end]==='{')depth++;if(source[end]==='}')depth--;end++;}
  return source.slice(open+1,end-1).replace(/var (\w+):[A-Za-z0-9_]+/g,'var $1');
}
let commands=[],dvars={ui_p2p_create:'0',ui_p2p_enabled:'0'};
global.CFunctionContainerCommon={Dvar_GetString:n=>dvars[n]||'',setdvar:(n,v)=>{dvars[n]=v},execnow:c=>commands.push(c)};
global.int=x=>Number(x)|0;
function control(map,gt,host=true,level=1){
 const c={room:null,lastContext:'',localSelection:false,visible:false};
 for(const n of ['mapID','category','gameType','owner','difficultyIntent','setRoom'])c[n]=new Function(n==='setRoom'?'value':'',body(n));
 c.enter=()=>c.setRoom({currentMap:{id:map},currentMatchMode:{id:gt},forces:{isHost:host},currentLevel:level});
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
assert(source.includes('refreshStatus') && !source.includes('drawRect'),'the room shows the armed choice read-only (local_play_deferred_test.js) and draws nothing itself');
console.log('Create Room choice transfers once to MP/Zombies/Survival native room context; default, map change, leave and exclusions pass');

commands=[];dvars={ui_p2p_startup_protocol:'1',ui_p2p_create:'1',ui_p2p_enabled:'0'};
c=control('mp_killhouse_sh','war');c.enter();
assert.deepEqual(commands,['p2p_room mp_killhouse_sh war mp','p2p_localhost 1']);
assert(!commands.some(x=>x.indexOf('p2p_start_intent')===0),'Create never fabricates Ready/Start intent');
dvars.ui_p2p_enabled='1';commands=[];c.room.currentMap.id='bad;quit';c.setRoom(c.room);
assert.deepEqual(commands,['p2p_cancel_start'],'invalid changed context revokes the earlier attempt without dedicated fallback');
commands=[];c.room.currentMap.id='mp_rust';c.setRoom(c.room);
assert.deepEqual(commands,['p2p_room mp_rust war mp'],'valid changed context remains selected and rearms');
commands=[];c.room.forces.isHost=false;c.setRoom(c.room);
assert.deepEqual(commands,['p2p_localhost 0'],'owner loss invalidates through genuine disable');
console.log('New mode room bridge only arms, invalid changed context cancels and retains selection, and owner loss disables');

commands=[];dvars={ui_p2p_startup_protocol:'2',ui_p2p_create:'1',ui_p2p_enabled:'0'};
c=control('zm_factory_sh','zm',true,0);c.enter();
assert.deepEqual(commands,['p2p_room zm_factory_sh zm zm 1 0','p2p_localhost 1'],'real tier0 stays0');
commands=[];dvars.ui_p2p_enabled='1';c.room.currentLevel=3;c.setRoom(c.room);
assert.deepEqual(commands,['p2p_room zm_factory_sh zm zm 1 3'],'same-map difficulty change rewrites exact context and revokes old native attempt');
commands=[];c.setRoom(c.room);assert.deepEqual(commands,[],'unchanged exact context remains idempotent');
for(const bad of [-1,4,1.5,NaN,Infinity,'3',true,undefined]) {
 commands=[];c.room.currentLevel=bad;c.setRoom(c.room);
 assert.deepEqual(commands,['p2p_cancel_start'],'invalid or missing actual tier cannot reuse pending intent');
}
commands=[];dvars.ui_p2p_create='1';c=control('mp_rust','war',true,17);c.enter();
assert.deepEqual(commands,['p2p_room mp_rust war mp 0 -1','p2p_localhost 1'],'MP league is not a difficulty');
console.log('V2 tier0/Endless3 presence, same-map edits, malformed tiers and independent MP league domain pass');
