#ifndef SCRIPT_USEHINT_H
#define SCRIPT_USEHINT_H
#ifdef OL_SERVER
#include "scr_vm.h"
#ifdef __cplusplus
extern "C" {
#endif
struct gentity_s;
void Scr_ForceUseHintOn(scr_entref_t ref);
void Scr_ForceUseHintOff(scr_entref_t ref);
void Scr_SetHintString(scr_entref_t ref);
qboolean ForceUseHint_Apply(struct gentity_s *entity);
void ForceUseHint_ResetClient(int slot);
void ForceUseHint_ResetAll(void);
#ifdef __cplusplus
}
#endif
#endif
#endif
