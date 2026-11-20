// Actual GSC bodies; only VM/native rendering, damage and actor boundaries are doubled.
const fs=require('fs'),path=require('path'),assert=require('assert');
const raw=path.join(__dirname,'../bin/home/codol_rawfiles');
const source=fs.readFileSync(path.join(raw,'maps/mp/gametypes/_survival_ai.gsc'),'utf8');
const shared=fs.readFileSync(path.join(raw,'animscripts/soldier_shared.gsc'),'utf8');
function body(s,n){let a=s.indexOf('\n'+n+'(');assert(a>=0,n);a=s.indexOf('{',a);let b=a+1,d=1;while(d){if(s[b]==='{')d++;if(s[b]==='}')d--;b++;}return s.slice(a+1,b-1);}
function lower(b){return b.replace(/\.size/g,'.length').replace(/codescripts\\character::modelMounted/g,'modelMounted').replace(/self/g,'this')
 .replace(/(this|charge|canister|fx|p|ai) (\w+)\s*\(/g,'$1.$2(');}
global.isDefined=x=>x!==undefined&&x!==null;global.int=Math.trunc;global.spawnStruct=()=>({});
global.level={survival_enemies:[],_effect:{chemical_cloud:'weapon/grenade/fx_gas_poison_mp',martyrdom_c4_explosion:'explosions/grenadeexp_default'},survival_over:false};
global.isWeaponMounted=w=>w!=='absent';global.precacheItem=()=>{};global.weaponType=w=>['rpg_mp','m79_mp','at4_mp'].includes(w)?'projectile':'grenade';global.weaponClass=w=>w==='m79_mp'?'grenade':'rocketlauncher';
const typeGrenade=new Function('weapon',body(source,'type_grenade'));
assert.equal(typeGrenade(''),'');assert.equal(typeGrenade('absent'),'');assert.equal(typeGrenade('rpg_mp'),'');assert.equal(typeGrenade('frag_grenade_mp'),'frag_grenade_mp');assert.equal(typeGrenade('tabun_gas_mp'),'tabun_gas_mp');
global.type_attributes=new Function('t','scale','var repeated,a;'+body(source,'type_attributes'));
const easy={health:25,speed:.9,accuracy:.9},elite={health:125,speed:1.3,accuracy:.97};
assert.deepEqual(type_attributes(easy,1),{health:25,speed:.9,accuracy:.27});
let a=type_attributes(elite,1.1);assert.equal(a.health,137);assert(Math.abs(a.speed-1.365)<1e-9);assert(Math.abs(a.accuracy-.3492)<1e-9);
assert.deepEqual(type_attributes(elite,3),{health:375,speed:1.5,accuracy:.6});
global.get_wave=new Function('num','var base,pass,past,w;'+lower(body(source,'get_wave')));
level.survival_last_wave=20;level.survival_waves=[];
level.survival_repeat_waves=[17,18,19,20].map(num=>({num,boss_delay:1,squad_type:'elite',squad_size:4,specials:['dog_splode'],special_counts:[2],bosses_ai:['jug_regular'],bosses_nonai:[],repeating:true}));
for(let n=21;n<=29;n++){const w=get_wave(n);assert.equal(w.scale,1+.1*(n-20));assert.equal(w.specials[0],'dog_splode');assert.equal(w.bosses_ai[0],'jug_regular');assert.equal(w.squad_size,4);}
assert.strictEqual(get_wave(21),level.survival_waves[21]);
const grenade_weapon=new Function(lower(body(shared,'grenade_weapon')));
assert.equal(grenade_weapon.call({survival_grenade:''}),'');
assert.equal(grenade_weapon.call({survival_grenade:'tabun_gas_mp'}),'tabun_gas_mp');
level.coop_ai_grenade='frag_grenade_mp';assert.equal(grenade_weapon.call({}),'frag_grenade_mp');
// Execute actual soldier spawn: table weapon/health/accuracy reaches the actor,
// grenade absence is explicit, and specials dispatch uses the table's real ref.
global.is_dog_type=r=>r.startsWith('dog');global.olprintln=()=>{};
global.spawn_soldier=(cls,o,angles,kind)=>({coop_weapon:kind.weapon,coop_role:kind.role,arm_soldier(){},first_pose(){},actorUseCover(value){this.cover=value;},setThreatBiasGroup(g){this.group=g;},special_behaviour(r){this.special=r;},loot_roll(){}});
global.chemical_grenade=()=>{};
let spawnBody=lower(body(source,'spawn_type')).replace('::chemical_grenade','chemical_grenade').replace(/maps\\mp\\gametypes\\_survival_utility::spawn_soldier/g,'spawn_soldier')
 .replace(/ai maps\\mp\\gametypes\\_survival_utility::(\w+)\(/g,'ai.$1(')
 .replace(/ai thread death_watch\(\);/g,'ai.death_watch_started=true;');
const spawn_type=new Function('ref','origin','angles','scale','var t,kind,ai,attributes;'+spawnBody);
level.survival_ai={easy:{...easy,classname:'actor_enemy_so_easy',weapon:'model1887_mp',grenade:'',boss:false},chemical:{health:75,speed:1.2,accuracy:1,classname:'actor_enemy_so_chemical',weapon:'m79_mp',grenade:'tabun_gas_mp',boss:false}};
let soldier=spawn_type('easy',0,0,1);assert.equal(soldier.coop_weapon,'model1887_mp');assert.equal(soldier.health,25);assert.equal(soldier.coop_grenades,0);assert.equal(soldier.special,'easy');
soldier=spawn_type('chemical',0,0,1);assert.equal(soldier.coop_weapon,'m79_mp');assert.equal(soldier.coop_role,'grenadelauncher');assert.equal(soldier.survival_grenade,'tabun_gas_mp');assert.equal(soldier.coop_grenades,2);assert.equal(soldier.special,'chemical');assert.strictEqual(soldier.survival_grenade_on_explode,chemical_grenade);
global.closest_player=()=>({health:100});global.isSubStr=(s,n)=>s.includes(n);
global.native_dog=()=>({setThreatBiasGroup(g){this.group=g;}});
let dogBody=lower(body(source,'spawn_dog')).replace(/maps\\mp\\gametypes\\_survival_utility::spawn_dog/g,'native_dog')
 .replace(/ai thread death_watch\(\);/g,'ai.death_watch_started=true;').replace(/ai thread dog_c4\(\);/g,'ai.has_c4=true;').replace(/ai thread dog_hunt\(\);/g,'ai.hunt_started=true;');
const spawnDog=new Function('ref','origin','angles','scale','var t,ai,attributes;'+dogBody);
level.survival_ai.dog_reg={health:25,speed:1,accuracy:1};level.survival_ai.dog_splode={health:25,speed:1,accuracy:1};
let dog=spawnDog('dog_reg',0,0,1);assert.equal(dog.health,25);assert.equal(dog.group,'dogs');assert(dog.hunt_started);assert(!dog.has_c4);
dog=spawnDog('dog_splode',0,0,1.1);assert.equal(dog.health,27);assert(dog.has_c4);assert(dog.death_watch_started);
let abilities=[];
const dispatch=new Function('ref',body(source,'special_behaviour').replace(/self thread (\w+)\(/g,'this.$1('));
const specialActor={martyrdom_ability(f){abilities.push(['c4',f]);},chemical_ability(){abilities.push(['gas']);},claymore_planter(){abilities.push(['claymore']);}};
for(const ref of ['martyrdom','martyrdom_volatile','chemical','hazmat','claymore','easy'])dispatch.call(specialActor,ref);
assert.deepEqual(abilities,[['c4',1.5],['c4',.05],['gas'],['gas'],['claymore']]);
// Real hazard ownership survives actor cleanup: capture origin at death and
// transfer work to level before any fuse wait.
let scheduled=[];level.schedule=(...x)=>scheduled.push(x);global.modelMounted=()=>true;
const martyr=new Function('fuse','var attacker,origin;'+lower(body(source,'martyrdom_ability')).replace('this waittill( "death", attacker );','this.waittill("death");').replace('level thread detonate_c4( origin, fuse );','level.schedule("c4",origin,fuse);'));
martyr.call({origin:100,attach(){},waittill(){return;}},1.5);assert.deepEqual(scheduled,[['c4',116,1.5]]);
const dogC4=new Function('var attacker,origin;'+lower(body(source,'dog_c4')).replace('this waittill( "death", attacker );','this.waittill("death");').replace('level thread detonate_c4( origin, 0.5 );','level.schedule("c4",origin,.5);'));
dogC4.call({origin:200,attach(){},waittill(){} });assert.deepEqual(scheduled.at(-1),['c4',216,.5]);
const chemicalDeath=new Function('var attacker,origin;'+body(source,'chemical_ability').replace(/self/g,'this').replace('this waittill( "death", attacker );','this.waittill("death");').replace('level thread chemical_cloud( origin );','level.schedule("gas",origin);'));
chemicalDeath.call({origin:300,waittill(){} });assert.deepEqual(scheduled.at(-1),['gas',324]);
let damage=[],entities=[],waits=[];
global.spawn=(cls,origin)=>{const e={cls,origin,setModel(m){this.model=m;},delete(){this.deleted=true;}};entities.push(e);return e;};
global.wait=t=>waits.push(t);global.playSoundAtPosition=()=>{};global.playFx=()=>{};global.earthquake=()=>{};global.radiusDamage=(...args)=>damage.push(args);
const detonate=new Function('origin','fuse','var charge;'+lower(body(source,'detonate_c4')));
detonate(116,1.5);assert.equal(damage.length,1);assert.deepEqual(damage[0].slice(0,4),[116,192,100,50]);assert.equal(damage[0][4].survival_enemy,true);assert(entities.at(-1).deleted);
level.survival_over=true;detonate(100,.5);assert.equal(damage.length,1);assert(entities.at(-1).deleted);level.survival_over=false;
// Gas cloud native boundaries: visible shipped FX, line-of-sight and gas-mask
// protection, real MOD_GAS/tabun damage, finite lifetime, and cleanup.
let fxEnt;global.spawnFx=(effect,origin)=>(fxEnt={effect,origin,delete(){this.deleted=true;}});global.triggerFx=()=>{};
global.distanceSquared=(a,b)=>(a-b)**2;global.bulletTracePassed=(a,b,c,p)=>!p.blocked;global.getTime=()=>5000;
const player=(origin,mask=false,blocked=false)=>({origin,mask,blocked,damage:[],hasAbilityModifier(n){assert.equal(n,'modGasMask');return this.mask;},doDamage(...a){this.damage.push(a);},shellShock(s,t){assert.equal(s,'tabun_gas_mp');}});
let players=[player(0),player(0,true),player(500),player(0,false,true),Object.assign(player(0),{laststand:true})];global.living_players=()=>players;
let gasBody=lower(body(source,'chemical_cloud')).replace(/maps\\mp\\gametypes\\_survival_utility::living_players/g,'living_players');
const gas=new Function('origin','var canister,fx,t,players,i,p;'+gasBody);
gas(0);assert.equal(players[0].damage.length,12);for(const p of players.slice(1))assert.equal(p.damage.length,0);
assert.equal(players[0].damage[0][5],'MOD_GAS');assert.equal(players[0].damage[0][7],'tabun_gas_mp');assert.equal(fxEnt.effect,'weapon/grenade/fx_gas_poison_mp');assert(fxEnt.deleted);assert(entities.at(-1).deleted);
level.survival_over=true;gas(0);assert.equal(players[0].damage.length,12);assert(fxEnt.deleted);assert(entities.at(-1).deleted);level.survival_over=false;
const chemGrenade=new Function('var origin;'+body(source,'chemical_grenade').replace('self waittill( "explode", origin );','origin=this.waittill("explode");').replace('level thread chemical_cloud( origin );','level.schedule("gas",origin);'));
scheduled=[];chemGrenade.call({waittill(n){assert.equal(n,'explode');return 123;}});assert.deepEqual(scheduled,[['gas',123]]);
assert(shared.includes('grenade = self codol_actorThrowGrenade('));assert(shared.includes('grenade thread [[self.survival_grenade_on_explode]]();'));
console.log('Actual Survival GSC: independent endless scales, table weapon/offhand spawning, safe C4 transfer/radial damage, gas projectile/cloud damage+LOS+mask+FX cleanup pass');

// Bonus back-carried weapons are not the fired gun's native magazine. Keep
// their authored40% bonus clip, but apply the user's zero-reserve drop rule.
const lootTail=body(source,'loot_drop_on_death').slice(body(source,'loot_drop_on_death').indexOf('item = spawn('));
global.loot_max=(a,b)=>Math.max(a,b);global.weaponClipSize=()=>32;
let droppedAmmo;global.spawn=()=>({itemWeaponSetAmmo(clip,reserve){droppedAmmo=[clip,reserve];}});
const bonusDrop=new Function('weapon','origin','var item,clip;'+lootTail.replace('item itemWeaponSetAmmo(', 'item.itemWeaponSetAmmo('));
bonusDrop('m4_mp',0);assert.deepEqual(droppedAmmo,[12,0]);weaponClipSize=()=>1;bonusDrop('rpg_mp',0);assert.deepEqual(droppedAmmo,[1,0]);
assert(!lootTail.includes('weaponMaxAmmo'),'no reserve grant on bonus drop');
