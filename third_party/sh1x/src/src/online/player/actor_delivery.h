#ifndef ACTOR_DELIVERY_H
#define ACTOR_DELIVERY_H
#include "actor_anim_journal.h"
#include "entities.h"
#ifdef __cplusplus
extern "C" {
#endif
void ActorDeliveryReset(void);
void ActorDeliveryClear(unsigned client);
int ActorDeliveryArchive(int frame,unsigned time);
int ActorDeliveryCapture(unsigned client,unsigned sequence,int frame,unsigned time);
int ActorDeliveryPrepare(unsigned client,unsigned sequence,unsigned acknowledged,
 const entity_state_t *entities,unsigned count,animcmd_t *out,unsigned capacity,unsigned *written);
#ifdef __cplusplus
}
#endif
#endif
