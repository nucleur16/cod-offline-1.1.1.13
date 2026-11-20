#ifdef OL_SERVER
#include "persistent_runtime.h"
#endif
/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "game.h"
#include "recoil_state.h"
#include "sprint_state.h"
#include "weapon_lock.h"
#include "playerstate_ext.h"
#include "hud_icon_overrides.h"
#include "pmove_c1.h"
#ifdef OL_SERVER
#include "weapon_input.h"
#include "radar_input.h"
#include "scoreboard_stats.h"
#include "spectate_defaults.h"
#include "script_vehicle_builtins.h"
#include "script_player_builtins.h"
#include "radar_state.h"
#include "weapon_state.h"
#include "clientstate_ext.h"
#endif
#include "script_usehint.h"
#include "script_playeruse.h"
#include "script_vision.h"
#include "lookat.h"
#include "sys_cod4defs.h"
#include "qcommon_io.h"
#include <string.h>
#include <math.h>
#include <limits.h>

int ReadUsermoveMSGFrom(msg_t *msg,uint32_t key,unsigned count,
                              const ol_usercmd_t *from,ol_usercmd_t *commands,
                              size_t capacity,float origin[3],uint32_t *origin_time)
{
    ol_usercmd_t decoded[MAX_USERCMDS],base={0};
    ol_msg_t wire; float position[3]; uint32_t time; unsigned i;
    if(!msg) return 0;
    if(!commands || !origin || !origin_time || !count || count>MAX_USERCMDS || count>capacity ||
       msg->overflowed || msg->splitData || msg->cursize<0 || msg->readcount<0 || msg->bit<0 ||
       msg->readcount>msg->cursize || msg->cursize>INT_MAX/8 ||
       ((msg->bit&7) && msg->bit/8>=msg->readcount)) goto fail;
    if(from) base=*from;
    MsgReadInit(&wire,msg->data,(size_t)msg->cursize);
    wire.cursor=(size_t)msg->readcount; wire.bit=(size_t)msg->bit;
    for(i=0;i<count;++i) {
        if(!ReadUsercmd(&wire,key,&base,&decoded[i])) goto fail;
        base=decoded[i];
    }
    for(i=0;i<3;++i) {
        uint32_t word=ReadLong(&wire);
        memcpy(position+i,&word,4);
        if(!isfinite(position[i])) goto fail;
    }
    time=ReadLong(&wire);
    if(wire.failed) goto fail;
    memcpy(commands,decoded,count*sizeof(*commands));
    memcpy(origin,position,sizeof(position)); *origin_time=time;
    msg->readcount=(int)wire.cursor; msg->bit=(int)wire.bit;
    return 1;
fail:
    msg->overflowed=qtrue; return 0;
}

int ReadUsermoveMSG(msg_t *msg,uint32_t key,unsigned count,ol_usercmd_t *commands,
                          size_t capacity,float origin[3],uint32_t *origin_time)
{
    return ReadUsermoveMSGFrom(msg,key,count,NULL,commands,capacity,origin,origin_time);
}

int DefaultWireUsercmd(const playerState_t *ps,ol_usercmd_t *out)
{
    unsigned primary=0; int weapon,offhand,alt; unsigned i;
    if(!ps || !out) return 0;
    memset(out,0,sizeof(*out));
    weapon=WeaponToWire(ps->weapon);
    offhand=WeaponToWire((unsigned)ps->offHandIndex);
    if(weapon<0 || offhand<0 || !WeaponStatePrimaryWeapon(ps,&primary) ||
       (alt=WeaponToWire(primary))<0) return 0;
    out->weapon=(uint16_t)weapon;
    out->offHandIndex=(uint16_t)offhand;
    out->altModeWeapon=(uint16_t)alt;
    for(i=0;i<2;++i)
        out->angles[i]=(uint16_t)ANGLE2SHORT(ps->viewangles[i]-ps->delta_angles[i]);
    /* C1's CL_WritePacket seeds the first delta from a default command made
     * from its current playerState.  Resetting this base to an all-zero command
     * made unchanged stance/weapon fields decode as zero on alternating packets. */
    if(ps->otherFlags&POF_PLAYER) {
        if(ps->eFlags&8) out->buttons|=0x100;
        else if(ps->eFlags&4) out->buttons|=0x200;
        if(ps->leanf<0.0f) out->buttons|=0x40;
        else if(ps->leanf>0.0f) out->buttons|=0x80;
        /* C1 keeps the derived ADS-active state on 0x800.  Bit 0x100000 is
         * the raw right-mouse trigger (and also participates in akimbo and
         * equipment firing), so synthesizing it from fWeaponPosFrac changes
         * weapon input while merely delta-basing a command. */
        if(ps->fWeaponPosFrac!=0.0f) out->buttons|=0x800;
        if(ps->pm_flags&(PmoveC1Active()?0x4000:0x8000)) out->buttons|=2;
    }
    return 1;
}

