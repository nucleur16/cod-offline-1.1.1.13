"""Execute the actual display-only AS body; no C1 process or screen use."""
from pathlib import Path
import json
import re
import shutil
import subprocess
import unittest

ROOT=Path(__file__).resolve().parents[1]
SOURCE=ROOT/'zones/voice_ui_mp/rawfile/scaleform/default/codo.gfx.gfxpatch/as/components/room_part/RoomInfo.as'

def run(actual):
    script=r'''
const assert=require('assert');
const render=new Function('cDat',BODY);
let checks=0;
function fresh(name='Display Host') {
 const host={id:'14124485489710931920',realName:name,name:'[clan] '+name,
  _localQuickNativeRoleKnown:true,_localQuickNativeMemberID:'14124485489710931920',_localQuickNativeHost:true};
 const guest={id:'4936378376416122282',realName:name,name:name,
  _localQuickNativeRoleKnown:true,_localQuickNativeMemberID:'4936378376416122282',_localQuickNativeHost:false};
 return {host,guest,data:{name:'p69c7b0123456789',forces:{force0:[host],force1:[guest]}}};
}
function check(f,expected) {
 const before=JSON.stringify(f.data),view={myRoomNameTF:{text:''}};
 render.call(view,f.data);
 assert.strictEqual(view.myRoomNameTF.text,expected);
 assert.strictEqual(JSON.stringify(f.data),before,'rendering cannot alter native title/UID/Ready/owner data');
 checks++;
}
for(const name of ['Display Host','中文玩家','<b>literal</b>']) {
 let f=fresh(name);check(f,name);
 f.host.name='unrelated clan formatting';check(f,name);
 f.guest.realName='p69c7b0123456789';check(f,name);
 f.host._localQuickNativeMemberID='stale';check(f,f.data.name);
 f=fresh(name);f.host._localQuickNativeRoleKnown=false;check(f,f.data.name);
 f=fresh(name);f.host._localQuickNativeHost=false;check(f,f.data.name);
 f=fresh(name);f.guest._localQuickNativeHost=true;check(f,f.data.name);
 f=fresh(name);f.data.forces.force1.push(f.host);check(f,f.data.name);
 for(const missing of [undefined,null,'',5]) {
  f=fresh(name);f.host.realName=missing;check(f,f.data.name);
 }
 f=fresh(name);f.data.forces=null;check(f,f.data.name);
 f=fresh(name);f.data.forces.force1=null;check(f,f.data.name);
 f=fresh(name);f.data.forces.isHost=true;f.host.isMe=true;
 f.host._localQuickNativeHost=false;check(f,name);
 f.guest.isMe=true;check(f,f.data.name);
 f=fresh(name);f.data.forces.isHost=true;f.host.isMe=false;check(f,f.data.name);
}
console.log(JSON.stringify({passed:true,checks,actual_AS_body:true,C1_acceptance:false}));
'''.replace('BODY',json.dumps(actual))
    return subprocess.run([shutil.which('node') or 'node','-e',script],text=True,capture_output=True)

class RoomDisplayOwner(unittest.TestCase):
    def test_actual_owner_display_and_fallback_preserve_native_data(self):
        source=SOURCE.read_text()
        actual=source.split('// BEGIN room display owner name',1)[1].split('// END room display owner name',1)[0]
        actual=re.sub(r'var (\w+):(String|Object|int|Array)',r'var \1',actual)
        actual=re.sub(r'for each\(var (\w+) in ([^)]+)\)',r'for (var \1 of \2)',actual)
        result=run(actual)
        self.assertEqual(result.returncode,0,result.stdout+result.stderr)
        self.assertEqual(json.loads(result.stdout)['checks'],51)

    def test_old_raw_title_fails_private_identity_display_fixture(self):
        result=run('this.myRoomNameTF.text=cDat.name;')
        self.assertNotEqual(result.returncode,0)
        self.assertIn('AssertionError',result.stderr)

if __name__=='__main__':unittest.main()
