"""Execute the production Present consumer against a fake device; no real D3D/window."""
import os
import pathlib
import subprocess

ROOT = pathlib.Path(__file__).resolve().parents[1]
s = (ROOT / 'console.c').read_text()
a = s.index('static void ConDraw(')
b = s.index('/* ---- developer HUD:', a)
prefix = r'''
#include <windows.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "../console_mailbox.h"
typedef struct {int dummy;} IDirect3DDevice9;
typedef struct {int dummy;} IDirect3DSurface9;
typedef struct {int dummy;} IDirect3DTexture9;
typedef struct {UINT Width,Height;} D3DSURFACE_DESC;
typedef struct {void *pBits;int Pitch;} D3DLOCKED_RECT;
static IDirect3DDevice9 device;
static IDirect3DSurface9 surface;
static IDirect3DTexture9 texture;
static IDirect3DDevice9 *g_texDev=&device;
static IDirect3DTexture9 *g_tex=&texture;
static int g_texW=320,g_texH=200,g_usedH,uploads;
static DWORD uploaded[320*200];
static HANDLE g_conWorkEvent;
static volatile LONG g_conOpen=1,g_inDraw,g_conDirty,g_conRequestedW,g_conRequestedH;
static ConRasterFrame g_conFrames[2];
static DWORD g_conUploadedSerial;
#define D3D_OK 0
#define D3DBACKBUFFER_TYPE_MONO 0
#define D3DFMT_A8R8G8B8 0
#define D3DPOOL_MANAGED 0
#define Log(...) ((void)0)
#define IDirect3DDevice9_TestCooperativeLevel(dev) 0
#define IDirect3DDevice9_GetBackBuffer(dev,a,b,c,out) (*(out)=&surface,0)
#define IDirect3DSurface9_Release(surface) ((void)0)
#define IDirect3DTexture9_Release(texture) ((void)0)
#define IDirect3DDevice9_Release(device) ((void)0)
#define IDirect3DDevice9_AddRef(device) ((void)0)
#define IDirect3DDevice9_CreateTexture(dev,w,h,a,b,c,d,out,e) (*(out)=&texture,0)
static void IDirect3DSurface9_GetDesc(IDirect3DSurface9 *p,D3DSURFACE_DESC *d){(void)p;d->Width=320;d->Height=200;}
static HRESULT IDirect3DTexture9_LockRect(IDirect3DTexture9 *p,int level,D3DLOCKED_RECT *lr,const RECT *r,int flags){(void)p;(void)level;(void)r;(void)flags;uploads++;lr->pBits=uploaded;lr->Pitch=320*4;return 0;}
#define IDirect3DTexture9_UnlockRect(texture,level) ((void)0)
#define ConBlit(...) 1
'''
main = r'''
int main(void){
    g_conWorkEvent=CreateEventA(NULL,FALSE,FALSE,NULL);assert(g_conWorkEvent);
    for(int test=0;test<3;test++){
        uploads=0;g_conUploadedSerial=0;g_usedH=0;
        for(int i=0;i<2;i++){
            ConRasterFrame *f=&g_conFrames[i];
            f->pixels=calloc(320*20,4);assert(f->pixels);
            f->serial=(test==1)?(2-i):(1+i);f->w=320;f->h=200;f->used=20;
            for(int p=0;p<320*20;p++)f->pixels[p]=f->serial;
            ConFramePublish(f);
        }
        if(test==2)g_conFrames[1].w=640; /* Newest frame is stale-size. */
        ConDraw(&device);
        assert(uploads==1);
        assert(g_conUploadedSerial==(test==2?1:2));
        assert(uploaded[0]==g_conUploadedSerial && g_usedH==20);
        for(int i=0;i<2;i++){assert(g_conFrames[i].state==CON_FREE);free(g_conFrames[i].pixels);}
    }
    CloseHandle(g_conWorkEvent);
    puts("production Present consumer PASS: one upload for either newest-slot order; wrong-size frame discarded");
    return 0;
}
'''
out = ROOT / 'obj'
source = out / 'test_console_draw.c'
source.write_text(prefix + s[a:b] + main)
gcc = pathlib.Path('C:/msys64/mingw32/bin/gcc.exe')
env = os.environ.copy()
env['PATH'] = str(gcc.parent) + ';' + env['PATH']
exe = out / 'test_console_draw.exe'
build = subprocess.run([str(gcc), '-O2', '-Wall', '-Wextra', str(source), '-o', str(exe)], env=env, capture_output=True, text=True)
if build.returncode:
    raise RuntimeError(build.stderr)
r = subprocess.run([str(exe)], env=env, check=True, capture_output=True, text=True, creationflags=subprocess.CREATE_NO_WINDOW)
print(r.stdout, end='')
