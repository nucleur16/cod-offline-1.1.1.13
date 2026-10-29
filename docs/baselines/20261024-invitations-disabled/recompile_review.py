"""Review evidence for FFDec's 2026-10-28 MatchController recompile.

Resolved exports are tied to exact before/current ABC hashes. Their only
observable change is the reviewed Survival map ID; two Number(NaN)
initializers are equivalent and overwritten on both branches.
"""
from pathlib import Path
import hashlib,json,zlib,subprocess,shutil
from behavior_fixture import method


def resolved_equivalence(before, after):
    old='filter.map = TutorialMap.SURVIVAL;'
    new='filter.map = "coop_survival_mp_fav_tropical";'
    if before.count(old)!=1 or after.count(new)!=1:
        raise ValueError('reviewed Survival default assignment missing')
    if before.count('var factor:Number = NaN;')!=2 or after.count('var factor:Number = Number(NaN);')!=2:
        raise ValueError('reviewed equivalent NaN initializer inventory changed')
    normalized=after.replace(new,old).replace('var factor:Number = Number(NaN);','var factor:Number = NaN;')
    if normalized!=before:
        raise ValueError('unreviewed resolved ActionScript change')
    return True


def snapshots(doc):
    review=json.loads((doc/'reviewed_recompile_20261028.json').read_text())
    artifacts={}
    for name,pin in review['artifacts'].items():
        data=zlib.decompress((doc/name).read_bytes())
        if hashlib.sha256(data).hexdigest()!=pin:
            raise ValueError('review snapshot hash differs: '+name)
        artifacts[name]=data
    return review,artifacts


def execute(doc):
    review,artifacts=snapshots(doc)
    before=artifacts['MatchController.before-20261028.as.zlib'].decode()
    after=artifacts['MatchController.after-20261028.as.zlib'].decode()
    resolved_equivalence(before,after)
    methods={label:{n:method(s,n) for n in ['getXPLeagueFactor','getGPLeagueFactor','gotoSurvivalQuickMatchHandle']}
             for label,s in [('before',before),('after',after)]}
    script=r'''
const assert=require('assert'),M=METHODS; let checks=0;
let returned,calls=[];
const service={getXPLeagueFactor(...args){calls.push(['XP',...args]);return returned;},getGPLeagueFactor(...args){calls.push(['GP',...args]);return returned;}};
const scope={NaN,Number,PlayListType:{RANKED:'ranked'},MatchService:{getInstance:()=>service},
QuickMatchFilterObj:function(){},RoomListType:{COOP:'coop'},TutorialMap:{SURVIVAL:'coop_survival_mp_afghan'},ServerChannel:{CHANNEL_9:9},BCEvent:{EVT_QUICK_MATCH:'quick'}};
function run(label,name,view,...args){let m=M[label][name];return new Function('scope',...m.params,'with(scope){'+m.body+'}').call(view,scope,...args);}
for(const n of ['getXPLeagueFactor','getGPLeagueFactor'])for(const playlist of [null,{type:'ranked'},{type:'other'}])for(const v of [NaN,Infinity,-Infinity,0,-0,1,-3,0.25]){
 returned=v;calls=[];const a=run('before',n,{},0,17,playlist);const ac=calls.slice();calls=[];const b=run('after',n,{},0,17,playlist);
 assert(Object.is(a,b));assert.deepEqual(ac,calls);checks+=2;
}
let outputs={};
for(const label of ['before','after']){
 const seen=[];run(label,'gotoSurvivalQuickMatchHandle',{joinChannel:c=>seen.push(c),broadcast:(e,f)=>seen.push([e,f])},null);outputs[label]=seen;
}
assert.equal(outputs.before[1][1].filter.map,'coop_survival_mp_afghan');
assert.equal(outputs.after[1][1].filter.map,'coop_survival_mp_fav_tropical');
outputs.before[1][1].filter.map=outputs.after[1][1].filter.map;
assert.deepEqual(outputs.before,outputs.after);checks+=3;
console.log(JSON.stringify({checks,only_observable_change:'supported Survival map ID'}));
'''
    script=script.replace('METHODS',json.dumps(methods))
    return subprocess.run([shutil.which('node') or 'node','-e',script],text=True,capture_output=True)
