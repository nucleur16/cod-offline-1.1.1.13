#ifdef OL_SERVER
#include "persistent_runtime.h"
#endif
/* Opus voice relay for C1 clients.
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * C1 lost all of IW's voice code (no capture, no codec, CL_ConnectionlessPacket
 * prints "voice communication is disabled" for "v"). Kernel33's voice.c puts it
 * back with Opus and talks to this relay over the game socket, as IW3/IW4 do:
 * out-of-band "v" packets that ride the client's bdDTLS association both ways.
 *
 * Client -> server, after "\xff\xff\xff\xffv\0":
 *     u8 version (1), u8 flags (bit 0: last packet of this transmission),
 *     u16 seq (little endian, sequence number of the first frame),
 *     u8 frameCount (1..VOICE_MAX_FRAMES), then per frame u8 len + len
 *     bytes of one 20 ms Opus frame.
 * Server -> client, after "\xff\xff\xff\xffv\0":
 *     u8 version (1), u8 flags, u8 talker clientNum, u16 seq, u8 frameCount,
 *     frames as above. One talker per packet; the relay forwards immediately.
 *
 * The talker is the client whose netchan address sent the packet; the server
 * never trusts a client number from the payload. The Opus frames are relayed
 * opaquely (validated only for size), so the codec settings are the client's.
 *
 * Who hears whom (VoiceCanHear):
 *   - team modes: teammates only; non-team modes (the talker's session team is
 *     TEAM_FREE, which _menus.gsc gives every player when !level.teamBased):
 *     everyone. sv_alltalk 1 lifts the team rule.
 *   - a dead talker reaches only listeners who are not playing (dead or
 *     spectating), as IW's voice_deadChat 0. voice_deadChat 1 lifts it.
 *   - killcam (classic CoD4): a victim watching the killcam of their own death
 *     hears their killer, enemy or not. "Watching" is what _killcam.gsc sets:
 *     sess.forceSpectatorClient == the killer and sess.archiveTime > 0; "their
 *     own death" is the attacker Scr_PlayerKilled last reported for them. One
 *     direction only: the killer does not hear the victim through this rule.
 *   - per-listener mutes (muteList) and the server's mutelevel are respected.
 *
 * Talk balloon: C1 draws headicontalkballoon over a teammate's player entity
 * while its es.eFlags has 0x100 (CG head icons 0x45AF8C). C1's own Pmove sets
 * that bit from usercmd button 0x10000000 (the "input locked" button, set while
 * a menu/chat owns the keyboard), so it is already a "this player is talking/
 * typing" marker. OverlayEnginePlayerEntityFlags ORs it in on the wire
 * while VoiceIsTalking. */
#ifdef OL_SERVER
#include "voice.h"
#include "q_shared.h"
#include "qcommon.h"
#include "qcommon_io.h"
#include "cvar.h"
#include "g_shared.h"
#include "server.h"
#include "netchan.h"
#include "sys_net.h"
#include "cmd.h"

#define VOICE_VERSION    1
#define VOICE_MAX_FRAMES 6
#define VOICE_MAX_FRAME  255

static cvar_t *sv_alltalk;
static cvar_t *voiceMaxPackets;
static cvar_t *voiceEcho;
static cvar_t *sv_voiceDebug;
static cvar_t *g_voiceChatTalkingDuration;

static int voice_last_time[MAX_CLIENTS];   /* svs.time of the talker's last packet, 0 = silent */
static int voice_bucket_time[MAX_CLIENTS]; /* rate limit: one-second window start */
static int voice_bucket_count[MAX_CLIENTS];
static int voice_last_killer[MAX_CLIENTS];
static int voice_debug_lines;

extern cvar_t *voice_deadChat;
static void VoiceHear_f(void);

void VoiceInit(void)
{
    int i;
    sv_alltalk = Cvar_RegisterBool("sv_alltalk", qfalse, 1,
        "Voice chat reaches every player regardless of team (dead players still reach only the dead unless voice_deadChat)");
    voiceMaxPackets = Cvar_RegisterInt("voiceMaxPackets", 40, 5, 200, 0,
        "Voice packets per second accepted from one client; the rest are dropped");
    voiceEcho = Cvar_RegisterBool("voiceEcho", qfalse, 0,
        "Relay each talker's voice back to the talker too (single-client voice test)");
    sv_voiceDebug = Cvar_RegisterBool("sv_voiceDebug", qfalse, 0,
        "Print a line per relayed voice packet (capped)");
    g_voiceChatTalkingDuration = Cvar_RegisterInt("g_voiceChatTalkingDuration", 500, 0, 10000, 0,
        "Milliseconds the talk balloon stays up after the last voice packet");
    Cmd_AddCommand("voicehear", VoiceHear_f);
    for (i = 0; i < MAX_CLIENTS; ++i) {
        voice_last_time[i] = 0;
        voice_bucket_time[i] = 0;
        voice_bucket_count[i] = 0;
        voice_last_killer[i] = -1;
    }
}

