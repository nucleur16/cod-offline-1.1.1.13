#ifndef SPAWN_ARTILLERY_H
#define SPAWN_ARTILLERY_H
/* T5 artillery's static NORMAL cylinder subset. Not a replacement for the
 * complete attached/player/weapon spawn-influencer system. */
void ResetArtilleryInfluencers(void);
void Scr_AddArtilleryInfluencer(void);
void Scr_RemoveArtilleryInfluencer(void);
void Scr_ArtillerySpawnWeight(void);
#endif
