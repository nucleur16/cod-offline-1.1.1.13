"""Offline native warm-policy/roster/engine-boundary tests; never boots a server."""
from pathlib import Path
import argparse
import hashlib
import json
import os
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
if os.name == 'nt':
    def linux(path):
        return subprocess.check_output(['wsl.exe','--exec','wslpath','-a',str(path.resolve())],text=True).strip()
    raise SystemExit(subprocess.call(['wsl.exe','--exec','python3','-B',linux(Path(__file__)),
        '--output',linux(args.output)]))

out = args.output.resolve()
out.mkdir(parents=True, exist_ok=False)
includes = ['-I'+str(ROOT/'src')] + ['-I'+str(p) for p in sorted((ROOT/'src/online').iterdir()) if p.is_dir()]
flags = ['gcc','-m32','-msse2','-mfpmath=sse','-std=gnu11','-DOL_SERVER',
    '-D_GNU_SOURCE','-fno-pie','-no-pie','-ffunction-sections','-fdata-sections',*includes]
rows = []
sources = {}


def source(name):
    path = ROOT/name
    sources[name] = hashlib.sha256(path.read_bytes()).hexdigest()
    return str(path)


def run(name, command):
    process = subprocess.run(command, cwd=out, capture_output=True, text=True, timeout=60)
    log = process.stdout + process.stderr
    (out/(name+'.log')).write_text(log)
    row = dict(name=name, exit_code=process.returncode,
        log_sha256=hashlib.sha256(log.encode()).hexdigest(), tail=log[-2500:])
    rows.append(row)
    print(json.dumps(dict(name=name,exit_code=process.returncode,tail=log[-600:])),flush=True)
    return process.returncode == 0


policy = source('src/online/net/warm_policy.c')
adapter = source('src/online/net/warm_standby.c')
admission = source('src/online/net/admission.c')
policy_test = source('tests/test_warm_policy.c')
ticket_test = source('tests/test_warm_ticket.c')
engine_test = source('tests/native_profiles/test_warm_native.c')
passed = run('policy-build', [*flags,policy_test,policy,'-o',str(out/'policy')])
if passed:
    passed &= run('policy', [str(out/'policy')])

# Exercise real parser under ASan/UBSan, including file replacement/removal.
ticket_flags = ['gcc','-std=gnu11','-g','-O1','-fno-omit-frame-pointer',
    '-ffunction-sections','-fdata-sections','-fsanitize=address,undefined',
    '-DTEST_COARSE_STAT',*includes]
ok = run('ticket-build',[*ticket_flags,ticket_test,admission,'-Wl,--gc-sections','-o',str(out/'ticket')])
passed &= ok
if ok:
    passed &= run('ticket',[str(out/'ticket')])

main = (ROOT/'src/sv_main.c').read_text()
start = main.index('    if (PersistentRuntimeFrame() || WarmStandbyFrame()) {',main.index('qboolean SV_Frame('))
end = main.index('\n    }',start)+6
gate = main[start:end]
assert gate.index('sv.timeResidual=0;') < gate.index('NET_Sleep(')
assert start < main.index('    SV_PreFrame( );',start) < main.index('        G_RunFrame( svs.time );',start)
frame = out/'warm_frame_gate.c'
frame.write_text('#include "server.h"\n#include "qcommon.h"\n#include "sys_net.h"\n'
    '#include "warm_standby.h"\n#include "persistent_runtime.h"\nunsigned long long test_clock_stamp_us;\n'
    'int TestWarmFrameGate(void) {\n' + gate.replace('clock_stamp_us','test_clock_stamp_us') +
    '\nreturn qtrue;\n}\n')
ok = run('adapter-build',[*flags,engine_test,policy,adapter,str(frame),
    source('src/mbedtls/sha256.c'),source('src/mbedtls/platform_util.c'),
    '-Wl,--gc-sections','-o',str(out/'adapter')])
passed &= ok
if ok:
    for mode in range(4):
        passed &= run('adapter-'+str(mode),[str(out/'adapter'),str(mode)])

# Existing Quick paths use the actual disabled warm adapter, not a new stub.
ok = run('legacy-build',[*flags,source('tests/native_profiles/test_legacy_quick.c'),
    source('src/online/net/quickplay.c'),admission,
    source('src/online/net/match_status.c'),adapter,policy,
    '-Wl,--gc-sections','-o',str(out/'legacy')])
passed &= ok
if ok:
    passed &= run('legacy-quick',[str(out/'legacy')])

# Whole touched Linux translation units, including real SV_Frame/DirectConnect.
# UPDATE_PROXYSERVER_NAME is an unrelated required build-time configuration
# absent in a bare compiler call; this interface-only value is never executed.
for i,name in enumerate(('src/online/net/warm_policy.c','src/online/net/warm_standby.c',
    'src/online/net/admission.c','src/online/net/match_status.c',
    'src/online/net/quickplay.c','src/sv_main.c','src/sv_client.c','src/online/assets/asset_commands.c')):
    passed &= run('interface-'+str(i),[*flags,'-DUPDATE_PROXYSERVER_NAME="unused-test.invalid"',
        '-c',source(name),'-o',str(out/('interface-'+str(i)+'.o'))])

receipt = dict(passed=bool(passed),checks=rows,source_sha256=sources,
    frame_boundary_sha256=hashlib.sha256(frame.read_bytes()).hexdigest(),
    scope='Actual native policy, actual bounded roster parser with sanitizers, actual warm adapter and exact extracted pre-simulation frame gate; full touched Linux translation units compile. Engine globals/cvars/commands/NET_Sleep/ticket lookup are fixture providers in adapter test. Hash fixture wraps actual retained mbedTLS SHA256. Existing Quick source runs with the real warm adapter disabled.',
    native_server_or_client_started=False,screen_used=False,production_changed=False,
    full_native_link_or_boot=False,allocator_integrated=False,instant_loading_or_CPU_saving_proven=False)
(out/'qualification.json').write_text(json.dumps(receipt,indent=2)+'\n')
print(json.dumps(dict(passed=bool(passed),checks=len(rows),output=str(out))),flush=True)
raise SystemExit(0 if passed else 1)
