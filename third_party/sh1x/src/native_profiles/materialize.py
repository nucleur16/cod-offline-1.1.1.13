"""Materialize a pinned native source profile into a NEW external directory.

No server is built/launched and no selector, account or runtime is modified.
The primary source tree is the normal profile; Zombies is an exact source delta.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def materialize(repo,output,category,profile,library_dir=None):
 repo=Path(repo).resolve();meta=repo/'native_profiles';m=json.loads((meta/'manifest.json').read_text())
 assert m['routing']=={'mp':'normal','coop':'normal','zm':'zombies'}
 assert category in m['routing'] and profile==m['routing'][category],'unsupported profile/category pair'
 output=Path(output).resolve();assert not output.exists(),'output must be new'
 assert not output.is_relative_to(repo),'generated inputs must stay outside Git'
 normal=json.loads((meta/'normal-code-files.json').read_text());selected=json.loads((meta/(profile+'-code-files.json')).read_text())
 for name,h in normal.items():
  p=repo/name;assert p.resolve().is_relative_to(repo) and not p.is_symlink() and sha(p)==h,('normal source drift',name)
 assert sha(meta/'zombies.patch')==m['zombies_delta_sha256'],'Zombies delta changed'
 output.mkdir(parents=True)
 for name in normal:
  dest=output/name;assert dest.resolve().is_relative_to(output);dest.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(repo/name,dest)
 if profile=='zombies':
  prefix=['git','-c','core.autocrlf=false','-c','core.eol=lf','apply']
  # apply is a filesystem operation here. If output is below an unrelated Git
  # checkout, repository discovery otherwise filters every patch path against
  # that checkout's prefix and can report success while silently skipping it.
  environment=dict(os.environ)
  for key in ('GIT_DIR','GIT_WORK_TREE','GIT_INDEX_FILE','GIT_PREFIX','GIT_COMMON_DIR'):
   environment.pop(key,None)
  environment['GIT_CEILING_DIRECTORIES']=str(output.parent)
  subprocess.run(prefix+['--check','--whitespace=nowarn',str(meta/'zombies.patch')],cwd=output,env=environment,check=True,capture_output=True)
  subprocess.run(prefix+['--whitespace=nowarn',str(meta/'zombies.patch')],cwd=output,env=environment,check=True,capture_output=True)
 actual={p.relative_to(output).as_posix() for p in output.rglob('*') if p.is_file()}
 assert actual==set(selected),'materialized native source inventory differs'
 assert all(sha(output/name)==h for name,h in selected.items()),'materialized native source hash differs'
 contract=m['profiles'][profile].get('resident_contract_profile_sha256')
 if contract:
  header=(output/'src/codol/net/codol_persistent_build.h').read_text()
  names=re.findall(r'^#define CODOL_RESIDENT_PROFILE_NAME "([a-z]+)"$',header,re.M)
  identities=re.findall(r'^#define CODOL_RESIDENT_PROFILE_SHA "([0-9a-f]{64})"$',header,re.M)
  assert names==[profile] and identities==[contract],'resident compiled contract identity differs'
 libraries={}
 if library_dir:
  library_dir=Path(library_dir).resolve();(output/'lib-native').mkdir()
  for name,h in m['native_libraries'].items():
   p=library_dir/name;assert not p.is_symlink() and sha(p)==h,('wrong native library',name)
   shutil.copyfile(p,output/'lib-native'/name);libraries[name]=h
 variables=dict(m['required_make_variables']);variables.update(m['profiles'][profile].get('required_make_variables',{}))
 receipt={'schema':'codol-native-source-materialization-v1','profile':profile,'category':category,'source_files':len(selected),'source_manifest_sha256':sha(meta/(profile+'-code-files.json')),'source_profile_manifest_sha256':sha(meta/'manifest.json'),'native_libraries':libraries,'tested_binary_sha256':m['profiles'][profile]['tested_native_sha256'],'prior_tested_binary_sha256':m['profiles'][profile].get('prior_tested_native_sha256'),'qualification_state':m['profiles'][profile].get('qualification_state','recorded-tested-inputs'),'required_make_variables':variables,'runtime_or_publication':False}
 if contract:receipt['resident_contract_profile_sha256']=contract
 if 'required_build_environment' in m:receipt['required_build_environment']=dict(m['required_build_environment'])
 (output/'SOURCE-RECEIPT.json').write_text(json.dumps(receipt,indent=2)+'\n')
 return receipt
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--category',required=True,choices=['mp','coop','zm']);p.add_argument('--profile',required=True,choices=['normal','zombies']);p.add_argument('--output',required=True);p.add_argument('--library-dir');a=p.parse_args()
 try:r=materialize(Path(__file__).resolve().parents[1],a.output,a.category,a.profile,a.library_dir)
 except (AssertionError,KeyError,ValueError,FileNotFoundError,subprocess.CalledProcessError) as e:p.exit(2,'REJECT: '+str(e)+'\n')
 print(json.dumps(r,indent=2))
if __name__=='__main__':main()
