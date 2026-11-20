// Execute one iteration of actual maintainBots; engine bot creation/removal are boundaries.
const fs=require('fs'),path=require('path'),assert=require('assert');
const source=fs.readFileSync(path.join(__dirname,'../bin/home/codol_rawfiles/maps/mp/gametypes/_codol_bots.gsc'),'utf8');
function body(s,n){let a=s.indexOf('\n'+n+'(');assert(a>=0);a=s.indexOf('{',a);let b=a+1,d=1;while(d){if(s[b]==='{')d++;if(s[b]==='}')d--;b++;}return s.slice(a+1,b-1);}
let code=body(source,'maintainBots').replace(/\.size/g,'.length').replace(/foreach \( player in players \)/g,'for (const player of players)').replace(/level endon\( "game_ended" \);/,'level.endon("game_ended");').replace(/wait (\d+);/g,'wait($1);');
const maintain=new Function('var matchLimit,bots,humans,players,availableBots,devTarget,devCapacity,desired,fill,botMax;'+code);
global.isDefined=x=>x!==undefined&&x!==null;global.game={botSerial:0};global.isCombatBot=p=>!!p.bot;
let dvars,players,events;const STOP={};global.getDvar=n=>String(dvars[n]??'');global.getDvarInt=n=>parseInt(dvars[n]??'0')||0;global.setDvar=(n,v)=>dvars[n]=v;global.getEntArray=()=>players;global.spawnBots=n=>{assert(Number.isInteger(n)&&n>=0);events.push(['spawn',n]);};global.removeBots=(b,n)=>{assert(Number.isInteger(n)&&n>=0&&n<=b.length);events.push(['remove',n]);};global.wait=n=>{assert.equal(n,1);throw STOP;};
function run({target='',humans=2,bots=0,max=16,mode='war',forced,fill=0,normal=0,testhumans=0}){dvars={scr_devbot_target:target,sv_maxclients:max,scr_botfill:fill,scr_botmax:11,scr_bots:normal,scr_botfill_testhumans:testhumans};players=Array.from({length:humans},()=>({isbot:false})).concat(Array.from({length:bots},()=>({isbot:true,bot:true})));events=[];global.level={gameType:mode,forcedTeam:forced,endon(){}};try{maintain();}catch(e){if(e!==STOP)throw e;}return events;}
assert.deepEqual(run({target:'0',bots:3}),[['remove',3]]);
assert.deepEqual(run({target:'8',bots:3}),[['spawn',5]]);
assert.deepEqual(run({target:'2',bots:5}),[['remove',3]]);
assert.deepEqual(run({target:'99',humans:17,bots:2}),[['remove',2]]);
assert.deepEqual(run({target:'99',mode:'gun',humans:2}),[['spawn',4]]);
assert.deepEqual(run({target:'99',humans:0}),[['spawn',15]],'empty host keeps reserved human seat');
assert.deepEqual(run({target:'99',humans:0,testhumans:-99}),[['spawn',16]],'negative test counts cannot exceed hard dev cap');
assert.deepEqual(run({target:'-3',bots:1}),[['remove',1]]);
assert.deepEqual(run({target:'',humans:2,bots:3,fill:12}),[['spawn',7]],'empty override resumes normal fill');
assert.deepEqual(run({target:'',humans:2,bots:10,normal:8}),[],'normal non-fill does not gain new removal semantics');
assert.deepEqual(run({target:'8',bots:0,forced:'allies'}),[],'Survival never gains room-fill bots');
console.log('Actual maintainBots: empty/zero protocol, exact spawn/remove, negative/full/overfull clamps, playlist capacity, reserved human seat, unchanged normal fill and Survival exclusion passed');
