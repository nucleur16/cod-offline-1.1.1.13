// Offline: Create Room has no Local Play box. Exercise the creators' actual option and create-event handlers: no
// control is made, and a room is created the way the ticked box created it (ui_p2p_create 1, one player minimum)
// wherever the box used to be offered - multiplayer, zombies, survival - and as before for a Campaign room.
const fs=require('fs'),path=require('path'),assert=require('assert');
const as=path.join(__dirname,'../zones/voice_ui_mp/rawfile/scaleform/default/codo.gfx.gfxpatch/as');
const source=fs.readFileSync(path.join(as,'codo/views/MatchMiniRoomCreatorView.as'),'utf8');
const zombies=fs.readFileSync(path.join(as,'codo/views/MatchMiniRoomCreatorCyborgsView.as'),'utf8');
const mpct=fs.readFileSync(path.join(as,'codo/views/matchmpctroomcreator_sub/MatchMPCTRoomCreatorView_mpcreator.as'),'utf8');
const room=fs.readFileSync(path.join(as,'codo/views/InRoomBaseView.as'),'utf8');
function body(name,text=source){
 const start=text.indexOf('function '+name+'(');assert(start>=0,name);
 let open=text.indexOf('{',start),depth=1,end=open+1;
 while(depth && end<text.length){if(text[end]==='{')depth++;if(text[end]==='}')depth--;end++;}
 return text.slice(open+1,end-1).replace(/var (\w+):[A-Za-z0-9_]+/g,'var $1').replace(/super\.getSyncDP\(/g,'this.getSyncDP(');
}
// nothing of the box is left in the front end's views: not the class, not an instance, not a child
for(const [name,text] of [['MatchMiniRoomCreatorView',source],['MatchMiniRoomCreatorCyborgsView',zombies],['MatchMPCTRoomCreatorView_mpcreator',mpct],['InRoomBaseView',room]])
 assert(!/LocalCreateRoomOption|localCreateOption|K33UI_LOCAL_PLAY/.test(text),name+' makes no Local Play box');
assert(!/addChild\(/.test(body('updateLocalOption')),'the option update adds nothing to the creator');

const events=[];global.CFunctionContainerCommon={setdvar:(n,v)=>events.push([n,v])};
global.QuickMatchType={MP:1,CYBORG:2,COOP:3};
const update=new Function('with(this){'+body('updateLocalOption')+'}');
const choose=new Function('cEvt','with(this){'+body('onLocalCreateChosen')+'}');
function creator(type,map){return {_currentMatchType:type,_optionMap:{selectedValue:{id:map}},myCreateBtn:{x:178,y:626},localCreateAllowed:false};}
// multiplayer, zombies and survival rooms are created as local rooms without anything to tick
for(const [type,map] of [[1,'mp_killhouse_sh'],[2,'zm_prototype_sh'],[3,'coop_survival_mp_cross_fire']]){
 const c=creator(type,map);update.call(c);assert.strictEqual(c.localCreateAllowed,true,map);
 assert.deepStrictEqual(Object.keys(c).sort(),['_currentMatchType','_optionMap','localCreateAllowed','myCreateBtn'],'the update places no control');
 choose.call(c,null);assert.deepEqual(events.pop(),['ui_p2p_create','1']);
}
// Campaign keeps its own route, as when the box was hidden for it
let c=creator(3,'coop_spec_mp_fav_tropical');c.localCreateAllowed=true;update.call(c);
assert.strictEqual(c.localCreateAllowed,false);
choose.call(c,null);assert.deepEqual(events.pop(),['ui_p2p_create','0']);
const listeners=body('onInitListener');
assert(listeners.indexOf('this.onLocalCreateChosen')<listeners.indexOf('this.onCreateRoomBtnClickHandle'),'ui_p2p_create is stored before both stock create handlers');
assert(body('enableData').includes('CFunctionContainerCommon.setdvar("ui_p2p_create","0")') && body('onBackBtnClickHandle').includes('CFunctionContainerCommon.setdvar("ui_p2p_create","0")'),'panel entry and Back clear a stale ui_p2p_create');

global.MatchType={COOP:'coop',ZM:'zm'};global.int=Number;
const createMP=new Function('cEvt','with(this){'+body('onCreateRoomBtnClickHandle')+'}');
const createZM=new Function('cEvt','with(this){'+body('onCreateRoomBtnClickHandle',zombies)+'}');
function roomCreator(allowed){
 return {_currentMatchType:QuickMatchType.MP,_optionMap:{selectedValue:{id:'mp_killhouse_sh',minPlayer:2,maxPlayer:4}},
  localCreateAllowed:allowed,myRoomNameTF:{text:'LocalHost'},myPasswordTF:{text:''},
  myOptionCB_0:{selectedValue:{id:'normal'}},myOptionCB_1:{selectedValue:{id:4}},myOptionCB_2:{selectedValue:{id:'war'}},
  broadcast:(_event,room)=>{global.created=room}};
}
global.BCEvent={EVT_CREATE_ROOM:'create'};
let mp=roomCreator(true);createMP.call(mp,null);assert.equal(global.created.minPlayer,1,'a local room starts with one player');
mp.localCreateAllowed=false;createMP.call(mp,null);assert.equal(global.created.minPlayer,2,'a room that is not local keeps its map minimum');
const zm=roomCreator(true);zm._optionMap.selectedValue.id='zm_prototype_sh';
zm.myDifficultySelector={diffData:[{id:1}],selectedIndex:0};zm.getSyncDP=()=>{};
global.SyncDataID={LIST_DIFFICULTY:'difficulty'};
dvars={};
CFunctionContainerCommon.Dvar_GetString=n=>dvars[n]||'';
dvars.ui_p2p_create='1';createZM.call(zm,null);assert.equal(global.created.minPlayer,1);
dvars.ui_p2p_create='0';createZM.call(zm,null);assert.equal(global.created.minPlayer,2);
// the multiplayer lobby's creator: always local for a multiplayer room
const mpctParams=new Function('with(this){'+body('getRoomCreatorParams',mpct)+'}');
const mpctRoom={_currentMatchType:QuickMatchType.MP,
 myRoomNameTF:{text:'LocalHost'},myPasswordTF:{text:''},
 _optionMap:{selectedValue:{id:'mp_killhouse_sh',minPlayer:2,maxPlayer:6}},
 myOptionCB_0:{selectedValue:{id:'casual'}},myOptionCB_1:{selectedValue:{id:6}},
 myOptionCB_2:{selectedValue:{id:'war'}}};
let params=mpctParams.call(mpctRoom);assert.equal(params.room.minPlayer,1);
assert.deepEqual(events.pop(),['ui_p2p_create','1']);
mpctRoom._currentMatchType=QuickMatchType.COOP;mpctRoom.myOptionCB_1={selectedValue:{id:1}};params=mpctParams.call(mpctRoom);
assert.equal(params.room.minPlayer,2);assert.deepEqual(events.pop(),['ui_p2p_create','0']);
for(const fn of ['enable','disable'])assert(body(fn,mpct).includes('CFunctionContainerCommon.setdvar("ui_p2p_create","0")'),'the lobby creator clears a stale ui_p2p_create on '+fn);
console.log('Create Room has no Local Play box: MP, Zombies and Survival rooms are created local (ui_p2p_create 1, one player minimum); Campaign unchanged; stale choice cleared');
