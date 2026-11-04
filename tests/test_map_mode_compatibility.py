"""Execute actual AS providers/handlers with independently observed control events.

CodoComboBox.selectedDataIndex dispatches even for the same index; dataProvider
may auto-select. Album's data/index setters do NOT dispatch its arrow event.
These contracts were exported from cumulative9bda tags4781/4870; parent tab
ordering comes from tag4882 and the manual CT creator from tag5188. Native
callbacks are counters, not gameplay. MAP_MODE_AS_ROOT also exercises FFDec's
actual compiled export with exactly the same cases.
"""
from pathlib import Path
import json
import os
import re
import shutil
import subprocess
import unittest
from test_quick_ready_gate import body, javascript

ROOT = Path(__file__).resolve().parents[1]
AS = Path(os.environ.get('MAP_MODE_AS_ROOT', str(ROOT / 'zones/voice_ui_mp/rawfile/scaleform/default/codo.gfx.gfxpatch/as')))
FILES = {
    'match': 'codo/controllers/MatchController.as',
    'lobby': 'codo/controllers/LobbyController.as',
    'mp': 'codo/views/matchmpctroomcreator_sub/MatchMPCTRoomCreatorView_mpcreator.as',
    'mini': 'codo/views/MatchMiniRoomCreatorView.as',
    'ct': 'codo/views/matchmpctroomcreator_sub/MatchMPCTRoomCreatorView_ctcreator.as',
}


def lowered(text):
    return re.sub(r'(?<![.\w])(_optionMap|_currentMatchType)', r'this.\1', re.sub(r'var (\w+):\w+', r'var \1', javascript(text)))


