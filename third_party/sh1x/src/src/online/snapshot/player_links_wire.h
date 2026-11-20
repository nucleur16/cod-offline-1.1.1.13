#ifndef PLAYER_LINKS_WIRE_H
#define PLAYER_LINKS_WIRE_H
#include "playerstate_ext.h"
#ifdef __cplusplus
extern "C" {
#endif
int OverlayPlayerLinks(player_fields_t *wire, const ps_links_t *captured);
int OverlayEnginePlayerLinks(player_fields_t *wire, const playerState_t *ps);
#ifdef __cplusplus
}
#endif
#endif