void VoiceClientReset(int clientNum)
{
    if (clientNum < 0 || clientNum >= MAX_CLIENTS) return;
    voice_last_time[clientNum] = 0;
    voice_bucket_count[clientNum] = 0;
    voice_last_killer[clientNum] = -1;
}

void VoiceNoteKill(int victim, int attacker)
{
    if (victim < 0 || victim >= MAX_CLIENTS) return;
    voice_last_killer[victim] = attacker >= 0 && attacker < MAX_CLIENTS && attacker != victim ? attacker : -1;
}

int VoiceIsTalking(int clientNum)
{
    int duration;
    if (clientNum < 0 || clientNum >= MAX_CLIENTS || !voice_last_time[clientNum]) return 0;
    if (!sv_voice || !sv_voice->boolean) return 0;
    duration = g_voiceChatTalkingDuration ? g_voiceChatTalkingDuration->integer : 500;
    if (svs.time - voice_last_time[clientNum] <= duration) return 1;
    voice_last_time[clientNum] = 0;
    return 0;
}

static int voice_playing(const gclient_t *c)
{
    /* Intermission is everybody's end of match: nobody is "dead" there. */
    return c->sess.sessionState == SESS_STATE_PLAYING || c->sess.sessionState == SESS_STATE_INTERMISSION;
}

/* Why talker's voice reaches listener (both client slots): 0 it does not, 1 team (or a non-team
 * mode), 2 sv_alltalk, 3 the listener's own killcam, 4 echo. */
static int VoiceHearReason(int listener, int talker)
{
    client_t *lcl = &svs.clients[listener];
    gclient_t *lc, *tc;
    int tteam, lteam;

    if (lcl->state < CS_ACTIVE || lcl->netchan.remoteAddress.type == NA_BOT) return 0;
    if (listener == talker) return voiceEcho->boolean ? 4 : 0;
    if (lcl->muteList[talker]) return 0;
    if (!level.clients || listener >= level.maxclients || talker >= level.maxclients) return 0;
    lc = &level.clients[listener];
    tc = &level.clients[talker];
    if (lc->sess.connected != CON_CONNECTED || tc->sess.connected != CON_CONNECTED) return 0;

    /* Killcam of the listener's own death: hear the killer. */
    if (lc->sess.forceSpectatorClient == talker && lc->sess.archiveTime > 0 &&
        voice_last_killer[listener] == talker)
        return 3;

    if (!voice_playing(tc) && voice_playing(lc) && !(voice_deadChat && voice_deadChat->boolean))
        return 0;

    if (sv_alltalk->boolean) return 2;
    tteam = (int)tc->sess.cs.team;
    lteam = (int)lc->sess.cs.team;
    if (tteam == TEAM_FREE && lteam != TEAM_SPECTATOR) return 1; /* !level.teamBased */
    return tteam == lteam ? 1 : 0;
}

static int VoiceCanHear(int listener, int talker)
{
    return VoiceHearReason(listener, talker) != 0;
}

/* "voicehear": for every human listener, whom it would hear right now and why. */
static void VoiceHear_f(void)
{
    int l, t;
    static const char reason[] = "-tak e";
    for (l = 0; l < sv_maxclients->integer && l < MAX_CLIENTS; ++l) {
        client_t *cl = &svs.clients[l];
        char line[1024];
        int n;
        gclient_t *gc;
        if (cl->state < CS_ACTIVE || cl->netchan.remoteAddress.type == NA_BOT || !level.clients ||
            l >= level.maxclients) continue;
        gc = &level.clients[l];
        n = Com_sprintf(line, sizeof(line), "voicehear: %d %s team %d ss %d fsc %d arch %d killer %d hears:",
                        l, cl->name, (int)gc->sess.cs.team, (int)gc->sess.sessionState,
                        gc->sess.forceSpectatorClient, gc->sess.archiveTime, voice_last_killer[l]);
        for (t = 0; t < sv_maxclients->integer && t < MAX_CLIENTS && n < (int)sizeof(line) - 16; ++t) {
            int r;
            if (svs.clients[t].state < CS_ACTIVE || t >= level.maxclients) continue;
            r = VoiceHearReason(l, t);
            n += Com_sprintf(line + n, sizeof(line) - n, " %d%c(t%d,s%d)", t, reason[r],
                             (int)level.clients[t].sess.cs.team, (int)level.clients[t].sess.sessionState);
        }
        ((void)0);
    }
}

