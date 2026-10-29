"""Live, read-only CoD menu inspector and genuine Scaleform preview launcher.

Run with a .menu, decoded MenuList JSON, serialized .bin with JSON sidecar,
SWF/GFX, or .gfxpatch directory. No game process is modified.
"""
from __future__ import annotations
import argparse
import concurrent.futures
import hashlib
import json
import os
import re
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from zonekit import gfx, menus

DEFAULT_GAME = Path(r'D:\CoD\CODOL Builds\Call of Duty Online 1.1.1.13')
DEFAULT_PLAYER = Path(r'D:\CoD\Scaleform\GFx SDK 3.3\Bin\Win32\Msvc80\GFxPlayer\FxPlayer_D3D9_Release_Static.exe')
DEFAULT_FFDEC = Path(r'C:\Program Files (x86)\FFDec\ffdec-cli.exe')
DEFAULT_DVARS = {'g_gametype': 'survival', 'surHUD_enemy': 4, 'surHUD_credits': 1250,
                 'ui_active': 0, 'spectatingfree': 0, 'spectatingclient': 0,
                 'milliseconds': 10000, 'ui_show_killedBy': 0}
LOCALIZATION = {'SO_SURVIVAL_ENEMIES_LEFT': '&&1 enemies left',
                'SO_SURVIVAL_ENEMY_LEFT': '&&1 enemy left', 'SO_SURVIVAL_CREDITS': 'Credits'}


class Expression:
    """Restricted interpreter of real Statement entries; never Python/JS eval."""
    PRECEDENCE = {'||': 1, '&&': 2, '|': 3, '&': 4, '==': 5, '!=': 5,
                  '<': 6, '>': 6, '<=': 6, '>=': 6, '+': 7, '-': 7,
                  '*': 8, '/': 8, '%': 8}

    def __init__(self, values=None, localize=None):
        self.values = values or {}
        self.localize = LOCALIZATION | (localize or {})
        self.unknown = set()

    def tokens(self, st):
        out = []
        for e in st['p'].get('4', []):
            kind, typ, val = struct.unpack('<IIi', bytes.fromhex(e['raw']))
            if kind == 0:
                out.append(('op', menus.OPS_1113[typ]))
            elif typ == 0: out.append(('value', val))
            elif typ == 1: out.append(('value', struct.unpack_from('<f', bytes.fromhex(e['raw']), 8)[0]))
            elif typ == 2: out.append(('value', e['p'].get('8', '')))
            elif typ == 3: out.append(('value', self.statement(e['p']['8'])))
        return out

    def statement(self, st):
        return self.evaluate(self.tokens(st))

    def text(self, source):
        tokens = []
        for t in re.findall(r'"(?:\\.|[^"\\])*"|&&|\|\||==|!=|<=|>=|[-+*/%<>()!,&|]|[\w.@]+', source):
            if t.startswith('"'): tokens.append(('value', json.loads(t)))
            elif re.fullmatch(r'\d+(?:\.\d+)?', t): tokens.append(('value', float(t)))
            else: tokens.append(('op', t))
        return self.evaluate(tokens)

    def evaluate(self, tokens):
        pos = 0

        def take():
            nonlocal pos
            if pos >= len(tokens): raise ValueError('incomplete expression')
            token = tokens[pos]; pos += 1
            return token

        def expression(level=0):
            nonlocal pos
            kind, token = take()
            if kind == 'value': left = token
            elif token == '(':
                left = expression()
                if pos < len(tokens) and tokens[pos][1] == ')': pos += 1
                else: raise ValueError('unclosed expression')
            elif token in ('!', '-', '+'):
                v = expression(9)
                left = not v if token == '!' else (-v if token == '-' else +v)
            elif token.startswith(('dvar', 'localvar')):
                # Static forms use a separate '(static)' decoration in text sources only.
                wrapped = pos >= 2 and tokens[pos-2] == ('op', '(')
                key = str(expression(9)); v = self.values.get(key, 0)
                left = str(v) if 'string' in token else (float(v) if 'float' in token else int(v or 0))
                # C1 static-function entries have an implicit opening call token.
                if '(static)' in token and not wrapped and pos < len(tokens) and tokens[pos] == ('op', ')'): pos += 1
            elif token in ('string', 'float', 'int'):
                left = {'string': str, 'float': float, 'int': int}[token](expression(9))
            elif token == 'locstring':
                key = str(expression(9)).lstrip('@')
                left = self.localize.get(key, '@' + key)
                if key not in self.localize: self.unknown.add('localization: ' + key)
                i = 1
                while pos < len(tokens) and tokens[pos][1] == ',':
                    pos += 1; left = left.replace('&&' + str(i), str(expression(0))); i += 1
            else:
                if token not in self.values: self.unknown.add('engine value: ' + str(token))
                left = self.values.get(token, 0)
            while pos < len(tokens):
                op = tokens[pos][1]; priority = self.PRECEDENCE.get(op, -1)
                if priority < level: break
                pos += 1; right = expression(priority + 1)
                if op == '+': left = str(left) + str(right) if isinstance(left, str) or isinstance(right, str) else left + right
                elif op == '-': left -= right
                elif op == '*': left *= right
                elif op == '/': left /= right
                elif op == '%': left %= right
                elif op == '==': left = left == right
                elif op == '!=': left = left != right
                elif op == '<': left = left < right
                elif op == '>': left = left > right
                elif op == '<=': left = left <= right
                elif op == '>=': left = left >= right
                elif op == '&&': left = bool(left) and bool(right)
                elif op == '||': left = bool(left) or bool(right)
                elif op == '&': left = int(left) & int(right)
                elif op == '|': left = int(left) | int(right)
            return left
        try:
            result = expression()
            # C1's prefix functions can carry trailing close tokens in dumps.
            if any(t[1] != ')' for t in tokens[pos:]): raise ValueError('unconsumed expression')
            return result
        except (ValueError, TypeError, ZeroDivisionError, IndexError) as e:
            self.unknown.add('expression: ' + str(e))
            return None


