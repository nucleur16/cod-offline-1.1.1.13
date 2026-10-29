"""Extract stock room rectangles from an FFDec -swf2xml export for layout regression tests."""
import xml.etree.ElementTree as E, functools,json
import argparse,hashlib
from pathlib import Path
parser=argparse.ArgumentParser(__doc__)
parser.add_argument('--xml',required=True)
parser.add_argument('--swf',required=True)
parser.add_argument('--out',required=True)
parser.add_argument('--game',help='also extract caption metrics from stock and patched English fonts')
args=parser.parse_args()
root=E.parse(args.xml).getroot();tags=root.find('tags')
defs={}
for t in tags:
 if t.get('type')=='DefineScalingGridTag': continue
 for k in ('spriteId','shapeId','characterID','characterId','buttonId','textId'):
  if t.get(k): defs[int(t.get(k))]=t;break
sym=next(t for t in tags if t.get('type')=='SymbolClassTag')
symbols=dict(zip([n.text for n in sym.find('names')],[int(n.text) for n in sym.find('tags')]))
def union(bs):
 bs=[b for b in bs if b]
 return (min(b[0] for b in bs),min(b[1] for b in bs),max(b[2] for b in bs),max(b[3] for b in bs)) if bs else None
def matrix(t):
 m=t.find('matrix')
 if m is None:return (1,0,0,1,0,0)
 a=float(m.get('scaleX')) if m.get('hasScale')=='true' else 1
 d=float(m.get('scaleY')) if m.get('hasScale')=='true' else 1
 b=float(m.get('rotateSkew0')) if m.get('hasRotate')=='true' else 0
 c=float(m.get('rotateSkew1')) if m.get('hasRotate')=='true' else 0
 return a,b,c,d,float(m.get('translateX',0))/20,float(m.get('translateY',0))/20
def transform(b,m):
 if not b:return None
 a,sk0,sk1,d,tx,ty=m
 p=[(a*x+sk1*y+tx,sk0*x+d*y+ty) for x,y in [(b[0],b[1]),(b[0],b[3]),(b[2],b[1]),(b[2],b[3])]]
 return (min(x for x,y in p),min(y for x,y in p),max(x for x,y in p),max(y for x,y in p))
def char(t):return int(t.get('characterId','0')) or symbols.get(t.get('className',''),0)
@functools.lru_cache(None)
def bounds(i):
 t=defs.get(i)
 if t is None:return None
 for k in ('shapeBounds','bounds','textBounds'):
  b=t.find(k)
  if b is not None:return tuple(float(b.get(k))/20 for k in ('Xmin','Ymin','Xmax','Ymax'))
 ts=t.find('subTags')
 if ts is None:return None
 return union([transform(bounds(char(x)),matrix(x)) for x in ts if x.get('type','').startswith('PlaceObject') and char(x)!=i])

hidden={'myChatTitleTF','myChatBg','myScrollBar','myHistoryTF','myInputTF','mySendBtn'}
fixture={'source':'patch_ui_mp:scaleform/default/codo.gfx (1.1.1.13)',
         'swf_sha256':hashlib.sha256(Path(args.swf).read_bytes()).hexdigest(),
         'stage':[0,0,float(root.find('displayRect').get('Xmax'))/20,float(root.find('displayRect').get('Ymax'))/20],
         'rooms':{}}
for name in ['inRoomView','inRoomView_cyborg']:
 items={t.get('name'):t for t in defs[symbols[name]].find('subTags') if t.get('name')}
 positions={n:list(matrix(t)[4:]) for n,t in items.items()}
 rects={n:list(transform(bounds(char(t)),matrix(t))) for n,t in items.items() if bounds(char(t))}
 input=rects['myInputTF'];title=rects['myChatTitleTF']
 controls={n:b for n,b in rects.items() if n not in hidden and not n.startswith('my3D')}
 fixture['rooms'][name]={'sprite_id':symbols[name],
    'positions':positions,'input_bounds':input,
    'reserved_hidden_chat_slot':[input[0],title[1],input[2],input[3]],
    'visible_controls':controls,
    'external_image_instances_without_local_bounds':[n for n in items if n not in rects]}
# ChattingMainGroupView: POS_X_OPEN=GameInfo.PANEL_GAP=50,
# POS_Y_ORIGIN=876 (independently exported stock AS constants).
chat=bounds(symbols['chattingMainGroupView'])
fixture['global_rooms_chat']=[chat[0]+50,chat[1]+876,chat[2]+50,chat[3]+876]
fixture['global_rooms_chat_position_source']={
 'class':'codo.views.ChattingMainGroupView','x':'GameInfo.PANEL_GAP = 50',
 'y':'POS_Y_ORIGIN = 876','child_symbol':'chattingMainGroupView'}
start=bounds(symbols['bottomStartGameMPView'])
fixture['global_start_ready']={str(y):[start[0]+1000,start[1]+y,start[2]+1000,start[3]+y] for y in [16,878]}
if args.game:
 import sys
 sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
 from zonekit import stock,gfx,font3
 game=Path(args.game)
 source=stock.ZoneSource(str(game/'C1.exe'),[str(game/'client_shipRetail/chinese')])
 native=source.rawfile('patch_code_post_gfx_mp:scaleform/fonts_ingame.gfx')
 english=gfx.build_patch(str(Path(__file__).resolve().parents[1]/'zones/codol_english_font/rawfile/scaleform/fonts_ingame.gfx.gfxpatch'),native)
 metrics=[]
 for locale,movie in [('stock',native),('english',english)]:
  for tag,a,b,name in gfx.tags(movie):
   if tag!=75: continue
   f=font3.Font3.parse(movie[a+(6 if movie[a]&63==63 else 2):b])
   kern={(a,b):v for a,b,v in f.kern}
   widths={}
   for caption in ['LOCAL HOSTING: DISABLED','LOCAL HOSTING: ENABLED']:
    codes=[ord(c) for c in caption]
    advance=sum(f.adv[f.codes.index(c)] for c in codes)
    advance+=sum(kern.get(pair,0) for pair in zip(codes,codes[1:]))
    widths[caption]=round(advance*14/font3.EM,4)
   metrics.append({'locale':locale,'font':f.display_name,'width_at_14px':widths,
                   'line_height_at_14px':round((f.ascent+abs(f.descent)+f.leading)*14/font3.EM,4)})
 fixture['caption_font_metrics']=metrics
Path(args.out).parent.mkdir(parents=True,exist_ok=True)
Path(args.out).write_text(json.dumps(fixture,indent=2)+'\n',encoding='utf-8')
print('Extracted both room timelines and global Start/Ready geometry from stock SWF XML')