static client_t *voice_find_client(netadr_t *from)
{
    int i;
    for (i = 0; i < sv_maxclients->integer && i < MAX_CLIENTS; ++i) {
        client_t *cl = &svs.clients[i];
        if (cl->state >= CS_CONNECTED && NET_CompareAdr(from, &cl->netchan.remoteAddress)) return cl;
    }
    return NULL;
}

void VoicePacket(netadr_t *from, msg_t *msg)
{
    byte out[16 + VOICE_MAX_FRAMES * (VOICE_MAX_FRAME + 1)];
    const byte *p;
    int avail, version, flags, seq, count, i, talker, size, sent = 0;
    client_t *cl;

    if (!sv_voice || !sv_voice->boolean || !sv_alltalk) return;
    cl = voice_find_client(from);
    if (!cl || cl->state < CS_ACTIVE) return;
    talker = (int)(cl - svs.clients);
    cl->lastPacketTime = svs.time;
    if (cl->mutelevel) return;

    p = msg->data + msg->readcount;
    avail = msg->cursize - msg->readcount;
    if (avail < 5) return;
    version = p[0];
    flags = p[1];
    seq = p[2] | (p[3] << 8);
    count = p[4];
    if (version != VOICE_VERSION || count < 1 || count > VOICE_MAX_FRAMES) return;
    /* validate the frame list and its total size */
    size = 5;
    for (i = 0; i < count; ++i) {
        int len;
        if (size >= avail) return;
        len = p[size];
        if (len < 1 || size + 1 + len > avail) return;
        size += 1 + len;
    }

    /* rate limit: a one-second window per talker */
    if (svs.time - voice_bucket_time[talker] >= 1000 || svs.time < voice_bucket_time[talker]) {
        voice_bucket_time[talker] = svs.time;
        voice_bucket_count[talker] = 0;
    }
    if (++voice_bucket_count[talker] > voiceMaxPackets->integer) return;

    voice_last_time[talker] = (flags & 1) ? 0 : (svs.time ? svs.time : 1);

    out[0] = 'v';
    out[1] = 0;
    out[2] = VOICE_VERSION;
    out[3] = (byte)flags;
    out[4] = (byte)talker;
    out[5] = (byte)(seq & 0xff);
    out[6] = (byte)(seq >> 8);
    memcpy(out + 7, p + 4, size - 4); /* count + frames */

    for (i = 0; i < sv_maxclients->integer && i < MAX_CLIENTS; ++i) {
        if (!VoiceCanHear(i, talker)) continue;
        NET_OutOfBandData(NS_SERVER, &svs.clients[i].netchan.remoteAddress, out, 7 + size - 4);
        sent++;
    }
    /* debug: the start and end of each transmission and every 50th packet, capped per map process */
    if (sv_voiceDebug->boolean && voice_debug_lines < 20000 &&
        (seq < 2 || (flags & 1) || !((seq >> 1) % 50))) {
        voice_debug_lines++;
        ((void)0);
    }
}
#endif

#ifdef OL_SERVER
#include "persistent_tail.h"
#include <string.h>
#include <limits.h>
static int resident_tail_zero(const void *data,size_t bytes){
 const unsigned char *p=(const unsigned char*)data;for(size_t i=0;i<bytes;++i)if(p[i])return 0;return 1;
}

int PersistentTailVoiceEmpty(void){for(unsigned i=0;i<MAX_CLIENTS;++i)if(voice_last_time[i]||voice_bucket_time[i]||voice_bucket_count[i]||voice_last_killer[i]!=-1)return 0;return 1;}
void PersistentTailVoiceReset(void){for(unsigned i=0;i<MAX_CLIENTS;++i){VoiceClientReset(i);voice_bucket_time[i]=0;}}
#endif