static uint32_t held_word(const player_fields_t *held,uint16_t offset)
{
    unsigned i;
    for(i=0;i<PLAYER_FIELDS;++i)
        if(OL_PlayerField(i)->offset==offset) return held->field[i];
    return 0;
}
static float held_float(const player_fields_t *held,uint16_t offset)
{
    uint32_t word=held_word(held,offset); float value;
    memcpy(&value,&word,sizeof(value));
    return isfinite(value) ? value : 0.0f;
}
/* C1's first-command delta base. CL_WritePacket (virtualised, called into
 * MSG_WriteDeltaUsercmdKey 0x59fa30 with from = a stack command) builds it
 * the T5 way, MSG_SetDefaultUserCmd(&cl->snap.ps) (OpenBLOPS
 * msg_mp.cpp:3601), over the playerState of the newest snapshot it holds,
 * not the live one. Measured live 2026-09-28: base angles = snapshot
 * viewangles (the -87 fields C1 transmits only while following, view-locked
 * or at pm_type 1/6, so normally 0) minus delta_angles, weapon/alt from
 * +0x33c/+0x340, offhand +0x328, and 0x800 while fWeaponPosFrac != 0.
 * The caller passes the server's held wire fields for that snapshot.
 *
 * C1's builder is 0x0059F810, and it is NOT all of T5's: under otherFlags
 * 0x1000 it sets only prone 0x100 / crouch 0x200 (eFlags 8/4, 0x59f8a3) and
 * ADS 0x800 (+0x348 != 0, 0x59f8ca), then returns at 0x59f8e2. T5's lean
 * 0x40/0x80 and sprint 0x2 bits are absent. A bit the server adds to the
 * base that C1 does not is decoded as held whenever C1 sends the buttons
 * unchanged or short-delta (both inherit the base's upper bits): with the
 * sprint bit, a sprinting player who had released the sprint key was read
 * as still holding it, so the next press was not a rising edge, PM_UpdateSprint
 * never toggled sprint off, and C1 (which predicted the toggle) was snapped
 * back into sprint-in. Lean stuck the same way. */
void DefaultWireUsercmdFromFields(const player_fields_t *held,ol_usercmd_t *out)
{
    unsigned i;
    if(!out) return;
    memset(out,0,sizeof(*out));
    if(!held) return;
    out->weapon=(uint16_t)held_word(held,0x33c);
    out->altModeWeapon=(uint16_t)held_word(held,0x340);
    out->offHandIndex=(uint16_t)held_word(held,0x328);
    for(i=0;i<2;++i)
        out->angles[i]=(uint16_t)ANGLE2SHORT(held_float(held,(uint16_t)(0x14c+4*i))-
                                             held_float(held,(uint16_t)(0x09c+4*i)));
    if(held_word(held,0x010)&POF_PLAYER) {
        uint32_t eflags=held_word(held,0x0ec);
        if(eflags&8) out->buttons|=0x100;
        else if(eflags&4) out->buttons|=0x200;
        if(held_float(held,0x348)!=0.0f) out->buttons|=0x800;
    }
}

static ol_usercmd_t last_commands[MAX_CLIENTS];
static unsigned action_held[MAX_CLIENTS],action_edges[MAX_CLIENTS];

