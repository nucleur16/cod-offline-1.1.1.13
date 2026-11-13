"""Execute production GDI raster functions on an isolated worker, without a window."""
import os
import pathlib
import subprocess

ROOT = pathlib.Path(__file__).resolve().parents[1]
s = (ROOT / 'src/console/console.c').read_text()
render = s[s.index('static HDC g_dc;'):s.index('static BOOL ConTakeSnapshot(')]
suggest = s[s.index('typedef struct {\n    char name[80];'):s.index('} ConSuggest;') + len('} ConSuggest;')]
start = s.index('static const char *ConStrIStr(')
end = s.index('\n}', start) + 2
prefix = '''#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#define GAME_CODEPAGE 936
#define CON_LINE_LEN 320
#define CON_INPUT_LEN 256
#define CON_SHOWN 24
#define CON_RESULTS 12
static volatile LONG g_conRows;
static int failCreate;
static HBITMAP WINAPI TestCreateDIBSection(HDC dc, const BITMAPINFO *bi, UINT use, void **bits, HANDLE section, DWORD offset) {
    if(failCreate){failCreate=0;return NULL;}
    return CreateDIBSection(dc,bi,use,bits,section,offset);
}
#define CreateDIBSection TestCreateDIBSection
'''
main = r'''
static DWORD WINAPI raster(void *unused) {
    (void)unused;
    DWORD owner=GetCurrentThreadId(), started=GetTickCount();
    DWORD before=GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS);
    assert(ConEnsureDib(1280,720));
    failCreate=1;assert(!ConEnsureDib(1920,1080));
    assert(!g_dib && !g_bits && !g_dibW && !g_dibH);
    assert(ConEnsureDib(1280,720) && g_dib && g_bits);
    strcpy(g_snap.input,"rcon status");g_snap.cursor=11;g_snap.blink=1;
    g_snap.lineCount=120;g_snap.total=4096;g_snap.suggCount=24;g_snap.suggTotal=24;
    for(int i=0;i<120;i++)sprintf(g_snap.lines[i],"^3log %d ^7production text and colours",i);
    for(int i=0;i<24;i++){sprintf(g_snap.sugg[i].name,"command%d",i);strcpy(g_snap.sugg[i].value,"123");}
    DWORD warm = 0;
    for(int n=0;n<40;n++){
        int w=(n%2)?1920:1280,h=(n%2)?1080:720;
        g_snap.expanded=n%3==0;
        assert(ConEnsureDib(w,h));
        int used=ConRender(w,h);assert(used>0&&used<=h);
        size_t opaque=0;for(size_t i=0;i<(size_t)w*used;i++)opaque+=g_bits[i]>>24?1:0;
        assert(opaque>0);assert(GetCurrentThreadId()==owner);
        if(n==3)warm=GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS);
    }
    DWORD after=GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS);
    printf("GDI object delta: %lu -> %lu\n",(unsigned long)before,(unsigned long)after);
    assert(after==warm && after<=before+8);
    printf("actual production GDI PASS: 40 worker frames with resize/colours/alpha, %lu ms, GDI objects %lu -> %lu\n",(unsigned long)(GetTickCount()-started),(unsigned long)before,(unsigned long)after);
    return 0;
}
int main(void){HANDLE t=CreateThread(NULL,0,raster,NULL,0,NULL);assert(t);WaitForSingleObject(t,INFINITE);CloseHandle(t);return 0;}
'''
out = ROOT / 'obj'
out.mkdir(exist_ok=True)
source = out / 'test_console_raster.c'
source.write_text(prefix + suggest + '\n' + s[start:end] + '\n' + render + main)
gcc = pathlib.Path('C:/msys64/mingw32/bin/gcc.exe')
env = os.environ.copy()
env['PATH'] = str(gcc.parent) + ';' + env['PATH']
exe = out / 'test_console_raster.exe'
subprocess.run([str(gcc), '-O2', '-Wall', '-Wextra', str(source), '-lgdi32', '-o', str(exe)], env=env, check=True, capture_output=True)
r = subprocess.run([str(exe)], env=env, check=True, capture_output=True, text=True, creationflags=subprocess.CREATE_NO_WINDOW)
print(r.stdout, end='')
