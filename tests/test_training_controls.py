"""Execute the actual AS training lineage and edit/Ready handlers at lifecycle boundaries.

Native observations are inputs. This does not replace real C1 entry acceptance.
"""
from pathlib import Path
import json,re,subprocess,unittest
from test_quick_ready_gate import body,javascript
ROOT=Path(__file__).resolve().parents[1]
SOURCE=ROOT/'zones/voice_ui_mp/rawfile/scaleform/default/codo.gfx.gfxpatch/as/codo/controllers/LobbyController.as'
class TrainingControls(unittest.TestCase):
 def test_training_commit_return_ready_and_custom_room_boundaries(self):
  source=SOURCE.read_text(encoding='utf8')
  methods={n:javascript(body(source,n)) for n in ('isManagedTrainingRoom','isSupportedMapMode','initRoomInfo','onChangeRoomInfoHandle','onChangeReadyStateHandle','applyRoomReadyState','resetRoom')}
  methods={n:re.sub(r'var (\w+):(PlayListObj|RoomObj)',r'var \1',s) for n,s in methods.items()}
  script=r'''
const assert=require('assert');
const MatchType={PVP:0},MapType={NONE:0},PlayListType={COMBAT:0},CombatType={CT:0};
const BCEvent={ROOM_INFO_ONLY:1,ROOM_STATE_CHANGED:2};
const SyncDataID={GET_MAP_TYPE:1},uint=x=>Number(x)>>>0,int=x=>Number(x)|0;
let ready=false,readyWrites=0,editWrites=0;
const CFunctionContainer={SetReadyState:v=>{ready=v;readyWrites++},ChangeRoomInfo:()=>editWrites++};
const dvars={};
const CFunctionContainerCommon={Dvar_GetString:key=>dvars[key]||'',setdvar:(key,value)=>{dvars[key]=value}};
const methods=METHODS;
function fresh(id) {
 const c={_trainingRoomIndex:'',_trainingPlaylist:-1,
  _roomObj:{index:0,currentMatchMode:{},currentMap:{},forces:{votes:{},maps:[],difficulties:[],pilots:[]}},
  resetRoomVoting:()=>{},getCurrentPlayList:()=>({id,type:0,combatTrainingType:0}),getSyncDP:()=>0,
  broadcast:()=>{},isRoomHost:()=>true,isRoomInfoReady:()=>true,isReady:()=>ready,isLocalCampaignRoom:()=>false,cancelCampaignHostStart:()=>{}};
 for(const [name,text] of Object.entries(methods)) c[name]=name==='isSupportedMapMode'?new Function('mapID','modeID',text):new Function(name==='initRoomInfo'?'obj':name==='applyRoomReadyState'?'readyToSet':name.startsWith('on')?'cEvt':'unused',text);
 return c;
}
function info(playlist,index=263) {return {name:'p15db7b2eb48e32',index,password:'',isPlayList:playlist,
 matchType:0,currentLevel:1,currentMatchMode:{id:playlist?'playlist':'war',name:'Team Deathmatch'},
 currentMinPlayers:1,currentMaxPlayers:1,currentMap:{id:'mp_killhouse_sh',name:'Killhouse'},playListID:0};}
for(const id of [0,2]) {
 let c=fresh(id);c.initRoomInfo(info(true));assert(c.isManagedTrainingRoom());
 c.initRoomInfo(info(false));assert(c.isManagedTrainingRoom(),'native commit must retain matchmaking ownership');
 c=fresh(id);c.initRoomInfo(info(false));assert(c.isManagedTrainingRoom(),'a new movie/controller after gameplay must restore the same room context');
 assert.strictEqual(c._roomObj._managedTrainingPlaylist,id);
 for(const type of ['map','mode']) c.onChangeRoomInfoHandle({myPara:{type,data:{id:'mp_nuked'}}});
 assert.strictEqual(editWrites,0,'manual edit must not reach native mutation');
 for(let i=0;i<4;i++) { c.onChangeReadyStateHandle({});assert.strictEqual(c._roomObj.forces.enableChangeMapBtn,false); }
 assert.strictEqual(readyWrites,(id===0?4:8),'Ready callback must remain reachable');
 assert(c.isRoomHost(),'internal coordinator must remain available for booking');
 c.initRoomInfo(info(false,264));assert(!c.isManagedTrainingRoom(),'another room must not inherit lineage');
 c.initRoomInfo(info(false));assert(!c.isManagedTrainingRoom(),'replacement must discard the previous context');
 c.initRoomInfo(info(true));const replaced=fresh(id);const wrongOwner=info(false);wrongOwner.name='AnotherOwner';replaced.initRoomInfo(wrongOwner);assert(!replaced.isManagedTrainingRoom(),'room owner/name mismatch must discard context');
 c.initRoomInfo(info(true));c.resetRoom();assert(!c.isManagedTrainingRoom());
 assert.strictEqual(dvars.codol_training_room_context,'','Leave must clear process-persistent context');
 c.initRoomInfo(info(false));assert(!c.isManagedTrainingRoom(),'custom room after Leave must remain editable');
}
for(const id of [1,11]) {const c=fresh(id);c.getCurrentPlayList=()=>({id,type:1,combatTrainingType:0});c.initRoomInfo(info(true));assert(!c.isManagedTrainingRoom());}
console.log(JSON.stringify({passed:true,trainingModes:2,readyWrites,nativeEditWrites:editWrites,C1:false}));
'''.replace('METHODS',json.dumps(methods))
  result=subprocess.run(['node','-e',script],capture_output=True,text=True)
  self.assertEqual(result.returncode,0,result.stdout+result.stderr)
  self.assertTrue(json.loads(result.stdout)['passed'])
if __name__=='__main__':unittest.main()
