#ifdef OL_SERVER
#include "persistent_runtime.h"
#endif
/* Script-owned weapon HUD icon overrides and their publication.
 * See hud_icon_overrides.h for the two-stage provenance, the publication
 * point and the lifetime. SPDX-License-Identifier: AGPL-3.0-or-later */
#include "hud_icon_overrides.h"
#include "playerstate_ext.h"
#include "sys_cod4defs.h"
#include <string.h>

/* Stage 1: the equivalent of IW4's client+0x3264 and C1's client+0x35ac. It is
 * script state of a client slot, not of a playerState, so it is keyed by client
 * number and is never reached by a playerState copy - which is exactly what
 * keeps a following spectator's own overrides intact. */
static uint16_t client_hud_icons[MAX_CLIENTS][HUD_ICON_CLIENT_SLOTS];

int SetClientHudIconOverride(unsigned client, unsigned slot, unsigned material)
{
    if (client >= MAX_CLIENTS || slot >= HUD_ICON_CLIENT_SLOTS) return 0;
    /* The index is into the retained CS_MATERIALS range; 0 is "no override".
     * An index outside it is a producer that did not use G_MaterialIndex, and
     * the publication could not carry it. */
    if (material >= (unsigned)MAX_MATERIALS) return 0;
    client_hud_icons[client][slot] = (uint16_t)material;
    return 1;
}

unsigned GetClientHudIconOverride(unsigned client, unsigned slot)
{
    if (client >= MAX_CLIENTS || slot >= HUD_ICON_CLIENT_SLOTS) return 0;
    return client_hud_icons[client][slot];
}

void PublishClientHudIconOverrides(unsigned client)
{
    if (client >= MAX_CLIENTS) return;
    /* IW4 0x005d80bc copies the whole array, unconditionally, every frame it
     * runs: the playerState half is a copy of the script half and nothing else
     * writes it, so there is no merge and no "changed" test here either. */
    PlayerStateExtSetClientHudIcons(client, client_hud_icons[client]);
}

void ResetClientHudIconOverrides(unsigned client)
{
    if (client >= MAX_CLIENTS) return;
    memset(client_hud_icons[client], 0, sizeof(client_hud_icons[client]));
}

void ResetAllHudIconOverrides(void)
{
    memset(client_hud_icons, 0, sizeof(client_hud_icons));
}

#ifdef OL_SERVER
#include "persistent_statics.h"

static int resident_static_zero(const void *data,size_t bytes) {
    const unsigned char *p=(const unsigned char*)data;
    for(size_t i=0;i<bytes;++i)if(p[i])return 0;
    return 1;
}

int PersistentHudIconsEmpty(void) {return resident_static_zero(client_hud_icons,sizeof(client_hud_icons));}
#endif
