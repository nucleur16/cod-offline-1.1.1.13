const fs=require('fs'),path=require('path'),assert=require('assert');
const raw=path.join(__dirname,'../bin/home/codol_rawfiles');const source=fs.readFileSync(path.join(raw,'maps/_zombiemode_powerup_test.gsc'),'utf8');
function body(s,n){let a=s.indexOf('\n'+n+'(');assert(a>=0,n);a=s.indexOf('{',a);let b=a+1,d=1;while(d){if(s[b]==='{')d++;if(s[b]==='}')d--;b++;}return s.slice(a+1,b-1);}
let enabled=0,developer=0,started=0;global.getDvarInt=n=>n==='developer'?developer:enabled;global.level={start(){started++;}};
const main=new Function(body(source,'main').replace('level thread powerup_test_watch();','level.start();'));
for(const [dev,on,count] of [[0,0,0],[0,1,0],[1,0,0],[1,1,1]]){developer=dev;enabled=on;main();assert.equal(started,count);}
const players=[{id:7,getEntityNumber(){return this.id;}},{id:0,getEntityNumber(){return this.id;}}];global.getPlayers=()=>players;
const find=new Function('client_num','var players,i;'+body(source,'powerup_test_player').replace(/\.size/g,'.length').replace(/players\[ i \] getEntityNumber\(\)/g,'players[i].getEntityNumber()'));
assert.equal(find(0),players[1]);assert.equal(find(7),players[0]);assert.equal(find(-1),undefined);assert.equal(find(99),undefined);
for(const name of ['powerup_test_deathmachine','powerup_test_fire','powerup_test_gather','powerup_test_player','powerup_test_sample','powerup_test_watch'])assert(body(source,name).length>30);
assert(source.includes('maps\\_zombiemode_powerups::specific_powerup_drop('));assert(source.includes('maps\\_zombiemode_powerups::pandora_powerup('));
// The native VM rejects duplicate exports in include scopes, even when main's
// developer gate is disabled. Verify the real imported function namespace.
const clean=s=>s.replace(/\/\*[\s\S]*?\*\//g,'').replace(/\/\/[^\n]*/g,'');
const exported=s=>[...clean(s).matchAll(/^\s*([A-Za-z_]\w*)\s*\([^;{}]*\)\s*\{/gm)].map(m=>m[1]).filter(n=>!['if','for','while','switch'].includes(n));
const owners=new Map();
for(const m of clean(source).matchAll(/^\s*#include\s+([^;]+);/gm)){
  const file=m[1].trim().replace(/\\/g,'/')+'.gsc';
  for(const symbol of exported(fs.readFileSync(path.join(raw,file),'utf8'))){
    assert(!owners.has(symbol),`native VM duplicate include export ${symbol}: ${owners.get(symbol)} and ${file}`);
    owners.set(symbol,file);
  }
}
for(const helper of ['is_true','vector_scale','get_closest','groundpos'])
  assert.equal(owners.get(helper),'maps/_zombiemode_utility.gsc',helper+' must have its actual ZM producer');
console.log('Historical diagnostic actual GSC: double developer opt-in, exact client selection/refusal, six preserved helpers routing through current power-up handlers passed');