def window(node):
    raw = bytes.fromhex(node['raw']); p = node['p']
    return dict(name=p.get('0', ''), rect=list(struct.unpack_from('<4f', raw, 4)),
                client_rect=list(struct.unpack_from('<4f', raw, 0x18)),
                client_align=list(raw[0x28:0x2a]),
                align=list(raw[20:22]), color=list(struct.unpack_from('<4f', raw, 0x50)),
                backcolor=list(struct.unpack_from('<4f', raw, 0x60)),
                material=p.get('160', {}).get('name', ''), ownerdraw=struct.unpack_from('<i', raw, 0x38)[0])


def decoded(node, values=None, localization=None):
    evaluator = Expression(values, localization)
    if 'assets' in node:
        lists = [a['value'] for a in node['assets'] if a.get('type') == 'menufile']
    elif node.get('kind') == 'MenuList': lists = [node]
    else: raise ValueError('Expected C1 MenuList JSON or zone-menus asset dump')
    out = []
    for data in lists:
        for m in data['p'].get('8', []):
            menu = window(m); menu['items'] = []
            mp = m['p']; menu['visible'] = evaluator.statement(mp['228']) if '228' in mp else True
            for i, item in enumerate(mp.get('284', [])):
                it = window(item); p = item['p']; r = bytes.fromhex(item['raw'])
                # C1 Item_SetScreenCoords 608D07..D36 discards the cached
                # rectangle and rebuilds it from rectClient + parent origin.
                it['cached_rect']=list(it['rect'])
                it['cached_align']=list(it['align']);it['align']=list(it['client_align'])
                it['rect']=list(it['client_rect'])
                it['rect'][0]+=menu['rect'][0];it['rect'][1]+=menu['rect'][1]
                if p.get('320'):evaluator.unknown.add('item geometry expressions require engine evaluation: '+str(i))
                it.update(index=i, text=p.get('228', ''), scale=struct.unpack_from('<f', r, 0xd4)[0],
                          textalign=struct.unpack_from('<i', r, 0xc8)[0],
                          textoffset=list(struct.unpack_from('<2f', r, 0xcc)),
                          visible=evaluator.statement(p['324']) if '324' in p else True,
                          expression=menus.expr(p['332']) if '332' in p else '',
                          visibility=menus.expr(p['324']) if '324' in p else '', source=item)
                if '332' in p: it['text'] = evaluator.statement(p['332'])
                if it['text'] is not None: it['text'] = str(it['text'])
                if isinstance(it['text'],str) and it['text'].startswith('@'):
                    it['text'] = evaluator.localize.get(it['text'][1:], it['text'])
                if it['ownerdraw']: evaluator.unknown.add('native ownerdraw: ' + str(it['ownerdraw']))
                if it['material'] not in ('', ',white', 'white'): evaluator.unknown.add('material placeholder: ' + it['material'])
                menu['items'].append(it)
            out.append(menu)
    return {'menus': out, 'warnings': sorted(evaluator.unknown), 'format': 'C1 decoded MenuList'}


