// Execute the actual GSC marker functions with HUD/objective engine boundaries.
const fs=require('fs'),assert=require('assert');
const source=fs.readFileSync(__dirname+'/../bin/home/codol_rawfiles/maps/mp/gametypes/_survival_kiosk.gsc','utf8');
function body(name){let start=source.indexOf('\n'+name+'('),open=source.indexOf('{',start),end=open+1,depth=1;assert(start>=0);while(depth){if(source[end]==='{')depth++;if(source[end]==='}')depth--;end++;}return source.slice(open+1,end-1);}
function lower(text){return text.replace(/self/g,'this').replace(/\.size/g,'.length')
 .replace(/maps\\mp\\gametypes\\_gameobjects::getNextObjID/g,'getNextObjID')
 .replace(/this thread marker_rotation\(\);/g,'this.rotation_started=true;')
 .replace(/(icon|kiosk\.survival_world_icon|kiosk\.survival_perk_icon) (setShader|setWaypoint|destroy)\(/g,'$1.$2(');}
let next=0,objectives={},huds=[];
const level={survival_kiosk_ammo_icon:'kiosk_item_ammo',survival_kiosk_perk_icon:'specialty_plus_one',survival_kiosk_support_icon:'specialty_predator_missile'};
const objective_add=(id,state,origin)=>{assert.strictEqual(state,'active');objectives[id]={origin};};
const objective_icon=(id,icon)=>{assert(objectives[id]);objectives[id].icon=icon;};
const objective_delete=id=>{assert(objectives[id]);delete objectives[id];};
const getNextObjID=()=>next++;
const isDefined=x=>x!==undefined&&x!==null;
const newHudElem=()=>{const hud={setShader(name,w,h){this.shader=name;assert.strictEqual(w,12);assert.strictEqual(h,12);},setWaypoint(...args){assert.deepStrictEqual(args,[1,1,0]);this.waypoint=true;},destroy(){this.destroyed=true;}};huds.push(hud);return hud;};
const world_icon=Function('newHudElem','return function(origin,material,height){'+lower(body('world_icon'))+'}')(newHudElem);
const locker={origin:[100,200,300],survival_kind:'kiosk_locker'};
const support={origin:[400,500,600],survival_kind:'kiosk_killstreak'};
const invoke=Function('level','objective_add','objective_icon','getNextObjID','world_icon','return function(){'+lower(body('create_markers'))+'}')(level,objective_add,objective_icon,getNextObjID,world_icon);
invoke.call(locker);invoke.call(support);
assert.strictEqual(next,2);assert.strictEqual(huds.length,3);
assert.strictEqual(objectives[0].origin,locker.origin);assert.strictEqual(objectives[1].origin,support.origin);
assert.deepStrictEqual(huds.map(h=>h.shader),['kiosk_item_ammo','specialty_plus_one','specialty_predator_missile']);
assert.deepStrictEqual(huds.map(h=>[h.x,h.y,h.z]),[[100,200,348],[100,200,372],[400,500,648]]);
assert(huds.every(h=>h.archived&&h.waypoint&&h.alpha===.75));assert(locker.rotation_started&&!support.rotation_started);
const rotation=body('marker_rotation').replace(/level endon\( "survival_over" \);/,'').replace(/wait\( 2 \);/g,'yield 2;');
const generator=Function('level','objective_icon','return function*(){'+lower(rotation)+'}')(level,objective_icon).call(locker);
generator.next();generator.next();assert.strictEqual(objectives[0].icon,'specialty_plus_one');
generator.next();assert.strictEqual(objectives[0].icon,'kiosk_item_ammo');
level.survival_kiosks=[locker,support];
Function('level','objective_delete','isDefined',lower(body('markers_lifecycle').replace(/level waittill\( "survival_over" \);/,'')))(level,objective_delete,isDefined);
assert.deepStrictEqual(objectives,{});assert(huds.every(h=>h.destroyed));
console.log('Actual Survival GSC: 3 original service HUD icons, 2 authored compass anchors, alternating locker services and complete shutdown cleanup pass');
