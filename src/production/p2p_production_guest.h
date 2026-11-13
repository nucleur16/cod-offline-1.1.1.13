#ifndef CODOL_P2P_PRODUCTION_GUEST_H
#define CODOL_P2P_PRODUCTION_GUEST_H
#include "p2p_production_wire.h"
#define P2PR_GUEST_SIZE 256u
/* CP2PGB02 +32 is the original LOCAL intent generation. CPRPv1's distinct
 * native generation remains 1; these fields are never interchangeable. */
/* Production binding receipt, not legacy private CPGB/retirement-event bytes.
 * Publisher must have verified the signed own install + genuine native ingress
 * and actual signed Host server endpoint. Parsing is NOT authority, Ready or a connect. */
int P2PRGuestParse(const unsigned char *,size_t,const p2pp_scope *,uint64_t own_uid,
                   uint64_t generation,uint64_t wall,uint64_t mono);
#endif
