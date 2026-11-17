"""Author the two Survival kiosks for C1's native interactive menu path.
The inputs are the converted original CoDOL menus, not replacement gameplay: the
.menu sources of zones/survival_menus (2.6.9.7's kiosks converted by zonekit/menus.py).
The output is menufile/<name>.menu, which the zone build compiles.
Balance and prices read authoritative server-published client dvars; purchase
responses still flow to the Survival GSC credit/purchase validation.
"""
from pathlib import Path
import copy
import json
import struct
import sys
import re
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from zonekit import menus as codec
from zonekit import menu_source
from zonekit.codecs import encode_localize
ROOT=Path(__file__).resolve().parents[1]
ZONE=ROOT/'zones/patch_common_gametype_survival'
SOURCES=ROOT/'zones/survival_menus/menufile'
def asset(name):return 'ui_mp/scriptmenus/'+name+'.menu'
def original(name):
    """The converted original kiosk (survival_menus' .menu source)."""
    return menu_source.load(SOURCES/(name+'.menu'),asset(name))
def header(name):
    return [asset(name)+': written by tools/build_survival_kiosks.py from zones/survival_menus/menufile/'+name+'.menu',
            '(edit the generator or that source, not this file). IW menu language: zonekit/menu_source.py.']

def op(name):
    return {'kind':'expressionEntry','raw':struct.pack('<III',0,codec.OP_1113[name],0).hex(),'p':{}}
def text(value):
    return {'kind':'expressionEntry','raw':struct.pack('<III',1,2,0).hex(),'p':{'8':value}}
def integer(value):
    return {'kind':'expressionEntry','raw':struct.pack('<IIi',1,0,value).hex(),'p':{}}
def statement(entries):
    return {'kind':'Statement','raw':struct.pack('<IIIII',len(entries),0,0,0,0).hex(),'p':{'4':entries}}
def dvar(name):
    return [op('('),op('dvarint'),text(name),op(')')]
def traverse(node):
    if isinstance(node,dict):
        yield node
        for value in node.get('p',{}).values():yield from traverse(value)
    elif isinstance(node,list):
        for value in node:yield from traverse(value)

def author(original):
    node=copy.deepcopy(original)
    menu=node['p']['8'][0];items=menu['p']['284']
    # Preserve the kiosk title; a separate readable line sits immediately above it.
    balance=copy.deepcopy(items[5]);balance['p']['0']='survival_credit_balance'
    balance['p']['332']=statement([op('('),op('locstring'),text('@SO_SURVIVAL_CREDITS'),op(')'),op('+'),text(': '),op('+'),op('('),op('string')]+dvar('surHUD_credits')+[op(')')])
    # Existing title's y=4 expression -> balance y=-18; same centered panel, width/font.
    for floating in balance['p'].get('320',[]):
        if struct.unpack_from('<i',bytes.fromhex(floating['raw']))[0]==1:
            floating['p']['4']=statement([integer(-18)])
    raw=bytearray.fromhex(balance['raw']);struct.pack_into('<f',raw,0xd4,0.30);balance['raw']=raw.hex()
    items.append(balance);raw=bytearray.fromhex(menu['raw']);struct.pack_into('<I',raw,0xb0,len(items));menu['raw']=raw.hex()
    rows=[0,1,2,3,4] if menu['p']['0']=='kiosk_locker' else [10,11,13,14]
    price_rewrites=0
    for item in items[:-1]:
        exp=item.get('p',{}).get('332')
        if not exp:continue
        strings=[v.get('p',{}).get('8') for v in traverse(exp) if v.get('kind')=='expressionEntry']
        if '@SO_SURVIVAL_NUM_CARDS' not in strings:continue
        entries=exp['p']['4'];index=None
        # Converted original price: locstring @... , int tablelookup table , 0 , row , 3.
        for i,e in enumerate(entries):
            if e['kind']=='expressionEntry' and struct.unpack_from('<II',bytes.fromhex(e['raw']))==(0,codec.OP_1113['tablelookup']):
                numbers=[struct.unpack('<IIi',bytes.fromhex(x['raw']))[2] for x in entries[i+1:] if struct.unpack_from('<II',bytes.fromhex(x['raw']))==(1,0)]
                if len(numbers)>=3:index=numbers[1]
        if index not in rows:raise ValueError('Unexpected kiosk price expression')
        item['p']['332']=statement([op('('),op('string')]+dvar('surHUD_cost_'+str(index))+[op(')')])
        price_rewrites+=1
    if price_rewrites!=len(rows)*2:raise ValueError('Expected enabled/disabled prices for every kiosk row')
    # This category patch loads before common_gametype_survival; its highlight must
    # use a material already provided by common_mp instead of a forward reference.
    for v in traverse(node):
        if v.get('asset')=='material' and v.get('name')==',navbar_selection_bar_focus':v['name']=',gradient_fadein'
    return node

def main():
    required={'SO_SURVIVAL_CREDITS','SO_SURVIVAL_ENEMIES_LEFT','SO_SURVIVAL_ENEMY_LEFT'}
    for name in ('kiosk_locker','kiosk_killstreak'):
        node=author(original(name))
        required.update(re.findall(r'@([A-Z][A-Z0-9_]+)', json.dumps(node)))
        (ZONE/'menufile'/(name+'.menu')).write_text(menu_source.decompile_menufile(node,header(name)),newline='\n')
        print(name+': original controls preserved; live credit balance and authoritative row prices encoded')
    translations=dict(line.split('=',1) for line in (ZONE/'sources/english.txt').read_text().splitlines() if '=' in line)
    locale=ZONE/'localize';locale.mkdir(exist_ok=True)
    manifest=ROOT/'zone_source/patch_common_gametype_survival.csv'
    text_manifest='\n'.join(line for line in manifest.read_text().splitlines() if not line.startswith('localize,'))+'\n'
    for key in sorted(required & translations.keys()):
        (locale/(key+'.bin')).write_bytes(encode_localize(key,translations[key].encode('ascii')))
        text_manifest+='localize,'+key+',file=localize/'+key+'.bin\n'
    manifest.write_text(text_manifest,newline='\n')
    print('Original English localization dependencies: '+', '.join(sorted(required & translations.keys())))
if __name__=='__main__':main()
