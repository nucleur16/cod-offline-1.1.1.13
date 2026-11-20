#ifndef SCRIPT_CLIENT_STATUS_COLOUR_H
#define SCRIPT_CLIENT_STATUS_COLOUR_H
#ifdef OL_SERVER
#include "scr_vm.h"
/* Explicit compatibility name, not a recovered original method name.
 * Exact C1 a5a7 player method tablec3b430, index63, handler51dbb0:
 * player receiver, no argument reads, writes client+359c=1. */
void Scr_EnableClientStatusColour(scr_entref_t ref);
#endif
#endif
