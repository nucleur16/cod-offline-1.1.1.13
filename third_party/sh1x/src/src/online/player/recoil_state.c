/* Fire-recoil scale. See recoil_state.h for the measured IW4/C1
 * provenance of the flag, the magnitude and both consumers, and
 * playerstate_ext.h for the storage model the magnitude travels in. */
#include "recoil_state.h"
#include "weapon_flags.h"

int ArmPlayerRecoilScale(playerState_t *ps, int percent)
{
    ps_ext_t *ext;
    if (!ps || percent < RECOIL_SCALE_MIN || percent > RECOIL_SCALE_MAX) return 0;
    ext = PlayerStateExt(ps);
    if (!ext) return 0;
    ext->recoil_percent = (unsigned char)percent;
    ext->recoil_known = 1;
    ps->weapFlags |= WEAP_RECOIL_SCALE;
    return 1;
}

void DisarmPlayerRecoilScale(playerState_t *ps)
{
    if (!ps) return;
    /* C1 0x005198da clears the bit and leaves ps+0x3330 alone, so a later
     * player_recoilScaleOn( same scale ) is a one-field change on the wire. */
    ps->weapFlags &= ~WEAP_RECOIL_SCALE;
}

int PlayerRecoilScalePercent(const playerState_t *ps)
{
    const ps_ext_t *ext = ps ? PlayerStateExtConst(ps) : NULL;
    if (!ext || !ext->recoil_known) return -1;
    return ext->recoil_percent;
}

float PlayerRecoilScale(const playerState_t *ps)
{
    int percent;
    if (!ps || !(ps->weapFlags & WEAP_RECOIL_SCALE)) return 1.0f;
    percent = PlayerRecoilScalePercent(ps);
    if (percent < 0) return 1.0f;
    /* IW4 0x4a60dc and C1 0x004222c6 both convert the integer and divide by the
     * 100.0 double before the multiply; keep that order and precision. */
    return (float)((double)percent / 100.0);
}

int OverlayEngineRecoilScale(player_fields_t *wire, const playerState_t *ps)
{
    player_fields_t out;
    unsigned i, found = 0;
    int armed, percent;
    if (!wire || !ps) return 0;
    armed = (ps->weapFlags & WEAP_RECOIL_SCALE) != 0;
    percent = PlayerRecoilScalePercent(ps);
    /* An armed state with no recorded magnitude publishes 100, which is the
     * multiplier the server-side consumer also applies in that case; publishing
     * 0 would delete the client's predicted recoil on a bookkeeping failure. */
    if (percent < 0) percent = armed ? RECOIL_SCALE_MAX : 0;
    out = *wire;
    for (i = 0; i < PLAYER_FIELDS; ++i) {
        unsigned offset = OL_PlayerField(i)->offset;
        /* C1 BG_WeaponFireRecoil 0x004222bf tests weapFlags bit 0x400 and only
         * then reads the percentage, exactly like the retained consumer. Leave
         * every other weapon flag to its own producer. */
        if (offset == 0x344) {
            out.field[i] = (out.field[i] & ~UINT32_C(0x400)) | (armed ? UINT32_C(0x400) : 0);
            found |= 1;
        } else if (offset == 0x3330) {
            out.field[i] = (uint32_t)percent;
            found |= 2;
        }
    }
    if (found != 3) return 0;
    *wire = out;
    return 1;
}

/* Field-specific: the extension also carries the weapon lock, and a respawn or
 * reconnect that drops the recoil magnitude must not drop a lock, nor the
 * reverse. The whole-block clear is only for the map/game reset. */
void ResetPlayerRecoilScale(unsigned client)
{
    PlayerStateExtClearClientRecoil(client);
}

void ResetAllPlayerRecoilScales(void)
{
    /* ResetGameInput means every field of every storage. */
    PlayerStateExtClearAll();
}
