// Execute the authored GSC functions; engine calls are boundary stubs.
const fs=require('fs'),path=require('path'),assert=require('assert');
const root=path.join(__dirname,'../bin/home/codol_rawfiles');
const sur=fs.readFileSync(path.join(root,'maps/mp/gametypes/_survival.gsc'),'utf8');
const ks=fs.readFileSync(path.join(root,'maps/mp/killstreaks/_killstreaks.gsc'),'utf8');
function body(s,n){let a=s.indexOf('\n'+n+'(');assert(a>=0,n);a=s.indexOf('{',a);let b=a+1,d=1;while(d){if(s[b]==='{')d++;if(s[b]==='}')d--;b++;}return s.slice(a+1,b-1);}
global.isDefined=x=>x!==undefined&&x!==null;global.int=Math.trunc;
let mode='survival';global.getDvar=()=>mode;global.level={survival_dog_damage_cap:35};
global.survivalRules=new Function(body(ks,'codolSurvivalRules'));
global.rewardAllowed=new Function('reward',body(ks,'codolRewardAllowed'));
const bite=new Function('damage','var cap;'+body(sur,'dog_melee_damage'));
assert.equal(bite(1000),35);assert.equal(bite(50),35);assert.equal(bite(20),20);assert.equal(bite(0),0);
assert.equal(100-bite(1000)*2,30);assert(100-bite(1000)*3<=0);
level.survival_dog_damage_cap=0;assert.equal(bite(1000),35);level.survival_dog_damage_cap=25;assert.equal(bite(1000),25);
for(mode of ['survival','war','dm','coop','zom']){
 assert.equal(rewardAllowed({earned:true}),mode!=='survival',mode+' earned');
 assert.equal(rewardAllowed({earned:false}),true,mode+' purchased');
 assert.equal(rewardAllowed(undefined),false);
}
mode='survival';
// Actual guard prefixes must return before any award, notification, weapon or XP side effect.
for(const [name,args] of [['checkKillstreakReward',['streakCount']],['giveKillstreak',['streakName','isEarned','awardXp','owner']]]){
 let b=body(ks,name),boundary=name==='checkKillstreakReward'?'\n\tself codolPublish':'\n\tself endon';
 b=b.slice(0,b.indexOf(boundary));const f=new Function(...args,b+'\nreturn "continue";');
 assert.equal(f('x',true),undefined,name+' blocks earned');
 if(name==='giveKillstreak')assert.equal(f('x',false),'continue','purchased support proceeds');
 mode='war';assert.equal(f('x',true),'continue',name+' MP unchanged');mode='survival';
}
for(const n of ['codolActivateReward','killstreakUsePressed'])assert(body(ks,n).includes('!codolRewardAllowed( reward )'),n+' protects activation');
const spawn=body(ks,'onPlayerSpawned');assert(spawn.includes('codolSurvivalRules() && !isDefined( self.codolSurvivalStreaksInitialized )'));
assert(spawn.indexOf('self clearKillstreaks()')<spawn.indexOf('self thread killstreakUseWaiter()'));
// Execute the actual first-spawn guard twice: carryover is removed once, then purchases survive.
const firstSpawn=spawn.slice(spawn.indexOf('if ( codolSurvivalRules()'),spawn.indexOf('// Publish script progression'))
 .replace(/self/g,'this').replace(/this clearKillstreaks\(/g,'this.clearKillstreaks(');
const enter=new Function(firstSpawn), entrant={pers:{cur_kill_streak:11,lastEarnedStreak:11},clears:0,clearKillstreaks(){this.clears++;}};
mode='survival';enter.call(entrant);assert.equal(entrant.clears,1);assert.equal(entrant.pers.cur_kill_streak,0);
enter.call(entrant);assert.equal(entrant.clears,1,'later lives retain bought support');
mode='war';enter.call({pers:{},clearKillstreaks(){throw Error('MP carryover must not clear');}});mode='survival';
assert(sur.includes('level.survival_killstreaks = false;'));
const damage=body(sur,'player_damage');assert(damage.includes('sMeansOfDeath == "MOD_MELEE" && isDefined( eAttacker.isdog ) && eAttacker.isdog'));
assert(damage.indexOf('dog_melee_damage( iDamage )')<damage.indexOf('self armor_soak'));
// Run the full player damage wrapper with actual bite cap and armor boundary.
global.isAlive=p=>p.health>0;global.isPlayer=p=>!!p.player;global.isSubStr=(s,n)=>s.includes(n);global.getTime=()=>1000;
global.is_enemy=e=>!!e.enemy;global.dog_melee_damage=bite;
level.survival_world_damage_scale=.5;level.survival_red_cutoff=.2;level.survival_dog_damage_cap=35;
let playerDamage=body(sur,'player_damage').replace(/self/g,'this')
 .replace(/this \[\[level.survival_mp_player_damage\]\]\(/g,'this.applyDamage(')
 .replace(/this (ai_hit|armor_soak)\(/g,'this.$1(');
const take=new Function('eInflictor','eAttacker','iDamage','iDFlags','sMeansOfDeath','sWeapon','vPoint','vDir','sHitLoc','psOffsetTime',playerDamage);
let victim={health:100,maxhealth:100,armor:0,ai_hit:n=>n*.18,armor_soak(n){this.beforeArmor=n;const absorbed=Math.min(this.armor,n);this.armor-=absorbed;return n-absorbed;},applyDamage(i,a,n){this.health-=n;}};
const dog={enemy:true,isdog:true};take.call(victim,dog,dog,1000,0,'MOD_MELEE');assert.equal(victim.health,65);
victim.armor=20;take.call(victim,dog,dog,1000,0,'MOD_MELEE');assert.equal(victim.beforeArmor,35);assert.equal(victim.health,50);
take.call(victim,dog,dog,50,0,'MOD_EXPLOSIVE');assert.equal(victim.health,25,'dog splash retains world damage model');
victim.health=100;const soldier={enemy:true};take.call(victim,soldier,soldier,70,0,'MOD_MELEE');assert.equal(victim.health,30,'non-dog melee unchanged');
const ai=fs.readFileSync(path.join(root,'maps/mp/gametypes/_survival_ai.gsc'),'utf8');
const award=body(ai,'award_kill');assert(award.includes('self.kills'));assert(!award.includes('checkKillstreakReward'));assert(!award.includes('cur_kill_streak'));
console.log('Survival dog cap/three bites, armor ordering, MP mode isolation, earned reward denial, purchased support allowance, both activation guards, first-spawn carryover clearing and AI kill counting passed');

const timeout=new Function('note','time',body(sur,'waittill_timeout').replace(/self endon\( note \);/,'this.endon(note);').replace(/wait\( time \);/,'this.wait(time);'));
const clock={calls:[],endon(){},wait(n){assert(n>0);this.calls.push(n);}};
for(const duration of [-4,0,1,25])timeout.call(clock,'ready',duration);
assert.deepEqual(clock.calls,[1,25],'expired intermission never submits negative/zero native wait');
assert(body(sur,'intermission').includes('if ( total < 0 )'));
