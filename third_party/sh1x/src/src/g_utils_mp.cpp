#include "q_shared.h"
#include "entity.h"
#include "dobj.h"
#include "g_shared.h"
#include "server_public.h"

const char *entityTypeNames[] =
{
  "ET_GENERAL",
  "ET_PLAYER",
  "ET_PLAYER_CORPSE",
  "ET_ITEM",
  "ET_MISSILE",
  "ET_INVISIBLE",
  "ET_SCRIPTMOVER",
  "ET_SOUND_BLEND",
  "ET_FX",
  "ET_LOOP_FX",
  "ET_PRIMARY_LIGHT",
  "ET_MG42",
  "ET_HELICOPTER",
  "ET_PLANE",
  "ET_VEHICLE",
  "ET_VEHICLE_COLLMAP",
  "ET_VEHICLE_CORPSE",
#ifdef OL_SERVER
  "ET_VEHICLE_SPAWNER",
#endif
  "ET_ACTOR",
  "ET_ACTOR_SPAWNER",
  "ET_ACTOR_CORPSE"
#ifndef OL_SERVER
  , "ET_STREAMER_HINT"
#endif
};

extern "C"{

const char *__cdecl G_GetEntityTypeName(gentity_t *ent)
{
    assert((unsigned)ent->s.eType < sizeof(entityTypeNames)/sizeof(entityTypeNames[0])); //0x15

    return entityTypeNames[ent->s.eType];
}


#ifdef OL_SERVER
/* 1 when boneIndex names a bone of the entity's current server DObj. Used by
 * G_DObjGetLocalTagMatrix (_g_utils_mp.asm) before it indexes the matrix
 * array, for the same stale-index case G_DObjCalcBone now rejects. */
int __cdecl DObjBoneIndexValid(gentity_s *ent, int boneIndex)
{
  DObj_s *obj = Com_GetServerDObj(ent->s.number);
  return obj && boneIndex >= 0 && boneIndex < DObjNumBones(obj);
}
#endif

void __cdecl G_DObjCalcBone(gentity_s *ent, int boneIndex)
{
  void (__cdecl *controller)(gentity_s *, int *);
  DObj_s *obj;
  int partBits[10];

  obj = Com_GetServerDObj(ent->s.number);
  if ( obj )
  {
#ifdef OL_SERVER
    /* A bone index resolved against a model the entity has since swapped
     * (setModel straight after spawnTurret) can exceed the rebuilt DObj.
     * DObjGetHierarchyBits then walks parent links out of the skeleton:
     * access violation in turret_think_init (measured 2026-09-24). */
    if ( boneIndex < 0 || boneIndex >= DObjNumBones(obj) )
      return;
#endif
    if ( !SV_DObjCreateSkelForBone(obj, boneIndex) )
    {
      DObjGetHierarchyBits(obj, boneIndex, partBits);
      controller = entityHandlers[ent->handler].controller;
      if ( controller )
      {
        controller(ent, partBits);
      }
      DObjCalcSkel(obj, partBits);
    }
  }
}

#ifdef OL_SERVER
/* setModelScale (script_model_scale.c): C1 draws an entity's whole
 * pose scaled about its origin (cent pose scale from entityState +54,
 * 0x441532), so a tag of a scaled entity sits at origin + scale * the
 * unscaled local position. Every server tag position below follows the
 * drawn model; the tag's axes stay unit length. */
float EntityModelScale(int num);
extern int __cdecl SV_DObjGetBoneIndex(gentity_s *ent, unsigned int boneName);
extern DObjAnimMat *__cdecl SV_DObjGetMatrixArray(gentity_s *ent);

static void EntityTagAxis(gentity_s *ent, float ent_axis[4][3])
{
  AnglesToAxis(ent->r.currentAngles, ent_axis);
  ent_axis[3][0] = ent->r.currentOrigin[0];
  ent_axis[3][1] = ent->r.currentOrigin[1];
  ent_axis[3][2] = ent->r.currentOrigin[2];
}

static void EntityTagOrigin(gentity_s *ent, const DObjAnimMat *mat, float ent_axis[4][3], float *out)
{
  float scale = EntityModelScale(ent->s.number);
  float trans[3] = { mat->trans[0] * scale, mat->trans[1] * scale, mat->trans[2] * scale };
  MatrixTransformVector43(trans, ent_axis, out);
}

signed int __cdecl G_DObjGetWorldTagPos(gentity_s *ent, unsigned int tagName, float *pos)
{
  float ent_axis[4][3];
  DObjAnimMat *mats;
  int bone = SV_DObjGetBoneIndex(ent, tagName);
  if ( bone < 0 )
    return 0;
  G_DObjCalcBone(ent, bone);
  mats = SV_DObjGetMatrixArray(ent);
  if ( !mats )
    return 0;
  EntityTagAxis(ent, ent_axis);
  EntityTagOrigin(ent, &mats[bone], ent_axis, pos);
  return 1;
}

void __cdecl G_DObjGetWorldBoneIndexPos(gentity_s *ent, int boneIndex, float *pos)
{
  float ent_axis[4][3];
  DObjAnimMat *mats;
  G_DObjCalcBone(ent, boneIndex);
  mats = SV_DObjGetMatrixArray(ent);
  EntityTagAxis(ent, ent_axis);
  EntityTagOrigin(ent, &mats[boneIndex], ent_axis, pos);
}

void __cdecl G_DObjGetWorldBoneIndexMatrix(gentity_s *ent, int boneIndex, float (*tagMat)[3])
{
  float ent_axis[4][3];
  float axis[3][3];
  DObjAnimMat *mat;
  G_DObjCalcBone(ent, boneIndex);
  mat = &SV_DObjGetMatrixArray(ent)[boneIndex];
  EntityTagAxis(ent, ent_axis);
  ConvertQuatToMat(mat, axis);
  MatrixMultiply(axis, ent_axis, tagMat);
  EntityTagOrigin(ent, mat, ent_axis, &(*tagMat)[9]);
}
#endif

signed int __cdecl G_DObjGetWorldTagMatrix(struct gentity_s *ent, unsigned int tagName, float (*tagMat)[3])
{
  float ent_axis[4][3];
  DObjAnimMat *mat;
  float axis[3][3];

  mat = G_DObjGetLocalTagMatrix(ent, tagName);
  if ( !mat )
  {
    return 0;
  }
  AnglesToAxis(ent->r.currentAngles, ent_axis);
  ent_axis[3][0] = ent->r.currentOrigin[0];
  ent_axis[3][1] = ent->r.currentOrigin[1];
  ent_axis[3][2] = ent->r.currentOrigin[2];

  ConvertQuatToMat(mat, axis);
  MatrixMultiply(axis, ent_axis, tagMat);
#ifdef OL_SERVER
  EntityTagOrigin(ent, mat, ent_axis, &(*tagMat)[9]);
#else
  MatrixTransformVector43(mat->trans, ent_axis, &(*tagMat)[9]);
#endif
  return 1;
}

void __cdecl G_SetAngle(gentity_s *ent, const float *angle)
{
	assert(!IS_NAN(angle[0]) && !IS_NAN(angle[1]) && !IS_NAN(angle[2]));

  VectorCopy(angle, ent->s.lerp.apos.trBase);

  ent->s.lerp.apos.trType = TR_STATIONARY;
  ent->s.lerp.apos.trTime = 0;
  ent->s.lerp.apos.trDuration = 0;

  VectorClear(ent->s.lerp.apos.trDelta);
  VectorCopy(angle, ent->r.currentAngles);

}

}