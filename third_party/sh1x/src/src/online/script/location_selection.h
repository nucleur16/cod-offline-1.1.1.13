#ifndef LOCATION_SELECTION_H
#define LOCATION_SELECTION_H
#include "scr_vm.h"
void BeginLocationSelection(scr_entref_t ref);
void BeginLocationMortarSelection(scr_entref_t ref);
void EndLocationSelection(scr_entref_t ref);
struct gentity_s;
void ConfirmLocation(struct gentity_s *ent, const float *position);
int LocationSelectionAction(struct gentity_s *ent);
#endif
