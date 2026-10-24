"""Public-asset-free CI policy regression. Outputs only to explicit scratch."""
from pathlib import Path
import argparse,subprocess,hashlib,json
p=argparse.ArgumentParser();p.add_argument('--output-dir',type=Path,required=True);p.add_argument('--cc',default='cc');a=p.parse_args()
here=Path(__file__).resolve().parent;root=here.parents[2];out=a.output_dir.resolve();out.mkdir(parents=True,exist_ok=True)
binary=out/'gfx_date_policy_test'
subprocess.run([a.cc,'-std=c99','-O2','-Wall','-Wextra','-Werror','-I'+str(root),str(here/'policy_test.c'),'-o',str(binary)],check=True)
result=subprocess.run([str(binary)],check=True,text=True,capture_output=True)
pins={name:hashlib.sha256((root/name).read_bytes()).hexdigest() for name in ['gfx_date_time.c','gfx_date_time_policy.h','gfx_date_time_guards.h']}
proof={'passed':True,'policy':result.stdout.strip(),'source_sha256':pins,'boundary':'Compiled arithmetic policy only; exact-native and actual Windows rules are separate explicit-asset tests.'}
(out/'portable-result.json').write_text(json.dumps(proof,indent=2)+'\n');print(json.dumps(proof))
