"""Execute the actual dispatcher installer against the supported hook chains (32-bit GCC)."""
from pathlib import Path
import subprocess
import tempfile

source = (Path(__file__).resolve().parents[1] / 'k33tick.c').read_text()
start = source.index('static void K33InstallMainTicks(void)')
body_start = source.index('{', start)
depth = 1
end = body_start + 1
while depth:
    depth += (source[end] == '{') - (source[end] == '}')
    end += 1
installer = source[start:end]
harness = r'''
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
typedef unsigned char BYTE;
typedef uint32_t DWORD;
typedef int32_t LONG;
typedef uintptr_t DWORD_PTR;
static BYTE callBytes[5];
static DWORD g_k33TickNext;
static int g_k33TickCount = 4, redirected;
static void VOICE_TickHook(void) {}
static void DISCORD_TickHook(void) {}
static void PI_TickHook(void) {}
static void K33_MainTickHook(void) {}
#define VOICE_EVENTLOOP_CALL_VA ((DWORD)(uintptr_t)callBytes)
#define VOICE_EVENTLOOP_FUNC_VA 0x589B20u
#define Readable(p,n) 1
#define Log(...) ((void)0)
static void RedirectCall(DWORD at, void *target) {
    assert(at == VOICE_EVENTLOOP_CALL_VA && target == (void *)K33_MainTickHook);
    ++redirected;
}
'''
harness += installer
harness += r'''
int main(void) {
    DWORD targets[] = {VOICE_EVENTLOOP_FUNC_VA, (DWORD)(uintptr_t)VOICE_TickHook,
        (DWORD)(uintptr_t)DISCORD_TickHook, (DWORD)(uintptr_t)PI_TickHook, 0x12345678u};
    for (int i=0; i<5; ++i) {
        LONG rel=(LONG)(targets[i]-VOICE_EVENTLOOP_CALL_VA-5);
        callBytes[0]=0xE8; memcpy(callBytes+1,&rel,4);
        redirected=0;g_k33TickNext=0;
        K33InstallMainTicks();
        assert(redirected == (i<4));
        if(i<4) assert(g_k33TickNext==targets[i]);
    }
    callBytes[0]=0x90;redirected=0;K33InstallMainTicks();assert(!redirected);
    puts("PASS: actual tick installer retains stock/voice/Discord/P2P chains and rejects unknown targets");
    return 0;
}
'''
with tempfile.TemporaryDirectory() as tmp:
    path = Path(tmp)
    (path / 'test.c').write_text(harness)
    subprocess.run(['gcc', '-m32', '-no-pie', '-O0', str(path / 'test.c'), '-o', str(path / 'test')], check=True)
    subprocess.run([str(path / 'test')], check=True)
