#ifndef OBITUARY_H
#define OBITUARY_H
#include "entity.h"
void OL_GScr_Obituary(void);
/* Decode only this producer's captured, archived event state. */
int DecodeObituary(const entityState_t *,unsigned *victim,
                         unsigned *attacker,unsigned *death_code);
#endif
