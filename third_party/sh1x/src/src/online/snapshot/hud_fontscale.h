#ifndef HUD_FONTSCALE_H
#define HUD_FONTSCALE_H
#include "q_shared.h"
/* Semantic state must not inherit engine-header packing. */
#pragma pack(push, 8)
typedef struct { float from; int start, duration; } hud_fontscale_t;
struct game_hudelem_s;
struct gclient_s;
void ResetHudFontScale(struct game_hudelem_s *);
void ResetHudFontScales(void);
void CopyClientHudFontScale(struct gclient_s *,int,hudelem_t *,const struct game_hudelem_s *);
/* Only live native HUD objects/copies captured by HudElem_UpdateClient have
 * identity here. Historical snapshots must capture bindings before copying. */
int GetHudFontScale(const hudelem_t *,hud_fontscale_t *);
#pragma pack(pop)
#endif
