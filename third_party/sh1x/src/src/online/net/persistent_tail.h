/* Verified concrete room owners; no transport or wallet reset. */
#ifndef PERSISTENT_TAIL_H
#define PERSISTENT_TAIL_H
#ifdef __cplusplus
extern "C" {
#endif
int PersistentTailVoiceEmpty(void);
void PersistentTailVoiceReset(void);
int PersistentTailOverheadEmpty(void);
void PersistentTailOverheadReset(void);
int PersistentTailFontScaleEmpty(void);
void PersistentTailFontScaleReset(void);
int PersistentTailObjectivesEmpty(void);
void PersistentTailObjectivesReset(void);
int PersistentTailModelScaleEmpty(void);
void PersistentTailModelScaleReset(void);
int PersistentTailShieldEmpty(void);
void PersistentTailShieldReset(void);
int PersistentTailActorStepEmpty(void);
void PersistentTailActorStepReset(void);
int PersistentTailActorScriptsEmpty(void);
void PersistentTailActorScriptsReset(void);
int PersistentTailUnstuckEmpty(void);
void PersistentTailUnstuckReset(void);
int PersistentTailRestEmpty(void);
void PersistentTailRestReset(void);
int PersistentTailLerpEmpty(void);
void PersistentTailLerpReset(void);
int PersistentTailSwayEmpty(void);
void PersistentTailSwayReset(void);
int PersistentTailBotEmpty(void);
void PersistentTailBotReset(void);
int PersistentTailOffhandEmpty(void);
void PersistentTailOffhandReset(void);
int PersistentTailNavEmpty(void);
void PersistentTailNavReset(void);
int PersistentTailAnimTablesEmpty(void);
void PersistentTailAnimTablesReset(void);
int PersistentTailPathNodesEmpty(void);
void PersistentTailPathNodesReset(void);
int PersistentTailVehicleNodesEmpty(void);
void PersistentTailVehicleNodesReset(void);
int PersistentTailAnimNotifyEmpty(void);
void PersistentTailAnimNotifyReset(void);
int PersistentTailCoopEmpty(void);
void PersistentTailCoopReset(void);
int PersistentTailArtilleryEmpty(void);
void PersistentTailArtilleryReset(void);
int PersistentTailNavCanReset(void);
int PersistentTailPathNodesCanRelease(void);
int PersistentTailVehicleNodesCanRelease(void);
int PersistentTailAnimTablesCanRelease(void);
int PersistentTailAnimNotifyQuiet(void);
int PersistentTailOwnersReset(void);
int PersistentTailOwnersEmpty(void);
unsigned PersistentTailFailure(void);
#ifdef __cplusplus
}
#endif
#endif
