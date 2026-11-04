"""Package the authoritative Survival CSV overrides beside its native kiosks.

Mounted StringTables precede loose rawfile fallbacks in C1/sh1x. Updating only
the server CSV therefore cannot replace an already mounted authored wave table.
"""
from pathlib import Path
import argparse,csv,io
ROOT=Path(__file__).resolve().parents[1]
MAPS=('abandon_sh','ambush_sh','crash_tropical','cross_fire','fav_tropical',
      'hardhat_sh','highrise','seatown','subbase_sh','terminal_cls','tier_1')
def sync(rawfiles):
    target=ROOT/'zones/patch_common_gametype_survival/stringtable/mp'
    target.mkdir(parents=True,exist_ok=True)
    manifest=ROOT/'zone_source/patch_common_gametype_survival.csv'
    lines=[l for l in manifest.read_text().splitlines() if not l.startswith('stringtable,mp/survival_waves_')]
    for name in MAPS:
        filename='survival_waves_'+name+'.csv'
        data=(rawfiles/'mp'/filename).read_text()
        rows=list(csv.reader(io.StringIO(data)))
        if len(rows)!=20 or [int(r[2]) for r in rows]!=list(range(1,21)):
            raise ValueError('not the complete authored twenty-wave table: '+filename)
        columns=max(map(len,rows))
        if any(len(r)!=columns for r in rows):raise ValueError('ragged wave table: '+filename)
        (target/filename).write_text(data,encoding='utf8',newline='\n')
        lines.append('stringtable,mp/'+filename+',columns='+str(columns))
    manifest.write_text('\n'.join(lines)+'\n',encoding='utf8',newline='\n')
    print('Packaged '+str(len(MAPS))+' Survival StringTable overrides; existing kiosk assets retained')
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--rawfiles',required=True,type=Path)
    sync(p.parse_args().rawfiles)
