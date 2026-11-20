#ifdef OL_SERVER
#include "persistent_runtime.h"
#endif
#ifdef OL_SERVER
#include "scr_vm_functions.h"
#include "g_hud.h"
#include "hud_fontscale.h"
#include <math.h>
#include <stdint.h>
#include <string.h>

static hud_fontscale_t scales[MAX_HUDELEMS];
/* C1 524970 accepts positive font sizes; the IW3 1.4..4.6 limits do not
 * apply. The wire serializer separately validates representable sizes. */
void HudSetFontScale(game_hudelem_t *hud,int offset)
{
    float value=Scr_GetFloat(0); (void)offset;
    if(!isfinite(value) || value<=0) { Scr_Error("HUD font scale must be finite and positive"); return; }
    hud->elem.fontScale=value;
}
static struct {
    const hudElemState_t *source;
    hud_fontscale_t scales[2*MAX_HUDELEMENTS];
} clients[MAX_CLIENTS];

static int slot(const void *p,const void *base,size_t stride,unsigned count)
{
    uintptr_t a=(uintptr_t)p,b=(uintptr_t)base;
    if(a<b || a-b>=stride*count || (a-b)%stride) return -1;
    return (int)((a-b)/stride);
}
void ResetHudFontScale(game_hudelem_t *hud)
{
    int i=slot(hud,g_hudelems,sizeof(*hud),MAX_HUDELEMS);
    if(i>=0) memset(&scales[i],0,sizeof(scales[i]));
}
void ResetHudFontScales(void)
{
    memset(scales,0,sizeof(scales)); memset(clients,0,sizeof(clients));
}
void CopyClientHudFontScale(gclient_t *client,int clientNum,hudelem_t *dest,const game_hudelem_t *hud)
{
    int i=slot(hud,g_hudelems,sizeof(*hud),MAX_HUDELEMS);
    int j=slot(dest,&client->ps.hud,sizeof(*dest),2*MAX_HUDELEMENTS);
    if(clientNum<0 || clientNum>=MAX_CLIENTS || i<0 || j<0) return;
    if(clients[clientNum].source!=&client->ps.hud) memset(&clients[clientNum],0,sizeof(clients[clientNum]));
    clients[clientNum].source=&client->ps.hud;
    clients[clientNum].scales[j]=scales[i];
}
int GetHudFontScale(const hudelem_t *hud,hud_fontscale_t *out)
{
    int i,j;
    if(!hud || !out) return 0;
    i=slot(hud,g_hudelems,sizeof(g_hudelems[0]),MAX_HUDELEMS);
    if(i>=0) { *out=scales[i]; return 1; }
    for(i=0;i<MAX_CLIENTS;++i) if(clients[i].source) {
        j=slot(hud,clients[i].source,sizeof(*hud),2*MAX_HUDELEMENTS);
        if(j>=0) { *out=clients[i].scales[j]; return 1; }
    }
    return 0;
}
void Scr_ChangeFontScaleOverTime(scr_entref_t ref)
{
    hud_fontscale_t next;
    float seconds,from; int64_t elapsed; game_hudelem_t *hud;
    if(ref.classnum!=1 || ref.entnum>=MAX_HUDELEMS || !g_hudelems[ref.entnum].elem.type) {
        Scr_ObjectError("changeFontScaleOverTime requires a HUD element"); return;
    }
    seconds=Scr_GetFloat(0);
    if(!isfinite(seconds) || seconds<=0 || seconds>60) {
        Scr_ParamError(0,"Font-scale duration must be greater than zero and at most 60 seconds"); return;
    }
    hud=&g_hudelems[ref.entnum]; next=scales[ref.entnum]; from=hud->elem.fontScale;
    if(!isfinite(from)) { Scr_ObjectError("HUD font scale must be finite"); return; }
    elapsed=(int64_t)level.time-next.start;
    if(next.duration>0 && elapsed<next.duration) {
        float fraction=elapsed<0 ? 0 : (float)elapsed/next.duration;
        from=next.from+(from-next.from)*fraction;
    }
    next.from=from; next.start=level.time;
    /* IW4 rounds the float milliseconds to nearest (x87 default ties-even). */
    next.duration=(int)nearbyint((double)(float)(seconds*1000.0)+0x1p-30);
    scales[ref.entnum]=next;
}
#endif

#ifdef OL_SERVER
#include "persistent_tail.h"
#include <string.h>
#include <limits.h>
static int resident_tail_zero(const void *data,size_t bytes){
 const unsigned char *p=(const unsigned char*)data;for(size_t i=0;i<bytes;++i)if(p[i])return 0;return 1;
}

int PersistentTailFontScaleEmpty(void){return resident_tail_zero(scales,sizeof(scales))&&resident_tail_zero(clients,sizeof(clients));}
void PersistentTailFontScaleReset(void){ResetHudFontScales();}
#endif
