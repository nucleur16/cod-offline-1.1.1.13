/* Renewable identity/cleanup barrier for a continuously resident process.
 * Pure policy; engine observations must be produced by the owning game thread.
 * Caller must authenticate commands and validate the exact catalogue/profile.
 * SPDX-License-Identifier: AGPL-3.0-or-later */
#include "persistent_policy.h"
#include <limits.h>
#include <string.h>

static int hex(const char *value, unsigned size)
{
    unsigned i, nonzero=0;
    if (!value || strlen(value)!=size) return 0;
    for (i=0;i<size;++i) {
        char c=value[i];
        if (!((c>='0' && c<='9') || (c>='a' && c<='f'))) return 0;
        nonzero|=c!='0';
    }
    return nonzero!=0;
}

static int identity(const persistent_policy_t *s, const persistent_identity_t *id)
{
    return id && hex(id->boot,32) && hex(id->lease,32) && id->epoch &&
        !strcmp(s->boot,id->boot) && !strcmp(s->lease,id->lease) && s->epoch==id->epoch;
}

static int empty(const persistent_observation_t *o)
{ return o && o->known==1 && o->humans==0; }

static int clean(const persistent_observation_t *o)
{ return empty(o) && o->bots==0 && o->world_ready==1; }

int PersistentInit(persistent_policy_t *s, const char *boot)
{
    memset(s,0,sizeof(*s));
    if (!boot || !*boot) return 1;
    if (!hex(boot,32)) return 0;
    memcpy(s->boot,boot,33);s->phase=PS_BOOTING;
    return 1;
}

int PersistentBootClean(persistent_policy_t *s, const persistent_observation_t *o)
{
    if (s->phase!=PS_BOOTING || !clean(o)) return 0;
    s->phase=PS_IDLE;return 1;
}

int PersistentBootVacant(persistent_policy_t *s, const persistent_observation_t *o, int virgin)
{
    if (s->phase!=PS_BOOTING || s->epoch || virgin!=1 ||
        !empty(o) || o->bots || o->world_ready) return 0;
    s->phase=PS_IDLE;return 1;
}

int PersistentBind(persistent_policy_t *s, const persistent_identity_t *id,
    const char *selection, const persistent_observation_t *o)
{
    if (!id || !hex(id->boot,32) || strcmp(s->boot,id->boot) || !hex(id->lease,32) ||
        !hex(selection,64)) return 0;
    if (identity(s,id) && !strcmp(s->selection,selection) &&
        (s->phase==PS_CONFIGURING || s->phase==PS_READY || s->phase==PS_ACTIVE)) return 2;
    if (s->phase!=PS_IDLE || !empty(o) || o->bots || s->epoch==UINT64_MAX ||
        id->epoch!=s->epoch+1) return 0;
    s->epoch=id->epoch;memcpy(s->lease,id->lease,33);memcpy(s->selection,selection,65);
    memset(s->report,0,sizeof(s->report));memset(s->members,0,sizeof(s->members));
    s->member_count=0;s->expiry=0;s->phase=PS_CONFIGURING;
    return 1;
}

int PersistentWorldReady(persistent_policy_t *s, const persistent_identity_t *id,
    const char *actual, const persistent_observation_t *o)
{
    if (!identity(s,id) || !hex(actual,64) || strcmp(s->selection,actual) ||
        !empty(o) || o->bots || o->world_ready!=1) return 0;
    if (s->phase==PS_READY) return 2;
    if (s->phase!=PS_CONFIGURING) return 0;
    s->phase=PS_READY;return 1;
}

int PersistentCancelIdle(persistent_policy_t *s,const persistent_identity_t *id,
    const persistent_observation_t *o)
{
    if(!id||!hex(id->boot,32)||strcmp(s->boot,id->boot)||!hex(id->lease,32)||
       s->phase!=PS_IDLE||s->epoch==UINT64_MAX||id->epoch!=s->epoch+1||
       !empty(o)||o->bots||!o->world_ready||s->member_count||s->expiry||s->lease[0]||s->report[0])return 0;
    s->epoch=id->epoch;
    memset(s->selection,0,sizeof(s->selection));
    return 1;
}

