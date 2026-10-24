/* Included by kernel33.c. Exact C1 1.1.1.13 native official-news refresh.
 * Install only after unpacking, alongside the other exact-image patches.
 * All operations execute from the existing frontend news frame call; no thread,
 * endpoint, locale override, AS task fabrication, dvar, console command or UI
 * feature is introduced. See OWNERSHIP.md and integration.patch.txt. */
#include "news_refresh_policy.h"
#include "news_refresh_guards.h"
#define NEWS_FRAME_VA 0x005BC9E0u
#define NEWS_FRAME_CALL 0x006B67AAu
#define NEWS_MANAGER_VA 0x06C1B29Cu
#define NEWS_FREE_VA 0x00ADC0B8u
static NRClock g_newsRefreshClock;
static int NewsReadable(void *unused,uint32_t a,uint32_t n){
    (void)unused;return a&&Readable((void *)(DWORD_PTR)a,n);
}
static uint32_t NewsRead32(void *unused,uint32_t a){(void)unused;return *(volatile uint32_t *)(DWORD_PTR)a;}
static unsigned NewsRead8(void *unused,uint32_t a){(void)unused;return *(volatile unsigned char *)(DWORD_PTR)a;}
static void NewsWrite32(void *unused,uint32_t a,uint32_t v){(void)unused;*(volatile uint32_t *)(DWORD_PTR)a=v;}
static void NewsNativeStep(void *unused){(void)unused;((void(__cdecl *)(void))(DWORD_PTR)NEWS_FRAME_VA)();}
static void NewsNativeFree(void *unused,uint32_t a){
    (void)unused;((void(__cdecl *)(void *))(DWORD_PTR)NEWS_FREE_VA)((void *)(DWORD_PTR)a);
}
__attribute__((used,noinline)) void __cdecl NewsRefreshFrame(void){
    uint32_t m=*(volatile uint32_t *)(DWORD_PTR)NEWS_MANAGER_VA;
    NRPort p={NULL,NewsReadable,NewsRead32,NewsWrite32,NewsRead8,NewsNativeStep,NewsNativeFree};
    /* Do not interpret another object as this measured manager. Native execution
     * is still passed through unchanged if this extra runtime guard fails. */
    if(m&&(!Readable((void *)(DWORD_PTR)m,0x498)||
           *(uint32_t *)(DWORD_PTR)m!=0x00C64364u||
           *(uint32_t *)(DWORD_PTR)(m+16)!=0x00BDE50Cu)){
        NewsNativeStep(NULL);return;
    }
    NRResult result=nr_frame(&p,&g_newsRefreshClock,m,GetTickCount());
    if(result.started)Log("news refresh: real native text request started");
    if(result.completed)Log("news refresh: native text cycle completed (%s)",result.replaced?"new body":"last good body retained");
}
static uint32_t NewsGuardHash(const BYTE *p,uint32_t n){
    uint32_t h=2166136261u;while(n--){h^=*p++;h*=16777619u;}return h;
}
static void PatchNewsRefresh(void){
    BYTE *call=(BYTE *)(DWORD_PTR)NEWS_FRAME_CALL; LONG before=(LONG)(NEWS_FRAME_VA-(NEWS_FRAME_CALL+5));
    LONG after=(LONG)((DWORD_PTR)NewsRefreshFrame-(NEWS_FRAME_CALL+5));
    if(!Readable(call,5)||call[0]!=0xE8||memcmp(call+1,&before,4)){
        Log("news refresh: exact native frame call differs; retained native behavior");return;
    }
    for(unsigned i=0;i<sizeof(g_newsGuards)/sizeof(g_newsGuards[0]);i++){
        const NewsGuard *g=&g_newsGuards[i];const BYTE *code=(const BYTE *)(DWORD_PTR)g->va;
        if(!Readable(code,g->length)||NewsGuardHash(code,g->length)!=g->hash){
            Log("news refresh: exact %s bytes differ; retained native behavior",g->name);return;
        }
    }
    WriteCode(call+1,&after,4);
    Log("news refresh: exact native frontend text lifecycle installed (60 seconds)");
}
