#ifdef OL_SERVER
#include "persistent_runtime.h"
#endif
/* SPDX-License-Identifier: AGPL-3.0-or-later */
#ifdef OL_SERVER
#include "spectate_defaults.h"
#include "g_shared.h"
#include "qcommon_io.h"
#include <string.h>

/* IW4 session vectors337c/3388, consumed by StopFollowing459e70. */
static struct { vec3_t origin, angles; } defaults[MAX_CLIENTS];
void Scr_SetSpectateDefaults(scr_entref_t ref)
{
    gentity_t *entity = VM_GetGEntityForEntRef(ref);
    unsigned client;
    if (!entity) return;
    if (!entity->client) {
        Scr_ObjectError(va("entity %i is not a player", entity->s.number));
        return;
    }
    client = (unsigned)entity->s.number;
    if (client >= MAX_CLIENTS) {
        Scr_ObjectError("player entity index is outside client storage");
        return;
    }
    /* Preserve IW4's sequential writes, including origin mutation if reading
     * argument1 subsequently throws. Extra arguments are ignored. */
    Scr_GetVector(0, defaults[client].origin);
    Scr_GetVector(1, defaults[client].angles);
}
void GetSpectateDefaults(unsigned client, float *origin, float *angles)
{
    if (client >= MAX_CLIENTS) return;
    memcpy(origin, defaults[client].origin, sizeof(vec3_t));
    memcpy(angles, defaults[client].angles, sizeof(vec3_t));
}
void ResetSpectateDefaults(unsigned client)
{
    if (client < MAX_CLIENTS) memset(defaults + client, 0, sizeof(defaults[client]));
}
void ResetAllSpectateDefaults(void)
{
    memset(defaults, 0, sizeof(defaults));
}
#endif

#ifdef OL_SERVER
#include "persistent_statics.h"

static int resident_static_zero(const void *data,size_t bytes) {
    const unsigned char *p=(const unsigned char*)data;
    for(size_t i=0;i<bytes;++i)if(p[i])return 0;
    return 1;
}

int PersistentSpectateDefaultsEmpty(void) {return resident_static_zero(defaults,sizeof(defaults));}
#endif
