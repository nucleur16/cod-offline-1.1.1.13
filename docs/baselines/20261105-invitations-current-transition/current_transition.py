"""Current MatchController transition; historical invitation reviews immutable."""
from pathlib import Path
import hashlib,importlib.util,json,re,struct,zlib

ROOT=Path(__file__).resolve().parents[3]
LEGACY=ROOT/'docs/baselines/20261024-invitations-disabled'
CLASS='codo.controllers.MatchController'
PATCH='zones/voice_ui_mp/rawfile/scaleform/default/codo.gfx.gfxpatch'
DOC=Path(__file__).resolve().parent
TRANSITION_SHA256='ca644ff7819f0e970c1ce8f50982c45009bd5bf9f85e04857a58872762e52c57'
DENIAL_CODE='d030ef018602000047'

def need(ok,message):
    if not ok:raise ValueError(message)
def sha(raw):return hashlib.sha256(raw).hexdigest()
def module(name,path):
    raw=path.read_bytes();m=__import__('types').ModuleType(name);m.__file__=str(path);exec(compile(raw,str(path),'exec'),m.__dict__);return m
A=module('current_transition_abc',LEGACY/'abc_reader.py')
G=module('current_transition_abi',ROOT/'tools/theatre-retirement/abc_guard.py')
F=module('current_transition_source',LEGACY/'behavior_fixture.py')

def extract(tag):
    need(len(tag)>=7,'bounded complete ABC tag')
    value=struct.unpack_from('<H',tag)[0];header=6 if value&63==63 else 2
    size=struct.unpack_from('<I',tag,2)[0]if header==6 else value&63
    need(value>>6==82 and size==len(tag)-header,'exact DoABC tag/length')
    return tag[tag.index(b'\0',header+4)+1:]

def read_contract():
    raw=(DOC/'transition.json').read_bytes()
    need(sha(raw)==TRANSITION_SHA256,'reviewed current transition contract differs')
    return json.loads(raw)

def historical_parent():
    contract=read_contract()
    raw=zlib.decompress((LEGACY/'MatchController.after-integration-20261029.abc.zlib').read_bytes())
    need(sha(raw)==contract['parent_abc_sha256'],'immutable historical integration endpoint differs')
    return raw

def invitation_contract(old,new,source,resolved,code_hex):
    need(code_hex==DENIAL_CODE,'immutable historical denial code required')
    for abc in(old,new):
        matches=[b for b in abc['bodies'].values()if b['name'].endswith('/onProposeRoomMembershipHandle')]
        need(len(matches)==1 and matches[0]['code'].hex()==code_hex,'protected invitation denial bytecode differs')
    for text in(source.decode('utf-8'),resolved.decode('utf-8')):
        need(F.method(text,'onProposeRoomMembershipHandle')['body'].strip()in('','return;'),'invitation denial source/compiled semantics differ')

def verify_abi(before,after):
    old,oldmethods,_=interface(before);new,newmethods,_=interface(after)
    need(old.keys()==new.keys(),'current transition class inventory differs')
    changed=[];added=[]
    for key,prior in old.items():
        current=new[key]
        need(all(prior[k]==current[k]for k in('superclass','flags','protected','interfaces','initializer')),'current public class ABI differs')
        need(not(prior['traits'].keys()-current['traits'].keys()),'existing member removed')
        for member,value in prior['traits'].items():
            if current['traits'][member]!=value:
                need(member[1]is not None and member[1][0]==5 and member[2]in('listSameTypeMap','listMap','listMode'),'unreviewed public/protected or private ABI difference')
                changed.append(member[2])
        for member in current['traits'].keys()-prior['traits'].keys():
            need(member[1]is not None and member[1][0]==5 and member[2]=='isSupportedMapMode','unreviewed added member')
            added.append(member[2])
    need(sorted(changed)==['listMap','listMode','listSameTypeMap']and added==['isSupportedMapMode'],'exact reviewed private ABI transition inventory')
    return dict(private_optional_signatures=sorted(changed),private_helper=added,old_methods=len(oldmethods),current_methods=len(newmethods),existing_public_protected_ABI_exact=True)

