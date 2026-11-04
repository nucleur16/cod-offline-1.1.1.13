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
    inputs += [root/name for name in ('p2p_active_bridge.c','p2p_active_wire.c','p2p_active_wire.h',
        'tests/p2p_active_fiber_cases.inc','tests/p2p_active_fiber_scaffold.inc','tests/p2p_active_io_scaffold.inc')]
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
    active=(root/'p2p_active_bridge.c').read_text()
    active_globals=active[active.index('enum {'):active.index('static BOOL PiActiveClock')]
    declarations='#include "p2p_active_wire.h"\n'+bridge[start:end]+'\n'+native_host[globals_begin:globals_end]+'\n'+active_globals
    selected={}
    for name in ('P2PSAtomicCAS','P2PSWallMillis','P2PStartupProtocolMode','P2PSParentAlive','P2PSRefuse','P2PSCurrent','P2PSClose','P2PSAcknowledge'):
        selected[name]=function(bridge,name)
    selected['P2PStartupAttemptCurrent']=function(sources['p2p.c'],'P2PStartupAttemptCurrent')
    selected['P2PStartupInvalidate']=function(sources['p2p.c'],'P2PStartupInvalidate')
    for name in ('Sh1xHost_UseVerifiedScripts','Sh1xHost_InstallRelayPrincipals','Sh1xHost_RelayPrincipalsCurrent','Sh1xHost_QuarantineRelayPrincipals'):
        selected[name]=function(native_host,name)
    for name in ('PiStartupPrincipalCheck','PiInstallStartup','PiAbortStartupWorker','PiServerFiber','P2PInprocUsed','P2PInprocPrincipalFresh','P2PInprocCancelStartup'):
        selected[name]=function(sources['p2p_inproc.c'],name)
    active_names=['PiActiveClock','PiActiveDeny','PiActiveBootstrapExpired','PiActivePeer','PiActivePermission','PiActiveIoCurrent','PiActiveTransfer','PiActiveBetweenFrames']
    for name in active_names:selected[name]=function(active,name)
    order=['P2PSAtomicCAS','P2PSWallMillis','P2PStartupProtocolMode','P2PStartupAttemptCurrent','P2PSParentAlive','P2PSRefuse','P2PSCurrent','P2PSClose','P2PSAcknowledge',
           'Sh1xHost_UseVerifiedScripts','Sh1xHost_InstallRelayPrincipals','Sh1xHost_RelayPrincipalsCurrent','Sh1xHost_QuarantineRelayPrincipals',
           'PiStartupPrincipalCheck','PiInstallStartup','PiAbortStartupWorker','PiServerFiber','P2PInprocUsed','P2PInprocPrincipalFresh',
           'P2PInprocCancelStartup','P2PStartupInvalidate']
    at=order.index('PiStartupPrincipalCheck');order[at:at]=active_names
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
        template=(root/'tests/test_p2p_startup_fiber.c').read_text()
        template=template.replace('static int CODOL_HostMatchInitialize(void)','int CODOL_HostMatchInitialize(void)')
        template=template.replace('static BOOL CloseHandle(HANDLE handle)',
            (root/'tests/p2p_active_io_scaffold.inc').read_text()+'\nstatic BOOL CloseHandle(HANDLE handle)')
        template=template.replace('assert(handle==(HANDLE)77);++closed;', 'assert(handle==(HANDLE)77);assert(!io_pending);++closed;')
        template=template.replace('#include "actual_startup_declarations.inc"',
            (root/'tests/p2p_active_fiber_scaffold.inc').read_text()+'\n#include "actual_startup_declarations.inc"')
        template=template.replace('int main(int argc,char **argv)',(root/'tests/p2p_active_fiber_cases.inc').read_text()+'\nint main(int argc,char **argv)')
        template=template.replace('int success=!strcmp(argv[1],"success")','int success=!strncmp(argv[1],"active-",7)||!strcmp(argv[1],"success")')
        template=template.replace('fixture_thread=22;assert(P2PSAcknowledge(&g_piStartup));',
            'assert(P2PACaptureBootstrap(&g_piStartup.view,&g_p2pa.boot));\n        fixture_thread=22;assert(P2PSAcknowledge(&g_piStartup));')
        template=template.replace('char *copy=NULL;size_t n=0;',
            'if(!strncmp(argv[1],"active-",7)){active_between_frame_case(argv[1]);return 0;}\n        char *copy=NULL;size_t n=0;')
        (scratch/'startup-fiber.c').write_text(template)
        command=['gcc','-m32','-std=gnu11','-O1','-g','-D_GNU_SOURCE','-DCODOL_SERVER',
            '-ffunction-sections','-fdata-sections','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie']+includes+[
            str(scratch/'startup-fiber.c'),str(root/'p2p_startup_wire.c'),str(root/'p2p_active_wire.c'),
            str(native/'src/codol/net/codol_principal_binding.c'),str(native/'src/codol/script/codol_verified_sources.c'),
            '-L'+str(args.native_libs),'-lmbedcrypto','-Wl,--gc-sections','-o',str(binary)]
        okay=execute(command)
        scenarios=['success','script-fail','script-two','principal-fail','principal-two','cancel-before',
                   'cancel-script','cancel-principal','cancel-current','parent-current','deadline-current','pipe-pid-current',
                   'timeout-owned','cancel-wins','wrong-thread','runtime-expiry','wall-expiry','wall-expiry-latch','init-fail','cancel-yield']
        scenarios+=['active-success','active-abort-owned','active-native-two','active-expired-bootstrap','active-parent-loss']
        scenarios+=['active-install-budget','active-commit-bootstrap-boundary']
        scenarios+=['active-io-success','active-io-timeout','active-io-parent','active-io-foreign']
        if okay:
            for scenario in scenarios:
                okay=execute([str(binary),scenario,str(args.vectors/'request.bin'),str(args.vectors/'envelope.bin')])and okay
        # A targeted negative control executes the same source after removing
        # only the accepted post-native Current freshness fix.
        current=selected['PiStartupPrincipalCheck']
        old=current.replace('&&\n        PiActivePermission()','',1)
        assert old!=current
        refusal=selected['P2PSRefuse']
        no_latch=refusal.replace('if(bridge)InterlockedExchange((volatile LONG *)&bridge->aborted,1);','(void)bridge;',1)
        assert no_latch!=refusal
        no_time_latches=functions.replace(refusal,no_latch,1).replace(selected['PiActiveBootstrapExpired'],
            'static void PiActiveBootstrapExpired(void) {}\n',1)
        controls=[('old-current','cancel-current',functions.replace(current,old,1)),
                  ('no-client-time-latches','wall-expiry-latch',no_time_latches)]
        promotion=selected['PiActiveBetweenFrames']
        late_promotion=promotion.replace('if(result&&first&&!PiActivePermission())result=0;','',1)
        assert late_promotion!=promotion
        controls.append(('late-bootstrap-promotion','active-commit-bootstrap-boundary',
                         functions.replace(promotion,late_promotion,1)))
        for label,scenario,mutated in controls:
            (scratch/'actual_startup_functions.inc').write_text(mutated)
            negative=scratch/('startup-fiber-'+label);bad_compile=command[:-1]+[str(negative)]
            negative_compiled=execute(bad_compile);okay=okay and negative_compiled
            if negative_compiled:
                result=subprocess.run([str(negative),scenario,str(args.vectors/'request.bin'),str(args.vectors/'envelope.bin')],env=environment,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
                results.append(dict(negative_control=label,scenario=scenario,exit_code=result.returncode,output=result.stdout))
                okay=okay and result.returncode!=0
        # Actual native install advances mono with flat wall. Recomputing lease
        # budgets at commit must fail the same between-Frame regression.
        (scratch/'actual_startup_functions.inc').write_text(functions)
        actual_wire=(root/'p2p_active_wire.c').read_text()
        late_wire=actual_wire.replace('policy->deadline=policy->offered_deadline;policy->hard_deadline=policy->offered_hard_deadline;',
            'policy->deadline=mono+control->expires-wall;policy->hard_deadline=mono+control->hard_expires-wall;',1)
        assert late_wire!=actual_wire
        (scratch/'late_active_wire.c').write_text(late_wire)
        bad_command=[str(scratch/'late_active_wire.c') if part==str(root/'p2p_active_wire.c') else part for part in command]
        bad_command[-1]=str(scratch/'active-late-install')
        late_compiled=execute(bad_command);okay=okay and late_compiled
        if late_compiled:
            result=subprocess.run([bad_command[-1],'active-install-budget',str(args.vectors/'request.bin'),str(args.vectors/'envelope.bin')],env=environment,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
            results.append(dict(negative_control='late-install-budget-refresh',exit_code=result.returncode,output=result.stdout))
            okay=okay and result.returncode!=0 and 'g_p2pa.policy.deadline==39000' in result.stdout
    paths=inputs+[root/'tests/test_p2p_startup_fiber.py',root/'tests/test_p2p_startup_fiber.c',
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
