/* GSC entry points for the script-model animation family.
 *
 * Registration (shared patch, ScriptStartupExtra_Register):
 *   Scr_AddFunction("precachempanim",       Scr_PrecacheMpAnim,          qfalse);
 *   Scr_AddMethod  ("scriptmodelplayanim",  ScrCmd_ScriptModelPlayAnim,  qfalse);
 *   Scr_AddMethod  ("scriptmodelclearanim", ScrCmd_ScriptModelClearAnim, qfalse);
 *
 * Measured argument contract (IW4 0x5f8380, 0x4d9a00, 0x4512e0):
 *   precachempanim(<string animation>)                       global, init only, no return
 *   <script model> scriptmodelplayanim(<string animation>)   no return
 *   <script model> scriptmodelclearanim()                    no return
 *
 * The animation a script names must have been precached by the same map's
 * gametype or level script before its first wait. Unlike IW4, whose client
 * resolves a configstring name, C1's measured consumer selects the animation
 * by an entry index inside a tree the client picks from its own realm-0
 * registry (see model_animation.h for the aligned trace of
 * 0x43fcf9..0x43fdba, 0x443540/0x4435bb and the two registries). So a play
 * here has to resolve to an entry index inside a tree whose client id has been
 * declared with BindClientAnimTree; a name that resolves in no declared
 * tree, or ambiguously in several, is refused rather than stored as an
 * animation that cannot be published.
 *
 * These handlers capture resolved fields into the entity-state extension
 * before committing live animation intent. Snapshot copies and archive records
 * retain those fields for the model overlay. Registration remains pending
 * until tree entry correspondence and replay semantics are established.
 */
#ifndef SCRIPT_MODEL_ANIMATION_H
#define SCRIPT_MODEL_ANIMATION_H
#include "scr_vm_functions.h"

#ifdef __cplusplus
extern "C" {
#endif

void Scr_PrecacheMpAnim(void);
void ScrCmd_ScriptModelPlayAnim(scr_entref_t ref);
void ScrCmd_ScriptModelClearAnim(scr_entref_t ref);

#ifdef __cplusplus
}
#endif
#endif
