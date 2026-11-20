#ifdef OL_SERVER
#include "persistent_runtime.h"
#endif
/* The lobby is the authority for paid Zombies revive tokens. Only its
 * password-protected local RCON receipt can complete an armed down. */
#ifdef OL_SERVER
#include "revive_token.h"
#include "connect.h"
#include "configstrings.h"
#include "ol_profile.h"
#include "script_laststand.h"
#include "server.h"
#include "g_shared.h"
#include "cmd.h"
#include "scr_vm_functions.h"
#include "cscr_stringlist.h"
#include "qcommon_io.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    uint64_t uid;
    int count;
    int armed_at;
    int armed;
    int spend_requested;
    uint64_t recent_receipts[8];
    unsigned next_receipt;
} token_client_t;
static token_client_t token_clients[MAX_GAME_CLIENTS];

static int hex16(const char *s,uint64_t *out)
{
    uint64_t value=0;
    unsigned i;
    if(!s || strlen(s)!=16 || !out)return 0;
    for(i=0;i<16;++i) {
        unsigned c=(unsigned char)s[i], digit;
        if(c>='0' && c<='9')digit=c-'0';
        else if(c>='a' && c<='f')digit=c-'a'+10;
        else return 0;
        value=(value<<4)|digit;
    }
    if(!value)return 0;
    *out=value;
    return 1;
}

void ResetReviveTokenClient(unsigned client)
{
    if(client<MAX_GAME_CLIENTS)
        memset(&token_clients[client],0,sizeof(token_clients[client]));
}

static int active_uid(unsigned slot,uint64_t uid)
{
    uint64_t secure_uid=0;
    return uid && slot<MAX_GAME_CLIENTS && sv_maxclients &&
        slot<(unsigned)sv_maxclients->integer &&
        svs.clients[slot].state==CS_ACTIVE &&
        svs.clients[slot].netchan.remoteAddress.type!=NA_BOT &&
        g_entities[slot].client && g_entities[slot].r.inuse &&
        ClientConnectIdentity(slot)==uid &&
        ClientSecureProfile(slot,&secure_uid) && secure_uid==uid;
}

static int player_slot(scr_entref_t ref)
{
    if(ref.classnum || ref.entnum>=MAX_GAME_CLIENTS ||
       !g_entities[ref.entnum].client) {
        Scr_ObjectError("Revive token requires a player");
        return -1;
    }
    return ref.entnum;
}

void Scr_GetReviveTokenCount(scr_entref_t ref)
{
    int slot=player_slot(ref);
    uint64_t uid;
    if(slot<0)return;
    uid=ClientConnectIdentity((unsigned)slot);
    Scr_AddInt(active_uid((unsigned)slot,uid) && token_clients[slot].uid==uid ?
        token_clients[slot].count : 0);
}

void Scr_ArmReviveToken(scr_entref_t ref)
{
    int slot=player_slot(ref);
    uint64_t uid;
    token_client_t *state;
    if(slot<0)return;
    uid=ClientConnectIdentity((unsigned)slot);
    if(!active_uid((unsigned)slot,uid) || !LastStand_IsActive(&g_entities[slot]) ||
       g_entities[slot].health<=0) { Scr_AddInt(0); return; }
    state=&token_clients[slot];
    if(state->uid!=uid) {
        memset(state,0,sizeof(*state));
        state->uid=uid;
    }
    state->armed_at=level.time;
    state->armed=1;
    state->spend_requested=0;
    Scr_AddInt(1);
}

void Scr_RequestReviveTokenSpend(scr_entref_t ref)
{
    int slot=player_slot(ref);
    token_client_t *state;
    uint64_t uid;
    if(slot<0)return;
    state=&token_clients[slot];
    uid=ClientConnectIdentity((unsigned)slot);
    if(!active_uid((unsigned)slot,uid) || !LastStand_IsActive(&g_entities[slot]) ||
       g_entities[slot].health<=0 || state->uid!=uid || !state->armed ||
       state->spend_requested || level.time<state->armed_at ||
       level.time-state->armed_at>60000) {
        Scr_AddInt(0);
        return;
    }
    state->spend_requested=1;
    SV_GameSendServerCommand(slot,1,"9 consume token");
    Scr_AddInt(1);
}

void Scr_ClearReviveToken(scr_entref_t ref)
{
    int slot=player_slot(ref);
    if(slot>=0) {
        token_clients[slot].armed=0;
        token_clients[slot].spend_requested=0;
    }
}

void TokenInventory_f(void)
{
    uint64_t uid;
    char *end;
    long count;
    int slot;
    if(Cmd_Argc()!=3 || !hex16(Cmd_Argv(1),&uid))return;
    count=strtol(Cmd_Argv(2),&end,10);
    if(*end || count<0 || count>100000)return;
    for(slot=0;sv_maxclients && slot<sv_maxclients->integer &&
        slot<MAX_GAME_CLIENTS;++slot)if(active_uid((unsigned)slot,uid)) {
        token_client_t *state=&token_clients[slot];
        if(state->uid!=uid) {
            memset(state,0,sizeof(*state));
            state->uid=uid;
        }
        state->count=(int)count;
    }
}

void TokenConsumed_f(void)
{
    uint64_t uid,receipt;
    int slot;
    if(Cmd_Argc()!=3 || !hex16(Cmd_Argv(1),&uid) ||
       !hex16(Cmd_Argv(2),&receipt))return;
    if(!com_sv_running || !com_sv_running->integer || sv.state!=SS_GAME)goto rejected;
    for(slot=0;sv_maxclients && slot<sv_maxclients->integer &&
        slot<MAX_GAME_CLIENTS;++slot)if(active_uid((unsigned)slot,uid)) {
        token_client_t *state=&token_clients[slot];
        unsigned i,notify;
        /* The lobby retries a receipt after an RCON reply is lost. A repeat
         * must acknowledge the original acceptance without a second revive. */
        if(state->uid==uid)
            for(i=0;i<8;++i)if(state->recent_receipts[i]==receipt)goto accepted;
        if(!LastStand_IsActive(&g_entities[slot]) || g_entities[slot].health<=0 ||
           state->uid!=uid || !state->armed || !state->spend_requested ||
           level.time<state->armed_at ||
           level.time-state->armed_at>60000)goto rejected;
        state->recent_receipts[state->next_receipt++%8]=receipt;
        state->armed=0; /* clear before script callbacks can run */
        state->spend_requested=0;
        if(state->count>0)--state->count;
        notify=SL_GetString("revive_token_consumed",0);
        Scr_Notify(&g_entities[slot],notify,0);
        SL_RemoveRefToString(notify);
        goto accepted;
    }
rejected:
    Com_Printf(CON_CHANNEL_DONT_FILTER,"token_result %s rejected\n",Cmd_Argv(2));
    return;
accepted:
    Com_Printf(CON_CHANNEL_DONT_FILTER,"token_result %s accepted\n",Cmd_Argv(2));
}
#endif

#ifdef OL_SERVER
#include "persistent_entities.h"
static int resident_entity_zero(const void *data,size_t bytes) {
    const unsigned char *p=(const unsigned char*)data;
    for(size_t i=0;i<bytes;++i)if(p[i])return 0;
    return 1;
}

int PersistentReviveTokensEmpty(void){return resident_entity_zero(token_clients,sizeof(token_clients));}
#endif
