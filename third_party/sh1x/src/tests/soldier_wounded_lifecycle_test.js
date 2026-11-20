// Execute original GSC wounded phases/counter/kill handler. Native method bodies
// are independently compiled in test_actor_wounded_production.py.
const fs=require('fs'),assert=require('assert'),root=__dirname+'/../bin/home/codol_rawfiles/';
const death=fs.readFileSync(root+'animscripts/soldier_death.gsc','utf8');
const pain=fs.readFileSync(root+'animscripts/soldier_pain.gsc','utf8');
const survival=fs.readFileSync(root+'maps/mp/gametypes/_survival_ai.gsc','utf8');
function body(source,name){const at=source.indexOf('\n'+name+'('),open=source.indexOf('{',at);assert(at>=0,name);let end=open+1,depth=1;while(depth){depth+=(source[end]==='{')-(source[end]==='}');end++;}return source.slice(open+1,end-1);}
function lower(s){return s.replace(/self/g,'this').replace(/%([a-zA-Z0-9_]+)/g,'"$1"').replace(/\.size/g,'.length')
 .replace(/this (endon|actorBeginWounded|actorFinishWounded|setFlaggedAnimKnobAllRestart|setFlaggedAnimKnobAll|animmode|orientmode)\(/g,'this.$1(')
 .replace(/DoNoteTracksForTime\(/g,'yield DoNoteTracksForTime(');}
assert(!body(death,'main').includes('try_long_death'),'death cannot initiate a live wounded sequence');
assert(pain.includes('animscripts\\soldier_death::try_wounded()'));
const livingCode=body(survival,'living_enemies').replace(/\.size/g,'.length');
for(const crawl of [true,false])for(const interrupted of [false,true]){
 const level={survival_enemies:[],events:[],notify(name){this.events.push(name);}};
 const killer={credits:0,kills:0,perf:0,add_credits(n){this.credits+=n;},perf_kill(){this.perf++;},award_kill(){this.kills++;}};
 let beginCalls=0,finishCalls=0,animations=0,randomCall=0;
 const self={health:25,maxhealth:100,origin:[],angles:[0,0],a:{pose:'stand'},damageMod:'MOD_RIFLE_BULLET',
  endon(){},actorBeginWounded(){beginCalls++;this.wounded=true;return true;},
  actorFinishWounded(){finishCalls++;this.health=0;this.wounded=false;watch.next(killer);},
  setFlaggedAnimKnobAllRestart(){animations++;},setFlaggedAnimKnobAll(){animations++;},animmode(){},orientmode(){}};
 level.survival_enemies=[self];
 const env={level,isDefined:x=>x!==undefined,isAlive:e=>e.health>0,getDvar:()=>'',getEntArray:()=>[],getTime:()=>1000,
  randomInt:()=>++randomCall===2?(crawl?0:1):0,randomFloat:()=>0,damageLocationIsAny:()=>false,
  may_move:()=>true,delta_end:()=>[],getMoveDelta:()=>[],trace_anim(){},stat_bump(){},pick:x=>x[0],
  list2:(...a)=>a,list3:(...a)=>a,list4:(...a)=>a,getAnimLength:()=>1,DoNoteTracksForTime:()=> 'animation',
  credit_owner:x=>x,int:Math.floor};
 const living=Function('env','with(env){return function(){var out,i,e;'+livingCode+'}}')(env);
 const watchCode=body(survival,'death_watch').replace('self waittill( "death", attacker );','attacker=yield "death";')
  .replace(/level notify\(/g, 'level.notify(').replace(/killer maps\\mp\\gametypes\\_survival::(add_credits|perf_kill)\(/g,'killer.$1(')
  .replace('killer award_kill(', 'killer.award_kill(').replace(/self/g,'this');
 const watch=Function('env','with(env){return function*(){var attacker,killer,value;'+watchCode+'}}')(env).call(self);
 assert.equal(watch.next().value,'death');
 const run=Function('env','with(env){return function*(){var players,i,crouched,crawl,down,kind,cycles,final,twitch,length;'+lower(body(death,'try_wounded'))+'}}')(env).call(self);
 assert.equal(run.next().value,'animation');assert.equal(beginCalls,1);
 assert.equal(self.health,25);assert.equal(living().length,1);assert.equal(killer.kills,0);assert.equal(level.events.length,0);
 if(interrupted){self.health=0;self.wounded=false;watch.next(killer);run.return();assert.equal(finishCalls,0);}
 else{let step;do{assert.equal(living().length,1);assert.equal(killer.kills,0);step=run.next();}while(!step.done);assert.equal(finishCalls,1);}
 assert.equal(living().length,0);assert.equal(killer.kills,1);assert.equal(killer.credits,100);assert.equal(killer.perf,1);assert.deepEqual(level.events,['survival_axis_died']);
 const pickDeath=Function('env','with(env){return function(){'+lower(body(death,'get_death_anim'))+'}}')(env).call(self);
 assert(pickDeath.startsWith(crawl?'dying_crawl_death':'dying_back_death'));
}
// A real lethal hit/high remaining health never enters the wounded path.
for(const health of [0,-5,40,100]){
 const env={isAlive:e=>e.health>0};
 const run=Function('env','with(env){return function*(){'+lower(body(death,'try_wounded'))+'}}')(env).call({health,maxhealth:100,endon(){}});
 assert.deepEqual(run.next(),{value:false,done:true});
}
console.log('Actual GSC wounded crawl/back: live positive health counts until final damage, no early rewards, finishing-shot interruption, one kill/credit and grounded final death; lethal/high-health hits never resurrect');
