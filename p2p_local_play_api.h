/*
 * p2p_local_play_api.h - what the in-process lobby (the C++ port inside Kernel33.dll) may read and
 * call about the owner's Local Play choice. See docs/P2P_DEFERRED_HOST_START.md.
 *
 * Ticking LOCAL PLAY arms a host and starts nothing. The lobby must therefore hand the match off
 * while the owner's intent is "armed" (not ready): the embedded server is started by Kernel33 when
 * the match launches for the owner, and the owner's client connects to it when it answers.
 *
 * Both functions are safe on any thread. Neither starts a server, loads a DLL or touches C1: the
 * launch call only raises a request that p2p.c's main-thread tick validates and consumes.
 */
#ifndef K33_P2P_LOCAL_PLAY_API_H
#define K33_P2P_LOCAL_PLAY_API_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct K33LocalPlayIntentInfo {
    unsigned size;                 /* sizeof(K33LocalPlayIntentInfo) */
    unsigned long long owner_uid;  /* the local player's uid the intent is bound to; 0 = none */
    int enabled;                   /* LOCAL PLAY is ticked */
    int armed;                     /* enabled, valid, nothing running: "will host on launch" */
    int launched;                  /* this selection's host start was issued */
    int ready;                     /* the host answers with exactly this selection */
    int failed;                    /* the choice cannot be hosted (see error) */
    int port;                      /* loopback UDP port the host will listen / listens on */
    int protocol;                  /* 0 legacy, 1/2 local startup IPC (the production protocol never arms here) */
    char map[64], session_map[64], gametype[32], type[16];
    char state[16];                /* the intent file's state=: idle armed starting ready stopping exited failed */
    char error[256];
} K33LocalPlayIntentInfo;

/* -> 1 and *out when Kernel33 has published an intent snapshot, else 0. The same fields as the
 * intent file <state>\p2p_host.<uid16hex>.txt, without the two-second file heartbeat. */
int K33LocalPlayIntent(K33LocalPlayIntentInfo *out);

/* The lobby is handing START_GAME for exactly this selection to the local owner. -> 1 when the
 * launch request was raised (or that selection's host was already started), 0 when the owner's
 * current intent is not this enabled, armed selection: the lobby must then withhold START_GAME.
 * Never substitute another server for a refused Local Play room. */
int K33LocalPlayLaunch(unsigned long long owner_uid, const char *session_map, const char *gametype, const char *type);

#ifdef __cplusplus
}
#endif
#endif
