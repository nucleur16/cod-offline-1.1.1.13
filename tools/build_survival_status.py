"""Port selected authored CoDOL2.6.9.7 scorebar items into C1's menu format.
Root build_survival_hud.py appends these to C1's existing scorebar_hd; this
component is not an independent runtime menu. Unknown later stat0 is explicitly
adapted to the real current Survival wallet, not claimed semantically identical.
"""
from pathlib import Path
import copy,json,struct,sys
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from zonekit import menus
ROOT=Path(__file__).resolve().parents[1]
SOURCE=ROOT/'zones/survival_hud_postgfx_mp/sources/survival_scorebar_2697.json'
OUT=ROOT/'zones/survival_hud_postgfx_mp/components'
def op(name):return {'kind':'expressionEntry','raw':struct.pack('<III',0,menus.OP_1113[name],0).hex(),'p':{}}
def string(value):return {'kind':'expressionEntry','raw':struct.pack('<III',1,2,0).hex(),'p':{'8':value}}
def stmt(es):return {'kind':'Statement','raw':struct.pack('<IIIII',len(es),0,0,0,0).hex(),'p':{'4':es}}
def dvar(name):return [op('('),op('dvarint'),string(name),op(')')]
def survival():return [op('('),op('dvarstring'),string('g_gametype'),op(')'),op('=='),string('survival')]
class Adapter(menus.Converter):
 def statement(self,v):
  v=copy.deepcopy(v)
  for e in v['p'].get('4',[]):
   r=bytes.fromhex(e['raw']);kind,number=struct.unpack_from('<II',r)
   if kind==0 and number==menus.OPS_2697.index('issurvival'):
    # Converter recursively remaps a function operand's 2.6 expression.
    oldops={n:i for i,n in enumerate(menus.OPS_2697)}
    es=survival()
    for x in es:
     raw=bytearray.fromhex(x['raw']);t,a=struct.unpack_from('<II',raw)
     if t==0:struct.pack_into('<I',raw,4,oldops[menus.OPS_1113[a]])
     x['raw']=raw.hex()
    e.update(kind='expressionEntry',raw=struct.pack('<III',1,3,0).hex(),p={'8':stmt(es)})
  return super().statement(v)
def author(source):
 node=copy.deepcopy(source);item=node['p']['8'][0]['p']['288'][3]
 # These three expressions use unproven later HUD-stat operators240/242/246.
 # Remove only those expressions before the strict standard converter runs.
 item['p'].pop('328');item['p'].pop('336');item['p'].pop('324')
 raw=bytearray.fromhex(item['raw']);struct.pack_into('<I',raw,0x140,0);item['raw']=raw.hex()
 node['p']['8'][0]['p'].pop('232',None)
 result=Adapter().menulist(node);menu=result['p']['8'][0];items=menu['p']['284']
 result['p']['0']='ui_mp/scriptmenus/survival_status_component.menu';menu['p']['0']='survival_status_component'
 # C1's stock renderer supplies white; the later wavedivideline material is
 # absent from the installed C1/common Survival zones. Preserve its1px rect.
 items[0]['p']['160']={'asset':'material','name':',white'}
 wallet=items[3];raw=bytearray.fromhex(wallet['raw'])
 # The numeric template used0.63 scale, too tall for its22-unit gap below
 # the0.24-scale enemy label. Give the wallet its own equally sized text row:
 #28 baseline units below the unchanged enemy row, within the bottom anchor.
 struct.pack_into('<f',raw,8,-30.0)
 struct.pack_into('<f',raw,menus.W13+0x30,0.24)
 wallet['raw']=raw.hex()
 wallet['p']['324']=stmt(survival())
 wallet['p']['332']=stmt([op('('),op('locstring'),string('@SO_SURVIVAL_CREDITS'),op(')'),op('+'),string(': '),op('+'),op('('),op('string')]+dvar('surHUD_credits')+[op(')')])
 menu['p']['228']=stmt(survival()+[op('&&'),op('!'),op('ui_active'),op('&&'),op('!'),op('spectatingfree'),op('&&'),op('!'),op('spectatingclient')])
 # Embedded items also require the full global context because only items,
 # not this component menuDef, are inserted into C1's existing scorebar.
 for i in items:
  old=i['p']['324']['p']['4'];i['p']['324']=stmt([op('(')]+menu['p']['228']['p']['4']+[op(')'),op('&&'),op('(')]+old+[op(')')])
 return result

def main():
 OUT.mkdir(exist_ok=True);result=author(json.loads(SOURCE.read_text()))
 (OUT/'survival_status.json').write_text(json.dumps(result,indent=1)+'\n',newline='\n')
 (OUT/'survival_status.txt').write_text(menus.listing(result),newline='\n')
 (OUT/'survival_status.bin').write_bytes(menus.encode_menulist(result))
 print('4 authored items converted; only strict C1 operators, live wallet adaptation, original enemy geometry')
if __name__=='__main__':main()