def verify_prior_current(root,tag,*,source=None):
    root=Path(root);contract=read_contract()
    need(contract['format']=='invitations-current-transition-v1','reviewed transition schema')
    current=extract(tag);baseline=historical_parent()
    reviewed=zlib.decompress((DOC/'MatchController.current.abc.zlib').read_bytes())
    resolved=zlib.decompress((DOC/'MatchController.current.as.zlib').read_bytes())
    need(sha(reviewed)==contract['current_abc_sha256']and sha(resolved)==contract['resolved_export_sha256'],'current immutable compiler/export snapshot differs')
    need(sha(tag)==contract['current_tag_sha256']and current==reviewed,'unreviewed current compiled MatchController transition')
    if source is None:source=(root/PATCH/'as/codo/controllers/MatchController.as').read_bytes()
    need(sha(source)==contract['source_sha256'],'compiled producer/source identity differs')
    recipe=(root/PATCH/'patch.txt').read_text(encoding='utf-8')
    row='as '+CLASS+' as/codo/controllers/MatchController.as '+contract['source_sha256']
    need(recipe.splitlines().count(row)==1,'exact source producer recipe row required')
    old,new=A.read(baseline),A.read(current)
    need(sha(new['tables'])==contract['current_tables_sha256']and len(new['bodies'])==contract['current_body_count'],'current metadata/body inventory differs')
    need({str(i):dict(name=b['name'],raw_sha256=sha(b['raw']),code_sha256=sha(b['code']))for i,b in new['bodies'].items()}==contract['current_body_pins'],'every current body must match explicit reviewed endpoint')
    proof=verify_abi(baseline,current)
    invitation_contract(old,new,source,resolved,contract['invitation_denial_code_hex'])
    need(sha((LEGACY/'abi-contract.json').read_bytes())==contract['historical_abi_contract_sha256'],'historical ABI receipt must remain immutable')
    return dict(**proof,current_body_count=len(new['bodies']),current_abc_sha256=sha(current),protected_invitation_noop=True,historical_parent_sha256=sha(baseline))

def verify_current(root,tag,*,source=None):
    successor=module('current_playlist_selector_successor',Path(root)/'tools/playlist-selector-producer/current_endpoint.py')
    import types
    return successor.verify_successor(root,tag,source=source,prior=types.SimpleNamespace(**globals()))


def interface(data):
    parsed = G.parse(data)
    bounded = A.read(data)
    strings, spaces, names = (parsed['strings'], parsed['spaces'], parsed['names'])

    def name(index):
        if index == 0:
            return '*'
        kind, ns, text = names[index]
        assert text is not None, 'unresolved type name'
        space = None if ns is None else (spaces[ns][0], strings[spaces[ns][1]])
        return (kind, space, strings[text])
    r = G.Reader(data)
    r.p = parsed['methods_begin']
    methods = []
    for _ in range(r.n()):
        count = r.n()
        returns = name(r.n())
        args = tuple((name(r.n()) for _ in range(count)))
        label = strings[r.n()]
        flags = r.b()
        options = []
        parameters = []
        if flags & 8:
            for _ in range(r.n()):
                value, kind = (r.n(), r.b())
                pool = {1: 'string', 3: 'int', 4: 'uint', 6: 'double'}.get(kind)
                options.append((kind, bounded['values'][pool][value] if pool else value))
        if flags & 128:
            parameters = [strings[r.n()] for _ in range(count)]
        methods.append(dict(name=label, args=args, returns=returns, flags=flags, options=options, parameters=parameters))

    def traits():
        result = {}
        for _ in range(r.n()):
            key = name(r.n())
            kind = r.b()
            role = kind & 15
            slot = r.n()
            value = r.n()
            entry = {'kind': kind, 'slot': slot}
            if role in (0, 6):
                entry['type'] = name(value)
                default = r.n()
                entry['default'] = (default, r.b()) if default else None
            elif role in (1, 2, 3, 5):
                entry['signature'] = methods[value]
            elif role == 4:
                entry['class_index'] = value
            else:
                raise ValueError('unknown trait')
            if kind & 64:
                entry['metadata'] = [r.n() for _ in range(r.n())]
            assert key not in result
            result[key] = entry
        return result
    r.p = parsed['classes_begin']
    count = r.n()
    classes = {}
    for _ in range(count):
        key = name(r.n())
        superclass = name(r.n())
        flags = r.b()
        protected = spaces[r.n()] if flags & 8 else None
        interfaces = tuple((name(r.n()) for _ in range(r.n())))
        initializer = methods[r.n()]
        classes[key] = dict(superclass=superclass, flags=flags, protected=protected, interfaces=interfaces, initializer=initializer, traits=traits())
    bodies = {body['name']: sha(body['raw']) for body in bounded['bodies'].values()}
    return (classes, methods, bodies)
