"""Actual C query codec versus independent immutable Python golden JSON."""
from pathlib import Path
import hashlib,json,subprocess
root=Path(__file__).resolve().parents[1];native=Path(r'E:\codol-p2p-native-adapter-20261104')
golden=Path(r'E:\codol-p2p-native-scope-evidence-20261104\vectors-v1.json')
out=Path(r'E:\codol-p2p-principal-observation-evidence-20261104\client-codec');out.mkdir(parents=True,exist_ok=True)
data=json.loads(golden.read_text());files=[]
for name,key in [('request','bootstrap_request'),('envelope','bootstrap_envelope'),('hello','hello')]:
 path=out/(name+'.bin');path.write_bytes(bytes.fromhex(data[key]));files.append(path)
for vector in data['vectors']:
 for kind in ('query','reply'):
  path=out/(vector['name']+'-'+kind+'.bin');path.write_bytes(bytes.fromhex(vector[kind]));files.append(path)
unix=lambda p:'/mnt/'+str(p)[0].lower()+str(p)[2:].replace('\\','/')
binary=out/'query-codec';reply=out/'c-reply.bin'
command=['wsl','--exec','gcc','-m32','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer',
 '-I'+unix(native/'src'),unix(root/'tests/test_p2p_native_query_wire.c'),unix(root/'src/p2p/p2p_active_wire.c'),
 unix(root/'src/p2p/p2p_startup_wire.c'),unix(native/'src/mbedtls/sha256.c'),unix(native/'src/mbedtls/platform_util.c'),'-o',unix(binary)]
results=[]
for args in [command,['wsl','--exec',unix(binary),*[unix(path)for path in files],unix(reply)]]:
 result=subprocess.run(args,capture_output=True,text=True,encoding='utf-8',errors='replace')
 results.append(dict(args=args,exit_code=result.returncode,output=result.stdout+result.stderr))
 print(result.stdout+result.stderr,end='',flush=True)
 if result.returncode:raise SystemExit(result.returncode)
assert reply.read_bytes()==bytes.fromhex(data['vectors'][0]['reply'])
receipt=dict(okay=True,scope=__doc__,golden_sha256=hashlib.sha256(golden.read_bytes()).hexdigest(),results=results,
 sources={name:hashlib.sha256((root/name).read_bytes()).hexdigest()for name in
 ['src/p2p/p2p_active_wire.c','src/p2p/p2p_active_wire.h','src/p2p/p2p_startup_wire.c','tests/test_p2p_native_query_wire.c','tests/test_p2p_native_query_wire.py']},
 actual_c_reply_sha256=hashlib.sha256(reply.read_bytes()).hexdigest())
(out/'query-codec-tests.json').write_text(json.dumps(receipt,indent=2)+'\n')
