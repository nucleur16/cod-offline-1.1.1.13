#ifndef CONST_MODELS_H
#define CONST_MODELS_H
#include "configstrings.h"
#pragma pack(push, 8)
#ifdef __cplusplus
extern "C" {
#endif
/* C1's constant model rows that name an xmodel no zone of their category
 * ships, each paired with the asset the category's zones carry in its place.
 * C1 fills its constant configstrings from mp/constConfigStrings_<category>.csv
 * (C1 0x612850 loads the table into 0x2a99a44) and CG_Init registers every
 * model row, so a row naming an absent xmodel becomes C1's default model
 * (Kernel33 parity log, zm_prototype_sh 2026-10-13: CS 1442, 1444, 1445).
 *
 * A gamestate value at a constant index replaces the constant: the client
 * applies the server's configstrings over the inherited table, and its
 * runtime "d" handler (C1 0x487593..0x487660) rebuilds the whole pool taking
 * the new value at the changed index, with no constant check. */
typedef int (*model_available_fn)(const char *name, void *context);
typedef void (*const_model_report_fn)(unsigned index, const char *from,
                                            const char *to, void *context);
/* For zone_type's rows: find the model slot whose inherited value is the
 * row's constant name and, when available() says the constant is absent and
 * the replacement present, publish the replacement as an override of that
 * slot. A slot already overridden is left alone. Returns the number of slots
 * overridden, -1 on invalid arguments or a failed override. */
int SubstituteConstModels(configstrings_t *overrides,
    const configstrings_t *inherited, const char *zone_type,
    model_available_fn available, void *context,
    const_model_report_fn report);
/* The same for the constant effect rows (CS_EFFECTS); available() then
 * answers for an effect name. */
int SubstituteConstEffects(configstrings_t *overrides,
    const configstrings_t *inherited, const char *zone_type,
    model_available_fn available, void *context,
    const_model_report_fn report);
#ifdef __cplusplus
}
#endif
#pragma pack(pop)
#endif
