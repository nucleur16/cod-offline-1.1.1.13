/* Native warm match: freeze simulation, not the process/network event loop.
 * No used game world is ever re-parked. SPDX-License-Identifier: AGPL-3.0-or-later */
#include "warm_policy.h"
#include "warm_standby.h"
#include "admission.h"
#include "server.h"
#include "cvar.h"
#include "cmd.h"
#include "qcommon_io.h"
#include "sec_crypto.h"
#include <stdio.h>
#include <string.h>

static warm_policy_t warm;
static int initialized;

/* Actual native settings, not a client-provided selection label. The worker
 * must compare the prepared selection and repeat this exact digest on claim.
 * A setting changing between its status poll and activation fails closed. */
static int selection_digest(char out[65])
{
    static const char *names[]={"mapname","sv_sessionMap","sv_zoneType",
        "g_gametype","sv_playlistId","g_difficulty","zombiesEndlessEnabled",
        "sv_quickPlay","quickParties","quickSession",
        "sv_mapRotation","sv_mapRotationCurrent","sv_randomMapRotation",
        "oneShotMatch","exitToLobby","g_allowVote",
        "sv_maxclients","sv_privateClients","sv_fps","scr_bots","scr_botfill",
        "scr_botmax","sv_botDifficulty","xpRatePercent","gpRatePercent","scr_xpscale"};
    char text[16384];
    size_t used=0;
    unsigned i;
    unsigned long size=65;
    for (i=0;i<sizeof(names)/sizeof(*names);++i) {
        const char *value=Cvar_GetVariantString(names[i]);
        int n=snprintf(text+used,sizeof(text)-used,"%s=%s\n",names[i],value ? value : "");
        if (n<0 || (size_t)n>=sizeof(text)-used) return 0;
        used+=(size_t)n;
    }
    if (!Sec_HashMemory(SEC_HASH_SHA256,text,used,out,&size,qfalse)) return 0;
    out[64]=0;
    return WarmHex(out,64);
}

static unsigned humans(void)
{
    unsigned count=0;
    int i, maximum=sv_maxclients ? sv_maxclients->integer : 0;
    if (maximum<0 || maximum>MAX_CLIENTS || (maximum && !svs.clients)) return 1;
    for (i=0;i<maximum;++i)
        if (svs.clients[i].state>=CS_CONNECTED &&
            svs.clients[i].netchan.remoteAddress.type!=NA_BOT) ++count;
    return count;
}

void WarmStandbyBeginMap(void)
{
    if (!initialized) {
        cvar_t *preparation=Cvar_RegisterString("warmPreparation","",CVAR_INIT,
            "Worker-owned one-use prepared-match identifier; empty disables warm standby");
        WarmPolicyInit(&warm,preparation ? preparation->string : "");
        initialized=1;
    }
    WarmPolicyBeginMap(&warm);
}

void WarmStandbyMapReady(void)
{
    char digest[65];
    if (warm.phase!=WARM_LOADING) return;
    WarmPolicyMapReady(&warm,selection_digest(digest) ? digest : "");
}

int WarmStandbyFrame(void)
{
    char digest[65];
    const char *current=NULL;
    if (warm.phase==WARM_OFF) return 0;
    if (warm.phase==WARM_READY && selection_digest(digest)) current=digest;
    return WarmPolicyFrame(&warm,current,humans());
}

int WarmStandbyUnassigned(void)
{ return warm.phase!=WARM_OFF && warm.phase!=WARM_ACTIVE; }

int WarmStandbyAdmit(uint64_t uid)
{
    if (warm.phase==WARM_OFF) return 1;
    return WarmPolicyAdmit(&warm) && uid &&
        AdmissionWarmTicket((unsigned)Cvar_VariableIntegerValue("net_port"),
            warm.preparation,warm.lease,uid);
}

const char *WarmStandbyStatus(void)
{
    static char text[256];
    snprintf(text,sizeof(text),"warm_phase=%u warm_preparation=%s warm_lease=%s warm_selection=%s",
        (unsigned)warm.phase,warm.preparation[0] ? warm.preparation : "-",
        warm.lease[0] ? warm.lease : "-",warm.selection[0] ? warm.selection : "-");
    return text;
}

static void activate(void)
{
    char digest[65];
    int result=0;
    if (Cmd_Argc()==4 && com_sv_running && com_sv_running->integer &&
        sv.state==SS_GAME && selection_digest(digest) && !strcmp(digest,Cmd_Argv(3)))
        result=WarmPolicyActivate(&warm,Cmd_Argv(1),Cmd_Argv(2),Cmd_Argv(3),humans());
    Com_Printf(CON_CHANNEL_DONT_FILTER,"sh1x_warm_activate accepted=%d %s\n",result,
        WarmStandbyStatus());
}

void WarmStandbyRegister(void)
{ Cmd_AddCommand("sh1x_warm_activate",activate); }
