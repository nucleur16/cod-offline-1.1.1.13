// Actual GSC family/pacing/reload functions, with native definition/anim queries doubled.
const fs=require('fs'),assert=require('assert'),root=__dirname+'/../bin/home/codol_rawfiles/animscripts/';
const src={};for(const n of ['shared','combat','cover','move'])src[n]=fs.readFileSync(root+'soldier_'+n+'.gsc','utf8');
function body(file,name){let a=src[file].indexOf('\n'+name+'(');assert(a>=0,name);a=src[file].indexOf('{',a);let b=a+1,d=1;while(d){if(src[file][b]==='{')d++;if(src[file][b]==='}')d--;b++;}return src[file].slice(a+1,b-1);}
function lower(b){return b.replace(/self/g,'this').replace(/%([a-zA-Z0-9_]+)/g,'"$1"').replace(/\.size/g,'.length').replace(/animscripts\\soldier_combat::/g,'')
 .replace(/projectile_weapon\(\)/g,'this.projectile_weapon()').replace(/weapon_kind\(\)/g,'this.weapon_kind()')
 .replace(/this (\w+)\(/g,'this.$1(');}
global.isDefined=v=>v!==undefined;global.int=Math.trunc;global.randomInt=()=>2;
const defs={m79_mp:{type:'projectile',cls:'grenade',fire:.350,reload:2.050},rpg_mp:{type:'projectile',cls:'rocketlauncher',fire:.700,reload:3.9},mp5k_mp:{type:'bullet',cls:'smg',fire:.075,reload:2.7}};
global.weaponType=w=>defs[w].type;global.weaponClass=w=>defs[w].cls;global.weaponIsBoltAction=()=>false;
global.weaponFireTime=w=>defs[w].fire;global.weaponReloadTime=w=>defs[w].reload;global.getAnimLength=()=>.75;
global.animArrayPickRandom=n=>n==='reload'?'posed_reload':n;global.animArray=n=>n;global.tuning=()=>({burst_scale:1});
const projectile=new Function(lower(body('shared','projectile_weapon'))),kind=new Function('var kind,w,cls;'+lower(body('shared','weapon_kind')));
const duration=new Function('clip','var duration,weaponTime;'+lower(body('shared','reload_duration')));
const burst=new Function('dist','var n;'+lower(body('combat','role_burst'))),delay=new Function('var delay;'+lower(body('combat','role_fire_delay'))),fireAnim=new Function('shots',lower(body('combat','fire_anim_for')));
function actor(w){return {coop_weapon:w,coop_role:'smg',a:{array:{}},projectile_weapon:projectile,weapon_kind:kind};}
const gl=actor('m79_mp'),rocket=actor('rpg_mp'),smg=actor('mp5k_mp');
assert.equal(kind.call(gl),'rifle','M79 is rifle-held, not a shoulder launcher');assert.equal(kind.call(rocket),'rpg');assert.equal(kind.call(smg),'rifle');
for(const a of [gl,rocket]){assert.equal(burst.call(a,100),1);assert.equal(delay.call(a),defs[a.coop_weapon].fire);assert.equal(fireAnim.call(a,8),'single');assert.equal(duration.call(a,'reload'),defs[a.coop_weapon].reload);}
assert.equal(burst.call(smg,100),6);assert.equal(delay.call(smg),.075);assert.equal(fireAnim.call(smg,3),'fire');assert.equal(duration.call(smg,'reload'),.75);
global.level={coop_animsets:{rpg_stand:{single:'rpg_stand_fire',straight_level:'rpg_aim',reload:'rpg_stand_reload'},rifle_stand:{single:'rifle_single',straight_level:'rifle_aim'}}};
global.getArrayKeys=Object.keys;
const aim=new Function('cover','mode','pose','var kind,key,base,a,keys,i;'+lower(body('cover','aim_set')));
const rocketAim=aim.call(rocket,{lean_aim_straight:'rifle_lean'},'lean','stand');assert.equal(rocketAim.straight_level,'rpg_aim');assert.equal(rocketAim.reload,'rpg_stand_reload');
// Projectile cover reload selects its own posed animation and waits the
// native weapon reload duration before the magazine completion boundary.
let complete=0,chosen;
const env={trace_anim(){},stat_bump(){},DoNoteTracksForTime:t=>t,reload_duration:clip=>duration.call(rocket,clip)};
Object.assign(rocket,{endon(){},setFlaggedAnimKnobAllRestart(flag,clip){chosen=clip;},actorReloadComplete(){complete++;}});
const coverCode=lower(body('cover','cover_reload')).replace(/DoNoteTracksForTime\(/g,'yield DoNoteTracksForTime(');
const coverReload=Function('env','with(env){return function*(reloadAnim){'+coverCode+'}}')(env);
let work=coverReload.call(rocket,'rifle_cover_reload');assert.equal(work.next().value,3.9);assert.equal(chosen,'posed_reload');assert.equal(complete,0);work.return();assert.equal(complete,0);
work=coverReload.call(rocket,'rifle_cover_reload');work.next();assert(work.next().done);assert.equal(complete,1);
const runReload=new Function(lower(body('move','try_run_reload')));assert.equal(runReload.call(rocket),false);
// The native launcher consumes target direction. Do not extend its aim past
// the target as the retained bullet trace does.
const fireBody=body('combat','fire_shot');assert(fireBody.includes('end = target;'));assert(fireBody.includes('if ( !projectile_weapon() )'));
assert(fireBody.includes('self codol_actorShoot( self.coop_weapon, start, end );'));
console.log('Actual GSC: native projectile classification, M79/rifle vs RPG/shoulder animation, single shots, weapon fire/reload timing, safe interrupted cover reload, no running refill, exact native target contract pass');
