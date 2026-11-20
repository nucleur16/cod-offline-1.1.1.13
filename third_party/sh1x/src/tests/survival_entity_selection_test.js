// Execute the actual GSC cleanup body over the shipped Favela MapEnts metadata.
const fs=require('fs'),path=require('path'),zlib=require('zlib'),assert=require('assert');
const root=path.join(__dirname,'..');
const source=fs.readFileSync(path.join(root,'bin/home/codol_rawfiles/maps/mp/_load.gsc'),'utf8');
const fixture=JSON.parse(zlib.inflateSync(fs.readFileSync(path.join(__dirname,'fixtures/survival_fav_entity_policy.json.zlib'))));
function body(text){let a=text.indexOf('codolRemoveMissionEntities()\n{');assert(a>=0);a=text.indexOf('{',a)+1;let p=a,depth=1;while(depth){if(text[p]==='{')depth++;if(text[p]==='}')depth--;p++;}return text.slice(a,p-1).replace(/\.size\b/g,'.length').replace(/ent\s+delete\(\);/g,'ent.delete();');}
const run=new Function('getDvar','getentarray','isDefined','assertEx','printLn',body(source));
function execute(category,specialops){const entities=fixture.entities.map(e=>({...e,deleted:false,delete(){this.deleted=true;}}));run(n=>n==='sv_zoneType'?category:specialops,()=>entities,v=>v!==undefined,(v,msg)=>assert(v,msg),()=>{});return entities;}
const mission=e=>e.script_specialops===1 || e.script_specialopsname!==undefined;
assert.equal(fixture.entities.length,2178);
for(const [category,specialops,filter] of [['mp','0',true],['mp','1',false],['survival','0',true],['survival','1',true],['coop','0',false],['coop','1',false],['zm','0',false]]){
 const result=execute(category,specialops);
 assert.equal(result.filter(e=>e.deleted).length,filter?241:0,category+'/'+specialops);
 result.forEach((e,i)=>assert.equal(e.deleted,filter && mission(fixture.entities[i]),'only explicit mission ownership can delete entity '+i));
 if(filter){assert.equal(result.filter(e=>e.deleted && (e.model||'').startsWith('*')).length,123);assert.equal(result.filter(e=>!e.deleted && (e.model||'').startsWith('*')).length,125);assert.equal(result.filter(e=>!e.deleted && e.classname==='script_model').length,378);assert.equal(result.filter(e=>!e.deleted && e.script_specialops===0).length,22);}
}
// A regression to the old mp-only guard must reproduce the campaign-blocker leak.
const old=body(source).replace('category != "mp" && category != "survival"','category != "mp"');
const broken=new Function('getDvar','getentarray','isDefined','assertEx','printLn',old);
let leaked=fixture.entities.map(e=>({...e,deleted:false,delete(){this.deleted=true;}}));broken(n=>n==='sv_zoneType'?'survival':'0',()=>leaked,v=>v!==undefined,(v,m)=>assert(v,m),()=>{});assert.equal(leaked.filter(e=>e.deleted).length,0);
const placements=fs.readFileSync(path.join(root,'bin/home/codol_rawfiles/maps/mp/survival/mp_fav_tropical.gsc'),'utf8');
for(const [field,count] of Object.entries(fixture.survival_authored_placements)){
 const pattern=new RegExp('level\\.'+field+'\\[\\s*level\\.'+field+'\\.size\\s*\\]\\s*=','g');
 assert.equal([...placements.matchAll(pattern)].length,count,field+' authored data retained');
}
console.log('Survival entity selection:241 mission props/123 brushes removed;1937 retained/125 brushes; MP/coop guards and authored player/kiosk/drop placements pass');
