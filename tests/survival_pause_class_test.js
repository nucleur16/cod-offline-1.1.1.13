const fs=require('fs'),assert=require('assert');
const src=fs.readFileSync('zones/controller_ingame/rawfile/scaleform/default/codoingame.gfx.gfxpatch/as/MainPause.as','utf8');
let start=src.indexOf('private function survivalClassMenuFrame(');assert(start>=0);start=src.indexOf('{',start);let end=start+1,depth=1;
while(depth){if(src[end]==='{')depth++;if(src[end]==='}')depth--;end++;}
const frame=new Function('event',src.slice(start+1,end-1));
let vars,events,manager;
global.CFunctionContainerCommon={Dvar_GetString:n=>vars[n]||'',setdvar:(n,v)=>vars[n]=v,ScriptMenuResponse:(m,r)=>events.push([m,r])};
global.CommManagerPause={getInstance:()=>manager};
function fixture(){vars={g_gametype:'survival',survival_class_purchase:'1'};events=[];manager={bIsInGameMenuOpen:false};return{survivalClassMenuSeenOpen:false};}
let p=fixture();frame.call(p);assert.equal(events.length,0,'queued open must not cancel');manager.bIsInGameMenuOpen=true;frame.call(p);assert(p.survivalClassMenuSeenOpen);
manager.bIsInGameMenuOpen=false;frame.call(p);frame.call(p);assert.deepEqual(events,[['changeclass','back']]);assert.equal(vars.survival_class_purchase,'0');
p=fixture();manager.bIsInGameMenuOpen=true;frame.call(p);vars.survival_class_purchase='0';manager.bIsInGameMenuOpen=false;frame.call(p);assert.equal(events.length,0,'committed selection needs no cancellation');
p=fixture();vars.g_gametype='war';manager.bIsInGameMenuOpen=true;frame.call(p);manager.bIsInGameMenuOpen=false;frame.call(p);assert.equal(events.length,0,'other modes unchanged');
console.log('Actual pause observer: delayed open, Escape close once, successful selection, inactive intent and other modes passed');
