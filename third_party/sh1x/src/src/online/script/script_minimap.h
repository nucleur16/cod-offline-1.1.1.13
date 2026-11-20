#ifndef SCRIPT_MINIMAP_H
#define SCRIPT_MINIMAP_H
/* Real C1 fifteen-slot namespace. Returns wire selector1..15, zero on failure. */
#ifdef __cplusplus
extern "C" {
#endif
unsigned RegisterMinimapIcon(const char *name);
void Scr_PrecacheMinimapIcon(void);
void Scr_SetNorthYaw(void);
void Scr_SpawnPlane(void);
#ifdef __cplusplus
}
#endif
#endif
