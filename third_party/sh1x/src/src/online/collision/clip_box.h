/* CoDOL script clip boxes: <script_model> codol_setClipBox( halfWidth, height ).
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * A level script that places an object the map's collision lacks (Kino's perk
 * machines, T5 zombie_theater.ents, which CoDOL's edit of the map dropped
 * with their clip) needs it solid for the player's OWN movement prediction as
 * well as for the server, or the player is corrected every frame he pushes
 * against it.  C1 1.1.1.13 predicts against exactly two entity shapes
 * (CG_ClipMoveToEntity 0x4856d0, CG_LinkEntity 0x4855c0):
 *
 *   - entityState solid (+0xa8, cent+0x18c) 0xffffff: an inline brush model
 *     of the client's own clipMap, which a level script cannot add;
 *   - any other nonzero solid: a packed box, decoded by 0x485360 as
 *       half.x = half.y = (solid & 0xff) - 1
 *       mins.z = 2 - ((solid >> 8) & 0xff)
 *       maxs.z = ((solid >> 16) & 0xff) - 33
 *     axis-aligned (angles vec3_origin, 0x4857fa), centred on the entity's
 *     origin in x/y, contents 1 for every eType but the player (0x485320).
 *
 * So the box is square, symmetric and integral.  codol_setClipBox gives the
 * entity the server box (-w,-w,0)..(w,w,h) and SV_LinkEntity packs it so that
 * C1 decodes exactly that box: (w+1) | 0 << 8 | (h+33) << 16, and the
 * snapshot overlay (engine_state.c, base = 2 - byte1) publishes byte1 2.
 * The ordinary IW3 packer would publish maxs.x and maxs.z + 32, which C1
 * decodes one unit smaller on every face.  The server contents are
 * CONTENTS_BODY (0x2000000): in the player (0x2814011) and actor (0x2820015)
 * clip masks, so both are blocked, but NOT in the use-candidate sight trace
 * (Player_UpdateCursorHints, mask 0x11), so a usable entity whose origin sits
 * inside the box (Kino's perk triggers) can still be used. */
#ifndef CLIP_BOX_H
#define CLIP_BOX_H
#ifdef OL_SERVER
#include "scr_vm.h"
#pragma pack(push, 8)
#ifdef __cplusplus
extern "C" {
#endif
#define CLIP_BOX_CONTENTS 0x2000000
#define CLIP_BOX_MAX_HALF_WIDTH 254
#define CLIP_BOX_MAX_HEIGHT 222
/* The C1 spelling of a clip box: what 0x485360 decodes to (-w,-w,0)..(w,w,h)
 * once the overlay maps byte1 0 to 2.  0 when w or h is out of range.
 * byte1 0 is a value the IW3 packer never writes (it clamps to 1..255). */
static inline unsigned ClipBoxPackedSolid(int halfWidth, int height)
{
    if(halfWidth < 1 || halfWidth > CLIP_BOX_MAX_HALF_WIDTH ||
       height < 1 || height > CLIP_BOX_MAX_HEIGHT) return 0;
    return (unsigned)(halfWidth + 1) | ((unsigned)(height + 33) << 16);
}
/* SV_LinkEntity: 1 and *solid set when entity <num> is a live clip box whose
 * bounds and contents still are the ones codol_setClipBox gave it. */
int ClipBoxSolid(int num, const float *mins, const float *maxs, int contents, int bmodel, int *solid);
/* G_FreeEntity: a recycled entity number is no clip box. */
void ClipBoxFreed(int num);
void Scr_SetClipBox(scr_entref_t ref);
#ifdef __cplusplus
}
#endif
#pragma pack(pop)
#endif
#endif