def parse_menu(text, values=None, localization=None):
    """Parse brace-form .menu sources; preserve unsupported directives as diagnostics."""
    tokens = [t for t in re.findall(r'"(?:\\.|[^"\\])*"|/\*.*?\*/|//[^\n]*|[{}()]|[^\s{}()]+', text,flags=re.S)
              if not t.startswith(('//','/*'))]
    pos = 0; warnings = set(); evaluator = Expression(values, localization)
    counts = {'name': 1, 'rect': 4, 'forecolor': 4, 'backcolor': 4, 'bordercolor': 4,
              'text': 1, 'textscale': 1, 'textalign': 1, 'textalignx': 1, 'textaligny': 1,
              'background': 1, 'visible': 1, 'ownerdraw': 1, 'type': 1, 'style': 1,
              'decoration': 0, 'fullscreen': 1, 'font': 1, 'textfont': 1, 'textstyle': 1,
              'group': 1, 'border': 1, 'bordersize': 1, 'exp': 0}

    def value(t):
        if t.startswith('"'): return json.loads(t)
        try: return float(t)
        except ValueError: return t

    def block():
        nonlocal pos
        data = {'children': []}
        while pos < len(tokens):
            key = tokens[pos]; pos += 1
            if key == '}': return data
            if key.lower() in ('menudef', 'itemdef'):
                if pos >= len(tokens) or tokens[pos] != '{': raise ValueError('Expected { after ' + key)
                pos += 1; data['children'].append((key.lower(), block())); continue
            if pos < len(tokens) and tokens[pos] == '{':
                depth=1; pos+=1
                while pos < len(tokens) and depth:
                    depth += (tokens[pos]=='{') - (tokens[pos]=='}'); pos+=1
                warnings.add('event/script block is not executed: ' + key); continue
            key = key.lower()
            if key == 'visible' and pos < len(tokens) and tokens[pos] == '(':
                depth=0; parts=[]
                while pos < len(tokens):
                    t=tokens[pos];pos+=1;parts.append(t);depth+=(t=='(')-(t==')')
                    if depth==0:break
                data[key]=evaluator.text(' '.join(parts)); continue
            if key not in counts:
                warnings.add('unsupported directive/token: ' + key); continue
            n=counts[key]
            if pos+n>len(tokens):raise ValueError('Missing arguments: '+key)
            data[key]=[value(t) for t in tokens[pos:pos+n]];pos+=n
            if key=='rect' and pos+1<len(tokens) and all(re.fullmatch(r'\d+',t) for t in tokens[pos:pos+2]):
                data['align']=[int(t) for t in tokens[pos:pos+2]];pos+=2
        return data

    def adapt(d,index=0):
        def scalar(k, default): return d.get(k,[default])[0]
        rect=d.get('rect',[0,0,640,480]); rect=[float(v) for v in rect]
        return dict(name=scalar('name',''),rect=rect,align=d.get('align',[0,0]),
                    color=d.get('forecolor',[1,1,1,1]),backcolor=d.get('backcolor',[0,0,0,0]),
                    material=scalar('background',''),ownerdraw=scalar('ownerdraw',0),index=index,
                    visible=d.get('visible', [1])[0] if isinstance(d.get('visible',[1]),list) else d['visible'],
                    text=scalar('text',''),scale=scalar('textscale',.25),textalign=scalar('textalign',0),
                    textoffset=[scalar('textalignx',0),scalar('textaligny',0)],source=d)
    tree=block(); out=[]
    def visit(d):
        for kind,child in d['children']:
            if kind=='menudef':
                m=adapt(child); m['items']=[adapt(v,i) for i,(k,v) in enumerate(child['children']) if k=='itemdef'];out.append(m)
            else:visit(child)
    visit(tree)
    if not out: raise ValueError('No menuDef blocks. Binary .menu files need a decoded MenuList JSON sidecar.')
    return {'menus':out,'warnings':sorted(warnings|evaluator.unknown),'format':'Text .menu (macros/includes must be expanded)'}


