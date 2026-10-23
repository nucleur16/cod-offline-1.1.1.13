"""Execute the actual bounded AS gate/Ready observer bodies with JS syntax lowering.

Native callbacks and elapsed time are fixture inputs; this is not C1 acceptance.
No duplicate policy implementation is used. FFDec compilation is checked separately.
"""
from pathlib import Path
import json
import re
import shutil
import subprocess
import unittest

ROOT = Path(__file__).resolve().parents[1]
AS = ROOT / "zones/voice_ui_mp/rawfile/scaleform/default/codo.gfx.gfxpatch/as/codo/controllers"


def body(source, name):
    start = source.index("function " + name + "(")
    begin = source.index("{", start)
    depth = 1
    end = begin + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[begin + 1:end - 1]


def javascript(text):
    text = re.sub(r"var (\w+):(Object|Boolean|String|Array|int|uint|Number)", r"var \1", text)
    return re.sub(r"for each\(var (\w+) in ([^)]+)\)", r"for (var \1 of \2)", text)


class QuickReadyGate(unittest.TestCase):
    def test_real_as_bodies_current_cycle_and_stale_native_states(self):
        node = shutil.which("node")
        self.assertIsNotNone(node, "Node is required to execute the actual AS method bodies")
        match = (AS / "MatchController.as").read_text()
        lobby = (AS / "LobbyController.as").read_text()
        observer = javascript(lobby.split("// BEGIN shared Quick native Ready cycle evidence",1)[1].split("// END shared Quick native Ready cycle evidence",1)[0])
        gate = javascript(match.split("// BEGIN shared Quick all-current-member gate",1)[1].split("// END shared Quick all-current-member gate",1)[0]) + "\nreturn quickMembersReady;"
        timer_source = body(match, "onRankedMatchPassedTimeTimerHandle")
        start = timer_source.index('if(quickFilter["_localQuickVoteFinal"] == true && quickFilter["_localQuickSharedBookingStarted"] != true && this.getSyncDP(SyncDataID.IS_ROOM_HOST) && quickMembersReady)')
        begin = timer_source.index("{", start)
        end, depth = begin + 1, 1
        while depth:
            depth += (timer_source[end] == "{") - (timer_source[end] == "}")
            end += 1
        actual_start = javascript(gate.rsplit("return quickMembersReady;", 1)[0] + timer_source[start:end])
        script = """
const assert = require('assert');
const uint = value => Number(value) >>> 0;
const SyncDataID = {IS_ME:'me'};
const CFunctionContainer = {IsRankedMatch:()=>ranked, IsRoomHost:()=>localHost};
let ranked=true, localHost=true;
const RankedMatchStatus={READY_IN_ROOM:'ready'};
const observe = new Function('member','ready','roomMemberObj', OBSERVER);
const allReady = new Function('quickRoom','queuedRequired', GATE);
function fresh() {
 const peers=[{id:'1',name:'host'},{id:'2',name:'guest'}];
 for (const p of peers) Object.assign(p,{_localQuickNativeRoleKnown:true,
 _localQuickNativeMemberID:p.id,_localQuickNativeReady:true});
 const room={name:'host',_localQuickSharedLineage:true,forces:{force0:[peers[0]],force1:[peers[1]]}};
 const owner={_roomObj:room,_roomMembers:peers,getSyncDP:(_key,args)=>args[0].id==='1'};
 const event=(i,value)=>{observe.call(owner,peers[i].id,value,peers[i]);peers[i]._localQuickNativeReady=value;};
 return {peers,room,owner,event,ready:()=>allReady(room,2)};
}
let f=fresh(); assert(!f.ready(),'prior-match cached true is not current results ACK');
f.event(0,false); f.event(0,true);
assert(!f.ready(),'host alone must not start the native query');
for(let elapsed=0;elapsed<240;elapsed++) assert(!f.ready(),'human results wait has no native task clock');
f.event(1,false); f.event(1,true); assert(f.ready(),'genuine guest ACK completes current cycle');
f.event(1,false); assert(!f.ready(),'genuine unready cancels eligibility');
f.event(1,true); assert(f.ready());
f.event(0,false); f.event(0,true); assert(!f.ready(),'new host false invalidates previous guest true');
f.event(1,true); assert(f.ready(),'real current true event restores eligibility');
f=fresh(); f.event(1,false);f.event(1,true);f.event(0,false);f.event(0,true);
assert(!f.ready(),'early guest local state cannot masquerade as accepted current host cycle');
f.event(1,true);assert(f.ready(),'existing post-vote genuine guest Ready completes early-ACK path');
f.peers[1]._localQuickNativeRoleKnown=false;assert(!f.ready(),'unknown native identity');
f.peers[1]._localQuickNativeRoleKnown=true;assert(f.ready());
f.room.forces.force1.push(f.peers[1]);assert(!f.ready(),'duplicate native member');f.room.forces.force1.pop();
f.room.forces.force1.push({id:'3',name:'new',_localQuickNativeRoleKnown:true,_localQuickNativeMemberID:'3',_localQuickNativeReady:true});
assert(!f.ready(),'new member cached true cannot join current ready cohort');f.room.forces.force1.pop();
f.room.name='guest';assert(!f.ready(),'host swap invalidates frozen host epoch');f.room.name='host';
f.room._localQuickSharedLineage=false;const saved=JSON.stringify(f.room);
observe.call(f.owner,f.peers[0].id,false,f.peers[0]);assert.strictEqual(JSON.stringify(f.room),saved,'manual room observer is inactive');assert(!f.ready());
f=fresh();ranked=false;f.event(0,false);assert(!f.room._localQuickReadyCycle,'unranked manual room untouched');
ranked=true;f=fresh();f.event(0,false);f.event(0,true);
const timerStep=new Function('quickRoom','queuedRequired','quickFilter', TIMER);
const filter={_localQuickVoteFinal:true,_localQuickSharedBookingStarted:false};
const calls={starts:0,getSyncDP:()=>true,stopRankedMatchPassedTimeTimer:()=>{},stopRankedMatchGoRoomTimer:()=>{},stopRankedMatchGoGameTimer:()=>{},changeRankedMatchStatus:()=>{},startMatchCountDown(){this.starts++;}};
for(let elapsed=0;elapsed<240;elapsed++) timerStep.call(calls,f.room,2,filter);
assert.strictEqual(calls.starts,0,'actual timer path cannot start query during delayed human results');
f.event(1,false);f.event(1,true);timerStep.call(calls,f.room,2,filter);
assert.strictEqual(calls.starts,1,'actual current member ACK starts existing native countdown');
for(let elapsed=0;elapsed<120;elapsed++) timerStep.call(calls,f.room,2,filter);
assert.strictEqual(calls.starts,1,'actual booking flag keeps native query one-shot');
console.log(JSON.stringify({passed:true,contours:14,actual_as_method_bodies:true,native_c1_acceptance:false}));
""".replace("OBSERVER", json.dumps(observer)).replace("GATE", json.dumps(gate)).replace("TIMER", json.dumps(actual_start))
        result = subprocess.run([node, "-e", script], text=True, capture_output=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertTrue(json.loads(result.stdout)["passed"])

    def test_actual_callers_gate_before_stopping_wait_or_starting_native_countdown(self):
        source = (AS / "MatchController.as").read_text()
        ready = body(source, "bind_quickMatchReady")
        guard = ready.index("if(!quickMembersReady)")
        final = ready.index('if(quickFilter["_localQuickVoteFinal"] != true)')
        countdown = ready.index("this.startMatchCountDown();", guard)
        self.assertLess(final, guard)
        self.assertLess(guard, ready.index("this.stopRankedMatchPassedTimeTimer();", guard))
        self.assertLess(guard, countdown)
        self.assertIn("this.startRankedMatchPassedTimeTimer();", ready[guard:ready.index("this.stopRankedMatchPassedTimeTimer();", guard)])
        timer = body(source, "onRankedMatchPassedTimeTimerHandle")
        guard = timer.index("&& quickMembersReady)")
        self.assertLess(guard, timer.index("this.startMatchCountDown();", guard))
        lobby = (AS / "LobbyController.as").read_text()
        event = body(lobby, "cppSetMemberReadyState")
        self.assertIn("// BEGIN shared Quick native Ready cycle evidence", event)
        snapshots = body(lobby, "initRoomMembers")
        self.assertEqual(snapshots.count('["_localQuickReadyCycle"] ='),1)
        self.assertIn('["_localQuickReadyCycle"] = 0;', snapshots)
        self.assertNotIn("SetReadyState", lobby.split("// BEGIN shared Quick native Ready cycle evidence",1)[1].split("// END shared Quick native Ready cycle evidence",1)[0])
        self.assertNotIn("SetReadyState", source.split("// BEGIN shared Quick all-current-member gate",1)[1].split("// END shared Quick all-current-member gate",1)[0])

if __name__ == "__main__":
    unittest.main()
