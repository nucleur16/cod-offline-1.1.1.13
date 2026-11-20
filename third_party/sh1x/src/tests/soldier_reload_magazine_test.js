// Execute actual GSC reload phases as generators. An animation wait is the
// boundary; killing its thread there must leave both magazine and scheduler.
const fs=require('fs'),assert=require('assert'),root=__dirname+'/../bin/home/codol_rawfiles/animscripts/';
const source={};for(const n of ['combat','cover','move'])source[n]=fs.readFileSync(root+'soldier_'+n+'.gsc','utf8');
function body(file,name){const s=source[file],at=s.indexOf('\n'+name+'('),open=s.indexOf('{',at);assert(at>=0,name);let end=open+1,depth=1;while(depth){if(s[end]==='{')depth++;if(s[end]==='}')depth--;end++;}return s.slice(open+1,end-1);}
function lower(s){return s.replace(/self/g,'this').replace(/%([a-zA-Z0-9_]+)/g,'"$1"')
 .replace(/animscripts\\soldier_combat::/g,'')
 .replace(/this (endon|actorReloadComplete|actorClipCapacity|actorClipRemaining|setFlaggedAnimKnobAllRestart|setFlaggedAnimKnobAll|orientmode)\(/g,'this.$1(')
 .replace(/DoNoteTracksForTime\(/g,'yield DoNoteTracksForTime(');}
function fixture(){const self={a:{movement:'run',burstShots:23},moveplaybackrate:1,coop_role:'smg',remaining:9,capacity:32,completions:0,
 endon(name){assert.strictEqual(name,'killanimscript');},setFlaggedAnimKnobAllRestart(){},setFlaggedAnimKnobAll(){},orientmode(){},
 actorReloadComplete(){this.completions++;this.remaining=this.capacity;return 1;},actorClipCapacity(){return this.capacity;},actorClipRemaining(){return this.remaining;}};
 const env={isDefined:v=>v!==undefined,isAI:()=>true,projectile_weapon:()=>false,reload_duration:()=>2,animArrayPickRandom:()=> 'reload',getAnimLength:()=>2,DoNoteTracksForTime:()=> 'animation',stop_aiming(){},start_aiming(){},trace_anim(){},stat_bump(){},role_clip:()=>self.capacity,clip_remaining:()=>self.remaining,needs_reload:()=>true,randomInt:()=>0,pick:v=>v,go_to_cover(){},mode:'B',cover:{B_to_alert_reload:'back_reload',B_to_alert:'back',alert_idle:'idle'},hideYaw:0,dir:'left'};
 return {self,env};}
let corner=body('cover','corner_step_out_and_shoot');corner=corner.slice(corner.indexOf('backKey = mode + "_to_alert";'));
for(const [name,code] of [['exposed',body('combat','Reload')],['cover',body('cover','cover_reload')],['running',body('move','try_run_reload')],['return-to-cover',corner]]) {
 for(const interrupt of [false,true]) {
  const {self,env}=fixture();
  const run=Function('env','with(env){return function*(reloadAnim){'+lower(code)+'}}')(env).call(self,'reload');
  assert.strictEqual(run.next().value,'animation',name);
  assert.strictEqual(self.remaining,9,name+' preserves actual magazine during animation');
  assert.strictEqual(self.a.burstShots,23,name+' preserves scheduling count during animation');
  if(interrupt){run.return();assert.strictEqual(self.completions,0);assert.strictEqual(self.remaining,9);assert.strictEqual(self.a.burstShots,23);}
  else{assert(run.next().done,name);assert.strictEqual(self.completions,1);assert.strictEqual(self.remaining,32);assert.strictEqual(self.a.burstShots,0);}
 }
}
const {self,env}=fixture();
const role=Function('env','with(env){return function(){'+lower(body('combat','role_clip'))+'}}')(env);
const remaining=Function('env','with(env){return function(){'+lower(body('combat','clip_remaining'))+'}}')(env);
assert.strictEqual(role.call(self),32);assert.strictEqual(remaining.call(self),9);
self.a.burstShots=999;assert.strictEqual(remaining.call(self),9,'real native magazine wins over historical family counter');
env.isAI=()=>false;assert.strictEqual(role.call(self),30,'historical family fallback only outside native AI');
assert(source.combat.includes('if ( shots > remaining )')&&source.combat.includes('if ( clip_remaining() <= 0 )'));
console.log('Actual GSC: all four reload paths preserve interrupted magazines, refill only after animation, and use native 32-round capacity/remaining over family counters');
