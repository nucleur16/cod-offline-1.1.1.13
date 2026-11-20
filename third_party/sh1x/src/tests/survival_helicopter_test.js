// Execute the actual Survival factory and shared rotor selection/animation bodies.
const fs=require('fs'),assert=require('assert');
const raw=__dirname+'/../bin/home/codol_rawfiles/';
const vehicles=fs.readFileSync(raw+'maps/mp/gametypes/_survival_vehicles.gsc','utf8');
const rotors=fs.readFileSync(raw+'common_scripts/_heli_rotors.gsc','utf8');
function body(source,name){let start=source.indexOf('\n'+name+'('),open=source.indexOf('{',start),end=open+1,depth=1;assert(start>=0);while(depth){if(source[end]==='{')depth++;if(source[end]==='}')depth--;end++;}return source.slice(open+1,end-1);}
const isDefined=x=>x!==undefined&&x!==null;
const isSubStr=(s,v)=>s.includes(v);
const rotor_anim_name=Function('isDefined','isSubStr','return function(model){'+body(rotors,'rotor_anim_name')+'}')(isDefined,isSubStr);
let mounted=new Set(['bh_rotors']),lookups=[],animationCalls=[];
const getAnim=(tree,name)=>{assert.strictEqual(tree,'vehicles');lookups.push(name);return mounted.has(name)?name:undefined;};
const rotor_anim=Function('isDefined','rotor_anim_name','codol_getAnim','return function(model){'+body(rotors,'rotor_anim')+'}')(isDefined,rotor_anim_name,getAnim);
let disabled=false;
const getDvar=()=>disabled?'0':'';
const olprintln=()=>{};
const spin=body(rotors,'spin_rotors').replace(/self getEntityNumber\(\)/g,'this.getEntityNumber()').replace(/self setAnim\(/g,'this.setAnim(').replace(/self\./g,'this.');
const spin_rotors=Function('isDefined','getDvar','rotor_anim','is_helicopter_model','codolprintln','return function(){'+spin+'}')(isDefined,getDvar,rotor_anim,m=>isDefined(rotor_anim_name(m)),olprintln);
let spawn=true,calls=[];
const spawnHelicopter=(owner,origin,angles,vehicle,model)=>{assert.strictEqual(owner,undefined);if(!spawn)return undefined;return {model,getEntityNumber:()=>100,setAnim(...args){animationCalls.push(args);}};};
let lowered=body(vehicles,'spawn_heli').replace(/heli common_scripts\\_heli_rotors::spin_rotors\(\);/,'spin_rotors.call(heli);').replace(/heli (setVehicleTeam|vehicleSetCompassIcon|setMaxPitchRoll|Vehicle_SetSpeed|setYawSpeed|setHoverParams)\(/g,(_,name)=>'record(heli,"'+name+'",');
const record=(heli,name,...args)=>{calls.push([name,...args]);};
const spawn_heli=Function('isDefined','spawnHelicopter','spin_rotors','record','return function(origin,angles,vehicle,model){'+lowered+'}')(isDefined,spawnHelicopter,spin_rotors,record);
for(const model of ['vehicle_little_bird_armed','vehicle_mi24p_hind_desert','vehicle_mi17_woodland_fly_cheap']){
 calls=[];animationCalls=[];lookups=[];
 const heli=spawn_heli([0,0,100],[0,0,0],'cobra_mp',model);
 assert.strictEqual(heli.team,'axis');assert.strictEqual(heli.survival_enemy,true);
 assert.deepStrictEqual(calls.slice(0,2),[['setVehicleTeam','axis'],['codol_vehicleSetCompassIcon','enemy']]);
 assert.deepStrictEqual(animationCalls,[['bh_rotors',1,0,1]]);
 spin_rotors.call(heli);assert.strictEqual(animationCalls.length,1);
}
mounted=new Set(['mi17_heli_rotors','bh_rotors']);animationCalls=[];
spawn_heli([0,0,100],[0,0,0],'pavelow_mp','vehicle_mi17_woodland_fly_cheap');assert.strictEqual(animationCalls[0][0],'mi17_heli_rotors');
disabled=true;animationCalls=[];spawn_heli([],[],'cobra_mp','vehicle_little_bird_armed');assert.strictEqual(animationCalls.length,0);
spawn=false;calls=[];assert.strictEqual(spawn_heli([],[],'cobra_mp','vehicle_little_bird_armed'),undefined);assert.strictEqual(calls.length,0);
console.log('PASS: actual Survival boss/drop factory selects native enemy icon and axis team; real rotor function uses mounted animation/fallback, idempotence, disabled option, failed spawn');
