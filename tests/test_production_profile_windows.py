"""Run exact production/test-mode and command-file bodies in hidden owned fixtures.

The engine command sink is an explicit counter; no game or user UI is touched.
Requires Windows plus the existing WSL i686 cross compiler.
"""
from pathlib import Path
import argparse,hashlib,json,os,subprocess,time

ROOT=Path(__file__).resolve().parents[1]
def body(path,name):
    text=path.read_text();start=text.index('static '+('BOOL' if name.startswith('Ctl') else 'void')+' '+name+'(')
    opened=text.index('{',start);depth=1;end=opened+1
    while depth:
        if text[end]=='{':depth+=1
        elif text[end]=='}':depth-=1
        end+=1
    return text[start:end]
def linux(p):return '/mnt/'+p.drive[0].lower()+'/'+p.as_posix()[3:]
def main():
    p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    assert os.name=='nt' and not a.output.exists();a.output.mkdir(parents=True)
    header=ROOT/'production_profile.h'
    functions='\n'.join([body(ROOT/'controller.c','CtlTestMode'),body(ROOT/'controller.c','CtlTestCmdsEnabled'),body(ROOT/'voice.c','VoiceCmdFile')])
    source='''#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "PROFILE_HEADER"
#define Log(...) ((void)0)
static int dispatches;
static BOOL C1CbufAddText(const char *text) { if (strcmp(text,"owned fixture marker\\n")) ExitProcess(4); ++dispatches;return TRUE; }
FUNCTIONS
int main(void) {
 char path[MAX_PATH]; DWORD n=GetModuleFileNameA(0,path,MAX_PATH);
 while(n && path[n-1]!='\\\\')n--;
 _snprintf(path+n,MAX_PATH-n,"k33_cmd_%lu.txt",GetCurrentProcessId());
 FILE *f=fopen(path,"wb");if(!f)return 5;fputs("owned fixture marker\\n",f);fclose(f);
 VoiceCmdFile();
 BOOL exists=GetFileAttributesA(path)!=INVALID_FILE_ATTRIBUTES;
 int ok=CODOL_LOCAL_TESTING ? (CtlTestMode() && CtlTestCmdsEnabled() && dispatches==1 && !exists) :
                           (!CtlTestMode() && !CtlTestCmdsEnabled() && !dispatches && exists);
 DeleteFileA(path);if(!ok)return 6;
 puts("PASS: exact profile/command-file body boundary");return 0;
}
'''.replace('PROFILE_HEADER',linux(header.resolve())).replace('FUNCTIONS',functions)
    c=a.output/'profile_fixture.c';c.write_text(source);results=[]
    for name,flags,valid in [('shipping-default',[],True),('quiet-QA',['-DCODOL_LOCAL_TESTING=1','-DCODOL_DIAGNOSTICS=0'],True),
        ('development',['-DCODOL_LOCAL_TESTING=1'],True),('shipping-debug-forbidden',['-DCODOL_DIAGNOSTICS=1'],False),
        ('invalid-test-mode',['-DCODOL_LOCAL_TESTING=2'],False),('invalid-diagnostics',['-DCODOL_LOCAL_TESTING=1','-DCODOL_DIAGNOSTICS=2'],False)]:
        folder=a.output/name;folder.mkdir();exe=folder/'fixture.exe'
        r=subprocess.run(['wsl','-d','Ubuntu','--','i686-w64-mingw32-gcc','-O2','-Wall','-static',*flags,linux(c.resolve()),'-o',linux(exe.resolve())],capture_output=True,text=True,encoding='utf8',errors='replace',timeout=30)
        (folder/'compile.log').write_text(r.stdout+r.stderr,encoding='utf8')
        assert (r.returncode==0)==valid,name
        if valid:
            run=subprocess.run([str(exe),'k33_cmdFile','1','gpad_testcmds','1'],capture_output=True,text=True,timeout=10,creationflags=subprocess.CREATE_NO_WINDOW)
            assert run.returncode==0,run.stdout+run.stderr
            results.append(dict(case=name,compiled=True,hidden_actual_Windows_body_passed=True))
        else:results.append(dict(case=name,invalid_profile_compile_rejected=True))
    receipt=dict(at=time.time(),cases=results,source_capsule_sha256=hashlib.sha256(c.read_bytes()).hexdigest(),
                 exact_function_bodies=True,engine_command_sink='counter double',game_or_user_UI_touched=False)
    (a.output/'qualification.json').write_text(json.dumps(receipt,indent=2)+'\n')
    print(json.dumps(receipt))
if __name__=='__main__':main()
