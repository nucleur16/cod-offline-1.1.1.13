"""Build a separate quiet script IWD without editing its immutable input.

Production removes developer blocks and statement calls to native developer-only
output builtins. Assertions, gameplay messages, errors, print/logprint and other code are preserved.
Development reproduces every original entry. All edits carry exact source spans.
"""
from pathlib import Path
import argparse,hashlib,json,re,zipfile

DEVELOPER_BUILTINS=frozenset(('println','print3d','printstar','line'))
def sha(data):return hashlib.sha256(data).hexdigest()
def verify_developer_builtins(native_src):
    source=Path(native_src)/'scr_vm_main.c'
    data=source.read_bytes()
    # Bind this transform to the registrations of the exact materialized native
    # profile. Never silently strip a builtin that became gameplay-visible.
    text=re.sub(r'/\*[\s\S]*?\*/|//[^\r\n]*','',data.decode('utf8'))
    registrations={}
    for name in DEVELOPER_BUILTINS:
        flags=re.findall(r'Scr_AddFunction\s*\(\s*"'+name+r'"\s*,\s*\w+\s*,\s*(\d+)\s*\)',text)
        if flags!=['1']:raise ValueError('expected one developer-only native registration: '+name)
        registrations[name]=1
    return dict(source=str(source),sha256=sha(data),registrations=registrations)
def tokens(text):
    pattern=r'//[^\r\n]*|/\*[\s\S]*?\*/|"(?:\\[\s\S]|[^"\\])*"|/#|#/|[A-Za-z_][A-Za-z_0-9]*|::|\S'
    for m in re.finditer(pattern,text):
        token=m[0]
        if token.startswith('//') or token.startswith('/*'):continue
        yield token,m.start(),m.end()

def quiet(text):
    ts=list(tokens(text));edits=[];opened=None;active=[]
    for token,start,end in ts:
        if token=='/#':
            if opened is not None:raise ValueError('nested developer block')
            opened=start
        elif token=='#/':
            if opened is None:raise ValueError('unmatched developer block close')
            edits.append((opened,end,'developer block'));opened=None
        elif opened is None:active.append((token,start,end))
    if opened is not None:raise ValueError('unclosed developer block')
    # These native builtins are void developer-only statements. Never rewrite
    # declarations, namespaced calls, method calls or value expressions.
    for i,(token,start,end) in enumerate(active):
        if token.lower() not in DEVELOPER_BUILTINS or i+1>=len(active) or active[i+1][0]!='(':continue
        previous=active[i-1][0] if i else '{'
        if previous not in ('{','}',';',')',':','else'):continue
        depth=0;j=i+1
        while j<len(active):
            value=active[j][0]
            if value=='(':depth+=1
            elif value==')':
                depth-=1
                if depth==0:break
            j+=1
        if depth or j+1>=len(active) or active[j+1][0]!=';':continue
        edits.append((start,active[j+1][2],'developer builtin '+token.lower()))
    edits.sort();last=0;result=[];proof=[]
    for start,end,kind in edits:
        if start<last:raise ValueError('overlapping diagnostic edits')
        original=text[start:end]
        result.extend((text[last:start],'{}'+''.join(c for c in original if c in '\r\n') if kind.startswith('developer builtin') else ''.join(c for c in original if c in '\r\n')+' '))
        proof.append(dict(start=start,end=end,line=text.count('\n',0,start)+1,kind=kind,before_sha256=sha(original.encode('latin1'))))
        last=end
    result.append(text[last:]);return ''.join(result),proof

def build(source,output,profile,native_src=None):
    source=Path(source);output=Path(output)
    if profile not in ('production','development'):raise ValueError('unknown profile')
    if profile=='production' and native_src is None:raise ValueError('production requires exact materialized native source')
    native_proof=verify_developer_builtins(native_src) if profile=='production' else None
    if output.exists():raise ValueError('output already exists')
    output.mkdir(parents=True)
    destination=output/'sh1x_svr_scripts.iwd';proof={};files={};changed=0
    with zipfile.ZipFile(source) as original,zipfile.ZipFile(destination,'w') as target:
        names=original.namelist()
        if len(names)!=len(set(names)):raise ValueError('duplicate archive names')
        for entry in original.infolist():
            name=entry.filename
            if Path(name).is_absolute() or '..' in Path(name).parts:raise ValueError('unsafe archive name')
            before=original.read(name);after=before
            if name.endswith('.gsc') and profile=='production':
                text,edits=quiet(before.decode('latin1'));after=text.encode('latin1')
                if edits:proof[name]=edits
            changed+=after!=before;files[name]=dict(before=sha(before),after=sha(after))
            target.writestr(entry,after)
            if name.endswith('.gsc'):
                path=output/'resolver-source'/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(after)
    receipt=dict(profile=profile,input=str(source),input_sha256=sha(source.read_bytes()),output_sha256=sha(destination.read_bytes()),
        files=files,changed_scripts=changed,edits=proof,developer_builtins=sorted(DEVELOPER_BUILTINS),
        non_script_entries_unchanged=True,gameplay_notifications_and_non_developer_logging_preserved=True,
        native_resolver_and_gameplay_pending=True,installed=False,production_changed=False)
    receipt['native_registration_proof']=native_proof
    (output/'profile.json').write_text(json.dumps(receipt,indent=2)+'\n')
    return receipt

def verify_archive(source):
    """Refuse developer tracing in a production archive without rewriting it."""
    source=Path(source);scripts=0
    with zipfile.ZipFile(source) as archive:
        names=archive.namelist()
        if len(names)!=len(set(names)):raise ValueError('duplicate archive names')
        for name in names:
            if Path(name).is_absolute() or '..' in Path(name).parts:raise ValueError('unsafe archive name')
            data=archive.read(name)
            if name.endswith('.gsc'):
                scripts+=1
                if quiet(data.decode('latin1'))[1]:raise ValueError('production developer tracing remains: '+name)
    if not scripts:raise ValueError('production archive contains no scripts')
    return dict(profile='production',scripts=scripts,sha256=sha(source.read_bytes()))

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--input',type=Path)
    p.add_argument('--output',type=Path);p.add_argument('--profile',choices=('production','development'))
    p.add_argument('--native-src',type=Path,help='Exact materialized native src directory (required for production)')
    p.add_argument('--verify',type=Path,help='Verify a quiet production archive without changing it')
    a=p.parse_args()
    if a.verify:
        if any((a.input,a.output,a.profile,a.native_src)):p.error('--verify cannot be combined with build arguments')
        print(json.dumps(verify_archive(a.verify)))
    else:
        if not all((a.input,a.output,a.profile)):p.error('build requires --input, --output and --profile')
        r=build(a.input,a.output,a.profile,a.native_src)
        print(json.dumps({k:r[k] for k in ('profile','input_sha256','output_sha256','changed_scripts','non_script_entries_unchanged')}))
