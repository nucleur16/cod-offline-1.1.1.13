/*
 * dw_api.h - the C boundary of the offline Demonware lobby (offline/, docs/OFFLINE_LOBBY.md).
 *
 * This is the only header Kernel33's C sources include. No C++ type, exception or object crosses it.
 * The lobby runs on its own thread (dw_net.cpp's select loop); the callbacks are invoked on
 * THAT thread and must only copy data (they may not call into C1).
 */
#ifndef K33_DW_API_H
#define K33_DW_API_H
#ifdef __cplusplus
extern "C" {
#endif

/* The room owner's Local Play intent (the fields of K33LocalPlayIntentInfo, p2p_local_play_api.h). */
typedef struct DwOfflineRoute {
    unsigned long long ownerUid;
    int enabled;             /* local hosting is on */
    int armed, launched, ready, failed;
    int port;                /* UDP port of the embedded match host on 127.0.0.1 */
    char map[64], sessionMap[64], gametype[32], category[16];
    char state[16];          /* idle armed starting ready stopping exited failed */
    char error[256];
} DwOfflineRoute;

typedef struct DwOfflineStartInfo {
    unsigned long long uid;
    char playerName[64], map[64], sessionMap[64], gametype[32], zoneType[16];
    int hostPort;
    int local;
    int difficulty;          /* Zombies: 0..3 (3 = Endless); -1 for every other category */
} DwOfflineStartInfo;

typedef struct DwOfflineConfig {
    const char *resources;    /* optional override folder for the embedded resources, or NULL */
    const char *profile;      /* the players folder: <profile>\<name>\mpdata, mpdata_s, account.txt (dw_store.h) */
    const char *playerName;   /* the local player's profile name = its folder under `profile`; NULL = name it by uid */
    const char *legacyStore;  /* optional: a k33_offline\profile store of the first offline builds, imported once */
    const char *state;        /* securitykey.bin, game_admissions.txt: shared with the embedded match host */
    const char *matchProfile; /* the match host's profilePath: mpdata_s.<uid>, match_report.json */
    const char *zoneDir;      /* the client's zone folder (map availability), or NULL */
    const char *gameDir;      /* the game install: usermaps\<map>\, main\sh1x_rawfiles (Survival placements);
                                 or NULL */
    int port;                 /* loopback TCP + UDP port of the lobby; 0 = a free port (the normal case) */
    int altPort;              /* alternate STUN port; 0 = a free port */
    int hostPort;             /* the embedded match host's UDP port */
    void (*log)(const char *line);
    int (*localRoute)(unsigned long long uid, DwOfflineRoute *out);
    /* The selection the lobby resolved for the room the local player owns (K33LocalPlayAuto): arm the
     * embedded host for it; with `launch`, START_GAME is being handed to the owner now.
     * -> 1 and *port; 0 and `error` when it cannot be hosted (START_GAME is withheld). */
    int (*start)(unsigned long long ownerUid, const char *sessionMap, const char *gametype, const char *type, int difficulty,
                 int launch, int *port, char *error, int errorSize);
    /* The owner left its room (K33LocalPlayLeave). */
    void (*leave)(unsigned long long ownerUid);
    void (*startGame)(const DwOfflineStartInfo *info);
    /* Optional: the owner's next START_GAME loads this map and mode while its room keeps its own names and roster
     * (dw::Host::StartNames); empty names drop what was staged. -> 1 when it will. Called on the lobby thread. */
    int (*startNames)(const char *map, const char *gametype);
    /* Optional: the display name the local player set for the profile `profileName` (the console's `name`).
     * -> 1 and the name in out (NUL-terminated), or 0 when there is none. Called on the lobby thread; it must
     * only copy data. */
    int (*displayName)(const char *profileName, char *out, int outSize);
    /* Optional: whether the zones list `level` as a Survival map (dw::Host::SurvivalSupported): 1, 0, or -1 for
     * unknown. Called on the lobby thread; it must only read the embedder's own copy. */
    int (*survivalSupported)(const char *level);
} DwOfflineConfig;

/* Starts the listener thread and waits (at most 3 s) until it is bound.
 * -> the TCP port it listens on (the port asked for, or the free one obtained), or 0 when the lobby
 *    could not listen (the caller must then refuse offline mode). */
int DwOfflineStartServer(const DwOfflineConfig *config);
/* The alternate STUN port of the running listener, 0 when there is none. */
int DwOfflineAltPort(void);
/* Asks the listener to stop and waits for it (at most 5 s). */
void DwOfflineStopServer(void);
int DwOfflineRunning(void);

#ifdef __cplusplus
}
#endif
#endif
