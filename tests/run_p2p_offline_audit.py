"""Bounded Linux/WSL checks, never executes a Windows fixture or client."""
from pathlib import Path
import json
import os
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[1]
OUTPUT=ROOT/'output-lifecycle-audit'
OUTPUT.mkdir(exist_ok=True)
results=[]
env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0')
def run(name,argv):
    p=subprocess.run(argv,capture_output=True,text=True,env=env)
    results.append(dict(name=name,command=argv,exit_code=p.returncode,stdout=p.stdout,stderr=p.stderr))
    if p.returncode: raise RuntimeError(name+' failed: '+p.stderr)
    print(name+': PASS')
try:
    with tempfile.TemporaryDirectory(prefix='p2p-bounded-audit-') as tmp:
        for fixture in ('test_p2p_match_options.c','test_p2p_status.c','bd_packet_guard_fixture.c','snapshot_guard_fixture.c','join_party_guard_fixture.c','challenge_sender_guard_fixture.c'):
            binary=str(Path(tmp)/fixture.removesuffix('.c'))
            run(fixture+' compile',['gcc','-m32','-std=gnu99','-O1','-g','-Wall','-Wextra','-Werror','-Wno-unused-parameter','-fno-pie','-no-pie','-fsanitize=address,undefined','-fno-omit-frame-pointer',str(ROOT/'tests'/fixture),'-o',binary])
            run(fixture+' run',[binary])
        run('actual inproc monitor lifecycle',['python3',str(ROOT/'tests'/'test_p2p_inproc_lifecycle.py')])
finally:
    (OUTPUT/'offline-results.json').write_text(json.dumps(results,indent=2)+'\n')
