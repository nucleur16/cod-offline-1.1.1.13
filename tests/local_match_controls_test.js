// Execute the actual AS event-handler bodies with native/display stubs.
const fs=require('fs'),path=require('path'),assert=require('assert');
const source=fs.readFileSync(path.join(__dirname,'../zones/voice_ui_mp/rawfile/scaleform/default/codo.gfx.gfxpatch/as/codo/k33/LocalMatchControls.as'),'utf8');
function body(name) {
  const start=source.indexOf('function '+name+'('); assert(start>=0,name);
  let open=source.indexOf('{',start),depth=1,end=open+1;
  while(depth && end<source.length) { if(source[end]==='{') depth++; if(source[end]==='}') depth--; end++; }
  return source.slice(open+1,end-1).replace(/var (\w+):[A-Za-z0-9_]+/g,'var $1');
}
let commands=[],enabled='0';
global.CFunctionContainerCommon={Dvar_GetString:n=>n==='ui_p2p_enabled'?enabled:'',execnow:c=>commands.push(c)};
function control(map,gt,host=true) {
  const c={room:{currentMap:{id:map},currentMatchMode:{id:gt},forces:{isHost:host}},lastContext:'',refreshStatus:()=>{}};
  for(const n of ['mapID','category','gameType','owner','setRoom','toggleLocal']) c[n]=new Function(n==='setRoom'?'value':'e',body(n));
  return c;
}
let c=control('mp_killhouse_sh','war'); c.setRoom(c.room);
assert.equal(c.visible,true);
assert.deepEqual(commands,['p2p_room mp_killhouse_sh war mp']);
c.setRoom(c.room); assert.equal(commands.length,1,'unchanged context is not republished');
c.toggleLocal(null); assert.equal(commands.pop(),'p2p_localhost 1');
enabled='1'; c.toggleLocal(null); assert.equal(commands.pop(),'p2p_localhost 0'); enabled='0';
commands=[];
c=control('coop_survival_mp_cross_fire','coop'); c.setRoom(c.room);
assert.deepEqual(commands,['p2p_room coop_survival_mp_cross_fire survival survival']);
commands=[]; c=control('zm_factory_sh','zm'); c.setRoom(c.room);
assert.deepEqual(commands,['p2p_room zm_factory_sh zm zm']);
c.room.currentMap.id='zm_theater_sh'; c.setRoom(c.room);
assert.equal(commands.pop(),'p2p_room zm_theater_sh zm zm');
commands=[]; c=control('mp_fav_tropical','survival'); c.setRoom(c.room);
assert.deepEqual(commands,['p2p_room mp_fav_tropical survival survival']);
commands=[]; c=control('mp_killhouse_sh;quit','war'); c.setRoom(c.room); assert.equal(commands.length,0);
commands=[]; c=control('mp_killhouse_sh','war',false); c.setRoom(c.room); c.toggleLocal(null);
assert.equal(c.visible,false); assert.equal(commands.length,0);
c=control('coop_spec_mp_fav_tropical','coop'); c.setRoom(c.room); c.toggleLocal(null);
assert.equal(c.visible,false); assert.equal(commands.length,0,'Campaign uses controller defaults');
c=control('mp_killhouse_sh','war'); c.setRoom(c.room); commands=[];
c.room.forces.isHost=false; c.setRoom(c.room); c.toggleLocal(null);
assert.equal(c.visible,false); assert.equal(commands.length,0);
c.setRoom(null); assert.equal(c.visible,false);
assert(!source.includes('p2p_match_start') && !source.includes('p2pstop'));
assert(!source.includes('portInput') && !source.includes('difficulty') && !source.includes('bots'));
console.log('Local hosting toggle: context-only room publication, enable/disable commands, MP/Zombies/Survival categories, host-only visibility, Campaign exclusion, and no settings arguments pass');
