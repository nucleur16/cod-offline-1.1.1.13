"""Audit each authored Survival level's actual GSC wave selection and shipped table."""
import argparse
import csv
import json
from pathlib import Path
import re
import subprocess
ROOT=Path(__file__).resolve().parents[1]

def function_body(source,name):
    start=source.index('\n'+name+'(');begin=source.index('{',start);depth=1;end=begin+1
    while depth:
        depth+=(source[end]=='{')-(source[end]=='}');end+=1
    return source[begin+1:end-1]

def audit(rawfiles,asset_names=None):
    rawfiles=Path(rawfiles)
    source=(rawfiles/'maps/mp/gametypes/_survival.gsc').read_text()
    levels=(rawfiles/'maps/mp/gametypes/_survival_levels.gsc').read_text()
    maps=sorted(set(re.findall(r'case "(mp_[a-z0-9_]+)":',levels)))
    selector=function_body(source,'wave_table_for_map')
    selected=json.loads(subprocess.check_output(['node','-e','const select=new Function("mapname",'+json.dumps(selector)+'); console.log(JSON.stringify('+json.dumps(maps)+'.map(m=>({map:m,table:select(m)}))));'],text=True))
    results=[]
    for entry in selected:
        table=entry['table'];specific='mp/survival_waves_'+entry['map'][3:]+'.csv'
        expected=specific if (rawfiles/specific).is_file() else 'mp/survival_waves_tier_1.csv'
        if table!=expected:raise ValueError(str(entry)+': differs from supplied authored table/tier')
        if asset_names is not None and table not in asset_names:raise ValueError(table+': absent from actual Survival fastfile')
        with (rawfiles/table).open(newline='',encoding='utf-8') as source_table:
            rows=list(csv.reader(source_table))
        waves=[r for r in rows if len(r)>3 and r[2].isdigit() and int(r[2])>0 and r[3]]
        if not any(r[2]=='1' for r in waves):raise ValueError(table+': no authored first wave')
        results.append(dict(entry,waves=len(waves)))
    # Execute the real precache prefix. Missing-name existence probing must never occur.
    prefix=function_body(source,'precache').split('precacheString(',1)[0]
    prefix=re.sub(r'maps\\mp\\gametypes\\_survival_utility::init\(\);','',prefix)
    subprocess.run(['node','-e','let level={}; let map=""; const getDvar=()=>map; const wave_table_for_map=new Function("mapname",'+json.dumps(selector)+');const tableLookup=()=>{throw new Error("Missing-table probe reached")};const init=new Function('+json.dumps(prefix)+');for(const id of '+json.dumps(maps)+'){map=id;init();if(!level.survival_wave_table)throw new Error(id);}'],check=True)
    return results

def main():
    parser=argparse.ArgumentParser(__doc__)
    parser.add_argument('--rawfiles',type=Path,default=ROOT/'bin/home/codol_rawfiles')
    parser.add_argument('--game',type=Path)
    parser.add_argument('--asset-tool',type=Path)
    parser.add_argument('--out',type=Path)
    args=parser.parse_args()
    assets=None
    if args.game or args.asset_tool:
        if not (args.game and args.asset_tool):parser.error('--game and --asset-tool must be used together')
        text=subprocess.check_output([str(args.asset_tool),'zone-assets','--input',str(args.game/'client_shipRetail/chinese/common_gametype_survival.ff'),'--exe',str(args.game/'C1.exe')],text=True)
        assets={line.split(' ',1)[1] for line in text.splitlines() if line.startswith('stringtable ')}
    results=audit(args.rawfiles,assets)
    if args.out:
        args.out.parent.mkdir(parents=True,exist_ok=True);args.out.write_text(json.dumps(results,indent=2)+'\n')
    for entry in results:print(entry['map']+': '+entry['table']+' ('+str(entry['waves'])+' authored waves)')
    print(str(len(results))+' Survival maps passed; missing-table probes are never executed')
if __name__=='__main__':main()
