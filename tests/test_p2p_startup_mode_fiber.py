#!/usr/bin/env python3
"""Actual v2 client fiber + native options APIs/provider/parser, i386 only.

OS/DTLS/fiber and engine cvar/map execution are explicit doubles. Existing
native renderer, ownership/scope APIs, three installers and observers execute.
No DLL/Windows/game process is loaded or launched.
"""
import argparse,ast,hashlib,importlib.util,json,os,re,struct,subprocess,tempfile
from pathlib import Path


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--native',type=Path,required=True)
    parser.add_argument('--vectors',type=Path,required=True)
    parser.add_argument('--evidence',type=Path,required=True)
    parser.add_argument('--native-libs',type=Path,required=True)
    args=parser.parse_args();root=Path(__file__).resolve().parents[1];native=args.native.resolve();out=args.evidence.resolve();out.mkdir(parents=True,exist_ok=True)
    inputs=[root/name for name in ('p2p.c','p2p_inproc.c','p2p_startup_bridge.c','p2p_startup_wire.c','p2p_startup_wire.h','p2p_active_bridge.c','p2p_active_wire.c','p2p_active_wire.h','tests/test_p2p_startup_fiber.c','tests/p2p_startup_mode_cases.inc','tests/p2p_active_fiber_scaffold.inc','tests/p2p_active_io_scaffold.inc')]
    inputs+=[native/name for name in ('src/win32/sys_host.c','src/codol/net/codol_quickplay.c','src/codol/net/codol_match_options.c','src/codol/net/codol_match_options.h','src/codol/net/codol_network.c','src/codol/net/codol_principal_binding.c','src/codol/script/codol_verified_sources.c','tests/test_codol_principal_clock.c','tests/run_codol_match_options.py')]
    input_hashes={str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs}
    spec=importlib.util.spec_from_file_location('actual_fiber_reader',root/'tests/test_p2p_startup_fiber.py')
    reader=importlib.util.module_from_spec(spec);spec.loader.exec_module(reader)
    def function(text,name):
        # Require the definition's opening brace before any semicolon. Several
        # real functions have forward declarations; a prototype is not a body.
        hit=re.search(r'^[^\n;/*]*\b'+re.escape(name)+r'\([^;{}]*\)\s*\{',text,re.M)
        assert hit,name
        start=hit.start();at=text.index('{',start)+1;depth=1
        while depth:depth+=(text[at]=='{')-(text[at]=='}');at+=1
        return text[start:at]+'\n'
    host=(native/'src/win32/sys_host.c').read_text();quick=(native/'src/codol/net/codol_quickplay.c').read_text()
    bridge=(root/'p2p_startup_bridge.c').read_text();client=(root/'p2p_inproc.c').read_text();commands=(root/'p2p.c').read_text();active=(root/'p2p_active_bridge.c').read_text()
    template=(root/'tests/test_p2p_startup_fiber.c').read_text().split('int main(int argc,char **argv)')[0]
    # These two functions are engine/Win32 test doubles in the sibling fixture,
    # not client/native authorization logic. V2 invokes the real native entry.
    template=template.replace(function(template,'fixture_init'),r'''
static int fixture_init(const char *args) {
    assert(fixture_fiber&&fixture_thread==11&&script_calls==1&&principal_calls==1);
    ++init_calls;return Sh1xHost_InitWithMatchOptions(args);
}
''').replace(function(template,'fixture_frame'),r'''
static int fixture_frame(void) {
    assert(fixture_fiber&&fixture_thread==11);return Sh1xHost_Frame();
}
''')
    template=template.replace("out[0]=protocol?'1':'0';","out[0]=(char)('0'+protocol);")
    template=template.replace('static char g_piArgs[32]="fixture";','static char g_piArgs[8192]="fixture";')
    template=template.replace('static int CODOL_HostMatchInitialize(void)','int CODOL_HostMatchInitialize(void)')
    template=template.replace('static BOOL CloseHandle(HANDLE handle)',(root/'tests/p2p_active_io_scaffold.inc').read_text()+'\nstatic BOOL CloseHandle(HANDLE handle)')
    template=template.replace('#include "actual_startup_declarations.inc"',(root/'tests/p2p_active_fiber_scaffold.inc').read_text()+'\n#include "actual_startup_declarations.inc"')
    template=template.replace('#include "codol_verified_sources.h"','#include "codol_verified_sources.h"\n#include "codol_match_options.h"\n#include "codol_quickplay.h"')
    # Reuse the explicit cvar/engine-double producer already reviewed with the
    # actual native subsystem; extract its constant text without executing its runner.
    tree=ast.parse((native/'tests/run_codol_match_options.py').read_text())
    prefix=next(ast.literal_eval(node.value) for node in tree.body if isinstance(node,ast.Assign) and any(isinstance(t,ast.Name) and t.id=='prefix' for t in node.targets))
    prefix=prefix.replace(function(prefix,'InterlockedExchange'),'').replace('fixture_init','fixture_engine_init').replace('init_calls','native_engine_calls')
    prefix=prefix.replace('fixture_set("sv_running","1")','fixture_set("sv_running","0")')
    prefix+=r'''
#define GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS 4
#define GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT 2
#define INFINITE UINT32_MAX
typedef const char *LPCSTR;
static int GetModuleHandleExA(unsigned flags,LPCSTR address,HMODULE *out){(void)flags;(void)address;*out=(void *)1;return 1;}
static unsigned GetModuleFileNameA(HMODULE module,char *out,unsigned size){(void)module;snprintf(out,size,"C:\\fixture\\sh1x_host.dll");return (unsigned)strlen(out);}
void Sys_SetExeFile(const char *p){(void)p;}
void Sys_SetBinaryPath(const char *p){(void)p;}
void Sys_SetExeFileShort(const char *p){(void)p;}
void Sys_InitThreadContext(void){}
void Q_strncpyz(char *out,const char *in,int size){snprintf(out,(size_t)size,"%s",in);}
static void Sleep(DWORD value){(void)value;assert(!"unexpected native helper exit");abort();}
int Sys_MainInit(char *line){int result=fixture_engine_init(line);return init_result?init_result:result;}
'''
    template=template.replace('#include "actual_startup_functions.inc"',prefix+'\n#include "actual_startup_functions.inc"')
    template+=(root/'tests/p2p_startup_mode_cases.inc').read_text()+r'''
void Com_Frame(void) {
    assert(fixture_fiber&&host_in_call==1);++frame_calls;
    fixture_set("sv_running","1");fixture_set("mapname",(const char *)g_piStartup.match_options+176);
    if(wrong_rules)fixture_set("sv_maxclients","17");
    if(late_frame_rule&&frame_calls==2)fixture_set("sv_maxclients","17");
    if(g_piQuit)host_state=2;
}
'''
    begin=bridge.index('typedef struct {');end=bridge.index('} P2PStartupBridge;',begin)+len('} P2PStartupBridge;')
    nb=host.index('static int sys_hosted;');ne=host.index('int Sys_IsHosted(',nb)
    ab=active.index('enum {');ae=active.index('static BOOL PiActiveClock',ab)
    declarations='#include "p2p_active_wire.h"\n'+bridge[begin:end]+'\n'+host[nb:ne]+'\n'+active[ab:ae]
    selected={};order=[]
    def add(source,names):
        for name in names:selected[name]=function(source,name);order.append(name)
    add((root/'p2p_active_wire.c').read_text(),['p2pa_u32','p2pa_u64'])
    add(bridge,['P2PSAtomicCAS','P2PSWallMillis','P2PStartupProtocolMode'])
    add(commands,['P2PStartupAttemptCurrent'])
    add(bridge,['P2PSParentAlive','P2PSRefuse','P2PSCurrent','P2PSClose','P2PSAcknowledge'])
    add(host,['CODOL_HostSourceControlAllowed','Sh1xHost_UseVerifiedScripts','Sh1xHost_InstallRelayPrincipals','Sh1xHost_InstallActiveRelayPrincipals','Sh1xHost_RelayPrincipalsCurrent','Sh1xHost_QuarantineRelayPrincipals','Sys_HostExit','Sh1xHost_Init','Sh1xHost_Frame','match_options_owner','match_options_read','match_options_snapshot'])
    add(quick,['training_options_read','verified_training_human_limit','training_human_limit','CODOL_TrainingHumanLimit'])
    add(host,['match_options_current','Sh1xHost_UseVerifiedMatchOptions','Sh1xHost_InitWithMatchOptions','Sh1xHost_ObserveMatchOptions','Sh1xHost_ObserveRelayPrincipals'])
    add(active,['PiActiveClock','PiActiveDeny','PiActiveBootstrapExpired','PiActivePeer','PiActivePermission','PiActiveIoCurrent','PiActiveTransfer','PiActiveBetweenFrames','PiActiveStagePacket','PiActiveMakeReply'])
    add(client,['PiStartupPrincipalCheck','PiStartupOptionsCheck','PiInstallStartup','PiAbortStartupWorker','PiServerFiber','P2PInprocUsed','P2PInprocPrincipalFresh','P2PInprocCancelStartup'])
    add(commands,['P2PStartupInvalidate'])
    clock=(native/'tests/test_codol_principal_clock.c').read_text().split('int main(void)')[0]
    cases=['success','script-fail','principal-fail','options-fail','options-two','options-scope','cancel-options','init-fail','wrong-rule','observe-two','late-frame-rule']
    # The independent codec vectors deliberately use generation3. A fresh
    # native component correctly requires initialgeneration1. Adapt only that
    # scope in BOTH CPRB/CPMO and recompute public framing hashes; no signature
    # or actual authenticated service claim is made by this native fixture.
    data=json.loads((args.vectors/'vectors.json').read_text());golden=next(c for c in data['cases'] if c['case']=='mp')
    q=(args.vectors/'mp.request.bin').read_bytes();p=bytearray((args.vectors/'mp.envelope.bin').read_bytes())
    assert hashlib.sha256(q).hexdigest()==golden['files']['request']['sha256'] and hashlib.sha256(p).hexdigest()==golden['files']['envelope']['sha256']
    sn,rn,mn=struct.unpack_from('<III',p,16);rp=192+sn;mp=rp+rn;struct.pack_into('<Q',p,rp+32,1);struct.pack_into('<Q',p,mp+104,1)
    p[64:96]=hashlib.sha256(p[192:rp]).digest();p[96:128]=hashlib.sha256(p[rp:mp]).digest();p[128:160]=hashlib.sha256(p[mp:]).digest()
    p[160:192]=hashlib.sha256(b'CODOL-P2P-STARTUP-V2\0'+q+struct.pack('<III',sn,rn,mn)+p[64:160]).digest()
    (out/'request.bin').write_bytes(q);(out/'envelope.bin').write_bytes(p)
    results=[];negatives=[];env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0')
    with tempfile.TemporaryDirectory(prefix='p2p-v2-fiber-') as tmp:
        scratch=Path(tmp);(scratch/'principal_clock_io.inc').write_text(clock);(scratch/'actual_startup_declarations.inc').write_text(declarations)
        (scratch/'fixture.c').write_text(template)
        includes=['-I'+str(root),'-I'+str(scratch),'-I'+str(native/'src'),'-I'+str(native/'tests')]+['-I'+str(p) for p in (native/'src/codol').iterdir() if p.is_dir()]
        binary=scratch/'mode-fiber';cmd=['gcc','-m32','-std=gnu11','-O1','-g','-D_GNU_SOURCE','-DCODOL_SERVER','-ffunction-sections','-fdata-sections','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie']+includes+[str(scratch/'fixture.c'),str(root/'p2p_startup_wire.c'),str(root/'p2p_active_wire.c'),str(native/'src/codol/net/codol_match_options.c'),str(native/'src/codol/net/codol_principal_binding.c'),str(native/'src/codol/script/codol_verified_sources.c'),'-L'+str(args.native_libs),'-lmbedcrypto','-pthread','-Wl,--gc-sections','-o',str(binary)]
        def compile_source(functions):
            text='\n'.join(functions[name] for name in order);(scratch/'actual_startup_functions.inc').write_text(text)
            run=subprocess.run(cmd,capture_output=True,text=True);(out/'compile.log').write_text(run.stdout+run.stderr);assert run.returncode==0,run.stderr
        compile_source(selected)
        for case in cases:
            run=subprocess.run([str(binary),case,str(out/'request.bin'),str(out/'envelope.bin')],capture_output=True,text=True,env=env)
            results.append({'case':case,'exit_code':run.returncode,'output':run.stdout+run.stderr});print(case,run.returncode,flush=True);assert run.returncode==0,run.stdout+run.stderr
        for name,case,function_name,old,new in [('missing-third-install','success','PiInstallStartup','g_piStartup.options_result=g_piStartupOptions(g_piStartup.match_options,sizeof(g_piStartup.match_options));','g_piStartup.options_result=1;'),('missing-rules-observe','wrong-rule','PiStartupOptionsCheck','g_piObserveOptions(g_piStartup.match_options,sizeof(g_piStartup.match_options))==1','1')]:
            mutant=dict(selected);assert mutant[function_name].count(old)==1;mutant[function_name]=mutant[function_name].replace(old,new);compile_source(mutant)
            run=subprocess.run([str(binary),case,str(out/'request.bin'),str(out/'envelope.bin')],capture_output=True,text=True,env=env);detected=run.returncode!=0 and 'Assertion' in run.stderr
            negatives.append({'name':name,'case':case,'detected':detected,'output':run.stdout+run.stderr});assert detected;print('detected',name,flush=True)
    assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest()==digest for p,digest in input_hashes.items()),'compiler input changed during qualification'
    report={'passed':True,'cases':results,'causal_negatives':negatives,'input_sha256':input_hashes,'function_sha256':{k:hashlib.sha256(v.encode()).hexdigest() for k,v in selected.items()},'fixture_scope':__doc__,'vector_scope':'Independent Python codec vector public generation3 adapted to native initialgeneration1 with exact paired CPMO. Not an authenticated service fixture.'}
    (out/'receipt.json').write_text(json.dumps(report,indent=2)+'\n')


if __name__=='__main__':main()
