/* Resolve live hint assets once; archived readers use the captured binding. */
#ifdef OL_SERVER
#include "g_shared.h"
#include "playerstate_ext.h"
#include "usehint_wire.h"
#include "sv_weapons.h"
#include "usercmd.h"
#include "knife_pickup.h"
#include "bg_public.h"
#include "weapon_slots.h"

void CaptureClientUseHint(unsigned client)
{
    gentity_t *ent;
    ps_ext_t *ext;
    int weapon;
    if(client >= MAX_CLIENTS || client >= (unsigned)level.maxclients) return;
    ent=&g_entities[client];
    if(!ent->r.inuse || !ent->client) return;
    ext=PlayerStateExt(&ent->client->ps);
    if(!ext) return;
    ext->use_hint_weapon=0;
    ext->throwback_weapon=0;
    if(ent->client->ps.cursorHint <= USEHINT_ICON_MAX) return;
    if(!RefreshServerWeaponMappings()) return;
    weapon=WeaponToWire((unsigned)(ent->client->ps.cursorHint-4));
    if(weapon>0 && weapon<=USEHINT_WEAPON_MAX)
        ext->use_hint_weapon=(uint16_t)(weapon+1);
    if(ext->use_hint_weapon && ent->client->ps.throwBackGrenadeTimeLeft>0 &&
       (unsigned)ent->client->ps.cursorHintEntIndex<ENTITYNUM_WORLD) {
        gentity_t *missile=&g_entities[ent->client->ps.cursorHintEntIndex];
        unsigned native_weapon=(unsigned)(ent->client->ps.cursorHint-4);
        const struct WeaponDef *definition;
        if(!missile->r.inuse || missile->s.eType!=ET_MISSILE ||
           ItemIndexWeapon(missile->s.index)!=native_weapon ||
           !native_weapon || native_weapon>=(unsigned)BG_GetNumWeapons())return;
        definition=BG_GetWeaponDef((int)native_weapon);
        /* C1 server50baed/50b9f9 accepts frag-class throwback only. */
        if(definition->offhandClass==OFFHAND_CLASS_FRAG_GRENADE &&
           !IsKnifeDefinition(definition))
            ext->throwback_weapon=(uint16_t)native_weapon;
    }
}
#endif
