#include "scr_vm.h"
#include "qcommon_io.h"

#ifdef OL_SERVER
/* Keep retained damage indices stable; C1 publication uses named MODs. */
uint16_t mod_gas_string;
/* 17: an explosive-bullet weapon's hit and splash (C1 0x505ee0 selects C1's
 * MOD 3 for a definition with +0xbcb set; bullet_explode.c). */
uint16_t mod_explosive_bullet_string;
#endif

uint16_t* modNames[] =
{
  &scr_const.MOD_UNKNOWN,
  &scr_const.MOD_PISTOL_BULLET,
  &scr_const.MOD_RIFLE_BULLET,
  &scr_const.MOD_GRENADE,
  &scr_const.MOD_GRENADE_SPLASH,
  &scr_const.MOD_PROJECTILE,
  &scr_const.MOD_PROJECTILE_SPLASH,
  &scr_const.MOD_MELEE,
  &scr_const.MOD_HEAD_SHOT,
  &scr_const.MOD_CRUSH,
  &scr_const.MOD_TELEFRAG,
  &scr_const.MOD_FALLING,
  &scr_const.MOD_SUICIDE,
  &scr_const.MOD_TRIGGER_HURT,
  &scr_const.MOD_EXPLOSIVE,
  &scr_const.MOD_IMPACT
#ifdef OL_SERVER
  , &mod_gas_string
  , &mod_explosive_bullet_string
#endif
};



