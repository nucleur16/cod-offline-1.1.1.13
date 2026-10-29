#ifndef K33_P2P_UI_API_H
#define K33_P2P_UI_API_H
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    char map[64], gametype[32], difficulty[16];
    int zone, child, lan, port, bots;
} P2PUiOptions;
typedef struct {
    int busy, ready, restartRequired, pending;
    char status[512];
} P2PUiState;
int P2PUiListSurvivalMaps(char (*maps)[64], int maximum);
void P2PUiDefaults(P2PUiOptions *options);
void P2PUiGetState(P2PUiState *state);
int P2PUiStart(const P2PUiOptions *options);
void P2PUiStop(void);
#ifdef __cplusplus
}
#endif
#endif