class MapModeCompatibility(unittest.TestCase):
    def test_actual_providers_creator_events_and_native_mutation_boundaries(self):
        sources = {key: (AS / path).read_text(encoding='utf-8-sig') for key, path in FILES.items()}
        wanted = {
            'match': ['isSupportedMapMode', 'listMap', 'listMode', 'listSameTypeMap', 'onCreateRoomHandle', 'onQuickMatchHandle'],
            'lobby': ['usesLocalStartupProtocol', 'isSupportedMapMode', 'isManagedTrainingRoom', 'onChangeRoomInfoHandle', 'onChangeReadyStateHandle', 'applyRoomReadyState', 'onStartMatchHandle', 'refreshRoomMaps'],
            'mp': ['set active', 'set mapDP', 'refreshMPMaps', 'refreshMPData', 'onMapChangedHandle', 'onMapAlbumChangeHandle', 'getRoomCreatorParams'],
            'mini': ['set mapDP', 'refreshMPMaps', 'refreshMPData', 'onMapChangedHandle'],
            'ct': ['set active', 'setMaps', 'onMapCBChangedHandle', 'onMapAlbumChangeHandle', 'getRoomCreatorParams'],
        }
        methods = {key: {name: lowered(body(sources[key], name)) for name in names} for key, names in wanted.items()}
        # Exercise the real early manual-CT refusal before any mutable Quick
        # booking state. The continuation is a counter, not a second policy.
        methods['match']['onQuickMatchHandle'] = methods['match']['onQuickMatchHandle'].split('var maxPlayer', 1)[0] + '\nthis.continued++;'
        display = body(sources['lobby'], 'refreshRoomStatus')
        begin = display.index('forceInfo.showMode =')
        stop = display.index('\n            }\n            i++;', begin)
        mode_display = lowered(display[begin:stop])
        script = r'''
const assert=require('assert');
const M=METHODS;
globalThis.CFunctionContainerCommon={Dvar_GetString:()=>'',setdvar:()=>{}};
const uint=x=>Number(x)>>>0,int=x=>Number(x)|0;
const MatchType={PVP:'pvp',ZM:'zm',COOP:'coop'},QuickMatchType={MP:1,COOP:2,CYBORG:3,TRAINING:4};
const CoopMapType={ANY:0,STORY:1,SPEC_OPS:2,SURVIVAL:3,GetCoopSubType:()=>0};
const SyncDataID=new Proxy({}, {get:(_,k)=>k}),BCEventCommon={},SanitizedType={ROOM_NAME:1};
const MsgBtnCount={ONE:1},AsyncTaskID={START_MATCH:1};
const LocalizationRoom={ANY:'Any'},RoomListType={MP:1};
const BCEvent={ROOM_STATE_CHANGED:1};
const CodoComboBox={EVT_CHANGE:'change'};
const StringUtil={trim:x=>x.trim()},ScaleformUtil={isScaleform:true};
const MapObj=function(x){Object.assign(this,x)},ModeObj=MapObj;
const S='mp_sd_jardin';
const maps=[{id:'mp_killhouse_sh',unlocked:true,minPlayer:1,maxPlayer:18,description:'Killhouse'},
            {id:S,unlocked:true,minPlayer:4,maxPlayer:18,description:'Pillars'},
            {id:'mp_rust',unlocked:true,minPlayer:1,maxPlayer:18,description:'Rust'}];
const ids=['war','sd','dom','sab','dm','ctf','koth','dd','gun','oic','shrp','infect','combat'];
const modes=ids.map(id=>({id,name:id}));
const MatchService={getInstance:()=>({listMap:()=>maps.slice(),listMatchMode:()=>modes.slice()})};
const provider={};
function method(c, group, name, args='cEvt') {return new Function(...args.split(',').filter(Boolean),M[group][name]).bind(c);}
for(const [name,args] of Object.entries({isSupportedMapMode:'mapID,modeID',listMap:'matchType,insertAny=false,coop=0,coopList=false,modeID=null',listSameTypeMap:'mapID,modeID=null',listMode:'matchType,isCombat=false,insertAny=false,mapID=null'})) provider[name]=method(provider,'match',name,args);
const snapshot=JSON.stringify({maps,modes});
let checks=0;
for(const id of [...ids,'sd2','',null,'bogus']) {
 const choices=provider.listMap(MatchType.PVP,false,0,false,id);
 assert.strictEqual(choices.some(m=>m.id===S), id==='sd'||id===null);
 const eligible=provider.listMode(MatchType.PVP,false,false,S);
 assert.deepStrictEqual(eligible.map(m=>m.id),['sd']);
 assert.deepStrictEqual(provider.listMap(MatchType.PVP).map(m=>m.id),maps.map(m=>m.id));
 assert.deepStrictEqual(provider.listMode(MatchType.PVP,false,false,'mp_rust').map(m=>m.id),ids.filter(x=>x!=='combat'));
 assert.strictEqual(provider.isSupportedMapMode('mp_rust',id),true);
 assert.strictEqual(provider.isSupportedMapMode(S,id),id==='sd');checks++;
}
assert.deepStrictEqual(provider.listMap(MatchType.PVP,true,0,false,'war').map(x=>x.id),[undefined,'mp_killhouse_sh','mp_rust']);
assert.deepStrictEqual(provider.listSameTypeMap(S,'combat').map(x=>x.id),['mp_killhouse_sh','mp_rust']);
assert.strictEqual(JSON.stringify({maps,modes}),snapshot,'never mutate shared service caches');

let events=0;
class Combo {
 constructor(){this._data=[];this.index=-1;this.listeners=[];this.enabled=true;}
 addEventListener(_,fn){if(!this.listeners.includes(fn))this.listeners.push(fn);}
 removeEventListener(_,fn){this.listeners=this.listeners.filter(x=>x!==fn);}
 get dataProvider(){return this._data;}
 set dataProvider(v){const old=this.selectedValue;this._data=v; if(v.length&&(!old||this.selectedValue!==old))this.selectedDataIndex=0;}
 get selectedValue(){return this._data[this.index]||null;}
 get selectedDataIndex(){return this.index;}
 set selectedDataIndex(v){assert(v< this._data.length||v<0,'out-of-range combo write');this.index=v;if(v>=0){assert(++events<500,'recursive setter loop');for(const f of this.listeners)f({currentTarget:this});}}
}
class Album {
 constructor(){this._data=[];this.index=-1;this.listeners=[];}
 get data(){return this._data;}
 set data(v){this._data=v;if(v&&v.length)this.currentIndex=0;}
 set currentIndex(i){if(i>=0&&i<this._data.length)this.index=i;}
 get currentIndex(){return this.index;}
 arrow(i){this.currentIndex=i;for(const f of this.listeners)f({myPara:{index:i}});}
}
function sync(key,args=[]) {
 if(key==='LIST_MAP')return provider.listMap(...args);
 if(key==='LIST_MODE')return provider.listMode(...args);
 if(key==='LIST_MAX_PLAYER')return [{id:6},{id:12},{id:18}];
 if(key==='LIST_LEAGUE')return [{id:1}];
 if(key==='LIST_DIFFICULTY')return [{id:1}];
 throw Error('Unexpected creator provider '+key);
}
function creator(group,album=new Album()) {
 const c={_currentMatchType:1,mouseEnabled:true,_mapAlbum:album,localCreateOption:{visible:false,selected:false},updateLocalOption:()=>{},getSyncDP:sync,myPasswordTF:{text:''},myRoomNameTF:{text:'QA'},showMapInfo:m=>{c.preview=m.id;}};
 for(let i=0;i<4;i++)c['myOptionCB_'+i]=new Combo();c._optionMap=c.myOptionCB_3;
 for(const name of ['refreshMPMaps','refreshMPData','onMapChangedHandle'])c[name]=method(c,group,name,name==='refreshMPMaps'?'modeID':'cEvt');
 if(group==='mp') {
  for(const name of ['onMapAlbumChangeHandle','getRoomCreatorParams'])c[name]=method(c,group,name);
  Object.defineProperty(c,'active',{set:method(c,group,'set active','cSwi')});
  album.listeners.push(c.onMapAlbumChangeHandle);
 }
 Object.defineProperty(c,'mapDP',{set:method(c,group,'set mapDP','mapDP')});
 c.refreshMPData();return c;
}
for(const group of ['mp','mini']) {
 const c=creator(group);
 assert(!c._optionMap.dataProvider.some(x=>x.id===S));
 c._optionMap.selectedDataIndex=1; // Rust in war.
 c.myOptionCB_2.selectedDataIndex=1; // SD, list gains Pillars before Rust.
 assert.strictEqual(c._optionMap.selectedValue.id,'mp_rust','preserve ID, not array index');
 assert.strictEqual(c._optionMap.selectedDataIndex,2);
 c._optionMap.selectedDataIndex=1;assert.strictEqual(c._optionMap.selectedValue.id,S);
 if(group==='mp')assert.strictEqual(c._mapAlbum.data[c._mapAlbum.currentIndex].id,S);
 else assert.strictEqual(c.preview,S);
 c.myOptionCB_2.selectedDataIndex=0; // war excludes Pillars before submit.
 assert.strictEqual(c._optionMap.selectedValue.id,'mp_killhouse_sh');
 assert(!c._optionMap.dataProvider.some(x=>x.id===S));
 if(group==='mp') {
  const room=c.getRoomCreatorParams().room;
  assert.strictEqual(room.map,'mp_killhouse_sh');assert.strictEqual(room.mode,'war');
  assert.strictEqual(c._mapAlbum.data[c._mapAlbum.currentIndex].id,room.map);
 } else assert.strictEqual(c.preview,'mp_killhouse_sh');checks++;
}
const album=new Album(),mp=creator('mp',album);
const ct={mouseEnabled:false,_mapAlbum:album,myOptionCB_1:new Combo(),myMapTF:{},_difficultiesDat:[{id:1}],myLevelSelector:{selectedIndex:0},getSyncDP:sync};
for(const n of ['setMaps','onMapCBChangedHandle','onMapAlbumChangeHandle','getRoomCreatorParams'])ct[n]=method(ct,'ct',n);
Object.defineProperty(ct,'active',{set:method(ct,'ct','set active','cSwi')});
ct.myOptionCB_1.addEventListener('change',ct.onMapCBChangedHandle);album.listeners.push(ct.onMapAlbumChangeHandle);
ct.setMaps();assert(!ct.myOptionCB_1.dataProvider.some(x=>x.id===S));
assert.strictEqual(album.data,mp._optionMap.dataProvider,'inactive CT initialization cannot replace MP album');
mp.myOptionCB_2.selectedDataIndex=1;mp._optionMap.selectedDataIndex=1;
const ctOriginal=ct.myOptionCB_1.selectedValue.id;
album.arrow(2);assert.strictEqual(mp._optionMap.selectedValue.id,'mp_rust');assert.strictEqual(ct.myOptionCB_1.selectedValue.id,ctOriginal);
mp.active=false;ct.active=true; // Actual parent's CT switch ordering.
assert.strictEqual(album.data,ct.myOptionCB_1.dataProvider);
const mpOriginal=mp._optionMap.selectedValue.id;
album.arrow(1);assert.strictEqual(ct.myOptionCB_1.selectedValue.id,'mp_rust');assert.strictEqual(mp._optionMap.selectedValue.id,mpOriginal);
const f=ct.getRoomCreatorParams().filter;assert.strictEqual(f.mode,'combat');assert.notStrictEqual(f.map,S);
mp.active=true;ct.active=false; // Actual parent's MP switch ordering.
assert.strictEqual(album.data,mp._optionMap.dataProvider);
album.arrow(1);assert.strictEqual(mp._optionMap.selectedValue.id,S);assert.strictEqual(ct.myOptionCB_1.selectedValue.id,'mp_rust');
ct.setMaps();assert.strictEqual(album.data,mp._optionMap.dataProvider,'cached inactive provider is harmless');checks++;

'''
        # Function constructors resolve globals in Node's global realm.
        script = script.replace("const M=METHODS;", "const M=METHODS;\nglobalThis.QuickMatchFilterObj=function(){};globalThis.CFunctionContainerCommon={Dvar_GetString:()=>'',setdvar:()=>{}};")
        script += r'''
let edits=[],readyWrites=[],startWrites=0,notices=0;
globalThis.CFunctionContainerCommon={Dvar_GetString:()=>'',setdvar:()=>{}};
const CFunctionContainer={ChangeRoomInfo:(d,t)=>edits.push({d,t}),SetReadyState:r=>readyWrites.push(r)};
function lobby(map=S,mode='war') {
 const c={_trainingPlaylist:-1,_trainingRoomIndex:'',_roomObj:{index:318,name:'QA',isPlayList:false,currentMap:{id:map},currentMatchMode:{id:mode},currentMinPlayers:1,currentMaxPlayers:6,currentLevel:1,forces:{maps:[],votes:{},showMode:true}},_roomMembers:[{}],isRoomInfoReady:()=>true,isRoomHost:()=>true,isReady:()=>false,isLocalCampaignRoom:()=>false,cancelCampaignHostStart:()=>{},broadcast:()=>{},showMsg:()=>notices++,request:()=>startWrites++,getSyncDP:(k,a)=>k==='LIST_SAME_TYPE_MAP'?provider.listSameTypeMap(...a):k==='MAP_OBJ'?maps.find(m=>m.id===a[0]):k==='LIST_DIFFICULTY'?[]:false};
 for(const n of Object.keys(M.lobby))c[n]=method(c,'lobby',n,n==='isSupportedMapMode'?'mapID,modeID':n==='applyRoomReadyState'?'readyToSet':'cEvt');return c;
}
for(const mode of ids.concat(['sd2','bogus',''])) {
 const c=lobby(S,mode);edits=[];readyWrites=[];startWrites=0;
 c.onChangeReadyStateHandle({});c.onStartMatchHandle({});
 assert.strictEqual(readyWrites.length,mode==='sd'?1:0);assert.strictEqual(startWrites,mode==='sd'?1:0);
 c.isReady=()=>true;c.onChangeReadyStateHandle({});assert.strictEqual(readyWrites.at(-1),false,'Cancel remains genuine');
 c.onChangeRoomInfoHandle({myPara:{type:'mode',data:'war'}});assert.strictEqual(edits.length,0);
 c.onChangeRoomInfoHandle({myPara:{type:'mode',data:'sd'}});assert.strictEqual(edits.length,mode==='sd'?0:1);
 c.refreshRoomMaps();assert(c._roomObj.forces.maps.some(x=>x&&x.id===S),'actual current card retained even when invalid');
 assert.strictEqual(c._roomObj.forces.maps[c._roomObj.forces.selectedMapIndex].id,S);
 checks++;
}
for(const mode of ['war','combat','sd2','sd']) {
 const c=lobby('mp_rust',mode);edits=[];
 c.onChangeRoomInfoHandle({myPara:{type:'map',data:{id:S,minPlayer:4,maxPlayer:18}}});
 assert.strictEqual(edits.length,mode==='sd'?1:0);
 c.onChangeRoomInfoHandle({myPara:{type:'mode',data:'dom'}});assert.strictEqual(edits.at(-1).d.mode,'dom');
 c.refreshRoomMaps();assert.strictEqual(c._roomObj.forces.maps.some(x=>x.id===S),mode==='sd');checks++;
}
const display=new Function('forceInfo','isCT','isZm','isCoop','j',MODE_DISPLAY);
for(const mode of ['war','sd']) {
 const c=lobby(S,mode),force={isHost:true};c.getSyncDP=(k,a)=>provider.listMode(...a);
 display.call(c,force,false,false,false,0);
 assert.deepStrictEqual(force.modes.map(x=>x.id),['sd']);assert.strictEqual(force.selectedModeIndex,mode==='sd'?0:-1);checks++;
}
for(const manual of [false,true])for(const mode of ['war','combat','sd2','sd']) {
 const c={...provider,_currentMatchType:1,_currentQuickMatchFilter:null,continued:0,checkInParty:()=>false,checkInPartyAsNotHost:()=>false,checkInPartySize:()=>false,isInRoom:()=>false,isInRankedMatch:()=>false,showMsg:()=>notices++,broadcast:()=>c.continued++};
 const para={map:S,mode,name:'QA',maxPlayer:12};
 const n=manual?'onQuickMatchHandle':'onCreateRoomHandle';c[n]=method(c,'match',n);
 c[n]({myPara:manual?{filter:para}:para});assert.strictEqual(c.continued,mode==='sd'?1:0);checks++;
}
assert.strictEqual(JSON.stringify({maps,modes}),snapshot);
console.log(JSON.stringify({passed:true,checks,control_events:events,source:SOURCE_PATH,actual_C1:false}));
'''
        script = script.replace('METHODS', json.dumps(methods)).replace('MODE_DISPLAY', json.dumps(mode_display)).replace('SOURCE_PATH', json.dumps(str(AS)))
        # Source bodies use imported AS classes/constants; expose fixtures in
        # the same realm as Function constructors without rewriting policy.
        expose = 'Object.assign(globalThis,{uint,int,MatchType,QuickMatchType,CoopMapType,SyncDataID,BCEventCommon,SanitizedType,MsgBtnCount,AsyncTaskID,BCEvent,CodoComboBox,StringUtil,ScaleformUtil,MapObj,ModeObj,MatchService,LocalizationRoom,RoomListType});\n'
        script = script.replace('const provider={};', expose + 'const provider={};')
        script = script.replace('function lobby(map=S,mode=', 'globalThis.CFunctionContainer=CFunctionContainer;globalThis.CFunctionContainerCommon=CFunctionContainerCommon;\nfunction lobby(map=S,mode=')
        node = shutil.which('node')
        self.assertIsNotNone(node)
        result = subprocess.run([node], input=script, capture_output=True, text=True, encoding='utf-8')
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertTrue(json.loads(result.stdout)['passed'])
        print(result.stdout.strip())


if __name__ == '__main__':
    unittest.main()
