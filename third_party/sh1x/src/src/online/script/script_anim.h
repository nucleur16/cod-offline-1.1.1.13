/* T5 scripted-animation GSC surface: the builtins that consume a %anim
 * reference variable.
 *
 * Registered from src/scr_vm_main.c:
 *   Scr_AddMethod("clearanim",                        Scr_ClearAnim,                   qfalse);
 *   Scr_AddMethod("setanim",                          Scr_SetAnim,                     qfalse);
 *   Scr_AddMethod("setanimlimited",                   Scr_SetAnimLimited,              qfalse);
 *   Scr_AddMethod("setanimrestart",                   Scr_SetAnimRestart,              qfalse);
 *   Scr_AddMethod("setanimknob",                      Scr_SetAnimKnob,                 qfalse);
 *   Scr_AddMethod("setanimknoblimited",               Scr_SetAnimKnobLimited,          qfalse);
 *   Scr_AddMethod("setanimknobrestart",               Scr_SetAnimKnobRestart,          qfalse);
 *   Scr_AddMethod("setanimknoball",                   Scr_SetAnimKnobAll,              qfalse);
 *   Scr_AddMethod("setanimknoballrestart",            Scr_SetAnimKnobAllRestart,       qfalse);
 *   Scr_AddMethod("setflaggedanim",                   Scr_SetFlaggedAnim,              qfalse);
 *   Scr_AddMethod("setflaggedanimlimited",            Scr_SetFlaggedAnimLimited,       qfalse);
 *   Scr_AddMethod("setflaggedanimrestart",            Scr_SetFlaggedAnimRestart,       qfalse);
 *   Scr_AddMethod("setflaggedanimknob",               Scr_SetFlaggedAnimKnob,          qfalse);
 *   Scr_AddMethod("setflaggedanimknobrestart",        Scr_SetFlaggedAnimKnobRestart,   qfalse);
 *   Scr_AddMethod("setflaggedanimknoblimited",        Scr_SetFlaggedAnimKnobLimited,   qfalse);
 *   Scr_AddMethod("setflaggedanimknoblimitedrestart", Scr_SetFlaggedAnimKnobLimitedRestart, qfalse);
 *   Scr_AddMethod("setflaggedanimknoball",            Scr_SetFlaggedAnimKnobAll,       qfalse);
 *   Scr_AddMethod("setflaggedanimknoballrestart",     Scr_SetFlaggedAnimKnobAllRestart,qfalse);
 *   Scr_AddMethod("setanimtime",                      Scr_SetAnimTime,                 qfalse);
 *   Scr_AddMethod("getanimtime",                      Scr_GetAnimTime,                 qfalse);
 *
 * See docs/SCRIPTED_ANIMATION_20260920.md for the measurements these
 * follow and for what is deliberately not here.
 */
#ifndef SCRIPT_ANIM_H
#define SCRIPT_ANIM_H
#ifdef OL_SERVER
#include "scr_vm.h"
#ifdef __cplusplus
extern "C" {
#endif

void Scr_ClearAnim(scr_entref_t ref);
void Scr_SetAnim(scr_entref_t ref);
void Scr_SetAnimLimited(scr_entref_t ref);
void Scr_SetAnimRestart(scr_entref_t ref);
void Scr_SetAnimKnob(scr_entref_t ref);
void Scr_SetAnimKnobLimited(scr_entref_t ref);
void Scr_SetAnimKnobRestart(scr_entref_t ref);
void Scr_SetAnimKnobAll(scr_entref_t ref);
void Scr_SetAnimKnobAllRestart(scr_entref_t ref);
void Scr_SetFlaggedAnim(scr_entref_t ref);
void Scr_SetFlaggedAnimLimited(scr_entref_t ref);
void Scr_SetFlaggedAnimRestart(scr_entref_t ref);
void Scr_SetFlaggedAnimKnob(scr_entref_t ref);
void Scr_SetFlaggedAnimKnobRestart(scr_entref_t ref);
void Scr_SetFlaggedAnimKnobLimited(scr_entref_t ref);
void Scr_SetFlaggedAnimKnobLimitedRestart(scr_entref_t ref);
void Scr_SetFlaggedAnimKnobAll(scr_entref_t ref);
void Scr_SetFlaggedAnimKnobAllRestart(scr_entref_t ref);
void Scr_SetAnimTime(scr_entref_t ref);
void Scr_GetAnimTime(scr_entref_t ref);
/* T5 GScr_AnimScripted_SP / StopAnimScripted: the scripted-animation state
 * (actor_lifecycle.cpp owns the actor half). */
void Scr_AnimScripted(scr_entref_t ref);
void Scr_StopAnimScripted(scr_entref_t ref);
/* codol_animScriptedModel / codol_stopAnimScriptedModel: animscripted with
 * root motion for a script_model (script_model_rootmotion.h). */
void Scr_AnimScriptedModel(scr_entref_t ref);
void Scr_StopAnimScriptedModel(scr_entref_t ref);
/* codol_getAnim( tree, name ): a clip the server tree already holds (from the
 * client animlists), by name, without adding a node; getStartOrigin /
 * getStartAngles: IW4 scene start placement. */
void Scr_GetAnimByName(void);
void Scr_GetStartOrigin(void);
void Scr_GetStartAngles(void);

#ifdef __cplusplus
}
#endif
#endif
#endif