int PersistentGrant(persistent_policy_t *s, const persistent_identity_t *id,
    const uint64_t *members, unsigned count, uint64_t expiry, uint64_t now)
{
    unsigned i,j;
    if (!identity(s,id) || (s->phase!=PS_READY && s->phase!=PS_ACTIVE) ||
        !members || !count || count>18 || expiry<=now || expiry-now>120) return 0;
    for (i=0;i<count;++i) {
        if (!members[i]) return 0;
        for (j=0;j<i;++j) if (members[j]==members[i]) return 0;
    }
    memset(s->members,0,sizeof(s->members));memcpy(s->members,members,count*sizeof(*members));
    s->member_count=count;s->expiry=expiry;
    return 1;
}

int PersistentStart(persistent_policy_t *s, const persistent_identity_t *id,
    const persistent_observation_t *o, uint64_t now)
{
    if (!identity(s,id)) return 0;
    if (s->phase==PS_ACTIVE) return 2;
    if (s->phase!=PS_READY || !empty(o) || o->bots || o->world_ready!=1 ||
        !s->member_count || s->expiry<=now) return 0;
    s->phase=PS_ACTIVE;return 1;
}

int PersistentAdmit(const persistent_policy_t *s, const persistent_identity_t *id,
    uint64_t uid, uint64_t now)
{
    unsigned i;
    if (s->phase==PS_OFF) return 1; /* Additional gate only; normal auth still required. */
    if (s->phase!=PS_ACTIVE || !identity(s,id) || !uid || s->expiry<=now) return 0;
    for (i=0;i<s->member_count;++i) if (s->members[i]==uid) return 1;
    return 0;
}

int PersistentEnd(persistent_policy_t *s, const persistent_identity_t *id,
    enum persistent_end reason, const persistent_observation_t *o)
{
    if (!identity(s,id) || (reason!=PS_GAME_ENDED && reason!=PS_ABANDONED &&
        reason!=PS_CANCELLED)) return 0;
    if (s->phase==PS_FINISHING || s->phase==PS_RESETTING) return 2;
    if (reason==PS_GAME_ENDED && s->phase!=PS_ACTIVE) return 0;
    if (reason!=PS_GAME_ENDED && (!empty(o) ||
        (s->phase!=PS_CONFIGURING && s->phase!=PS_READY && s->phase!=PS_ACTIVE))) return 0;
    s->phase=PS_FINISHING;
    /* Close NEW admission immediately; connected clients are not disconnected. */
    s->member_count=0;s->expiry=0;memset(s->members,0,sizeof(s->members));
    return 1;
}

int PersistentReportDurable(persistent_policy_t *s, const persistent_identity_t *id,
    const char *report)
{
    if (!identity(s,id) || !hex(report,64) ||
        (s->phase!=PS_FINISHING && s->phase!=PS_RESETTING)) return 0;
    if (*s->report) return !strcmp(s->report,report) ? 2 : 0;
    memcpy(s->report,report,65);return 1;
}

int PersistentBeginReset(persistent_policy_t *s, const persistent_identity_t *id,
    const persistent_observation_t *o)
{
    if (!identity(s,id) || !empty(o) || !hex(s->report,64)) return 0;
    if (s->phase==PS_RESETTING) return 2;
    if (s->phase!=PS_FINISHING) return 0;
    s->phase=PS_RESETTING;return 1;
}

int PersistentResetComplete(persistent_policy_t *s, const persistent_identity_t *id,
    const persistent_observation_t *o)
{
    if (!id || !clean(o)) return 0;
    if (s->phase==PS_IDLE && hex(id->boot,32) && hex(id->lease,32) &&
        !strcmp(s->boot,id->boot) && !strcmp(s->completed_lease,id->lease) &&
        s->epoch==id->epoch) return 2;
    if (!identity(s,id) || s->phase!=PS_RESETTING) return 0;
    memcpy(s->completed_lease,s->lease,33);memset(s->lease,0,sizeof(s->lease));
    memset(s->report,0,sizeof(s->report));memset(s->members,0,sizeof(s->members));
    s->member_count=0;s->expiry=0;s->phase=PS_IDLE;
    return 1;
}

int PersistentSimulate(const persistent_policy_t *s, const persistent_observation_t *o)
{
    if (s->phase==PS_OFF || s->phase==PS_ACTIVE) return 1;
    if (!o || o->known!=1 || o->humans) return 1;
    return 0;
}
