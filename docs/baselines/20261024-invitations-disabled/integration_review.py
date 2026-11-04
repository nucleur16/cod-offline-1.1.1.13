"""Independent resolved-ABC evidence for the production-main integration.

The historical Survival review stays frozen. This next layer allows exactly
the three intentional production-main method changes, keeping every other
resolved source line and all method/trait/class/script tables identical.
"""
from pathlib import Path
import hashlib,json,zlib,subprocess,shutil
from behavior_fixture import method

QUICK=' || choice.name == "Quick Match" && choice.type == PlayListType.RANKED && choice.minPartySize == 1 && (choice.maxPartySize == 1 || choice.maxPartySize == 6) && choice.maxRoomSize == 12'
OLD='''         if(playListObj != null && playListObj.type == PlayListType.RANKED)'''
NEW='''         if(playListObj != null && playListObj.type == PlayListType.COMBAT && (playListObj.name == "Boot Camp" || playListObj.name == "Bot Training"))
         {
            factor = playListObj.name == "Boot Camp" ? 1 : 0.5;
         }
         else if(playListObj != null && playListObj.type == PlayListType.RANKED)'''

def resolved_changes(before,after):
    if before.count(QUICK)!=1 or QUICK in after:
        raise ValueError('reviewed Quick visibility change missing')
    if before.count(OLD)!=2 or after.count(NEW)!=2:
        raise ValueError('reviewed training reward branches missing')
    normalized=before.replace(QUICK,'').replace(OLD,NEW)
    if normalized!=after:
        raise ValueError('unreviewed resolved ActionScript change')
    return True

def snapshots(doc):
    review=json.loads((doc/'reviewed_integration_20261029.json').read_text())
    artifacts={name:zlib.decompress((doc/name).read_bytes()) for name in review['artifacts']}
    for name,data in artifacts.items():
        if hashlib.sha256(data).hexdigest()!=review['artifacts'][name]:
            raise ValueError('integration snapshot hash differs: '+name)
    return review,artifacts

def execute(doc):
    _,artifacts=snapshots(doc)
    names=['listPlayListNew','getXPLeagueFactor','getGPLeagueFactor']
    sources={side:artifacts[f'MatchController.{side}-integration-20261029.as.zlib'].decode() for side in ('before','after')}
    resolved_changes(sources['before'],sources['after'])
    methods={side:{n:method(s,n) for n in names} for side,s in sources.items()}
    for side in methods:
        methods[side]['listPlayListNew']['body']=methods[side]['listPlayListNew']['body'].replace('for each(var choice in choices)','for(var choice of choices)')
    script=r'''
const assert=require('assert'),M=METHODS;let calls=[];
const service={getXPLeagueFactor(...a){calls.push(['XP',...a]);return -3;},getGPLeagueFactor(...a){calls.push(['GP',...a]);return -3;}};
const scope={NaN,Number,PlayListType:{RANKED:'ranked',COMBAT:'combat'},CombatType:{CT:'ct'},MatchService:{getInstance:()=>service}};
function run(side,n,view,...args){const m=M[side][n];return new Function('scope',...m.params,'with(scope){'+m.body+'}').call(view,scope,...args);}
let checks=0;
for(const n of ['getXPLeagueFactor','getGPLeagueFactor'])for(const p of [null,{type:'ranked'},{type:'combat',name:'Boot Camp'},{type:'combat',name:'Bot Training'},{type:'combat',name:'Other'}]){
 calls=[];const old=run('before',n,{},0,17,p),oldCalls=calls.slice();calls=[];const actual=run('after',n,{},0,17,p);
 if(p&&p.type==='combat'&&['Boot Camp','Bot Training'].includes(p.name)){
  assert.equal(actual,p.name==='Boot Camp'?1:0.5);assert.deepEqual(calls,[]);
 }else{assert(Object.is(old,actual));assert.deepEqual(calls,oldCalls);}checks+=2;
}
const playlists=[null,{type:'combat',id:0,combatTrainingType:'ct'},{type:'combat',id:2,combatTrainingType:'ct'},{type:'combat',id:1,combatTrainingType:'ct'},{type:'combat',id:0,combatTrainingType:'other'},{name:'Quick Match',type:'ranked',minPartySize:1,maxPartySize:6,maxRoomSize:12}];
const view={listPlayList:(_,type)=>playlists.filter(p=>p===null||p.type===type)};
assert.deepEqual(run('after','listPlayListNew',view),playlists.slice(1,3));checks++;
assert.equal(run('before','listPlayListNew',view).length,3);checks++;
console.log(JSON.stringify({checks}));
'''
    return subprocess.run([shutil.which('node') or 'node','-e',script.replace('METHODS',json.dumps(methods))],text=True,capture_output=True)
