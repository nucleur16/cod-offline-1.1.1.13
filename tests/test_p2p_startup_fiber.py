#!/usr/bin/env python3
"""Execute actual extracted startup fiber/bridge/API bodies, Linux i386 only.

No Windows process, game, native DLL or public socket is started. Native source
provider and principal parser are compiled directly. OS/fiber/engine calls use
explicit doubles; this is not Windows fiber or pipe-kernel qualification.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile


def function(text,name):
    hit=re.search(r'(?m)^(?:static |__declspec\(|void |int |unsigned |uint64_t )[^;\n]*\b'+re.escape(name)+r'\s*\(',text)
    if not hit:raise ValueError('actual definition missing: '+name)
    begin=hit.start();at=text.index('{',hit.start());depth=1;at+=1;state='code'
    while depth:
        c=text[at];n=text[at+1:at+2]
        if state=='line':
            if c=='\n':state='code'
        elif state=='comment':
            if c=='*'and n=='/':state='code';at+=1
        elif state in ('"',"'"):
            if c=='\\':at+=1
            elif c==state:state='code'
        elif c=='/'and n=='/':state='line';at+=1
        elif c=='/'and n=='*':state='comment';at+=1
        elif c in ('"',"'"):state=c
        elif c=='{':depth+=1
        elif c=='}':depth-=1
        at+=1
    return text[begin:at]+'\n'


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--native',type=Path,required=True)
    parser.add_argument('--evidence',type=Path,required=True)
    parser.add_argument('--vectors',type=Path,default=Path('/mnt/e/codol-p2p-startup-ipc-evidence-20261104/vectors-v1'))
    parser.add_argument('--native-libs',type=Path,default=Path('/mnt/c/Users/zz/Documents/ChatGPT/Call of Duty Online 1.1.1.13 Work/re/p2p-native-parity-20261104/lib-native'))
    args=parser.parse_args();root=Path(__file__).resolve().parents[1];native=args.native.resolve();out=args.evidence.resolve();out.mkdir(parents=True,exist_ok=True)
    inputs=[root/name for name in ('p2p.c','p2p_inproc.c','p2p_startup_bridge.c','p2p_startup_wire.c','p2p_startup_ownership.h')]
    inputs+=[native/name for name in ('src/win32/sys_host.c','src/codol/net/codol_network.c',
        'src/codol/net/codol_principal_binding.c','src/codol/script/codol_verified_sources.c')]
    before={str(path):hashlib.sha256(path.read_bytes()).hexdigest()for path in inputs}
    sources={name:(root/name).read_text() for name in ('p2p.c','p2p_inproc.c','p2p_startup_bridge.c')}
    native_host=(native/'src/win32/sys_host.c').read_text()
    run=function(sources['p2p_inproc.c'],'P2PInprocRun')
    assert run.index('InterlockedIncrement(&g_piSession)')<run.index('InterlockedExchange(&g_piState, PI_INIT_PENDING)')
    assert 'g_piSession = 0'not in run and 'g_piSession=0'not in run
    bridge=sources['p2p_startup_bridge.c'];start=bridge.index('typedef struct {');end=bridge.index('} P2PStartupBridge;',start)+len('} P2PStartupBridge;')
    globals_begin=native_host.index('static int sys_hosted;');globals_end=native_host.index('int Sys_IsHosted(',globals_begin)
    declarations=bridge[start:end]+'\n'+native_host[globals_begin:globals_end]
    selected={}
    for name in ('P2PSAtomicCAS','P2PSWallMillis','P2PStartupProtocolMode','P2PSParentAlive','P2PSRefuse','P2PSCurrent','P2PSClose','P2PSAcknowledge'):
        selected[name]=function(bridge,name)
    selected['P2PStartupAttemptCurrent']=function(sources['p2p.c'],'P2PStartupAttemptCurrent')
    selected['P2PStartupInvalidate']=function(sources['p2p.c'],'P2PStartupInvalidate')
    for name in ('Sh1xHost_UseVerifiedScripts','Sh1xHost_InstallRelayPrincipals','Sh1xHost_RelayPrincipalsCurrent','Sh1xHost_QuarantineRelayPrincipals'):
        selected[name]=function(native_host,name)
    for name in ('PiStartupPrincipalCheck','PiInstallStartup','PiAbortStartupWorker','PiServerFiber','P2PInprocUsed','P2PInprocPrincipalFresh','P2PInprocCancelStartup'):
        selected[name]=function(sources['p2p_inproc.c'],name)
    order=['P2PSAtomicCAS','P2PSWallMillis','P2PStartupProtocolMode','P2PStartupAttemptCurrent','P2PSParentAlive','P2PSRefuse','P2PSCurrent','P2PSClose','P2PSAcknowledge',
           'Sh1xHost_UseVerifiedScripts','Sh1xHost_InstallRelayPrincipals','Sh1xHost_RelayPrincipalsCurrent','Sh1xHost_QuarantineRelayPrincipals',
           'PiStartupPrincipalCheck','PiInstallStartup','PiAbortStartupWorker','PiServerFiber','P2PInprocUsed','P2PInprocPrincipalFresh',
           'P2PInprocCancelStartup','P2PStartupInvalidate']
    clock=(native/'tests/test_codol_principal_clock.c').read_text();clock=clock[:clock.index('int main(void)')]
    results=[];okay=True;environment=dict(os.environ,ASAN_OPTIONS='detect_leaks=0')
    def execute(command,expected=0):
        result=subprocess.run(command,env=environment,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
        results.append(dict(command=command,exit_code=result.returncode,expected_exit=expected,output=result.stdout))
        print(result.stdout,end='',flush=True)
        return result.returncode==expected
    with tempfile.TemporaryDirectory(prefix='codol-startup-fiber-')as temp:
        scratch=Path(temp)
        (scratch/'actual_startup_declarations.inc').write_text(declarations)
        functions='\n'.join(selected[name]for name in order)
        (out/'actual_startup_functions.inc').write_text(functions)
        (out/'actual_startup_declarations.inc').write_text(declarations)
        (scratch/'actual_startup_functions.inc').write_text(functions)
        (scratch/'principal_clock_io.inc').write_text(clock)
        includes=['-I'+str(scratch),'-I'+str(root),'-I'+str(native/'src'),'-I'+str(native/'tests')]
        includes+=['-I'+str(path)for path in sorted((native/'src/codol').iterdir())if path.is_dir()]
        binary=scratch/'startup-fiber'
        command=['gcc','-m32','-std=gnu11','-O1','-g','-D_GNU_SOURCE','-DCODOL_SERVER',
            '-ffunction-sections','-fdata-sections','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie']+includes+[
            str(root/'tests/test_p2p_startup_fiber.c'),str(root/'p2p_startup_wire.c'),
            str(native/'src/codol/net/codol_principal_binding.c'),str(native/'src/codol/script/codol_verified_sources.c'),
            '-L'+str(args.native_libs),'-lmbedcrypto','-Wl,--gc-sections','-o',str(binary)]
        okay=execute(command)
        scenarios=['success','script-fail','script-two','principal-fail','principal-two','cancel-before',
                   'cancel-script','cancel-principal','cancel-current','parent-current','deadline-current','pipe-pid-current',
                   'timeout-owned','cancel-wins','wrong-thread','runtime-expiry','wall-expiry','wall-expiry-latch','init-fail','cancel-yield']
        if okay:
            for scenario in scenarios:
                okay=execute([str(binary),scenario,str(args.vectors/'request.bin'),str(args.vectors/'envelope.bin')])and okay
        # A targeted negative control executes the same source after removing
        # only the accepted post-native Current freshness fix.
        current=selected['PiStartupPrincipalCheck']
        old=current.replace('&&\n        P2PSCurrent(&g_piStartup)','',1)
        assert old!=current
        refusal=selected['P2PSRefuse']
        no_latch=refusal.replace('if(bridge)InterlockedExchange((volatile LONG *)&bridge->aborted,1);','(void)bridge;',1)
        assert no_latch!=refusal
        controls=[('old-current','cancel-current',functions.replace(current,old,1)),
                  ('old-expiry-latch','wall-expiry-latch',functions.replace(refusal,no_latch,1))]
        for label,scenario,mutated in controls:
            (scratch/'actual_startup_functions.inc').write_text(mutated)
            negative=scratch/('startup-fiber-'+label);bad_compile=command[:-1]+[str(negative)]
            negative_compiled=execute(bad_compile);okay=okay and negative_compiled
            if negative_compiled:
                result=subprocess.run([str(negative),scenario,str(args.vectors/'request.bin'),str(args.vectors/'envelope.bin')],env=environment,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
                results.append(dict(negative_control=label,scenario=scenario,exit_code=result.returncode,output=result.stdout))
                okay=okay and result.returncode!=0
    paths=[root/name for name in sources]+[root/'tests/test_p2p_startup_fiber.py',root/'tests/test_p2p_startup_fiber.c',
        root/'p2p_startup_wire.c',root/'p2p_startup_ownership.h',native/'src/win32/sys_host.c',
        native/'src/codol/net/codol_network.c',native/'src/codol/net/codol_principal_binding.c',native/'src/codol/script/codol_verified_sources.c']
    report=dict(okay=okay,scope=__doc__,functions_sha256={name:hashlib.sha256(value.encode()).hexdigest()for name,value in selected.items()},
        source_files=[dict(path=str(path),sha256=hashlib.sha256(path.read_bytes()).hexdigest())for path in paths],
        scenarios=scenarios,results=results,limits=['Windows process/pipe/fiber I/O and engine Init/Frame are doubles',
            'native source provider/principal parser and extracted native owner exports are actual code',
            'g_piSession reservation established from actual Run source ordering; no full Run/DLL loader executed',
            'one initial HostMatchInitialize callback is a fixture seam, not a completed cleanup/rehost witness'])
    report['compiler_input_changes']=[str(path)for path in inputs if hashlib.sha256(path.read_bytes()).hexdigest()!=before[str(path)]]
    report['okay']=okay=okay and not report['compiler_input_changes']
    (out/'startup-fiber-tests.json').write_text(json.dumps(report,indent=2)+'\n')
    raise SystemExit(0 if okay else 1)


if __name__=='__main__':main()
