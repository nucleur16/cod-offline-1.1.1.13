/* Eight-word hide-part masks for 253-bone DObjs; see hide_parts.h. */
#include "q_shared.h"
#include "entity.h"
#include "g_shared.h"
#include "dobj.h"
#include "entitystate_ext.h"
#include "hide_parts.h"
#include <string.h>

void __cdecl DObjSetHidePartBits(DObj *obj, const unsigned int *partBits);

void GetEntityHidePartBits(const struct gentity_s *ent, uint32_t bits[8])
{
    EntityStateHidePartBits(ent ? &ent->s : NULL, bits);
}

void StoreEntityHidePartBits(struct gentity_s *ent, const uint32_t bits[8])
{
    if (ent)
        EntityStateSetHidePartBits(&ent->s, bits);
}

void SetEntityPartHidden(struct gentity_s *ent, struct DObj_s *obj, unsigned int bone, int hidden)
{
    uint32_t bits[DOBJ_PART_BIT_WORDS];
    GetEntityHidePartBits(ent, bits);
    if (obj && bone < obj->numBones) {
        uint32_t bit = 0x80000000u >> (bone & 31);
        if (hidden)
            bits[bone >> 5] |= bit;
        else
            bits[bone >> 5] &= ~bit;
    }
    if (obj)
        DObjSetHidePartBits(obj, bits);
    StoreEntityHidePartBits(ent, bits);
}

void ShowAllEntityParts(struct gentity_s *ent, struct DObj_s *obj)
{
    uint32_t bits[DOBJ_PART_BIT_WORDS] = {0};
    if (obj)
        DObjSetHidePartBits(obj, bits);
    StoreEntityHidePartBits(ent, bits);
}

void DObjSetEntityHidePartBits(struct DObj_s *obj, const struct gentity_s *ent)
{
    uint32_t bits[DOBJ_PART_BIT_WORDS];
    if (!obj)
        return;
    GetEntityHidePartBits(ent, bits);
    DObjSetHidePartBits(obj, bits);
}

void DObjSetEntityNumHidePartBits(struct DObj_s *obj, int handle)
{
    if (handle < 0 || handle >= MAX_GENTITIES)
        return;
    DObjSetEntityHidePartBits(obj, &g_entities[handle]);
}

void CopyEntityHidePartBits(struct gentity_s *to, const struct gentity_s *from)
{
    uint32_t bits[DOBJ_PART_BIT_WORDS];
    GetEntityHidePartBits(from, bits);
    StoreEntityHidePartBits(to, bits);
}
