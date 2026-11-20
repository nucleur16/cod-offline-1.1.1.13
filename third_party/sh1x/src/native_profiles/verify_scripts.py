"""Verify a retained script pack against pinned Git blobs, without checking out live files."""
from pathlib import Path
import argparse,hashlib,json,re,subprocess,zipfile

SKIP={'COLLATION_CONFLICTS.txt','COLLATION_MANIFEST.json'}
ASSETS={'bin/home/main/waypoints/':'waypoints/','bin/home/main/mp/':'mp/'}

def archive_files(repo,commit,paths=()):
    assert re.fullmatch('[0-9a-f]{40}',commit),'pin a complete source commit'
    tree=subprocess.check_output(['git','ls-tree','-rz','--full-tree',commit,'--',*paths],cwd=repo)
    entries=[]
    for item in tree.split(b'\0'):
        if not item:continue
        header,name=item.split(b'\t',1);mode,kind,oid=header.split()
        assert mode in (b'100644',b'100755') and kind==b'blob','source contains a non-regular Git entry'
        entries.append((name.decode('utf-8'),oid))
    # git archive can apply export/EOL conversion. Read stored blob bytes so
    # host autocrlf settings and working-tree edits cannot change provenance.
    result=subprocess.run(['git','cat-file','--batch'],cwd=repo,input=b''.join(oid+b'\n' for _,oid in entries),
        capture_output=True,check=True).stdout
    offset=0;files={}
    for name,oid in entries:
        end=result.index(b'\n',offset);actual,kind,size=result[offset:end].split();length=int(size)
        assert actual==oid and kind==b'blob'
        offset=end+1;files[name]=result[offset:offset+length];offset+=length
        assert result[offset:offset+1]==b'\n';offset+=1
    assert offset==len(result)
    return files

def expected_files(native,raw,native_commit,raw_commit,checkout_crlf=None):
    result={'scripts/'+name:data for name,data in archive_files(raw,raw_commit).items()
            if Path(name).name not in SKIP}
    for name,data in archive_files(native,native_commit,tuple(ASSETS)).items():
        prefix=next((p for p in ASSETS if name.startswith(p)),None)
        if prefix:result[ASSETS[prefix]+name[len(prefix):]]=data
    for name,digest in (checkout_crlf or {}).items():
        assert name.startswith(('waypoints/','mp/')) and name in result,'invalid asset EOL scope'
        data=result[name]
        assert hashlib.sha256(data).hexdigest()==digest and b'\r' not in data,'asset EOL source differs'
        result[name]=data.replace(b'\n',b'\r\n')
    return result

def verify(native,raw,profile,pack):
    native=Path(native);pack=Path(pack)
    manifest=json.loads((native/'native_profiles/manifest.json').read_text())
    selected=manifest['profiles'][profile]
    assert not pack.is_symlink() and hashlib.sha256(pack.read_bytes()).hexdigest()==selected['scripts_sha256'],'script pack differs from qualified artifact'
    commit=subprocess.check_output(['git','rev-parse','HEAD'],cwd=native,text=True).strip()
    expected=expected_files(native,raw,commit,selected['raw_commit'],selected.get('asset_checkout_crlf'))
    with zipfile.ZipFile(pack) as archive:
        names=archive.namelist()
        assert len(names)==len(set(names)) and set(names)==set(expected),'script/asset inventory differs from pinned source'
        for name,data in expected.items():
            assert archive.read(name)==data,'script/asset differs from pinned Git blob: '+name
    return dict(passed=True,profile=profile,native_commit=commit,raw_commit=selected['raw_commit'],
        scripts_sha256=selected['scripts_sha256'],files=len(expected),working_files_read=False,
        scripts_built_or_published=False,runtime_changed=False)

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--raw-repo',type=Path,required=True)
    parser.add_argument('--profile',choices=('normal','zombies'),required=True);parser.add_argument('--pack',type=Path,required=True)
    args=parser.parse_args();print(json.dumps(verify(Path(__file__).resolve().parents[1],args.raw_repo,args.profile,args.pack)))
