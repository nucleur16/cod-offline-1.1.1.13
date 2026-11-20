const fs=require('fs'),path=require('path'),assert=require('assert');
const raw=path.join(__dirname,'../bin/home/codol_rawfiles');
const s=fs.readFileSync(path.join(raw,'maps/mp/gametypes/_survival.gsc'),'utf8');
const a=fs.readFileSync(path.join(raw,'maps/mp/gametypes/_survival_ai.gsc'),'utf8');
const v=fs.readFileSync(path.join(raw,'maps/mp/gametypes/_survival_vehicles.gsc'),'utf8');
function body(src,n){let a=src.indexOf('\n'+n+'(');assert(a>=0,n);a=src.indexOf('{',a);let b=a+1,d=1;while(d){if(src[b]==='{')d++;if(src[b]==='}')d--;b++;}return src.slice(a+1,b-1);}
function lower(x){return x.replace(/\.size/g,'.length').replace(/maps\\mp\\gametypes\\_survival_ai::living_enemies/g,'living_enemies').replace(/maps\\mp\\gametypes\\_survival_vehicles::living_choppers/g,'living_choppers').replace(/p setClientDvar\(/g,'p.setClientDvar(');}
global.isDefined=x=>x!==undefined&&x!==null;global.isAlive=x=>x.health>0;
global.level={survival_enemies:[],survival_choppers:[]};
global.living_enemies=new Function('var out,i,e;'+lower(body(a,'living_enemies')));
global.living_choppers=new Function('var out,i,c,n;'+lower(body(v,'living_choppers')));
global.hud_enemy_count=new Function('var count;'+lower(body(s,'hud_enemy_count')));
assert.equal(hud_enemy_count(),0);
level.survival_enemies=['infantry','dog','chemical','bomber','juggernaut','special'].map(type=>({type,health:100}));
level.survival_enemies.push(undefined,{health:0});level.survival_choppers=[{},{},{survival_dead:true},undefined];
assert.equal(hud_enemy_count(),8,'all real species + live attack choppers, no corpse/deleted/dead helicopter');
const p={writes:[],setClientDvar(n,v){this.writes.push([n,v]);}};let players=[p];global.getPlayers=()=>players;
const Gen=Object.getPrototypeOf(function*(){}).constructor;
const updates=new Gen('var count,players,i,p;'+lower(body(s,'hud_enemy_updates')).replace('level endon( "survival_over" );','').replace(/wait\(/g,'yield('));
let run=updates();assert.equal(run.next().value,.25);assert.deepEqual(p.writes,[['surHUD_enemy',8]]);run.next();assert.equal(p.writes.length,1,'unchanged count does not spam reliable client dvars');
level.survival_enemies[1].health=0;level.survival_choppers[0].survival_dead=true;run.next();assert.deepEqual(p.writes.at(-1),['surHUD_enemy',6]);
const joined={writes:[],setClientDvar:p.setClientDvar};players.push(joined);run.next();assert.deepEqual(joined.writes,[['surHUD_enemy',6]],'late join receives current count');run.return();
assert(!body(s,'player_hud').includes('self.survival_hud_credits ='),'single native credit display');
assert(body(s,'player_hud').includes('"surHUD_credits", self.survival_credit'));
console.log('Actual Survival HUD producer: all6 ground types + attack helicopters, corpse/death pruning, zero/start values, live deaths, late join and changed-only publication; native wallet has no duplicate server hudelem');
