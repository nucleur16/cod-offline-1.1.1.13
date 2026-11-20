/* Script-model character clones: a script_model drawn as a client's character.
 *
 * C1 1.1.1.13 (base 0x400000), MEASURED for the end-of-match top players
 * scene (docs/ENDGAME_TOP3.md):
 *
 *   - Builtin 0x55b5e0 (method table entry 0xcd3788; C1 strips every builtin
 *     name, so the original name is not recoverable) takes an entity and, when
 *     it owns a client, stores client number + 1 at the script mover's
 *     entityState +0x64 (0x55b6cd..0x55b6d0). For an actor it stores the
 *     composition slot + 0x14 instead (0x55b6d5..0x55b6dc). It then links the
 *     source's world weapons (or the named weapons of an optional second
 *     argument) to the mover and returns them.
 *   - +0x64 is eType 6 wire field 0x064 (7 bits). CG's DObj builder for script
 *     movers 0x43e809 reads it from centity+0xcc (currentState, which
 *     0x46b739..0x46b747 copies from nextState+8 as 0x1d dwords, so
 *     centity+0x70 is wire +0x08) and, when nonzero, calls 0x4437c0 with
 *     value - 1. 0x4437c0 composes the DObj from that client's character
 *     record - all of its parts, the same models CG_NewClientInfo draws the
 *     player with - on the realm-0 animtree named by +094 (0x443540, case
 *     eType 6). An index of 0x13 or more selects an actor composition slot
 *     (0x443809..0x443821), so a client clone is bounded by 0x13.
 *
 * That is how C1 shows a player's own character on a script model, which an
 * ordinary eType-6 DObj cannot: 0x443610 builds those from one model only.
 *
 * Here: <script_model> codol_setCharacterClone( <player> [, <weapons>] )
 * stores the client in the native lerp union
 * (LerpEntityStateScriptMover.characterClient), which
 * OverlayEngineModelEntity publishes at +064, gives the model the
 * player's outfit server-side (so the hand tags exist), and links world guns
 * to it like 0x55b5e0: the player's current world guns, or one per weapon
 * name given (a string or an array of strings). It returns the linked guns as
 * an array; deleting them is the caller's job. codol_setCharacterClone() or
 * ( undefined ) clears the clone and returns an empty array. */
#ifndef SCRIPT_CHARACTER_CLONE_H
#define SCRIPT_CHARACTER_CLONE_H
/* Largest publishable +064 value: client 0x12 + 1. */
#define CHARACTER_CLONE_MAX 0x13
/* The method itself is declared in scr_vm_functions.h with the other
 * builtins and registered in scr_vm_main.c. */
#endif
