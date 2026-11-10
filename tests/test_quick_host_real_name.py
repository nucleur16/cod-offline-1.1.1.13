"""Run the actual AS host selectors and Ready callback with native-shaped names.

C1 callback110 formats name as '[clan] realName' and exports bare realName
separately. Room callback111 keeps the bare host name. Native I/O and time are
fixture inputs; these checks do not constitute real-client acceptance.
"""
from pathlib import Path
import json
import re
import shutil
import subprocess
import unittest

ROOT = Path(__file__).resolve().parents[1]
AS = ROOT / 'zones/voice_ui_mp/rawfile/scaleform/default/codo.gfx.gfxpatch/as/codo/controllers'


def method(source, name):
    start = source.index('function ' + name + '(')
    begin = source.index('{', start)
    end, depth = begin + 1, 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[begin + 1:end - 1]


def javascript(text):
    # The fixture receiver supplies BaseController's synchronous dispatch.
    # A standalone Function cannot use AS super syntax; keep the same call,
    # receiver and arguments rather than substituting a native role shortcut.
    text = text.replace('super.getSyncDP(', 'this.getSyncDP(')
    text = re.sub(r'var (\w+):(Object|Boolean|String|Array|int|uint|Number|RoomMemberObj)', r'var \1', text)
    return re.sub(r'for each\(var (\w+) in ([^)]+)\)', r'for (var \1 of \2)', text)


def parts(match, lobby):
    hosts, gates = [], []
    for name in ('bind_quickMatchReady', 'onRankedMatchPassedTimeTimerHandle'):
        text = method(match, name)
        first = text.index('var acceptedHost:Object = null;')
        end = text.index('if(acceptedHostCount == 1 && acceptedHost["_localQuickNativeReady"] == false)', first)
        hosts.append(javascript(text[first:end]) + '\nreturn {count:acceptedHostCount,host:acceptedHost};')
        gates.append(javascript(text.split('// BEGIN shared Quick all-current-member gate', 1)[1]
            .split('// END shared Quick all-current-member gate', 1)[0]) + '\nreturn quickMembersReady;')
    return dict(hosts=hosts, gates=gates, callback=javascript(method(lobby, 'cppSetMemberReadyState')))


