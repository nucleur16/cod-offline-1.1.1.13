/* Actor notetrack effect rows whose effect the match cannot draw.
 *
 * The rows come from mp/constConfigStrings_<category>.csv, the baseline C1
 * itself loads, and that baseline was captured on one map: the coop table
 * (CS 6150..6171) is the training level's footsteps, where mud (surface 14)
 * plays effect slot 38, impacts/footstep_mud_dark. That effect ships only in
 * the killhouse coop zones. On every other coop map C1 resolves it to its
 * default effect, so each actor stepping on mud left a "Missing FX" box.
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later */
#include "notetrack_effects.h"
#include <stdio.h>
#include <string.h>

/* Field 3 (zero based) of a row: its start and length, or 0. */
static int effect_field(const char *row, size_t *start, size_t *length)
{
    unsigned field = 0;
    size_t i, begin = 0;
    for (i = 0;; ++i) {
        if (row[i] == ';' || !row[i]) {
            if (field == 3) {
                *start = begin; *length = i - begin;
                return row[i] == ';';
            }
            if (!row[i]) return 0;
            ++field; begin = i + 1;
        }
    }
}

int DisableUnavailableNotetrackEffects(configstrings_t *overrides,
    const configstrings_t *inherited, effect_available_fn available,
    void *context, notetrack_report_fn report)
{
    unsigned slot, changed = 0;
    if (!overrides || !available) return -1;
    for (slot = 0; slot < NOTETRACK_EFFECT_SLOTS; ++slot) {
        unsigned index = CS_NOTETRACK_EFFECTS + slot, effect = 0;
        const char *row = GetEffectiveConfigstring(overrides, inherited, index);
        const char *name;
        char rewritten[1024];
        size_t start, length, i;
        if (!row || !*row || !effect_field(row, &start, &length) || !length || length > 3) continue;
        for (i = 0; i < length; ++i) {
            if (row[start + i] < '0' || row[start + i] > '9') break;
            effect = effect * 10 + (unsigned)(row[start + i] - '0');
        }
        if (i != length || !effect) continue;
        name = effect < EFFECT_SLOTS ?
            GetEffectiveConfigstring(overrides, inherited, CS_EFFECTS + effect) : NULL;
        if (name && *name && available(name, context)) continue;
        if (strlen(row) >= sizeof(rewritten)) return -1;
        memcpy(rewritten, row, start);
        rewritten[start] = '0';
        strcpy(rewritten + start + 1, row + start + length);
        if (report) report(index, row, name ? name : "", context);
        if (!OL_SetConfigstring(overrides, index, rewritten)) return -1;
        ++changed;
    }
    return (int)changed;
}
