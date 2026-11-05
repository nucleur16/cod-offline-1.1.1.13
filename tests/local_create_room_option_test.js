// Exercise the creator's actual option and create-event handlers.
const fs=require('fs'),path=require('path'),assert=require('assert');
const source=fs.readFileSync(path.join(__dirname,'../zones/voice_ui_mp/rawfile/scaleform/default/codo.gfx.gfxpatch/as/codo/views/MatchMiniRoomCreatorView.as'),'utf8');
const helper=fs.readFileSync(path.join(__dirname,'../zones/voice_ui_mp/rawfile/scaleform/default/codo.gfx.gfxpatch/as/codo/k33/LocalCreateRoomOption.as'),'utf8');
const zombies=fs.readFileSync(path.join(__dirname,'../zones/voice_ui_mp/rawfile/scaleform/default/codo.gfx.gfxpatch/as/codo/views/MatchMiniRoomCreatorCyborgsView.as'),'utf8');
const mpct=fs.readFileSync(path.join(__dirname,'../zones/voice_ui_mp/rawfile/scaleform/default/codo.gfx.gfxpatch/as/codo/views/matchmpctroomcreator_sub/MatchMPCTRoomCreatorView_mpcreator.as'),'utf8');
function body(name,text=source){
 const start=text.indexOf('function '+name+'(');assert(start>=0,name);
 let open=text.indexOf('{',start),depth=1,end=open+1;
 while(depth && end<text.length){if(text[end]==='{')depth++;if(text[end]==='}')depth--;end++;}
 return text.slice(open+1,end-1).replace(/var (\w+):[A-Za-z0-9_]+/g,'var $1').replace(/super\.getSyncDP\(/g,'this.getSyncDP(');
}
const events=[];global.CFunctionContainerCommon={setdvar:(n,v)=>events.push([n,v])};
global.QuickMatchType={MP:1,CYBORG:2,COOP:3};
const update=new Function('with(this){'+body('updateLocalOption')+'}');
const choose=new Function('cEvt','with(this){'+body('onLocalCreateChosen')+'}');
function creator(type,map,selected){return {_currentMatchType:type,_optionMap:{selectedValue:{id:map}},myCreateBtn:{x:178,y:626},localCreateOption:{selected,visible:false}};}
for(const [type,map] of [[1,'mp_killhouse_sh'],[2,'zm_prototype_sh'],[3,'coop_survival_mp_cross_fire']]){
 const c=creator(type,map,true);update.call(c);assert.equal(c.localCreateOption.visible,true);
 assert.equal(c.localCreateOption.x,type===1?90:15);
 assert.equal(c.localCreateOption.y,type===1?356:632);
 assert.equal(c.localCreateOption.multiplayerTheme,type===1);
 choose.call(c,null);assert.deepEqual(events.pop(),['ui_p2p_create','1']);
 c.localCreateOption.selected=false;choose.call(c,null);assert.deepEqual(events.pop(),['ui_p2p_create','0']);
}
let c=creator(3,'coop_spec_mp_fav_tropical',true);update.call(c);
assert.equal(c.localCreateOption.visible,false);assert.equal(c.localCreateOption.selected,false);
choose.call(c,null);assert.deepEqual(events.pop(),['ui_p2p_create','0']);
const listeners=body('onInitListener');
assert(listeners.indexOf('this.onLocalCreateChosen')<listeners.indexOf('this.onCreateRoomBtnClickHandle'),'preference stored before both stock create handlers');
assert(source.includes('localCreateOption.selected = false;') && source.includes('CFunctionContainerCommon.setdvar("ui_p2p_create","0")'),'new panel entry and Back clear stale choice');
assert(helper.includes('this.chosen=value;') && helper.includes('this.selected=!this.chosen;'),'visual option toggles');
global.MatchType={COOP:'coop',ZM:'zm'};global.int=Number;
const createMP=new Function('cEvt','with(this){'+body('onCreateRoomBtnClickHandle')+'}');
const createZM=new Function('cEvt','with(this){'+body('onCreateRoomBtnClickHandle',zombies)+'}');
function roomCreator(selected){
 return {_currentMatchType:QuickMatchType.MP,_optionMap:{selectedValue:{id:'mp_killhouse_sh',minPlayer:2,maxPlayer:4}},
  localCreateOption:{visible:true,selected},myRoomNameTF:{text:'LocalHost'},myPasswordTF:{text:''},
  myOptionCB_0:{selectedValue:{id:'normal'}},myOptionCB_1:{selectedValue:{id:4}},myOptionCB_2:{selectedValue:{id:'war'}},
  broadcast:(_event,room)=>{global.created=room}};
}
global.BCEvent={EVT_CREATE_ROOM:'create'};
let mp=roomCreator(true);createMP.call(mp,null);assert.equal(global.created.minPlayer,1);
mp.localCreateOption.selected=false;createMP.call(mp,null);assert.equal(global.created.minPlayer,2);
const zm=roomCreator(true);zm._optionMap.selectedValue.id='zm_prototype_sh';
zm.myDifficultySelector={diffData:[{id:1}],selectedIndex:0};zm.getSyncDP=()=>{};
global.SyncDataID={LIST_DIFFICULTY:'difficulty'};
dvars={};
CFunctionContainerCommon.Dvar_GetString=n=>dvars[n]||'';
dvars.ui_p2p_create='1';createZM.call(zm,null);assert.equal(global.created.minPlayer,1);
dvars.ui_p2p_create='0';createZM.call(zm,null);assert.equal(global.created.minPlayer,2);
const mpctParams=new Function('with(this){'+body('getRoomCreatorParams',mpct)+'}');
const mpctRoom={_currentMatchType:QuickMatchType.MP,localCreateOption:{visible:true,selected:true},
 myRoomNameTF:{text:'LocalHost'},myPasswordTF:{text:''},
 _optionMap:{selectedValue:{id:'mp_killhouse_sh',minPlayer:2,maxPlayer:6}},
 myOptionCB_0:{selectedValue:{id:'casual'}},myOptionCB_1:{selectedValue:{id:6}},
 myOptionCB_2:{selectedValue:{id:'war'}}};
let params=mpctParams.call(mpctRoom);assert.equal(params.room.minPlayer,1);
assert.deepEqual(events.pop(),['ui_p2p_create','1']);
mpctRoom.localCreateOption.selected=false;params=mpctParams.call(mpctRoom);
assert.equal(params.room.minPlayer,2);assert.deepEqual(events.pop(),['ui_p2p_create','0']);
assert(mpct.includes('this.localCreateOption.visible = this._currentMatchType == QuickMatchType.MP;'));
console.log('Create Room option: MP, Zombies and Survival selectable; Campaign hidden; default/back and create-event ordering pass');
