// Execute actual window selection/fire handler with native damage at boundary.
const fs=require('fs'),assert=require('assert');
const src=fs.readFileSync(__dirname+'/../bin/home/codol_rawfiles/maps/_zombiemode_spawner.gsc','utf8');
function body(name){const at=src.indexOf('\n'+name+'('),open=src.indexOf('{',at);assert(at>=0);let end=open+1,depth=1;while(depth){depth+=(src[end]==='{')-(src[end]==='}');end++;}return src.slice(open+1,end-1);}
function lower(s){return s.replace(/self/g,'this').replace(/\.size/g,'.length').replace(/%body/g,'"body"')
 .replace(/this animscripted\(/g,'this.animscripted(')
 .replace(/this animscripts\\zombie_shared::DoNoteTracksForTime\(/g,'this.notes(').replace('::window_attack_notetrack','window_attack_notetrack')
 .replace(/player doDamage\(/g,'player.doDamage(');}
const env={isDefined:x=>x!==undefined,is_true:x=>!!x,isAlive:e=>!!e&&e.health>0,is_player_valid:p=>!!p&&p.health>0&&!p.laststand,
 distance2d:(a,b)=>Math.hypot(a[0]-b[0],a[1]-b[1]),abs:Math.abs,randomInt:()=>0,getAnimLength:()=>2,level:{_zombie_window_attack:['actual_window_anim']}};
const strike=Function('env','with(env){return function(note){var node,anchor,i,player;'+lower(body('window_attack_notetrack'))+'}}')(env);
function fixture(){const node={origin:[0,0,0],trigger_location:{origin:[0,0,0]},barrier_chunks:[{destroyed:false}]};
 const p={health:100,origin:[20,0,0],damage:[],doDamage(...args){this.damage.push(args);this.health-=args[0];}};
 const a={health:100,origin:[-48,0,0],angles:[0,0,0],has_legs:true,attacking_node:node,animplaybackrate:1,meleeDamage:60,anims:0,animscripted(){this.anims++;},
 notes(length,flag,callback){assert.equal(length,2);assert.equal(flag,'windowattack');assert.equal(callback,env.window_attack_notetrack);this.strike=strike;}};
 env.getPlayers=()=>[p];env.window_attack_notetrack=strike;
 return {a,p,node};}
const select=Function('env','with(env){return function(){var node,i,players,reach;'+lower(body('should_attack_player_thru_boards'))+'}}')(env);
let {a,p,node}=fixture();
// A live engine note is required: starting/finishing the clip alone never damages.
// The actual timed-pump clears context on return; emulate its fire callbacks while running.
a.notes=function(length,flag,callback){assert.equal(flag,'windowattack');strike.call(this,'sndnt#cloth');assert.equal(p.damage.length,0);strike.call(this,'fire');strike.call(this,'fire');};
assert.equal(select.call(a),true);assert.equal(a.anims,1);assert.equal(p.health,40);assert.equal(p.damage.length,1);
assert.deepEqual(p.damage[0],[60,a.origin,a,a,0,'MOD_MELEE']);
assert.equal(a.window_melee_targets,undefined);strike.call(a,'fire');assert.equal(p.damage.length,1,'late note cannot damage with expired attack context');
for(const change of [f=>f.p.origin=[150,0,0],f=>f.p.origin=[20,0,95],f=>f.p.laststand=true,f=>f.p.health=0,f=>f.a.has_legs=false,f=>f.node.barrier_chunks[0].unbroken=true]){
 const f=fixture();change(f);assert.equal(select.call(f.a),false);assert.equal(f.a.anims,0);assert.equal(f.p.damage.length,0);}
// Targets are revalidated at the strike, including near-window/height and owner.
for(const change of [f=>f.p.origin=[50,0,0],f=>f.p.origin=[20,0,51],f=>f.p.origin=[-100,0,0],f=>f.p.laststand=true,f=>f.a.health=0,f=>f.a.attacking_node={},f=>f.a.meleeDamage=0]){
 const f=fixture();f.a.notes=function(){change(f);strike.call(this,'fire');};select.call(f.a);assert.equal(f.p.damage.length,0);}
// Authored node origin is the existing repair-anchor fallback; missing metadata
// is not replaced by a fabricated marker or a global extended melee range.
({a,p,node}=fixture());delete node.trigger_location;a.notes=function(){strike.call(this,'fire');};assert(select.call(a));assert.equal(p.health,40);
console.log('Actual window melee: fire-note-only native60 damage, MP ABI, one contact, live/laststand/range/floor/window/ownership guards and expired-context cleanup passed');
