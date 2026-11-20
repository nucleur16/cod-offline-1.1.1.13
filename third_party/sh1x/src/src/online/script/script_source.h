#ifndef SCRIPT_SOURCE_H
#define SCRIPT_SOURCE_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Source-only roots: overlay, base/mp, then base, followed by the exact
 * alpha0022 US Army LMG/SMG-B export-filename corrections. No match returns 0;
 * Two exact damaged reference head-alias bodies receive whitespace repairs.
 * Overlay contents never receive these repairs.
 * malformed/unreadable source returns -1 without falling through. Success
 * returns 1 and an owned NUL-terminated buffer; caller frees it. */
int ReadScriptSource(const char *base,const char *overlay,const char *name,
    char **out,size_t *length);
/* Same validation/ownership, with server-owned adaptations between the
 * user overlay and reference roots. Empty native_overlay omits this layer. */
int ReadScriptSourceWithNativeOverlay(const char *base,const char *overlay,
    const char *native_overlay,const char *name,char **out,size_t *length);
/* Where each CoDOL asset category finds its engine entry points, following
 * the IW4/T5 tree layout (docs/GSC_TREE_LAYOUT.md): multiplayer keeps the
 * maps/mp/ namespace; the singleplayer-style modes -- coop (IW4 Spec Ops /
 * campaign co-op) and zm (T5 zombies) -- live in maps/, their actors in the
 * top-level animscripts/, aitype/ and character/. Every category shares one
 * script tree and one search path; nothing shadows another category's file.
 *
 *   gametypeDir  non-NULL: the rules script is <gametypeDir><g_gametype>
 *                (multiplayer). NULL: the category has exactly one rules
 *                module, rulesModule, whatever g_gametype says.
 *   forcedGametype / fallbackGametype: the g_gametype spelling the category's
 *                client contract admits (logged when the dvar differs) and
 *                the one a missing multiplayer rules script falls back to.
 *   callbackModule: the module whose CodeCallback_* labels the engine binds.
 *   levelPrefix / levelSuffix: the level script is
 *                <levelPrefix><mapname><levelSuffix> (levelSuffix NULL = "").
 *                Survival uses the MP level script and loads entity placements
 *                through maps/mp/gametypes/_survival_levels.
 *
 * A NULL or unknown category answers the multiplayer row, the server's
 * historical default (script tooling, early init). */
typedef struct script_entry_points_s {
    const char *category;
    const char *gametypeDir;
    const char *rulesModule;
    const char *forcedGametype;
    const char *fallbackGametype;
    const char *callbackModule;
    const char *levelPrefix;
    const char *levelSuffix;
} script_entry_points_t;
const script_entry_points_t *ScriptEntryPoints(const char *category);
/* The entry points GScr_LoadGameTypeScript bound for the current script load,
 * latched by SelectScriptEntryPoints so later lookups (actor and
 * vehicle damage callbacks, the script-command callback) name the very module
 * the engine bound, not one re-derived from a dvar that may have moved. */
const script_entry_points_t *SelectScriptEntryPoints(const char *category);
const script_entry_points_t *CurrentScriptEntryPoints(void);
/* Any root may be "fs:<dir>": its files are read through this host reader
 * instead of the OS filesystem (the server passes its search-path reader, so a
 * script tree can ship inside an .iwd). The reader returns 1 with a malloc'd
 * buffer of at least length+1 bytes, 0 when the file does not exist, -1 on error. */
typedef int (*script_fs_reader_t)(const char *path,char **out,size_t *length);
void SetScriptFsReader(script_fs_reader_t reader);
#ifdef __cplusplus
}
#endif
#endif
