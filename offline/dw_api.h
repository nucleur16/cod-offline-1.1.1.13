/*
 * dw_api.h - the C boundary of the offline Demonware lobby (offline/, docs/OFFLINE_LOBBY.md).
 *
 * This is the only header Kernel33's C sources include. No C++ type, exception or object crosses it.
 * The lobby runs on its own thread (dw_net.cpp's select loop); the three callbacks are invoked on
 * THAT thread and must only copy data (they may not call into C1).
 */
#ifndef K33_DW_API_H
#define K33_DW_API_H
#ifdef __cplusplus
extern "C" {
#endif

typedef struct DwOfflineRoute {
    int enabled;             /* the room owner ticked Local Play */
    int port;                /* UDP port of the embedded match host on 127.0.0.1 */
    char map[64], sessionMap[64], gametype[32], category[16];
} DwOfflineRoute;

typedef struct DwOfflineStartInfo {
    unsigned long long uid;
    char playerName[64], map[64], sessionMap[64], gametype[32], zoneType[16];
    int hostPort;
    int local;
} DwOfflineStartInfo;

typedef struct DwOfflineConfig {
    const char *resources;    /* optional override folder for the embedded resources, or NULL */
    const char *profile;      /* the persistent store */
    const char *state;        /* securitykey.bin, game_admissions.txt: shared with the embedded match host */
    const char *matchProfile; /* the match host's codol_profilePath: mpdata_s.<uid>, match_report.json */
    const char *zoneDir;      /* the client's zone folder (map availability), or NULL */
    int port;                 /* loopback TCP + UDP port of the lobby */
    int altPort;              /* alternate STUN port */
    int hostPort;             /* the embedded match host's UDP port */
    void (*log)(const char *line);
    int (*localRoute)(unsigned long long uid, DwOfflineRoute *out);
    void (*startGame)(const DwOfflineStartInfo *info);
} DwOfflineConfig;

/* Starts the listener thread and waits (at most 3 s) until it is bound.
 * -> the TCP port, or 0 when the lobby could not listen (the caller must then refuse offline mode). */
int DwOfflineStartServer(const DwOfflineConfig *config);
/* Asks the listener to stop and waits for it (at most 5 s). */
void DwOfflineStopServer(void);
int DwOfflineRunning(void);

#ifdef __cplusplus
}
#endif
#endif