void RecordActionSlot(unsigned client,unsigned slot,int down)
{
    unsigned bit;
    if(client>=MAX_CLIENTS || slot>=5)return;
    bit=1u<<slot;
    if(down){action_edges[client]|=bit&~action_held[client];action_held[client]|=bit;}
    else action_held[client]&=~bit;
}
unsigned ActionSlotButtons(unsigned client)
{
    return client<MAX_CLIENTS ? action_held[client]|action_edges[client]:0;
}
void ClearActionSlotEdges(unsigned client)
{
    if(client<MAX_CLIENTS)action_edges[client]=0;
}
void ResetActionSlots(unsigned client)
{
    if(client<MAX_CLIENTS)action_held[client]=action_edges[client]=0;
}

int ReadUsercmdMSG(msg_t *msg, uint32_t key,
                         const ol_usercmd_t *from, ol_usercmd_t *to)
{
    ol_msg_t wire;
    if (!msg) return 0;
    /* Match the complete usermove reader's bit-pool contract. A partial pool
     * must belong to a byte already reserved below readcount; accepting a
     * pool ahead of that cursor would decode bytes in a different order. */
    if (!from || !to || msg->overflowed || msg->splitData || msg->cursize < 0 ||
        msg->readcount < 0 || msg->bit < 0 || msg->readcount > msg->cursize ||
        msg->cursize > INT_MAX / 8 ||
        ((msg->bit & 7) && msg->bit / 8 >= msg->readcount)) {
        msg->overflowed = qtrue; return 0;
    }
    MsgReadInit(&wire, msg->data, msg->cursize);
    wire.cursor = msg->readcount; wire.bit = msg->bit;
    if (!ReadUsercmd(&wire, key, from, to)) {
        msg->overflowed = qtrue; return 0;
    }
    msg->readcount = wire.cursor; msg->bit = wire.bit;
    return 1;
}

void ToEngineUsercmd(const ol_usercmd_t *wire, usercmd_t *engine,
                           unsigned current_weapon, unsigned current_offhand)
{
    ol_usercmd_t game;
    int weapon, offhand; unsigned i;
    FilterUsercmd(wire, &game);
    memset(engine, 0, sizeof(*engine));
    engine->serverTime = game.serverTime;
    /* Shared IW3 bits through frag/smoke; do not turn CoDOL's action-slot or
     * remote/dive bits into unrelated IW3 actions. Prone and crouch stay as
     * levels on every command: C1 CL_CmdButtons produces them that way and
     * retained PM_CheckDuck consumes the current levels directly. */
    engine->buttons = game.buttons & UINT32_C(0xff3f);
    /* Location confirmation/cancellation consume the bound C1 raw command
     * directly in LocationSelectionAction; no IW3 button remapping. */
    /* 0x800 is shared ADS-active state. Keep C1's 0x100000 raw trigger in the
     * native command sidecar; converting it to IW3 ADS makes server movement
     * enter ADS earlier/differently than the C1 predictor. */
    for (i = 0; i < 3; ++i) engine->angles[i] = game.angles[i];
    engine->forwardmove = game.direction & 1 ? 127 : game.direction & 2 ? -127 : 0;
    engine->rightmove = game.direction & 4 ? 127 : game.direction & 8 ? -127 : 0;
    weapon = WeaponToEngine(game.weapon);
    offhand = WeaponToEngine(game.offHandIndex);
    UsercmdSetWeapon(engine, weapon >= 0 ? (unsigned)weapon : (unsigned)current_weapon);
    UsercmdSetOffhand(engine, offhand >= 0 ? (unsigned)offhand : (unsigned)current_offhand);
    engine->meleeChargeYaw = game.meleeChargeYaw * (360.0f / 65536.0f);
    engine->meleeChargeDist = game.meleeChargeDist;
    memcpy(engine->selectedLocation, game.selectedLocation, 2);
}

/* C1 PM_Weapon_CheckForChange 0x41b109..0x41b14b: a usercmd whose weapon is 0
 * while ps->weapon is held, off a turret, is ignored unless
 * cmd.serverTime - ps->0x333c > 200. The first commands after a (re)spawn still
 * carry the dead/spectating weapon 0; without this the server would begin a
 * switch to "no weapon" that the C1 predictor (now fed the same stamp) refuses. */
