// Execute incoming GSC bodies; engine/equipment boundaries are explicit stubs.
const fs=require('fs'),path=require('path'),assert=require('assert');
const root=path.join(__dirname,'../bin/home/codol_rawfiles');
const read=n=>fs.readFileSync(path.join(root,n),'utf8');
const weapons=read('maps/_zombiemode_weapons.gsc'),zm=read('maps/_zombiemode.gsc'),combat=read('animscripts/zombie_combat.gsc');
function body(s,n){let a=s.indexOf('\n'+n+'(');assert(a>=0,n);a=s.indexOf('{',a);let b=a+1,d=1;while(d){if(s[b]==='{')d++;if(s[b]==='}')d--;b++;}return s.slice(a+1,b-1);}
function translate(s){return s.replace(/self/g,'this').replace(/\.size/g,'.length').replace(/(this|player) maps\\\w+::(\w+)\s*\(/g,'$1.$2(').replace(/model thread weapon_show\s*\(/g,'model.weapon_show(').replace(/(this|player) (\w+)\s*\(/g,'$1.$2(');}
global.isDefined=x=>x!==undefined&&x!==null;global.level={};let override=0;global.getDvarInt=()=>override;global.spawnStruct=()=>({});
const put=new Function('clip','fractions','var entry;'+translate(body(zm,'zombie_melee_strike')));
const times=new Function('clip','var i;'+translate(body(zm,'zombie_melee_strike_times')));
level._zombie_melee_strike=[];put('exception',[.2143,.4571]);assert.deepEqual(times('exception'),[.2143,.4571]);assert.equal(times('normal'),undefined);override=-1;assert.equal(times('exception'),undefined);override=0;
let delays=[],hits=0;global.getAnimLength=()=>10;global.wait=n=>delays.push(n);
const schedule=new Function('zombie_attack','fractions','var length,elapsed,i,at;'+translate(body(combat,'strike_schedule')));
schedule.call({endon(){},swing_hit(){hits++;return true;}},'exception',[.2,.5]);assert.equal(hits,2);assert.deepEqual(delays,[2,3]);
delays=[];hits=0;schedule.call({endon(){},swing_hit(){hits++;return false;}},'exception',[.2,.5]);assert.equal(hits,1);assert.deepEqual(delays,[2]);
const classify=new Function('weapon_name',body(weapons,'is_lethal_wall_weapon'));assert(classify('frag_grenade_zm'));assert(classify('semtex_grenade_zm'));assert(!classify('m14ebr_zm'));
const buy=new Function('player','cost','var lethal,model;'+translate(body(weapons,'lethal_wall_buy')));
function fixture(score,has,fraction){const p={score,owned:has,fraction,grants:[],refills:[],sounds:[],lethal_grenade(){return 'semtex_grenade_zm';},hasWeapon(){return this.owned;},getFractionMaxAmmo(){return this.fraction;},no_cash_vox(){this.noCash=true;},minus_to_player_score(n){this.score-=n;},giveWeapon(n){this.grants.push(n);},giveMaxAmmo(n){this.refills.push(n);},play_sound_on_ent(n){this.sounds.push(n);}};const wall={first_time_triggered:false,zombie_weapon_upgrade:'frag_grenade_zm',wall_buy_model(){return undefined;},play_sound_on_ent(){}};return {p,wall};}
let {p,wall}=fixture(500,true,.5);buy.call(wall,p,250);assert.equal(p.score,250);assert.deepEqual(p.refills,['semtex_grenade_zm']);assert.deepEqual(p.grants,[]);assert.equal(wall.first_time_triggered,true);
({p,wall}=fixture(500,true,1));buy.call(wall,p,250);assert.equal(p.score,500);assert.deepEqual(p.refills,[]);
({p,wall}=fixture(100,false,0));buy.call(wall,p,250);assert.equal(p.score,100);assert(p.noCash);assert.deepEqual(p.grants,[]);
({p,wall}=fixture(500,false,0));p.lethal_grenade=()=>undefined;buy.call(wall,p,250);assert.deepEqual(p.grants,['frag_grenade_zm']);assert.deepEqual(p.refills,['frag_grenade_zm']);
const perks=read('maps/_zombiemode_perks.gsc');assert(body(perks,'vending_trigger_think').includes('if ( self.classname == "trigger_use" )'));
console.log('Incoming ZM GSC bodies: clip-specific strike timing/override/cancel, lethal walls own-grenade refill/full/insufficient/fallback, safe usable perk trigger passed');

// Node-authored traversal chooses C1 climb/drop clips; crawler exceptions stay explicit.
global.is_true=x=>!!x;
const jump=read('animscripts/zombie_jump.gsc');
const traverse=new Function('animscript','var legs;'+translate(body(jump,'pick_climb_or_drop_anim')).replace(/%(\w+)/g,'"$1"'));
assert.equal(traverse.call({has_legs:true},'zombie_jump_up_to_climb'),'ai_zombie_jump_up_2_climb');
assert.equal(traverse.call({has_legs:false},'zombie_jump_up_to_climb'),'ai_zombie_crawl_jump_up_2_climb');
assert.equal(traverse.call({has_legs:true},'zombie_jump_down_120'),'ai_zombie_jump_down_120');
assert.equal(traverse.call({has_legs:false},'zombie_jump_down_120'),'ai_zombie_crawl_jump_down_97');
assert.equal(traverse.call({has_legs:false},'zombie_jump_down_40'),undefined);
assert.equal(traverse.call({has_legs:true},'unknown'),undefined);
const tele=read('maps/zm_factory_sh_teleporter.gsc');global.factory_sound_log=()=>{};
const hold=new Function(translate(body(tele,'teleport_stance_hold')).replace('this thread teleport_stance_watch();','this.teleport_stance_watch();'));
const release=new Function(translate(body(tele,'teleport_stance_release')));
const person={name:'probe',stance:'prone',watchers:0,crouch:[],prone:[],getStance(){return this.stance;},setStance(x){this.stance=x;},allowCrouch(x){this.crouch.push(x);},allowProne(x){this.prone.push(x);},teleport_stance_watch(){this.watchers++;},notify(){}};
hold.call(person);hold.call(person);assert.equal(person.stance,'stand');assert.equal(person.watchers,1);assert.deepEqual(person.crouch,[false]);
release.call(person);release.call(person);assert.deepEqual(person.crouch,[false,true]);assert.deepEqual(person.prone,[false,true]);
console.log('Incoming narrow ZM GSC: authored traversal/crawler/fallback and idempotent teleport stance hold/release passed');
