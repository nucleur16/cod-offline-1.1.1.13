#ifndef NOTETRACK_EFFECTS_H
#define NOTETRACK_EFFECTS_H
#include "configstrings.h"
#pragma pack(push, 8)
#ifdef __cplusplus
extern "C" {
#endif
/* C1's actor notetrack effect table: CS 6134..6261, one row per
 * (notetrack, surface) as "notetrack;surface;tag;effect;soundprefix;soundsuffix;".
 * C1's server builtin 0x562dd0 writes the rows with "%s;%d;%s;%d;%s;%s;"
 * (0xc5c118); the client half 0x734530 walks 0x6d13304 down to 0x6d13108
 * (base 0x6d0d130), matches the notetrack an actor's animation hit and the
 * surface it traced under the tag (0x734210), and plays effect slot
 * 0x16d9c88[effect] (CS 3651 + effect) at the tag. Effect 0 plays the sound
 * alone. */
#define CS_NOTETRACK_EFFECTS 6134
#define NOTETRACK_EFFECT_SLOTS 128
typedef int (*effect_available_fn)(const char *name, void *context);
typedef void (*notetrack_report_fn)(unsigned index, const char *row,
                                          const char *effect, void *context);
/* For each effective row naming an effect slot whose effective name is empty
 * or that available() rejects, write an override of the same row with effect
 * 0: C1 then keeps the row's footstep sound and draws no effect, where an
 * effect absent from the mounted zones resolves to C1's default effect, the
 * "Missing FX" placeholder. Rows that do not parse, or name effect 0, are left
 * alone. Returns the number of rows overridden, -1 on invalid arguments or a
 * failed override (earlier overrides stay). */
int DisableUnavailableNotetrackEffects(configstrings_t *overrides,
    const configstrings_t *inherited, effect_available_fn available,
    void *context, notetrack_report_fn report);
#ifdef __cplusplus
}
#endif
#pragma pack(pop)
#endif
