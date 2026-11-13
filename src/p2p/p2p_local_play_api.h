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

/* Offline mode: every room the local player owns is hosted by the embedded server; there is no
 * choice to tick. The in-process lobby tells Kernel33 the selection it resolved for the owner's room
 * (and is about to tell the client): Kernel33 arms the host for exactly that selection, replacing
 * whatever was armed, and with `launch` also raises the launch (the lobby is handing START_GAME to
 * the owner now). `difficulty` is the Zombies room's (0..2), -1 for none.
 * -> 1: accepted; *port is the loopback UDP port the host serves on. The work is done by p2p.c's
 *       main-thread tick; nothing is started on the caller's thread.
 * -> 0: this selection cannot be hosted; `error` says why (an unknown category or map, a missing
 *       server, a host that already ran in this client run, ...). The lobby must withhold START_GAME;
 *       no other server is ever substituted. The same text is published to the UI (ui_p2p_error).
 * Any thread. */
int K33LocalPlayAuto(unsigned long long owner_uid, const char *session_map, const char *gametype, const char *type,
                     int difficulty, int launch, int *port, char *error, int error_size);
/* The owner left the room: nothing stays armed and a launched host is stopped. Any thread. */
void K33LocalPlayLeave(unsigned long long owner_uid);

#ifdef __cplusplus
}
#endif
#endif
