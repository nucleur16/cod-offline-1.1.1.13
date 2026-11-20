/* Script-owned radar input, distinct from captured playerState output. */
#ifndef RADAR_INPUT_H
#define RADAR_INPUT_H
#ifdef __cplusplus
extern "C" {
#endif
int ClientRadarBlockInput(unsigned client);
void SetClientRadarBlockInput(unsigned client, int value);
void ResetClientRadarInput(unsigned client);
void ResetAllRadarInput(void);
#ifdef __cplusplus
}
#endif
#endif