def load_menu(path, values=None, localization=None):
    path=Path(path)
    if path.suffix.lower()=='.bin' or b'\0' in path.read_bytes()[:128]:
        sibling=path.with_suffix('.json')
        if not sibling.exists():raise ValueError('Binary menu needs decoded JSON sidecar from zone-menus extraction')
        path=sibling
    text=path.read_text(encoding='utf-8-sig')
    return decoded(json.loads(text),values,localization) if text.lstrip().startswith('{"') or path.suffix=='.json' else parse_menu(text,values,localization)


def anchor(rect, align, width, height):
    """640x480 screen-placement approximation; 8/9/10 are full-screen anchors."""
    x,y,w,h=rect; sx=sy=height/480
    def point(v,a,extent,axis):
        if a in (8,1):return v*sy
        if a in (9,2):return extent/2+v*sy
        if a in (10,3):return extent+v*sy
        if a in (4,):return v*extent/(640 if axis==0 else 480)
        return (extent-640*sy)/2+v*sy if axis==0 else v*sy
    return [point(x,align[0],width,0),point(y,align[1],height,1),w*sx,h*sy]


def fingerprint(path):
    paths=sorted(path.rglob('*')) if path.is_dir() else [path]
    return tuple((str(p),p.stat().st_mtime_ns,p.stat().st_size) for p in paths if p.is_file())


def movie_bytes(path,game,ffdec):
    if path.is_dir():
        from zonekit.stock import ZoneSource
        meta,_=gfx.read_patch(str(path))
        source=ZoneSource(str(game/'C1.exe'),[str(game/'client_shipRetail/chinese'),str(game/'client_shipRetail/patch')])
        stock=source.rawfile(meta['stock']); movie=gfx.build_patch(str(path),stock)
        for cls,rel,digest in meta['as']:
            file=path/rel
            if gfx.source_sha(str(file))!=digest:
                movie=gfx.compile_as(gfx.with_stock_class(movie,stock,cls),cls,str(file),str(ffdec))
        return movie
    return path.read_bytes()


def prepared_movie(path,game,ffdec,outdir):
    from ui_inspector_assets import prepare_movie_assets
    movie=movie_bytes(path,game,ffdec)
    return movie,prepare_movie_assets(movie,outdir,game)


