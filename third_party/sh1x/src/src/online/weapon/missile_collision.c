/* Native C1 sticky collision policy; never reads IW3 WeaponDef.stickiness. */
#ifdef OL_SERVER
#include "missile_stickiness.h"
#include "entity.h"
#include "cm_public.h"
#include "sentient.h"

int NativeMissileStickinessMode(const struct WeaponDef *definition)
{
    missile_stickiness_t metadata;
    if(!GetNativeWeaponMissileStickiness(definition,&metadata))return -1;
    return metadata.mode;
}
/* Low byte: C1 mode. Bit8: collision may settle against this receiver.
 * T5 BounceMissile separates ground/all modes and excludes sentients for
 * all-no-sentients. Resolve actual native target identity, not C1 offsets. */
int NativeMissileCollisionPolicy(const struct WeaponDef *definition,trace_t *trace)
{
    int mode=NativeMissileStickinessMode(definition),may_stop=1;
    unsigned target=(unsigned)(unsigned short)Trace_GetEntityHitId(trace);
    gentity_t *hit=target<ENTITYNUM_WORLD ? &g_entities[target] : NULL;
    if(mode<0)return 0;
    if(hit && hit->r.inuse) {
        int sentient=hit->s.eType==ET_PLAYER || Sentient_ForEntity(hit)!=NULL;
        if(sentient && mode!=STICK_NONE && mode!=STICK_ALL &&
           mode!=STICK_FLESH)may_stop=0;
        if(hit->s.eType==ET_MISSILE && mode!=STICK_GROUND_WITH_YAW)may_stop=0;
    }
    return mode|(may_stop?0x100:0);
}
#endif
