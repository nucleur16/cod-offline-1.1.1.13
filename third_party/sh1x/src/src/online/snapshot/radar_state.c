#ifdef OL_SERVER
#include "persistent_runtime.h"
#endif
/* Radar / EMP playerState carriers and their CoDOL wire translations.
 * Evidence, widths, carrier choice and lifecycle: radar_state.h. */
#include "radar_state.h"
#include <string.h>

static byte team_radar_blocked[TEAM_NUM_TEAMS];

int TeamRadarBlocked(int team)
{
    if (team < 0 || team >= TEAM_NUM_TEAMS) return 0;
    return team_radar_blocked[team] != 0;
}

void SetTeamRadarBlocked(int team, int blocked)
{
    if (team < 0 || team >= TEAM_NUM_TEAMS) return;
    team_radar_blocked[team] = blocked ? 1 : 0;
}

void ResetTeamRadarBlocks(void)
{
    memset(team_radar_blocked, 0, sizeof(team_radar_blocked));
}

static void set_flag(playerState_t *ps, int flag, int on)
{
    if (!ps) return;
    if (on) ps->eFlags |= flag;
    else ps->eFlags &= ~flag;
}

int PlayerStateRadarBlocked(const playerState_t *ps)
{
    return ps && (ps->eFlags & PS_EF_RADAR_BLOCKED) != 0;
}

int PlayerStateEMPJammed(const playerState_t *ps)
{
    return ps && (ps->eFlags & PS_EF_EMP_JAMMED) != 0;
}

int PlayerStateRadarJammer(const playerState_t *ps)
{
    return ps && (ps->eFlags & PS_EF_RADAR_JAMMER) != 0;
}

void PlayerStateSetRadarJammer(playerState_t *ps, int on)
{ set_flag(ps, PS_EF_RADAR_JAMMER, on); }

void PlayerStateSetEMPJammed(playerState_t *ps, int on)
{ set_flag(ps, PS_EF_EMP_JAMMED, on); }

void PlayerStateSetRadarBlocked(playerState_t *ps, int on)
{ set_flag(ps, PS_EF_RADAR_BLOCKED, on); }

int OverlayEngineRadar(player_fields_t *wire, const playerState_t *ps)
{
    player_fields_t out;
    unsigned i, found = 0;
    if (!wire || !ps) return 0;
    out = *wire;
    for (i = 0; i < PLAYER_FIELDS; ++i) {
        switch (OL_PlayerField(i)->offset) {
        /* 0x21c: radar enabled, compared against 1 by C1's radarisenabled
         * operand at 0x005e2f7a. The retained ClientEndFrame_300..420
         * (_g_active_mp.asm:686..699) already produced it into this playerState
         * from level.teamHasRadar[client->sess.cs.team] and the 'hasradar'
         * client field, so a delayed or archived state publishes the value it
         * was produced with. Nothing else in the tree published this field. */
        case 0x21c: out.field[i] = ps->radarEnabled ? 1u : 0u; found |= 1; break;
        /* 0x220: radar blocked, the hard-jam bit both C1 cores test first
         * (0x004714c2, 0x0047152a). PublishClientRadar put it into this
         * playerState's own eFlags on the frame this state was produced. */
        case 0x220:
            out.field[i] = (ps->eFlags & PS_EF_RADAR_BLOCKED) ? 1u : 0u;
            found |= 2; break;
        /* 0x010 bit 0x400: the EMP jam. C1's isempjammed operand reads it as
         * (dword[ps+0x10] >> 10) & 1 at 0x005e2fcb and its own builtin writes it
         * at 0x00549826/0x00549832. One bit of a 26-bit word several producers
         * own, so read-modify-write. */
        case 0x010:
            out.field[i] = (out.field[i] & ~UINT32_C(0x400)) |
                ((ps->eFlags & PS_EF_EMP_JAMMED) ? UINT32_C(0x400) : 0);
            found |= 4; break;
        default: break;
        }
    }
    if (found != 7) return 0;
    *wire = out; return 1;
}

int OverlayEngineRadarJammer(entity_state_t *wire, const entityState_t *es)
{
    unsigned i;
    if (!wire || !es || es->eType != ET_PLAYER || wire->type != (unsigned)ET_PLAYER) return 0;
    for (i = 0; i < ENTITY_FIELDS; ++i) {
        if (EntityField(wire->type, i)->offset != 0x008) continue;
        /* C1's per-entity add loop tests es+8 bit 0x200000 at 0x0045ba83 and
         * measures the distance from that entity to the local playerState's
         * origin, keeping the nearest. The retained BG_PlayerStateToEntityState
         * copied the jammer's playerState eFlags into this entity state
         * (_bg_misc.asm:2164..2167), so the state being serialized answers for
         * itself - including an archived one. */
        wire->field[i] = (wire->field[i] & ~UINT32_C(0x200000)) |
            ((es->lerp.eFlags & PS_EF_RADAR_JAMMER) ? UINT32_C(0x200000) : 0);
        return 1;
    }
    return 0;
}

#ifdef OL_SERVER
#include "persistent_statics.h"

static int resident_static_zero(const void *data,size_t bytes) {
    const unsigned char *p=(const unsigned char*)data;
    for(size_t i=0;i<bytes;++i)if(p[i])return 0;
    return 1;
}

int PersistentTeamRadarEmpty(void) {return resident_static_zero(team_radar_blocked,sizeof(team_radar_blocked));}
#endif
