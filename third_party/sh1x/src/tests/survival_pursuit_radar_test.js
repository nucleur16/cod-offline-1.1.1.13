// Execute production Survival movement and UAV status bodies with native boundaries.
const fs=require('fs'),path=require('path'),assert=require('assert');
const raw=path.join(__dirname,'../bin/home/codol_rawfiles');
function text(p){return fs.readFileSync(path.join(raw,p),'utf8');}
function body(s,n){let a=s.indexOf('\n'+n+'(');assert(a>=0,n);a=s.indexOf('{',a);let b=a+1,d=1;while(d){if(s[b]==='{')d++;if(s[b]==='}')d--;b++;}return s.slice(a+1,b-1);}
function lower(s){return s.replace(/\bself\b/g,'this').replace(/this (\w+)\(/g,'this.$1(');}
const ai=text('maps/mp/gametypes/_survival_ai.gsc'),squad=text('maps/mp/gametypes/_survival_squad.gsc'),uav=text('maps/mp/killstreaks/_uav.gsc');
global.isDefined=x=>x!==undefined&&x!==null;
global.level={survival_over:false,survival_aggressive:false};
const target={origin:[20,50,100]};global.closest_player=()=>target;
let h=lower(body(ai,'hunt')).replace(/this\.(endon|notify)\([^;]*;/g,'').replace(/wait\(/g,'yield(');
const gen=Object.getPrototypeOf(function*(){}).constructor;
const hunt=new gen('var target;'+h);
let goals=[];let soldier={origin:[2000,0,0],setGoalPos(p){goals.push(p);}};
let run=hunt.call(soldier);assert.equal(run.next().value,1);assert.equal(soldier.goalradius,128);assert.strictEqual(goals[0],target.origin);
target.origin=[200,400,0];run.next();assert.strictEqual(goals[1],target.origin,'refresh moving player');level.survival_over=true;assert(run.next().done);
level.survival_over=false;global.closest_player=()=>undefined;run=hunt.call(soldier);run.next();run.return();assert.equal(goals.length,2,'no invalid target goal');
let f=lower(body(squad,'setup_follower')).replace(/this\.(endon|notify)\([^;]*;/g,'').replace(/wait\(/g,'yield(');
global.isAlive=e=>e.health>0;const follow=new gen('leader',f);let leader={health:100,origin:[4,5,6],enemy:{}};run=follow.call(soldier,leader);assert.equal(run.next().value,1.5);assert.equal(soldier.goalradius,128,'enemy visibility must not expand follower lag600');assert.strictEqual(goals[2],leader.origin);level.survival_aggressive=true;assert(run.next().done,'aggressive hunt owns actor after handoff');
// Actual native cover-selection input is disabled immediately for Survival;
// shared scripts keep it off using their pre-existing coop_no_cover gate.
const spawn=body(ai,'spawn_type');assert(spawn.includes('ai.coop_no_cover = true;'));assert(spawn.includes('ai codol_actorUseCover( false );'));
const shared=text('animscripts/soldier_shared.gsc');assert(body(shared,'update_cover_use').includes('isDefined( self.coop_no_cover )'));
let category='survival';global.getDvar=()=>category;
let enable=[],jam=[],updates=0;global.unblockTeamRadar=t=>jam.push([t,0]);global.blockTeamRadar=t=>jam.push([t,1]);global.setTeamRadarWrapper=(t,v)=>enable.push([t,v]);global.updateTeamUAVType=()=>updates++;
const update=new Function('team','var activeUAVs,activeCounterUAVs;'+body(uav,'updateTeamUAVStatus'));
function setup(){level={activeUAVs:{allies:0,axis:0},activeCounterUAVs:{allies:0,axis:0},otherTeam:{allies:'axis',axis:'allies'},radarMode:{}};enable=[];jam=[];updates=0;}
setup();update('allies');assert.deepEqual(enable,[['allies',1]]);assert.equal(level.radarMode.allies,'normal_radar');assert.equal(level.activeUAVs.allies,0,'mode baseline is not a fake aircraft/reward');
level.activeUAVs.allies=1;update('allies');level.activeUAVs.allies=0;update('allies');assert(enable.every(([t,v])=>v===1),'purchased expiry retains radar');
level.activeUAVs.allies=2;update('allies');assert.equal(level.radarMode.allies,'fast_radar','purchased double UAV still works');
level.activeCounterUAVs.axis=1;update('allies');assert.deepEqual(jam.at(-1),['allies',1],'normal jamming contract preserved');
setup();update('axis');assert.deepEqual(enable,[['axis',0]],'no enemy team baseline');
for(const c of ['mp','coop','zm','survival_extra','']){category=c;setup();update('allies');assert.deepEqual(enable,[['allies',0]],c+' unchanged');}
assert(body(text('maps/mp/gametypes/_survival.gsc'),'init').includes('maps\\mp\\killstreaks\\_uav::updateTeamUAVStatus( "allies" );'));
console.log('Actual Survival hunt/follower goals refresh close pursuit; native cover off; real UAV baseline persists through support expiry, preserving MP/Coop/ZM, enemy team, double UAV and jammer contracts');