class Inspector:
    def __init__(self,args):
        from PyQt5 import QtCore, QtGui, QtWidgets
        self.QtCore=QtCore;self.QtGui=QtGui;self.QtWidgets=QtWidgets
        self.args=args;self.path=Path(args.file).resolve() if args.file else None
        self.app=QtWidgets.QApplication.instance() or QtWidgets.QApplication(sys.argv[:1])
        # Offscreen Qt has no Windows font provider; register the real font file.
        self.font_family='Arial'
        font_path=Path(os.environ.get('WINDIR','C:/Windows'))/'Fonts/arial.ttf'
        if font_path.exists():
            font_id=QtGui.QFontDatabase.addApplicationFont(str(font_path))
            families=QtGui.QFontDatabase.applicationFontFamilies(font_id)
            if families:self.font_family=families[0]
        self.root=QtWidgets.QMainWindow();self.root.setWindowTitle('CoDOL UI Inspector - live source preview');self.root.resize(1440,920)
        self.pool=concurrent.futures.ThreadPoolExecutor(max_workers=1);self.future=None;self.player=None;self.log=None
        self.temp=tempfile.TemporaryDirectory(prefix='codol-ui-inspector-');self.stamp=None;self.document=None
        central=QtWidgets.QWidget();self.root.setCentralWidget(central);layout=QtWidgets.QVBoxLayout(central)
        bar=QtWidgets.QHBoxLayout();layout.addLayout(bar)
        for label,fn in [('Open file',self.open),('Open GFx patch',self.open_patch),('Reload',self.reload),('Play GFx',self.play),('Export report',self.export),('Save PNG',self.save_png)]:
            button=QtWidgets.QPushButton(label);button.clicked.connect(fn);bar.addWidget(button)
        self.resolution=QtWidgets.QComboBox();self.resolution.addItems(['640x480','1024x768','1280x720','1920x1080','2560x1440']);self.resolution.setCurrentText('1280x720');self.resolution.currentTextChanged.connect(self.draw);bar.addWidget(self.resolution)
        self.watch=QtWidgets.QCheckBox('Live reload');self.watch.setChecked(True);bar.addWidget(self.watch)
        self.bounds=QtWidgets.QCheckBox('Bounds');self.bounds.setChecked(True);self.bounds.toggled.connect(self.draw);bar.addWidget(self.bounds)
        self.hidden=QtWidgets.QCheckBox('Show hidden');self.hidden.toggled.connect(self.draw);bar.addWidget(self.hidden)
        self.selector=QtWidgets.QComboBox();self.selector.currentTextChanged.connect(self.draw);bar.addWidget(self.selector)
        self.status=QtWidgets.QLabel('Open .menu / MenuList JSON / SWF / GFX / .gfxpatch');self.status.setWordWrap(True);self.status.setMaximumHeight(54);layout.addWidget(self.status)
        note=QtWidgets.QLabel('Menu: system-font geometry preview; unresolved materials/ownerdraw are diagnosed. GFx: genuine SDK playback; game callbacks are not supplied.');note.setWordWrap(True);note.setMaximumHeight(36);layout.addWidget(note)
        split=QtWidgets.QSplitter();layout.addWidget(split,1)
        self.scene=QtWidgets.QGraphicsScene();self.scene.setBackgroundBrush(QtGui.QColor('#151b22'))
        self.view=QtWidgets.QGraphicsView(self.scene);self.view.setRenderHint(QtGui.QPainter.Antialiasing);split.addWidget(self.view)
        panel=QtWidgets.QWidget();side=QtWidgets.QVBoxLayout(panel);split.addWidget(panel);split.setSizes([1000,350])
        side.addWidget(QtWidgets.QLabel('Preview dvars / engine values (JSON)'))
        self.values=QtWidgets.QPlainTextEdit(json.dumps(DEFAULT_DVARS,indent=2));self.values.setMaximumHeight(260);side.addWidget(self.values)
        apply=QtWidgets.QPushButton('Apply preview state');apply.clicked.connect(self.reload);side.addWidget(apply)
        side.addWidget(QtWidgets.QLabel('Diagnostics / selected item'))
        self.detail=QtWidgets.QPlainTextEdit();self.detail.setReadOnly(True);side.addWidget(self.detail)
        self.scene.selectionChanged.connect(self.inspect)
        self.timer=QtCore.QTimer();self.timer.timeout.connect(self.tick);self.timer.start(500)
        self.app.aboutToQuit.connect(self.close)
        if self.path:self.reload()
        self.root.show()

    def open(self):
        p,_=self.QtWidgets.QFileDialog.getOpenFileName(self.root,'Open UI source','','UI sources (*.menu *.json *.bin *.gfx *.swf);;All files (*)')
        if p:self.path=Path(p);self.reload()

    def open_patch(self):
        p=self.QtWidgets.QFileDialog.getExistingDirectory(self.root,'Select .gfxpatch directory')
        if p:self.path=Path(p);self.reload()

    def reload(self):
        if not self.path or (self.future and not self.future.done()):return
        try:
            state=json.loads(self.values.toPlainText());self.stamp=fingerprint(self.path)
            if self.path.is_dir() or self.path.suffix.lower() in ('.gfx','.swf'):
                self.status.setText('Building movie in background: '+str(self.path));self.document=None
                self.future=self.pool.submit(prepared_movie,self.path,self.args.game,self.args.ffdec,self.temp.name);return
            document=load_menu(self.path,state);self.document=document
            selected=self.selector.currentText();names=[m['name'] or 'unnamed' for m in document['menus']]
            self.selector.blockSignals(True);self.selector.clear();self.selector.addItems(names)
            if selected in names:self.selector.setCurrentText(selected)
            self.selector.blockSignals(False);self.status.setText(str(self.path)+' - '+document['format']);self.draw()
        except Exception as e:self.status.setText('Parse error (last preview retained): '+str(e))

    def play(self):
        if self.document:self.status.setText('Select SWF/GFX or .gfxpatch for real Scaleform playback');return
        movie=Path(self.temp.name)/'preview.gfx'
        if not movie.exists():self.reload();return
        if not self.args.player.exists():self.status.setText('GFx SDK player missing; use --player PATH');return
        self.stop_player();self.log=open(Path(self.temp.name)/'player.log','w',encoding='utf-8')
        playable=movie
        self.player=subprocess.Popen([str(self.args.player),'-nh','-ns','-res',self.resolution.currentText().replace('x',':'),str(playable)],cwd=self.path if self.path.is_dir() else self.path.parent,stdout=self.log,stderr=self.log)
        self.status.setText('Real GFx player running; source saves rebuild/relaunch. Missing callbacks/assets appear in log.')

    def stop_player(self):
        if self.player and self.player.poll() is None:
            self.player.terminate()
            try:self.player.wait(timeout=3)
            except subprocess.TimeoutExpired:self.player.kill();self.player.wait()
        self.player=None
        if self.log:self.log.close();self.log=None

    def tick(self):
        try:
            if self.future and self.future.done():
                future=self.future;self.future=None;movie,assets=future.result();gfx.tags(gfx_uncompress(movie));self.asset_report=assets
                target=Path(self.temp.name)/'preview.gfx';target.write_bytes(movie)
                self.scene.clear();self.scene.addText('Scaleform movie ready. Click Play GFx for real interactive playback.\n'+str(len(movie))+' bytes - SHA256 '+hashlib.sha256(movie).hexdigest()).setDefaultTextColor(self.QtGui.QColor('white'))
                if self.player is not None:self.play()
                else:self.status.setText('GFx ready: '+str(self.path))
            if self.player:
                log=Path(self.temp.name)/'player.log';self.detail.setPlainText(log.read_text(errors='replace')[-16000:] if log.exists() else '')
            if self.watch.isChecked() and self.path and fingerprint(self.path)!=self.stamp:self.reload()
            if self.document:self.view.fitInView(self.scene.sceneRect(),self.QtCore.Qt.KeepAspectRatio)
        except Exception as e:self.status.setText('Preview error: '+str(e))

    def draw(self,*_):
        if not self.document:return
        Q=self.QtCore.Qt;G=self.QtGui;W=self.QtWidgets
        self.scene.blockSignals(True);self.scene.clear()
        menu=next((m for m in self.document['menus'] if (m['name'] or 'unnamed')==self.selector.currentText()),self.document['menus'][0])
        width,height=map(int,self.resolution.currentText().split('x'));self.scene.setSceneRect(0,0,width,height)
        self.scene.addRect(0,0,width,height,G.QPen(G.QColor('#53687c')))
        warnings=list(self.document['warnings']);boxes=[]
        for item in menu['items']:
            if (menu['visible'] is False or not item['visible']) and not self.hidden.isChecked():continue
            x,y,w,h=anchor(item['rect'],item['align'],width,height)
            color=G.QColor.fromRgbF(*[max(0,min(1,v)) for v in item['color']])
            if item['material']:
                brush=G.QBrush(color) if item['material'].lstrip(',')=='white' else G.QBrush(G.QColor('#48657e'),Q.BDiagPattern)
                shape=self.scene.addRect(x,y,w,h,G.QPen(G.QColor('#55778e')),brush);shape.setData(0,item);shape.setFlag(W.QGraphicsItem.ItemIsSelectable)
            text=item.get('text') or ''
            if text:
                scale=height/480;f=G.QFont(self.font_family);f.setPixelSize(max(1,round(48*item['scale']*scale)))
                obj=self.scene.addSimpleText(text,f);obj.setBrush(color);metrics=G.QFontMetricsF(f)
                tx=x+item['textoffset'][0]*scale;ty=y+item['textoffset'][1]*scale-metrics.ascent()
                if item['textalign'] in (2,6,10):tx-=obj.boundingRect().width()
                elif item['textalign'] in (1,5,9):tx-=obj.boundingRect().width()/2
                obj.setPos(tx,ty);obj.setData(0,item);obj.setFlag(W.QGraphicsItem.ItemIsSelectable)
                boxes.append((item['index'],obj.sceneBoundingRect()))
            if self.bounds.isChecked():self.scene.addRect(x,y,max(w,3),max(h,3),G.QPen(G.QColor('#4a7a85'),1,Q.DotLine))
        for i,(a,box) in enumerate(boxes):
            for b,other in boxes[i+1:]:
                if box.intersects(other):
                    warnings.append(f'Text bounds overlap: item {a} / {b}');self.scene.addRect(box,G.QPen(G.QColor('#ff674f'),2))
        self.detail.setPlainText('\n'.join(warnings) or 'No unresolved preview fields or text overlaps.')
        self.document['preview_warnings']=warnings
        self.document['preview_viewport']={'width':width,'height':height,'menu':menu['name'],'font':self.font_family,'font_scope':'System font; not C1 rasterizer'}
        self.scene.blockSignals(False);self.view.fitInView(self.scene.sceneRect(),Q.KeepAspectRatio)

    def inspect(self):
        for obj in self.scene.selectedItems():
            data=obj.data(0)
            if data:self.detail.setPlainText(json.dumps(data,indent=2));break

    def export(self):
        p,_=self.QtWidgets.QFileDialog.getSaveFileName(self.root,'Export report','','JSON (*.json)')
        if p:Path(p).write_text(json.dumps(self.document or {'movie':str(self.path),'log':str(Path(self.temp.name)/'player.log')},indent=2)+'\n')

    def save_png(self):
        p,_=self.QtWidgets.QFileDialog.getSaveFileName(self.root,'Save preview','','PNG (*.png)')
        if not p:return
        self.write_png(p)

    def write_png(self,p):
        width,height=map(int,self.resolution.currentText().split('x'));image=self.QtGui.QImage(width,height,self.QtGui.QImage.Format_ARGB32)
        image.fill(self.QtGui.QColor('#151b22'));painter=self.QtGui.QPainter(image);painter.setRenderHint(self.QtGui.QPainter.Antialiasing)
        self.scene.render(painter);painter.end();image.save(p)

    def close(self):
        self.timer.stop();self.stop_player();self.pool.shutdown(wait=False,cancel_futures=True);self.temp.cleanup()


