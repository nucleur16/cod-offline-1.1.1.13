#ifndef SCRIPT_RANK_EXPERIENCE_H
#define SCRIPT_RANK_EXPERIENCE_H
#ifdef OL_SERVER
#include "scr_vm.h"
/* Explicit total-XP adapters; generic structured data access stays raw. */
void Scr_GetRankExperience(scr_entref_t);
void Scr_SetRankExperience(scr_entref_t);
#endif
#endif