int ApplySpawnWeaponGate(const playerState_t *ps, usercmd_t *engine)
{
    int spawn_time, age;
    if(!ps || !engine || UsercmdWeapon(engine)!=0 || ps->weapon==0) return 0;
    if(ps->eFlags & 0xc00) return 0;
    if(!PlayerStateSpawnTimeRead(ps,&spawn_time) || !spawn_time) return 0;
    age = engine->serverTime - spawn_time;
    if(age > 200) return 0;
    UsercmdSetWeapon(engine, (unsigned)ps->weapon);
    {
        static int lines;
        if(TRACE_VERBOSE() && lines<200) {
            ++lines;
            ((void)0);
        }
    }
    return 1;
}

void RecordUsercmd(unsigned client, const ol_usercmd_t *wire)
{
    if (client < MAX_CLIENTS) last_commands[client] = *wire;
}
const ol_usercmd_t *LastUsercmd(unsigned client)
{
    return client < MAX_CLIENTS ? &last_commands[client] : NULL;
}
/* One client slot's share of the input state this file owns. The cross-area
 * additions are in ResetClientInput in game_reset.c. */
void ResetClientInputCore(unsigned client)
{
#ifdef OL_SERVER
    ResetClientRadarInput(client);
#endif
    ResetActionSlots(client);
    ResetClientWeaponLock(client);
    ResetSprintOverrides(client);
    ResetPlayerRecoilScale(client);
    PlayerStateExtClearClientStun(client);
    /* The retained event counter is zeroed with the playerState; its epoch
     * follows, and neither the recoil nor the lock half is touched. */
    PlayerStateExtClearClientEvents(client);
    /* C1 keeps the loop sound in the playerState, so the gclient memset that
     * accompanies these sites silences it; the captured and archived states
     * keep theirs. */
    PlayerStateExtClearClientLoopSound(client);
    PlayerStateExtClearClientViewhands(client);
    PlayerStateExtClearClientDive(client);
    PlayerStateExtClearClientSpawnTime(client);
    PlayerStateExtClearClientHudIcons(client);
    /* The four relocated per-weapon arrays were inside the playerState the
     * gclient memset zeroes at these sites; zero their live home with it. */
    PlayerStateExtClearClientInventory(client);
#ifdef OL_SERVER
    ResetClientActionSlotState(client);
    ResetClientKillstreakHud(client);
    /* Re-arm the one-shot friendly crosshair colour command (lookat.c).
     * Without this the per-slot latch survives the whole server process, so a
     * client entering a slot a bot or an earlier player already used never
     * receives `s 291 "0 1 0 1"` and keeps C1's registered steel blue
     * (0.4,0.6,0.85, the reset value still live in the client at 0x0052B1FF)
     * on the friendly branch CG_GetCrosshairColor 0x0043C528 paints. */
    ResetLookAtClientState(client);
#endif
    PlayerStateExtClearClientAbilities(client);
    PlayerStateExtClearClientPerks(client);
    PlayerStateExtClearClientLinks(client);
    PlayerStateExtClearClientUseHint(client);
    PmoveC1ResetClient(client);
    if (client < MAX_CLIENTS) memset(&last_commands[client], 0, sizeof(last_commands[client]));
}
/* The part of the input reset this file's own neighbours own. The cross-area
 * aggregate that adds vision sets, player builtins, the remote view, turret
 * control, the turret minimap and HUD icon overrides is
 * ResetGameInput in game_reset.c: keeping it out of here is what
 * lets this translation unit be linked without those subsystems behind it. */
void ResetGameInputCore(void)
{
#ifdef OL_SERVER
    ResetAllRadarInput();
    ResetTeamRadarBlocks();
#endif
    memset(action_held,0,sizeof(action_held));
    memset(action_edges,0,sizeof(action_edges));
    ResetClientWeaponLocks();
    ResetAllSprintOverrides();
    ResetAllPlayerRecoilScales();
    memset(last_commands, 0, sizeof(last_commands));
    PmoveC1ResetAll();
    ResetWeaponMap();
}

#ifdef OL_SERVER
#include "persistent_statics.h"

static int resident_static_zero(const void *data,size_t bytes) {
    const unsigned char *p=(const unsigned char*)data;
    for(size_t i=0;i<bytes;++i)if(p[i])return 0;
    return 1;
}

int PersistentGameInputEmpty(void) {return resident_static_zero(action_held,sizeof(action_held)) && resident_static_zero(action_edges,sizeof(action_edges)) && resident_static_zero(last_commands,sizeof(last_commands));}
#endif