def gfx_uncompress(data):
    import zlib
    if data[:3] in (b'CFX',b'CWS'):return (b'GFX' if data[:3]==b'CFX' else b'FWS')+data[3:8]+zlib.decompress(data[8:])
    return data


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('file',nargs='?');ap.add_argument('--game',type=Path,default=DEFAULT_GAME)
    ap.add_argument('--player',type=Path,default=DEFAULT_PLAYER);ap.add_argument('--ffdec',type=Path,default=DEFAULT_FFDEC)
    ap.add_argument('--report',type=Path,help='Headless JSON menu parse/visibility report; no GUI')
    ap.add_argument('--state',type=Path,help='Preview dvar JSON file for --report')
    ap.add_argument('--snapshot',type=Path,help='Render menu PNG without a desktop window')
    ap.add_argument('--resolution',default='1280x720')
    ap.add_argument('--menu',help='Menu name to render')
    ap.add_argument('--capture-gfx',type=Path,help='SDK movie capture output directory; no desktop input')
    ap.add_argument('--capture-backend',choices=['auto','sdk','ffdec'],default='auto')
    ap.add_argument('--character',help='Static GFx component symbol or numeric character ID')
    ap.add_argument('--scenario',type=Path,help='JSON movie actions: advance/click/move/key/capture')
    args=ap.parse_args()
    if args.capture_gfx:
        if not args.file:ap.error('--capture-gfx requires a movie source')
        from ui_inspector_capture import capture_movie
        report=capture_movie(args.file,args.capture_gfx,args.game,json.loads(args.scenario.read_text()) if args.scenario else None,args.resolution,backend=args.capture_backend,ffdec=args.ffdec,**({'character':args.character} if args.character else {}))
        print(json.dumps({'backend':report['backend'],'exit_code':report['exit_code'],'captures':report['captures'],'unresolved_assets':report['assets']['unresolved'],'limitations':report['limitations']}));return
    if args.report:
        if not args.file:ap.error('--report requires a source file')
        state=DEFAULT_DVARS | (json.loads(args.state.read_text()) if args.state else {})
        args.report.write_text(json.dumps(load_menu(args.file,state),indent=2)+'\n');print(args.report);return
    if args.snapshot:os.environ['QT_QPA_PLATFORM']='offscreen'
    inspector=Inspector(args)
    if args.state:
        inspector.values.setPlainText(json.dumps(DEFAULT_DVARS|json.loads(args.state.read_text()),indent=2));inspector.reload()
    if args.menu:inspector.selector.setCurrentText(args.menu)
    if args.resolution:inspector.resolution.setCurrentText(args.resolution)
    if args.snapshot:
        if not inspector.document:raise ValueError('--snapshot requires a parsed menu source')
        inspector.app.processEvents();inspector.draw();inspector.write_png(str(args.snapshot))
        args.snapshot.with_suffix('.report.json').write_text(json.dumps(inspector.document,indent=2)+'\n',encoding='utf-8')
        inspector.close();print(args.snapshot);return
    sys.exit(inspector.app.exec_())


if __name__=='__main__':main()
