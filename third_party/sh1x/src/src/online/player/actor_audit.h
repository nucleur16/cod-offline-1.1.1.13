#ifndef ACTOR_AUDIT_H
#define ACTOR_AUDIT_H
/* Developer probes for actor collision and weapon-fire publication
 * (actor_audit.c).  No structs: nothing here depends on packing. */
struct gentity_s;
struct entityState_s;
#ifdef __cplusplus
extern "C" {
#endif
int ActorAuditEnabled(void);
int TraceFireEventsEnabled(void);
void ActorAuditFrame(struct gentity_s *e);
void ActorAuditPlaced(struct gentity_s *e);
void TraceFireEventsEntity(struct gentity_s *e);
#ifdef __cplusplus
}
#endif
#endif
