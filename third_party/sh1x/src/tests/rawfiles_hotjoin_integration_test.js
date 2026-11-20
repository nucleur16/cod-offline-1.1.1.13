// Actual merged HotJoin GSC function; stub native client/kick boundaries only.
const fs=require('fs'),path=require('path'),assert=require('assert');
const root=path.join(__dirname,'../bin/home/codol_rawfiles');const source=fs.readFileSync(path.join(root,'maps/mp/gametypes/_teams.gsc'),'utf8');
function body(s,n){let a=s.indexOf('\n'+n+'(');assert(a>=0,n);a=s.indexOf('{',a);let b=a+1,d=1;while(d){if(s[b]==='{')d++;if(s[b]==='}')d--;b++;}return s.slice(a+1,b-1);}
let code=body(source,'tryJoinTeam').replace(/self/g,'this').replace(/\.size/g,'.length').replace(/foreach \( player in level.players \)/g,'for (const player of level.players)').replace(/maps\\mp\\gametypes\\_ol_bots::isCombatBot/g,'isCombatBot').replace(/bots\[i\] getEntityNumber\(\)/g,'bots[i].getEntityNumber()');
global.isDefined=x=>x!==undefined&&x!==null;global.isPlayer=p=>p.player;global.isCombatBot=p=>!!p.bot;global.int=Math.trunc;global.level={};let kicks=[];
global.getJoinTeamPermissions=team=>level.players.filter(p=>p.pers.team===team).length<level.teamLimit;
global.kick=(id,reason)=>{assert.equal(reason,'Kicked by server');kicks.push(id);level.players=level.players.filter(p=>p.id!==id);};
const join=new Function('team','var bots,teamCount,needed,i;'+code);
const person=(id,bot=false)=>({id,player:true,bot,pers:{team:'allies'},getEntityNumber(){return this.id;}});
function setup(players,limit){level.players=players;level.teamLimit=limit;kicks=[];}
let human=person(0);setup([person(1),person(2,true)],2);assert(join.call(human,'allies'));assert.deepEqual(kicks,[2]);assert.equal(level.players.length,1);
setup([person(1),person(2)],2);assert.equal(join.call(human,'allies'),false);assert.deepEqual(kicks,[]);
setup([person(1),person(2,true)],2);assert.equal(join.call(person(3,true),'allies'),false);assert.deepEqual(kicks,[]);
setup([person(1),person(2,true),person(3,true)],2.5);assert(join.call(human,'allies'));assert.deepEqual(kicks,[3]);
setup([],2);assert(join.call(human,'allies'));assert.deepEqual(kicks,[]);
setup([person(1),person(2,true)],2);const permissions=getJoinTeamPermissions;global.getJoinTeamPermissions=()=>false;assert.equal(join.call(human,'allies'),false,'post-kick native permission is rechecked');global.getJoinTeamPermissions=permissions;
const countdown=body(source,'updateTeamBalanceCountdown');assert(countdown.indexOf('isDefined( level.forcedTeam )')<countdown.indexOf('!gameFlag( "prematch_done" )'));
const menus=fs.readFileSync(path.join(root,'maps/mp/gametypes/_menus.gsc'),'utf8');for(const name of ['menuAllies','menuAxis']){const b=body(menus,name);assert(b.includes('::tryJoinTeam'));assert(!b.includes('openpopupMenu'));assert(b.indexOf('return;')<b.indexOf('self closeMenus()'));}
console.log('Actual HotJoin GSC: human seat reservation, no human/bot displacement, integral count, bounded kicks, native permission recheck; forced Survival team and prematch/menu gates preserved');