def execute(value, *, mutation=None):
    script = r'''
const assert=require('assert');
const uint=value=>Number(value)>>>0;
const SyncDataID={IS_ME:'me',IS_ROOM_HOST:'host'};
const BCEvent={ROOM_FORCES:'forces'};
let localHost=true,localPlayerID='';
const CFunctionContainer={IsRankedMatch:()=>true,IsRoomHost:()=>localHost};
const CFunctionContainerCommon={Dvar_GetString:()=>'',GetPlayerID:()=>localPlayerID,setdvar:()=>{throw Error('no result-token fixture');}};
const actual=PARTS;
const hosts=actual.hosts.map(s=>new Function('quickRoom','const quickFilter={};\n'+s));
const gates=actual.gates.map(s=>new Function('quickRoom','queuedRequired',s));
const callback=new Function('member','ready',actual.callback);
function fresh(locale,asHost=true) {
 localHost=asHost;
 const peers=[{id:'4936378376416122282',realName:'LocaleInf1',name:'[qa18] LocaleInf1'},
              {id:'14124485489710931920',realName:'LocaleInf2',name:'[guest] LocaleInf2'}];
 for(const p of peers) Object.assign(p,{_localQuickNativeRoleKnown:true,_localQuickNativeMemberID:p.id,
  _localQuickNativeReady:false,_localQuickNativeHost:false,isReady:false,team:0});
 localPlayerID=peers[asHost?0:1].id;
 const room={name:asHost?'p69c7b0123456789':'LocaleInf1',_localQuickSharedLineage:true,forces:{force0:[peers[0]],force1:[peers[1]]}};
 const owner={_roomObj:room,_roomMembers:peers,
  getRoomMemberObjByID(id){return peers.find(p=>p.id===id);},
  getSyncDP(key,args){return key===SyncDataID.IS_ROOM_HOST ? asHost : args[0].id===peers[asHost?0:1].id;},
  broadcast:()=>{}};
 const event=(index,ready)=>callback.call(owner,peers[index].id,ready);
 return {locale,peers,room,owner,event};
}
let checks=0;
for(const locale of ['english','chinese']) for(const asHost of [true,false]) {
 let f=fresh(locale,asHost);
 // The selectors are extracted independently from both actual methods.
 for(const host of hosts) {
  const picked=host.call(f.owner,f.room);
  assert.strictEqual(picked.count,1,'private-title native host and signed-title guest must recognize the canonical owner');
  assert.strictEqual(picked.host.id,f.peers[0].id); checks++;
 }
 f.event(0,false);
 assert.strictEqual(f.room._localQuickReadyCycleHost,f.peers[0].id,'actual callback opens UID-bound cycle');checks++;
 assert.strictEqual(f.room._localQuickHostFalseSeen,true,'second actual callback selector retains host false edge');checks++;
 f.event(0,true);
 for(const gate of gates) assert.strictEqual(gate.call(f.owner,f.room,2),false,'one ready human cannot satisfy min2');
 f.event(1,false);f.event(1,true);
 for(const gate of gates) {assert.strictEqual(gate.call(f.owner,f.room,2),true,'clan names preserve current all-member ACK');checks++;}
 assert.strictEqual(f.peers[0].name,'[qa18] LocaleInf1','display clan remains unchanged');checks++;
 assert.strictEqual(f.peers[1].name,'[guest] LocaleInf2');checks++;
 f.peers[1].name='LocaleInf1';
 for(const host of hosts) assert.strictEqual(host.call(f.owner,f.room).count,1,'display-name spoof cannot become host');checks++;
 f.peers[1]._localQuickNativeMemberID='stale';
 for(const gate of gates) assert.strictEqual(gate.call(f.owner,f.room,2),false,'mismatched native UID rejected');checks++;
 f.peers[1]._localQuickNativeMemberID=f.peers[1].id;
 f.peers[1]._localQuickReadyCycle--;
 for(const gate of gates) assert.strictEqual(gate.call(f.owner,f.room,2),false,'prior epoch is not current ACK');checks++;
 f=fresh(locale,asHost);
 for(const bad of [undefined,null,'',5]) {
  f.peers[0].realName=bad;
  for(const host of hosts) assert.strictEqual(host.call(f.owner,f.room).count,asHost?1:0,'only local native owner can be identified without display text');checks++;
 }
 f=fresh(locale,asHost);f.peers[0]._localQuickNativeRoleKnown=false;
 for(const host of hosts) assert.strictEqual(host.call(f.owner,f.room).count,0,'unknown native role rejected');checks++;
 f=fresh(locale,asHost);f.peers[0]._localQuickNativeMemberID='stale';
 for(const host of hosts) assert.strictEqual(host.call(f.owner,f.room).count,0,'host UID mismatch rejected');checks++;
 f=fresh(locale,asHost);f.peers[1].realName=f.peers[0].realName;
 for(const host of hosts) assert.strictEqual(host.call(f.owner,f.room).count,asHost?1:2,'guest name ambiguity fails closed; local host stays bound to native UID');checks++;
 f=fresh(locale,asHost);f.owner.getSyncDP=(key,args)=>key===SyncDataID.IS_ROOM_HOST?asHost:args[0].id===f.peers[asHost?1:0].id;
 for(const host of hosts) assert.strictEqual(host.call(f.owner,f.room).count,0,'IS_ME and native host agreement retained');checks++;
 f=fresh(locale,asHost);
 if(asHost) {
  for(const bad of ['',f.peers[1].id,Number(f.peers[0].id)]) {
   localPlayerID=bad;
   for(const host of hosts) assert.strictEqual(host.call(f.owner,f.room).count,0,'missing, wrong or rounded native account UID fails closed');checks++;
  }
 } else {
  f.room.name='p69c7b0123456789';
  for(const host of hosts) assert.strictEqual(host.call(f.owner,f.room).count,0,'guest needs canonical signed host presentation');checks++;
  f.event(0,false);assert.strictEqual(f.room._localQuickReadyCycleHost,undefined,'guest private title cannot manufacture a host epoch');checks++;
 }
}
console.log(JSON.stringify({passed:true,checks,actual_AS_bodies:true,real_C1_acceptance:false}));
'''.replace('PARTS', json.dumps(value))
    node = shutil.which('node')
    if node is None:
        raise AssertionError('Node is required to execute the actual AS selectors')
    return subprocess.run([node, '-e', script], capture_output=True, text=True)


