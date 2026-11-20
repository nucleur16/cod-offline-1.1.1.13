/* Native C1 movement-state core.
 * SPDX-License-Identifier: AGPL-3.0-or-later */
#ifndef PMOVE_C1_H
#define PMOVE_C1_H

#include "bg_public.h"
#include "usercmd.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
    PM_C1_INPUT_READY      = 1u << 0,
    PM_C1_STANCE_READY     = 1u << 1,
    PM_C1_VIEWHEIGHT_READY = 1u << 2,
    PM_C1_GROUND_READY     = 1u << 3,
    PM_C1_SLIDE_READY      = 1u << 4,
    PM_C1_LADDER_READY     = 1u << 5,
    PM_C1_MANTLE_READY     = 1u << 6,
    PM_C1_DIVE_READY       = 1u << 7,
    PM_C1_WEAPON_READY     = 1u << 8
};

typedef struct {
    struct pmove_t *host;       /* trace/touch callback storage, not input */
    struct playerState_s *ps;
    ol_usercmd_t cmd;        /* authoritative C1 command */
    ol_usercmd_t oldcmd;     /* previously executed C1 command */
    struct pml_t local;
} pmove_c1_t;

typedef enum {
    PM_C1_MOVE_NORMAL = 0,
    PM_C1_MOVE_MANTLE,
    PM_C1_MOVE_LADDER
} pmove_c1_mode_t;

/* Entry seam for ClientThink.  It consumes the structurally bound full C1
 * command.  Zero means the native core is intentionally inactive and the
 * caller must preserve the current live path. */
int PmoveC1Try(struct pmove_t *pm);
unsigned PmoveC1Implemented(void);
unsigned PmoveC1Required(void);
int PmoveC1Active(void);
/* Parity switch "c1EventParity" (default 1): the predicted events the
 * server queues because C1 does -- EV_RESET_ADS on a sprint or melee out of a
 * held sight, night vision, foliage -- the left-hand entity fire event and the
 * per-snapshot variable-zoom debounce. 0 restores the previous behaviour, for
 * bisecting live. */
int C1EventParity(void);
/* C1 melee check 0x41c185..0x41c190 for the retained melee start. */
void __cdecl MeleeResetAdsC1(struct pmove_t *pm);
void PmoveC1ResetClient(unsigned client);
void PmoveC1ResetAll(void);
/* Every wire command in arrival order, including ones the server does not
 * execute; the command before the executing one becomes its first oldcmd. */
void PmoveC1NoteWireCommand(unsigned client, const ol_usercmd_t *command);
/* Nonzero when the C1 mover owns this command's clock. The level.time+200
 * ceiling is then applied here (to ucmd and to the C1 mover), and
 * ClientThink_real skips only its retained level.time-1000 floor. */
int PmoveC1OwnsClientClock(const struct playerState_s *ps,
                                 struct usercmd_s *ucmd, int level_time);

/* Component entry points for the native core.  Do not splice them into the
 * retained Pmove path: PmoveC1Try gates the complete core atomically. */
void REGPARM(2) PM_UpdateStanceC1(struct pmove_t *pm,
                                        struct pml_t *pml);

/* C1 PM_UpdateViewheight 0x00411D80.  Exported separately because linked,
 * vehicle and later native C1 movement paths need the same state producer. */
void PM_UpdateViewheightC1(struct pmove_t *pm, struct pml_t *pml);

/* C1 gives an active mantle exclusive ownership of the movement frame. */
int PM_C1MantleOwnsFrame(struct pmove_t *pm, struct pml_t *pml);

/* Atomic C1 ordering: mantle owns the frame before ladder acquisition runs. */
pmove_c1_mode_t PM_C1SelectMove(pmove_c1_t *move);
void PM_CheckLadderC1(pmove_c1_t *move);
void PM_LadderVelocityC1(pmove_c1_t *move);

#ifdef __cplusplus
}
#endif
#endif
