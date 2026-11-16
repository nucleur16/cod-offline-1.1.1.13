"""Exercise the exact QA pointer command in an owned, never-visible window."""
from pathlib import Path
import argparse, hashlib, json, os, subprocess

ROOT=Path(__file__).resolve().parents[1]
def function(name):
    text=(ROOT/'src/controller/controller.c').read_text()
    start=text.index('static '+('BOOL ' if name=='CtlTestMode' else 'void __cdecl ')+name+'(')
    opened=text.index('{',start);end=opened+1;depth=1
    while depth:
        if text[end]=='{':depth+=1
        elif text[end]=='}':depth-=1
        end+=1
    return text[start:end]
def linux(path):return '/mnt/'+path.drive[0].lower()+'/'+path.as_posix()[3:]
def main():
    p=argparse.ArgumentParser();p.add_argument('--output',required=True,type=Path);a=p.parse_args()
    assert os.name=='nt' and not a.output.exists();a.output.mkdir(parents=True)
    source=r'''#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "PROFILE_HEADER"
static HWND owned;
#define CTL_MAIN_HWND_VA (&owned)
static POINT g_ctlCursor;
static BOOL g_ctlCursorFake;
static DWORD g_selfPostMs;
static const char *arguments[]={"gpad_cursor","123","234","click"};
static int argc=4, moves,lefts,rights,keys,prints;
static BOOL install=TRUE;
static int VoiceArgc(void){return argc;}
static const char *VoiceArgv(int n){return arguments[n];}
static BOOL CtlInstallTestFakes(void){return install;}
#define C1Printf(...) (++prints)
FUNCTIONS
static LRESULT CALLBACK handler(HWND h,UINT m,WPARAM w,LPARAM l){
 if(m==WM_KEYDOWN || m==WM_KEYUP){if(w!=VK_ESCAPE)ExitProcess(13);++keys;}
 if(m==WM_MOUSEMOVE || m==WM_LBUTTONDOWN || m==WM_LBUTTONUP || m==WM_RBUTTONDOWN || m==WM_RBUTTONUP){
  if(LOWORD(l)!=123 || HIWORD(l)!=234)ExitProcess(12);
  if(m==WM_MOUSEMOVE)++moves;
  if(m==WM_LBUTTONDOWN || m==WM_LBUTTONUP)++lefts;
  if(m==WM_RBUTTONDOWN || m==WM_RBUTTONUP)++rights;
 }
 return DefWindowProcA(h,m,w,l);
}
static void pump(void){MSG m;while(PeekMessageA(&m,0,0,0,PM_REMOVE)){TranslateMessage(&m);DispatchMessageA(&m);}}
int main(void){
 HWND foreground=GetForegroundWindow();
 WNDCLASSA c={0};c.lpfnWndProc=handler;c.hInstance=GetModuleHandleA(0);c.lpszClassName="OwnedHiddenQAPointer";
 if(!RegisterClassA(&c))return 2;
 owned=CreateWindowA(c.lpszClassName,"fixture",WS_OVERLAPPED,0,0,320,240,0,0,c.hInstance,0);
 if(!owned || IsWindowVisible(owned))return 3;
 CtlCmdCursor();pump();arguments[3]="rightclick";CtlCmdCursor();pump();argc=3;CtlCmdCursor();pump();
 CtlCmdEscape();pump();int enabled=CtlTestMode();
 if(enabled ? (moves!=3 || lefts!=2 || rights!=2 || !g_ctlCursorFake) : (moves || lefts || rights || g_ctlCursorFake))return 4;
 if(keys!=(enabled?2:0))return 7;
 int count=moves+lefts+rights;argc=4;install=FALSE;CtlCmdCursor();pump();
 if(moves+lefts+rights!=count)return 5;
 if(GetForegroundWindow()!=foreground || IsWindowVisible(owned))return 6;
 DestroyWindow(owned);puts("PASS: owned hidden QA pointer, shipping/launch guards, unchanged foreground");return 0;
}
'''.replace('PROFILE_HEADER',linux((ROOT/'src/production_profile.h').resolve())).replace('FUNCTIONS','\n'.join(function(n) for n in ('CtlTestMode','CtlCmdCursor','CtlCmdEscape')))
    path=a.output/'pointer.c';path.write_text(source)
    rows=[]
    for name,flags,args in (('shipping',[],['k33_cmdFile','1']),('quiet-QA',['-DK33_LOCAL_TESTING=1','-DK33_DIAGNOSTICS=0'],['k33_cmdFile','1']),('QA-without-launch-guard',['-DK33_LOCAL_TESTING=1','-DK33_DIAGNOSTICS=0'],[])):
        exe=a.output/(name+'.exe')
        build=subprocess.run(['wsl','-d','Ubuntu','--','i686-w64-mingw32-gcc','-O2','-Wall','-static',*flags,linux(path.resolve()),'-o',linux(exe.resolve()),'-luser32'],capture_output=True,text=True,timeout=30)
        (a.output/(name+'-build.log')).write_text(build.stdout+build.stderr)
        assert build.returncode==0,build.stderr
        run=subprocess.run([str(exe),*args],capture_output=True,text=True,timeout=10,creationflags=subprocess.CREATE_NO_WINDOW)
        assert run.returncode==0,(name,run.returncode,run.stdout,run.stderr)
        rows.append(dict(case=name,passed=True,output=run.stdout.strip()))
    receipt=dict(cases=rows,actual_hidden_Windows_window=True,exact_function_bodies=True,game_or_user_UI_touched=False,source_sha256=hashlib.sha256(path.read_bytes()).hexdigest())
    (a.output/'qualification.json').write_text(json.dumps(receipt,indent=2)+'\n');print(json.dumps(receipt))
if __name__=='__main__':main()