class QuickHostRealName(unittest.TestCase):
    def test_inherited_dispatch_keeps_receiver_identity_and_false_role(self):
        value = parts((AS/'MatchController.as').read_text(), (AS/'LobbyController.as').read_text())
        script = r'''
const assert=require('assert');
const actual=PARTS;
const uint=value=>Number(value)>>>0;
const SyncDataID={IS_ME:'me',IS_ROOM_HOST:'host'};
const BCEvent={ROOM_FORCES:'forces'};
let localHost=false,localPlayerID='';
const CFunctionContainer={IsRankedMatch:()=>true,IsRoomHost:()=>localHost};
const CFunctionContainerCommon={Dvar_GetString:()=>'',GetPlayerID:()=>localPlayerID,setdvar:()=>{throw Error('unexpected token');}};
const hosts=actual.hosts.map(s=>new Function('quickRoom','const quickFilter={};\n'+s));
const gates=actual.gates.map(s=>new Function('quickRoom','queuedRequired',s));
const callback=new Function('member','ready',actual.callback);
for(const asHost of [true,false]) {
 localHost=asHost;
 const peers=[{id:'4936378376416122282',realName:'中文主持人',name:'[clan] 中文主持人'},
              {id:'14124485489710931920',realName:'Guest',name:'[guest] Guest'}];
 for(const p of peers) Object.assign(p,{_localQuickNativeRoleKnown:true,_localQuickNativeMemberID:p.id,
  _localQuickNativeReady:false,_localQuickNativeHost:false,isReady:false,team:0});
 const localMember=peers[asHost?0:1];
 localPlayerID=localMember.id;
 const room={name:asHost?'p69c7b0123456789':'中文主持人',_localQuickSharedLineage:true,
  forces:{force0:[peers[0]],force1:[peers[1]]}};
 const calls=[];
 const owner={_roomObj:room,_roomMembers:peers,
  getRoomMemberObjByID:id=>peers.find(p=>p.id===id),broadcast:()=>{},
  getSyncDP(key,args) {
   assert.strictEqual(this,owner,'inherited dispatch must retain the controller receiver');
   calls.push(key);
   if(key===SyncDataID.IS_ROOM_HOST) {assert.strictEqual(args,undefined);return asHost;}
   assert.strictEqual(key,SyncDataID.IS_ME);
   assert(Array.isArray(args) && args.length===1 && peers.includes(args[0]),'dispatch must receive the actual member object');
   return args[0]===localMember;
  }};
 for(const host of hosts) {
  const picked=host.call(owner,room);assert.strictEqual(picked.count,1);assert.strictEqual(picked.host,peers[0]);
 }
 for(const index of [0,1]) {callback.call(owner,peers[index].id,false);callback.call(owner,peers[index].id,true);}
 for(const gate of gates) {
  assert.strictEqual(gate.call(owner,room,2),true);
  assert.strictEqual(gate.call(owner,room,3),false,'two humans cannot satisfy a three-human gate');
 }
 assert(calls.includes(SyncDataID.IS_ROOM_HOST) && calls.includes(SyncDataID.IS_ME));
 peers[1]._localQuickNativeMemberID='stale';
 for(const gate of gates) assert.strictEqual(gate.call(owner,room,2),false);
 if(asHost) {
  localPlayerID=peers[1].id;
  for(const host of hosts) assert.strictEqual(host.call(owner,room).count,0,'wrong local identity cannot replace the host');
 }
}
console.log(JSON.stringify({passed:true,receiver_and_member_arguments:true,host_and_guest:true}));
'''.replace('PARTS', json.dumps(value))
        result = subprocess.run([shutil.which('node'), '-e', script], capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertTrue(json.loads(result.stdout)['passed'])

    def test_native_clan_format_and_identity_cycle_guards_actual_AS(self):
        value = parts((AS/'MatchController.as').read_text(), (AS/'LobbyController.as').read_text())
        result = execute(value)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertTrue(json.loads(result.stdout)['passed'])

    def test_each_original_display_name_selector_fails_causal_fixture(self):
        match=(AS/'MatchController.as').read_text();lobby=(AS/'LobbyController.as').read_text()
        value=parts(match,lobby)
        expression=re.compile(r'typeof (\w+)\.realName == "string" && \1\.realName.length > 0 && \1\.realName == String\(([^)]+)\)')
        seen=0
        for group in ('hosts','gates','callback'):
            texts=value[group] if isinstance(value[group],list) else [value[group]]
            for index,text in enumerate(texts):
                for hit in list(expression.finditer(text)):
                    old='String('+hit[1]+'.name) == String('+hit[2]+')'
                    mutated={k:list(v) if isinstance(v,list) else v for k,v in value.items()}
                    replacement=text[:hit.start()]+old+text[hit.end():]
                    if isinstance(value[group],list):mutated[group][index]=replacement
                    else:mutated[group]=replacement
                    with self.subTest(group=group,index=index,member=hit[1]):
                        result=execute(mutated)
                        self.assertNotEqual(result.returncode,0,'original display-name contour must fail tagged-host fixture')
                        self.assertIn('AssertionError',result.stderr)
                    seen+=1
        self.assertEqual(seen,6,'all six independent host-selector sites need the canonical-name fix')

    def test_each_local_native_UID_selector_is_required_for_private_title(self):
        value=parts((AS/'MatchController.as').read_text(),(AS/'LobbyController.as').read_text())
        expression=re.compile(r'String\((\w+)\.id\) == (acceptedLocalOwner|gateLocalOwner|canonicalLocalOwner)')
        seen=0
        for group in ('hosts','gates','callback'):
            texts=value[group] if isinstance(value[group],list) else [value[group]]
            for index,text in enumerate(texts):
                for hit in list(expression.finditer(text)):
                    changed={key:list(part) if isinstance(part,list) else part for key,part in value.items()}
                    replacement=text[:hit.start()]+'false'+text[hit.end():]
                    if isinstance(value[group],list):changed[group][index]=replacement
                    else:changed[group]=replacement
                    with self.subTest(group=group,index=index,member=hit[1]):
                        result=execute(changed)
                        self.assertNotEqual(result.returncode,0,'every local UID contour must fail the private-host fixture when removed')
                        self.assertIn('AssertionError',result.stderr)
                    seen+=1
        self.assertEqual(seen,6)


if __name__=='__main__': unittest.main()
